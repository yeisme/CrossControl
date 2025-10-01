#pragma once

#include <QDateTime>
#include <QJsonObject>
#include <QMap>
#include <QList>
#include <QReadWriteLock>
#include <QString>
#include <optional>

namespace DeviceGateway {

struct DeviceInfo {
    QString deviceId;
    QString hwInfo;
    // canonical connection endpoint (eg. "tcp://host:port", "udp://host:port", "COM3:115200")
    QString endpoint;
    QString firmwareVersion;
    QString owner;
    QString group;
    // path to an icon file associated with the device (stored in app data)
    QString iconPath;
    QDateTime lastSeen;

    QJsonObject toJson() const;
    static DeviceInfo fromJson(const QJsonObject& obj);
};

/**
 * @brief 设备注册表，提供最小的增删查改与导出功能
 *
 * 这是一个线程安全的内存注册表，适合作为 DeviceGateway 的本地存储层。
 */
class DeviceRegistry {
   public:
    DeviceRegistry();

    // 添加或更新设备信息（基于 deviceId）
    // persistToStorage 默认为 true，将同时写入 Storage 层的 devices 表以保证持久化
    void upsert(const DeviceInfo& info, bool persistToStorage = true);

    // 返回 deviceId 对应的信息（若不存在则返回 std::nullopt）
    std::optional<DeviceInfo> get(const QString& deviceId) const;

    // 删除设备，返回是否删除成功
    bool remove(const QString& deviceId);

    // 列出所有设备
    QList<DeviceInfo> list() const;

    // 导出为 CSV 文本，第一行为表头
    QString exportCsv() const;
    // 从 CSV 文本导入，返回成功导入的条目数
    int importCsv(const QString& csvText);

    // 导出 jsonnd 格式，每行一个 JSON 对象
    QString exportJsonNd() const;
    // 从 jsonnd 文本导入，返回成功导入的条目数
    int importJsonNd(const QString& jsonndText);

    // 将内存注册表的所有条目导出到一个独立的数据库文件（SQLite），
    // 返回写入的条目数，错误时返回 -1
    int exportToDatabaseFile(const QString& filePath) const;

    // 从独立数据库文件导入设备表到当前注册表，返回导入的条目数，错误时返回 -1
    // 如果 overwrite 为 true，则会覆盖已有条目
    int importFromDatabaseFile(const QString& filePath, bool overwrite = true);

    // 将内存注册表的所有条目导出到主 storage 数据库（将在该 DB 中创建/使用 devices 表）
    // 返回写入的条目数，错误时返回 -1
    int exportToStorage() const;

    // 从主 storage 数据库的 devices 表导入到当前注册表，返回导入的条目数，错误时返回 -1
    // 如果 overwrite 为 true，则会覆盖已有条目
    int importFromStorage(bool overwrite = true);

   private:
    mutable QReadWriteLock m_lock;
    QMap<QString, DeviceInfo> m_store;
};

}  // namespace DeviceGateway
