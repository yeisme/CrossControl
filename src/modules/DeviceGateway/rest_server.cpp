#include "modules/DeviceGateway/rest_server.h"

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <atomic>
#include <memory>
#include <thread>

#include "modules/DeviceGateway/device_registry.h"
#include "spdlog/spdlog.h"

// 要求项目中必须提供 unofficial-uwebsockets（uWebSockets）头文件。
// 如果找不到头文件，使用 #error 在配置或编译阶段直接失败，避免静默回退。
#if __has_include(<uwebsockets/App.h>)
#define HAVE_UWS 1
#include <uwebsockets/App.h>
#else
#define HAVE_UWS 0
#endif

#if !HAVE_UWS
#error \
    "uWebSockets headers not found; REST endpoints require unofficial-uwebsockets. Configure and enable it in CMake."
#endif

namespace DeviceGateway {

// Impl: RestServer 的内部实现细节。封装 registry、监听端口和运行线程。
// 该结构仅对本 cpp 文件可见，避免泄露内部实现到公有头文件。
struct RestServer::Impl {
    DeviceRegistry& registry;  // 设备注册表（线程安全）
    unsigned short port;       // REST 服务监听端口
    std::thread worker;        // 运行 uWS::App 的后台线程
    std::atomic<bool> running{false};

    Impl(DeviceRegistry& r, unsigned short p) : registry(r), port(p) {}
};

RestServer::RestServer(DeviceRegistry& registry, unsigned short port)
    : impl_(std::make_unique<Impl>(registry, port)) {}

RestServer::~RestServer() {
    stop();
}

// start(): 启动 REST 服务的后台线程并运行 uWebSockets 的事件循环。
// 注意：uWS::App::run() 是阻塞的，因此我们在单独线程中运行它。
void RestServer::start() {
    if (impl_->running.load()) return;
    impl_->running.store(true);
    impl_->worker = std::thread([this]() {
        try {
            spdlog::info("RestServer starting on port {}", impl_->port);

            uWS::App app;

            // upsertHandler: 处理 POST /register 和 POST /devices
            // 接收 JSON body，解析为 DeviceInfo 并 upsert 到 DeviceRegistry。
            // 输入: JSON 对象 { deviceId, hwInfo, firmwareVersion, owner, group, lastSeen }
            // 输出: 成功返回文本 "OK\n"，失败返回 "Invalid JSON" 或 "Missing deviceId"。
            auto upsertHandler = [this](auto* res, auto* req) {
                auto body = std::make_shared<std::string>();
                res->onData([this, res, body](std::string_view chunk, bool last) {
                    body->append(chunk.data(), chunk.size());
                    if (!last) return;
                    QJsonParseError err;
                    QJsonDocument doc =
                        QJsonDocument::fromJson(QByteArray::fromStdString(*body), &err);
                    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
                        res->end("Invalid JSON\n");
                        return;
                    }
                    DeviceInfo info = DeviceInfo::fromJson(doc.object());
                    if (info.deviceId.isEmpty()) {
                        res->end("Missing deviceId\n");
                        return;
                    }
                    try {
                        impl_->registry.upsert(info);
                        res->end("OK\n");
                    } catch (const std::exception& ex) {
                        spdlog::error("RestServer: upsert exception: {}", ex.what());
                        res->end("Internal error\n");
                    }
                });
            };

            // 向后兼容的注册接口
            app.post("/register", upsertHandler);
            // 更通用的设备管理接口：新增/更新设备
            app.post("/devices", upsertHandler);

            // GET /devices -> 返回所有设备的 JSON 数组，便于前端加载设备列表
            app.get("/devices", [this](auto* res, auto* req) {
                QList<DeviceInfo> list = impl_->registry.list();
                QJsonArray arr;
                for (const auto& d : list) arr.append(d.toJson());
                QJsonDocument doc(arr);
                QByteArray out = doc.toJson(QJsonDocument::Compact);
                res->end(std::string(out.constData(), out.size()));
            });

            // GET /devices.csv -> 导出 CSV 文本，第一行为表头，供导出使用
            app.get("/devices.csv", [this](auto* res, auto* req) {
                QString csv = impl_->registry.exportCsv();
                QByteArray out = csv.toUtf8();
                res->end(std::string(out.constData(), out.size()));
            });

            // GET /devices.jsonnd -> 导出 ND-JSON（每行一个 JSON 对象），方便批量导入/流水线处理
            app.get("/devices.jsonnd", [this](auto* res, auto* req) {
                QString nd = impl_->registry.exportJsonNd();
                QByteArray out = nd.toUtf8();
                res->end(std::string(out.constData(), out.size()));
            });

            // POST /devices/import/csv -> 接收 CSV 文本并导入，返回 { imported: n }
            // 前端可以直接上传 CSV 文件内容到此接口完成批量导入
            app.post("/devices/import/csv", [this](auto* res, auto* req) {
                auto body = std::make_shared<std::string>();
                res->onData([this, res, body](std::string_view chunk, bool last) {
                    body->append(chunk.data(), chunk.size());
                    if (!last) return;
                    QString csv = QString::fromStdString(*body);
                    int imported = impl_->registry.importCsv(csv);
                    QJsonObject outObj;
                    outObj["imported"] = imported;
                    QJsonDocument doc(outObj);
                    QByteArray out = doc.toJson(QJsonDocument::Compact);
                    res->end(std::string(out.constData(), out.size()));
                });
            });

            // POST /devices/import/jsonnd -> 导入 newline-delimited JSON（每行一个 JSON 对象）
            app.post("/devices/import/jsonnd", [this](auto* res, auto* req) {
                auto body = std::make_shared<std::string>();
                res->onData([this, res, body](std::string_view chunk, bool last) {
                    body->append(chunk.data(), chunk.size());
                    if (!last) return;
                    QString nd = QString::fromStdString(*body);
                    int imported = impl_->registry.importJsonNd(nd);
                    QJsonObject outObj;
                    outObj["imported"] = imported;
                    QJsonDocument doc(outObj);
                    QByteArray out = doc.toJson(QJsonDocument::Compact);
                    res->end(std::string(out.constData(), out.size()));
                });
            });

            // GET /devices/{id} -> 返回指定设备信息（JSON），若未找到返回 "Not found"
            app.get("/devices/*", [this](auto* res, auto* req) {
                auto url = std::string(req->getUrl().data(), req->getUrl().size());
                const std::string prefix = "/devices/";
                if (url.rfind(prefix, 0) == 0) {
                    std::string id = url.substr(prefix.size());
                    QString qid = QString::fromStdString(id);
                    auto opt = impl_->registry.get(qid);
                    if (!opt) {
                        res->end("Not found\n");
                        return;
                    }
                    QJsonDocument doc(opt->toJson());
                    QByteArray out = doc.toJson(QJsonDocument::Compact);
                    res->end(std::string(out.constData(), out.size()));
                    return;
                }
                res->end("Not Found\n");
            });

            // DELETE /devices/{id} -> 删除设备，返回 OK 或 Not Found
            app.del("/devices/*", [this](auto* res, auto* req) {
                auto url = std::string(req->getUrl().data(), req->getUrl().size());
                const std::string prefix = "/devices/";
                if (url.rfind(prefix, 0) == 0) {
                    std::string id = url.substr(prefix.size());
                    QString qid = QString::fromStdString(id);
                    bool ok = impl_->registry.remove(qid);
                    if (ok)
                        res->end("OK\n");
                    else
                        res->end("Not Found\n");
                    return;
                }
                res->end("Not Found\n");
            });

            // 监听端口并运行事件循环；listen 回调会打印监听状态
            app.listen(impl_->port,
                       [&](auto* listen_socket) {
                           if (listen_socket) {
                               spdlog::info("RestServer listening on port {}", impl_->port);
                           } else {
                               spdlog::error("RestServer failed to listen on port {}", impl_->port);
                           }
                       })
                .run();
        } catch (const std::exception& ex) { spdlog::error("RestServer exception: {}", ex.what()); }
        impl_->running.store(false);
    });
}

// stop(): 请求停止服务并 join 后台线程。
// 说明：uWebSockets 的 run() 是阻塞的；如果需要在运行时优雅停止，应当在 start()
// 中保存可用于停止的 loop/app 对象并在此调用其 shutdown API。当前实现为
// best-effort：join 后台线程（如果库没有提供 stop 接口则可能需依赖进程退出）。
void RestServer::stop() {
    if (!impl_->running.load()) return;
    spdlog::info("RestServer stop requested");
    if (impl_->worker.joinable()) impl_->worker.join();
}

bool RestServer::running() const {
    return impl_->running.load();
}

}  // namespace DeviceGateway
