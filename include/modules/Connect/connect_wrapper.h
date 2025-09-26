#pragma once

/**
 * @file connect_wrapper.h
 * @author your name (you@domain.com)
 * @brief 连接工厂类
 * @version 0.1
 * @date 2025-09-26
 * ConnectFactory 用于在运行时隐藏具体实现类型，返回统一的
 * IConnectPtr 以便 UI 或其它组件解耦具体的连接实现。
 *
 * @copyright Copyright (c) 2025
 *
 */

#include <concepts>
#include <memory>

#include "iface_connect.h"

/**
 * @concept ConnectImplConcept
 * @brief 约束 ConnectImpl 必须满足的条件
 *
 * 要求：
 *  - 必须从 IConnect 继承（派生类或相同类型），实现默认构造。
 */
template <typename T>
concept ConnectImplConcept = std::derived_from<T, IConnect> && std::default_initializable<T>;

/**
 * @brief 连接工厂类
 *
 * @tparam ConnectImpl
 */
template <ConnectImplConcept ConnectImpl>
class ConnectFactory {
   public:
    /**
     * @brief 创建指定实现的 IConnect 实例
     * @return 返回封装在 unique_ptr 中的 IConnect 实例
     */
    static IConnectPtr create() { return std::make_unique<ConnectImpl>(); }
};
