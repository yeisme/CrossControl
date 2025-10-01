// Reworked implementation: QWidget + left sidebar + right content stack
#include "crosscontrolwidget.h"

#include <QApplication>
#include <QCoreApplication>
#include <QFile>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QStackedWidget>
#include <QStyle>
#include <QVBoxLayout>

#include "facerecognitionwidget.h"
#include "logging.h"
#include "modules/Config/config.h"
#include "widgets/devicemanagementwidget.h"
#include "spdlog/spdlog.h"

// Default constructor delegates to gateway-taking constructor with nullptr
CrossControlWidget::CrossControlWidget(QWidget* parent) : CrossControlWidget(nullptr, parent) {}

CrossControlWidget::CrossControlWidget(DeviceGateway::DeviceGateway* gateway, QWidget* parent)
    : QWidget(parent), deviceGateway(gateway) {
    LogM::instance().init();  // 初始化日志系统
    logging::useAsDefault("CrossControl");
    spdlog::debug("CrossControlWidget constructed");

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
    auto makeBtn = [](const QString& text, const QString& iconPath, const QString& name) {
        auto* b = new QPushButton(text);
        b->setObjectName(name);
        b->setCheckable(true);
        b->setMinimumHeight(38);
        b->setCursor(Qt::PointingHandCursor);
        b->setStyleSheet("text-align: left; padding: 6px 14px; font-size: 14px;");
        if (!iconPath.isEmpty()) {
            QIcon icon(iconPath);
            b->setIcon(icon);
            b->setIconSize(QSize(18, 18));
        }
        return b;
    };

    btnDashboard = makeBtn(QCoreApplication::translate("CrossControlWidget", "Dashboard"),
                           ":/icons/icons/dashboard.svg",
                           "btnDashboard");
    btnMonitor = makeBtn(QCoreApplication::translate("CrossControlWidget", "Monitor"),
                         ":/icons/icons/monitor.svg",
                         "btnMonitor");
    btnUnlock = makeBtn(QCoreApplication::translate("CrossControlWidget", "Remote Unlock"),
                        ":/icons/icons/unlock.svg",
                        "btnUnlock");
    btnVisitRecord = makeBtn(QCoreApplication::translate("CrossControlWidget", "Visit Records"),
                             ":/icons/icons/visit.svg",
                             "btnVisitRecord");
    btnMessage = makeBtn(QCoreApplication::translate("CrossControlWidget", "Messages"),
                         ":/icons/icons/message.svg",
                         "btnMessage");
    btnSetting = makeBtn(QCoreApplication::translate("CrossControlWidget", "System Settings"),
                         ":/icons/icons/setting.svg",
                         "btnSetting");
    btnLogs = makeBtn(QCoreApplication::translate("CrossControlWidget", "Logs"),
                      ":/icons/icons/logs.svg",
                      "btnLogs");
    btnFaceRec = makeBtn(QCoreApplication::translate("CrossControlWidget", "Face Recognition"),
                         ":/icons/icons/app.svg",
                         "btnFaceRec");
    btnDeviceMgmt = makeBtn(QCoreApplication::translate("CrossControlWidget", "Device Management"),
                            ":/icons/icons/device.svg",
                            "btnDeviceMgmt");

    for (QPushButton* b : {btnDashboard,
                    btnMonitor,
                    btnUnlock,
                    btnVisitRecord,
                    btnMessage,
                    btnSetting,
                    btnLogs,
                    btnFaceRec,
                    btnDeviceMgmt}) {
        sideLayout->addWidget(b);
    }

    sideLayout->addStretch(1);

    btnLogout = makeBtn(QCoreApplication::translate("CrossControlWidget", "Logout"),
                        ":/icons/icons/logout.svg",
                        "btnLogout");
    btnLogout->setCheckable(false);
    btnLogout->setProperty("destructive", true);
    btnLogout->setStyleSheet(
        "text-align: left; padding: 6px 10px; font-size: 14px; color: #ff7777;");
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
    faceRecWidget = new FaceRecognitionWidget();
    // Device management page
    deviceMgmtWidget = new DeviceManagementWidget(deviceGateway);

    contentStack->addWidget(mainWidget);
    contentStack->addWidget(monitorWidget);
    contentStack->addWidget(visitRecordWidget);
    contentStack->addWidget(messageWidget);
    contentStack->addWidget(settingWidget);
    contentStack->addWidget(unlockWidget);
    contentStack->addWidget(logWidget);
    contentStack->addWidget(faceRecWidget);
    contentStack->addWidget(deviceMgmtWidget);

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
    auto setActive = [this](QPushButton* active) {
        for (auto* b : {btnDashboard,
                        btnMonitor,
                        btnUnlock,
                        btnVisitRecord,
                        btnMessage,
                        btnSetting,
                        btnLogs,
                        btnFaceRec}) {
            if (!b) continue;
            b->setProperty("active", b == active ? "true" : "false");
            b->style()->unpolish(b);
            b->style()->polish(b);
        }
    };

    connect(btnDashboard, &QPushButton::clicked, this, [this, setActive]() {
        setActive(btnDashboard);
        showMainPage();
    });
    connect(btnMonitor, &QPushButton::clicked, this, [this, setActive]() {
        setActive(btnMonitor);
        showMonitorPage();
    });
    connect(btnUnlock, &QPushButton::clicked, this, [this, setActive]() {
        setActive(btnUnlock);
        showUnlockPage();
    });
    connect(btnVisitRecord, &QPushButton::clicked, this, [this, setActive]() {
        setActive(btnVisitRecord);
        showVisitRecordPage();
    });
    connect(btnMessage, &QPushButton::clicked, this, [this, setActive]() {
        setActive(btnMessage);
        showMessagePage();
    });
    connect(btnSetting, &QPushButton::clicked, this, [this, setActive]() {
        setActive(btnSetting);
        showSettingPage();
    });
    connect(btnLogs, &QPushButton::clicked, this, [this, setActive]() {
        setActive(btnLogs);
        showLogPage();
    });
    connect(btnFaceRec, &QPushButton::clicked, this, [this, setActive]() {
        setActive(btnFaceRec);
        contentStack->setCurrentWidget(faceRecWidget);
    });
    connect(btnDeviceMgmt, &QPushButton::clicked, this, [this, setActive]() {
        setActive(btnDeviceMgmt);
        contentStack->setCurrentWidget(deviceMgmtWidget);
    });
    connect(btnLogout, &QPushButton::clicked, this, &CrossControlWidget::onLogout);

    // 子页面返回主页面
    connect(monitorWidget, &MonitorWidget::backToMain, this, &CrossControlWidget::showMainPage);
    connect(
        visitRecordWidget, &VisitRecordWidget::backToMain, this, &CrossControlWidget::showMainPage);
    connect(messageWidget, &MessageWidget::backToMain, this, &CrossControlWidget::showMainPage);
    connect(settingWidget, &SettingWidget::backToMain, this, &CrossControlWidget::showMainPage);
    connect(settingWidget, &SettingWidget::toggleThemeRequested, this, [this]() { toggleTheme(); });
    connect(unlockWidget, &UnlockWidget::cancelUnlock, this, &CrossControlWidget::showMainPage);
    connect(logWidget, &LogWidget::backToMain, this, &CrossControlWidget::showMainPage);
    connect(
        faceRecWidget, &FaceRecognitionWidget::backToMain, this, &CrossControlWidget::showMainPage);

    // 默认显示登录页
    rootStack->setCurrentWidget(loginWidget);
}

CrossControlWidget::~CrossControlWidget() = default;

void CrossControlWidget::onLoginSuccess() {
    rootStack->setCurrentWidget(appPage);
    showMainPage();
    // 默认激活 Dashboard
    QMetaObject::invokeMethod(btnDashboard, "click", Qt::QueuedConnection);
}

void CrossControlWidget::onLogout() {
    rootStack->setCurrentWidget(loginWidget);
}

void CrossControlWidget::showMainPage() {
    contentStack->setCurrentWidget(mainWidget);
}
void CrossControlWidget::showMonitorPage() {
    contentStack->setCurrentWidget(monitorWidget);
}
void CrossControlWidget::showVisitRecordPage() {
    contentStack->setCurrentWidget(visitRecordWidget);
}
void CrossControlWidget::showMessagePage() {
    contentStack->setCurrentWidget(messageWidget);
}
void CrossControlWidget::showSettingPage() {
    contentStack->setCurrentWidget(settingWidget);
}
void CrossControlWidget::showUnlockPage() {
    contentStack->setCurrentWidget(unlockWidget);
}
void CrossControlWidget::showLogPage() {
    contentStack->setCurrentWidget(logWidget);
}

// 主题管理：缓存当前主题（应用启动在 main.cpp 中默认加载 light）
void CrossControlWidget::toggleTheme() {
    static bool dark = false;
    dark = !dark;
    const QString path = dark ? ":/themes/themes/dark.qss" : ":/themes/themes/light.qss";
    { config::ConfigManager::instance().setValue("Preferences/theme", dark ? "dark" : "light"); }
    QFile f(path);
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (auto* core = QApplication::instance()) {
            auto* app = static_cast<QApplication*>(core);
            app->setStyleSheet(QString::fromUtf8(f.readAll()));
        }
    }
    // 刷新登录界面上的密码可见性图标（重新触发构造逻辑比较复杂，这里简单地通过 focus/clear palette
    // 方式强制重绘）
    if (loginWidget) {
        for (auto* le : loginWidget->findChildren<QLineEdit*>()) {
            if (le->echoMode() == QLineEdit::Password || le->echoMode() == QLineEdit::Normal) {
                // 通过改变字体再复原触发 repaint
                auto fnt = le->font();
                fnt.setPointSize(fnt.pointSize());
                le->setFont(fnt);
                le->update();
            }
        }
        // 如果当前是浅色主题（dark == false），修正那些在 UI 设计器中被硬编码为白色的标签文字颜色
        if (!dark) {
            for (auto* lbl : loginWidget->findChildren<QLabel*>()) {
                auto pal = lbl->palette();
                const QColor current = pal.color(QPalette::WindowText);
                // 若亮度非常高(>220) 且接近白色，则替换为深色可读文本
                if (current.lightness() > 220) {
                    pal.setColor(QPalette::WindowText, QColor("#1e293b"));
                    lbl->setPalette(pal);
                }
            }
        }
    }
}
