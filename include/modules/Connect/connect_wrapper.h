#pragma once

#include <concepts>
#include <memory>

#include "iface_connect_qt.h"

/**
 * @file connect_wrapper.h
 * @brief 简单工厂模板：通过模板参数创建 IConnectQt 派生对象
 *
 * ConnectFactory 用于在运行时隐藏具体实现类型，返回统一的
 * std::unique_ptr<IConnectQt> 以便 UI 或其它组件解耦具体的连接实现。
 */

/**
 * @concept ConnectImplConcept
 * @brief 约束 ConnectImpl 必须满足的条件
 *
 * 要求：
 *  - 必须从 IConnectQt 继承（派生类或相同类型），保证具有异步/同步接口。
 *  - 必须可以通过 QObject* 构造（构造函数签名为 (QObject*) 或 可隐式/显式接受 QObject*）。
 */
template <typename T>
concept ConnectImplConcept =
    std::derived_from<T, IConnectQt> && std::constructible_from<T, QObject*>;

template <ConnectImplConcept ConnectImpl>
class ConnectFactory {
   public:
    /**
     * @brief 创建指定实现的 IConnectQt 实例
     * @param parent QObject 父对象（可为 nullptr）
     * @return 返回封装在 unique_ptr 中的 IConnectQt 实例
     */
    static std::unique_ptr<IConnectQt> create(QObject* parent = nullptr) {
        return std::make_unique<ConnectImpl>(parent);
    }
};
