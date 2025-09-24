#ifndef LOGWIDGET_H
#define LOGWIDGET_H

#include <spdlog/common.h>
#include <spdlog/logger.h>

#include <QWidget>

constexpr bool DEFAULT_LOG_ENABLED = true;
constexpr int MAX_LOG_LINES = 1000;  // QTextEdit 最多保留的日志行数

QT_BEGIN_NAMESPACE
namespace Ui {
class LogWidget;
}
QT_END_NAMESPACE

// 运行日志页面：将 spdlog 输出以可选方式展示给用户
class LogWidget : public QWidget {
    Q_OBJECT

   public:
    explicit LogWidget(QWidget* parent = nullptr);
    ~LogWidget();

   signals:
    void backToMain();

   public slots:
    // 无需手动 append，qt sink 会自动写入 QTextEdit

   private slots:
    void on_btnBack_clicked();
    void on_chkEnable_stateChanged(int state);
    void on_cmbLevel_currentIndexChanged(int index);
    void on_btnClear_clicked();
    void on_btnSave_clicked();

   private:
    void applyColors();

   private:
    Ui::LogWidget* ui;
    bool m_enabled{DEFAULT_LOG_ENABLED};
    int m_minLevel{spdlog::level::debug};
    int m_maxLines{MAX_LOG_LINES};
};

#endif  // LOGWIDGET_H
