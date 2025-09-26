#include "connectwidget.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include "modules/Config/config.h"

#include "connect_wrapper.h"
#include "tcp_connect.h"
#include "modules/Connect/connect_factory.h"

ConnectWidget::ConnectWidget(QWidget* parent) : QWidget(parent) {
    m_endpoint = new QLineEdit(this);
    // 从配置中读取默认 endpoint，若未设置则使用旧的硬编码默认
    const QString epDefault = config::ConfigManager::instance().getOrDefault("Connect/endpoint", QVariant("127.0.0.1:9000")).toString();
    m_endpoint->setPlaceholderText(epDefault);
    m_openBtn = new QPushButton("Open", this);
    m_sendBtn = new QPushButton("Send", this);
    m_log = new QTextEdit(this);
    m_log->setReadOnly(true);

    auto h = new QHBoxLayout();
    h->addWidget(m_endpoint);
    h->addWidget(m_openBtn);
    h->addWidget(m_sendBtn);

    auto v = new QVBoxLayout(this);
    v->addLayout(h);
    v->addWidget(m_log);

    connect(m_openBtn, &QPushButton::clicked, this, &ConnectWidget::onOpenClicked);
    connect(m_sendBtn, &QPushButton::clicked, this, &ConnectWidget::onSendClicked);

    // 使用工厂创建默认同步 IConnect 实例
    m_conn = connect_factory::createDefaultConnect();
    // 使用定时器轮询接收数据（同步接口）
    if (m_conn) {
        m_timer = new QTimer(this);
        connect(m_timer, &QTimer::timeout, this, [this]() {
            QByteArray buf;
            qint64 n = m_conn->receive(buf, 8192);
            if (n > 0) onDataReady(buf);
        });
        m_timer->start(100);
    }
}

void ConnectWidget::onOpenClicked() {
    if (!m_conn) return;
    QString ep = m_endpoint->text().trimmed();
    if (m_conn->open(ep)) {
        m_log->append("Connected: " + ep);
    } else {
        m_log->append("Failed to connect to " + ep);
    }
}

void ConnectWidget::onSendClicked() {
    if (!m_conn) return;
    QByteArray data = "hello\n";
    qint64 n = m_conn->send(data);
    m_log->append(QString("Sent %1 bytes").arg(n));
}

void ConnectWidget::onConnected() {
    m_log->append("Connected");
}
void ConnectWidget::onDisconnected() {
    m_log->append("Disconnected");
}
void ConnectWidget::onDataReady(const QByteArray& data) {
    m_log->append("Recv: " + QString::fromUtf8(data));
}
void ConnectWidget::onError(const QString& err) {
    m_log->append("Error: " + err);
}
