#ifndef MAINWIDGET_H
#define MAINWIDGET_H

#include <QWidget>

#include "weatherwidget.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWidget;
}
QT_END_NAMESPACE

class MainWidget : public QWidget {
    Q_OBJECT

   public:
    MainWidget(QWidget *parent = nullptr);
    ~MainWidget();

   signals:
    void showMonitor();
    void showUnlock();
    void showVisitRecord();
    void showMessage();
    void showSetting();
    void showLogs();
    void logout();

   private slots:
    void on_btnMonitor_clicked();
    void on_btnUnlock_clicked();
    void on_btnVisitRecord_clicked();
    void on_btnMessage_clicked();
    void on_btnSetting_clicked();
    void on_btnLogs_clicked();
    void on_btnLogout_clicked();

   private:
    Ui::MainWidget *ui;
    WeatherWidget *weatherWidget;  // 新增天气组件
};

#endif  // MAINWIDGET_H