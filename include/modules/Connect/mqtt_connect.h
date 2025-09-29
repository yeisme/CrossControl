#pragma once

#include <QString>

#include "iface_connect.h"

/**
 * @file mqtt_connect.h
 * @brief 基于可选的 Paho MQTT C++ 客户端的 IConnect 适配器声明
 *
 * 该实现受编译开关 BUILD_MQTT_CLIENT 控制。如果未启用该选项，
 * 源文件将不链接 Paho，并且 connect_factory 会忽略 mqtt scheme。
 */
class MqttConnect : public IConnect {
   public:
    MqttConnect();
    explicit MqttConnect(const QString& endpoint, bool autoOpen = true);
    ~MqttConnect() override;

    bool open(const QString& endpoint) override;
    void close() override;
    qint64 send(const QByteArray& data) override;
    qint64 receive(QByteArray& out, qint64 maxBytes = 4096) override;
    void flush() override;
    bool isOpen() const override;

   private:
    // 内部实现对具体 MQTT 客户端（Paho）封装；使用 void* 避免直接依赖头
    void* m_client{nullptr};
    bool m_isOpen{false};
    QByteArray m_recvBuffer;
};
