#pragma once

#include <QString>
#include <memory>

namespace DeviceGateway {

class DeviceRegistry;

// Lightweight REST server that can be started/stopped asynchronously from a
// Qt application. It uses unofficial-uwebsockets when available; otherwise the
// class compiles to a stub that returns errors at runtime.
class RestServer {
   public:
    RestServer(DeviceRegistry& registry, unsigned short port = 8080);
    ~RestServer();

    // start the server (non-blocking)
    void start();
    // stop the server and join worker thread
    void stop();

    // return whether the server is running
    bool running() const;

   private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace DeviceGateway
