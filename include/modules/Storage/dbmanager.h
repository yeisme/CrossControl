#pragma once

#include <QDateTime>
#include <QSharedPointer>
#include <QSqlDatabase>
#include <QString>
#include <QTimeZone>
#include <QVariant>

namespace storage {

// 配置结构体，用于初始化数据库连接参数。
struct DbConfig {
    QString driver{"QSQLITE"};
    QString database{"./crosscontrol.db"};  // 例如: ./crosscontrol.db
    bool foreignKeys{true};
    bool checkDatabaseExists{true};
    QTimeZone qtTimeZone{QTimeZone::UTC};
    bool returnQDateTime{true};       // 返回 QDateTime 而非 QString
    QString prefix{"crosscontrol_"};  // 表前缀
    bool prefixIndexes{false};
};

/**
 * @brief 基于 Qt QSql 的数据库管理类（单例）
 *
 */
class DbManager {
   public:
    // 采用懒加载单例（线程安全的 C++11 局部静态）
    static DbManager& instance();

    // 初始化连接(可多次调用，后续重复调用将忽略除非 force=true)
    bool init(const DbConfig& cfg, bool force = false) noexcept;

    // 是否已经初始化
    bool initialized() const noexcept { return m_initialized; }

    // 取得底层的 QSqlDatabase (throws if not initialized)
    QSqlDatabase& db();

    // 关闭连接
    void close();

   private:
    DbManager() = default;
    ~DbManager() = default;

    DbManager(const DbManager&) = delete;
    DbManager& operator=(const DbManager&) = delete;

    bool m_initialized{false};
    QSqlDatabase m_db;         // 底层数据库对象（非共享指针）
    QString m_connectionName;  // 连接名
    DbConfig m_cfg;            // 保存当前配置
};

}  // namespace storage
