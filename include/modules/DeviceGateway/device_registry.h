#pragma once

#include <QDateTime>
#include <QJsonObject>
#include <QList>
#include <QReadWriteLock>
#include <QString>
#include <optional>

namespace DeviceGateway {

struct DeviceInfo {
    QString deviceId;
    QString hwInfo;
    QString firmwareVersion;
    QString owner;
    QString group;
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
    void upsert(const DeviceInfo& info);

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

   private:
    mutable QReadWriteLock m_lock;
    QMap<QString, DeviceInfo> m_store;
};

}  // namespace DeviceGateway
