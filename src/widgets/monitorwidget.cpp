#include "monitorwidget.h"

#include <qcoreapplication.h>

#include <QBoxLayout>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFrame>
#include <QHostAddress>
#include <QImage>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkDatagram>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPair>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTextEdit>
#include <QThread>
#include <QTimer>
#include <QUdpSocket>
#include <QUrl>
#include <QVector>
#include <QtSql/QSqlError>
#include <QtSql/QSqlQuery>

#include "modules/Config/config.h"
#include "modules/Connect/tcp_connect.h"
#include "modules/DeviceGateway/device_gateway.h"
#include "modules/Storage/storage.h"
#include "spdlog/spdlog.h"
#include "ui_monitorwidget.h"
// child operation widgets
#include "httpopswidget.h"
#include "serialopswidget.h"
// new operation widgets
#include "tcpopswidget.h"
#include "udpopswidget.h"

MonitorWidget::MonitorWidget(DeviceGateway::DeviceGateway* gateway, QWidget* parent)
    : QWidget(parent), ui(new Ui::MonitorWidget) {
    ui->setupUi(this);
    // MonitorWidget no longer creates a global QNetworkAccessManager; HttpOpsWidget
    // manages its own network manager. Keep include for now for compatibility.
    // populate device list from registry if gateway provided
    if (gateway) {
        gateway_ = gateway;
        auto devs = gateway_->registry().listDevices();
        for (const auto& d : devs) {
            const QString text = d.endpoint.isEmpty() ? d.id : d.endpoint;
            ui->comboDevices->addItem(text, d.id);
        }
    }
    spdlog::debug("MonitorWidget constructed");
    // no-op here; worker created when user clicks Start
    // Note: ui->setupUi calls QMetaObject::connectSlotsByName(this) which
    // auto-connects slots named like on_<object>_<signal>(). Avoid manual
    // connects to prevent duplicate invocations.

    {
        auto btnStop = this->findChild<QPushButton*>("btnStop");
        if (btnStop) btnStop->setEnabled(false);
    }

    // setup server
    m_server = new QTcpServer(this);
    // find optional listen port edit (uic may or may not have created a named member)
    m_listenEdit = this->findChild<QLineEdit*>("editListenPort");
    // find legacy send button (textbox removed)
    auto sendBtn = this->findChild<QPushButton*>("btnSendText");

    // Replace legacy send/textbox with an operations tab widget (runtime-created)
    // so we don't need to change the .ui immediately. If the UI already contains
    // an operations/tab widget, prefer to reuse it. If a design-time tab named
    // "tabHttp" exists but is a plain QWidget (not a HttpOpsWidget), replace
    // that page with a proper HttpOpsWidget so forwarding logic can find it.
    bool createTab = true;
    QTabWidget* tabOpsExisting = this->findChild<QTabWidget*>("tabOperations");
    if (tabOpsExisting) createTab = false;
    if (createTab) {
        // legacy textbox removed from UI; keep send button hidden if present
        if (sendBtn) sendBtn->setVisible(false);

        QTabWidget* tabOps = new QTabWidget(this);
        tabOps->setObjectName("tabOperations");

        // ---- HTTP tab ----
        HttpOpsWidget* httpOps = new HttpOpsWidget(tabOps);
        httpOps->setObjectName("tabHttp");
        tabOps->addTab(httpOps, QCoreApplication::translate("MonitorWidget", "HTTP"));

        // ---- MQTT tab (placeholder) ----
        QWidget* mqttTab = new QWidget(tabOps);
        mqttTab->setObjectName("tabMqtt");
        QVBoxLayout* mqttLayout = new QVBoxLayout(mqttTab);
        mqttLayout->addWidget(new QLabel(QStringLiteral("MQTT client (TBD)"), mqttTab));
        tabOps->addTab(mqttTab, QCoreApplication::translate("MonitorWidget", "MQTT"));


    // ---- TCP tab ----
    if (!this->findChild<TcpOpsWidget*>()) {
        TcpOpsWidget* tcpOps = new TcpOpsWidget(tabOps);
        tcpOps->setObjectName("tabTcp");
        tabOps->addTab(tcpOps, QCoreApplication::translate("MonitorWidget", "TCP"));
    }

    // ---- UDP tab ----
    if (!this->findChild<UdpOpsWidget*>()) {
        UdpOpsWidget* udpOps = new UdpOpsWidget(tabOps);
        udpOps->setObjectName("tabUdp");
        tabOps->addTab(udpOps, QCoreApplication::translate("MonitorWidget", "UDP"));
    }

    // ---- Serial tab ----
    SerialOpsWidget* serialOps = new SerialOpsWidget(tabOps);
    serialOps->setObjectName("tabSerial");
    tabOps->addTab(serialOps, QCoreApplication::translate("MonitorWidget", "Serial"));

        // Insert tabOps into the UI near the legacy send controls when possible
        QWidget* parentWidget = nullptr;
        if (sendBtn) parentWidget = sendBtn->parentWidget();
        if (parentWidget) {
            if (auto lay = parentWidget->layout()) {
                int pos = -1;
                if (sendBtn) pos = lay->indexOf(sendBtn);
                if (pos >= 0) {
                    if (auto bx = qobject_cast<QBoxLayout*>(lay))
                        bx->insertWidget(pos, tabOps);
                    else
                        lay->addWidget(tabOps);
                } else
                    lay->addWidget(tabOps);
            } else {
                tabOps->setParent(parentWidget);
            }
        } else {
            // fallback place it under this widget
            if (auto topLay = this->layout())
                topLay->addWidget(tabOps);
            else
                tabOps->setParent(this);
        }

        // child widgets manage their own send connections
    }

    // If UI already contains a tabOperations widget, check whether its HTTP page
    // is a plain QWidget named "tabHttp" (from .ui). If so, replace that page
    // with a HttpOpsWidget instance so MonitorWidget can find it by type/name.
    if (tabOpsExisting) {
        // look for an existing page named tabHttp
        for (int i = 0; i < tabOpsExisting->count(); ++i) {
            QWidget* page = tabOpsExisting->widget(i);
            if (!page) continue;
                if (page->objectName() == QLatin1String("tabHttp")) {
                // If it's already the correct type, nothing to do
                if (qobject_cast<HttpOpsWidget*>(page)) break;
                // create new HttpOpsWidget and replace the page
                HttpOpsWidget* httpOps = new HttpOpsWidget(tabOpsExisting);
                httpOps->setObjectName("tabHttp");
                // preserve tab text
                QString title = tabOpsExisting->tabText(i);
                tabOpsExisting->removeTab(i);
                tabOpsExisting->insertTab(i, httpOps, title);
                break;
            }
        }
    }

    // Ensure TCP and UDP pages exist or replace placeholders if present
    if (tabOpsExisting) {
        bool foundTcp = false;
        bool foundUdp = false;
        // If a child widget of these types already exists anywhere in the
        // MonitorWidget hierarchy, treat them as present to avoid duplicating
        // the same functional widget (prevents duplicate input fields).
        if (this->findChild<TcpOpsWidget*>()) foundTcp = true;
        if (this->findChild<UdpOpsWidget*>()) foundUdp = true;
        for (int i = 0; i < tabOpsExisting->count(); ++i) {
            QWidget* page = tabOpsExisting->widget(i);
            if (!page) continue;
            if (page->objectName() == QLatin1String("tabTcp")) {
                foundTcp = true;
                if (!qobject_cast<TcpOpsWidget*>(page)) {
                    TcpOpsWidget* tcpOps = new TcpOpsWidget(tabOpsExisting);
                    tcpOps->setObjectName("tabTcp");
                    QString title = tabOpsExisting->tabText(i);
                    tabOpsExisting->removeTab(i);
                    tabOpsExisting->insertTab(i, tcpOps, title);
                    // Delete the old page widget to avoid orphaned duplicate UI
                    page->deleteLater();
                }
            } else if (page->objectName() == QLatin1String("tabUdp")) {
                foundUdp = true;
                if (!qobject_cast<UdpOpsWidget*>(page)) {
                    UdpOpsWidget* udpOps = new UdpOpsWidget(tabOpsExisting);
                    udpOps->setObjectName("tabUdp");
                    QString title = tabOpsExisting->tabText(i);
                    tabOpsExisting->removeTab(i);
                    tabOpsExisting->insertTab(i, udpOps, title);
                    // Delete the old page widget to avoid orphaned duplicate UI
                    page->deleteLater();
                }
            }
        }
        // append if missing (place before Serial if present)
        int insertIndex = tabOpsExisting->count();
        for (int i = 0; i < tabOpsExisting->count(); ++i) {
            if (tabOpsExisting->widget(i) && tabOpsExisting->widget(i)->objectName() == QLatin1String("tabSerial")) {
                insertIndex = i; // insert before serial
                break;
            }
        }
        if (!foundTcp) {
            TcpOpsWidget* tcpOps = new TcpOpsWidget(tabOpsExisting);
            tcpOps->setObjectName("tabTcp");
            tabOpsExisting->insertTab(insertIndex++, tcpOps, QCoreApplication::translate("MonitorWidget", "TCP"));
        }
        if (!foundUdp) {
            UdpOpsWidget* udpOps = new UdpOpsWidget(tabOpsExisting);
            udpOps->setObjectName("tabUdp");
            tabOpsExisting->insertTab(insertIndex++, udpOps, QCoreApplication::translate("MonitorWidget", "UDP"));
        }
        // Debug: list tabs and their widget types
        for (int j = 0; j < tabOpsExisting->count(); ++j) {
            QWidget* w = tabOpsExisting->widget(j);
            QString name = w ? w->objectName() : QStringLiteral("(null)");
            spdlog::info("Monitor tab[{}] = {} ({})", j, name.toStdString(), w ? w->metaObject()->className() : "(no widget)");
        }
    }

    // If UI already contains a promoted SerialOpsWidget (design-time promotion),
    // bind it to the DeviceGateway so we can preselect serial ports based on
    // device metadata or endpoint and update selection when device changes.
    if (gateway_) {
        SerialOpsWidget* serialWidget =
            this->findChild<SerialOpsWidget*>(QStringLiteral("tabSerial"));
        if (serialWidget) {
            auto trySelectFromDevice = [this, serialWidget](const DeviceGateway::DeviceInfo& d) {
                QString selPort;
                if (d.metadata.contains("serial_port"))
                    selPort = d.metadata.value("serial_port").toString();
                else if (!d.endpoint.isEmpty()) {
                    QString ep = d.endpoint.trimmed();
                    if (ep.startsWith("COM", Qt::CaseInsensitive) || ep.contains("/dev/"))
                        selPort = ep;
                }
                if (!selPort.isEmpty()) {
                    if (auto combo =
                            serialWidget->findChild<QComboBox*>(QStringLiteral("comboPort"))) {
                        int idx = combo->findText(selPort);
                        if (idx >= 0) combo->setCurrentIndex(idx);
                    }
                }
            };

            // Preselect from first device if present
            auto devs = gateway_->registry().listDevices();
            if (!devs.isEmpty()) trySelectFromDevice(devs.first());

            // Update when user changes selected device in UI
            if (ui->comboDevices) {
                connect(ui->comboDevices,
                        QOverload<int>::of(&QComboBox::currentIndexChanged),
                        this,
                        [this, serialWidget, trySelectFromDevice](int) {
                            QString id;
                            if (ui->comboDevices->currentData().isValid())
                                id = ui->comboDevices->currentData().toString();
                            else
                                id = ui->comboDevices->currentText();
                            if (id.isEmpty()) return;
                            auto opt = gateway_->registry().getDevice(id);
                            if (opt.has_value()) trySelectFromDevice(opt.value());
                        });
            }
        }
    }
    connect(m_server, &QTcpServer::newConnection, this, [this]() {
        while (m_server->hasPendingConnections()) {
            QTcpSocket* s = m_server->nextPendingConnection();
            spdlog::info("new server-side client connected: {}:{}",
                         s->peerAddress().toString().toStdString(),
                         s->peerPort());
            m_clientBuffers.insert(s, QByteArray());
            connect(s, &QTcpSocket::readyRead, this, [this, s]() {
                QByteArray d = s->readAll();
                this->handleIncomingData(s, d);
            });
            connect(s, &QTcpSocket::disconnected, this, [this, s]() {
                spdlog::info("client disconnected {}:{}",
                             s->peerAddress().toString().toStdString(),
                             s->peerPort());
                m_clientBuffers.remove(s);
                s->deleteLater();
            });
            // send default start command to newly connected client
            const QByteArray startCmd = "START_STREAM\n";
            s->write(startCmd);
            s->flush();
        }
    });

    // The MonitorWidget no longer manages connections/listening/saving directly when integrated
    // with DeviceRegistry. Hide controls that belong to device provisioning and keep only display.
    if (auto w = this->findChild<QWidget*>("btnListen")) w->setVisible(false);
    if (auto w = this->findChild<QWidget*>("btnStop")) w->setVisible(false);
    if (auto w = this->findChild<QWidget*>("btnConnect")) w->setVisible(false);
    if (auto w = this->findChild<QWidget*>("btnSaveDevice")) w->setVisible(false);
    if (auto w = this->findChild<QWidget*>("editHost")) w->setVisible(false);
    if (auto w = this->findChild<QWidget*>("editPort")) w->setVisible(false);
    if (auto w = this->findChild<QWidget*>("btnSendText")) w->setVisible(false);

    // legacy: previously the device list could be loaded from config. When a DeviceGateway is
    // present we prefer its registry. We keep this fallback for compatibility.
    if (ui->comboDevices->count() == 0) {
        auto& cfg = config::ConfigManager::instance();
        QVariant devs = cfg.getOrDefault("Connect/devices", QVariant());
        if (devs.isValid()) {
            // devices may be stored as JSON string or QStringList
            if (devs.typeId() == QMetaType::QString) {
                // try parse as json array
                QJsonDocument doc = QJsonDocument::fromJson(devs.toString().toUtf8());
                if (doc.isArray()) {
                    for (auto v : doc.array()) ui->comboDevices->addItem(v.toString());
                } else if (!devs.toString().isEmpty()) {
                    ui->comboDevices->addItem(devs.toString());
                }
            } else if (devs.canConvert<QVariantList>()) {
                for (auto v : devs.toList()) ui->comboDevices->addItem(v.toString());
            }
        }
    }

    // default save path
    auto& cfg2 = config::ConfigManager::instance();
    QString def = cfg2.getOrDefault("Monitor/savePath", QVariant("./video_data")).toString();
    ui->editSavePath->setText(def);
    QDir d(def);
    if (!d.exists()) d.mkpath(".");

    // Style the device card to look like a subtle card (if present)
    if (auto deviceCard = this->findChild<QFrame*>("deviceCard")) {
        deviceCard->setStyleSheet(
            "QFrame { background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #ffffff, stop:1 "
            "#f6f6f6);"
            " border: 1px solid #ddd; border-radius: 6px; padding: 8px; }");
    }

    // ensure the device info label reflects initial state
    if (auto lbl = this->findChild<QLabel*>("labelDeviceCardText"))
        lbl->setText(QCoreApplication::translate("MonitorWidget", "No device selected"));

    connect(ui->comboDevices,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            &MonitorWidget::on_comboDevices_currentIndexChanged);

    // Refresh button: reload devices from registry
    if (auto btnRefresh = this->findChild<QPushButton*>("btnRefresh")) {
        connect(btnRefresh, &QPushButton::clicked, this, [this]() {
            ui->comboDevices->clear();
            if (gateway_) {
                auto devs = gateway_->registry().listDevices();
                for (const auto& d : devs) {
                    const QString text = d.endpoint.isEmpty() ? d.name + " (" + d.id + ")"
                                                              : d.name + " - " + d.endpoint;
                    ui->comboDevices->addItem(text, d.id);
                }
            }
        });
    }

    // Child operation widgets manage their own UI signals (HTTP/TCP/UDP)
    // ensure actions table exists when storage available (child widgets expect it)
    ensureActionsTable();
    // create UDP socket lazily when needed
}

// Simple worker object that will live in the tcp worker thread and own the QTcpSocket
class TcpWorkerObj : public QObject {
   public:
    QTcpSocket* socket = nullptr;
    ~TcpWorkerObj() override {
        if (socket) {
            socket->close();
            socket->deleteLater();
            socket = nullptr;
        }
    }
};

MonitorWidget::~MonitorWidget() {
    // ensure worker stopped
    if (m_workerThread) {
        m_workerThread->quit();
        m_workerThread->wait(2000);
    }
    if (m_syncConn) {
        m_syncConn->close();
        delete m_syncConn;
        m_syncConn = nullptr;
    }
    if (m_workerThread) {
        delete m_workerThread;
        m_workerThread = nullptr;
    }
    delete ui;
}

void MonitorWidget::startListening() {
    bool ok = false;
    int port = 0;
    if (m_listenEdit) port = m_listenEdit->text().toInt(&ok);
    if (!ok) port = ui->editPort->text().toInt(&ok);
    if (!ok) port = 8086;
    if (m_server->listen(QHostAddress::Any, port)) {
        spdlog::info("listening on port {}", port);
        if (auto btn = this->findChild<QPushButton*>("btnListen")) {
            btn->setText(QCoreApplication::translate("MonitorWidget", "Listening..."));
            // Use a yellow with black text and subtle border to ensure readability in
            // light themes (white-on-yellow has poor contrast).
            btn->setStyleSheet(
                "background-color: #FFD54F; color: black; border: 1px solid #b38f00");
        }
    } else {
        spdlog::error("failed to listen on port {}", port);
        if (auto btn = this->findChild<QPushButton*>("btnListen")) {
            btn->setText(QCoreApplication::translate("MonitorWidget", "Listen Failed"));
            btn->setStyleSheet("background-color: red; color: white");
        }
    }
}

void MonitorWidget::stopListening() {
    if (m_server->isListening()) m_server->close();
    // close all client sockets
    for (auto sock : m_clientBuffers.keys()) {
        if (sock) {
            sock->disconnectFromHost();
            sock->deleteLater();
        }
    }
    m_clientBuffers.clear();
    if (auto btn = this->findChild<QPushButton*>("btnListen")) {
        btn->setText(QCoreApplication::translate("MonitorWidget", "Listen"));
        btn->setStyleSheet("");
    }
}

void MonitorWidget::handleIncomingData(QTcpSocket* sock, const QByteArray& data) {
    // accumulate per-client
    QByteArray& buf = m_clientBuffers[sock];
    buf.append(data);
    // reuse parsing logic similar to onDataReady
    while (true) {
        if (buf.size() < 6) break;
        int headIndex = buf.indexOf(QByteArray::fromHex("AA55"));
        if (headIndex == -1) {
            buf.clear();
            break;
        }
        if (headIndex > 0) buf.remove(0, headIndex);
        if (buf.size() < 6) break;
        uint32_t jpegLen = (uint8_t)buf[2] | ((uint8_t)buf[3] << 8) | ((uint8_t)buf[4] << 16) |
                           ((uint8_t)buf[5] << 24);
        uint32_t fullLen = 2 + 4 + jpegLen + 2;
        if (buf.size() < fullLen) break;
        QByteArray jpegData = buf.mid(6, jpegLen);
        QByteArray crcData = buf.mid(6 + jpegLen, 2);
        uint16_t calc = crc16_ccitt((const uint8_t*)jpegData.constData(), jpegData.size());
        uint16_t recv = (uint8_t)crcData[0] | ((uint8_t)crcData[1] << 8);
        if (calc != recv) {
            spdlog::warn("CRC mismatch");
            buf.remove(0, fullLen);
            continue;
        }
        QPixmap pixmap;
        if (!pixmap.loadFromData(jpegData)) {
            spdlog::warn("failed to load pixmap from server client");
            buf.remove(0, fullLen);
            continue;
        }
        m_lastPixmap = pixmap;
        QPixmap scaled = pixmap.scaled(ui->labelMonitorFeed->size(), Qt::KeepAspectRatio);
        ui->labelMonitorFeed->setPixmap(scaled);

        // optionally save
        QString saveDir = ui->editSavePath->text();
        QDir d(saveDir);
        if (!d.exists()) d.mkpath(".");
        QString filename = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz") + ".jpg";
        QFile f(d.filePath(filename));
        if (f.open(QIODevice::WriteOnly)) {
            f.write(jpegData);
            f.close();
        }
        buf.remove(0, fullLen);
    }
}

// legacy send textbox removed; old broadcast handler removed.

void MonitorWidget::on_btnHttpSend_clicked() {
    // Forward to HttpOpsWidget (if present) to avoid duplicated HTTP implementation
    auto http = this->findChild<HttpOpsWidget*>(QStringLiteral("tabHttp"));
    if (!http) http = this->findChild<HttpOpsWidget*>();
    if (http) {
        QMetaObject::invokeMethod(http, "on_btnSend_clicked", Qt::QueuedConnection);
    } else {
        spdlog::warn("Monitor: HttpOpsWidget not found to forward HTTP send");
    }
}

int MonitorWidget::broadcastToClients(const QByteArray& data) {
    int cnt = 0;
    for (QTcpSocket* s : m_clientBuffers.keys()) {
        if (s && s->state() == QAbstractSocket::ConnectedState) {
            qint64 n = s->write(data);
            s->flush();
            if (n > 0) {
                ++cnt;
                spdlog::debug("Sent {} bytes to client {}:{}",
                              n,
                              s->peerAddress().toString().toStdString(),
                              s->peerPort());
            } else {
                spdlog::warn("Failed to send to client {}:{}",
                             s->peerAddress().toString().toStdString(),
                             s->peerPort());
            }
        }
    }
    return cnt;
}

void MonitorWidget::ensureActionsTable() {
    using namespace storage;
    try {
        if (!Storage::db().isOpen()) return;
        QSqlQuery q(Storage::db());
        // Create per-type actions tables so different action types can be stored separately
        q.exec(
            "CREATE TABLE IF NOT EXISTS http_actions (name TEXT PRIMARY KEY, payload BLOB, "
            "created_at TEXT)");
        q.exec(
            "CREATE TABLE IF NOT EXISTS tcp_actions (name TEXT PRIMARY KEY, payload BLOB, "
            "created_at TEXT)");
        q.exec(
            "CREATE TABLE IF NOT EXISTS udp_actions (name TEXT PRIMARY KEY, payload BLOB, "
            "created_at TEXT)");
        q.exec(
            "CREATE TABLE IF NOT EXISTS serial_actions (name TEXT PRIMARY KEY, payload BLOB, "
            "created_at TEXT)");
    } catch (...) { spdlog::warn("ensureActionsTable: storage not available or failed"); }
}

int MonitorWidget::saveActionToDb(const QString& name,
                                  const QString& type,
                                  const QByteArray& payload) {
    using namespace storage;
    try {
        if (!Storage::db().isOpen()) return -1;
        QSqlQuery q(Storage::db());
        q.prepare(
            "INSERT INTO actions (name, type, payload, created_at) VALUES (?, ?, ?, "
            "datetime('now'))");
        q.addBindValue(name);
        q.addBindValue(type);
        q.addBindValue(payload);
        if (!q.exec()) {
            spdlog::error("Failed to insert action: {}", q.lastError().text().toStdString());
            return -1;
        }
        return q.lastInsertId().toInt();
    } catch (const std::exception& e) {
        spdlog::warn("saveActionToDb exception: {}", e.what());
    } catch (...) { spdlog::warn("saveActionToDb unknown exception"); }
    return -1;
}

QVector<QPair<int, QString>> MonitorWidget::loadActionsFromDb(const QString& type) {
    using namespace storage;
    QVector<QPair<int, QString>> out;
    try {
        if (!Storage::db().isOpen()) return out;
        QSqlQuery q(Storage::db());
        if (type.isEmpty())
            q.exec("SELECT id, name FROM actions ORDER BY id DESC LIMIT 100");
        else {
            q.prepare("SELECT id, name FROM actions WHERE type = ? ORDER BY id DESC LIMIT 100");
            q.addBindValue(type);
            q.exec();
        }
        while (q.next()) {
            int id = q.value(0).toInt();
            QString name = q.value(1).toString();
            out.append(qMakePair(id, name));
        }
    } catch (...) { spdlog::warn("loadActionsFromDb failed"); }
    return out;
}

// TCP operations removed

QByteArray MonitorWidget::serializeHttpAction(const QString& url,
                                              const QString& method,
                                              const QString& token,
                                              bool autoBearer,
                                              const QByteArray& body) {
    // Deprecated: HttpOpsWidget handles serialization. Return empty to indicate not used.
    Q_UNUSED(url)
    Q_UNUSED(method)
    Q_UNUSED(token)
    Q_UNUSED(autoBearer)
    Q_UNUSED(body)
    spdlog::warn("Monitor::serializeHttpAction called: use HttpOpsWidget instead");
    return QByteArray();
}

bool MonitorWidget::deserializeHttpAction(const QByteArray& payload,
                                          QString& url,
                                          QString& method,
                                          QString& token,
                                          bool& autoBearer,
                                          QByteArray& body) {
    Q_UNUSED(payload)
    Q_UNUSED(url)
    Q_UNUSED(method)
    Q_UNUSED(token)
    Q_UNUSED(autoBearer)
    Q_UNUSED(body)
    spdlog::warn("Monitor::deserializeHttpAction called: use HttpOpsWidget instead");
    return false;
}

void MonitorWidget::on_btnSaveHttpAction_clicked() {
    auto http = this->findChild<HttpOpsWidget*>(QStringLiteral("tabHttp"));
    if (!http) http = this->findChild<HttpOpsWidget*>();
    if (http) {
        QMetaObject::invokeMethod(http, "on_btnSaveAction_clicked", Qt::QueuedConnection);
    } else {
        spdlog::warn("Monitor: HttpOpsWidget not found to forward save http action");
    }
}

void MonitorWidget::on_btnLoadHttpAction_clicked() {
    auto http = this->findChild<HttpOpsWidget*>(QStringLiteral("tabHttp"));
    if (!http) http = this->findChild<HttpOpsWidget*>();
    if (http) {
        QMetaObject::invokeMethod(http, "on_btnLoadAction_clicked", Qt::QueuedConnection);
    } else {
        spdlog::warn("Monitor: HttpOpsWidget not found to forward load http action");
    }
}

// TCP load action removed

// UDP action forwarding removed

void MonitorWidget::on_btnBackFromMonitor_clicked() {
    emit backToMain();
}

void MonitorWidget::on_btnChoosePath_clicked() {
    QString dir = QFileDialog::getExistingDirectory(
        this,
        QCoreApplication::translate("MonitorWidget", "Choose save path"),
        ui->editSavePath->text());
    if (!dir.isEmpty()) {
        ui->editSavePath->setText(dir);
        config::ConfigManager::instance().setValue("Monitor/savePath", dir);
        QDir d(dir);
        if (!d.exists()) d.mkpath(".");
    }

    // ensure edit fields exist (if UI created them)
    if (ui->comboDevices->count() > 0) {
        // populate host/port from current selection
        const QString cur = ui->comboDevices->currentText();
        const auto p = cur.split(':');
        if (p.size() == 2) {
            ui->editHost->setText(p[0]);
            ui->editPort->setText(p[1]);
        }
    }

    // Note: on_comboDevices_currentIndexChanged auto-slot will also run when selection changes
}

void MonitorWidget::on_btnConnect_clicked() {
    const QString ep = ui->comboDevices->currentText();
    if (ep.isEmpty()) {
        spdlog::warn("no endpoint selected");
        return;
    }
    spdlog::info("connecting to {}", ep.toStdString());
    m_recvBuffer.clear();
    // endpoint 格式 host:port
    const auto parts = ep.split(':');
    if (parts.size() != 2) {
        spdlog::error("invalid endpoint {}", ep.toStdString());
        return;
    }
    bool ok = false;
    int port = parts[1].toInt(&ok);
    if (!ok) {
        spdlog::error("invalid port in endpoint");
        return;
    }
    // create worker and thread
    if (m_workerThread) {
        spdlog::warn("already running");
        return;
    }
    m_syncConn = new TcpConnect();
    m_workerThread = new QThread();

    // worker object
    QObject* worker = new QObject();
    worker->moveToThread(m_workerThread);

    // when thread starts, open connection and run loop via lambda posted to thread
    const QString endpoint = parts[0] + ":" + QString::number(port);
    connect(m_workerThread, &QThread::started, worker, [this, endpoint]() {
        spdlog::info("worker thread started, opening {}", endpoint.toStdString());
        if (!m_syncConn->open(endpoint)) {
            spdlog::error("sync open failed");
            return;
        }
        // simple loop: try to receive small chunks with timeout
        while (m_syncConn->isOpen()) {
            QByteArray out;
            qint64 r = m_syncConn->receive(out, 65536);
            if (r > 0) {
                // emit to UI thread via queued connection using lambda + QMetaObject
                QMetaObject::invokeMethod(
                    qApp, [this, out]() { this->onDataReady(out); }, Qt::QueuedConnection);
            } else {
                // sleep a bit to avoid busy loop
                QThread::msleep(20);
            }
        }
    });

    // when thread finishes, cleanup worker
    connect(m_workerThread, &QThread::finished, worker, &QObject::deleteLater);
    m_workerThread->start();
}

void MonitorWidget::on_btnSaveDevice_clicked() {
    QString host = ui->editHost->text().trimmed();
    QString port = ui->editPort->text().trimmed();
    if (host.isEmpty() || port.isEmpty()) {
        spdlog::warn("host or port empty");
        return;
    }
    QString ep = host + ":" + port;
    int idx = ui->comboDevices->currentIndex();
    if (idx >= 0) {
        ui->comboDevices->setItemText(idx, ep);
    } else {
        ui->comboDevices->addItem(ep);
        ui->comboDevices->setCurrentIndex(ui->comboDevices->count() - 1);
    }
    // persist to config as JSON array
    QJsonArray arr;
    for (int i = 0; i < ui->comboDevices->count(); ++i) arr.append(ui->comboDevices->itemText(i));
    QJsonDocument doc(arr);
    config::ConfigManager::instance().setValue(
        "Connect/devices", QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
}

void MonitorWidget::on_comboDevices_currentIndexChanged(int index) {
    if (index < 0) return;
    // If we have a DeviceGateway, use stored registry entries (itemData contains device id)
    QVariant idData = ui->comboDevices->itemData(index);
    if (gateway_ && idData.isValid()) {
        const QString id = idData.toString();
        auto opt = gateway_->registry().getDevice(id);
        if (opt) {
            const auto d = *opt;
            QString info =
                QCoreApplication::translate("MonitorWidget", "Name: %1\nType: %2\nLast seen: %3")
                    .arg(d.name)
                    .arg(d.type)
                    .arg(d.lastSeen.isValid()
                             ? d.lastSeen.toString()
                             : QCoreApplication::translate("MonitorWidget", "N/A"));
            lblDeviceInfo_->setText(info);
            // also populate host/port edit fields if endpoint looks like host:port
            const auto p = d.endpoint.split(':');
            if (p.size() == 2) {
                ui->editHost->setText(p[0]);
                ui->editPort->setText(p[1]);
            }
            return;
        }
    }
    // fallback: previous behavior parsing host:port from plain text
    const QString cur = ui->comboDevices->itemText(index);
    const auto p = cur.split(':');
    if (p.size() == 2) {
        ui->editHost->setText(p[0]);
        ui->editPort->setText(p[1]);
    }
}

void MonitorWidget::on_btnStop_clicked() {
    if (m_workerThread) {
        if (m_syncConn) m_syncConn->close();
        m_workerThread->quit();
        m_workerThread->wait(2000);
        delete m_workerThread;
        m_workerThread = nullptr;
        if (m_syncConn) {
            delete m_syncConn;
            m_syncConn = nullptr;
        }
    }
}

void MonitorWidget::onDataReady(const QByteArray& data) {
    // append and try parse frames
    m_recvBuffer.append(data);
    while (true) {
        if (m_recvBuffer.size() < 6) break;
        int headIndex = m_recvBuffer.indexOf(QByteArray::fromHex("AA55"));
        if (headIndex == -1) {
            m_recvBuffer.clear();
            break;
        }
        if (headIndex > 0) m_recvBuffer.remove(0, headIndex);
        if (m_recvBuffer.size() < 6) break;
        uint32_t jpegLen = (uint8_t)m_recvBuffer[2] | ((uint8_t)m_recvBuffer[3] << 8) |
                           ((uint8_t)m_recvBuffer[4] << 16) | ((uint8_t)m_recvBuffer[5] << 24);
        uint32_t fullLen = 2 + 4 + jpegLen + 2;
        if (m_recvBuffer.size() < fullLen) break;
        QByteArray jpegData = m_recvBuffer.mid(6, jpegLen);
        QByteArray crcData = m_recvBuffer.mid(6 + jpegLen, 2);
        uint16_t calc = crc16_ccitt((const uint8_t*)jpegData.constData(), jpegData.size());
        uint16_t recv = (uint8_t)crcData[0] | ((uint8_t)crcData[1] << 8);
        if (calc != recv) {
            spdlog::warn("CRC mismatch");
            m_recvBuffer.remove(0, fullLen);
            continue;
        }
        QPixmap pixmap;
        if (!pixmap.loadFromData(jpegData)) {
            spdlog::warn("failed to load pixmap");
            m_recvBuffer.remove(0, fullLen);
            continue;
        }
        m_lastPixmap = pixmap;
        QPixmap scaled = pixmap.scaled(ui->labelMonitorFeed->size(), Qt::KeepAspectRatio);
        ui->labelMonitorFeed->setPixmap(scaled);

        // save to disk
        QString saveDir = ui->editSavePath->text();
        QDir d(saveDir);
        if (!d.exists()) d.mkpath(".");
        QString filename = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz") + ".jpg";
        QFile f(d.filePath(filename));
        if (f.open(QIODevice::WriteOnly)) {
            f.write(jpegData);
            f.close();
        } else {
            spdlog::warn("failed to save image to {}", d.filePath(filename).toStdString());
        }

        m_recvBuffer.remove(0, fullLen);
    }
}

uint16_t MonitorWidget::crc16_ccitt(const uint8_t* buf, uint32_t len) {
    uint16_t crc = 0xFFFF;
    while (len--) {
        crc ^= (uint16_t)(*buf++) << 8;
        for (int k = 0; k < 8; ++k) {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc <<= 1;
        }
    }
    return crc;
}

void MonitorWidget::setTcpConnected(bool connected) {
    // Update status label
    if (auto lbl = this->findChild<QLabel*>("labelTcpStatus")) {
        if (connected) {
            lbl->setText(QCoreApplication::translate("MonitorWidget", "Connected"));
            lbl->setStyleSheet("color: #2e7d32; font-weight: bold;");
        } else {
            lbl->setText(QCoreApplication::translate("MonitorWidget", "Disconnected"));
            lbl->setStyleSheet("color: #b00020; font-weight: bold;");
        }
    }

    // enable/disable TCP send button
    if (auto btn = this->findChild<QPushButton*>("btnTcpSend")) btn->setEnabled(connected);
}