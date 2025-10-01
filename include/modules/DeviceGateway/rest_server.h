#pragma once

// Minimal rest server placeholder API so DeviceGateway can optionally use
// an HTTP server implementation (drogon) without hard dependency in headers.
namespace DeviceGateway {

class DeviceGateway;  // forward

class RestServer {
   public:
    RestServer(DeviceGateway* gw = nullptr);
    ~RestServer();

    bool start(unsigned short port = 8080);
    void stop();
    bool isRunning() const;

    void setGateway(DeviceGateway* gw);
   private:
    // no-op private; implementation uses drogon and module-global state
    DeviceGateway* m_gateway{nullptr};
};

}  // namespace DeviceGateway
