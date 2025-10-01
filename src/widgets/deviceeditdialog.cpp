#include "widgets/deviceeditdialog.h"

#include <qcoreapplication.h>

#include <QFormLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QLabel>
#include <QUuid>
#include <QHBoxLayout>
#include <QMessageBox>

DeviceEditDialog::DeviceEditDialog(QWidget* parent) : QDialog(parent) {
    auto* form = new QFormLayout(this);
    leId_ = new QLineEdit(this);
    btnGenId_ = new QPushButton(QCoreApplication::translate("DeviceEditDialog", "Generate UUID"), this);
    leName_ = new QLineEdit(this);
    leStatus_ = new QLineEdit(this);
    leEndpoint_ = new QLineEdit(this);
    leType_ = new QLineEdit(this);
    leHw_ = new QLineEdit(this);
    leFw_ = new QLineEdit(this);
    leOwner_ = new QLineEdit(this);
    leGroup_ = new QLineEdit(this);
    teMeta_ = new QTextEdit(this);
    // Structured auth fields for convenience
    leAuthUser_ = new QLineEdit(this);
    leAuthPass_ = new QLineEdit(this);
    leAuthPass_->setEchoMode(QLineEdit::Password);
    leAuthToken_ = new QLineEdit(this);

    // ID and Name are required fields. Add a red asterisk label after the field label.
    auto* idLabel = new QLabel(QCoreApplication::translate("DeviceEditDialog", "ID <span style=\"color:red\">*</span>"), this);
    idLabel->setTextFormat(Qt::RichText);
    // place ID field and generate button on one row
    auto* idRow = new QHBoxLayout();
    idRow->addWidget(leId_);
    idRow->addWidget(btnGenId_);
    form->addRow(idLabel, idRow);

    auto* nameLabel = new QLabel(QCoreApplication::translate("DeviceEditDialog", "Name <span style=\"color:red\">*</span>"), this);
    nameLabel->setTextFormat(Qt::RichText);
    form->addRow(nameLabel, leName_);
    form->addRow(QCoreApplication::translate("DeviceEditDialog", "Status"), leStatus_);
    form->addRow(QCoreApplication::translate("DeviceEditDialog", "Endpoint"), leEndpoint_);
    form->addRow(QCoreApplication::translate("DeviceEditDialog", "Type"), leType_);
    form->addRow(QCoreApplication::translate("DeviceEditDialog", "HW Info"), leHw_);
    form->addRow(QCoreApplication::translate("DeviceEditDialog", "Firmware"), leFw_);
    form->addRow(QCoreApplication::translate("DeviceEditDialog", "Owner"), leOwner_);
    form->addRow(QCoreApplication::translate("DeviceEditDialog", "Group"), leGroup_);
    form->addRow(QCoreApplication::translate("DeviceEditDialog", "Auth User"), leAuthUser_);
    form->addRow(QCoreApplication::translate("DeviceEditDialog", "Auth Password"), leAuthPass_);
    form->addRow(QCoreApplication::translate("DeviceEditDialog", "Auth Token"), leAuthToken_);
    form->addRow(QCoreApplication::translate("DeviceEditDialog", "Metadata (JSON)"), teMeta_);

    auto* btnBox = new QHBoxLayout();
    auto* ok = new QPushButton(QCoreApplication::translate("DeviceEditDialog", "OK"), this);
    auto* cancel = new QPushButton(QCoreApplication::translate("DeviceEditDialog", "Cancel"), this);
    btnBox->addWidget(ok);
    btnBox->addWidget(cancel);
    form->addRow(btnBox);

    // validate required fields before accept
    connect(ok, &QPushButton::clicked, this, [this]() {
        if (leId_->text().trimmed().isEmpty() || leName_->text().trimmed().isEmpty()) {
            QMessageBox::warning(this, QCoreApplication::translate("DeviceEditDialog", "Missing fields"),
                                 QCoreApplication::translate("DeviceEditDialog", "Please fill required fields: ID and Name."));
            return;
        }
        QDialog::accept();
    });
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);

    connect(btnGenId_, &QPushButton::clicked, this, [this]() {
        const auto uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
        leId_->setText(uuid);
    });
}

void DeviceEditDialog::setDevice(const DeviceGateway::DeviceInfo& d) {
    leId_->setText(d.id);
    leName_->setText(d.name);
    leStatus_->setText(d.status);
    leEndpoint_->setText(d.endpoint);
    leType_->setText(d.type);
    leHw_->setText(d.hw_info);
    leFw_->setText(d.firmware_version);
    leOwner_->setText(d.owner);
    leGroup_->setText(d.group);
    teMeta_->setPlainText(QString::fromUtf8(
        QJsonDocument(QJsonObject::fromVariantMap(d.metadata)).toJson(QJsonDocument::Compact)));
    // populate structured auth fields from metadata if present
    if (d.metadata.contains("auth_user")) leAuthUser_->setText(d.metadata.value("auth_user").toString());
    if (d.metadata.contains("auth_pass")) leAuthPass_->setText(d.metadata.value("auth_pass").toString());
    if (d.metadata.contains("auth_token")) leAuthToken_->setText(d.metadata.value("auth_token").toString());
}

DeviceGateway::DeviceInfo DeviceEditDialog::device() const {
    DeviceGateway::DeviceInfo d;
    d.id = leId_->text();
    d.name = leName_->text();
    d.status = leStatus_->text();
    d.endpoint = leEndpoint_->text();
    d.type = leType_->text();
    d.hw_info = leHw_->text();
    d.firmware_version = leFw_->text();
    d.owner = leOwner_->text();
    d.group = leGroup_->text();
    const auto txt = teMeta_->toPlainText();
    if (!txt.isEmpty()) {
        const auto doc = QJsonDocument::fromJson(txt.toUtf8());
        if (doc.isObject()) d.metadata = doc.object().toVariantMap();
    }
    // Structured auth fields override or populate metadata
    if (!leAuthUser_->text().isEmpty()) d.metadata.insert("auth_user", leAuthUser_->text());
    if (!leAuthPass_->text().isEmpty()) d.metadata.insert("auth_pass", leAuthPass_->text());
    if (!leAuthToken_->text().isEmpty()) d.metadata.insert("auth_token", leAuthToken_->text());
    return d;
}
