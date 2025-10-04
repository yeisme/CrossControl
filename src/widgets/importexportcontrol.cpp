#include "importexportcontrol.h"

#include <QCoreApplication>
#include <QHBoxLayout>
#include <QPushButton>

ImportExportControl::ImportExportControl(QWidget* parent) : QWidget(parent) {
    btnImport_ =
        new QPushButton(QCoreApplication::translate("ImportExportControl", "Import"), this);
    btnExport_ =
        new QPushButton(QCoreApplication::translate("ImportExportControl", "Export"), this);
    auto* lay = new QHBoxLayout(this);
    lay->addWidget(btnImport_);
    lay->addWidget(btnExport_);

    connect(btnImport_, &QPushButton::clicked, this, &ImportExportControl::onImportClicked);
    connect(btnExport_, &QPushButton::clicked, this, &ImportExportControl::onExportClicked);
}

void ImportExportControl::onImportClicked() {
    emit importRequested(QStringLiteral("default"));
}
void ImportExportControl::onExportClicked() {
    emit exportRequested(QStringLiteral("default"));
}
