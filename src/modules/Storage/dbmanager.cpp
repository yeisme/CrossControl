#include "dbmanager.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>

using Orm::DB;

namespace storage {

DbManager &DbManager::instance() {
    static DbManager inst;
    return inst;
}

bool DbManager::init(const DbConfig &cfg, bool force) noexcept {
    if (m_initialized && !force) return true;

    try {
        QVariantHash connectionConfig{
            {"driver", cfg.driver},
            {"database", cfg.database},
            {"foreign_key_constraints", cfg.foreignKeys},
            {"check_database_exists", cfg.checkDatabaseExists},
            {"qt_timezone", QVariant::fromValue(cfg.qtTimeZone)},
            {"return_qdatetime", cfg.returnQDateTime},
            {"prefix", cfg.prefix},
            {"prefix_indexes", cfg.prefixIndexes},
        };

        m_manager = DB::create(connectionConfig);
        m_initialized = (m_manager != nullptr);
        m_cfg = cfg;
        return m_initialized;
    } catch (const Orm::Exceptions::OrmError &) {
        qWarning() << "TinyORM init failed (OrmError)";
    } catch (const std::exception &e) {
        qWarning() << "DB init std::exception:" << e.what();
    } catch (...) { qWarning() << "DB init unknown exception"; }
    return false;
}

Orm::DatabaseManager &DbManager::db() {
    if (!m_initialized) throw std::runtime_error("DbManager not initialized");
    return *m_manager;
}

void DbManager::close() {
    if (!m_initialized) return;

    try {
        m_manager.reset();
    } catch (...) {}
    m_initialized = false;
}

}  // namespace storage
