#include "tcp_connect_qt.h"

#include <QHostAddress>

/**
 * @brief 通过继承自 IConnectQt 实现基于 QTcpSocket 的异步连接
 * 
 * @param parent 
 */
TcpConnectQt::TcpConnectQt(QObject* parent) : IConnectQt(parent), m_socket(new QTcpSocket(this)) {
    connect(m_socket, &QTcpSocket::connected, this, &TcpConnectQt::onConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &TcpConnectQt::onDisconnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &TcpConnectQt::onReadyRead);
    connect(m_socket,
            QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::errorOccurred),
            this,
            &TcpConnectQt::onErrorOccurred);
}

TcpConnectQt::~TcpConnectQt() {
    close();
}

bool TcpConnectQt::openAsync(const QString& endpoint) {
    const auto parts = endpoint.split(':');
    if (parts.size() != 2) return false;
    const QString host = parts[0];
    bool ok = false;
    int port = parts[1].toInt(&ok);
    if (!ok) return false;
    m_socket->abort();
    m_socket->connectToHost(host, static_cast<quint16>(port));
    return true;  // 发起连接成功，最终通过 connected() 信号通知
}

void TcpConnectQt::close() {
    if (m_socket) m_socket->disconnectFromHost();
}

qint64 TcpConnectQt::send(const QByteArray& data) {
    return sendAsync(data);
}

qint64 TcpConnectQt::sendAsync(const QByteArray& data) {
    if (!m_socket) return -1;
    qint64 n = m_socket->write(data);
    // 不阻塞，数据写入后会触发 bytesWritten 信号（如需要可以监听）
    return n;
}

qint64 TcpConnectQt::receive(QByteArray& out, qint64 maxBytes) {
    if (!m_socket) return -1;
    qint64 available = m_socket->bytesAvailable();
    qint64 toRead = qMin(available, maxBytes);
    out = m_socket->read(static_cast<qint64>(toRead));
    return out.size();
}

void TcpConnectQt::flush() {
    if (m_socket) m_socket->flush();
}

bool TcpConnectQt::isOpen() const {
    return m_socket && m_socket->state() == QAbstractSocket::ConnectedState;
}

void TcpConnectQt::onConnected() {
    emit connected();
}
void TcpConnectQt::onDisconnected() {
    emit disconnected();
}
void TcpConnectQt::onReadyRead() {
    if (!m_socket) return;
    QByteArray data = m_socket->readAll();
    if (!data.isEmpty()) emit dataReady(data);
}
void TcpConnectQt::onErrorOccurred(QAbstractSocket::SocketError err) {
    Q_UNUSED(err)
    if (!m_socket) return;
    emit errorOccurred(m_socket->errorString());
}
