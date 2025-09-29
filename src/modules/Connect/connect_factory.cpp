#include "modules/Connect/connect_factory.h"

#include <QString>
#include <QUrl>
#include <QUrlQuery>

#include "modules/Connect/serial_connect.h"
#include "modules/Connect/tcp_connect.h"
#include "modules/Connect/udp_connect.h"

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

    if (lower.startsWith("serial://") || lower.startsWith("serial:") || lower.contains("com") ||
        lower.contains("/dev/")) {
        QString s = stripPrefixes({"serial://", "serial:"}, ep);
        return makeSerialConnect(s);
    }

    // default to TCP
    return std::make_unique<TcpConnect>(ep, true);
}

}  // namespace connect_factory
