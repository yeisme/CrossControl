#ifndef KEYVALUEEDITOR_H
#define KEYVALUEEDITOR_H

#include <QDialog>

QT_BEGIN_NAMESPACE
namespace Ui { class KeyValueEditor; }
QT_END_NAMESPACE

class KeyValueEditor : public QDialog {
    Q_OBJECT
public:
    explicit KeyValueEditor(QWidget* parent = nullptr);
    ~KeyValueEditor() override;

    // Returns the table contents serialized as compact JSON (object of key->value)
    QString toJson() const;

signals:
    // emitted when Insert button pressed with json text
    void insertRequested(const QString& json);

private slots:
    void on_btnAddRow_clicked();
    void on_btnRemoveRow_clicked();
    void on_btnExportJson_clicked();
    void on_btnInsert_clicked();

private:
    Ui::KeyValueEditor* ui;
};

#endif // KEYVALUEEDITOR_H
