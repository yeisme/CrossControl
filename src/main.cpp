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

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);

    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString &locale : uiLanguages) {
        const QString baseName = "CrossControl_" + QLocale(locale).name();
        if (translator.load(":/i18n/" + baseName)) {
            a.installTranslator(&translator);
            break;
        }
    }

    // Redirect Qt's internal messages to spdlog so existing qDebug() calls are captured.
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
