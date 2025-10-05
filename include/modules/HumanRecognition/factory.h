/**
 * @file factory.h
 * @brief 后端工厂类型与注册 API 声明
 */
#pragma once

#include <qcontainerfwd.h>

#include <QHash>
#include <QVector>
#include <functional>

#include "ihumanrecognitionbackend.h"

namespace HumanRecognition {

/**
 * @brief 工厂函数类型：无配置构造
 *
 * 返回一个 IHumanRecognitionBackend 的独占指针。通常由具体后端在静态初始化时
 * 将自己的工厂函数注册到框架中。
 */
using BackendFactory = std::function<std::unique_ptr<IHumanRecognitionBackend>()>;

/**
 * @brief 工厂函数类型：接收配置的构造
 *
 * 有些后端需要从 JSON 配置中初始化模型或参数，使用此类型的工厂函数。
 */
using BackendFactoryWithConfig =
    std::function<std::unique_ptr<IHumanRecognitionBackend>(const QJsonObject&)>;

/**
 * @brief 机器识别后端工厂注册表
 *
 */
using BackendFactoryMap = QHash<QString, BackendFactory>;

/**
 * @brief 机器识别后端工厂注册表，支持json配置
 *
 */
using BackendFactoryWithConfigMap = QHash<QString, BackendFactoryWithConfig>;

/**
 * @brief 注册一个无配置后端工厂
 * @param name 唯一后端名
 * @param factory 工厂函数
 */
void registerBackendFactory(const QString& name, BackendFactory factory);

/**
 * @brief 注册一个带配置的后端工厂
 * @param name 唯一后端名
 * @param factory 接受 QJsonObject 配置的工厂函数
 */
void registerBackendFactory(const QString& name, BackendFactoryWithConfig factory);

/**
 * @brief 注销已注册的后端工厂
 * @return 如果存在并成功注销返回 true，否则返回 false
 */
bool unregisterBackendFactory(const QString& name);

/**
 * @brief 创建一个后端实例（无配置）
 * @param name 后端名
 * @return 成功返回具体实现的唯一指针，失败返回 nullptr
 */
std::unique_ptr<IHumanRecognitionBackend> createBackendInstance(const QString& name);

/**
 * @brief 创建一个带配置的后端实例
 * @param name 后端名
 * @param config JSON 配置
 * @return 成功返回具体实现的唯一指针，失败返回 nullptr
 */
std::unique_ptr<IHumanRecognitionBackend> createBackendInstance(const QString& name,
                                                                const QJsonObject& config);

/**
 * @brief 清除所有已注册的后端工厂
 */
void clearRegisteredBackends();

/**
 * @brief 返回当前已注册的后端名称（稳定快照）
 *
 * 返回值可用于 UI 列表或调试输出。
 */
QVector<QString> listRegisteredBackends();

}  // namespace HumanRecognition
