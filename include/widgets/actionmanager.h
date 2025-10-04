#pragma once

#include <QDialog>
#include <QPair>
#include <QVector>

namespace Ui { class ActionManager; }

class ActionManager : public QDialog {
    Q_OBJECT
   public:
    explicit ActionManager(const QString& type, QWidget* parent = nullptr);
    ~ActionManager() override;

    // selected name when user clicks Load
    QString selectedName() const;

   signals:
    void actionLoaded(const QString& name, const QByteArray& payload);

   private slots:
    void on_btnRefresh_clicked();
    void on_btnLoad_clicked();
    void on_btnDelete_clicked();
    void on_btnRename_clicked();
    void on_btnExport_clicked();
    void on_btnImport_clicked();
    void on_btnMigrate_clicked();
    void on_listActions_currentRowChanged(int row);

   private:
    Ui::ActionManager* ui;
    QString m_type;
    QVector<QPair<QString, QString>> m_rows; // name, created_at
    void refreshList();
    QByteArray currentPayload() const;
};
