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

    this->addWidget(loginWidget);
    this->addWidget(mainWidget);
    this->addWidget(monitorWidget);
    this->addWidget(visitRecordWidget);
    this->addWidget(messageWidget);
    this->addWidget(settingWidget);
    this->addWidget(unlockWidget);

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