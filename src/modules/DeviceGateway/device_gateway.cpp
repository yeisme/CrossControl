#include "modules/DeviceGateway/device_gateway.h"

#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include <QUrlQuery>
#include <QtSql/QSqlError>
#include <QtSql/QSqlQuery>

#include "modules/Connect/connect_factory.h"
#include "modules/DeviceGateway/rest_server.h"
#include "modules/Storage/storage.h"
#include "spdlog/spdlog.h"

#ifdef HAS_DROGON
using DeviceGateway::RestServer;
#endif

namespace DeviceGateway {

DeviceGateway::DeviceGateway() = default;
DeviceGateway::~DeviceGateway() {
    shutdown();
}

bool DeviceGateway::isRestRunning() const {
    if (!m_restServer) return false;
    auto srv = static_cast<RestServer*>(m_restServer);
    return srv->isRunning();
}

DeviceRegistry& DeviceGateway::registry() {
    return m_registry;
}

bool DeviceGateway::init() {
    // Prefer Drogon if compiled in; otherwise use our Qt-based RestServer
    // preserve legacy behavior by starting REST on default port 8080
    return startRest(8080);
}

bool DeviceGateway::startRest(unsigned short port) {
    // If already running, treat as success
    if (isRestRunning()) return true;
    // Prefer Drogon if compiled in; otherwise use our Qt-based RestServer
#ifdef HAS_DROGON
    if (!m_restServer) {
        auto srv = new RestServer(this);
        if (srv->start(port)) {
            m_restServer = srv;
            return true;
        } else {
            delete srv;
            m_restServer = nullptr;
            return false;
        }
    }
    return true;
#else
    if (!m_restServer) {
        // create our Qt-based rest server
        auto srv = new RestServer(this);
        // store pointer in m_restServer and start listening on port
        m_restServer = srv;
        if (!srv->start(port)) {
            spdlog::warn("DeviceGateway: failed to start internal REST server");
            delete srv;
            m_restServer = nullptr;
            return false;
        }
    }
    spdlog::info("DeviceGateway: internal REST server started on port {}", std::to_string(port));
    return true;
#endif
}

void DeviceGateway::stopRest() {
    if (m_restServer) {
        auto srv = static_cast<RestServer*>(m_restServer);
        srv->stop();
        delete srv;
        m_restServer = nullptr;
    }
}

void DeviceGateway::shutdown() {
    if (m_restServer) {
        auto srv = static_cast<RestServer*>(m_restServer);
        srv->stop();
        delete srv;
        m_restServer = nullptr;
    }

    // Close all managed connections
    for (auto it = m_connections.begin(); it != m_connections.end(); ++it) {
        if (it.value() && it.value()->isOpen()) it.value()->close();
    }
    m_connections.clear();
}

bool DeviceGateway::addDeviceWithConnect(const DeviceInfo& dev) {
    if (!m_registry.addDevice(dev)) {
        spdlog::warn("DeviceGateway: addDeviceWithConnect failed, invalid device id");
        return false;
    }
    // Persist device metadata into Storage if available
    if (!persistDevice(dev)) {
        spdlog::warn("DeviceGateway: failed to persist device {}", dev.id.toStdString());
    } else {
        spdlog::info("DeviceGateway: persisted device {}", dev.id.toStdString());
    }

    if (dev.endpoint.isEmpty()) return true;  // nothing to connect

    // Allow authentication details via device metadata. Support common keys:
    //  - auth_user / auth_pass  -> injected as user:pass@ in URL
    //  - auth_token            -> appended as query ?token=...
    QString ep = dev.endpoint.trimmed();
    if (!dev.metadata.isEmpty()) {
        // prefer QUrl parsing
        QUrl url = QUrl::fromUserInput(ep);
        bool changed = false;
        if (dev.metadata.contains("auth_user") && dev.metadata.contains("auth_pass")) {
            const QString u = dev.metadata.value("auth_user").toString();
            const QString p = dev.metadata.value("auth_pass").toString();
            if (!u.isEmpty()) {
                url.setUserName(u);
                url.setPassword(p);
                changed = true;
            }
        }
        if (dev.metadata.contains("auth_token")) {
            const QString token = dev.metadata.value("auth_token").toString();
            if (!token.isEmpty()) {
                QUrlQuery q(url);
                q.addQueryItem("token", token);
                url.setQuery(q);
                changed = true;
            }
        }
        if (changed) ep = url.toString();
    }

    auto conn = connect_factory::createByEndpoint(ep);
    if (!conn) {
        spdlog::warn("DeviceGateway: could not create connection for endpoint {}",
                     dev.endpoint.toStdString());
        return true;  // unable to create a connection object, but device still registered
    }

    // try to open; if open fails we still keep the conn object but may not store it
    if (conn->open(dev.endpoint)) {
        m_connections.insert(dev.id, conn);
        spdlog::info("DeviceGateway: opened connection for device {} -> {}",
                     dev.id.toStdString(),
                     dev.endpoint.toStdString());
    } else {
        spdlog::warn("DeviceGateway: failed to open connection for device {} endpoint {}",
                     dev.id.toStdString(),
                     dev.endpoint.toStdString());
    }
    return true;
}

bool DeviceGateway::removeDeviceAndClose(const QString& id) {
    if (!m_registry.removeDevice(id)) {
        spdlog::warn("DeviceGateway: removeDeviceAndClose failed, id not found: {}",
                     id.toStdString());
        return false;
    }
    if (m_connections.contains(id)) {
        auto conn = m_connections.take(id);
        if (conn && conn->isOpen()) {
            conn->close();
            spdlog::info("DeviceGateway: closed connection for device {}", id.toStdString());
        }
    }
    // Also remove persisted device row if storage available
    using namespace storage;
    if (Storage::db().isOpen()) {
        QSqlQuery q(Storage::db());
        q.prepare("DELETE FROM devices WHERE id = ?");
        q.addBindValue(id);
        if (!q.exec()) {
            spdlog::warn("DeviceGateway: failed to delete device {} from storage: {}",
                         id.toStdString(),
                         q.lastError().text().toStdString());
        } else {
            spdlog::info("DeviceGateway: deleted device {} from storage", id.toStdString());
        }
    }
    return true;
}

bool DeviceGateway::ensureDevicesTable() {
    using namespace storage;
    if (!Storage::db().isOpen()) return false;
    QSqlQuery q(Storage::db());
    const QString create = QStringLiteral(
        "CREATE TABLE IF NOT EXISTS devices ("
        "id TEXT PRIMARY KEY,"
        "name TEXT,"
        "status TEXT,"
        "endpoint TEXT,"
        "type TEXT,"
        "hw_info TEXT,"
        "firmware_version TEXT,"
        "owner TEXT,"
        "group_name TEXT,"
        "last_seen INTEGER,"
        "metadata TEXT"
        ");");
    if (!q.exec(create)) {
        spdlog::error("DeviceGateway: Failed to create devices table: {}",
                      q.lastError().text().toStdString());
        return false;
    }
    spdlog::debug("DeviceGateway: ensured devices table exists");
    return true;
}

bool DeviceGateway::persistDevice(const DeviceInfo& dev) {
    using namespace storage;
    if (!Storage::db().isOpen()) return false;
    if (!ensureDevicesTable()) return false;

    QSqlQuery q(Storage::db());
    q.prepare(
        "INSERT OR REPLACE INTO "
        "devices(id,name,status,endpoint,type,hw_info,firmware_version,owner,group_name,last_seen,"
        "metadata) VALUES(?,?,?,?,?,?,?,?,?,?,?)");
    q.addBindValue(dev.id);
    q.addBindValue(dev.name);
    q.addBindValue(dev.status);
    q.addBindValue(dev.endpoint);
    q.addBindValue(dev.type);
    q.addBindValue(dev.hw_info);
    q.addBindValue(dev.firmware_version);
    q.addBindValue(dev.owner);
    q.addBindValue(dev.group);
    if (dev.lastSeen.isValid())
        q.addBindValue(static_cast<qint64>(dev.lastSeen.toSecsSinceEpoch()));
    else
        q.addBindValue(QVariant());

    QJsonObject mo;
    for (auto it = dev.metadata.constBegin(); it != dev.metadata.constEnd(); ++it)
        mo.insert(it.key(), QJsonValue::fromVariant(it.value()));
    q.addBindValue(QString::fromUtf8(QJsonDocument(mo).toJson(QJsonDocument::Compact)));

    if (!q.exec()) {
        spdlog::error("DeviceGateway: Failed to persist device {}: {}",
                      dev.id.toStdString(),
                      q.lastError().text().toStdString());
        return false;
    }
    return true;
}

QVector<DeviceInfo> DeviceGateway::loadDevicesFromStorage() const {
    QVector<DeviceInfo> out;
    using namespace storage;
    if (!Storage::db().isOpen()) return out;
    QSqlQuery q(Storage::db());
    if (!q.exec("SELECT "
                "id,name,status,endpoint,type,hw_info,firmware_version,owner,group_name,last_seen,"
                "metadata FROM devices"))
        return out;
    while (q.next()) {
        DeviceInfo d;
        d.id = q.value(0).toString();
        d.name = q.value(1).toString();
        d.status = q.value(2).toString();
        d.endpoint = q.value(3).toString();
        d.type = q.value(4).toString();
        d.hw_info = q.value(5).toString();
        d.firmware_version = q.value(6).toString();
        d.owner = q.value(7).toString();
        d.group = q.value(8).toString();
        bool ok = false;
        qint64 ts = q.value(9).toLongLong(&ok);
        if (ok) d.lastSeen = QDateTime::fromSecsSinceEpoch(ts);
        const QString meta = q.value(10).toString();
        if (!meta.isEmpty()) {
            const auto doc = QJsonDocument::fromJson(meta.toUtf8());
            if (doc.isObject()) { d.metadata = doc.object().toVariantMap(); }
        }
        spdlog::debug("DeviceGateway: loaded device {} from storage", d.id.toStdString());
        out.append(d);
    }
    return out;
}

bool DeviceGateway::exportDevicesCsv(const QString& path) const {
    auto devs = loadDevicesFromStorage();
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
    QTextStream ts(&f);
    ts << "id,name,status,endpoint,type,hw_info,firmware_version,owner,group_name,last_seen,"
          "metadata\n";
    for (const auto& d : devs) {
        ts << d.id << "," << d.name << "," << d.status << "," << d.endpoint << "," << d.type << ","
           << d.hw_info << "," << d.firmware_version << "," << d.owner << "," << d.group << ","
           << (d.lastSeen.isValid() ? QString::number(d.lastSeen.toSecsSinceEpoch()) : QString())
           << ","
           << QJsonDocument(QJsonObject::fromVariantMap(d.metadata)).toJson(QJsonDocument::Compact)
           << "\n";
    }
    return true;
}

bool DeviceGateway::exportDevicesJsonND(const QString& path) const {
    auto devs = loadDevicesFromStorage();
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
    QTextStream ts(&f);
    for (const auto& d : devs) {
        QJsonObject jo;
        jo["id"] = d.id;
        jo["name"] = d.name;
        jo["status"] = d.status;
        jo["endpoint"] = d.endpoint;
        jo["type"] = d.type;
        jo["hw_info"] = d.hw_info;
        jo["firmware_version"] = d.firmware_version;
        jo["owner"] = d.owner;
        jo["group"] = d.group;
        jo["last_seen"] = d.lastSeen.isValid()
                              ? QJsonValue::fromVariant(d.lastSeen.toSecsSinceEpoch())
                              : QJsonValue();
        jo["metadata"] = QJsonObject::fromVariantMap(d.metadata);
        ts << QString::fromUtf8(QJsonDocument(jo).toJson(QJsonDocument::Compact)) << "\n";
    }
    return true;
}

bool DeviceGateway::importDevicesCsv(const QString& path, bool createConnections) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        spdlog::error("DeviceGateway: cannot open CSV import file {}", path.toStdString());
        return false;
    }
    QTextStream ts(&f);

    auto parseCsvLine = [](const QString& line) {
        QStringList out;
        QString cur;
        bool inQuotes = false;
        for (int i = 0; i < line.size(); ++i) {
            const QChar c = line.at(i);
            if (inQuotes) {
                if (c == '"') {
                    // peek next char to see if it's an escaped quote
                    if (i + 1 < line.size() && line.at(i + 1) == '"') {
                        cur.append('"');
                        ++i;  // consume escaped quote
                    } else {
                        inQuotes = false;
                    }
                } else {
                    cur.append(c);
                }
            } else {
                if (c == ',') {
                    out.append(cur.trimmed());
                    cur.clear();
                } else if (c == '"') {
                    inQuotes = true;
                } else {
                    cur.append(c);
                }
            }
        }
        out.append(cur.trimmed());
        return out;
    };

    // Read header
    if (ts.atEnd()) return true;
    const QString header = ts.readLine();
    Q_UNUSED(header);
    int lineNo = 1;
    while (!ts.atEnd()) {
        const QString line = ts.readLine();
        ++lineNo;
        if (line.trimmed().isEmpty()) continue;
        const QStringList parts = parseCsvLine(line);
        if (parts.size() < 11) {
            spdlog::warn("DeviceGateway: CSV import line {} has insufficient columns (got {})",
                         lineNo,
                         parts.size());
            continue;
        }
        DeviceInfo d;
        d.id = parts.value(0);
        d.name = parts.value(1);
        d.status = parts.value(2);
        d.endpoint = parts.value(3);
        d.type = parts.value(4);
        d.hw_info = parts.value(5);
        d.firmware_version = parts.value(6);
        d.owner = parts.value(7);
        d.group = parts.value(8);
        const QString tsStr = parts.value(9);
        if (!tsStr.isEmpty()) {
            bool ok = false;
            qint64 secs = tsStr.toLongLong(&ok);
            if (ok) d.lastSeen = QDateTime::fromSecsSinceEpoch(secs);
        }
        const QString meta = parts.value(10);
        if (!meta.isEmpty()) {
            const auto doc = QJsonDocument::fromJson(meta.toUtf8());
            if (doc.isObject()) d.metadata = doc.object().toVariantMap();
        }
        if (!persistDevice(d)) {
            spdlog::warn("DeviceGateway: failed to persist device {} from CSV", d.id.toStdString());
            continue;
        }
        if (createConnections) addDeviceWithConnect(d);
    }
    spdlog::info("DeviceGateway: CSV import completed from {}", path.toStdString());
    return true;
}

bool DeviceGateway::importDevicesJsonND(const QString& path, bool createConnections) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        spdlog::error("DeviceGateway: cannot open NDJSON import file {}", path.toStdString());
        return false;
    }
    QTextStream ts(&f);
    int lineNo = 0;
    while (!ts.atEnd()) {
        ++lineNo;
        const QString line = ts.readLine();
        if (line.trimmed().isEmpty()) continue;
        const auto doc = QJsonDocument::fromJson(line.toUtf8());
        if (!doc.isObject()) {
            spdlog::warn("DeviceGateway: NDJSON import line {} is not an object", lineNo);
            continue;
        }
        const auto jo = doc.object();
        DeviceInfo d;
        d.id = jo.value("id").toString();
        d.name = jo.value("name").toString();
        d.status = jo.value("status").toString();
        d.endpoint = jo.value("endpoint").toString();
        d.type = jo.value("type").toString();
        d.hw_info = jo.value("hw_info").toString();
        d.firmware_version = jo.value("firmware_version").toString();
        d.owner = jo.value("owner").toString();
        d.group = jo.value("group").toString();
        if (jo.contains("last_seen") && jo.value("last_seen").isDouble()) {
            qint64 secs = static_cast<qint64>(jo.value("last_seen").toDouble());
            d.lastSeen = QDateTime::fromSecsSinceEpoch(secs);
        }
        if (jo.contains("metadata") && jo.value("metadata").isObject())
            d.metadata = jo.value("metadata").toObject().toVariantMap();
        if (!persistDevice(d)) {
            spdlog::warn("DeviceGateway: failed to persist device {} from NDJSON",
                         d.id.toStdString());
            continue;
        }
        if (createConnections) addDeviceWithConnect(d);
    }
    spdlog::info("DeviceGateway: NDJSON import completed from {}", path.toStdString());
    return true;
}

}  // namespace DeviceGateway
