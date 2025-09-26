#pragma once

#include <QString>
#include <QTcpSocket>

#include "iface_connect.h"

/**
 * @file tcp_connect.h
 * @brief 基于 QTcpSocket 的同步连接实现声明
 *
 * TcpConnect 提供 IConnect 的同步实现，封装 QTcpSocket 以实现
 * 基本的 open/close/send/receive/flush/isOpen 接口。
 */
class TcpConnect : public IConnect {
   public:
    /**
     * @brief 构造函数，未打开套接字
     */
    TcpConnect();

    /**
     * @brief 析构函数，会自动关闭套接字并释放资源
     */
    ~TcpConnect() override;

    /**
     * @brief 根据 endpoint 打开连接
     * @param endpoint 格式示例："127.0.0.1:9000"
     * @return 成功返回 true，失败返回 false
     */
    bool open(const QString& endpoint) override;

    /**
     * @brief 关闭连接并释放资源
     */
    void close() override;

    /**
     * @brief 同步发送数据
     * @param data 要发送的数据
     * @return 写入的字节数，错误返回 -1
     */
    qint64 send(const QByteArray& data) override;

    /**
     * @brief 同步接收数据
     * @param out 接收缓冲，接收到的数据写入该参数
     * @param maxBytes 最大读取字节数
     * @return 实际读取的字节数；0 表示当前无数据；-1 表示发生错误
     */
    qint64 receive(QByteArray& out, qint64 maxBytes = 4096) override;

    /**
     * @brief 刷新/清理缓冲区（当前实现可用于丢弃接收缓冲）
     */
    void flush() override;

    /**
     * @brief 查询是否已打开
     * @return 已打开返回 true，否则 false
     */
    bool isOpen() const override;

   private:
    QTcpSocket m_socket;
};
