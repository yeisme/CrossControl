#pragma once

#include <QSqlDatabase>
#include <QtGlobal>

#ifdef Storage_EXPORTS
#define STORAGE_EXPORT Q_DECL_EXPORT
#else
#define STORAGE_EXPORT Q_DECL_IMPORT
#endif

namespace storage {

/**
 * @brief Facade-style helper for initializing and accessing the project's database
 *        using Qt's QSql API.
 */
class STORAGE_EXPORT Storage final {
   public:
    // Initialize SQLite database (creates a connection if needed)
    static bool initSqlite(const QString& path);

    // Obtain reference to underlying QSqlDatabase (throws if not initialized)
    static QSqlDatabase& db();
};

}  // namespace storage
