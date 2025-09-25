#include "storage.h"
#include <QSqlDatabase>

namespace storage {

bool Storage::initSqlite(const QString &path) {
    DbConfig cfg;
    cfg.database = path;
    return DbManager::instance().init(cfg);
}

QSqlDatabase &Storage::db() { return DbManager::instance().db(); }

}  // namespace storage
