#include "widgets/deviceeditdialog.h"

#include <qcoreapplication.h>

#include <QFormLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>

DeviceEditDialog::DeviceEditDialog(QWidget* parent) : QDialog(parent) {
    auto* form = new QFormLayout(this);
    leId_ = new QLineEdit(this);
    leName_ = new QLineEdit(this);
    leStatus_ = new QLineEdit(this);
    leEndpoint_ = new QLineEdit(this);
    leType_ = new QLineEdit(this);
    leHw_ = new QLineEdit(this);
    leFw_ = new QLineEdit(this);
    leOwner_ = new QLineEdit(this);
    leGroup_ = new QLineEdit(this);
    teMeta_ = new QTextEdit(this);

    form->addRow(QCoreApplication::translate("DeviceEditDialog", "ID"), leId_);
    form->addRow(QCoreApplication::translate("DeviceEditDialog", "Name"), leName_);
    form->addRow(QCoreApplication::translate("DeviceEditDialog", "Status"), leStatus_);
    form->addRow(QCoreApplication::translate("DeviceEditDialog", "Endpoint"), leEndpoint_);
    form->addRow(QCoreApplication::translate("DeviceEditDialog", "Type"), leType_);
    form->addRow(QCoreApplication::translate("DeviceEditDialog", "HW Info"), leHw_);
    form->addRow(QCoreApplication::translate("DeviceEditDialog", "Firmware"), leFw_);
    form->addRow(QCoreApplication::translate("DeviceEditDialog", "Owner"), leOwner_);
    form->addRow(QCoreApplication::translate("DeviceEditDialog", "Group"), leGroup_);
    form->addRow(QCoreApplication::translate("DeviceEditDialog", "Metadata (JSON)"), teMeta_);

    auto* btnBox = new QHBoxLayout();
    auto* ok = new QPushButton(QCoreApplication::translate("DeviceEditDialog", "OK"), this);
    auto* cancel = new QPushButton(QCoreApplication::translate("DeviceEditDialog", "Cancel"), this);
    btnBox->addWidget(ok);
    btnBox->addWidget(cancel);
    form->addRow(btnBox);

    connect(ok, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
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
    return d;
}
