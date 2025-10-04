#ifndef MONITORWIDGET_H
#define MONITORWIDGET_H

#include <QByteArray>
#include <QPixmap>
#include <QWidget>
#include <cstdint>

class QTcpServer;
class QTcpSocket;
#include <QMap>

class TcpConnect;
class QThread;
class QNetworkAccessManager;
class QNetworkReply;
class QLineEdit;
class QTextEdit;
class QComboBox;
class QPushButton;

QT_BEGIN_NAMESPACE
namespace Ui {
class MonitorWidget;
}
QT_END_NAMESPACE

/**
 * @brief 监控页面小部件
 *
 * 该类负责：
 * - 作为客户端模式时，通过 TcpConnect 在后台线程中拉取来自设备的视频数据；
 * - 作为服务端模式时，使用 QTcpServer 接受多个客户端的连接并分别处理二进制帧；
 * - 解析自定义的帧协议（包头 0xAA55 + 4 字节长度 + JPEG 数据 + 2 字节 CRC16-CCITT）；
 * - 将接收的 JPEG 在界面上显示并保存到磁盘；
 * - 提供保存设备、选择保存路径、开始/停止接收、监听端口和广播文本命令等操作。
 *
 * 设计要点：
 * - 保持点对点（TcpConnect）和服务端（QTcpServer）两种接收方式并存；
 * - 每个服务器端客户端维护独立的接收缓冲区以便正确拆包；
 * - CRC 校验失败会丢弃当前包并继续解析后续数据。
 */

namespace DeviceGateway { class DeviceGateway; }

class MonitorWidget : public QWidget {
    Q_OBJECT

   public:
    /**
     * @brief 构造函数
     * @param gateway 指向 DeviceGateway 实例（用于读取已注册设备）
     * @param parent 父 QWidget（可为 nullptr）
     */
    MonitorWidget(DeviceGateway::DeviceGateway* gateway, QWidget* parent = nullptr);

    /**
     * @brief 析构函数，确保后台线程和连接被正确关闭
     */
    ~MonitorWidget();

   signals:
    /**
     * @brief 请求返回主界面的信号
     *
     * 当用户点击返回按钮时发出，外部可以连接该信号以切换视图。
     */
    void backToMain();

   private slots:
    /**
     * @brief 返回主界面按钮的槽函数（由 UI 自动连接）
     */
    void on_btnBackFromMonitor_clicked();

    /**
     * @brief 点击连接（Start）按钮的槽函数，启动 TcpConnect 后台工作线程
     */
    void on_btnConnect_clicked();

    /**
     * @brief 停止接收/断开连接的槽函数
     */
    void on_btnStop_clicked();

    /**
     * @brief 选择保存路径的槽函数
     */
    void on_btnChoosePath_clicked();

    /**
     * @brief 保存当前主机/端口到设备列表的槽函数
     */
    void on_btnSaveDevice_clicked();

    /**
     * @brief 设备下拉选择改变回调，将 host:port 填入编辑框
     * @param index 当前选中索引
     */
    void on_comboDevices_currentIndexChanged(int index);

    // HTTP operations
    void on_btnHttpSend_clicked();

    /**
     * @brief 当点对点（TcpConnect）工作线程接收到数据时发出的槽
     * @param data 接收到的原始字节流（会追加到内部缓冲解析）
     */
    void onDataReady(const QByteArray& data);

   private:
    /**
     * @brief UI 指针，指向由 uic 生成的 UI 类
     */
    Ui::MonitorWidget* ui;

    // Pointer to DeviceGateway for reading registered devices
    DeviceGateway::DeviceGateway* gateway_{nullptr};

    // Simple label to show selected device details
    class QLabel* lblDeviceInfo_ = nullptr;

    // ---------------- 点对点（客户端）相关 ----------------
    /**
     * @brief 用于同步连接模式的连接对象（在后台线程使用）
     */
    TcpConnect* m_syncConn = nullptr;

    /**
     * @brief 后台工作线程，用于运行 m_syncConn 的接收循环
     */
    QThread* m_workerThread = nullptr;

    /**
     * @brief 点对点接收缓冲区（当使用 TcpConnect 时使用）
     */
    QByteArray m_recvBuffer;

    /**
     * @brief 最近一帧解码得到的 QPixmap，用于 UI 显示或后续处理
     */
    QPixmap m_lastPixmap;

    // ---------------- 服务端（多客户端）相关 ----------------
    /**
     * @brief QTcpServer 指针，用于绑定监听并接受多个客户端连接
     */
    QTcpServer* m_server = nullptr;

    /**
     * @brief 为每个客户端 socket 保持接收缓冲，以便进行粘包拆包
     * key: QTcpSocket*，value: 累积的 QByteArray 数据
     */
    QMap<QTcpSocket*, QByteArray> m_clientBuffers;

    /**
     * @brief 指向 UI 中的监听端口输入（使用 findChild 查找以兼容 uic 生成差异）
     */
    class QLineEdit* m_listenEdit = nullptr;

    // ---------------- 列表与控制方法 ----------------
    /**
     * @brief 启动监听（读取 UI 中端口并调用 m_server->listen）
     * @note 成功后会更新 UI 按钮样式表示正在监听，失败时设置失败样式
     */
    void startListening();

    /**
     * @brief 停止监听并关闭所有当前客户端连接
     */
    void stopListening();

    /**
     * @brief 处理来自某个已连接客户端的新数据（追加到该客户端独立缓冲并解析帧）
     * @param sock 触发 readyRead 的客户端 socket
     * @param data 这次 readyRead 读到的字节
     */
    void handleIncomingData(QTcpSocket* sock, const QByteArray& data);

    // 设备信息编辑框（在 UI 中存在）：editHost, editPort

    // HTTP and Serial operations are handled via child widgets
    void on_btnSaveHttpAction_clicked();
    void on_btnLoadHttpAction_clicked();

    // UI state helper
    void setTcpConnected(bool connected);


    /**
     * @brief CRC16-CCITT 计算函数，用于校验收到的 JPEG 数据
     * @param buf 数据缓冲区指针
     * @param len 缓冲区长度
     * @return 16 位 CRC 校验值
     */
    uint16_t crc16_ccitt(const uint8_t* buf, uint32_t len);

    /**
     * @brief 发送 UI 中编辑框文本到所有已连接客户端（由 btnSendText 触发）
     */
    // legacy broadcast button handler removed

    /**
     * @brief 将字节数据广播到当前所有已连接客户端
     * @param data 要发送的字节数据（通常为 UTF-8 文本或协议命令）
     */
    // Returns number of clients the data was written to
    int broadcastToClients(const QByteArray& data);
        // storage helpers
        void ensureActionsTable();
        int saveActionToDb(const QString& name, const QString& type, const QByteArray& payload);
        QVector<QPair<int, QString>> loadActionsFromDb(const QString& type);
    // HTTP action helpers
    QByteArray serializeHttpAction(const QString& url, const QString& method, const QString& token, bool autoBearer, const QByteArray& body);
    bool deserializeHttpAction(const QByteArray& payload, QString& url, QString& method, QString& token, bool& autoBearer, QByteArray& body);
    // device edit fields live in UI: editHost, editPort

    // Note: protocol-specific operations (HTTP/TCP/UDP/Serial) are handled by
    // dedicated child widgets (HttpOpsWidget/TcpOpsWidget/UdpOpsWidget/SerialOpsWidget).
    // Monitor no longer owns network workers or sockets directly to avoid duplicated logic.
    TcpConnect* m_tcpClient = nullptr;
};

#endif  // MONITORWIDGET_H