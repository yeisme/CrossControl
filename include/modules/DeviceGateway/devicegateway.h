#pragma once

namespace device_gateway {

class DeviceGateway {
public:
    DeviceGateway();
    ~DeviceGateway();

    void start();

private:
#if BUILD_MQTT_CLIENT
    // Placeholder for MQTT client instance (implementation-specific)
    void* mqtt_client_{nullptr};
#endif
};

} // namespace device_gateway
