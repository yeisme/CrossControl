#ifndef LOGWIDGET_H
#define LOGWIDGET_H

#include <QWidget>
#include <memory>

#include "spdlog/sinks/qt_sinks.h"

namespace spdlog {
namespace sinks {
class sink;
}
}  // namespace spdlog

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
    void installSink();
    void uninstallSink();
    void applyColors();

   private:
    Ui::LogWidget* ui;
    std::shared_ptr<spdlog::sinks::qt_color_sink_mt> m_sink;  // Qt 彩色 sink
    bool m_enabled{true};
    int m_minLevel{0};  // spdlog::level::trace
    int m_maxLines{500};
};

#endif  // LOGWIDGET_H
