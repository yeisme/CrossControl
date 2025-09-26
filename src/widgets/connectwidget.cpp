#include "connectwidget.h"

#include <QHBoxLayout>
#include <QVBoxLayout>

#include "connect_wrapper.h"
#include "tcp_connect_qt.h"

ConnectWidget::ConnectWidget(QWidget* parent) : QWidget(parent) {
    m_endpoint = new QLineEdit(this);
    m_endpoint->setPlaceholderText("127.0.0.1:9000");
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

    // 使用工厂创建 TcpConnectQt 实例
    m_conn = ConnectFactory<TcpConnectQt>::create(this);
    if (m_conn) {
        connect(m_conn.get(), &IConnectQt::connected, this, &ConnectWidget::onConnected);
        connect(m_conn.get(), &IConnectQt::disconnected, this, &ConnectWidget::onDisconnected);
        connect(m_conn.get(), &IConnectQt::dataReady, this, &ConnectWidget::onDataReady);
        connect(m_conn.get(), &IConnectQt::errorOccurred, this, &ConnectWidget::onError);
    }
}

void ConnectWidget::onOpenClicked() {
    if (!m_conn) return;
    QString ep = m_endpoint->text().trimmed();
    if (m_conn->openAsync(ep)) {
        m_log->append("Connecting... " + ep);
    } else {
        m_log->append("Failed to start connect to " + ep);
    }
}

void ConnectWidget::onSendClicked() {
    if (!m_conn) return;
    QByteArray data = "hello\n";
    qint64 n = m_conn->sendAsync(data);
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
