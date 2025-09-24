#ifndef CROSSCONTROLWIDGET_H
#define CROSSCONTROLWIDGET_H

#include <QStackedWidget>

#include "loginwidget.h"
#include "mainwidget.h"
#include "messagewidget.h"
#include "monitorwidget.h"
#include "settingwidget.h"
#include "unlockwidget.h"
#include "visitrecordwidget.h"
#include "logwidget.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class CrossControlWidget;
}
QT_END_NAMESPACE

class CrossControlWidget : public QStackedWidget {
    Q_OBJECT

   public:
    CrossControlWidget(QWidget *parent = nullptr);
    ~CrossControlWidget();

   private:
    Ui::CrossControlWidget *ui;

    LoginWidget *loginWidget;              // 登录界面
    MainWidget *mainWidget;                // 主界面
    MonitorWidget *monitorWidget;          // 监控界面
    VisitRecordWidget *visitRecordWidget;  // 访客记录界面
    MessageWidget *messageWidget;          // 消息界面
    SettingWidget *settingWidget;          // 设置界面
    UnlockWidget *unlockWidget;            // 解锁界面
    LogWidget *logWidget;                  // 日志界面（新）

   private slots:
    void onLoginSuccess();
    void onLogout();
    void showMainPage();
    void showMonitorPage();
    void showVisitRecordPage();
    void showMessagePage();
    void showSettingPage();
    void showUnlockPage();
    void showLogPage();
};

#endif  // CROSSCONTROLWIDGET_H