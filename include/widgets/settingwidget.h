#ifndef SETTINGWIDGET_H
#define SETTINGWIDGET_H

#include <QDir>
#include <QFileDialog>
#include <QListWidget>
#include <QTextEdit>
#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui {
class SettingWidget;
}
QT_END_NAMESPACE

class SettingWidget : public QWidget {
    Q_OBJECT

   public:
    SettingWidget(QWidget* parent = nullptr);
    ~SettingWidget();

   signals:
    void backToMain();
    void toggleThemeRequested();  // 请求切换主题

   public slots:
    void on_btnReloadSettings_clicked();
    void on_btnSaveSettings_clicked();

    // New slots for enhanced settings
    void on_btnConfigureStorage_clicked();
    void on_btnManageAccounts_clicked();
    void on_btnJsonEditor_clicked();

   private slots:
    void on_btnBackFromSetting_clicked();
    void on_btnToggleTheme_clicked();

   private:
    QWidget* addSettingRow(const QString& title, QWidget* control);

   private:
    Ui::SettingWidget* ui;
    void populateSettingsTable();
};

#endif  // SETTINGWIDGET_H