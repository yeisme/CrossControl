#include "modules/DeviceGateway/device_gateway.h"

#include "spdlog/spdlog.h"

namespace device_gateway {

DeviceGateway::DeviceGateway() {
    spdlog::info("DeviceGateway initialized (placeholder)");
#if BUILD_MQTT_CLIENT
    spdlog::info("DeviceGateway: MQTT client support enabled at compile time");
#endif
}

DeviceGateway::~DeviceGateway() {
    spdlog::info("DeviceGateway shutdown");
}

void DeviceGateway::start() {
    spdlog::info("DeviceGateway start() called");
}

}  // namespace device_gateway
