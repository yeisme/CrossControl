#include "monitorwidget.h"

#include <qcoreapplication.h>

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QHostAddress>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QLineEdit>
#include <QPixmap>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>
#include <QTimer>

#include "modules/Config/config.h"
#include "modules/Connect/tcp_connect.h"
#include "spdlog/spdlog.h"
#include "ui_monitorwidget.h"

MonitorWidget::MonitorWidget(QWidget* parent) : QWidget(parent), ui(new Ui::MonitorWidget) {
    ui->setupUi(this);
    spdlog::debug("MonitorWidget constructed");
    // no-op here; worker created when user clicks Start
    // Note: ui->setupUi calls QMetaObject::connectSlotsByName(this) which
    // auto-connects slots named like on_<object>_<signal>(). Avoid manual
    // connects to prevent duplicate invocations.

    ui->btnStop->setEnabled(false);

    // setup server
    m_server = new QTcpServer(this);
    // find optional listen port edit (uic may or may not have created a named member)
    m_listenEdit = this->findChild<QLineEdit*>("editListenPort");
    // find send edit/button
    auto sendEdit = this->findChild<QLineEdit*>("editSendText");
    auto sendBtn = this->findChild<QPushButton*>("btnSendText");
    if (sendBtn)
        connect(sendBtn, &QPushButton::clicked, this, &MonitorWidget::on_btnSendText_clicked);
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

    // listen button
    connect(ui->btnListen, &QPushButton::clicked, this, [this]() {
        if (m_server && m_server->isListening())
            stopListening();
        else
            startListening();
    });

    // load devices from config
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

    // default save path
    QString def = cfg.getOrDefault("Monitor/savePath", QVariant("./video_data")).toString();
    ui->editSavePath->setText(def);
    QDir d(def);
    if (!d.exists()) d.mkpath(".");
}

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
        ui->btnListen->setText("Listening...");
        ui->btnListen->setStyleSheet("background-color: yellow");
    } else {
        spdlog::error("failed to listen on port {}", port);
        ui->btnListen->setText("Listen Failed");
        ui->btnListen->setStyleSheet("background-color: red; color: white");
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
    ui->btnListen->setText("Listen");
    ui->btnListen->setStyleSheet("");
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

void MonitorWidget::on_btnSendText_clicked() {
    QLineEdit* edit = this->findChild<QLineEdit*>("editSendText");
    if (!edit) return;
    QString txt = edit->text();
    if (txt.isEmpty()) return;
    QByteArray bytes = txt.toUtf8();
    // append newline if not present
    if (!bytes.endsWith('\n')) bytes.append('\n');
    broadcastToClients(bytes);
}

void MonitorWidget::broadcastToClients(const QByteArray& data) {
    for (QTcpSocket* s : m_clientBuffers.keys()) {
        if (s && s->state() == QAbstractSocket::ConnectedState) {
            s->write(data);
            s->flush();
        }
    }
}

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