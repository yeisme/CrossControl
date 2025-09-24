#include "logwidget.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QFileDialog>
#include <QIODevice>
#include <QString>
#include <QTextCursor>
#include <QTextEdit>
#include <algorithm>
#include <string>

#include "spdlog/sinks/qt_sinks.h"
#include "spdlog/spdlog.h"
#include "ui_logwidget.h"

namespace {}  // namespace

LogWidget::LogWidget(QWidget* parent) : QWidget(parent), ui(new Ui::LogWidget) {
    ui->setupUi(this);

    // 初始化控件
    ui->cmbLevel->addItems({"TRACE", "DEBUG", "INFO", "WARN", "ERROR", "CRITICAL"});
    ui->cmbLevel->setCurrentIndex(m_minLevel);
    ui->chkEnable->setChecked(m_enabled);

    installSink();
}

LogWidget::~LogWidget() {
    uninstallSink();
    delete ui;
}

void LogWidget::installSink() {
    if (m_sink) return;
    // 使用 spdlog 的 qt_color_sink，绑定到我们的 QTextEdit
    m_sink = std::make_shared<spdlog::sinks::qt_color_sink_mt>(ui->textEdit, m_maxLines);
    // 时间/级别/消息
    m_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
    // 安装到默认 logger
    spdlog::default_logger()->sinks().push_back(m_sink);
}

void LogWidget::uninstallSink() {
    if (!m_sink) return;
    auto& sinks = spdlog::default_logger()->sinks();
    sinks.erase(std::remove(sinks.begin(), sinks.end(), m_sink), sinks.end());
    m_sink.reset();
}

void LogWidget::applyColors() {}

void LogWidget::on_btnBack_clicked() { emit backToMain(); }

void LogWidget::on_chkEnable_stateChanged(int state) {
    m_enabled = (state != 0);
    if (m_sink)
        m_sink->set_level(m_enabled ? static_cast<spdlog::level::level_enum>(m_minLevel)
                                    : spdlog::level::off);
}

void LogWidget::on_cmbLevel_currentIndexChanged(int index) {
    m_minLevel = index;
    // 将最低级别应用到 sink（留更宽松的默认 logger 级别，仅控制显示）
    if (m_sink && m_enabled) m_sink->set_level(static_cast<spdlog::level::level_enum>(m_minLevel));
}

void LogWidget::on_btnClear_clicked() { ui->textEdit->clear(); }

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
}
