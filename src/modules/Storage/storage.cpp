#include "storage.h"

#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>

#include "dbmanager.h"
// use config to store requested DB path
#include "modules/Config/config.h"

namespace storage {

bool Storage::initSqlite(const QString& path) {
    // 将请求的路径写入 config，使得 DbManager 从统一的 config 中读取
    config::ConfigManager::instance().setValue("Storage/database", path);

    return DbManager::instance().init();
}

QSqlDatabase& Storage::db() {
    return DbManager::instance().db();
}

static QString actionsTableForType(const QString& type) {
    if (type.compare("http", Qt::CaseInsensitive) == 0) return QStringLiteral("http_actions");
    if (type.compare("tcp", Qt::CaseInsensitive) == 0) return QStringLiteral("tcp_actions");
    if (type.compare("udp", Qt::CaseInsensitive) == 0) return QStringLiteral("udp_actions");
    if (type.compare("serial", Qt::CaseInsensitive) == 0) return QStringLiteral("serial_actions");
    return QStringLiteral("actions");
}

bool Storage::saveAction(const QString& type, const QString& name, const QByteArray& payload) {
    try {
        QSqlDatabase& d = DbManager::instance().db();
        QSqlQuery q(d);
        QString tbl = actionsTableForType(type);
        // create table if not exists
        q.exec(QString("CREATE TABLE IF NOT EXISTS %1 (name TEXT PRIMARY KEY, payload BLOB, created_at TEXT)").arg(tbl));
        QSqlQuery q2(d);
        q2.prepare(QString("INSERT OR REPLACE INTO %1 (name,payload,created_at) VALUES (?,?,datetime('now'))").arg(tbl));
        q2.addBindValue(name);
        q2.addBindValue(payload);
        if (!q2.exec()) {
            return false;
        }
        return true;
    } catch (...) {
        return false;
    }
}

QVector<QPair<QString, QString>> Storage::loadActions(const QString& type) {
    QVector<QPair<QString, QString>> out;
    try {
        QSqlDatabase& d = DbManager::instance().db();
        QSqlQuery q(d);
        QString tbl = actionsTableForType(type);
        q.exec(QString("SELECT name, created_at FROM %1 ORDER BY created_at DESC LIMIT 100").arg(tbl));
        while (q.next()) {
            QString name = q.value(0).toString();
            QString created = q.value(1).toString();
            out.append(qMakePair(name, created));
        }
    } catch (...) {
    }
    return out;
}

QByteArray Storage::loadActionPayload(const QString& type, const QString& name) {
    try {
        QSqlDatabase& d = DbManager::instance().db();
        QSqlQuery q(d);
        QString tbl = actionsTableForType(type);
        q.prepare(QString("SELECT payload FROM %1 WHERE name = ?").arg(tbl));
        q.addBindValue(name);
        if (!q.exec() || !q.next()) return QByteArray();
        return q.value(0).toByteArray();
    } catch (...) {
        return QByteArray();
    }
}

int Storage::migrateOldActions() {
    int migrated = 0;
    try {
        QSqlDatabase& d = DbManager::instance().db();
        QSqlQuery q(d);
        // Check if legacy table exists
        q.exec("SELECT name FROM sqlite_master WHERE type='table' AND name='actions'");
        if (!q.next()) return 0;
        QSqlQuery q2(d);
        q2.exec("SELECT name, type, payload FROM actions");
        while (q2.next()) {
            QString name = q2.value(0).toString();
            QString type = q2.value(1).toString();
            QByteArray payload = q2.value(2).toByteArray();
            if (saveAction(type, name, payload)) ++migrated;
        }
    } catch (...) {
    }
    return migrated;
}

bool Storage::deleteAction(const QString& type, const QString& name) {
    try {
        QSqlDatabase& d = DbManager::instance().db();
        QSqlQuery q(d);
        QString tbl = actionsTableForType(type);
        q.prepare(QString("DELETE FROM %1 WHERE name = ?").arg(tbl));
        q.addBindValue(name);
        return q.exec();
    } catch (...) {
        return false;
    }
}

bool Storage::renameAction(const QString& type, const QString& oldName, const QString& newName) {
    try {
        QSqlDatabase& d = DbManager::instance().db();
        // load payload
        QSqlQuery q(d);
        QString tbl = actionsTableForType(type);
        q.prepare(QString("SELECT payload FROM %1 WHERE name = ?").arg(tbl));
        q.addBindValue(oldName);
        if (!q.exec() || !q.next()) return false;
        QByteArray payload = q.value(0).toByteArray();

        // save under new name (INSERT OR REPLACE)
        if (!saveAction(type, newName, payload)) return false;

        // delete old
        QSqlQuery q2(d);
        q2.prepare(QString("DELETE FROM %1 WHERE name = ?").arg(tbl));
        q2.addBindValue(oldName);
        q2.exec();
        return true;
    } catch (...) {
        return false;
    }
}

}  // namespace storage
