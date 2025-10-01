#pragma once

#include <memory>

namespace DeviceGateway {

class DeviceRegistry; // forward-declare registry from device_registry.h
class RestServer;     // forward-declare rest server from rest_server.h

class DeviceGateway {
   public:
    DeviceGateway();
    ~DeviceGateway();

    void start();
    void stop();

    // Accessors for runtime components. Return raw pointers; ownership remains with
    // DeviceGateway instance.
    DeviceRegistry* registry() const { return registry_.get(); }
    RestServer* restServer() const { return restServer_.get(); }

   private:
    // Local in-memory registry for devices (defined in device_registry.h)
    std::unique_ptr<DeviceRegistry> registry_;

    // REST server for remote registration (defined in rest_server.h)
    std::unique_ptr<RestServer> restServer_;

#if BUILD_MQTT_CLIENT
    // Placeholder for MQTT client instance (implementation-specific)
    void* mqtt_client_{nullptr};
#endif
};

}  // namespace DeviceGateway
