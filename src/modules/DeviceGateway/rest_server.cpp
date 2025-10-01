
#include "modules/DeviceGateway/rest_server.h"

#ifdef HAS_DROGON
#include <drogon/drogon.h>

#include <atomic>
#include <memory>
#include <thread>
#include "modules/DeviceGateway/device_gateway.h"
#include "spdlog/spdlog.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QVariantMap>
#include <QString>
#include <QByteArray>
#include <string>
#include "logging/logging.h"

namespace DeviceGateway {

static std::unique_ptr<std::thread> g_drogonThread;
static std::atomic<bool> g_running{false};
static DeviceGateway* g_gateway = nullptr;
static std::atomic<bool> g_handlers_registered{false};
// Drogon does not support run()/quit()/run() cycles reliably in the same
// process. Track whether we've ever started the drogon app; if so, do not
// attempt to start it again after stopping.
static std::atomic<bool> g_started_once{false};

static void registerHandlers() {
    // Ensure handlers are registered only once. Drogo n asserts if handlers are
    // registered multiple times across stop/start cycles.
    bool expected = false;
    if (!g_handlers_registered.compare_exchange_strong(expected, true)) {
        // already registered
        return;
    }
    using namespace drogon;
    // obtain named REST logger so handler logs go to REST and (via LoggerManager)
    // also to the global Qt sink used by the main LogWidget.
    auto restLogger = logging::LoggerManager::instance().getLogger("REST");

    // GET /devices
        app().registerHandler(
            "/devices",
            [restLogger](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
            auto resp = HttpResponse::newHttpResponse();
            if (!g_gateway) {
                resp->setStatusCode(k500InternalServerError);
                resp->setContentTypeCode(CT_APPLICATION_JSON);
                resp->setBody("{\"error\":\"no gateway\"}");
                callback(resp);
                return;
            }
            const auto devs = g_gateway->loadDevicesFromStorage();
            QJsonArray arr;
            for (const auto& d : devs) {
                QJsonObject jo;
                jo["id"] = QJsonValue(d.id);
                jo["name"] = QJsonValue(d.name);
                jo["status"] = QJsonValue(d.status);
                jo["endpoint"] = QJsonValue(d.endpoint);
                jo["type"] = QJsonValue(d.type);
                jo["hw_info"] = QJsonValue(d.hw_info);
                jo["firmware_version"] = QJsonValue(d.firmware_version);
                jo["owner"] = QJsonValue(d.owner);
                jo["group"] = QJsonValue(d.group);
                if (d.lastSeen.isValid()) jo["last_seen"] = static_cast<qint64>(d.lastSeen.toSecsSinceEpoch());
                QJsonObject meta;
                for (auto it = d.metadata.constBegin(); it != d.metadata.constEnd(); ++it) {
                    meta[it.key()] = QJsonValue(it.value().toString());
                }
                jo["metadata"] = meta;
                arr.append(jo);
            }
            QJsonObject root;
            root["devices"] = arr;
            const QByteArray out = QJsonDocument(root).toJson(QJsonDocument::Compact);
            resp->setStatusCode(k200OK);
            resp->setContentTypeCode(CT_APPLICATION_JSON);
            resp->setBody(out.toStdString());
            callback(resp);
        });

    // POST /devices
        app().registerHandler(
            "/devices",
            [restLogger](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
            auto resp = HttpResponse::newHttpResponse();
            if (!g_gateway) {
                resp->setStatusCode(k500InternalServerError);
                resp->setContentTypeCode(CT_APPLICATION_JSON);
                resp->setBody("{\"error\":\"no gateway\"}");
                callback(resp);
                return;
            }
            restLogger->info(std::string("REST POST /devices from ") + std::string(req->getPeerAddr().toIp()));
            const auto bodyView = req->getBody();
            const std::string bodyStr(bodyView.data(), bodyView.size());
            QJsonParseError perr;
            const QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(bodyStr), &perr);
            if (perr.error != QJsonParseError::NoError || !doc.isObject()) {
                restLogger->warn(std::string("REST POST /devices invalid JSON: ") + perr.errorString().toStdString());
                resp->setStatusCode(k400BadRequest);
                resp->setContentTypeCode(CT_APPLICATION_JSON);
                QJsonObject err;
                err["error"] = QString("invalid_json");
                err["message"] = QString("%1").arg(perr.errorString());
                resp->setBody(QJsonDocument(err).toJson(QJsonDocument::Compact).toStdString());
                callback(resp);
                return;
            }
            QJsonObject body = doc.object();
            // validation: require non-empty id
            QJsonArray errsArr;
            if (!body.contains("id") || !body.value("id").isString() || body.value("id").toString().trimmed().isEmpty()) {
                QJsonObject ve;
                ve["field"] = QString("id");
                ve["message"] = QString("missing or empty");
                errsArr.append(ve);
            }
            if (!errsArr.isEmpty()) {
                QStringList flat;
                for (const auto& v : errsArr) flat << v.toObject().value("field").toString() + ": " + v.toObject().value("message").toString();
                restLogger->warn(std::string("REST POST /devices validation failed: ") + flat.join(", ").toStdString());
                resp->setStatusCode(k422UnprocessableEntity);
                resp->setContentTypeCode(CT_APPLICATION_JSON);
                QJsonObject err;
                err["errors"] = errsArr;
                resp->setBody(QJsonDocument(err).toJson(QJsonDocument::Compact).toStdString());
                callback(resp);
                return;
            }
            DeviceInfo d;
            d.id = body.value("id").toString();
            d.name = body.value("name").toString();
            d.status = body.value("status").toString();
            d.endpoint = body.value("endpoint").toString();
            d.type = body.value("type").toString();
            d.hw_info = body.value("hw_info").toString();
            d.firmware_version = body.value("firmware_version").toString();
            d.owner = body.value("owner").toString();
            d.group = body.value("group").toString();
            if (body.contains("metadata") && body.value("metadata").isObject()) {
                QVariantMap mm;
                const QJsonObject meta = body.value("metadata").toObject();
                for (auto it = meta.constBegin(); it != meta.constEnd(); ++it) mm.insert(it.key(), it.value().toVariant());
                d.metadata = mm;
            }
            if (!g_gateway->addDeviceWithConnect(d)) {
                restLogger->error(std::string("REST POST /devices failed to add device ") + d.id.toStdString());
                resp->setStatusCode(k500InternalServerError);
                resp->setContentTypeCode(CT_APPLICATION_JSON);
                resp->setBody("{\"error\":\"failed to add device\"}");
            } else {
                resp->setStatusCode(k200OK);
                resp->setContentTypeCode(CT_APPLICATION_JSON);
                resp->setBody("{\"result\":\"ok\"}");
            }
            callback(resp);
    }, {Post});

    // PUT/POST /devices/{id}
    app().registerHandler(
        R"(/devices/{id})",
            [restLogger](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
            auto resp = HttpResponse::newHttpResponse();
            if (!g_gateway) {
                resp->setStatusCode(k500InternalServerError);
                resp->setContentTypeCode(CT_APPLICATION_JSON);
                resp->setBody("{\"error\":\"no gateway\"}");
                callback(resp);
                return;
            }
            const auto idView = req->getParameter("id");
            const std::string idStr(idView.data(), idView.size());
            const auto bodyView = req->getBody();
            const std::string bodyStr(bodyView.data(), bodyView.size());
            QJsonParseError perr;
            const QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(bodyStr), &perr);
            if (perr.error != QJsonParseError::NoError || !doc.isObject()) {
                restLogger->warn(std::string("REST PUT/POST /devices/{id} invalid JSON: ") + perr.errorString().toStdString());
                resp->setStatusCode(k400BadRequest);
                resp->setContentTypeCode(CT_APPLICATION_JSON);
                QJsonObject err;
                err["error"] = QString("invalid_json");
                err["message"] = QString("%1").arg(perr.errorString());
                resp->setBody(QJsonDocument(err).toJson(QJsonDocument::Compact).toStdString());
                callback(resp);
                return;
            }
            QJsonObject body = doc.object();
            // validate path id
            if (QString::fromStdString(idStr).trimmed().isEmpty()) {
                resp->setStatusCode(k400BadRequest);
                resp->setContentTypeCode(CT_APPLICATION_JSON);
                QJsonObject err;
                err["error"] = QString("missing_id");
                err["message"] = QString("missing id in path");
                resp->setBody(QJsonDocument(err).toJson(QJsonDocument::Compact).toStdString());
                callback(resp);
                return;
            }
            DeviceInfo d;
            d.id = QString::fromStdString(idStr);
            d.name = body.value("name").toString();
            d.status = body.value("status").toString();
            d.endpoint = body.value("endpoint").toString();
            d.type = body.value("type").toString();
            d.hw_info = body.value("hw_info").toString();
            d.firmware_version = body.value("firmware_version").toString();
            d.owner = body.value("owner").toString();
            d.group = body.value("group").toString();
            if (body.contains("metadata") && body.value("metadata").isObject()) {
                QVariantMap mm;
                const QJsonObject meta = body.value("metadata").toObject();
                for (auto it = meta.constBegin(); it != meta.constEnd(); ++it) mm.insert(it.key(), it.value().toVariant());
                d.metadata = mm;
            }
            if (!g_gateway->persistDevice(d)) {
                restLogger->error(std::string("REST PUT/POST /devices/{id} failed to persist device ") + d.id.toStdString());
                resp->setStatusCode(k500InternalServerError);
                resp->setContentTypeCode(CT_APPLICATION_JSON);
                resp->setBody("{\"error\":\"failed to persist device\"}");
            } else {
                resp->setStatusCode(k200OK);
                resp->setContentTypeCode(CT_APPLICATION_JSON);
                resp->setBody("{\"result\":\"ok\"}");
            }
            callback(resp);
    }, {Post, Put});

    // DELETE /devices/{id}
    app().registerHandler(
        R"(/devices/{id})",
            [restLogger](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
            auto resp = HttpResponse::newHttpResponse();
            if (!g_gateway) {
                resp->setStatusCode(k500InternalServerError);
                resp->setContentTypeCode(CT_APPLICATION_JSON);
                resp->setBody("{\"error\":\"no gateway\"}");
                callback(resp);
                return;
            }
            const auto idView = req->getParameter("id");
            const std::string idStr(idView.data(), idView.size());
            if (QString::fromStdString(idStr).trimmed().isEmpty()) {
                resp->setStatusCode(k400BadRequest);
                resp->setContentTypeCode(CT_APPLICATION_JSON);
                resp->setBody("{\"error\":\"missing id\"}");
                callback(resp);
                return;
            }
            if (!g_gateway->removeDeviceAndClose(QString::fromStdString(idStr))) {
                restLogger->error(std::string("REST DELETE /devices/{id} failed to delete ") + idStr);
                resp->setStatusCode(k500InternalServerError);
                resp->setContentTypeCode(CT_APPLICATION_JSON);
                QJsonObject err;
                err["error"] = QString("failed_to_delete");
                err["message"] = QString("failed to delete device");
                resp->setBody(QJsonDocument(err).toJson(QJsonDocument::Compact).toStdString());
            } else {
                resp->setStatusCode(k200OK);
                resp->setContentTypeCode(CT_APPLICATION_JSON);
                QJsonObject ok;
                ok["result"] = QString("ok");
                resp->setBody(QJsonDocument(ok).toJson(QJsonDocument::Compact).toStdString());
            }
            callback(resp);
    }, {Delete});
}

RestServer::RestServer(DeviceGateway* gw) { m_gateway = gw; }
RestServer::~RestServer() { stop(); }

bool RestServer::start(unsigned short port) {
    if (g_running.load()) return true;
    if (!m_gateway) return false;
    g_gateway = m_gateway;
    // If we've already started and stopped once, do not attempt to restart
    // drogon in this process: it can assert internally. Require process
    // restart instead.
    if (g_started_once.load()) {
        spdlog::error("RestServer: drogon was already started once and cannot be restarted in the same process");
        return false;
    }
    try {
        registerHandlers();
        drogon::app().addListener("0.0.0.0", port);
    } catch (const std::exception& e) {
    spdlog::error(std::string("RestServer start failed: ") + std::string(e.what()));
        return false;
    } catch (...) {
        spdlog::error("RestServer start failed: unknown exception");
        return false;
    }
    g_drogonThread = std::make_unique<std::thread>([]() { drogon::app().run(); });
    g_running.store(true);
    g_started_once.store(true);
        logging::LoggerManager::instance().getLogger("REST")->info(
            std::string("RestServer: drogon listening on port ") + std::to_string(port));
    return true;
}

void RestServer::stop() {
    if (!g_running.load()) return;
    try {
        drogon::app().quit();
    } catch (...) {
    }
    if (g_drogonThread && g_drogonThread->joinable()) {
        g_drogonThread->join();
        g_drogonThread.reset();
    }
    g_running.store(false);
    spdlog::info("RestServer: stopped");
}

bool RestServer::isRunning() const { return g_running.load(); }

void RestServer::setGateway(DeviceGateway* gw) { m_gateway = gw; }

}  // namespace DeviceGateway

#else

// If we compiled without Drogon, the project should not include this file anymore.
#error "RestServer requires Drogon (define HAS_DROGON)"

#endif
