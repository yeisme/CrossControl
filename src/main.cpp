#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QMessageLogContext>
#include <QPushButton>
#include <QSplitter>
#include <QTextEdit>
#include <QTranslator>
#include <QVBoxLayout>
#include <QWidget>
#include <memory>

#include "crosscontrolwidget.h"
#include "spdlog/spdlog.h"

/**
 * @brief Redirect Qt log messages to spdlog
 *
 * @param type
 * @param context
 * @param msg
 */
static void qtMessageToSpdlog(QtMsgType type, const QMessageLogContext &context, const QString &msg) {
    // 将 Qt 消息转发到 spdlog
    const std::string text = msg.toUtf8().constData();
    switch (type) {
        case QtDebugMsg:
            spdlog::debug(text);
            break;
        case QtInfoMsg:
            spdlog::info(text);
            break;
        case QtWarningMsg:
            spdlog::warn(text);
            break;
        case QtCriticalMsg:
            spdlog::error(text);
            break;
        case QtFatalMsg:
            spdlog::critical(text);
            // Ensure the message is flushed before aborting.
            spdlog::shutdown();  // 确保在中止之前刷新消息
            abort();             // 中止程序
    }
}

// 为当前系统用户界面语言安装翻译器。如果加载并安装了翻译，则返回一个非空的 unique_ptr；否则返回 nullptr。
static std::unique_ptr<QTranslator> installSystemTranslator(QCoreApplication &app) {
    auto translator = std::make_unique<QTranslator>();
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    bool translatorLoaded = false;
    for (const QString &locale : uiLanguages) {
        const QString baseName = "CrossControl_" + QLocale(locale).name();  // e.g. zh_CN 

        // Candidate directories; resource and typical on-disk locations
        const QString appDir = QCoreApplication::applicationDirPath();
        const QStringList dirs = {":/i18n", appDir + "/i18n", appDir + "/../i18n", appDir + "/../../src/i18n"};

        for (const QString &dir : dirs) {
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

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);

    // 保持翻译器在应用程序的生命周期内有效
    const auto translatorHolder = installSystemTranslator(a);

    // 将 Qt 的内部信息重定向到 spdlog，以便捕获现有的 qDebug() 调用。
    qInstallMessageHandler(qtMessageToSpdlog);

    spdlog::info("Application starting");

    CrossControlWidget mainWindow;
    mainWindow.setWindowTitle("CrossControl");
    mainWindow.setMinimumSize(1024, 640);
    mainWindow.show();
    const int rc = a.exec();

    spdlog::info("Application exiting");
    spdlog::shutdown();
    return rc;
}
