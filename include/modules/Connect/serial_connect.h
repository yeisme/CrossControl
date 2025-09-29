#pragma once

#include <QIODevice>
#include <QString>
#include <QtSerialPort/QSerialPort>

#include "iface_connect.h"

/**
 * @file serial_connect.h
 * @brief 基于 QSerialPort 的串口连接实现声明
 *
 * endpoint 格式示例："COM3:115200" 或 "/dev/ttyUSB0:9600"
 */
class SerialConnect : public IConnect {
   public:
    /**
     * @brief 串口配置选项，提供默认值以便仅传入 endpoint 时仍能工作
     */
    struct Options {
        int baud;
        QSerialPort::DataBits dataBits;
        QSerialPort::Parity parity;
        QSerialPort::StopBits stopBits;
        QSerialPort::FlowControl flowControl;
        int readTimeoutMs;
        int writeTimeoutMs;

        Options()
            : baud(115200),
              dataBits(QSerialPort::Data8),
              parity(QSerialPort::NoParity),
              stopBits(QSerialPort::OneStop),
              flowControl(QSerialPort::NoFlowControl),
              readTimeoutMs(100),
              writeTimeoutMs(3000) {}
    };

    SerialConnect();
    /**
     * @brief 构造并可选择根据 endpoint 自动打开串口
     * @param endpoint 形如 "COM3:115200" 或 "/dev/ttyUSB0:9600"
     * @param autoOpen 若为 true 则在构造时尝试 open(endpoint)
     * @param options 可选配置，默认值允许只传入 endpoint
     */
    explicit SerialConnect(const QString& endpoint,
                           bool autoOpen = true,
                           const Options& options = Options());
    explicit SerialConnect(const Options& options);
    ~SerialConnect() override;

    bool open(const QString& endpoint) override;
    // 非虚函数重载：允许在调用 open 时传入额外选项
    bool open(const QString& endpoint, const Options& options);
    void close() override;
    qint64 send(const QByteArray& data) override;
    qint64 receive(QByteArray& out, qint64 maxBytes = 4096) override;
    void flush() override;
    bool isOpen() const override;

   private:
    QSerialPort m_port;
    Options m_options;
};
