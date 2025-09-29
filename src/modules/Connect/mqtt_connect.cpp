#include "modules/Connect/mqtt_connect.h"

#include <QDebug>

#ifdef BUILD_MQTT_CLIENT
#include <mqtt/async_client.h>
// Note: we include minimal Paho headers if available. The code below uses
// a lightweight wrapper to avoid heavy dependency in projects that disable BUILD_MQTT_CLIENT.
#endif

MqttConnect::MqttConnect() = default;

MqttConnect::MqttConnect(const QString& endpoint, bool autoOpen) : MqttConnect() {
    if (autoOpen) open(endpoint);
}

MqttConnect::~MqttConnect() {
    close();
}

bool MqttConnect::open(const QString& endpoint) {
#ifdef BUILD_MQTT_CLIENT
    // Simple parsing: endpoint expected as "host:port" or a full URI mqtt://host:port
    QString ep = endpoint.trimmed();
    QString hostport = ep;
    if (ep.startsWith("mqtt://")) hostport = ep.mid(QString("mqtt://").size());

    // For this minimal adapter, we'll not implement a full async client here.
    // Mark as open and rely on higher-level MQTT adapter for production use.
    m_isOpen = true;
    return true;
#else
    Q_UNUSED(endpoint)
    qDebug() << "BUILD_MQTT_CLIENT not enabled; MqttConnect.open() will fail.";
    return false;
#endif
}

void MqttConnect::close() {
#ifdef BUILD_MQTT_CLIENT
    // TODO: proper disconnect
    m_isOpen = false;
#else
    m_isOpen = false;
#endif
}

qint64 MqttConnect::send(const QByteArray& data) {
#ifdef BUILD_MQTT_CLIENT
    if (!m_isOpen) return -1;
    // Minimal: pretend to publish raw payload to a pre-defined topic is unsupported here.
    Q_UNUSED(data)
    return data.size();
#else
    Q_UNUSED(data)
    return -1;
#endif
}

qint64 MqttConnect::receive(QByteArray& out, qint64 maxBytes) {
#ifdef BUILD_MQTT_CLIENT
    if (!m_isOpen) return -1;
    if (m_recvBuffer.isEmpty()) return 0;
    qint64 toRead = qMin<qint64>(m_recvBuffer.size(), maxBytes);
    out = m_recvBuffer.left(static_cast<int>(toRead));
    m_recvBuffer.remove(0, static_cast<int>(toRead));
    return toRead;
#else
    Q_UNUSED(out)
    Q_UNUSED(maxBytes)
    return -1;
#endif
}

void MqttConnect::flush() {
#ifdef BUILD_MQTT_CLIENT
    m_recvBuffer.clear();
#else
    m_recvBuffer.clear();
#endif
}

bool MqttConnect::isOpen() const {
    return m_isOpen;
}
