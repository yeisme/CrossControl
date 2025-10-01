#pragma once

#include <QString>
#include <memory>
#include <functional>

namespace DeviceGateway {

class DeviceRegistry;

// Lightweight REST server that can be started/stopped asynchronously from a
// Qt application. Implemented using drogon HTTP framework.
class RestServer {
   public:
    // Server lifecycle state used for UI locking and diagnostics
    enum class State { Stopped = 0, Starting, Running, Stopping };

    RestServer(DeviceRegistry& registry, unsigned short port = 8080);
    ~RestServer();

    // start the server (non-blocking)
    void start();
    // stop the server and join worker thread
    void stop();

    // return whether the server is running
    bool running() const;

    // Set callbacks that are invoked when the server transitions to running
    // or stopped states. Callbacks are invoked from the server worker
    // thread; receivers should marshal to the Qt main thread if updating
    // UI. Passing a null std::function clears the callback.
    void setOnStarted(std::function<void()> cb);
    void setOnStopped(std::function<void()> cb);
    // Callback invoked whenever server state changes. Receivers should marshal
    // to the Qt main thread before touching UI. Passing nullptr clears it.
    void setOnStateChanged(std::function<void(State)> cb);

   private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace DeviceGateway
