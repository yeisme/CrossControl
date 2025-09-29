#include "modules/DeviceGateway/device_gateway.h"
#include <memory>
#include "modules/DeviceGateway/device_registry.h"
#include "modules/DeviceGateway/rest_server.h"

#include "spdlog/spdlog.h"

namespace DeviceGateway {

DeviceGateway::DeviceGateway() {
    spdlog::info("DeviceGateway initialized");
    registry_ = std::make_unique<::DeviceGateway::DeviceRegistry>();
    restServer_ = std::make_unique<::DeviceGateway::RestServer>(*registry_, 8080);
#if BUILD_MQTT_CLIENT
    spdlog::info("DeviceGateway: MQTT client support enabled at compile time");
#endif
}

DeviceGateway::~DeviceGateway() {
    stop();
    spdlog::info("DeviceGateway shutdown");
}

void DeviceGateway::start() {
    spdlog::info("DeviceGateway start() called");
    if (restServer_) restServer_->start();
}

void DeviceGateway::stop() {
    if (restServer_) restServer_->stop();
}

}  // namespace DeviceGateway
