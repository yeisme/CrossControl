#include "tcp_connect.h"

#include <QHostAddress>
#include <QTimer>

TcpConnect::TcpConnect() = default;

TcpConnect::~TcpConnect() {
    close();
}

bool TcpConnect::open(const QString& endpoint) {
    // endpoint 格式：host:port
    const auto parts = endpoint.split(':');
    if (parts.size() != 2) return false;
    const QString host = parts[0];
    bool ok = false;
    int port = parts[1].toInt(&ok);
    if (!ok) return false;

    m_socket.abort();
    m_socket.connectToHost(host, static_cast<quint16>(port));
    // 最多等待 3 秒
    if (!m_socket.waitForConnected(3000)) { return false; }
    return true;
}

void TcpConnect::close() {
    m_socket.close();
}

qint64 TcpConnect::send(const QByteArray& data) {
    if (!isOpen()) return -1;
    qint64 written = m_socket.write(data);
    if (!m_socket.waitForBytesWritten(3000)) { return -1; }
    return written;
}

qint64 TcpConnect::receive(QByteArray& out, qint64 maxBytes) {
    if (!isOpen()) return -1;
    if (!m_socket.waitForReadyRead(100)) {
        // 没有数据
        return 0;
    }
    qint64 available = m_socket.bytesAvailable();
    qint64 toRead = qMin(available, maxBytes);
    out = m_socket.read(static_cast<qint64>(toRead));
    return out.size();
}

void TcpConnect::flush() {
    m_socket.flush();
}

bool TcpConnect::isOpen() const {
    return m_socket.state() == QAbstractSocket::ConnectedState;
}
