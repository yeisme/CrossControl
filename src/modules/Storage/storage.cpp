#include "storage.h"

namespace storage {

bool Storage::initSqlite(const QString &path) {
    DbConfig cfg;
    cfg.database = path;
    return DbManager::instance().init(cfg);
}

Orm::DatabaseManager &Storage::db() { return DbManager::instance().db(); }

}  // namespace storage
