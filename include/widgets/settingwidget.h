#ifndef SETTINGWIDGET_H
#define SETTINGWIDGET_H

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

   private slots:
    void on_btnBackFromSetting_clicked();
    void on_btnToggleTheme_clicked();

   private:
    QWidget* addSettingRow(const QString& title, QWidget* control);

   private:
    Ui::SettingWidget* ui;
};

#endif  // SETTINGWIDGET_H