#include "keyvalueeditor.h"

#include <QFile>
#include <QFileDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>

#include "ui_keyvalueeditor.h"

KeyValueEditor::KeyValueEditor(QWidget* parent) : QDialog(parent), ui(new Ui::KeyValueEditor) {
    ui->setupUi(this);
    ui->table->setColumnCount(2);
    ui->table->setHorizontalHeaderLabels({QCoreApplication::translate("KeyValueEditor", "Key"),
                                          QCoreApplication::translate("KeyValueEditor", "Value")});
    // Use auto-connect by naming convention for on_btn..._clicked slots; keep explicit close
    connect(ui->btnClose, &QPushButton::clicked, this, &KeyValueEditor::close);
}

KeyValueEditor::~KeyValueEditor() {
    delete ui;
}

void KeyValueEditor::on_btnAddRow_clicked() {
    int r = ui->table->rowCount();
    ui->table->insertRow(r);
}

void KeyValueEditor::on_btnRemoveRow_clicked() {
    int r = ui->table->currentRow();
    if (r >= 0) ui->table->removeRow(r);
}

QString KeyValueEditor::toJson() const {
    QJsonObject obj;
    for (int i = 0; i < ui->table->rowCount(); ++i) {
        auto kIt = ui->table->item(i, 0);
        auto vIt = ui->table->item(i, 1);
        if (!kIt) continue;
        QString k = kIt->text();
        QString v = vIt ? vIt->text() : QString();
        if (!k.isEmpty()) obj.insert(k, QJsonValue(v));
    }
    return QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

void KeyValueEditor::on_btnExportJson_clicked() {
    QString json = toJson();
    QString path = QFileDialog::getSaveFileName(
        this,
        QCoreApplication::translate("KeyValueEditor", "Save JSON"),
        QString(),
        QCoreApplication::translate("KeyValueEditor", "JSON files (*.json);;All files (*)"));
    if (path.isEmpty()) return;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this,
                             QCoreApplication::translate("KeyValueEditor", "Export"),
                             QCoreApplication::translate("KeyValueEditor", "Failed to write file"));
        return;
    }
    f.write(json.toUtf8());
    f.close();
}

void KeyValueEditor::on_btnInsert_clicked() {
    QString json = toJson();
    emit insertRequested(json);
}
