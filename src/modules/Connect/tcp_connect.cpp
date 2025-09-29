#include "tcp_connect.h"

#include <QHostAddress>
#include <QTimer>
#include <QUrl>

TcpConnect::TcpConnect() = default;

TcpConnect::TcpConnect(const QString& endpoint, bool autoOpen) : TcpConnect() {
    if (autoOpen) open(endpoint);
}

TcpConnect::~TcpConnect() {
    close();
}

bool TcpConnect::open(const QString& endpoint) {
    // 支持的 endpoint 示例：
    //  - "host:port"
    //  - "host:port/path" (会忽略 path 部分用于建立 socket)
    //  - 带 scheme 的 URL，例如 "http://host:port/path" 或 "tcp://host:port"

    QString ep = endpoint.trimmed();
    QString host;
    int port = -1;

    if (ep.contains("://")) {
        // 使用 QUrl 解析完整 URL
        QUrl url(ep);
        host = url.host();
        port = url.port(-1);
        if (host.isEmpty() || port <= 0) return false;
    } else {
        // 可能含有 path（例如 "host:port/path"），先去掉 path
        int slash = ep.indexOf('/');
        QString authority = (slash >= 0) ? ep.left(slash) : ep;
        const auto parts = authority.split(':');
        if (parts.size() != 2) return false;
        host = parts[0];
        bool ok = false;
        port = parts[1].toInt(&ok);
        if (!ok || port <= 0) return false;
    }

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
