#include "modules/DeviceGateway/device_registry.h"

#include <optional>
#include <QDateTime>

namespace DeviceGateway {

bool DeviceRegistry::addDevice(const DeviceInfo& dev) {
    if (dev.id.isEmpty()) return false;
    m_devices.insert(dev.id, dev);
    return true;
}

bool DeviceRegistry::removeDevice(const QString& id) {
    return m_devices.remove(id) > 0;
}

std::optional<DeviceInfo> DeviceRegistry::getDevice(const QString& id) const {
    if (!m_devices.contains(id)) return std::nullopt;
    return m_devices.value(id);
}

QVector<DeviceInfo> DeviceRegistry::listDevices() const {
    QVector<DeviceInfo> out;
    out.reserve(m_devices.size());
    for (auto it = m_devices.cbegin(); it != m_devices.cend(); ++it) { out.append(it.value()); }
    return out;
}

void DeviceRegistry::clear() {
    m_devices.clear();
}

}  // namespace DeviceGateway
