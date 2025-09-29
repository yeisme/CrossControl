#ifndef CROSSCONTROLWIDGET_H
#define CROSSCONTROLWIDGET_H

#include <QWidget>

class QStackedWidget;
class QPushButton;

#include "loginwidget.h"
#include "logwidget.h"
#include "mainwidget.h"
#include "messagewidget.h"
#include "monitorwidget.h"
#include "settingwidget.h"
#include "unlockwidget.h"
#include "visitrecordwidget.h"
class FaceRecognitionWidget;

class CrossControlWidget : public QWidget {
    Q_OBJECT

   public:
    explicit CrossControlWidget(QWidget* parent = nullptr);
    ~CrossControlWidget() override;

   private:
    // 顶层堆栈：登录页 / 应用主体
    QStackedWidget* rootStack{nullptr};

    // 应用主体：左侧边栏 + 右侧内容堆栈
    QWidget* appPage{nullptr};
    QWidget* sideBar{nullptr};
    QStackedWidget* contentStack{nullptr};

    // 侧边栏按钮
    QPushButton* btnDashboard{nullptr};
    QPushButton* btnMonitor{nullptr};
    QPushButton* btnUnlock{nullptr};
    QPushButton* btnVisitRecord{nullptr};
    QPushButton* btnMessage{nullptr};
    QPushButton* btnSetting{nullptr};
    QPushButton* btnLogs{nullptr};
    QPushButton* btnFaceRec{nullptr};
    QPushButton* btnLogout{nullptr};

    // 各页面
    LoginWidget* loginWidget{nullptr};              // 登录页面
    MainWidget* mainWidget{nullptr};                // Dashboard 主页面
    MonitorWidget* monitorWidget{nullptr};          // 监控页面
    VisitRecordWidget* visitRecordWidget{nullptr};  // 访客记录
    MessageWidget* messageWidget{nullptr};          // 消息
    SettingWidget* settingWidget{nullptr};          // 设置
    UnlockWidget* unlockWidget{nullptr};            // 解锁
    LogWidget* logWidget{nullptr};                  // 日志
    FaceRecognitionWidget* faceRecWidget{nullptr};  // 人脸识别页面

   public:
    // 供外部（例如 main.cpp）访问日志页面以完成全局绑定
    LogWidget* getLogWidget() const { return logWidget; }

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
    void toggleTheme();
};

#endif  // CROSSCONTROLWIDGET_H