
#include "tcpopswidget.h"

#include <spdlog/spdlog.h>

#include <QAbstractSocket>
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
#include <QTcpSocket>
#include <QTextEdit>
#include <QTimer>
#include <QtSql/QSqlError>
#include <QtSql/QSqlQuery>

#include "modules/Storage/dbmanager.h"
#include "modules/Storage/storage.h"
#include "ui_tcpopswidget.h"
#include "widgets/actionmanager.h"

TcpOpsWidget::TcpOpsWidget(QWidget* parent) : QWidget(parent), ui(new Ui::TcpOpsWidget) {
    ui->setupUi(this);
}

TcpOpsWidget::~TcpOpsWidget() {
    delete ui;
}

void TcpOpsWidget::on_btnConnect_clicked() {
    if (m_sock) return;
    QString host = ui->editHost->text().trimmed();
    int port = ui->editPort->text().toInt();
    if (host.isEmpty() || port <= 0) return;
    m_sock = new QTcpSocket(this);
    connect(m_sock, &QTcpSocket::connected, this, [this]() { setConnected(true); });
    connect(m_sock, &QTcpSocket::disconnected, this, [this]() { setConnected(false); });
    connect(m_sock, &QTcpSocket::readyRead, this, [this]() {
        QByteArray d = m_sock->readAll();
        ui->teRecv->append(QString::fromUtf8(d));
    });
    m_sock->connectToHost(host, static_cast<quint16>(port));
}

void TcpOpsWidget::on_btnDisconnect_clicked() {
    if (!m_sock) return;
    m_sock->disconnectFromHost();
    m_sock->deleteLater();
    m_sock = nullptr;
    setConnected(false);
}

void TcpOpsWidget::on_btnSend_clicked() {
    if (!m_sock || m_sock->state() != QAbstractSocket::ConnectedState) {
        QMessageBox::warning(this,
                             QCoreApplication::translate("TcpOpsWidget", "TCP"),
                             QCoreApplication::translate("TcpOpsWidget", "Not connected"));
        return;
    }
    QString txt = ui->editSend->text();
    if (txt.isEmpty()) return;
    QByteArray b = txt.toUtf8();
    qint64 n = m_sock->write(b);
    spdlog::info("TCP write {} bytes", n);
}

QByteArray TcpOpsWidget::serializeTcpAction(const QString& host, int port, const QByteArray& body) {
    QJsonObject obj;
    obj["host"] = host;
    obj["port"] = port;
    obj["body"] = QString::fromUtf8(body);
    obj["saved_by"] = QCoreApplication::applicationName();
    obj["saved_at"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}

bool TcpOpsWidget::deserializeTcpAction(const QByteArray& payload,
                                        QString& host,
                                        int& port,
                                        QByteArray& body) {
    QJsonParseError err;
    QJsonDocument d = QJsonDocument::fromJson(payload, &err);
    if (err.error != QJsonParseError::NoError || !d.isObject()) return false;
    QJsonObject obj = d.object();
    host = obj.value("host").toString();
    port = obj.value("port").toInt(0);
    body = obj.value("body").toString().toUtf8();
    return true;
}

void TcpOpsWidget::on_btnSaveAction_clicked() {
    QString host = ui->editHost->text();
    int port = ui->editPort->text().toInt();
    QByteArray body = ui->editSend->text().toUtf8();
    bool ok;
    QString name =
        QInputDialog::getText(this,
                              QCoreApplication::translate("TcpOpsWidget", "Save TCP Action"),
                              QCoreApplication::translate("TcpOpsWidget", "Action name:"),
                              QLineEdit::Normal,
                              QString(),
                              &ok);
    if (!ok || name.isEmpty()) return;
    QByteArray payload = serializeTcpAction(host, port, body);
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
    if (!Storage::saveAction("tcp", name, payload)) {
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

void TcpOpsWidget::on_btnLoadAction_clicked() {
    ActionManager dlg(QStringLiteral("tcp"), this);
    connect(&dlg,
            &ActionManager::actionLoaded,
            this,
            [this](const QString& name, const QByteArray& payload) {
                QString host;
                int port = 0;
                QByteArray body;
                if (!deserializeTcpAction(payload, host, port, body)) return;
                ui->editHost->setText(host);
                ui->editPort->setText(QString::number(port));
                ui->editSend->setText(QString::fromUtf8(body));
                if (auto lbl = this->findChild<QLabel*>(QStringLiteral("labelLoadedAction")))
                    lbl->setText(name);
            });
    dlg.exec();
}

void TcpOpsWidget::on_btnClear_clicked() {
    if (auto te = this->findChild<QTextEdit*>(QStringLiteral("teRecv"))) te->clear();
    if (auto ed = this->findChild<QLineEdit*>(QStringLiteral("editSend"))) ed->clear();
    if (auto lbl = this->findChild<QLabel*>(QStringLiteral("labelLoadedAction"))) lbl->clear();
}

void TcpOpsWidget::setConnected(bool connected) {
    ui->btnConnect->setEnabled(!connected);
    ui->btnDisconnect->setEnabled(connected);
    ui->btnSend->setEnabled(connected);
    ui->labelStatus->setText(connected
                                 ? QCoreApplication::translate("TcpOpsWidget", "Connected")
                                 : QCoreApplication::translate("TcpOpsWidget", "Disconnected"));
}
