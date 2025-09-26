#include "logwidget.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QFileDialog>
#include <QIODevice>
#include <QString>
#include <QTextCursor>
#include <QTextEdit>

#include "logging.h"
#include "spdlog/spdlog.h"
#include "ui_logwidget.h"

LogWidget::LogWidget(QWidget* parent) : QWidget(parent), ui(new Ui::LogWidget) {
    ui->setupUi(this);

    // 初始化控件
    ui->cmbLevel->addItems({"TRACE", "DEBUG", "INFO", "WARN", "ERROR", "CRITICAL"});
    ui->cmbLevel->setCurrentIndex(m_minLevel);
    ui->chkEnable->setChecked(m_enabled);

    // 注意：不要在这里做全局绑定（会导致应用启动早期的日志丢失）。
    // 全局绑定请在主流程中调用 bindToLoggerManager() 完成。
    spdlog::debug(
        "LogWidget initialized (deferred binding), enabled={} minLevel={}", m_enabled, m_minLevel);
}

LogWidget::~LogWidget() {
    // 不在析构中自动卸载全局 sink，避免在 UI 生命周期中意外移除。若需要显式卸载，请在
    // 应用级别通过 LoggerManager 进行管理。
    delete ui;
}

// 在主流程中调用以完成将 LogWidget 的 QTextEdit 作为全局 Qt sink 的绑定。
void LogWidget::bindToLoggerManager() {
    logging::LoggerManager::instance().attachQtSink(ui->textEdit, m_maxLines);
    logging::LoggerManager::instance().setEnabled(m_enabled);
    logging::LoggerManager::instance().setLevel(static_cast<spdlog::level::level_enum>(m_minLevel));
    spdlog::info("Log panel attached. Current min level: {}",
                 ui->cmbLevel->currentText().toStdString());
}

void LogWidget::applyColors() {}

/**
 * @brief 处理返回主页面按钮点击
 *
 */
void LogWidget::on_btnBack_clicked() {
    emit backToMain();
}

/**
 * @brief 处理启用日志复选框状态变化
 *
 * @param state 复选框状态
 */
void LogWidget::on_chkEnable_stateChanged(int state) {
    m_enabled = (state != 0);
    logging::LoggerManager::instance().setEnabled(m_enabled);
    if (m_enabled) {
        logging::LoggerManager::instance().setLevel(
            static_cast<spdlog::level::level_enum>(m_minLevel));
    }
    spdlog::info("Log panel {}", m_enabled ? "enabled" : "disabled");
    spdlog::debug("LogWidget: enabled changed to {}", m_enabled);
}

/**
 * @brief 处理日志级别下拉框的当前索引变化
 *
 * @param index 当前选中的索引
 */
void LogWidget::on_cmbLevel_currentIndexChanged(int index) {
    m_minLevel = index;
    // 将最低级别应用到统一 sink（不改变默认 logger 的其他 sinks 行为）
    if (m_enabled)
        logging::LoggerManager::instance().setLevel(
            static_cast<spdlog::level::level_enum>(m_minLevel));
    spdlog::info("Log level changed to {}", ui->cmbLevel->currentText().toStdString());
    spdlog::debug("LogWidget: level index {}", index);
}

/**
 * @brief 处理清除日志按钮点击
 *
 */
void LogWidget::on_btnClear_clicked() {
    ui->textEdit->clear();
}

/**
 * @brief 处理保存日志按钮点击
 *
 * 弹出文件对话框，允许用户选择保存路径和格式（HTML 或纯文本），
 * 然后将当前日志内容保存到指定文件。
 */
void LogWidget::on_btnSave_clicked() {
    const QString path = QFileDialog::getSaveFileName(
        nullptr,
        QCoreApplication::translate("LogWidget", "Save Logs"),
        QString::fromLatin1("logs.html"),
        QCoreApplication::translate("LogWidget", "HTML (*.html);;Text (*.txt)"));
    if (path.isEmpty()) return;
    if (path.toLower().endsWith(QString::fromLatin1(".html"))) {
        QFile f(path);
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            f.write(ui->textEdit->toHtml().toUtf8());
        }
    } else {
        QFile f(path);
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            f.write(ui->textEdit->toPlainText().toUtf8());
        }
    }

    spdlog::trace("Logs saved to {}", path.toStdString());
    spdlog::debug("LogWidget: saved logs to {}", path.toStdString());
}
