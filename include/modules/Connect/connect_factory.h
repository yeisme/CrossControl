#pragma once

#include <QNetworkAccessManager>
#include <memory>

#include "iface_connect.h"

#ifdef HAS_DROGON
#include <drogon/drogon.h>
#endif

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

#ifdef HAS_DROGON
/**
 * @brief 创建 drogon::HttpClient（如果已启用 DROGON）。
 * @param baseUrl 例如 "http://127.0.0.1:8080" 或 "https://example.com"
 */
std::shared_ptr<drogon::HttpClient> createHttpClient(const QString& baseUrl);
#endif

}  // namespace connect_factory
