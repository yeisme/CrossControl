#include "udp_connect.h"

#include <QHostAddress>
#include <QUrl>

UdpConnect::UdpConnect() {
    m_remoteHost.clear();
    m_remotePort = 0;
    m_hasRemote = false;
    m_bound = false;
}

UdpConnect::UdpConnect(const QString& endpoint, bool autoOpen) : UdpConnect() {
    if (autoOpen) open(endpoint);
}

UdpConnect::~UdpConnect() {
    close();
}

bool UdpConnect::open(const QString& endpoint) {
    // endpoint 支持多种形式：
    //  - host:port
    //  - host:port/path
    //  - :port 或 *:port
    //  - 带 scheme 的 URL，例如 "udp://host:port/path"

    QString ep = endpoint.trimmed();
    QString host;
    int port = -1;

    if (ep.contains("://")) {
        QUrl url(ep);
        host = url.host();
        port = url.port(-1);
        if (host.isEmpty() && port <= 0) return false;
    } else {
        int slash = ep.indexOf('/');
        QString authority = (slash >= 0) ? ep.left(slash) : ep;
        const auto parts = authority.split(':');
        if (parts.size() != 2) return false;
        host = parts[0];
        bool ok = false;
        port = parts[1].toInt(&ok);
        if (!ok) return false;
    }

    m_socket.abort();
    m_remoteHost.clear();
    m_remotePort = 0;
    m_hasRemote = false;
    m_bound = false;

    if (host.isEmpty() || host == "*" || host == "0.0.0.0") {
        // 绑定本地端口用于接收
        if (!m_socket.bind(QHostAddress::Any, static_cast<quint16>(port))) { return false; }
        m_bound = true;
    } else {
        // 指定远端（不自动绑定本地端口，QT 会在发送时分配）
        m_remoteHost = host;
        m_remotePort = static_cast<quint16>(port);
        m_hasRemote = true;
    }

    return true;
}

void UdpConnect::close() {
    m_socket.close();
}

qint64 UdpConnect::send(const QByteArray& data) {
    if (!isOpen()) return -1;
    if (m_hasRemote) {
        qint64 written = m_socket.writeDatagram(data, QHostAddress(m_remoteHost), m_remotePort);
        return written >= 0 ? written : -1;
    } else {
        // 未指定远端，无法发送
        return -1;
    }
}

qint64 UdpConnect::receive(QByteArray& out, qint64 maxBytes) {
    if (!isOpen()) return -1;
    if (!m_bound) {
        // 如果未绑定本地端口，则不保证能接收
        // 尝试读取已有数据
    }
    if (!m_socket.waitForReadyRead(100)) { return 0; }
    while (m_socket.hasPendingDatagrams()) {
        qint64 sz = static_cast<qint64>(m_socket.pendingDatagramSize());
        qint64 toRead = qMin(sz, maxBytes);
        out.resize(static_cast<int>(toRead));
        QHostAddress sender;
        quint16 senderPort;
        qint64 read =
            m_socket.readDatagram(out.data(), static_cast<qint64>(toRead), &sender, &senderPort);
        return read >= 0 ? read : -1;
    }
    return 0;
}

void UdpConnect::flush() { /* UDP 无持久缓冲，忽略 */ }

bool UdpConnect::isOpen() const {
    return m_socket.state() != QAbstractSocket::UnconnectedState || m_bound || m_hasRemote;
}
