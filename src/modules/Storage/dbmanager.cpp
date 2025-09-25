#include "dbmanager.h"

#include "logging.h"
#include "spdlog/spdlog.h"
#include <QDir>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QUuid>

namespace storage {

DbManager& DbManager::instance() {
    static DbManager inst;
    return inst;
}

bool DbManager::init(const DbConfig& cfg, bool force) noexcept {
    if (m_initialized && !force) return true;

    try {
        // If a previous connection exists, close it first when forcing
        if (m_initialized && force) close();

        // Build a unique connection name to avoid clashing with others
        m_connectionName =
            QStringLiteral("crosscontrol_connection_%1").arg(QUuid::createUuid().toString());

        // Ensure parent folder for file-based DB exists
        if (cfg.driver.compare("QSQLITE", Qt::CaseInsensitive) == 0) {
            QFileInfo fi(cfg.database);
            QDir dir = fi.dir();
            if (!dir.exists()) dir.mkpath(".");
        }

        m_db = QSqlDatabase::addDatabase(cfg.driver, m_connectionName);
        m_db.setDatabaseName(cfg.database);

        if (!m_db.open()) {
            spdlog::warn("Failed to open database: {}", m_db.lastError().text().toStdString());

            // If using SQLite, try fallback to AppDataLocation (writable)
            if (cfg.driver.compare("QSQLITE", Qt::CaseInsensitive) == 0) {
                const QString appDataDir =
                    QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
                if (!appDataDir.isEmpty()) {
                    QDir dir(appDataDir);
                    if (!dir.exists()) dir.mkpath(".");
                    const QString fallbackPath = dir.filePath(QFileInfo(cfg.database).fileName());
                    spdlog::warn("Attempting fallback DB path: {}", fallbackPath.toStdString());

                    // remove the failed connection and add a new one for fallback
                    QSqlDatabase::removeDatabase(m_connectionName);
                    m_connectionName = QStringLiteral("crosscontrol_connection_%1")
                                           .arg(QUuid::createUuid().toString());
                    m_db = QSqlDatabase::addDatabase(cfg.driver, m_connectionName);
                    m_db.setDatabaseName(fallbackPath);
                    if (m_db.open()) {
                        spdlog::warn("Opened fallback DB: {}", fallbackPath.toStdString());
                        m_cfg.database = fallbackPath;
                    } else {
                        spdlog::warn("Fallback DB open also failed: {}", m_db.lastError().text().toStdString());
                        QSqlDatabase::removeDatabase(m_connectionName);
                        m_connectionName.clear();
                        return false;
                    }
                } else {
                    QSqlDatabase::removeDatabase(m_connectionName);
                    m_connectionName.clear();
                    return false;
                }
            } else {
                QSqlDatabase::removeDatabase(m_connectionName);
                m_connectionName.clear();
                return false;
            }
        }

        // Set PRAGMA foreign_keys = ON for SQLite if requested
        if (cfg.driver.compare("QSQLITE", Qt::CaseInsensitive) == 0 && cfg.foreignKeys) {
            QSqlQuery q(m_db);
            if (!q.exec("PRAGMA foreign_keys = ON;")) {
                spdlog::warn("Failed to enable foreign_keys: {}", q.lastError().text().toStdString());
            }
        }

        m_cfg = cfg;
        m_initialized = true;
        return true;
    } catch (const std::exception& e) {
        spdlog::warn("DB init exception: {}", e.what());
    } catch (...) { spdlog::warn("DB init unknown exception"); }
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
