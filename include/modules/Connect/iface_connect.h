/**
 * @file iface_connect.h
 * @brief 抽象连接接口定义（跨传输类型通用）
 *
 * 本文件定义了一个最小化的抽象类 IConnect，用于对不同类型的连接
 *（例如 TCP、串口、虚拟/模拟通道）提供统一的同步接口。
 *
 * 设计原则：
 * - 保持接口精简，仅包含打开/关闭、读写、刷新与状态查询。
 * - 实现方可选择同步或基于回调/信号的异步策略（若异步请另外提供适配器）。
 */
#pragma once

#include <QByteArray>
#include <QString>
#include <QtGlobal>
#include <memory>

class IConnect;
using IConnectPtr = std::shared_ptr<IConnect>;

/**
 * @brief IConnect 抽象类
 *
 * 同步风格的连接接口。实现类应负责具体传输细节（socket、串口等）。
 * 约定：
 * - open() 负责建立连接并返回是否成功；endpoint 的格式由实现决定（例如 "host:port"）。
 * - send()/receive() 为同步操作，可能阻塞或非阻塞，具体语义由实现文档说明。
 */
class IConnect {
   public:
    virtual ~IConnect() = default;
    IConnect() = default;

    // 不可复制、不可移动
    IConnect(const IConnect&) = delete;
    IConnect& operator=(const IConnect&) = delete;
    IConnect(IConnect&&) = delete;
    IConnect& operator=(IConnect&&) = delete;

    /**
     * @brief 创建默认的 IConnect 实例（等同于 connect_factory::createDefaultConnect）
     * @return 返回一个已分配的 IConnectPtr（不会为 nullptr）
     */
    static IConnectPtr createDefault();

    /**
     * @brief 根据 endpoint 字符串解析并创建对应的 IConnect 实例
     * @param endpoint 支持类似 "tcp://host:port", "udp://host:port", "COM3:115200" 等格式
     * @return 识别并创建成功则返回实例；无法识别可返回 nullptr
     */
    static IConnectPtr createFromEndpoint(const QString& endpoint);

    /**
     * @brief 建立连接
     * @param endpoint 连接端点字符串（例如 TCP: "127.0.0.1:9000"，串口: "COM3:115200"），
     *                 具体格式由实现决定
     * @return 成功返回 true，失败返回 false
     */
    virtual bool open(const QString& endpoint) = 0;

    /**
     * @brief 关闭当前连接并释放资源
     */
    virtual void close() = 0;

    /**
     * @brief 发送数据（同步）
     * @param data 要发送的数据
     * @return 成功返回写入的字节数；遇到错误返回 -1
     */
    virtual qint64 send(const QByteArray& data) = 0;

    /**
     * @brief 接收数据（同步）
     * @param out 接收缓冲，接收到的数据写入此参数
     * @param maxBytes 最大读取字节数（默认 4096）
     * @return 实际读取的字节数；0 表示当前无数据；-1 表示发生错误
     */
    virtual qint64 receive(QByteArray& out, qint64 maxBytes = 4096) = 0;

    /**
     * @brief 刷新/清理缓冲区（例如丢弃接收缓冲）
     */
    virtual void flush() = 0;

    /**
     * @brief 查询连接是否处于打开状态
     * @return 打开返回 true，否则 false
     */
    virtual bool isOpen() const = 0;
};
