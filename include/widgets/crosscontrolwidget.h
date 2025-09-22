#ifndef CROSSCONTROLWIDGET_H
#define CROSSCONTROLWIDGET_H

#include <QStackedWidget>

#include "loginwidget.h"
#include "mainwidget.h"
#include "monitorwidget.h"
#include "visitrecordwidget.h"
#include "messagewidget.h"
#include "settingwidget.h"
#include "unlockwidget.h"

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

    LoginWidget *loginWidget;
    MainWidget *mainWidget;
    MonitorWidget *monitorWidget;
    VisitRecordWidget *visitRecordWidget;
    MessageWidget *messageWidget;
    SettingWidget *settingWidget;
    UnlockWidget *unlockWidget;

   private slots:
    void onLoginSuccess();
    void onLogout();
    void showMainPage();
    void showMonitorPage();
    void showVisitRecordPage();
    void showMessagePage();
    void showSettingPage();
    void showUnlockPage();
};

#endif  // CROSSCONTROLWIDGET_H