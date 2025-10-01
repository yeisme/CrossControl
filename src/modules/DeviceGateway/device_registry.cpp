#include "modules/DeviceGateway/device_registry.h"

#include <QIODevice>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStringList>
#include <QTextStream>
#include <QUuid>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlError>
#include <QtSql/QSqlQuery>

#include "modules/Storage/storage.h"

namespace DeviceGateway {

// forward declarations for helper functions defined further below
static bool ensureDevicesTable(QSqlDatabase& db);
static void loadFromStorageDb(QMap<QString, DeviceInfo>& store, QReadWriteLock& lock);

QJsonObject DeviceInfo::toJson() const {
    QJsonObject o;
    o["deviceId"] = deviceId;
    o["hwInfo"] = hwInfo;
    o["endpoint"] = endpoint;
    o["firmwareVersion"] = firmwareVersion;
    o["owner"] = owner;
    o["group"] = group;
    o["iconPath"] = iconPath;
    o["lastSeen"] = lastSeen.toString(Qt::ISODate);
    return o;
}

DeviceInfo DeviceInfo::fromJson(const QJsonObject& obj) {
    DeviceInfo d;
    d.deviceId = obj.value("deviceId").toString();
    d.hwInfo = obj.value("hwInfo").toString();
    d.endpoint = obj.value("endpoint").toString();
    d.firmwareVersion = obj.value("firmwareVersion").toString();
    d.owner = obj.value("owner").toString();
    d.group = obj.value("group").toString();
    d.iconPath = obj.value("iconPath").toString();
    d.lastSeen = QDateTime::fromString(obj.value("lastSeen").toString(), Qt::ISODate);
    return d;
}

DeviceRegistry::DeviceRegistry() {
    // attempt to load existing devices from the configured storage DB
    try {
        loadFromStorageDb(m_store, m_lock);
    } catch (...) {}
}

// Helper: ensure devices table exists in given DB
static bool ensureDevicesTable(QSqlDatabase& db) {
    if (!db.isOpen()) return false;
    QSqlQuery q(db);
    const QString sql =
        "CREATE TABLE IF NOT EXISTS devices ("
        "deviceId TEXT PRIMARY KEY,"
        "hwInfo TEXT,"
        "endpoint TEXT,"
        "firmwareVersion TEXT,"
        "owner TEXT,"
        "groupname TEXT,"
        "iconPath TEXT,"
        "lastSeen TEXT"
        ");";
    return q.exec(sql);
}

// Load existing devices from main storage DB if available
static void loadFromStorageDb(QMap<QString, DeviceInfo>& store, QReadWriteLock& lock) {
    try {
        QSqlDatabase& db = storage::Storage::db();
        if (!db.isOpen()) return;
        if (!ensureDevicesTable(db)) return;

        QSqlQuery q(db);
        if (!q.exec("SELECT deviceId, hwInfo, firmwareVersion, owner, groupname, iconPath, "
                    "lastSeen FROM "
                    "devices"))
            return;

        QWriteLocker w(&lock);
        while (q.next()) {
            DeviceInfo d;
            d.deviceId = q.value(0).toString();
            d.hwInfo = q.value(1).toString();
            d.endpoint = q.value(2).toString();
            d.firmwareVersion = q.value(2).toString();
            d.owner = q.value(3).toString();
            d.group = q.value(4).toString();
            d.iconPath = q.value(5).toString();
            d.lastSeen = QDateTime::fromString(q.value(6).toString(), Qt::ISODate);
            if (d.deviceId.isEmpty()) continue;
            store.insert(d.deviceId, d);
        }
    } catch (...) {
        // ignore DB errors here
    }
}

// existing upsert implementation moved to support optional persistence (see below)

// Persist single device to given DB
static bool persistDeviceToDb(QSqlDatabase& db, const DeviceInfo& d) {
    if (!db.isOpen()) return false;
    QSqlQuery q(db);
    q.prepare(
        "REPLACE INTO devices (deviceId, hwInfo, firmwareVersion, owner, groupname, iconPath, "
            "endpoint, lastSeen) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?)");
        q.addBindValue(d.deviceId);
        q.addBindValue(d.hwInfo);
        q.addBindValue(d.endpoint);
        q.addBindValue(d.firmwareVersion);
        q.addBindValue(d.owner);
        q.addBindValue(d.group);
        q.addBindValue(d.iconPath);
        q.addBindValue(d.lastSeen.toString(Qt::ISODate));
    return q.exec();
}

void DeviceRegistry::upsert(const DeviceInfo& info, bool persistToStorage) {
    QWriteLocker w(&m_lock);
    m_store.insert(info.deviceId, info);
    // optionally persist
    if (persistToStorage) {
        try {
            QSqlDatabase& db = storage::Storage::db();
            if (db.isOpen()) {
                ensureDevicesTable(db);
                persistDeviceToDb(db, info);
            }
        } catch (...) {}
    }
}

std::optional<DeviceInfo> DeviceRegistry::get(const QString& deviceId) const {
    QReadLocker r(&m_lock);
    if (!m_store.contains(deviceId)) return std::nullopt;
    return m_store.value(deviceId);
}

bool DeviceRegistry::remove(const QString& deviceId) {
    QWriteLocker w(&m_lock);
    const bool removed = m_store.remove(deviceId) > 0;
    if (removed) {
        // attempt to remove from storage DB as well
        try {
            QSqlDatabase& db = storage::Storage::db();
            if (db.isOpen()) {
                QSqlQuery q(db);
                q.prepare("DELETE FROM devices WHERE deviceId = ?");
                q.addBindValue(deviceId);
                q.exec();
            }
        } catch (...) {}
    }
    return removed;
}

QList<DeviceInfo> DeviceRegistry::list() const {
    QReadLocker r(&m_lock);
    return m_store.values();
}

QString DeviceRegistry::exportCsv() const {
    QReadLocker r(&m_lock);
    QString out;
    QTextStream ts(&out);
    ts << "deviceId,hwInfo,firmwareVersion,owner,group,iconPath,lastSeen\n";
    for (const auto& d : m_store) {
        QString id = d.deviceId;
        QString hw = d.hwInfo;
        QString fw = d.firmwareVersion;
        QString owner = d.owner;
        QString group = d.group;
        QString icon = d.iconPath;
        id.replace("\"", "\"\"");
        hw.replace("\"", "\"\"");
        fw.replace("\"", "\"\"");
        owner.replace("\"", "\"\"");
        group.replace("\"", "\"\"");
        icon.replace("\"", "\"\"");

        ts << '"' << id << '"' << ',';
        ts << '"' << hw << '"' << ',';
        ts << '"' << fw << '"' << ',';
        ts << '"' << owner << '"' << ',';
        ts << '"' << group << '"' << ',';
        ts << '"' << icon << '"' << ',';
        ts << '"' << d.lastSeen.toString(Qt::ISODate) << '"' << '\n';
    }
    return out;
}

QString DeviceRegistry::exportJsonNd() const {
    QReadLocker r(&m_lock);  // 读取锁
    QString out;             // 输出字符串
    QTextStream ts(&out);    // 文本流，方便写入字符串
    for (const auto& d : m_store) {
        QJsonObject obj = d.toJson();
        QJsonDocument doc(obj);
        ts << doc.toJson(QJsonDocument::Compact) << '\n';
    }
    return out;
}

// Simple CSV line parser that handles quoted fields and escaped quotes ("")
static QStringList parseCsvLine(const QString& line) {
    QStringList fields;
    QString cur;
    bool inQuotes = false;
    int len = line.length();
    for (int i = 0; i < len; ++i) {
        QChar c = line.at(i);
        if (inQuotes) {
            if (c == '"') {
                // lookahead for escaped quote
                if (i + 1 < len && line.at(i + 1) == '"') {
                    cur.append('"');
                    ++i;  // skip the escaped quote
                } else {
                    inQuotes = false;
                }
            } else {
                cur.append(c);
            }
        } else {
            if (c == '"') {
                inQuotes = true;
            } else if (c == ',') {
                fields.append(cur);
                cur.clear();
            } else {
                cur.append(c);
            }
        }
    }
    // push last field
    fields.append(cur);
    return fields;
}

int DeviceRegistry::importCsv(const QString& csvText) {
    if (csvText.isEmpty()) return 0;
    QTextStream ts(const_cast<QString*>(&csvText), QIODevice::ReadOnly);
    QString line;
    bool headerChecked = false;
    int imported = 0;

    while (!ts.atEnd()) {
        line = ts.readLine();
        if (line.trimmed().isEmpty()) continue;

        // detect header line
        if (!headerChecked) {
            QString lower = line.toLower();
            if (lower.contains("deviceid") && lower.contains("hwinfo")) {
                headerChecked = true;
                continue;  // skip header
            }
            headerChecked = true;  // first non-empty line processed as data if not header
        }

        QStringList cols = parseCsvLine(line);
        // Expect at least 7 columns: deviceId, hwInfo, firmwareVersion, owner, group, iconPath,
    // endpoint, lastSeen
        if (cols.size() < 7) continue;

        DeviceInfo d;
        d.deviceId = cols.at(0).trimmed();
        d.hwInfo = cols.at(1).trimmed();
    // optionally accept endpoint as third column
    if (cols.size() >= 8) d.endpoint = cols.at(2).trimmed();
    int fwIndex = (cols.size() >= 8) ? 3 : 2;
        d.firmwareVersion = cols.at(2).trimmed();
        d.owner = cols.at(3).trimmed();
        d.group = cols.at(4).trimmed();
        d.iconPath = cols.at(5).trimmed();
    QString last = cols.at(6).trimmed();
        if (!last.isEmpty()) {
            QDateTime dt = QDateTime::fromString(last, Qt::ISODate);
            if (dt.isValid())
                d.lastSeen = dt;
            else
                d.lastSeen = QDateTime();
        } else {
            d.lastSeen = QDateTime();
        }

        if (d.deviceId.isEmpty()) continue;  // ignore invalid

        // upsert
        {
            QWriteLocker w(&m_lock);
            m_store.insert(d.deviceId, d);
        }
        // persist to main storage DB if available
        try {
            QSqlDatabase& db = storage::Storage::db();
            if (db.isOpen()) {
                ensureDevicesTable(db);
                persistDeviceToDb(db, d);
            }
        } catch (...) {}
        ++imported;
    }
    return imported;
}

int DeviceRegistry::importJsonNd(const QString& jsonndText) {
    if (jsonndText.isEmpty()) return 0;
    QTextStream ts(const_cast<QString*>(&jsonndText), QIODevice::ReadOnly);
    QString line;
    int imported = 0;

    while (!ts.atEnd()) {
        line = ts.readLine();
        if (line.trimmed().isEmpty()) continue;
        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(line.toUtf8(), &err);
        if (err.error != QJsonParseError::NoError) continue;
        if (!doc.isObject()) continue;
            DeviceInfo d = DeviceInfo::fromJson(doc.object());
        if (d.deviceId.isEmpty()) continue;
        {
            QWriteLocker w(&m_lock);
            m_store.insert(d.deviceId, d);
        }
        try {
            QSqlDatabase& db = storage::Storage::db();
            if (db.isOpen()) {
                ensureDevicesTable(db);
                persistDeviceToDb(db, d);
            }
        } catch (...) {}
        ++imported;
    }
    return imported;
}

int DeviceRegistry::exportToDatabaseFile(const QString& filePath) const {
    if (filePath.isEmpty()) return -1;
    // Open a separate SQLite DB connection to write devices
    const QString connName = QStringLiteral("device_export_%1").arg(QUuid::createUuid().toString());
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connName);
    db.setDatabaseName(filePath);
    if (!db.open()) {
        QSqlDatabase::removeDatabase(connName);
        return -1;
    }

    if (!ensureDevicesTable(db)) {
        db.close();
        QSqlDatabase::removeDatabase(connName);
        return -1;
    }

    int written = 0;
    QReadLocker r(&m_lock);
    QSqlQuery q(db);
    db.transaction();
    for (const auto& d : m_store) {
        if (!persistDeviceToDb(db, d)) continue;
        ++written;
    }
    db.commit();
    db.close();
    QSqlDatabase::removeDatabase(connName);
    return written;
}

int DeviceRegistry::importFromDatabaseFile(const QString& filePath, bool overwrite) {
    if (filePath.isEmpty()) return -1;
    const QString connName = QStringLiteral("device_import_%1").arg(QUuid::createUuid().toString());
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connName);
    db.setDatabaseName(filePath);
    if (!db.open()) {
        QSqlDatabase::removeDatabase(connName);
        return -1;
    }

    if (!ensureDevicesTable(db)) {
        // If table doesn't exist, nothing to import
        db.close();
        QSqlDatabase::removeDatabase(connName);
        return 0;
    }

    QSqlQuery q(db);
    if (!q.exec("SELECT deviceId, hwInfo, firmwareVersion, owner, groupname, iconPath, lastSeen "
                "FROM devices")) {
        db.close();
        QSqlDatabase::removeDatabase(connName);
        return -1;
    }

    int imported = 0;
    while (q.next()) {
        DeviceInfo d;
        d.deviceId = q.value(0).toString();
        d.hwInfo = q.value(1).toString();
            d.endpoint = q.value(2).toString();
        d.firmwareVersion = q.value(2).toString();
        d.owner = q.value(3).toString();
        d.group = q.value(4).toString();
        d.iconPath = q.value(5).toString();
            d.lastSeen = QDateTime::fromString(q.value(6).toString(), Qt::ISODate);
        if (d.deviceId.isEmpty()) continue;

        // insert into memory and optionally persist to main storage DB
        {
            QWriteLocker w(&m_lock);
            if (overwrite || !m_store.contains(d.deviceId)) { m_store.insert(d.deviceId, d); }
        }
        // also persist into main storage DB
        try {
            QSqlDatabase& mainDb = storage::Storage::db();
            if (mainDb.isOpen()) {
                ensureDevicesTable(mainDb);
                persistDeviceToDb(mainDb, d);
            }
        } catch (...) {}
        ++imported;
    }

    db.close();
    QSqlDatabase::removeDatabase(connName);
    return imported;
}

int DeviceRegistry::exportToStorage() const {
    try {
        QSqlDatabase& db = storage::Storage::db();
        if (!db.isOpen()) return -1;
        if (!ensureDevicesTable(db)) return -1;

        int written = 0;
        QReadLocker r(&m_lock);
        QSqlQuery q(db);
        db.transaction();
        for (const auto& d : m_store) {
            if (!persistDeviceToDb(db, d)) continue;
            ++written;
        }
        db.commit();
        return written;
    } catch (...) { return -1; }
}

int DeviceRegistry::importFromStorage(bool overwrite) {
    try {
        QSqlDatabase& db = storage::Storage::db();
        if (!db.isOpen()) return -1;
        if (!ensureDevicesTable(db)) return 0;

        QSqlQuery q(db);
        if (!q.exec("SELECT deviceId, hwInfo, firmwareVersion, owner, groupname, iconPath, "
                    "lastSeen FROM "
                    "devices"))
            return -1;

        int imported = 0;
        while (q.next()) {
            DeviceInfo d;
            d.deviceId = q.value(0).toString();
            d.hwInfo = q.value(1).toString();
            d.endpoint = q.value(2).toString();
            d.firmwareVersion = q.value(2).toString();
            d.owner = q.value(3).toString();
            d.group = q.value(4).toString();
            d.iconPath = q.value(5).toString();
            d.lastSeen = QDateTime::fromString(q.value(6).toString(), Qt::ISODate);
            if (d.deviceId.isEmpty()) continue;

            QWriteLocker w(&m_lock);
            if (overwrite || !m_store.contains(d.deviceId)) {
                m_store.insert(d.deviceId, d);
                ++imported;
            }
        }
        return imported;
    } catch (...) { return -1; }
}

}  // namespace DeviceGateway
