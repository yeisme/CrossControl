#pragma once

#include <QWidget>

class QPushButton;

class ImportExportControl : public QWidget {
    Q_OBJECT
   public:
    explicit ImportExportControl(QWidget* parent = nullptr);

   signals:
    void importRequested(const QString& method);
    void exportRequested(const QString& method);

   private slots:
    /**
     * @brief Slot for handling import button click.
     *
     */
    void onImportClicked();
    /**
     * @brief Slot for handling export button click.
     *
     */
    void onExportClicked();

   private:
    QPushButton* btnImport_ = nullptr;
    QPushButton* btnExport_ = nullptr;
};
