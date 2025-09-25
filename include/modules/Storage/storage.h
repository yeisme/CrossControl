// Simple storage wrapper for database operations.
#pragma once

#include "dbmanager.h"

namespace storage {

/**
 * @brief Facade-style helper for initializing and accessing the TinyORM
 *        DatabaseManager used inside the project.
 */
class Storage final {
   public:
    // Initialize SQLite database (creates a connection if needed)
    static bool initSqlite(const QString &path);

    // Obtain reference to underlying Orm::DatabaseManager (throws if not initialized)
    static Orm::DatabaseManager &db();
};

}  // namespace storage
