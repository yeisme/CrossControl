#include "config.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QMetaType>

namespace config {

ConfigManager& ConfigManager::instance() {
    static ConfigManager inst;
    return inst;
}

ConfigManager::ConfigManager() {}

ConfigManager::~ConfigManager() {
    if (settings_) {
        settings_->sync();
        delete settings_;
        settings_ = nullptr;
    }
}

void ConfigManager::init(const QString& organization, const QString& application) {
    if (settings_) return;  // 已初始化

    if (!organization.isEmpty() || !application.isEmpty()) {
        QCoreApplication::setOrganizationName(organization);
        QCoreApplication::setApplicationName(application);
    }

    settings_ = new QSettings(QSettings::IniFormat,
                              QSettings::UserScope,
                              QCoreApplication::organizationName(),
                              QCoreApplication::applicationName());

    // 自动尝试在应用程序目录中发现 CrossControlConfig.json 并加载
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString candidate = QDir(appDir).filePath("CrossControlConfig.json");
    if (QFile::exists(candidate)) { loadFromJsonFile(candidate); }
}

/* @brief 将一个 JSON 值扁平化为一个扁平的键值映射。
 *
 * @param val 要扁平化的 JSON 值
 * @param prefix 当前键前缀（用于递归）
 * @param out 输出映射
 */
static void flattenJson(const QJsonValue& val,
                        const QString& prefix,
                        QMap<QString, QJsonValue>& out) {
    if (val.isObject()) {
        QJsonObject obj = val.toObject();
        for (auto it = obj.constBegin(); it != obj.constEnd(); ++it) {
            const QString key = prefix.isEmpty() ? it.key() : (prefix + "/" + it.key());
            flattenJson(it.value(), key, out);
        }
    } else if (val.isArray()) {
        // Convert arrays to JSON strings so they can be stored as single values
        out.insert(prefix, val);
    } else {
        out.insert(prefix, val);
    }
}

/**
 * @brief 从指定 JSON 文件加载配置
 *  TODO: 通过命令行参数指定配置文件路径
 * @param path
 * @return true
 * @return false
 */
bool ConfigManager::loadFromJsonFile(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
    const QByteArray data = f.readAll();
    f.close();

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError) return false;

    QMap<QString, QJsonValue> flat;
    if (doc.isObject()) {
        flattenJson(doc.object(), QString(), flat);
    } else if (doc.isArray()) {
        flattenJson(doc.array(), QString(), flat);
    } else {
        return false;
    }

    // 将解析出的键值写入 QSettings（覆盖现有值）
    for (auto it = flat.constBegin(); it != flat.constEnd(); ++it) {
        const QString key = it.key();
        const QJsonValue v = it.value();
        if (v.isString()) {
            settings_->setValue(key, v.toString());
        } else if (v.isBool()) {
            settings_->setValue(key, v.toBool());
        } else if (v.isDouble()) {
            settings_->setValue(key, v.toDouble());
        } else if (v.isArray() || v.isObject()) {
            settings_->setValue(key, QJsonDocument(v.toObject()).toJson(QJsonDocument::Compact));
        } else if (v.isNull()) {
            settings_->remove(key);
        } else {
            settings_->setValue(key, v.toVariant());
        }
    }

    settings_->sync();
    return true;
}

QVariant ConfigManager::getValue(const QString& key, const QVariant& defaultValue) const {
    if (!settings_) return defaultValue;
    return settings_->value(key, defaultValue);
}

void ConfigManager::setValue(const QString& key, const QVariant& value) {
    if (!settings_) init();
    settings_->setValue(key, value);
}

QVariant ConfigManager::setOrDefault(const QString& key, const QVariant& defaultValue) {
    if (!settings_) init();
    const QVariant v = settings_->value(key, QVariant());
    if (!v.isValid() || v.isNull()) {
        settings_->setValue(key, defaultValue);
        settings_->sync();
        return defaultValue;
    }
    if (v.canConvert<QString>() && v.toString().isEmpty()) {
        settings_->setValue(key, defaultValue);
        settings_->sync();
        return defaultValue;
    }
    return v;
}

void ConfigManager::remove(const QString& key) {
    if (!settings_) return;
    settings_->remove(key);
}

void ConfigManager::sync() {
    if (!settings_) return;
    settings_->sync();
}

bool ConfigManager::getBool(const QString& key, bool defaultValue) const {
    return getValue(key, defaultValue).toBool();
}

int ConfigManager::getInt(const QString& key, int defaultValue) const {
    return getValue(key, defaultValue).toInt();
}

QString ConfigManager::getString(const QString& key, const QString& defaultValue) const {
    return getValue(key, defaultValue).toString();
}

QVariant ConfigManager::getOrDefault(const QString& key, const QVariant& defaultValue) const {
    const QVariant v = getValue(key, QVariant());
    if (!v.isValid() || v.isNull()) return defaultValue;
    // Treat empty QString as unset
    if (v.canConvert<QString>()) {
        if (v.toString().isEmpty()) return defaultValue;
    }
    return v;
}

}  // namespace config
