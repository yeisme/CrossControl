#include "config.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QMetaType>
#include <functional>

namespace config {

// Embedded default JSON used when no external CrossControlConfig.json is available.
const char* ConfigManager::kEmbeddedDefaultJson = R"json({
    "Storage": {
        "driver": "QSQLITE",
        "database": "crosscontrol.db",
        "foreignKeys": true
    },
    "Connect": {
        "endpoint": "127.0.0.1:9000"
    },
    "Preferences": {
        "theme": "auto"
    }
})json";

// forward declare helper
static void flattenJson(const QJsonValue& val,
                        const QString& prefix,
                        QMap<QString, QJsonValue>& out);

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
    if (QFile::exists(candidate)) {
        loadFromJsonFile(candidate);
    } else {
        // Load embedded defaults if external config not present. Do not remember path so saves will
        // go to app dir by default.
        QJsonParseError err;
        const QByteArray data = QByteArray::fromStdString(std::string(kEmbeddedDefaultJson));
        const QJsonDocument doc = QJsonDocument::fromJson(data, &err);
        if (err.error == QJsonParseError::NoError) {
            QMap<QString, QJsonValue> flat;
            if (doc.isObject())
                flattenJson(doc.object(), QString(), flat);
            else if (doc.isArray())
                flattenJson(doc.array(), QString(), flat);

            for (auto it = flat.constBegin(); it != flat.constEnd(); ++it) {
                const QString key = it.key();
                const QJsonValue v = it.value();
                if (v.isString())
                    settings_->setValue(key, v.toString());
                else if (v.isBool())
                    settings_->setValue(key, v.toBool());
                else if (v.isDouble())
                    settings_->setValue(key, v.toDouble());
                else if (v.isObject())
                    settings_->setValue(key,
                                        QJsonDocument(v.toObject()).toJson(QJsonDocument::Compact));
                else if (v.isArray())
                    settings_->setValue(key,
                                        QJsonDocument(v.toArray()).toJson(QJsonDocument::Compact));
                else if (v.isNull())
                    settings_->remove(key);
                else
                    settings_->setValue(key, v.toVariant());
            }
            settings_->sync();
        }
    }
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
bool ConfigManager::loadFromJsonFile(const QString& path, bool rememberPath) {
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
        } else if (v.isObject()) {
            settings_->setValue(key, QJsonDocument(v.toObject()).toJson(QJsonDocument::Compact));
        } else if (v.isArray()) {
            settings_->setValue(key, QJsonDocument(v.toArray()).toJson(QJsonDocument::Compact));
        } else if (v.isNull()) {
            settings_->remove(key);
        } else {
            settings_->setValue(key, v.toVariant());
        }
    }

    settings_->sync();
    // remember path for future saves (if requested)
    if (rememberPath) jsonFilePath_ = path;
    return true;
}

bool ConfigManager::loadFromJsonString(const QString& json, bool rememberPath) {
    QJsonParseError err;
    const QByteArray data = json.toUtf8();
    const QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError) return false;

    QMap<QString, QJsonValue> flat;
    if (doc.isObject())
        flattenJson(doc.object(), QString(), flat);
    else if (doc.isArray())
        flattenJson(doc.array(), QString(), flat);
    else
        return false;

    for (auto it = flat.constBegin(); it != flat.constEnd(); ++it) {
        const QString key = it.key();
        const QJsonValue v = it.value();
        if (v.isString())
            settings_->setValue(key, v.toString());
        else if (v.isBool())
            settings_->setValue(key, v.toBool());
        else if (v.isDouble())
            settings_->setValue(key, v.toDouble());
        else if (v.isObject())
            settings_->setValue(key, QJsonDocument(v.toObject()).toJson(QJsonDocument::Compact));
        else if (v.isArray())
            settings_->setValue(key, QJsonDocument(v.toArray()).toJson(QJsonDocument::Compact));
        else if (v.isNull())
            settings_->remove(key);
        else
            settings_->setValue(key, v.toVariant());
    }
    settings_->sync();
    if (rememberPath && jsonFilePath_.isEmpty())
        jsonFilePath_ = QCoreApplication::applicationDirPath() + "/CrossControlConfig.json";
    return true;
}

QString ConfigManager::toJsonString() const {
    if (!settings_) return QString();
    const QStringList keys = settings_->allKeys();
    std::function<void(QJsonObject&, const QStringList&, int, const QJsonValue&)> setNested;
    setNested = [&](QJsonObject& obj, const QStringList& parts, int idx, const QJsonValue& val) {
        const QString& key = parts[idx];
        if (idx == parts.size() - 1) {
            obj.insert(key, val);
            return;
        }
        QJsonValue child = obj.value(key);
        QJsonObject childObj = child.isObject() ? child.toObject() : QJsonObject();
        setNested(childObj, parts, idx + 1, val);
        obj.insert(key, childObj);
    };

    QJsonObject mergedRoot;
    for (const QString& fullKey : keys) {
        QVariant v = settings_->value(fullKey);
        QJsonValue jsonVal;
        if (v.typeId() == QMetaType::QString) {
            const QString s = v.toString();
            QJsonParseError perr;
            QJsonDocument doc = QJsonDocument::fromJson(s.toUtf8(), &perr);
            if (perr.error == QJsonParseError::NoError) {
                if (doc.isObject())
                    jsonVal = doc.object();
                else if (doc.isArray())
                    jsonVal = doc.array();
                else
                    jsonVal = QJsonValue(s);
            } else {
                jsonVal = QJsonValue(s);
            }
        } else if (v.typeId() == QMetaType::Bool) {
            jsonVal = QJsonValue(v.toBool());
        } else if (v.canConvert<double>()) {
            jsonVal = QJsonValue(v.toDouble());
        } else {
            jsonVal = QJsonValue(v.toString());
        }
        QStringList parts = fullKey.split('/', Qt::SkipEmptyParts);
        if (parts.isEmpty()) continue;
        setNested(mergedRoot, parts, 0, jsonVal);
    }
    QJsonDocument outDoc(mergedRoot);
    return QString::fromUtf8(outDoc.toJson(QJsonDocument::Indented));
}

// Save QSettings contents back to a JSON file. If path empty, use remembered jsonFilePath_ or app
// dir
bool ConfigManager::saveToJsonFile(const QString& path) const {
    if (!settings_) return false;

    QString outPath = path;
    if (outPath.isEmpty()) {
        if (!jsonFilePath_.isEmpty())
            outPath = jsonFilePath_;
        else
            outPath = QCoreApplication::applicationDirPath() + "/CrossControlConfig.json";
    }

    // build a JSON object from flat keys (keys may contain '/') and unflatten into nested objects
    QJsonObject root;
    const QStringList keys = settings_->allKeys();
    for (const QString& fullKey : keys) {
        QVariant v = settings_->value(fullKey);

        QJsonValue jsonVal;
        // try to parse strings that are JSON
        if (v.typeId() == QMetaType::QString) {
            const QString s = v.toString();
            QJsonParseError perr;
            QJsonDocument doc = QJsonDocument::fromJson(s.toUtf8(), &perr);
            if (perr.error == QJsonParseError::NoError) {
                if (doc.isObject())
                    jsonVal = doc.object();
                else if (doc.isArray())
                    jsonVal = doc.array();
                else
                    jsonVal = QJsonValue(s);
            } else {
                jsonVal = QJsonValue(s);
            }
        } else if (v.typeId() == QMetaType::Bool) {
            jsonVal = QJsonValue(v.toBool());
        } else if (v.canConvert<double>()) {
            jsonVal = QJsonValue(v.toDouble());
        } else {
            jsonVal = QJsonValue(v.toString());
        }

        // unflatten into root by splitting on '/'
        QStringList parts = fullKey.split('/', Qt::SkipEmptyParts);
        QJsonObject* current = &root;
        for (int i = 0; i < parts.size(); ++i) {
            const QString& p = parts[i];
            if (i == parts.size() - 1) {
                current->insert(p, jsonVal);
            } else {
                QJsonValue child = current->value(p);
                if (!child.isObject()) {
                    // replace non-object or missing with an object
                    QJsonObject obj;
                    current->insert(p, obj);
                    child = current->value(p);
                }
                // move deeper
                QJsonObject nested = child.toObject();
                // Need to keep a reference; workaround by updating current to a temporary object
                // pointer We'll update the parent object after inserting deeper entries. Simpler
                // approach: operate via a stack of object paths using mutable objects. To keep this
                // implementation concise, extract nested, and set pointer via local variable hack.
                // Create a mutable copy path: rebuild nested pointers at end when writing; for now,
                // we will work by recursively merging which is acceptable for small config sizes.
                // Approach: set current to a temporary object stored in root by reference? C++
                // doesn't allow. Instead, use iterative merging: get reference to nested by
                // creating a reference in a map? Simpler: use helper function.
            }
        }
    }

    // Because creating nested QJsonObject references in-place is cumbersome, implement a second
    // pass that merges each key
    QJsonObject finalRoot;
    for (const QString& fullKey : keys) {
        QVariant v = settings_->value(fullKey);
        QJsonValue jsonVal;
        if (v.typeId() == QMetaType::QString) {
            const QString s = v.toString();
            QJsonParseError perr;
            QJsonDocument doc = QJsonDocument::fromJson(s.toUtf8(), &perr);
            if (perr.error == QJsonParseError::NoError) {
                if (doc.isObject())
                    jsonVal = doc.object();
                else if (doc.isArray())
                    jsonVal = doc.array();
                else
                    jsonVal = QJsonValue(s);
            } else {
                jsonVal = QJsonValue(s);
            }
        } else if (v.typeId() == QMetaType::Bool) {
            jsonVal = QJsonValue(v.toBool());
        } else if (v.canConvert<double>()) {
            jsonVal = QJsonValue(v.toDouble());
        } else {
            jsonVal = QJsonValue(v.toString());
        }

        QStringList parts = fullKey.split('/', Qt::SkipEmptyParts);
        QJsonObject* cursor = &finalRoot;
        for (int i = 0; i < parts.size(); ++i) {
            const QString& part = parts[i];
            if (i == parts.size() - 1) {
                cursor->insert(part, jsonVal);
            } else {
                QJsonValue existing = cursor->value(part);
                if (!existing.isObject()) {
                    // create
                    QJsonObject obj;
                    cursor->insert(part, obj);
                    existing = cursor->value(part);
                }
                QJsonObject nested = existing.toObject();
                // replace the object in-place to get a mutable reference
                cursor->insert(part, nested);
                // get pointer to nested inside finalRoot via lookup again
                QJsonValue lookup = cursor->value(part);
                QJsonObject tmp = lookup.toObject();
                // To modify nested, we need to assign back later. Use a stack of parent pointers:
                // simpler approach is to descend by keeping a vector of keys and rebuild finalRoot
                // after all insertions. For small config it's fine. Use a helper: create a
                // reference path and set using recursive function below. We'll use the recursive
                // setter defined next.
            }
        }
    }

    // Implement recursive setter to properly merge nested objects
    std::function<void(QJsonObject&, const QStringList&, int, const QJsonValue&)> setNested;
    setNested = [&](QJsonObject& obj, const QStringList& parts, int idx, const QJsonValue& val) {
        const QString& key = parts[idx];
        if (idx == parts.size() - 1) {
            obj.insert(key, val);
            return;
        }
        QJsonValue child = obj.value(key);
        QJsonObject childObj = child.isObject() ? child.toObject() : QJsonObject();
        setNested(childObj, parts, idx + 1, val);
        obj.insert(key, childObj);
    };

    QJsonObject mergedRoot;
    for (const QString& fullKey : keys) {
        QVariant v = settings_->value(fullKey);
        QJsonValue jsonVal;
        if (v.typeId() == QMetaType::QString) {
            const QString s = v.toString();
            QJsonParseError perr;
            QJsonDocument doc = QJsonDocument::fromJson(s.toUtf8(), &perr);
            if (perr.error == QJsonParseError::NoError) {
                if (doc.isObject())
                    jsonVal = doc.object();
                else if (doc.isArray())
                    jsonVal = doc.array();
                else
                    jsonVal = QJsonValue(s);
            } else {
                jsonVal = QJsonValue(s);
            }
        } else if (v.typeId() == QMetaType::Bool) {
            jsonVal = QJsonValue(v.toBool());
        } else if (v.canConvert<double>()) {
            jsonVal = QJsonValue(v.toDouble());
        } else {
            jsonVal = QJsonValue(v.toString());
        }
        QStringList parts = fullKey.split('/', Qt::SkipEmptyParts);
        if (parts.isEmpty()) continue;
        setNested(mergedRoot, parts, 0, jsonVal);
    }

    QJsonDocument outDoc(mergedRoot);
    QFile out(outPath);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
    out.write(outDoc.toJson(QJsonDocument::Indented));
    out.close();
    return true;
}

QVariant ConfigManager::getValue(const QString& key, const QVariant& defaultValue) const {
    if (!settings_) return defaultValue;
    return settings_->value(key, defaultValue);
}

void ConfigManager::setValue(const QString& key, const QVariant& value) {
    if (!settings_) init();
    settings_->setValue(key, value);
    settings_->sync();
    // attempt to persist back to JSON immediately (two-way sync). Ignore failures here.
    saveToJsonFile();
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

QStringList ConfigManager::allKeys() const {
    if (!settings_) return {};
    return settings_->allKeys();
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
