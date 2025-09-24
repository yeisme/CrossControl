// Reworked implementation: QWidget + left sidebar + right content stack
#include "crosscontrolwidget.h"

#include <QCoreApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

#include "logging.h"

CrossControlWidget::CrossControlWidget(QWidget* parent) : QWidget(parent) {
    LogM::instance().init();  // 初始化日志系统
    logging::useAsDefault("CrossControl");

    // 顶层：rootStack（登录页 / 应用主体）
    rootStack = new QStackedWidget(this);

    // 登录页
    loginWidget = new LoginWidget();
    rootStack->addWidget(loginWidget);

    // 应用主体：左侧边栏 + 右侧内容堆栈
    appPage = new QWidget(this);
    auto* appLayout = new QHBoxLayout(appPage);
    appLayout->setContentsMargins(0, 0, 0, 0);
    appLayout->setSpacing(0);

    // 左侧边栏
    sideBar = new QWidget(appPage);
    sideBar->setObjectName("sidebar");
    sideBar->setMinimumWidth(220);
    auto* sideLayout = new QVBoxLayout(sideBar);
    sideLayout->setContentsMargins(12, 12, 12, 12);
    sideLayout->setSpacing(8);

    // 顶部标题
    auto* lblTitle = new QLabel("CrossControl", sideBar);
    lblTitle->setStyleSheet("font-size: 18px; font-weight: 600; padding: 8px 4px;");
    sideLayout->addWidget(lblTitle);

    // 侧边栏按钮构造
    auto makeBtn = [](const QString& text) {
        auto* b = new QPushButton(text);
        b->setCheckable(false);
        b->setMinimumHeight(36);
        b->setStyleSheet("text-align: left; padding: 6px 10px; font-size: 14px;");
        return b;
    };

    btnDashboard = makeBtn(QCoreApplication::translate("CrossControlWidget", "Dashboard"));
    btnMonitor = makeBtn(QCoreApplication::translate("CrossControlWidget", "Monitor"));
    btnUnlock = makeBtn(QCoreApplication::translate("CrossControlWidget", "Remote Unlock"));
    btnVisitRecord = makeBtn(QCoreApplication::translate("CrossControlWidget", "Visit Records"));
    btnMessage = makeBtn(QCoreApplication::translate("CrossControlWidget", "Messages"));
    btnSetting = makeBtn(QCoreApplication::translate("CrossControlWidget", "System Settings"));
    btnLogs = makeBtn(QCoreApplication::translate("CrossControlWidget", "Logs"));

    for (auto* b :
         {btnDashboard, btnMonitor, btnUnlock, btnVisitRecord, btnMessage, btnSetting, btnLogs}) {
        sideLayout->addWidget(b);
    }

    sideLayout->addStretch(1);

    btnLogout = makeBtn(QCoreApplication::translate("CrossControlWidget", "Logout"));
    btnLogout->setStyleSheet(
        "text-align: left; padding: 6px 10px; font-size: 14px; color: #ff4444;");
    sideLayout->addWidget(btnLogout);

    // 右侧内容堆栈
    contentStack = new QStackedWidget(appPage);

    // 各页面
    mainWidget = new MainWidget();  // Dashboard 简化展示
    monitorWidget = new MonitorWidget();
    visitRecordWidget = new VisitRecordWidget();
    messageWidget = new MessageWidget();
    settingWidget = new SettingWidget();
    unlockWidget = new UnlockWidget();
    logWidget = new LogWidget();

    contentStack->addWidget(mainWidget);
    contentStack->addWidget(monitorWidget);
    contentStack->addWidget(visitRecordWidget);
    contentStack->addWidget(messageWidget);
    contentStack->addWidget(settingWidget);
    contentStack->addWidget(unlockWidget);
    contentStack->addWidget(logWidget);

    // 组装主体
    appLayout->addWidget(sideBar);
    appLayout->addWidget(contentStack, 1);

    rootStack->addWidget(appPage);

    // 最外层布局
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    outer->addWidget(rootStack);

    // 信号连接
    connect(loginWidget, &LoginWidget::loginSuccess, this, &CrossControlWidget::onLoginSuccess);

    // 侧边栏导航
    connect(btnDashboard, &QPushButton::clicked, this, &CrossControlWidget::showMainPage);
    connect(btnMonitor, &QPushButton::clicked, this, &CrossControlWidget::showMonitorPage);
    connect(btnUnlock, &QPushButton::clicked, this, &CrossControlWidget::showUnlockPage);
    connect(btnVisitRecord, &QPushButton::clicked, this, &CrossControlWidget::showVisitRecordPage);
    connect(btnMessage, &QPushButton::clicked, this, &CrossControlWidget::showMessagePage);
    connect(btnSetting, &QPushButton::clicked, this, &CrossControlWidget::showSettingPage);
    connect(btnLogs, &QPushButton::clicked, this, &CrossControlWidget::showLogPage);
    connect(btnLogout, &QPushButton::clicked, this, &CrossControlWidget::onLogout);

    // 子页面返回主页面
    connect(monitorWidget, &MonitorWidget::backToMain, this, &CrossControlWidget::showMainPage);
    connect(
        visitRecordWidget, &VisitRecordWidget::backToMain, this, &CrossControlWidget::showMainPage);
    connect(messageWidget, &MessageWidget::backToMain, this, &CrossControlWidget::showMainPage);
    connect(settingWidget, &SettingWidget::backToMain, this, &CrossControlWidget::showMainPage);
    connect(unlockWidget, &UnlockWidget::cancelUnlock, this, &CrossControlWidget::showMainPage);
    connect(logWidget, &LogWidget::backToMain, this, &CrossControlWidget::showMainPage);

    // 默认显示登录页
    rootStack->setCurrentWidget(loginWidget);
}

CrossControlWidget::~CrossControlWidget() = default;

void CrossControlWidget::onLoginSuccess() {
    rootStack->setCurrentWidget(appPage);
    showMainPage();
}

void CrossControlWidget::onLogout() { rootStack->setCurrentWidget(loginWidget); }

void CrossControlWidget::showMainPage() { contentStack->setCurrentWidget(mainWidget); }
void CrossControlWidget::showMonitorPage() { contentStack->setCurrentWidget(monitorWidget); }
void CrossControlWidget::showVisitRecordPage() {
    contentStack->setCurrentWidget(visitRecordWidget);
}
void CrossControlWidget::showMessagePage() { contentStack->setCurrentWidget(messageWidget); }
void CrossControlWidget::showSettingPage() { contentStack->setCurrentWidget(settingWidget); }
void CrossControlWidget::showUnlockPage() { contentStack->setCurrentWidget(unlockWidget); }
void CrossControlWidget::showLogPage() { contentStack->setCurrentWidget(logWidget); }
