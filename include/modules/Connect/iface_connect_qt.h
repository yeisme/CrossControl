/*
 * iface_connect_qt.h
 * 基于 Qt 的异步连接接口（IConnect 的 Qt 适配）
 *
 * 本文件定义了 IConnectQt 接口，继承自 QObject 与 IConnect，
 * 旨在为基于 Qt 的应用提供信号/槽风格的异步连接抽象。
 *
 * 设计要点：
 * - 保留 IConnect 的同步方法作为兼容性后备（实现可选择在内部调用异步接口）。
 * - 通过 signals/slots 提供连接状态、数据到达与错误报告的异步通知。
 * - endpoint 字符串的具体格式由具体实现决定（例如 "host:port" 或 "COM3:115200"）。
 */

#pragma once

#include <QObject>

#include "iface_connect.h"

/**
 * @class IConnectQt
 * @brief Qt 风格的连接接口
 *
 * IConnectQt 为 Qt 应用提供一个异步连接抽象。实现类应通过信号通知外部
 *（connected/disconnected/dataReady/errorOccurred）并实现同步回退方法以
 * 保证向后兼容。
 *
 * 使用约定：
 * - 调用者可选择直接使用异步接口（openAsync/sendAsync），也可使用同步
 *   接口（open/send）——同步接口可由实现转发到异步实现并阻塞等待结果。
 */
class IConnectQt : public QObject, public IConnect {
    Q_OBJECT
   public:
    /**
     * @brief 构造函数
     * @param parent QObject 父对象
     */
    explicit IConnectQt(QObject* parent = nullptr) : QObject(parent) {}

    ~IConnectQt() override = default;

    /**
     * @brief 异步打开连接
     *
     * 发起建立连接的异步请求。实现应尽快返回并在连接成功/失败时发射相应的信号。
     * @param endpoint 连接端点描述（格式由实现约定，例如 "127.0.0.1:9000"）
     * @return 如果异步启动成功返回 true；如果启动过程立即失败（例如参数非法）返回 false。
     */
    virtual bool openAsync(const QString& endpoint) = 0;

    /**
     * @brief 异步发送数据
     *
     * 将数据写入发送队列并尽快返回。实现可选择将数据缓冲并在合适时机发送。
     * @param data 要发送的数据
     * @return 写入发送队列的字节数；若发生立即性错误可返回 -1
     */
    virtual qint64 sendAsync(const QByteArray& data) = 0;

    /**
     * @brief 异步关闭连接（默认调用同步 close）
     *
     * 实现可覆盖以在关闭时进行异步清理并发射断开信号。
     */
    virtual void closeAsync() { close(); }

   signals:
    /**
     * @brief 当连接成功建立时发射
     */
    void connected();

    /**
     * @brief 当连接断开或关闭时发射
     */
    void disconnected();

    /**
     * @brief 当有数据可读时发射
     * @param data 本次到达的数据块（注意：实现可选择复用缓冲区或以分片方式发射）
     */
    void dataReady(const QByteArray& data);

    /**
     * @brief 当发生错误时发射
     * @param error 描述错误的字符串
     */
    void errorOccurred(const QString& error);
};
