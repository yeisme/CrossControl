#include "logging.h"

#include <QCoreApplication>
#include <QDir>
#include <QTextEdit>
#include <algorithm>
#include <chrono>

#include "spdlog/common.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/spdlog.h"
// We use registry internals to enumerate all registered loggers so that
// attachQtSink() can add the Qt sink to loggers not explicitly created
// via LoggerManager::getLogger(). This ensures "子模块的日志也能打印到日志页面".
#include "spdlog/details/registry.h"
// do not depend on spdlog internals (registry) — operate on default_logger and loggers we track

namespace logging {

LoggerManager& LoggerManager::instance() {
    static LoggerManager inst;  // 这里使用静态函数，全局唯一实例
    return inst;
}

void LoggerManager::ensureDefaultLogger_() {
    // 确保存在默认 logger；无论是否新建，都统一设置级别与格式
    if (!spdlog::default_logger()) {
        spdlog::set_default_logger(spdlog::stdout_color_mt("CrossControl"));
    }
    spdlog::set_level(spdlog::level::trace);
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%n] %v");
}

// Helper: ensure logs directory exists and create rotating file sink
void LoggerManager::enableFileLogging(bool enabled) {
    if (enabled == file_enabled_) return;
    file_enabled_ = enabled;
    if (file_enabled_) {
        // Determine logs directory. If log_dir_ is relative, resolve it
        // relative to applicationDirPath (if available) or cwd.
        QString dirPath;
        if (!log_dir_.empty()) {
            QString qdir = QString::fromStdString(log_dir_);
            QDir qd(qdir);
            if (qd.isAbsolute()) {
                dirPath = qdir;
            } else {
                const QString appDir = QCoreApplication::applicationDirPath();
                if (!appDir.isEmpty())
                    dirPath = appDir + "/" + qdir;
                else
                    dirPath = QDir::currentPath() + "/" + qdir;
            }
        } else {
            const QString appDir = QCoreApplication::applicationDirPath();
            if (!appDir.isEmpty())
                dirPath = appDir + "/logs";
            else
                dirPath = QDir::currentPath() + "/logs";
        }

        // create logs dir if needed
        QDir d(dirPath);
        if (!d.exists()) d.mkpath(".");

        const std::string logfile = (dirPath + "/CrossControl.log").toStdString();
        try {
            file_sink_ = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                logfile, 10 * 1024 * 1024, 7);
            // Apply the current desired level; if UI logging is disabled,
            // keep file sink off until enabled.
            file_sink_->set_level(enabled_ ? current_level_ : spdlog::level::off);
            file_sink_->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%n] %v");
        } catch (const std::exception& ex) {
            spdlog::error("Failed to create file sink: {}", ex.what());
            file_sink_.reset();
        }
        if (file_sink_) {
            // attach to default logger and tracked loggers
            if (spdlog::default_logger()) spdlog::default_logger()->sinks().push_back(file_sink_);
            for (auto& w : loggers_)
                if (auto lg = w.lock()) {
                    auto& s = lg->sinks();
                    if (std::find(s.begin(), s.end(), file_sink_) == s.end())
                        s.push_back(file_sink_);
                    // ensure this logger flushes at info and above so file reflects recent logs
                    lg->flush_on(spdlog::level::info);
                }
            // also ensure default logger flushes at info
            if (spdlog::default_logger()) spdlog::default_logger()->flush_on(spdlog::level::info);
        }
    } else {
        if (file_sink_) {
            auto removeSinkFromLogger = [&](std::shared_ptr<spdlog::logger> lg) {
                if (!lg) return;
                auto& s = lg->sinks();
                s.erase(std::remove(s.begin(), s.end(), file_sink_), s.end());
            };
            if (spdlog::default_logger()) removeSinkFromLogger(spdlog::default_logger());
            for (auto& w : loggers_)
                if (auto lg = w.lock()) removeSinkFromLogger(lg);
            file_sink_.reset();
        }
    }
}

void LoggerManager::setLogDirectory(const std::string& dir) {
    log_dir_ = dir;
    if (file_enabled_) {
        // re-create file sink with new directory
        enableFileLogging(false);
        enableFileLogging(true);
    }
}

void LoggerManager::setFileLogLevel(spdlog::level::level_enum level) {
    if (file_sink_) file_sink_->set_level(level);
}

void LoggerManager::init() {
    ensureDefaultLogger_();
    // Enable file logging by default. This will create logs/ under the
    // application directory (or cwd) unless the user calls setLogDirectory().
    enableFileLogging(true);
    // Start a periodic flush thread so file sinks are flushed regularly (1s).
    // This provides near-real-time persistence without forcing a flush on every
    // log call which can be expensive.
    spdlog::flush_every(std::chrono::seconds(1));
}

void LoggerManager::installQtSinkToAll_() {
    if (!qt_sink_) return;
    // 安装到默认 logger
    auto addSinkToLogger = [&](std::shared_ptr<spdlog::logger> lg) {
        if (!lg) return;
        auto& s = lg->sinks();
        if (std::find(s.begin(), s.end(), qt_sink_) == s.end()) { s.push_back(qt_sink_); }
    };

    // default logger first
    if (spdlog::default_logger()) addSinkToLogger(spdlog::default_logger());

    // add to loggers we explicitly track
    loggers_.erase(
        std::remove_if(loggers_.begin(), loggers_.end(), [](auto& w) { return w.expired(); }),
        loggers_.end());
    for (auto& w : loggers_)
        if (auto lg = w.lock()) addSinkToLogger(lg);

    // Also attempt to attach to a small set of known logger names used across modules.
    // This is conservative and avoids depending on spdlog internal registry APIs which
    // may vary between versions. If a module creates a named logger before our
    // attach call, spdlog::get(name) will return it and we ensure it has the Qt sink.
    const std::vector<std::string> known = {
        "AudioVideo", "HR.OpenCV", "HumanRecognition", "App", "CrossControl"};
    for (const auto& name : known) {
        if (auto lg = spdlog::get(name)) addSinkToLogger(lg);
    }

    // 注意：我们故意不在这里扫描 spdlog 的内部。为了确保 Qt 接收器的覆盖率， 请在主函数的早期调用
    // LoggerManager::init()，并在 UI 准备好后通过 attachQtSink() 附加 Qt 接收器。通过我们的
    // getLogger() 创建的新日志记录器如果存在 qt_sink_ 将继承它。
}

void LoggerManager::uninstallQtSinkFromAll_() {
    if (!qt_sink_) return;
    auto removeSinkFromLogger = [&](std::shared_ptr<spdlog::logger> lg) {
        if (!lg) return;
        auto& s = lg->sinks();
        s.erase(std::remove(s.begin(), s.end(), qt_sink_), s.end());
    };

    if (spdlog::default_logger()) removeSinkFromLogger(spdlog::default_logger());

    loggers_.erase(
        std::remove_if(loggers_.begin(), loggers_.end(), [](auto& w) { return w.expired(); }),
        loggers_.end());
    for (auto& w : loggers_)
        if (auto lg = w.lock()) removeSinkFromLogger(lg);

    const std::vector<std::string> known = {
        "AudioVideo", "HR.OpenCV", "HumanRecognition", "App", "CrossControl"};
    for (const auto& name : known) {
        if (auto lg = spdlog::get(name)) removeSinkFromLogger(lg);
    }

    // See note above: do not touch spdlog internals here.
}

void LoggerManager::attachQtSink(QTextEdit* edit, int maxLines, const std::string& pattern) {
    ensureDefaultLogger_();
    // 替换已有 sink：先卸载旧的
    uninstallQtSinkFromAll_();
    qt_sink_ = std::make_shared<spdlog::sinks::qt_color_sink_mt>(edit, maxLines);
    qt_sink_->set_pattern(pattern);
    qt_sink_->set_level(enabled_ ? spdlog::level::trace : spdlog::level::off);
    installQtSinkToAll_();
}

void LoggerManager::detachQtSink(QTextEdit*) {
    if (!qt_sink_) return;
    // 统一只允许一个 Qt sink，调用即移除当前 sink
    uninstallQtSinkFromAll_();
    qt_sink_.reset();
}

void LoggerManager::setEnabled(bool enabled) {
    enabled_ = enabled;
    if (qt_sink_) { qt_sink_->set_level(enabled ? current_level_ : spdlog::level::off); }
    if (file_sink_) { file_sink_->set_level(enabled ? current_level_ : spdlog::level::off); }
}

void LoggerManager::setLevel(spdlog::level::level_enum level) {
    current_level_ = level;
    if (qt_sink_) qt_sink_->set_level(level);
    if (file_sink_) file_sink_->set_level(level);
}

void LoggerManager::setPattern(const std::string& pattern) {
    if (qt_sink_) qt_sink_->set_pattern(pattern);
}

std::shared_ptr<spdlog::logger> LoggerManager::getLogger(const std::string& name) {
    ensureDefaultLogger_();
    auto lg = spdlog::get(name);
    if (!lg) {
        // 让具名 logger 复用默认 logger 的 sinks
        auto& dsinks = spdlog::default_logger()->sinks();
        lg = std::make_shared<spdlog::logger>(name, dsinks.begin(), dsinks.end());
        lg->set_level(spdlog::level::trace);
        lg->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
        // ensure new logger flushes at info and above
        lg->flush_on(spdlog::level::info);
        spdlog::register_logger(lg);
        // 如果我们已经有 qt_sink_，确保新的 logger 也带上它
        if (qt_sink_) {
            auto& s = lg->sinks();
            if (std::find(s.begin(), s.end(), qt_sink_) == s.end()) s.push_back(qt_sink_);
        }
        loggers_.push_back(lg);
    }
    return lg;
}

void LoggerManager::qtMessageHandler(QtMsgType type,
                                     const QMessageLogContext& /*context*/,
                                     const QString& msg) {
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
            spdlog::shutdown();
            abort();
    }
}

/**
 * @brief 获取具名 logger 的引用
 *
 * @param name
 * @return Logger&
 */
Logger& logger(const std::string& name) {
    auto lg = LoggerManager::instance().getLogger(name);
    return *lg;
}

/**
 * @brief 将具名 logger 设为全局默认 logger，之后可以直接用 spdlog::info 等全局函数
 *
 * @param name
 */
void useAsDefault(const std::string& name) {
    auto lg = LoggerManager::instance().getLogger(name);
    spdlog::set_default_logger(std::move(lg));
}

}  // namespace logging
