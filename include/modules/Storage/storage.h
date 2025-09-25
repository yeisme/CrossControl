#pragma once

#include "dbmanager.h"
#include <QSqlDatabase>

namespace storage {

/**
 * @brief Facade-style helper for initializing and accessing the project's database
 *        using Qt's QSql API.
 */
class Storage final {
   public:
    // Initialize SQLite database (creates a connection if needed)
    static bool initSqlite(const QString &path);

    // Obtain reference to underlying QSqlDatabase (throws if not initialized)
    static QSqlDatabase &db();
};

}  // namespace storage
