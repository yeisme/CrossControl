#pragma once

#include <QTcpSocket>

#include "iface_connect_qt.h"

/**
 * @file tcp_connect_qt.h
 * @brief 基于 QTcpSocket 的 Qt 异步连接实现声明
 *
 * TcpConnectQt 实现 IConnectQt，使用 QTcpSocket 提供异步事件（connected,
 * disconnected, readyRead, error）并对外发射统一信号。
 */
class TcpConnectQt : public IConnectQt {
    Q_OBJECT
   public:
    /**
     * @brief 构造函数
     * @param parent QObject 父对象
     */
    explicit TcpConnectQt(QObject* parent = nullptr);

    ~TcpConnectQt() override;

    // From IConnect (synchronous fallback)
    bool open(const QString& endpoint) override { return openAsync(endpoint); }
    void close() override;
    qint64 send(const QByteArray& data) override;
    qint64 receive(QByteArray& out, qint64 maxBytes = 4096) override;
    void flush() override;
    bool isOpen() const override;

    // Async specific
    bool openAsync(const QString& endpoint) override;
    qint64 sendAsync(const QByteArray& data) override;

   private slots:
    /**
     * @brief QTcpSocket::connected() 槽
     */
    void onConnected();

    /**
     * @brief QTcpSocket::disconnected() 槽
     */
    void onDisconnected();

    /**
     * @brief QTcpSocket::readyRead() 槽
     */
    void onReadyRead();

    /**
     * @brief QAbstractSocket::errorOccurred 槽
     */
    void onErrorOccurred(QAbstractSocket::SocketError err);

   private:
    QTcpSocket* m_socket;
};
