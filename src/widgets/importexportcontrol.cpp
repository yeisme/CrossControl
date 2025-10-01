#include "widgets/importexportcontrol.h"

#include <qcoreapplication.h>

#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

ImportExportControl::ImportExportControl(QWidget* parent) : QWidget(parent) {
    auto* v = new QVBoxLayout(this);
    btnImport_ = new QPushButton(tr("Import..."), this);
    btnExport_ = new QPushButton(tr("Export..."), this);
    v->addWidget(btnImport_);
    v->addWidget(btnExport_);
    v->setContentsMargins(0, 0, 0, 0);

    connect(btnImport_, &QPushButton::clicked, this, &ImportExportControl::onImportClicked);
    connect(btnExport_, &QPushButton::clicked, this, &ImportExportControl::onExportClicked);
}

void ImportExportControl::onImportClicked() {
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Import devices"));
    auto* layout = new QVBoxLayout(&dlg);
    layout->addWidget(new QLabel(tr("Choose import method:"), &dlg));

    auto* list = new QListWidget(&dlg);
    list->addItem(tr("Import from SQLite DB"));
    list->addItem(tr("Import from CSV"));
    list->addItem(tr("Import from JSONND"));
    list->setCurrentRow(0);
    layout->addWidget(list);

    auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    layout->addWidget(bb);
    connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() == QDialog::Accepted) {
        const auto txt = list->currentItem() ? list->currentItem()->text() : QString();
        if (!txt.isEmpty()) emit importRequested(txt);
    }
}

void ImportExportControl::onExportClicked() {
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Export devices"));
    auto* layout = new QVBoxLayout(&dlg);
    layout->addWidget(new QLabel(tr("Choose export method:"), &dlg));

    auto* list = new QListWidget(&dlg);
    list->addItem(tr("Export to SQLite DB"));
    list->addItem(tr("Export to CSV"));
    list->addItem(tr("Export to JSONND"));
    list->setCurrentRow(0);
    layout->addWidget(list);

    auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    layout->addWidget(bb);
    connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() == QDialog::Accepted) {
        const auto txt = list->currentItem() ? list->currentItem()->text() : QString();
        if (!txt.isEmpty()) emit exportRequested(txt);
    }
}
