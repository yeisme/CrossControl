#include "actionmanager.h"

#include <spdlog/spdlog.h>

#include <QFile>
#include <QFileDialog>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTextEdit>

#include "modules/Storage/storage.h"
#include "ui_actionmanager.h"

ActionManager::ActionManager(const QString& type, QWidget* parent)
    : QDialog(parent), ui(new Ui::ActionManager), m_type(type) {
    ui->setupUi(this);
    // Show the requested type in the UI so the dialog reflects the protocol (fix default HTTP
    // label)
    ui->labelTypeValue->setText(m_type.toUpper());
    this->setWindowTitle(
        QCoreApplication::translate("ActionManager", "Action Manager - %1").arg(m_type.toUpper()));
    spdlog::info("ActionManager constructed for type {}", m_type.toStdString());
    spdlog::default_logger()->flush();
    refreshList();
    ui->btnLoad->setEnabled(false);
    ui->btnDelete->setEnabled(false);
    ui->btnRename->setEnabled(false);
    ui->btnExport->setEnabled(false);
}

ActionManager::~ActionManager() {
    delete ui;
}

QString ActionManager::selectedName() const {
    int r = ui->listActions->currentRow();
    if (r < 0 || r >= m_rows.size()) return QString();
    return m_rows.at(r).first;
}

void ActionManager::refreshList() {
    using namespace storage;
    m_rows = Storage::loadActions(m_type);
    ui->listActions->clear();
    for (const auto& p : m_rows) {
        ui->listActions->addItem(QString("%1 - %2").arg(p.first, p.second));
    }
    spdlog::info("ActionManager: refreshList found {} rows for type {}",
                 m_rows.size(),
                 m_type.toStdString());
    spdlog::default_logger()->flush();
}

QByteArray ActionManager::currentPayload() const {
    QString name = selectedName();
    if (name.isEmpty()) return QByteArray();
    return storage::Storage::loadActionPayload(m_type, name);
}

void ActionManager::on_btnRefresh_clicked() {
    refreshList();
}

void ActionManager::on_btnLoad_clicked() {
    QString name = selectedName();
    if (name.isEmpty()) return;
    QByteArray payload = currentPayload();
    if (payload.isEmpty()) {
        QMessageBox::warning(
            this,
            QCoreApplication::translate("SerialOpsWidget", "Load"),
            QCoreApplication::translate("SerialOpsWidget", "Failed to load payload"));
        return;
    }
    spdlog::info(
        "ActionManager: loading action '{}' for type {}", name.toStdString(), m_type.toStdString());
    spdlog::default_logger()->flush();
    emit actionLoaded(name, payload);
    accept();
}

void ActionManager::on_btnDelete_clicked() {
    QString name = selectedName();
    if (name.isEmpty()) return;
    int ok = QMessageBox::question(
        this,
        QCoreApplication::translate("SerialOpsWidget", "Delete"),
        QCoreApplication::translate("SerialOpsWidget", "Delete action '%1'?").arg(name));
    if (ok != QMessageBox::Yes) return;
    using namespace storage;
    if (!Storage::deleteAction(m_type, name)) {
        QMessageBox::warning(
            this,
            QCoreApplication::translate("SerialOpsWidget", "Delete"),
            QCoreApplication::translate("SerialOpsWidget", "Failed to delete action '%1'")
                .arg(name));
        spdlog::warn("ActionManager: failed to delete action '{}' type {}",
                     name.toStdString(),
                     m_type.toStdString());
    } else {
        spdlog::info(
            "ActionManager: deleted action '{}' type {}", name.toStdString(), m_type.toStdString());
    }
    spdlog::default_logger()->flush();
    refreshList();
}

void ActionManager::on_btnRename_clicked() {
    QString name = selectedName();
    if (name.isEmpty()) return;
    bool ok;
    QString newName =
        QInputDialog::getText(this,
                              QCoreApplication::translate("SerialOpsWidget", "Rename"),
                              QCoreApplication::translate("SerialOpsWidget", "New name:"),
                              QLineEdit::Normal,
                              name,
                              &ok);
    if (!ok || newName.isEmpty() || newName == name) return;
    QByteArray payload = currentPayload();
    if (payload.isEmpty()) {
        QMessageBox::warning(
            this,
            QCoreApplication::translate("SerialOpsWidget", "Rename"),
            QCoreApplication::translate("SerialOpsWidget", "Failed to load payload"));
        return;
    }
    using namespace storage;
    if (!Storage::renameAction(m_type, name, newName)) {
        QMessageBox::warning(
            this,
            QCoreApplication::translate("SerialOpsWidget", "Rename"),
            QCoreApplication::translate("SerialOpsWidget", "Failed to rename action"));
        return;
    }
    spdlog::info("ActionManager: renamed action '{}' -> '{}' for type {}",
                 name.toStdString(),
                 newName.toStdString(),
                 m_type.toStdString());
    spdlog::default_logger()->flush();
    refreshList();
}

void ActionManager::on_btnExport_clicked() {
    QString name = selectedName();
    if (name.isEmpty()) return;
    QByteArray payload = currentPayload();
    if (payload.isEmpty()) {
        QMessageBox::warning(
            this,
            QCoreApplication::translate("SerialOpsWidget", "Export"),
            QCoreApplication::translate("SerialOpsWidget", "Failed to load payload"));
        return;
    }
    QString fn = QFileDialog::getSaveFileName(
        this,
        QCoreApplication::translate("SerialOpsWidget", "Export Action"),
        name + ".json",
        QCoreApplication::translate("SerialOpsWidget", "JSON files (*.json);;All files (*)"));
    if (fn.isEmpty()) return;
    QFile f(fn);
    if (!f.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(
            this,
            QCoreApplication::translate("SerialOpsWidget", "Export"),
            QCoreApplication::translate("SerialOpsWidget", "Failed to open file for write"));
        return;
    }
    f.write(payload);
    f.close();
}

void ActionManager::on_btnImport_clicked() {
    QString fn = QFileDialog::getOpenFileName(
        this,
        QCoreApplication::translate("SerialOpsWidget", "Import Action"),
        QString(),
        QCoreApplication::translate("SerialOpsWidget", "JSON files (*.json);;All files (*)"));
    if (fn.isEmpty()) return;
    QFile f(fn);
    if (!f.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this,
                             QCoreApplication::translate("SerialOpsWidget", "Import"),
                             QCoreApplication::translate("SerialOpsWidget", "Failed to open file"));
        return;
    }
    QByteArray payload = f.readAll();
    bool ok;
    QString name =
        QInputDialog::getText(this,
                              QCoreApplication::translate("SerialOpsWidget", "Import Action"),
                              QCoreApplication::translate("SerialOpsWidget", "Action name:"),
                              QLineEdit::Normal,
                              QString(),
                              &ok);
    if (!ok || name.isEmpty()) return;
    using namespace storage;
    if (!Storage::saveAction(m_type, name, payload)) {
        QMessageBox::warning(
            this,
            QCoreApplication::translate("SerialOpsWidget", "Import"),
            QCoreApplication::translate("SerialOpsWidget", "Failed to save imported action"));
        return;
    }
    spdlog::info(
        "ActionManager: imported action '{}' type {}", name.toStdString(), m_type.toStdString());
    spdlog::default_logger()->flush();
    refreshList();
}

void ActionManager::on_listActions_currentRowChanged(int row) {
    Q_UNUSED(row)
    QString name = selectedName();
    bool ok = !name.isEmpty();
    ui->btnLoad->setEnabled(ok);
    ui->btnDelete->setEnabled(ok);
    ui->btnRename->setEnabled(ok);
    ui->btnExport->setEnabled(ok);
    // preview payload in text box
    if (ok) {
        QByteArray payload = currentPayload();
        ui->tePreview->setPlainText(QString::fromUtf8(payload));
    } else {
        ui->tePreview->clear();
    }
}

void ActionManager::on_btnMigrate_clicked() {
    using namespace storage;
    int migrated = Storage::migrateOldActions();
    QMessageBox::information(
        this,
        QCoreApplication::translate("SerialOpsWidget", "Migrate"),
        QCoreApplication::translate("SerialOpsWidget", "Migrated %1 actions").arg(migrated));
    spdlog::info("ActionManager: migrateOldActions migrated {} rows", migrated);
    spdlog::default_logger()->flush();
    refreshList();
}
