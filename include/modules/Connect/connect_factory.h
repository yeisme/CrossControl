#pragma once

#include <QNetworkAccessManager>
#include <memory>

#include "iface_connect.h"

namespace connect_factory {

// 创建默认的 IConnect 实现（拥有指针）。
// 保持与历史兼容，默认仍为 TcpConnect
IConnectPtr createDefaultConnect();

// 根据给定 endpoint 或 uri 创建指定协议的实现。
// 支持形式：
//  - "tcp://host:port" 或 "host:port"(默认tcp)
//  - "udp://host:port" 或 "udp:host:port"
//  - "serial://COM3:115200" 或 "COM3:115200"
// 返回 nullptr 表示无法识别或创建
IConnectPtr createByEndpoint(const QString& endpoint);

/**
 * @brief Create a Network Access Manager object for HTTP/HTTPS requests.
 *
 * Returns a default, unconfigured QNetworkAccessManager wrapped in a shared_ptr.
 */
std::shared_ptr<QNetworkAccessManager> createNetworkAccessManager();

/**
 * @brief Create a Network Access Manager configured from an endpoint string.
 *
 * If the given endpoint appears to describe a proxy (for example:
 *  - "proxy://host:port" or "http-proxy://host:port"
 *  - "socks5://host:port" or "socks://host:port"
 *  - plain "host:port"
 * ), this function will set an appropriate QNetworkProxy on the returned
 * QNetworkAccessManager. If the endpoint is empty or does not look like a
 * proxy description, a default manager is returned (no proxy configured).
 */
std::shared_ptr<QNetworkAccessManager> createNetworkAccessManagerFromEndpoint(
    const QString& endpoint);

}  // namespace connect_factory
