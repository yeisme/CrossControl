#include "modules/Connect/connect_factory.h"

#include <QString>
#include <QUrl>
#include <QUrlQuery>
#include <memory>

#include "modules/Connect/serial_connect.h"
#include "modules/Connect/tcp_connect.h"
#include "modules/Connect/udp_connect.h"
#ifdef BUILD_MQTT_CLIENT
#include "modules/Connect/mqtt_connect.h"
#endif
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QRegularExpression>
#include <memory>

namespace connect_factory {

IConnectPtr createDefaultConnect() {
    return std::make_unique<TcpConnect>();
}

IConnectPtr createByEndpoint(const QString& endpoint) {
    // 解析 scheme，如 tcp://, udp://, serial://
    QString ep = endpoint.trimmed();
    QString lower = ep.toLower();

    // (removed unused Scheme enum and sub variable)

    // 去除任何给定的前缀（不区分大小写），返回剩余部分
    auto stripPrefixes = [&](const QStringList& prefixes, const QString& src) -> QString {
        for (const QString& p : prefixes) {
            if (lower.startsWith(p)) { return src.mid(p.size()); }
        }
        return src;
    };

    // 用于串行连接，以保持解析逻辑的独立
    auto makeSerialConnect = [&](const QString& subIn) -> IConnectPtr {
        QString subLocal = subIn;
        SerialConnect::Options opts;
        int qpos = subLocal.indexOf('?');
        QString path = subLocal;
        if (qpos >= 0) {
            path = subLocal.left(qpos);
            QString query = subLocal.mid(qpos + 1);
            QUrlQuery q(query);
            const auto items = q.queryItems();
            for (const auto& it : items) {
                QString k = it.first.trimmed().toLower();
                QString v = it.second.trimmed();
                if (k == "baud") {
                    bool ok = false;
                    int b = v.toInt(&ok);
                    if (ok) opts.baud = b;
                } else if (k == "parity") {
                    QString lv = v.toLower();
                    if (lv == "even")
                        opts.parity = QSerialPort::EvenParity;
                    else if (lv == "odd")
                        opts.parity = QSerialPort::OddParity;
                    else
                        opts.parity = QSerialPort::NoParity;
                } else if (k == "databits") {
                    int db = v.toInt();
                    if (db == 5)
                        opts.dataBits = QSerialPort::Data5;
                    else if (db == 6)
                        opts.dataBits = QSerialPort::Data6;
                    else if (db == 7)
                        opts.dataBits = QSerialPort::Data7;
                    else
                        opts.dataBits = QSerialPort::Data8;
                } else if (k == "stopbits") {
                    int sb = v.toInt();
                    if (sb == 2)
                        opts.stopBits = QSerialPort::TwoStop;
                    else
                        opts.stopBits = QSerialPort::OneStop;
                } else if (k == "flow") {
                    QString lv = v.toLower();
                    if (lv == "rtscts")
                        opts.flowControl = QSerialPort::HardwareControl;
                    else if (lv == "xonxoff")
                        opts.flowControl = QSerialPort::SoftwareControl;
                    else
                        opts.flowControl = QSerialPort::NoFlowControl;
                } else if (k == "readtimeoutms") {
                    opts.readTimeoutMs = v.toInt();
                } else if (k == "writetimeoutms") {
                    opts.writeTimeoutMs = v.toInt();
                }
            }
        }
        return std::make_unique<SerialConnect>(path, true, opts);
    };

    // determine scheme and delegate to handlers
    if (lower.startsWith("tcp://") || lower.startsWith("tcp:")) {
        QString s = stripPrefixes({"tcp://", "tcp:"}, ep);
        return std::make_unique<TcpConnect>(s, true);
    }

    if (lower.startsWith("udp://") || lower.startsWith("udp:")) {
        QString s = stripPrefixes({"udp://", "udp:"}, ep);
        return std::make_unique<UdpConnect>(s, true);
    }

#ifdef BUILD_MQTT_CLIENT
    if (lower.startsWith("mqtt://") || lower.startsWith("mqtt:")) {
        QString s = stripPrefixes({"mqtt://", "mqtt:"}, ep);
        return std::make_unique<MqttConnect>(s, true);
    }
#endif

    // HTTP/HTTPS: return TcpConnect for raw socket usage. For higher-level HTTP
    // semantics, prefer using QNetworkAccessManager obtained via
    // connect_factory::createNetworkAccessManager()/createNetworkAccessManagerFromEndpoint().
    if (lower.startsWith("http://") || lower.startsWith("https://")) {
        // Fall through to TcpConnect which can parse URLs (host:port)
        return std::make_unique<TcpConnect>(ep, true);
    }

    if (lower.startsWith("serial://") || lower.startsWith("serial:") || lower.contains("com") ||
        lower.contains("/dev/")) {
        QString s = stripPrefixes({"serial://", "serial:"}, ep);
        return makeSerialConnect(s);
    }

    // default to TCP
    return std::make_unique<TcpConnect>(ep, true);
}

std::shared_ptr<QNetworkAccessManager> createNetworkAccessManager() {
    return std::make_shared<QNetworkAccessManager>();
}

std::shared_ptr<QNetworkAccessManager> createNetworkAccessManagerFromEndpoint(
    const QString& endpoint) {
    QString ep = endpoint.trimmed();
    auto manager = createNetworkAccessManager();

    if (ep.isEmpty()) return manager;

    // 尝试将 endpoint 解析为 URL（支持 scheme://host:port/path）
    QUrl url(ep);
    QString scheme = url.scheme().toLower();
    QString host;
    quint16 port = 0;

    if (!url.host().isEmpty()) {
        host = url.host();
        port = static_cast<quint16>(url.port());
    } else {
        // 可能是 "host:port" 形式
        QRegularExpression re(R"(^\s*([^:\s]+)[:](\d+)\s*$)");
        QRegularExpressionMatch m = re.match(ep);
        if (m.hasMatch()) {
            host = m.captured(1);
            port = static_cast<quint16>(m.captured(2).toUInt());
        }
    }

    if (host.isEmpty()) return manager;

    // 根据 scheme 选择代理类型
    QNetworkProxy::ProxyType ptype = QNetworkProxy::HttpProxy;
    if (scheme.startsWith("socks"))
        ptype = QNetworkProxy::Socks5Proxy;
    else if (scheme.startsWith("http-proxy") || scheme.startsWith("proxy"))
        ptype = QNetworkProxy::HttpProxy;
    else if (scheme.isEmpty())
        ptype = QNetworkProxy::HttpProxy;  // 默认把 host:port 解释为 HTTP 代理

    QNetworkProxy proxy(ptype, host, port);
    manager->setProxy(proxy);
    return manager;
}

}  // namespace connect_factory
