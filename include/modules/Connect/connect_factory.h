#pragma once

#include "iface_connect.h"

namespace connect_factory {

// 创建默认的 IConnect 实现（拥有指针）。
// 当前为 TcpConnect 的 unique_ptr。
IConnectPtr createDefaultConnect();

}  // namespace connect_factory
