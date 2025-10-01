#pragma once

#include "modules/DeviceGateway/device_registry.h"

// reuse Connect interfaces for device endpoint connections
#include "modules/Connect/connect_factory.h"
#include "modules/Connect/iface_connect.h"

namespace DeviceGateway {

class DeviceGateway {
   public:
    DeviceGateway();
    ~DeviceGateway();

    DeviceRegistry& registry();

    // Initialize internal services (e.g. REST server). Returns true when
    // successfully initialized.
    bool init();
    // Start/stop REST server with explicit port (preserves legacy init() behavior)
    bool startRest(unsigned short port = 8080);
    void stopRest();
    void shutdown();
    bool isRestRunning() const;

    // Add/remove devices with optional immediate connection setup.
    bool addDeviceWithConnect(const DeviceInfo& dev);
    bool removeDeviceAndClose(const QString& id);

    // Persistence: store device metadata into Storage (SQLite)
    bool persistDevice(const DeviceInfo& dev);
    QVector<DeviceInfo> loadDevicesFromStorage() const;

    // Export helpers
    bool exportDevicesCsv(const QString& path) const;
    bool exportDevicesJsonND(const QString& path) const;
    // Import helpers
    bool importDevicesCsv(const QString& path, bool createConnections = false);
    bool importDevicesJsonND(const QString& path, bool createConnections = false);

   private:
    DeviceRegistry m_registry;
    // REST 服务器是可选的，如果 Drogon 不可用，可以为 null
    void* m_restServer{nullptr};

    // 从设备ID映射到创建的连接实例
    QMap<QString, IConnectPtr> m_connections;

    // 确保在配置的存储数据库中存在设备表
    bool ensureDevicesTable();
};

}  // namespace DeviceGateway
