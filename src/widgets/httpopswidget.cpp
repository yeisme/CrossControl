#include "httpopswidget.h"

#include <qcoreapplication.h>
#include <spdlog/spdlog.h>

#include <QCoreApplication>
#include <QDateTime>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QtSql/QSqlError>
#include <QtSql/QSqlQuery>

#include "keyvalueeditor.h"
#include "modules/Storage/dbmanager.h"
#include "modules/Storage/storage.h"
#include "ui_httpopswidget.h"
#include "widgets/actionmanager.h"

HttpOpsWidget::HttpOpsWidget(QWidget* parent) : QWidget(parent), ui(new Ui::HttpOpsWidget) {
    ui->setupUi(this);
    m_net = new QNetworkAccessManager(this);
    // Buttons are connected via auto-connect (on_<object>_<signal>() naming)
}

HttpOpsWidget::~HttpOpsWidget() {
    delete ui;
}

void HttpOpsWidget::on_btnSend_clicked() {
    QString url = ui->editUrl->text().trimmed();
    if (url.isEmpty()) return;
    QNetworkRequest netReq;
    netReq.setUrl(QUrl(url));
    QString token = ui->editToken->text().trimmed();
    if (!token.isEmpty()) {
        if (ui->chkBearer->isChecked() && !token.startsWith("Bearer ", Qt::CaseInsensitive))
            token = QString("Bearer ") + token;
        netReq.setRawHeader(QByteArrayLiteral("Authorization"), token.toUtf8());
    }
    QNetworkReply* r = nullptr;
    const QString method = ui->comboMethod->currentText().toUpper();
    const QByteArray body = ui->teRequest->toPlainText().toUtf8();
    // Determine Content-Type header: priority is user-provided editContentType,
    // otherwise use Send as JSON checkbox or automatic JSON detection for methods with body.
    QString contentType = ui->editContentType->text().trimmed();
    bool sendAsJson = ui->chkSendJson->isChecked();
    auto looksLikeJson = [](const QByteArray& b) -> bool {
        QJsonParseError err;
        QJsonDocument d = QJsonDocument::fromJson(b, &err);
        return (err.error == QJsonParseError::NoError) && (d.isObject() || d.isArray());
    };
    const QStringList methodsWithBody = {"POST", "PUT", "PATCH", "OPTIONS"};
    if (contentType.isEmpty() && methodsWithBody.contains(method)) {
        if (sendAsJson || looksLikeJson(body)) contentType = QStringLiteral("application/json");
    }
    if (!contentType.isEmpty()) netReq.setHeader(QNetworkRequest::ContentTypeHeader, contentType);
    if (method == "GET") {
        r = m_net->get(netReq);
    } else if (method == "POST") {
        r = m_net->post(netReq, body);
    } else if (method == "PUT") {
        r = m_net->put(netReq, body);
    } else if (method == "DELETE") {
        r = m_net->deleteResource(netReq);
    } else if (method == "HEAD") {
        r = m_net->head(netReq);
    } else if (method == "PATCH") {
        r = m_net->sendCustomRequest(netReq, QByteArrayLiteral("PATCH"), body);
    } else if (method == "OPTIONS") {
        r = m_net->sendCustomRequest(netReq, QByteArrayLiteral("OPTIONS"), body);
    } else {
        // fallback to POST behavior
        r = m_net->post(netReq, body);
    }
    ui->btnSend->setEnabled(false);
    connect(r, &QNetworkReply::finished, this, [this, r]() {
        ui->btnSend->setEnabled(true);
        if (r->error() != QNetworkReply::NoError) {
            ui->teResponse->setPlainText(r->errorString());
        } else {
            ui->teResponse->setPlainText(QString::fromUtf8(r->readAll()));
        }
        r->deleteLater();
    });
}

QByteArray HttpOpsWidget::serializeHttpAction(const QString& url,
                                              const QString& method,
                                              const QString& token,
                                              bool autoBearer,
                                              const QByteArray& body,
                                              const QString& contentType,
                                              bool sendAsJson) {
    QJsonObject obj;
    obj["url"] = url;
    obj["method"] = method;
    obj["token"] = token;
    obj["auto_bearer"] = autoBearer;
    obj["body"] = QString::fromUtf8(body);
    obj["content_type"] = contentType;
    obj["send_as_json"] = sendAsJson;
    // additional metadata
    obj["saved_by"] = QCoreApplication::applicationName();
    obj["saved_at"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}

bool HttpOpsWidget::deserializeHttpAction(const QByteArray& payload,
                                          QString& url,
                                          QString& method,
                                          QString& token,
                                          bool& autoBearer,
                                          QByteArray& body,
                                          QString& contentType,
                                          bool& sendAsJson) {
    QJsonParseError err;
    QJsonDocument d = QJsonDocument::fromJson(payload, &err);
    if (err.error != QJsonParseError::NoError || !d.isObject()) return false;
    QJsonObject obj = d.object();
    url = obj.value("url").toString();
    method = obj.value("method").toString("GET");
    token = obj.value("token").toString();
    autoBearer = obj.value("auto_bearer").toBool(false);
    body = obj.value("body").toString().toUtf8();
    contentType = obj.value("content_type").toString();
    sendAsJson = obj.value("send_as_json").toBool(false);
    return true;
}

void HttpOpsWidget::on_btnSaveAction_clicked() {
    QString url = ui->editUrl->text();
    QString method = ui->comboMethod->currentText();
    QString token = ui->editToken->text();
    bool autoBearer = ui->chkBearer->isChecked();
    QByteArray body = ui->teRequest->toPlainText().toUtf8();
    QString contentType = ui->editContentType->text().trimmed();
    bool sendAsJson = ui->chkSendJson->isChecked();
    bool ok;
    QString name =
        QInputDialog::getText(this,
                              QCoreApplication::translate("HttpOpsWidget", "Save HTTP Action"),
                              QCoreApplication::translate("HttpOpsWidget", "Action name:"),
                              QLineEdit::Normal,
                              QString(),
                              &ok);
    if (!ok || name.isEmpty()) return;
    QByteArray payload =
        serializeHttpAction(url, method, token, autoBearer, body, contentType, sendAsJson);
    using namespace storage;
    // lazy initialize DB if needed (use app dir default)
    if (!DbManager::instance().initialized()) {
        QString defaultDb =
            QCoreApplication::applicationDirPath() + QLatin1String("/crosscontrol.db");
        if (!Storage::initSqlite(defaultDb)) {
            QMessageBox::warning(
                this,
                QCoreApplication::translate("HttpOpsWidget", "Storage"),
                QCoreApplication::translate("HttpOpsWidget", "Storage not initialized."));
            return;
        }
    }
    if (!Storage::db().isOpen()) {
        QMessageBox::warning(
            this,
            QCoreApplication::translate("HttpOpsWidget", "Storage"),
            QCoreApplication::translate("HttpOpsWidget", "Storage not initialized."));
        return;
    }
    if (!Storage::saveAction("http", name, payload)) {
        QMessageBox::warning(this,
                             QCoreApplication::translate("HttpOpsWidget", "Save"),
                             QCoreApplication::translate("HttpOpsWidget", "Failed to save action"));
        return;
    }
    QMessageBox::information(
        this,
        QCoreApplication::translate("HttpOpsWidget", "Saved"),
        QCoreApplication::translate("HttpOpsWidget", "Saved action '%1'").arg(name));
}

void HttpOpsWidget::on_btnLoadAction_clicked() {
    // open action manager dialog for HTTP
    ActionManager dlg(QStringLiteral("http"), this);
    connect(&dlg,
            &ActionManager::actionLoaded,
            this,
            [this](const QString&, const QByteArray& payload) {
                QString url, method, token, contentType;
                bool autoBearer = false;
                QByteArray body;
                bool sendAsJson = false;
                if (!deserializeHttpAction(
                        payload, url, method, token, autoBearer, body, contentType, sendAsJson))
                    return;
                ui->editUrl->setText(url);
                int comboIdx = ui->comboMethod->findText(method);
                if (comboIdx >= 0) ui->comboMethod->setCurrentIndex(comboIdx);
                ui->editToken->setText(token);
                ui->chkBearer->setChecked(autoBearer);
                ui->teRequest->setPlainText(QString::fromUtf8(body));
                ui->editContentType->setText(contentType);
                ui->chkSendJson->setChecked(sendAsJson);
            });
    dlg.exec();
}

void HttpOpsWidget::on_btnClear_clicked() {
    // clear request and response text areas
    if (auto teReq = this->findChild<QTextEdit*>(QStringLiteral("teRequest"))) teReq->clear();
    if (auto teResp = this->findChild<QTextEdit*>(QStringLiteral("teResponse"))) teResp->clear();
}

void HttpOpsWidget::on_btnKV_clicked() {
    // open key-value editor and insert JSON into request body when requested
    KeyValueEditor* dlg = new KeyValueEditor(this);
    connect(dlg, &KeyValueEditor::insertRequested, this, [this](const QString& json) {
        if (auto teReq = this->findChild<QTextEdit*>(QStringLiteral("teRequest"))) {
            teReq->setPlainText(json);
        }
    });
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->exec();
}
