#ifndef CROSSCONTROL_LOGGING_H
#define CROSSCONTROL_LOGGING_H

#include <QMessageLogContext>
#include <QString>
#include <QtGlobal>
#include <memory>
#include <string>
#include <vector>

#include "spdlog/sinks/qt_sinks.h"
#include "spdlog/spdlog.h"  // 这里包含 spdlog/spdlog.h 方便直接使用 spdlog::info 等函数

class QTextEdit;

namespace logging {

// 统一日志管理器：集中管理 spdlog、Qt 消息重定向以及 Qt 文本控件 sink
class LoggerManager {
   public:
    static LoggerManager& instance();

    // 初始化全局日志（只需调用一次，幂等）
    void init();

    // 绑定 Qt 文本控件作为日志输出（统一的单一 sink）。重复调用会替换之前的控件。
    void attachQtSink(QTextEdit* edit,
                      int maxLines = 1000,
                      const std::string& pattern = "[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%n] %v");
    // 如果当前绑定的控件就是该对象，则移除该 Qt sink。
    void detachQtSink(QTextEdit* edit);

    // 控制级别与开关（仅影响 Qt sink，不改变默认 logger 的其他 sinks 行为）
    void setEnabled(bool enabled);
    void setLevel(spdlog::level::level_enum level);
    void setPattern(const std::string& pattern);

    // 获取或创建具名 logger（与默认 logger 共享同一组 sinks），
    // 便于在不同模块按名称区分来源，例如：getLogger("AudioVideo")。
    std::shared_ptr<spdlog::logger> getLogger(const std::string& name);

    // 将 Qt 的日志转发到 spdlog
    static void qtMessageHandler(QtMsgType type,
                                 const QMessageLogContext& context,
                                 const QString& msg);

   private:
    LoggerManager() = default;
    void ensureDefaultLogger_();
    void installQtSinkToAll_();
    void uninstallQtSinkFromAll_();

   private:
    std::shared_ptr<spdlog::sinks::qt_color_sink_mt> qt_sink_;
    bool enabled_{true};
    std::vector<std::weak_ptr<spdlog::logger>> loggers_;  // 已创建的具名 loggers
};

// 便捷别名与函数：返回具名 logger 的引用，便于使用点号语法
using Logger = spdlog::logger;
Logger& logger(const std::string& name);

// 将具名 logger 设为全局默认 logger，之后可以直接用 spdlog::info 等全局函数
void useAsDefault(const std::string& name);

}  // namespace logging

/**
 * @brief 日志管理器的别名
 *
 */
using LogM = logging::LoggerManager;

#endif  // CROSSCONTROL_LOGGING_H
