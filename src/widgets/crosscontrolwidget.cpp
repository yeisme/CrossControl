#include "crosscontrolwidget.h"

#include "ui_crosscontrolwidget.h"

CrossControlWidget::CrossControlWidget(QWidget* parent)
    : QStackedWidget(parent), ui(new Ui::CrossControlWidget) {
    ui->setupUi(this);

    loginWidget = new LoginWidget();
    mainWidget = new MainWidget();
    monitorWidget = new MonitorWidget();
    visitRecordWidget = new VisitRecordWidget();
    messageWidget = new MessageWidget();
    settingWidget = new SettingWidget();
    unlockWidget = new UnlockWidget();
        logWidget = new LogWidget();

    this->addWidget(loginWidget);
    this->addWidget(mainWidget);
    this->addWidget(monitorWidget);
    this->addWidget(visitRecordWidget);
    this->addWidget(messageWidget);
    this->addWidget(settingWidget);
    this->addWidget(unlockWidget);
        this->addWidget(logWidget);

    // Connect signals 跳转到各个页面
    connect(loginWidget,
            &LoginWidget::loginSuccess,
            this,
            &CrossControlWidget::onLoginSuccess);
    connect(mainWidget,
            &MainWidget::showMonitor,
            this,
            &CrossControlWidget::showMonitorPage);
    connect(mainWidget,
            &MainWidget::showUnlock,
            this,
            &CrossControlWidget::showUnlockPage);
    connect(mainWidget,
            &MainWidget::showVisitRecord,
            this,
            &CrossControlWidget::showVisitRecordPage);
    connect(mainWidget,
            &MainWidget::showMessage,
            this,
            &CrossControlWidget::showMessagePage);
    connect(mainWidget,
            &MainWidget::showSetting,
            this,
            &CrossControlWidget::showSettingPage);
    connect(mainWidget,
            &MainWidget::showLogs,
            this,
            &CrossControlWidget::showLogPage);
    connect(
        mainWidget, &MainWidget::logout, this, &CrossControlWidget::onLogout);

    // Back buttons 返回到主页面
    connect(monitorWidget,
            &MonitorWidget::backToMain,
            this,
            &CrossControlWidget::showMainPage);
    connect(visitRecordWidget,
            &VisitRecordWidget::backToMain,
            this,
            &CrossControlWidget::showMainPage);
    connect(messageWidget,
            &MessageWidget::backToMain,
            this,
            &CrossControlWidget::showMainPage);
    connect(settingWidget,
            &SettingWidget::backToMain,
            this,
            &CrossControlWidget::showMainPage);
    connect(unlockWidget,
            &UnlockWidget::cancelUnlock,
            this,
            &CrossControlWidget::showMainPage);
    connect(logWidget,
            &LogWidget::backToMain,
            this,
            &CrossControlWidget::showMainPage);
}

CrossControlWidget::~CrossControlWidget() {
    delete ui;
    delete loginWidget;
    delete mainWidget;
    delete monitorWidget;
    delete visitRecordWidget;
    delete messageWidget;
    delete settingWidget;
    delete unlockWidget;
        delete logWidget;
}

void CrossControlWidget::onLoginSuccess() { setCurrentWidget(mainWidget); }

void CrossControlWidget::onLogout() { setCurrentWidget(loginWidget); }

void CrossControlWidget::showMainPage() { setCurrentWidget(mainWidget); }

void CrossControlWidget::showMonitorPage() { setCurrentWidget(monitorWidget); }

void CrossControlWidget::showVisitRecordPage() {
    setCurrentWidget(visitRecordWidget);
}

void CrossControlWidget::showMessagePage() { setCurrentWidget(messageWidget); }

void CrossControlWidget::showSettingPage() { setCurrentWidget(settingWidget); }

void CrossControlWidget::showUnlockPage() { setCurrentWidget(unlockWidget); }

void CrossControlWidget::showLogPage() { setCurrentWidget(logWidget); }