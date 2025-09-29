
#include "serial_connect.h"

#include <QIODevice>
#include <QStringList>
#include <QtSerialPort/QSerialPort>

SerialConnect::SerialConnect() = default;

SerialConnect::SerialConnect(const Options& options) : m_options(options) {}

SerialConnect::SerialConnect(const QString& endpoint, bool autoOpen, const Options& options)
    : m_options(options) {
    if (autoOpen) open(endpoint, options);
}

SerialConnect::~SerialConnect() {
    close();
}

bool SerialConnect::open(const QString& endpoint) {
    return open(endpoint, m_options);
}

bool SerialConnect::open(const QString& endpoint, const Options& options) {
    // 格式: device:baud
    // Example: "COM3:115200" or "/dev/ttyUSB0:9600"
    const auto parts = endpoint.split(':');
    // 支持 "device" 或 "device:baud" 两种形式
    QString device;
    int baud = options.baud;
    if (parts.size() == 1) {
        device = parts[0];
    } else if (parts.size() == 2) {
        device = parts[0];
        bool ok = false;
        int parsed = parts[1].toInt(&ok);
        if (ok) baud = parsed;
    } else {
        return false;
    }

    if (device.isEmpty()) return false;

    if (m_port.isOpen()) m_port.close();  // 关闭已有连接
    m_port.setPortName(device);           // 设置端口
    m_port.setBaudRate(static_cast<qint32>(baud));
    m_port.setDataBits(options.dataBits);
    m_port.setParity(options.parity);
    m_port.setStopBits(options.stopBits);
    m_port.setFlowControl(options.flowControl);

    bool ok = m_port.open(QIODevice::ReadWrite);
    if (!ok) return false;

    // 保存成功使用的选项
    m_options = options;
    m_options.baud = baud;
    return true;
}

void SerialConnect::close() {
    if (m_port.isOpen()) m_port.close();
}

qint64 SerialConnect::send(const QByteArray& data) {
    if (!isOpen()) return -1;
    qint64 written = m_port.write(data);
    if (!m_port.waitForBytesWritten(m_options.writeTimeoutMs)) return -1;
    return written;
}

qint64 SerialConnect::receive(QByteArray& out, qint64 maxBytes) {
    if (!isOpen()) return -1;
    if (!m_port.waitForReadyRead(m_options.readTimeoutMs)) return 0;
    qint64 available = m_port.bytesAvailable();
    qint64 toRead = qMin(available, maxBytes);
    out = m_port.read(static_cast<qint64>(toRead));
    return out.size();
}

void SerialConnect::flush() {
    if (m_port.isOpen()) m_port.clear(QSerialPort::AllDirections);
}

bool SerialConnect::isOpen() const {
    return m_port.isOpen();
}
