#pragma once

#include <QDateTime>
#include <QMap>
#include <QString>
#include <QVariantMap>
#include <QVector>
#include <optional>

namespace DeviceGateway {

struct DeviceInfo {
    QString id;
    QString name;
    QString status;
    // Device endpoint, e.g. "tcp://host:port" or "COM3:115200"
    QString endpoint;
    // Optional device type (e.g. "camera", "thermostat")
    QString type;
    // 硬件信息字符串（例如型号/序列号）
    QString hw_info;
    // 固件版本字符串
    QString firmware_version;
    // 逻辑拥有者和组
    QString owner;
    QString group;
    // 最后一次看到的时间戳（如果未知可为 null）
    QDateTime lastSeen;
    QVariantMap metadata;
};

class DeviceRegistry {
   public:
    DeviceRegistry() = default;
    ~DeviceRegistry() = default;

    bool addDevice(const DeviceInfo& dev);
    bool removeDevice(const QString& id);
    std::optional<DeviceInfo> getDevice(const QString& id) const;
    QVector<DeviceInfo> listDevices() const;
    void clear();

   private:
    QMap<QString, DeviceInfo> m_devices;
};

}  // namespace DeviceGateway
