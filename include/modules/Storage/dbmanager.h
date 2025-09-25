#pragma once

#include <QDateTime>
#include <QString>
#include <QVariant>
#include <QVector>
#include <memory>
#include <orm/db.hpp>
#include <orm/exceptions/ormerror.hpp>

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
 * @brief 数据库管理类
 *
 */
class DbManager {
   public:
    // 采用懒加载单例（线程安全的 C++11 局部静态）
    static DbManager &instance();

    // 初始化连接(可多次调用，后续重复调用将忽略除非 force=true)
    bool init(const DbConfig &cfg, bool force = false) noexcept;

    // 是否已经初始化
    bool initialized() const noexcept { return m_initialized; }

    // 取得底层的 Orm::DatabaseManager 共享指针
    Orm::DatabaseManager &db();

    // 关闭连接
    void close();

   private:
    DbManager() = default;
    ~DbManager() = default;

    DbManager(const DbManager &) = delete;
    DbManager &operator=(const DbManager &) = delete;

    bool m_initialized{false};
    std::shared_ptr<Orm::DatabaseManager> m_manager;  // shared_ptr 得到的管理器
    DbConfig m_cfg;                                   // 保存当前配置
};

}  // namespace storage
