#ifndef CROSSCONTROL_CONFIG_H
#define CROSSCONTROL_CONFIG_H

#include <QString>
#include <QVariant>
#include <QSettings>

// Export macro for config library when built as shared
#ifdef config_EXPORTS
#define CONFIG_EXPORT Q_DECL_EXPORT
#else
#define CONFIG_EXPORT Q_DECL_IMPORT
#endif

namespace config {

// 简单的全局配置管理器，封装 QSettings，用于模块间统一配置访问
class CONFIG_EXPORT ConfigManager {
public:
    static ConfigManager& instance();

    // 初始化（可选指定组织与应用名）
    void init(const QString& organization = QString(), const QString& application = QString());

    // 读取/写入配置
    QVariant getValue(const QString& key, const QVariant& defaultValue = QVariant()) const;
    void setValue(const QString& key, const QVariant& value);

    // 移除键与同步
    void remove(const QString& key);
    void sync();

    // 方便的布尔/整型/字符串访问
    bool getBool(const QString& key, bool defaultValue = false) const;
    int getInt(const QString& key, int defaultValue = 0) const;
    QString getString(const QString& key, const QString& defaultValue = QString()) const;

private:
    ConfigManager();
    ~ConfigManager();

    QSettings* settings_ = nullptr;
};

// 方便别名
using Config = ConfigManager;

} // namespace config

#endif // CROSSCONTROL_CONFIG_H
