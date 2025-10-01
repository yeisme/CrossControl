#include "modules/DeviceGateway/rest_server.h"

#include <drogon/HttpController.h>
#include <drogon/drogon.h>
#include <trantor/utils/Logger.h>

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <atomic>
#include <functional>
#include <memory>
#include <thread>
#include <mutex>

#include "modules/DeviceGateway/device_registry.h"
#include "spdlog/spdlog.h"

namespace DeviceGateway {

// 将 QString 写入 drogon 响应的帮助函数
// 说明：drogon 使用 std::string 作为 HTTP body，工程内部使用 Qt 的 QString / QByteArray
// 本函数负责做编码转换并创建一个 drogon::HttpResponsePtr。所有处理函数统一通过该函数返回文本或 JSON
// 响应。 参数：
//   - text: 要写入响应的 QString 文本（通常已经是 UTF-8 可表示的内容）
//   - code: HTTP 状态码（默认 200 OK）
//   - contentType: Content-Type 字符串（包含 charset）
// 返回值：构造好的 drogon::HttpResponsePtr
static drogon::HttpResponsePtr makeTextResponse(
    const QString& text,
    drogon::HttpStatusCode code = drogon::k200OK,
    const std::string& contentType = "text/plain; charset=utf-8") {
    auto resp = drogon::HttpResponse::newHttpResponse();
    // 设置状态码和 Content-Type
    resp->setStatusCode(code);
    resp->setContentTypeString(contentType);
    // 将 QString 转为 UTF-8 字节序列，再拷贝到 std::string（drogon 的 body 类型）
    const QByteArray out = text.toUtf8();
    resp->setBody(std::string(out.constData(), static_cast<size_t>(out.size())));
    return resp;
}

// Drogon 控制器定义，集中路由
// DGController: 使用 drogon::HttpController 实现所有 REST 路由
// 设计说明：
//  - 我们把所有 DeviceGateway 的 REST 路由集中在一个 Controller 中，便于管理路径和共享
//  DeviceRegistry 指针。
//  - 使用 drogon 的 ADD_METHOD_TO 宏将 URL 路径和 HTTP 方法绑定到成员函数。
//  - 这些处理函数以非阻塞回调形式接收请求和响应回调（std::function<void(const
//  HttpResponsePtr&)>）。
//  - Controller 内部不直接管理线程池；真正的事件循环由 drogon::app().run() 驱动（见
//  RestServer::start）。
class DGController : public drogon::HttpController<DGController> {
   public:
    // Make the registry pointer atomic to avoid data-race when the main thread
    // clears the pointer while drogon worker threads may read it.
    static std::atomic<DeviceRegistry*> reg_;
    static void setRegistry(DeviceRegistry* r) { reg_.store(r, std::memory_order_release); }

    // 路由列表：通过 drogon 的宏将 URI 与成员函数绑定
    // 例如：POST /register -> postRegister
    // 占位语法 "/devices/{1}" 表示第一个 path segment 作为参数传递给对应的方法（方法签名须包含
    // std::string 参数）
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(DGController::postRegister, "/register", drogon::Post);
    ADD_METHOD_TO(DGController::postDevices, "/devices", drogon::Post);
    ADD_METHOD_TO(DGController::getDevices, "/devices", drogon::Get);
    ADD_METHOD_TO(DGController::getDevicesCsv, "/devices.csv", drogon::Get);
    ADD_METHOD_TO(DGController::getDevicesNd, "/devices.jsonnd", drogon::Get);
    ADD_METHOD_TO(DGController::postImportCsv, "/devices/import/csv", drogon::Post);
    ADD_METHOD_TO(DGController::postImportNd, "/devices/import/jsonnd", drogon::Post);
    ADD_METHOD_TO(DGController::getDeviceById, "/devices/{1}", drogon::Get);
    ADD_METHOD_TO(DGController::deleteDeviceById, "/devices/{1}", drogon::Delete);
    METHOD_LIST_END

    // POST /register: 设备上报或注册（复用 handleUpsert）
    // 注意：drogon 的回调模型是异步回调风格，函数内直接调用 cb(...) 即发送响应。
    void postRegister(const drogon::HttpRequestPtr& req,
                      std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        handleUpsert(req, std::move(cb));
    }
    void postDevices(const drogon::HttpRequestPtr& req,
                     std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        handleUpsert(req, std::move(cb));
    }
    // GET /devices: 返回全部设备的 JSON 数组
    // 流程：检查注册表指针 -> 从 DeviceRegistry 获取设备列表 -> 将每个 DeviceInfo 转为 QJsonObject
    // -> 序列化为 JSON 注意：这里将 Qt 的 JSON 文档转换为 QByteArray，再转换为 std::string 放入
    // drogon 响应体
    void getDevices(const drogon::HttpRequestPtr& /*req*/,
                    std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        DeviceRegistry* r = reg_.load(std::memory_order_acquire);
        if (!r) {
            cb(makeTextResponse("Registry not ready\n", drogon::k503ServiceUnavailable));
            return;
        }
        // 从 registry 获取当前设备列表（返回 QList<DeviceInfo>）
        const QList<DeviceInfo> list = r->list();
        QJsonArray arr;
        for (const auto& d : list) arr.append(d.toJson());
        const QJsonDocument doc(arr);
        const QByteArray out = doc.toJson(QJsonDocument::Compact);
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setContentTypeString("application/json; charset=utf-8");
        resp->setBody(std::string(out.constData(), static_cast<size_t>(out.size())));
        cb(resp);
    }
    void getDevicesCsv(const drogon::HttpRequestPtr& /*req*/,
                       std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        DeviceRegistry* r = reg_.load(std::memory_order_acquire);
        if (!r) {
            cb(makeTextResponse("Registry not ready\n", drogon::k503ServiceUnavailable));
            return;
        }
        const QString csv = r->exportCsv();
        cb(makeTextResponse(csv, drogon::k200OK, "text/csv; charset=utf-8"));
    }
    void getDevicesNd(const drogon::HttpRequestPtr& /*req*/,
                      std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        DeviceRegistry* r = reg_.load(std::memory_order_acquire);
        if (!r) {
            cb(makeTextResponse("Registry not ready\n", drogon::k503ServiceUnavailable));
            return;
        }
        const QString nd = r->exportJsonNd();
        cb(makeTextResponse(nd, drogon::k200OK, "application/x-ndjson; charset=utf-8"));
    }
    // POST /devices/import/csv: 接收 CSV 文本并导入到 DeviceRegistry
    // 关键点：drogon 的 getBody() 返回 string_view（引用底层缓冲区），这里要把它拷贝到 QByteArray /
    // QString 以便使用 Qt 的 CSV 解析逻辑。完成导入后返回导入计数（JSON 格式）。
    void postImportCsv(const drogon::HttpRequestPtr& req,
                       std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        DeviceRegistry* r = reg_.load(std::memory_order_acquire);
        if (!r) {
            cb(makeTextResponse("Registry not ready\n", drogon::k503ServiceUnavailable));
            return;
        }
        const auto sv = req->getBody();
        // 注意：sv.data() 可能不是以 '\0' 结尾，使用 fromUtf8(data, size) 来构造 QString
        const QString csv = QString::fromUtf8(sv.data(), static_cast<int>(sv.size()));
        const int imported = r->importCsv(csv);
        QJsonObject outObj;
        outObj["imported"] = imported;
        const QJsonDocument doc(outObj);
        const QByteArray out = doc.toJson(QJsonDocument::Compact);
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setContentTypeString("application/json; charset=utf-8");
        resp->setBody(std::string(out.constData(), static_cast<size_t>(out.size())));
        cb(resp);
    }
    void postImportNd(const drogon::HttpRequestPtr& req,
                      std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        DeviceRegistry* r = reg_.load(std::memory_order_acquire);
        if (!r) {
            cb(makeTextResponse("Registry not ready\n", drogon::k503ServiceUnavailable));
            return;
        }
        const auto sv = req->getBody();
        const QString nd = QString::fromUtf8(sv.data(), static_cast<int>(sv.size()));
        const int imported = r->importJsonNd(nd);
        QJsonObject outObj;
        outObj["imported"] = imported;
        const QJsonDocument doc(outObj);
        const QByteArray out = doc.toJson(QJsonDocument::Compact);
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setContentTypeString("application/json; charset=utf-8");
        resp->setBody(std::string(out.constData(), static_cast<size_t>(out.size())));
        cb(resp);
    }
    // GET /devices/{id}: 按 ID 查询设备信息
    // 路由中的 {1} 占位符会把路径段作为 std::string 参数传入（此处为 id）
    void getDeviceById(const drogon::HttpRequestPtr& /*req*/,
                       std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                       std::string id) {
        DeviceRegistry* r = reg_.load(std::memory_order_acquire);
        if (!r) {
            cb(makeTextResponse("Registry not ready\n", drogon::k503ServiceUnavailable));
            return;
        }
        const QString qid = QString::fromStdString(id);
        auto opt = r->get(qid);
        if (!opt) {
            cb(makeTextResponse("Not Found\n", drogon::k404NotFound));
            return;
        }
        const QJsonDocument doc(opt->toJson());
        const QByteArray out = doc.toJson(QJsonDocument::Compact);
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setContentTypeString("application/json; charset=utf-8");
        resp->setBody(std::string(out.constData(), static_cast<size_t>(out.size())));
        cb(resp);
    }
    // DELETE /devices/{id}: 删除指定设备
    void deleteDeviceById(const drogon::HttpRequestPtr& /*req*/,
                          std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                          std::string id) {
        DeviceRegistry* r = reg_.load(std::memory_order_acquire);
        if (!r) {
            cb(makeTextResponse("Registry not ready\n", drogon::k503ServiceUnavailable));
            return;
        }
        const QString qid = QString::fromStdString(id);
        const bool ok = r->remove(qid);
        if (ok)
            cb(makeTextResponse("OK\n", drogon::k200OK));
        else
            cb(makeTextResponse("Not Found\n", drogon::k404NotFound));
    }

   private:
    // 处理注册或上报的通用函数（POST /register 和 POST /devices 复用）
    // 步骤：
    //  1. 检查 registry 是否就绪
    //  2. 从请求体读取原始字节（drogon 返回 string_view），拷贝为 QByteArray 以交给 Qt JSON 解析
    //  3. 解析 JSON，验证必需字段（deviceId）
    //  4. 将 DeviceInfo 写入 registry（upsert），并处理可能的异常
    // 注意：此函数为静态私有函数，所有路由共享同一套逻辑，避免代码重复
    static void handleUpsert(const drogon::HttpRequestPtr& req,
                             std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        if (!reg_) {
            cb(makeTextResponse("Registry not ready\n", drogon::k503ServiceUnavailable));
            return;
        }
        // drogon::HttpRequest::getBody() 返回 string_view，引用底层缓冲区
        // 这里需要把内容拷贝到 QByteArray（或 std::string）以便 Qt 的 JSON 解析器使用
        const auto sv = req->getBody();
        const QByteArray body(sv.data(), static_cast<int>(sv.size()));
        QJsonParseError err;
        const QJsonDocument doc = QJsonDocument::fromJson(body, &err);
        if (err.error != QJsonParseError::NoError || !doc.isObject()) {
            // 返回 400：无效的 JSON
            cb(makeTextResponse("Invalid JSON\n", drogon::k400BadRequest));
            return;
        }
        // 将 JSON 对象转换为内部 DeviceInfo 表示
        const DeviceInfo info = DeviceInfo::fromJson(doc.object());
        if (info.deviceId.isEmpty()) {
            // deviceId 是必须字段
            cb(makeTextResponse("Missing deviceId\n", drogon::k400BadRequest));
            return;
        }
        try {
            // 写入 registry（可能触发磁盘/数据库持久化，视 DeviceRegistry 实现而定）
            DeviceRegistry* r = reg_.load(std::memory_order_acquire);
            if (!r) {
                cb(makeTextResponse("Registry not ready\n", drogon::k503ServiceUnavailable));
                return;
            }
            r->upsert(info);
            cb(makeTextResponse("OK\n"));
        } catch (const std::exception& ex) {
            // 捕获异常并记录日志，避免抛出到 drogon 线程导致进程不稳定
            spdlog::error("RestServer: upsert exception: {}", ex.what());
            cb(makeTextResponse("Internal error\n", drogon::k500InternalServerError));
        }
    }
};

std::atomic<DeviceRegistry*> DGController::reg_{nullptr};

// Impl: RestServer 内部实现
struct RestServer::Impl {
    DeviceRegistry& registry;          // 设备注册表
    unsigned short port;               // 监听端口
    std::thread worker;                // 工作线程
    std::atomic<bool> running{false};  // 是否正在运行
    std::mutex mutex;                  // protect start/stop transitions
    // optional callbacks
    std::function<void()> onStarted;
    std::function<void()> onStopped;
    std::function<void(RestServer::State)> onStateChanged;
    std::atomic<RestServer::State> state{RestServer::State::Stopped};

    Impl(DeviceRegistry& r, unsigned short p) : registry(r), port(p) {}
};

RestServer::RestServer(DeviceRegistry& registry, unsigned short port)
    : impl_(std::make_unique<Impl>(registry, port)) {}

RestServer::~RestServer() {
    stop();
}

void RestServer::start() {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->running.load()) return;
    // transition to Starting
    impl_->state.store(State::Starting);
    if (impl_->onStateChanged) impl_->onStateChanged(State::Starting);
    impl_->running.store(true);
    impl_->worker = std::thread([this]() {
        try {
            // std::thread::id is not directly formattable by fmt/spdlog on all
            // platforms. Convert to a stable numeric value via std::hash for
            // logging purposes.
            const std::thread::id this_tid = std::this_thread::get_id();
            const auto tid_hash = std::hash<std::thread::id>{}(this_tid);
            spdlog::info("RestServer (drogon) starting on port {} (thread id={})", impl_->port,
                         tid_hash);

            try {
                spdlog::info("RestServer: setting DGController registry pointer");
                DGController::setRegistry(&impl_->registry);
            } catch (const std::exception& ex) {
                spdlog::error("RestServer: exception while setting registry: {}", ex.what());
            } catch (...) {
                spdlog::error("RestServer: unknown exception while setting registry");
            }

            try {
                spdlog::info("RestServer: configuring drogon listener on port {}", impl_->port);
                drogon::app().setLogLevel(trantor::Logger::kInfo);
                drogon::app().addListener("0.0.0.0", static_cast<uint16_t>(impl_->port)).setThreadNum(1);
                spdlog::info("RestServer: drogon listener configured");
            } catch (const std::exception& ex) {
                spdlog::error("RestServer: exception while configuring drogon: {}", ex.what());
            } catch (...) {
                spdlog::error("RestServer: unknown exception while configuring drogon");
            }

            // notify started (worker thread) after listener configured but
            // before run() to indicate we're about to enter the event loop.
            try {
                impl_->state.store(State::Running);
                if (impl_->onStateChanged) impl_->onStateChanged(State::Running);
                if (impl_->onStarted) impl_->onStarted();
            } catch (const std::exception& ex) {
                spdlog::error("RestServer: onStarted callback threw exception: {}", ex.what());
            } catch (...) {
                spdlog::error("RestServer: onStarted callback threw unknown exception");
            }

            try {
                spdlog::info("RestServer: entering drogon event loop");
                drogon::app().run();
            } catch (const std::exception& ex) {
                spdlog::error("RestServer: exception during drogon::run(): {}", ex.what());
            } catch (...) {
                spdlog::error("RestServer: unknown exception during drogon::run()");
            }

            spdlog::info("RestServer (drogon) stopped");
            // transition to Stopped (worker thread)
            impl_->state.store(State::Stopped);
            if (impl_->onStateChanged) impl_->onStateChanged(State::Stopped);
            // notify stopped (worker thread) after exit from run()
            try {
                if (impl_->onStopped) impl_->onStopped();
            } catch (const std::exception& ex) {
                spdlog::error("RestServer: onStopped callback threw exception: {}", ex.what());
            } catch (...) {
                spdlog::error("RestServer: onStopped callback threw unknown exception");
            }
        } catch (const std::exception& ex) {
            spdlog::error("RestServer (drogon) exception: {}", ex.what());
        }
        impl_->running.store(false);
    });
}

void RestServer::stop() {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (!impl_->running.load()) return;
    // transition to Stopping
    impl_->state.store(State::Stopping);
    if (impl_->onStateChanged) impl_->onStateChanged(State::Stopping);
    spdlog::info("RestServer (drogon) stop requested");
    // Clear the global registry pointer first so that any incoming handler
    // invoked concurrently will safely see a null registry and return a 503
    // instead of dereferencing a potentially destroyed object.
    DGController::setRegistry(nullptr);
    drogon::app().quit();                                // 停止 drogon 事件循环
    if (impl_->worker.joinable()) impl_->worker.join();  // 等待线程结束
    impl_->running.store(false);
    spdlog::info("RestServer (drogon) stopped");
    // invoke stopped callback (in caller thread)
    if (impl_->onStopped) impl_->onStopped();
    // ensure state is Stopped and notify caller thread as well
    impl_->state.store(State::Stopped);
    if (impl_->onStateChanged) impl_->onStateChanged(State::Stopped);
}

void RestServer::setOnStateChanged(std::function<void(State)> cb) { impl_->onStateChanged = std::move(cb); }

void RestServer::setOnStarted(std::function<void()> cb) { impl_->onStarted = std::move(cb); }
void RestServer::setOnStopped(std::function<void()> cb) { impl_->onStopped = std::move(cb); }

bool RestServer::running() const {
    return impl_->running.load();
}

}  // namespace DeviceGateway
