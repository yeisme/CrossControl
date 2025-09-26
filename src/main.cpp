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
#include "modules/Config/config.h"
#include <QSplitter>
#include <QTextEdit>
#include <QTranslator>
#include <QVBoxLayout>
#include <QWidget>
#include <memory>

#include "crosscontrolwidget.h"
#include "logging.h"
#include "spdlog/spdlog.h"

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

    // 初始化全局配置管理（使用组织/应用名统一存储）
    config::ConfigManager::instance().init("CrossControl", "CrossControl");

    logging::useAsDefault("App");

    spdlog::info("Application started");

    CrossControlWidget mainWindow;
    mainWindow.setWindowTitle("CrossControl");
    // 应用程序图标
    QIcon appIcon(":/icons/icons/app.svg");
    if (!appIcon.isNull()) {
        mainWindow.setWindowIcon(appIcon);
        QApplication::setWindowIcon(appIcon);
    }

    // 加载样式
    // 读取用户上次主题偏好（默认 auto 跟随系统）
    const QString themePref = config::ConfigManager::instance().getString("Preferences/theme", "auto");
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
    const int rc = a.exec();

    spdlog::info("Application exiting");
    spdlog::shutdown();
    return rc;
}
