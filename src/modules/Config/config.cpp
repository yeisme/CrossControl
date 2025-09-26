#include "config.h"
#include <QCoreApplication>

namespace config {

ConfigManager& ConfigManager::instance() {
    static ConfigManager inst;
    return inst;
}

ConfigManager::ConfigManager() {
}

ConfigManager::~ConfigManager() {
    if (settings_) {
        settings_->sync();
        delete settings_;
        settings_ = nullptr;
    }
}

void ConfigManager::init(const QString& organization, const QString& application) {
    if (settings_) return; // 已初始化

    if (!organization.isEmpty() || !application.isEmpty()) {
        QCoreApplication::setOrganizationName(organization);
        QCoreApplication::setApplicationName(application);
    }

    settings_ = new QSettings(QSettings::IniFormat, QSettings::UserScope,
                              QCoreApplication::organizationName(),
                              QCoreApplication::applicationName());
}

QVariant ConfigManager::getValue(const QString& key, const QVariant& defaultValue) const {
    if (!settings_) return defaultValue;
    return settings_->value(key, defaultValue);
}

void ConfigManager::setValue(const QString& key, const QVariant& value) {
    if (!settings_) init();
    settings_->setValue(key, value);
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

} // namespace config
