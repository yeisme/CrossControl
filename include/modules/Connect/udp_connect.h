#pragma once

#include <QString>
#include <QUdpSocket>

#include "iface_connect.h"

/**
 * @file udp_connect.h
 * @brief 基于 QUdpSocket 的 UDP 连接实现声明
 *
 * UdpConnect 支持两种模式：
 *  - 指定远端："host:port"，发送时会向该远端发送数据，并可接收来自任意地址的数据；
 *  - 监听模式："0.0.0.0:port" 或 "*:port"，仅绑定本地端口用于接收来自任意远端的数据。
 */
class UdpConnect : public IConnect {
   public:
    UdpConnect();
    /**
     * @brief 构造并可选择根据 endpoint 自动打开 UDP（bind 或指定远端）
     * @param endpoint 形如 "host:port" 或 "*:port" 或 ":port"
     * @param autoOpen 若为 true 则在构造时尝试 open(endpoint)
     */
    explicit UdpConnect(const QString& endpoint, bool autoOpen = true);
    ~UdpConnect() override;

    bool open(const QString& endpoint) override;
    void close() override;
    qint64 send(const QByteArray& data) override;
    qint64 receive(QByteArray& out, qint64 maxBytes = 4096) override;
    void flush() override;
    bool isOpen() const override;

   private:
    QUdpSocket m_socket;
    QString m_remoteHost;
    quint16 m_remotePort{0};
    bool m_hasRemote{false};
    bool m_bound{false};
};
