#pragma once

#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QTimer>
#include <QWidget>

#include "iface_connect.h"

/**
 * @file connectwidget.h
 * @brief 连接配置与测试的 UI 组件
 *
 * ConnectWidget 提供一个简单的 UI，用于输入连接端点、打开/关闭连接、
 * 发送数据并查看日志。组件通过 IConnect 接口与底层连接实现解耦，
 * 使用轮询方式读取数据以支持同步实现。
 */
class ConnectWidget : public QWidget {
    Q_OBJECT
   public:
    /**
     * @brief 构造函数
     * @param parent 父窗口
     */
    explicit ConnectWidget(QWidget* parent = nullptr);

   private slots:
    /**
     * @brief 打开按钮点击处理
     */
    void onOpenClicked();

    /**
     * @brief 发送按钮点击处理
     */
    void onSendClicked();

    /**
     * @brief 连接建立时回调
     */
    void onConnected();

    /**
     * @brief 连接断开时回调
     */
    void onDisconnected();

    /**
     * @brief 有数据到达时回调
     * @param data 本次接收的数据
     */
    void onDataReady(const QByteArray& data);

    /**
     * @brief 错误发生时回调
     * @param err 错误描述
     */
    void onError(const QString& err);

   private:
    QLineEdit* m_endpoint;
    QPushButton* m_openBtn;
    QPushButton* m_sendBtn;
    QTextEdit* m_log;

    IConnectPtr m_conn;
    QTimer* m_timer{nullptr};
};
