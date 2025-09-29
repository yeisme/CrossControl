#include "modules/DeviceGateway/device_registry.h"

#include <QIODevice>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStringList>
#include <QTextStream>

namespace DeviceGateway {

QJsonObject DeviceInfo::toJson() const {
    QJsonObject o;
    o["deviceId"] = deviceId;
    o["hwInfo"] = hwInfo;
    o["firmwareVersion"] = firmwareVersion;
    o["owner"] = owner;
    o["group"] = group;
    o["lastSeen"] = lastSeen.toString(Qt::ISODate);
    return o;
}

DeviceInfo DeviceInfo::fromJson(const QJsonObject& obj) {
    DeviceInfo d;
    d.deviceId = obj.value("deviceId").toString();
    d.hwInfo = obj.value("hwInfo").toString();
    d.firmwareVersion = obj.value("firmwareVersion").toString();
    d.owner = obj.value("owner").toString();
    d.group = obj.value("group").toString();
    d.lastSeen = QDateTime::fromString(obj.value("lastSeen").toString(), Qt::ISODate);
    return d;
}

DeviceRegistry::DeviceRegistry() {}

void DeviceRegistry::upsert(const DeviceInfo& info) {
    QWriteLocker w(&m_lock);
    m_store.insert(info.deviceId, info);
}

std::optional<DeviceInfo> DeviceRegistry::get(const QString& deviceId) const {
    QReadLocker r(&m_lock);
    if (!m_store.contains(deviceId)) return std::nullopt;
    return m_store.value(deviceId);
}

bool DeviceRegistry::remove(const QString& deviceId) {
    QWriteLocker w(&m_lock);
    return m_store.remove(deviceId) > 0;
}

QList<DeviceInfo> DeviceRegistry::list() const {
    QReadLocker r(&m_lock);
    return m_store.values();
}

QString DeviceRegistry::exportCsv() const {
    QReadLocker r(&m_lock);
    QString out;
    QTextStream ts(&out);
    ts << "deviceId,hwInfo,firmwareVersion,owner,group,lastSeen\n";
    for (const auto& d : m_store) {
        QString id = d.deviceId;
        QString hw = d.hwInfo;
        QString fw = d.firmwareVersion;
        QString owner = d.owner;
        QString group = d.group;
        id.replace("\"", "\"\"");
        hw.replace("\"", "\"\"");
        fw.replace("\"", "\"\"");
        owner.replace("\"", "\"\"");
        group.replace("\"", "\"\"");

        ts << '"' << id << '"' << ',';
        ts << '"' << hw << '"' << ',';
        ts << '"' << fw << '"' << ',';
        ts << '"' << owner << '"' << ',';
        ts << '"' << group << '"' << ',';
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
        // Expect at least 6 columns: deviceId, hwInfo, firmwareVersion, owner, group, lastSeen
        if (cols.size() < 6) continue;

        DeviceInfo d;
        d.deviceId = cols.at(0).trimmed();
        d.hwInfo = cols.at(1).trimmed();
        d.firmwareVersion = cols.at(2).trimmed();
        d.owner = cols.at(3).trimmed();
        d.group = cols.at(4).trimmed();
        QString last = cols.at(5).trimmed();
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
        ++imported;
    }
    return imported;
}

}  // namespace DeviceGateway
