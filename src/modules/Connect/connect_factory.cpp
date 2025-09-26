#include "modules/Connect/connect_factory.h"

#include "modules/Connect/tcp_connect.h"

namespace connect_factory {

IConnectPtr createDefaultConnect() {
    return std::make_unique<TcpConnect>();
}

}  // namespace connect_factory
