#include "dbmanager.h"

#include <QDir>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QUuid>

#include "logging.h"
// 使用 config 模块提供配置
#include "modules/Config/config.h"

namespace storage {

auto l = logging::Logger("DbManager");

DbManager& DbManager::instance() {
    static DbManager inst;
    return inst;
}

bool DbManager::init(bool force) noexcept {
    if (m_initialized && !force) return true;

    try {
        // If a previous connection exists, close it first when forcing
        if (m_initialized && force) close();

        // Build a unique connection name to avoid clashing with others
        m_connectionName =
            QStringLiteral("crosscontrol_connection_%1").arg(QUuid::createUuid().toString());

        // 从 config 模块读取 Storage 相关配置
        auto &cfgm = config::ConfigManager::instance();
        // 必要配置项：driver, database（如果缺失则写入默认值）
        const QString driver = cfgm.setOrDefault("Storage/driver", QString("QSQLITE")).toString();
        const QString database = cfgm.setOrDefault("Storage/database", QString()).toString();
        const bool foreignKeys = cfgm.setOrDefault("Storage/foreignKeys", true).toBool();

        if (database.isEmpty()) {
            l.info("Storage/database is empty in config even after setOrDefault");
            return false;
        }

        l.info("Initializing database with driver '{}' at '{}'", driver.toStdString(),
               database.toStdString());

        // Ensure parent folder for file-based DB exists
        if (driver.compare("QSQLITE", Qt::CaseInsensitive) == 0) {
            QFileInfo fi(database);
            QDir dir = fi.dir();
            if (!dir.exists()) dir.mkpath(".");
        }

        m_db = QSqlDatabase::addDatabase(driver, m_connectionName);
        m_db.setDatabaseName(database);

        if (!m_db.open()) {
            l.warn("Failed to open database: {}", m_db.lastError().text().toStdString());

            // 如果打开失败则直接失败（不再 fallback 到程序目录）
            l.warn("Failed to open database (no fallback). Error: {}",
                   m_db.lastError().text().toStdString());
            QSqlDatabase::removeDatabase(m_connectionName);
            m_connectionName.clear();
            return false;
        }

        // Set PRAGMA foreign_keys = ON for SQLite if requested
        if (cfgm.getString("Storage/driver", QString("QSQLITE")).compare("QSQLITE", Qt::CaseInsensitive) ==
            0 && foreignKeys) {
            QSqlQuery q(m_db);
            if (!q.exec("PRAGMA foreign_keys = ON;")) {
                l.warn("Failed to enable foreign_keys: {}", q.lastError().text().toStdString());
            }
        }
        // 不再保存本地结构体 m_cfg
        m_initialized = true;
        return true;
    } catch (const std::exception& e) { l.warn("DB init exception: {}", e.what()); } catch (...) {
        l.warn("DB init unknown exception");
    }
    return false;
}

QSqlDatabase& DbManager::db() {
    if (!m_initialized) throw std::runtime_error("DbManager not initialized");
    return m_db;
}

void DbManager::close() {
    if (!m_initialized) return;

    try {
        if (m_db.isOpen()) m_db.close();
        if (!m_connectionName.isEmpty()) QSqlDatabase::removeDatabase(m_connectionName);
    } catch (...) {}
    m_connectionName.clear();
    m_initialized = false;
}

}  // namespace storage
