#include <QApplication>
#include <QColor>
#include <QFile>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLocale>
#include <QMessageLogContext>
#include <QPalette>
#include <QPushButton>
#include <QSplitter>
#include <QTextEdit>
#include <QTranslator>
#include <QVBoxLayout>
#include <QWidget>
#include <memory>

#include "crosscontrolwidget.h"
#include "logging.h"
#include "modules/Config/config.h"
#include "modules/DeviceGateway/device_gateway.h"
#include "spdlog/spdlog.h"

// 如果系统中存在 trantor/drogon 头，则把 drogon/trantor 的日志转发到项目的 spdlog 封装。
#if defined(__has_include)
#if __has_include(<trantor/utils/Logger.h>) && __has_include(<drogon/drogon.h>)
#include <drogon/drogon.h>
#include <trantor/utils/Logger.h>
#define CROSSCONTROL_HAS_DROGON 1
#else
#define CROSSCONTROL_HAS_DROGON 0
#endif
#else
#define CROSSCONTROL_HAS_DROGON 0
#endif

// 为当前系统用户界面语言安装翻译器。如果加载并安装了翻译，则返回一个非空的 unique_ptr；否则返回
// nullptr。
static std::unique_ptr<QTranslator> installSystemTranslator(QCoreApplication& app) {
    auto translator = std::make_unique<QTranslator>();
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    bool translatorLoaded = false;
    for (const QString& locale : uiLanguages) {
        const QString baseName = "CrossControl_" + QLocale(locale).name();  // e.g. zh_CN

        // Candidate directories; resource and typical on-disk locations
        const QString appDir = QCoreApplication::applicationDirPath();
        const QStringList dirs = {
            ":/i18n", appDir + "/i18n", appDir + "/../i18n", appDir + "/../../src/i18n"};

        for (const QString& dir : dirs) {
            const QString path = dir + "/" + baseName + ".qm";
            spdlog::info("Trying to load translation: {}", path.toStdString());
            if (translator->load(path)) {
                app.installTranslator(translator.get());
                translatorLoaded = true;
                spdlog::info("Loaded translation: {}", path.toStdString());
                break;
            }
        }
        if (translatorLoaded) break;
    }
    if (!translatorLoaded) {
        spdlog::warn("No translation loaded; falling back to English source texts.");
        return nullptr;
    }
    return translator;
}

// Detect system theme (simple heuristic): if window background is dark -> dark, else light
static QString detectSystemTheme(QApplication& app) {
    const QPalette pal = app.palette();
    const QColor bg = pal.color(QPalette::Window);
    // perceived luminance
    const double lum = 0.2126 * bg.red() + 0.7152 * bg.green() + 0.0722 * bg.blue();
    return (lum < 128.0) ? "dark" : "light";
}

int main(int argc, char* argv[]) {
    QApplication a(argc, argv);

    // 保持翻译器在应用程序的生命周期内有效
    const auto translatorHolder = installSystemTranslator(a);

    // Ensure application locale is set so date/time and weekday names are localized correctly.
    // If the system locale is Chinese, enforce zh_CN as the default QLocale so UI strings like
    // weekday names appear in Chinese.
    const QString sysName = QLocale::system().name();
    if (sysName.startsWith("zh", Qt::CaseInsensitive)) {
        QLocale::setDefault(QLocale(QLocale::Chinese, QLocale::China));
    } else {
        QLocale::setDefault(QLocale::system());
    }

    // 初始化统一日志，并将 Qt 消息重定向到 spdlog
    logging::LoggerManager::instance().init();
    qInstallMessageHandler(logging::LoggerManager::qtMessageHandler);

#if CROSSCONTROL_HAS_DROGON
    // 创建或获取名为 "Drogon" 的 spdlog logger（由 LoggerManager 管理的 shared_ptr）
    auto drogon_logger_sp = logging::LoggerManager::instance().getLogger("Drogon");
    if (drogon_logger_sp) {
        // 让 trantor 直接使用 spdlog logger（trantor 提供的集成点）
        trantor::Logger::enableSpdLog(-1, drogon_logger_sp);
    } else {
        // 作为后备：如果没有获取到 shared_ptr（极少见），退回到 setOutputFunction
        trantor::Logger::setOutputFunction(
            [](const char* msg, const uint64_t len) {
                std::string s(msg, static_cast<size_t>(len));
                spdlog::get("Drogon")->info(s);
            },
            []() {},
            -1);
    }
    // 将 drogon 的默认日志级别设置为 info（可根据需要调整）
    trantor::Logger::setLogLevel(trantor::Logger::kInfo);
#endif

    // 初始化全局配置管理（使用组织/应用名统一存储）
    config::ConfigManager::instance().init("CrossControl", "CrossControl");

    logging::useAsDefault("App");

    // 创建一个临时（不可见）的 QTextEdit 并在应用早期绑定为全局 Qt sink，
    // 以便捕获主窗口创建前产生的日志。LogWidget 就绪后会替换为真实的 UI
    // QTextEdit（通过 LogWidget::bindToLoggerManager），之后我们删除临时控件。
    QTextEdit* tmpLogEdit = new QTextEdit();
    tmpLogEdit->setVisible(false);
    logging::LoggerManager::instance().attachQtSink(tmpLogEdit, MAX_LOG_LINES);
    spdlog::info("Application started");

    // Create DeviceGateway early and pass to main window so UI can control REST/registry
    auto deviceGateway = std::make_unique<DeviceGateway::DeviceGateway>();
    CrossControlWidget mainWindow(deviceGateway.get());
    mainWindow.setWindowTitle("CrossControl");
    // 应用程序图标
    QIcon appIcon(":/icons/icons/app.svg");
    if (!appIcon.isNull()) {
        mainWindow.setWindowIcon(appIcon);
        QApplication::setWindowIcon(appIcon);
    }

    // 加载样式
    // 读取用户上次主题偏好（默认 auto 跟随系统）
    const QString themePref =
        config::ConfigManager::instance().getString("Preferences/theme", "auto");
    QString appliedTheme = themePref;
    if (themePref == "auto") { appliedTheme = detectSystemTheme(a); }
    const QString themePath =
        appliedTheme == "dark" ? ":/themes/themes/dark.qss" : ":/themes/themes/light.qss";
    QFile qss(themePath);
    if (qss.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QString style = QString::fromUtf8(qss.readAll());
        a.setStyleSheet(style);
    }
    mainWindow.setMinimumSize(1024, 640);
    mainWindow.show();

    // 完成全局绑定（将 sink 切换到 LogWidget 的 QTextEdit），并删除临时 QTextEdit
    if (auto lw = mainWindow.getLogWidget()) {
        lw->bindToLoggerManager();
        // bindToLoggerManager() 会在内部通过 LoggerManager::attachQtSink() 替换当前 sink，
        // 因此此时可以安全删除临时的 QTextEdit
        delete tmpLogEdit;
        tmpLogEdit = nullptr;
    }
    const int rc = a.exec();

    spdlog::info("Application exiting");
    // deviceGateway will be destroyed here as unique_ptr goes out of scope
    spdlog::shutdown();
    return rc;
}
