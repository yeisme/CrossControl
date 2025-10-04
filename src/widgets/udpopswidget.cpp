#include "udpopswidget.h"

#include <spdlog/spdlog.h>

#include <QCoreApplication>
#include <QDateTime>
#include <QHostAddress>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QTextEdit>
#include <QUdpSocket>

#include "modules/Storage/dbmanager.h"
#include "modules/Storage/storage.h"
#include "ui_udpopswidget.h"
#include "widgets/actionmanager.h"

UdpOpsWidget::UdpOpsWidget(QWidget* parent) : QWidget(parent), ui(new Ui::UdpOpsWidget) {
    ui->setupUi(this);
    spdlog::info("UdpOpsWidget constructed; parent='{}'",
                 parent ? parent->objectName().toStdString() : std::string("(none)"));
}

UdpOpsWidget::~UdpOpsWidget() {
    delete ui;
}

void UdpOpsWidget::on_btnBind_clicked() {
    if (m_sock) return;
    int port = ui->editBindPort->text().toInt();
    if (port <= 0) return;
    m_sock = new QUdpSocket(this);
    if (!m_sock->bind(static_cast<quint16>(port))) {
        QMessageBox::warning(this,
                             QCoreApplication::translate("TcpOpsWidget", "UDP"),
                             QCoreApplication::translate("TcpOpsWidget", "Failed to bind port"));
        m_sock->deleteLater();
        m_sock = nullptr;
        return;
    }
    connect(m_sock, &QUdpSocket::readyRead, this, [this]() {
        while (m_sock->hasPendingDatagrams()) {
            QByteArray buf;
            buf.resize(m_sock->pendingDatagramSize());
            QHostAddress addr;
            quint16 p = 0;
            m_sock->readDatagram(buf.data(), buf.size(), &addr, &p);
            ui->teRecv->append(QString::fromUtf8(buf));
        }
    });
    setBound(true);
}

void UdpOpsWidget::on_btnSend_clicked() {
    if (!m_sock) {
        QMessageBox::warning(this,
                             QCoreApplication::translate("TcpOpsWidget", "UDP"),
                             QCoreApplication::translate("TcpOpsWidget", "Socket not bound"));
        return;
    }
    QString peer = ui->editPeer->text().trimmed();
    if (peer.isEmpty()) return;
    QStringList parts = peer.split(':');
    if (parts.size() != 2) return;
    QString host = parts.at(0);
    int port = parts.at(1).toInt();
    QByteArray b = ui->editSend->text().toUtf8();
    qint64 n = m_sock->writeDatagram(b, QHostAddress(host), static_cast<quint16>(port));
    spdlog::info("UDP send {} bytes to {}:{}", n, host.toStdString(), port);
}

QByteArray UdpOpsWidget::serializeUdpAction(const QString& peer, const QByteArray& body) {
    QJsonObject obj;
    obj["peer"] = peer;
    obj["body"] = QString::fromUtf8(body);
    obj["saved_by"] = QCoreApplication::applicationName();
    obj["saved_at"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}

bool UdpOpsWidget::deserializeUdpAction(const QByteArray& payload,
                                        QString& peer,
                                        QByteArray& body) {
    QJsonParseError err;
    QJsonDocument d = QJsonDocument::fromJson(payload, &err);
    if (err.error != QJsonParseError::NoError || !d.isObject()) return false;
    QJsonObject obj = d.object();
    peer = obj.value("peer").toString();
    body = obj.value("body").toString().toUtf8();
    return true;
}

void UdpOpsWidget::on_btnSaveAction_clicked() {
    QString peer = ui->editPeer->text();
    QByteArray body = ui->editSend->text().toUtf8();
    bool ok;
    QString name =
        QInputDialog::getText(this,
                              QCoreApplication::translate("TcpOpsWidget", "Save UDP Action"),
                              QCoreApplication::translate("TcpOpsWidget", "Action name:"),
                              QLineEdit::Normal,
                              QString(),
                              &ok);
    if (!ok || name.isEmpty()) return;
    QByteArray payload = serializeUdpAction(peer, body);
    using namespace storage;
    if (!DbManager::instance().initialized()) {
        QString defaultDb =
            QCoreApplication::applicationDirPath() + QLatin1String("/crosscontrol.db");
        if (!Storage::initSqlite(defaultDb)) {
            QMessageBox::warning(
                this,
                QCoreApplication::translate("TcpOpsWidget", "Storage"),
                QCoreApplication::translate("TcpOpsWidget", "Storage not initialized."));
            return;
        }
    }
    if (!Storage::db().isOpen()) {
        QMessageBox::warning(
            this,
            QCoreApplication::translate("TcpOpsWidget", "Storage"),
            QCoreApplication::translate("TcpOpsWidget", "Storage not initialized."));
        return;
    }
    if (!Storage::saveAction("udp", name, payload)) {
        QMessageBox::warning(this,
                             QCoreApplication::translate("TcpOpsWidget", "Save"),
                             QCoreApplication::translate("TcpOpsWidget", "Failed to save action"));
        return;
    }
    QMessageBox::information(
        this,
        QCoreApplication::translate("TcpOpsWidget", "Saved"),
        QCoreApplication::translate("TcpOpsWidget", "Saved action '%1'").arg(name));
}

void UdpOpsWidget::on_btnLoadAction_clicked() {
    ActionManager dlg(QStringLiteral("udp"), this);
    connect(&dlg,
            &ActionManager::actionLoaded,
            this,
            [this](const QString& name, const QByteArray& payload) {
                QString peer;
                QByteArray body;
                if (!deserializeUdpAction(payload, peer, body)) return;
                ui->editPeer->setText(peer);
                ui->editSend->setText(QString::fromUtf8(body));
                if (auto lbl = this->findChild<QLabel*>(QStringLiteral("labelLoadedAction")))
                    lbl->setText(name);
            });
    dlg.exec();
}

void UdpOpsWidget::on_btnClear_clicked() {
    if (auto te = this->findChild<QTextEdit*>(QStringLiteral("teRecv"))) te->clear();
    if (auto ed = this->findChild<QLineEdit*>(QStringLiteral("editSend"))) ed->clear();
    if (auto lbl = this->findChild<QLabel*>(QStringLiteral("labelLoadedAction"))) lbl->clear();
}

void UdpOpsWidget::setBound(bool bound) {
    ui->btnBind->setEnabled(!bound);
    ui->btnSend->setEnabled(bound);
}
// UdpOpsWidget removed - stub implementation to satisfy any remaining includes.
#include "udpopswidget.h"

// No functional code. The UDP ops page has been removed from UI and logic.
