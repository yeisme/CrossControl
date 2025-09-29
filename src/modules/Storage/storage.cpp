#include "storage.h"

#include <QSqlDatabase>

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

}  // namespace storage
