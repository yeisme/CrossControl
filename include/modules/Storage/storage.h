#pragma once

#include <QtGlobal>
#include <QtSql/QSqlDatabase>

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

    // Save an action for a given type (e.g., "http", "tcp").
    // Returns true on success. Name is used as primary key (overwrite if exists).
    static bool saveAction(const QString& type, const QString& name, const QByteArray& payload);

    // Load list of actions for a given type. Returns vector of (name, created_at).
    static QVector<QPair<QString, QString>> loadActions(const QString& type);

    // Load payload for a named action of given type. Returns empty QByteArray on failure.
    static QByteArray loadActionPayload(const QString& type, const QString& name);

    // Migrate rows from legacy 'actions' table into per-type tables. Returns number migrated.
    static int migrateOldActions();

    // Delete a named action of given type. Returns true on success.
    static bool deleteAction(const QString& type, const QString& name);

    // Rename an action (oldName -> newName) for given type. Returns true on success.
    static bool renameAction(const QString& type, const QString& oldName, const QString& newName);
};

}  // namespace storage
