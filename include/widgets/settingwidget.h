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
    SettingWidget(QWidget *parent = nullptr);
    ~SettingWidget();

   signals:
    void backToMain();

   private slots:
    void on_btnBackFromSetting_clicked();

   private:
    Ui::SettingWidget *ui;
};

#endif  // SETTINGWIDGET_H