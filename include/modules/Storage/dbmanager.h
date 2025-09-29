#pragma once

#include <QDateTime>
#include <QSharedPointer>
#include <QSqlDatabase>
#include <QString>
#include <QTimeZone>
#include <QVariant>

namespace storage {

// 配置由 config 模块提供（Storage/* 键），不再使用本地结构体

/**
 * @brief 基于 Qt QSql 的数据库管理类（单例）
 *
 */
class DbManager {
   public:
    // 采用懒加载单例（线程安全的 C++11 局部静态）
    static DbManager& instance();

    // 初始化连接(可多次调用，后续重复调用将忽略除非 force=true)
    // 现在从 config::ConfigManager 读取配置（Storage/driver, Storage/database, Storage/foreignKeys
    // 等） 如果 config 不可用或必要配置缺失，init 会返回 false（应用应当停止启动）
    bool init(bool force = false) noexcept;

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
    // 配置从 config 模块读取，不在此保存
};

}  // namespace storage
