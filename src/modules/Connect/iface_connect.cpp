#include "iface_connect.h"

#include "connect_factory.h"

IConnectPtr IConnect::createDefault() {
    return connect_factory::createDefaultConnect();
}

IConnectPtr IConnect::createFromEndpoint(const QString& endpoint) {
    return connect_factory::createByEndpoint(endpoint);
}
