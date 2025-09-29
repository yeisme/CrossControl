#ifndef CROSSCONTROL_CONFIG_H
#define CROSSCONTROL_CONFIG_H

#include <QSettings>
#include <QString>
#include <QVariant>

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

    // 从指定 JSON 文件加载配置（会覆盖当前同名键），返回是否成功解析并应用
    // rememberPath 控制是否记住此路径以便后续保存（默认记住）
    bool loadFromJsonFile(const QString& path, bool rememberPath = true);

    // 读取/写入配置
    QVariant getValue(const QString& key, const QVariant& defaultValue = QVariant()) const;
    void setValue(const QString& key, const QVariant& value);
    // 如果键不存在或为空则写入默认值并返回该默认值，否则返回现有值
    QVariant setOrDefault(const QString& key, const QVariant& defaultValue = QVariant());
    QVariant getOrDefault(const QString& key, const QVariant& defaultValue = QVariant()) const;

    // Save current settings back to JSON file (if path omitted, use remembered path or app dir)
    bool saveToJsonFile(const QString& path = QString()) const;

    // Parse JSON content from string and apply to settings (optionally remember as the config file
    // path)
    bool loadFromJsonString(const QString& json, bool rememberPath = false);

    // Export current settings to a JSON string (pretty-printed)
    QString toJsonString() const;

    // 移除键与同步
    void remove(const QString& key);
    void sync();

    // 方便的布尔/整型/字符串访问
    bool getBool(const QString& key, bool defaultValue = false) const;
    int getInt(const QString& key, int defaultValue = 0) const;
    QString getString(const QString& key, const QString& defaultValue = QString()) const;

    // Return all keys currently present in the underlying QSettings
    QStringList allKeys() const;

   private:
    ConfigManager();
    ~ConfigManager();

    QSettings* settings_ = nullptr;
    // If a JSON file was loaded, remember its path to enable writing back
    QString jsonFilePath_;

    // Embedded default JSON content (fallback when no external config present)
    static const char* kEmbeddedDefaultJson;
};

// 方便别名
using Config = ConfigManager;

}  // namespace config

#endif  // CROSSCONTROL_CONFIG_H
