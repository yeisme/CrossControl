#include "factory.h"

#include <QJsonObject>
#include <mutex>

namespace HumanRecognition {

// 定义静态容器
static BackendFactoryMap g_backendFactories;
static BackendFactoryWithConfigMap g_backendFactoriesWithConfig;
static std::mutex g_backendFactoriesMutex;

void registerBackendFactory(const QString& name, BackendFactory factory) {
    std::lock_guard<std::mutex> lock(g_backendFactoriesMutex);
    g_backendFactories.insert(name, std::move(factory));
}

void registerBackendFactory(const QString& name, BackendFactoryWithConfig factory) {
    std::lock_guard<std::mutex> lock(g_backendFactoriesMutex);
    g_backendFactoriesWithConfig.insert(name, std::move(factory));
}

bool unregisterBackendFactory(const QString& name) {
    std::lock_guard<std::mutex> lock(g_backendFactoriesMutex);
    bool removed = g_backendFactories.remove(name) > 0;
    removed = g_backendFactoriesWithConfig.remove(name) > 0 || removed;
    return removed;
}

std::unique_ptr<IHumanRecognitionBackend> createBackendInstance(const QString& name) {
    std::lock_guard<std::mutex> lock(g_backendFactoriesMutex);
    if (g_backendFactories.contains(name)) { return g_backendFactories.value(name)(); }
    if (g_backendFactoriesWithConfig.contains(name)) {
        return g_backendFactoriesWithConfig.value(name)(QJsonObject{});
    }
    return nullptr;
}

std::unique_ptr<IHumanRecognitionBackend> createBackendInstance(const QString& name,
                                                                const QJsonObject& config) {
    std::lock_guard<std::mutex> lock(g_backendFactoriesMutex);
    if (g_backendFactoriesWithConfig.contains(name)) {
        return g_backendFactoriesWithConfig.value(name)(config);
    }
    if (g_backendFactories.contains(name)) { return g_backendFactories.value(name)(); }
    return nullptr;
}

void clearRegisteredBackends() {
    std::lock_guard<std::mutex> lock(g_backendFactoriesMutex);
    g_backendFactories.clear();
    g_backendFactoriesWithConfig.clear();
}

QVector<QString> listRegisteredBackends() {
    std::lock_guard<std::mutex> lock(g_backendFactoriesMutex);
    QVector<QString> out;
    out.reserve(g_backendFactories.size() + g_backendFactoriesWithConfig.size());
    // Collect keys from QHash maps
    for (const auto& k : g_backendFactories.keys()) out.append(k);
    for (const auto& k : g_backendFactoriesWithConfig.keys()) out.append(k);
    return out;
}

}  // namespace HumanRecognition
