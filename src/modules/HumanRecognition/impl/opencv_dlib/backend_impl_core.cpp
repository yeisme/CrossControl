#include "internal/backend_impl.h"

#if defined(HAS_OPENCV) && defined(HAS_DLIB)

#include <dlib/image_processing/shape_predictor.h>

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QVariant>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlError>
#include <QtSql/QSqlQuery>

#include "logging/logging.h"
#include "modules/Config/config.h"
#include "modules/Storage/dbmanager.h"

namespace HumanRecognition {

using namespace opencv_dlib;

OpenCVDlibBackend::Impl::Impl()
    : detector(dlib::get_frontal_face_detector()),
      shapePredictor(),
      recognitionNet(),
      mutex(),
      persons(),
      modelDirectory(),
      logger(logging::LoggerManager::instance().getLogger("HumanRecognition.OpenCVDlib")),
      personsTable(QString::fromLatin1(kDefaultPersonsTable)),
      matchThreshold(kDefaultMatchThreshold) {}

OpenCVDlibBackend::Impl::~Impl() = default;

/**
 * @brief 初始化后端，读取配置并加载人员缓存。
 *
 * 此函数确保数据库可用，并可根据配置自动加载模型与人员表。
 */
HRCode OpenCVDlibBackend::Impl::initialize(const QJsonObject& config) {
    if (logger) {
        logger->info("Initializing OpenCVDlibBackend with {} config entries",
                     static_cast<int>(config.size()));
    }
    // 先读取全局配置中的默认参数，例如数据库表名、匹配阈值等。
    refreshConfiguration();

    if (config.contains(QStringLiteral("personsTable"))) {
        // JSON 配置允许覆盖默认表名，需进行一次合法字符过滤以防 SQL 注入。
        personsTable = sanitizeTableName(config.value(QStringLiteral("personsTable")).toString());
        if (personsTable.isEmpty()) personsTable = QString::fromLatin1(kDefaultPersonsTable);
        config::ConfigManager::instance().setValue(QStringLiteral("HumanRecognition/PersonsTable"),
                                                   personsTable);
    }

    if (config.contains(QStringLiteral("matchThreshold"))) {
        // 当传入阈值小于 0 时回退到默认值，避免配置错误导致逻辑异常。
        matchThreshold = config.value(QStringLiteral("matchThreshold")).toDouble(matchThreshold);
        if (matchThreshold < 0.0) matchThreshold = kDefaultMatchThreshold;
        config::ConfigManager::instance().setValue(
            QStringLiteral("HumanRecognition/MatchThreshold"), matchThreshold);
    }

    if (config.contains(QStringLiteral("autoCreateTable"))) {
        // 插件级别的开关，可按需禁用自动建表（例如只读部署场景）。
        autoCreatePersonsTable =
            config.value(QStringLiteral("autoCreateTable")).toBool(autoCreatePersonsTable);
        config::ConfigManager::instance().setValue(
            QStringLiteral("HumanRecognition/AutoCreateTable"), autoCreatePersonsTable);
    }

    // 如果数据库不可用直接终止初始化，后续功能都依赖 Storage 模块。
    if (!ensureStorageReady()) { return HRCode::ModelLoadFailed; }

    if (config.contains(QStringLiteral("modelDirectory"))) {
        // 允许在配置中直接指定模型目录，便于无 UI 场景自动化启动。
        const QString dir = config.value(QStringLiteral("modelDirectory")).toString();
        if (!dir.isEmpty()) { return loadModel(dir); }
    }

    // 初始化完成后预加载一次人员缓存，保证随后的查询立即可用。
    const HRCode res = loadPersonsFromStorage();
    if (res != HRCode::Ok) return res;

    if (logger) {
        logger->info("OpenCVDlibBackend initialized. Persons table: {}, threshold: {}",
                     personsTable.toStdString(),
                     matchThreshold);
    }
    return HRCode::Ok;
}

/**
 * @brief 释放 dlib 网络与缓存资源，清理运行状态。
 */
HRCode OpenCVDlibBackend::Impl::shutdown() {
    std::scoped_lock lock(mutex);
    recognitionNet.reset();
    shapePredictor.reset();
    persons.clear();
    modelDirectory.clear();
    storageReady = false;
    if (logger) { logger->info("OpenCVDlibBackend shut down"); }
    return HRCode::Ok;
}

/**
 * @brief 加载 dlib 模型及旧版 JSON 人员库。
 *
 * @param modelPath 可以是目录或具体文件，内部会自动解析。
 */
HRCode OpenCVDlibBackend::Impl::loadModel(const QString& modelPath) {
    if (logger) { logger->info("Loading model from {}", modelPath.toStdString()); }
    QFileInfo info(modelPath);
    if (!info.exists()) {
        // 如果传入的是不存在的目录/文件，直接返回 ModelLoadFailed。
        if (logger) { logger->error("Model path not found: {}", modelPath.toStdString()); }
        return HRCode::ModelLoadFailed;
    }

    const ModelFiles files = resolveModelFiles(info);
    if (!QFileInfo::exists(files.shapePredictor) || !QFileInfo::exists(files.recognition)) {
        // 没有关键模型文件意味着当前目录不是完整的模型包。
        if (logger) {
            logger->error("Required model files missing. shape predictor = {}, recognition = {}",
                          files.shapePredictor.toStdString(),
                          files.recognition.toStdString());
        }
        return HRCode::ModelLoadFailed;
    }

    auto predictor = std::make_unique<dlib::shape_predictor>();
    auto net = std::make_unique<detail::FaceNet>();
    try {
        // 使用 dlib 自带的反序列化工具加载模型，若格式错误会抛出异常。
        dlib::deserialize(files.shapePredictor.toStdString()) >> *predictor;
        dlib::deserialize(files.recognition.toStdString()) >> *net;
    } catch (const std::exception& ex) {
        if (logger) { logger->error("Failed to deserialize models: {}", ex.what()); }
        return HRCode::ModelLoadFailed;
    }

    {
        // 写入共享成员需要加锁，避免其他线程正在进行识别导致 data race。
        std::scoped_lock lock(mutex);
        shapePredictor = std::move(predictor);
        recognitionNet = std::move(net);
        modelDirectory = files.baseDirectory;
    }

    // 加载模型完成后再次确保数据库可用（运行期间可能被关闭）。
    if (!ensureStorageReady()) return HRCode::ModelLoadFailed;

    // 每次加载模型后都刷新一次人员缓存，保证数据与数据库一致。
    HRCode res = loadPersonsFromStorage();
    if (res != HRCode::Ok) return res;

    if (!files.personDatabase.isEmpty() && QFileInfo::exists(files.personDatabase)) {
        // 兼容旧版本将人员信息保存在 JSON 中的情形，导入后仍然会刷新缓存。
        const HRCode importRes = loadPersonDatabase(files.personDatabase);
        if (importRes != HRCode::Ok && logger) {
            logger->warn("Failed to import legacy JSON database {}",
                         files.personDatabase.toStdString());
        }
    }

    if (logger) { logger->info("Loaded OpenCV/dlib models from {}", modelPath.toStdString()); }
    return HRCode::Ok;
}

/**
 * @brief 将当前人员缓存导出为 JSON 文件。
 */
HRCode OpenCVDlibBackend::Impl::saveModel(const QString& modelPath) {
    if (logger) { logger->info("Saving model to {}", modelPath.toStdString()); }
    QString directory;
    QString jsonPath;

    QFileInfo info(modelPath);
    if (!info.exists()) {
        directory = info.suffix().isEmpty() ? info.absoluteFilePath() : info.absolutePath();
        jsonPath = info.suffix().compare("json", Qt::CaseInsensitive) == 0
                       ? info.absoluteFilePath()
                       : QDir(directory).filePath(QStringLiteral("people.json"));
    } else if (info.isDir()) {
        directory = info.absoluteFilePath();
        jsonPath = QDir(directory).filePath(QStringLiteral("people.json"));
    } else {
        directory = info.absolutePath();
        jsonPath = info.suffix().compare("json", Qt::CaseInsensitive) == 0
                       ? info.absoluteFilePath()
                       : QDir(directory).filePath(QStringLiteral("people.json"));
    }

    if (directory.isEmpty()) return HRCode::ModelSaveFailed;
    QDir dir;
    if (!dir.mkpath(directory)) { return HRCode::ModelSaveFailed; }

    // 将数据库的最新数据导出为 JSON，方便迁移或手动编辑。
    const HRCode res = savePersonDatabase(jsonPath);
    if (res != HRCode::Ok) return res;

    int personCount = 0;
    {
        std::scoped_lock lock(mutex);
        modelDirectory = directory;
        personCount = persons.size();
    }

    if (logger) { logger->info("Exported {} persons to {}", personCount, jsonPath.toStdString()); }
    return HRCode::Ok;
}

/**
 * @brief 向数据库注册新人员，失败时自动刷新缓存重试。
 */
HRCode OpenCVDlibBackend::Impl::registerPerson(const PersonInfo& person) {
    if (person.id.isEmpty()) return HRCode::UnknownError;
    if (!storageReady && !ensureStorageReady()) return HRCode::UnknownError;

    {
        // 缓存层先检查是否重复注册，避免额外的数据库写操作。
        std::scoped_lock lock(mutex);
        if (persons.contains(person.id)) return HRCode::PersonExists;
    }

    if (!persistPerson(person, false)) {
        // 如果数据库写入失败，刷新缓存以判断是否由于并发注册导致记录已经存在。
        const HRCode reload = loadPersonsFromStorage();
        {
            std::scoped_lock lock(mutex);
            if (reload == HRCode::Ok && persons.contains(person.id)) {
                return HRCode::PersonExists;
            }
        }
        if (logger) { logger->error("Failed to persist person {}", person.id.toStdString()); }
        return HRCode::UnknownError;
    }

    {
        std::scoped_lock lock(mutex);
        persons.insert(person.id, person);
    }

    if (logger) {
        logger->info(
            "Registered person {} ({})", person.id.toStdString(), person.name.toStdString());
    }
    return HRCode::Ok;
}

/**
 * @brief 从数据库删除人员并同步到内存缓存。
 */
HRCode OpenCVDlibBackend::Impl::removePerson(const QString& personId) {
    if (personId.isEmpty()) return HRCode::UnknownError;
    if (!storageReady && !ensureStorageReady()) return HRCode::UnknownError;

    {
        // 如果缓存中不存在该人员，直接返回未找到，无需访问数据库。
        std::scoped_lock lock(mutex);
        if (!persons.contains(personId)) return HRCode::PersonNotFound;
    }

    if (!deletePersonFromDatabase(personId)) {
        if (logger) {
            logger->error("Failed to delete person {} from database", personId.toStdString());
        }
        return HRCode::UnknownError;
    }

    {
        std::scoped_lock lock(mutex);
        persons.remove(personId);
    }

    if (logger) { logger->info("Removed person {}", personId.toStdString()); }
    return HRCode::Ok;
}

/**
 * @brief 根据 id 查询人员（仅访问内存缓存，O(1)）。
 */
HRCode OpenCVDlibBackend::Impl::getPerson(const QString& personId, PersonInfo& outPerson) {
    std::scoped_lock lock(mutex);
    auto it = persons.constFind(personId);
    if (it == persons.cend()) {
        if (logger) { logger->info("GetPerson: {} not found", personId.toStdString()); }
        return HRCode::PersonNotFound;
    }
    outPerson = it.value();
    if (logger) {
        logger->debug("GetPerson: {} ({}) fetched from cache",
                      outPerson.id.toStdString(),
                      outPerson.name.toStdString());
    }
    return HRCode::Ok;
}

/**
 * @brief 训练接口暂未实现，直接返回 UnknownError。
 */
HRCode OpenCVDlibBackend::Impl::train(const QString&) {
    if (logger) { logger->warn("Train not implemented for OpenCVDlibBackend"); }
    return HRCode::UnknownError;
}

QString OpenCVDlibBackend::Impl::backendName() const {
    return QString::fromLatin1(kBackendName);
}

/**
 * @brief 根据输入路径推断关键模型文件路径。
 */
OpenCVDlibBackend::Impl::ModelFiles OpenCVDlibBackend::Impl::resolveModelFiles(
    const QFileInfo& info) const {
    ModelFiles files;
    QDir dir(info.isDir() ? info.absoluteFilePath() : info.absolutePath());
    files.baseDirectory = dir.absolutePath();

    const auto pickExisting = [&dir](const QStringList& candidates) -> QString {
        for (const auto& name : candidates) {
            const QString path = dir.filePath(name);
            if (QFileInfo::exists(path)) return path;
        }
        return QString();
    };

    if (info.isFile() && info.suffix().compare("dat", Qt::CaseInsensitive) == 0) {
        const QString lowerName = info.fileName().toLower();
        if (lowerName.contains("shape") && files.shapePredictor.isEmpty()) {
            files.shapePredictor = info.absoluteFilePath();
        } else if ((lowerName.contains("recognition") || lowerName.contains("resnet")) &&
                   files.recognition.isEmpty()) {
            files.recognition = info.absoluteFilePath();
        }
    } else if (info.isFile() && info.suffix().compare("json", Qt::CaseInsensitive) == 0) {
        files.personDatabase = info.absoluteFilePath();
    }

    if (files.shapePredictor.isEmpty()) {
        files.shapePredictor =
            pickExisting({QStringLiteral("shape_predictor_68_face_landmarks.dat"),
                          QStringLiteral("shape_predictor_5_face_landmarks.dat")});
    }
    if (files.recognition.isEmpty()) {
        files.recognition =
            pickExisting({QStringLiteral("dlib_face_recognition_resnet_model_v1.dat")});
    }
    if (files.personDatabase.isEmpty()) {
        files.personDatabase = dir.filePath(QStringLiteral("people.json"));
    }
    return files;
}

/**
 * @brief 从全局配置中心刷新阈值、表名等运行参数。
 */
void OpenCVDlibBackend::Impl::refreshConfiguration() {
    auto& cfg = config::ConfigManager::instance();
    const QString requestedTable = cfg.setOrDefault(QStringLiteral("HumanRecognition/PersonsTable"),
                                                    QString::fromLatin1(kDefaultPersonsTable))
                                       .toString();
    personsTable = sanitizeTableName(requestedTable);
    if (personsTable.isEmpty()) { personsTable = QString::fromLatin1(kDefaultPersonsTable); }

    matchThreshold =
        cfg.setOrDefault(QStringLiteral("HumanRecognition/MatchThreshold"), kDefaultMatchThreshold)
            .toDouble();
    if (matchThreshold < 0.0) matchThreshold = kDefaultMatchThreshold;
    autoCreatePersonsTable =
        cfg.setOrDefault(QStringLiteral("HumanRecognition/AutoCreateTable"), true).toBool();
}

/**
 * @brief 确保数据库已初始化并按需要创建表。
 */
bool OpenCVDlibBackend::Impl::ensureStorageReady() {
    if (storageReady) {
        if (logger) { logger->debug("Storage already ready"); }
        return true;
    }
    refreshConfiguration();

    try {
        if (!storage::DbManager::instance().init()) {
            if (logger) { logger->error("Failed to initialize storage database"); }
            return false;
        }
    } catch (const std::exception& ex) {
        if (logger) { logger->error("Storage initialization threw exception: {}", ex.what()); }
        return false;
    } catch (...) {
        if (logger) { logger->error("Storage initialization threw unknown exception"); }
        return false;
    }

    if (autoCreatePersonsTable && !ensurePersonsTable()) { return false; }
    storageReady = true;
    if (logger) {
        logger->info("Storage ready. personsTable={} autoCreate={}",
                     personsTable.toStdString(),
                     autoCreatePersonsTable);
    }
    return true;
}

/**
 * @brief 在 SQLite 中创建人员表与更新时间触发器。
 */
bool OpenCVDlibBackend::Impl::ensurePersonsTable() {
    const QString table =
        personsTable.isEmpty() ? QString::fromLatin1(kDefaultPersonsTable) : personsTable;
    try {
        QSqlDatabase& db = storage::DbManager::instance().db();
        QSqlQuery ddl(db);
        const QString ddlSql = QStringLiteral(
                                   "CREATE TABLE IF NOT EXISTS %1 ("
                                   "id TEXT PRIMARY KEY,"
                                   "name TEXT,"
                                   "metadata_json TEXT,"
                                   "feature_json TEXT,"
                                   "created_at TEXT DEFAULT CURRENT_TIMESTAMP,"
                                   "updated_at TEXT DEFAULT CURRENT_TIMESTAMP)")
                                   .arg(table);
        if (!ddl.exec(ddlSql)) {
            if (logger) {
                logger->error("Failed to create table {}: {}",
                              table.toStdString(),
                              ddl.lastError().text().toStdString());
            }
            return false;
        }

        QSqlQuery trigger(db);
        const QString triggerSql =
            QStringLiteral(
                "CREATE TRIGGER IF NOT EXISTS %1_update_timestamp "
                "AFTER UPDATE ON %1 "
                "BEGIN "
                "UPDATE %1 SET updated_at = CURRENT_TIMESTAMP WHERE id = NEW.id; "
                "END;")
                .arg(table);
        if (!trigger.exec(triggerSql)) {
            if (logger) {
                logger->warn("Failed to ensure update trigger for {}: {}",
                             table.toStdString(),
                             trigger.lastError().text().toStdString());
            }
        }
        if (logger) { logger->info("Persons table {} ensured", table.toStdString()); }
        return true;
    } catch (const std::exception& ex) {
        if (logger) { logger->error("ensurePersonsTable exception: {}", ex.what()); }
    } catch (...) {
        if (logger) { logger->error("ensurePersonsTable unknown exception"); }
    }
    return false;
}

/**
 * @brief 将任意文本转换为合法的 SQL 表名（仅保留字母/数字/下划线）。
 */
QString OpenCVDlibBackend::Impl::sanitizeTableName(const QString& requested) const {
    QString out;
    out.reserve(requested.size());
    for (const QChar& ch : requested) {
        if (ch.isLetterOrNumber() || ch == QLatin1Char('_')) { out.append(ch); }
    }
    return out;
}

/**
 * @brief 将元数据对象序列化为紧凑字符串，便于存储。
 */
QString OpenCVDlibBackend::Impl::serializeMetadata(const QJsonObject& obj) const {
    if (obj.isEmpty()) return QString();
    return QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

/**
 * @brief 将数据库中的字符串还原为 JSON 对象。
 */
QJsonObject OpenCVDlibBackend::Impl::deserializeMetadata(const QString& json) const {
    if (json.isEmpty()) return {};
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) return {};
    return doc.object();
}

/**
 * @brief 以 JSON 方式序列化特征向量，方便与其他后端共享。
 */
QString OpenCVDlibBackend::Impl::serializeFeature(const FaceFeature& feature) const {
    if (feature.values.isEmpty()) return QString();
    QJsonObject featObj;
    QJsonArray values;
    for (float v : feature.values) values.append(v);
    featObj.insert(QStringLiteral("values"), values);
    featObj.insert(QStringLiteral("version"), feature.version);
    if (feature.norm.has_value()) featObj.insert(QStringLiteral("norm"), *feature.norm);
    return QString::fromUtf8(QJsonDocument(featObj).toJson(QJsonDocument::Compact));
}

/**
 * @brief 从 JSON 字符串解析特征向量。
 */
std::optional<FaceFeature> OpenCVDlibBackend::Impl::deserializeFeature(const QString& json) const {
    if (json.isEmpty()) return std::nullopt;
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) return std::nullopt;
    const QJsonObject obj = doc.object();
    const QJsonArray values = obj.value(QStringLiteral("values")).toArray();
    if (values.isEmpty()) return std::nullopt;
    FaceFeature feat;
    feat.values.resize(values.size());
    for (int i = 0; i < values.size(); ++i) {
        feat.values[i] = static_cast<float>(values.at(i).toDouble());
    }
    feat.version = obj.value(QStringLiteral("version")).toString();
    if (obj.contains(QStringLiteral("norm"))) {
        feat.norm = static_cast<float>(obj.value(QStringLiteral("norm")).toDouble());
    }
    return feat;
}

/**
 * @brief 将人员写入数据库；若允许则执行 UPSERT。
 */
bool OpenCVDlibBackend::Impl::persistPerson(const PersonInfo& person, bool allowReplace) {
    if (!storageReady && !ensureStorageReady()) return false;
    const QString table =
        personsTable.isEmpty() ? QString::fromLatin1(kDefaultPersonsTable) : personsTable;
    try {
        QSqlDatabase& db = storage::DbManager::instance().db();
        const QString metadataJson = serializeMetadata(person.metadata);
        const QString featureJson = person.canonicalFeature.has_value()
                                        ? serializeFeature(*person.canonicalFeature)
                                        : QString();

        // INSERT 语句依赖 SQLite 主键约束，保障人员 ID 的唯一性。
        QSqlQuery insert(db);
        insert.prepare(QStringLiteral("INSERT INTO %1 "
                                      "(id, name, metadata_json, feature_json, created_at, "
                                      "updated_at) "
                                      "VALUES (?, ?, ?, ?, CURRENT_TIMESTAMP, "
                                      "CURRENT_TIMESTAMP)")
                           .arg(table));
        insert.addBindValue(person.id);
        insert.addBindValue(person.name);
        insert.addBindValue(metadataJson);
        insert.addBindValue(featureJson.isEmpty() ? QVariant() : QVariant(featureJson));

        if (insert.exec()) { return true; }

        if (!allowReplace) {
            // 不允许覆盖时直接返回失败，调用者可感知第一时间冲突。
            if (logger) {
                logger->warn("Failed to insert person {}: {}",
                             person.id.toStdString(),
                             insert.lastError().text().toStdString());
            }
            return false;
        }

        // 允许覆盖的情况下改用 UPDATE，以保持原有 created_at。
        QSqlQuery update(db);
        update.prepare(QStringLiteral("UPDATE %1 SET name = ?, metadata_json = ?, "
                                      "feature_json = ?, updated_at = CURRENT_TIMESTAMP "
                                      "WHERE id = ?")
                           .arg(table));
        update.addBindValue(person.name);
        update.addBindValue(metadataJson);
        update.addBindValue(featureJson.isEmpty() ? QVariant() : QVariant(featureJson));
        update.addBindValue(person.id);
        if (!update.exec()) {
            if (logger) {
                logger->error("Failed to upsert person {}: {}",
                              person.id.toStdString(),
                              update.lastError().text().toStdString());
            }
            return false;
        }
        if (update.numRowsAffected() == 0) {
            // UPDATE 未生效说明记录不存在，此时返回失败让上层决策。
            if (logger) {
                logger->warn("Upsert affected 0 rows for person {}", person.id.toStdString());
            }
            return false;
        }
        return true;
    } catch (const std::exception& ex) {
        if (logger) { logger->error("persistPerson exception: {}", ex.what()); }
    } catch (...) {
        if (logger) { logger->error("persistPerson unknown exception"); }
    }
    return false;
}

/**
 * @brief 从数据库移除单条人员记录。
 */
bool OpenCVDlibBackend::Impl::deletePersonFromDatabase(const QString& personId) {
    if (!storageReady && !ensureStorageReady()) return false;
    const QString table =
        personsTable.isEmpty() ? QString::fromLatin1(kDefaultPersonsTable) : personsTable;
    try {
        QSqlDatabase& db = storage::DbManager::instance().db();
        QSqlQuery query(db);
        // DELETE 语句执行完毕后，可通过受影响行数判断是否删除成功。
        query.prepare(QStringLiteral("DELETE FROM %1 WHERE id = ?").arg(table));
        query.addBindValue(personId);
        if (!query.exec()) {
            if (logger) {
                logger->error("Failed to delete person {}: {}",
                              personId.toStdString(),
                              query.lastError().text().toStdString());
            }
            return false;
        }
        const bool removed = query.numRowsAffected() > 0;
        if (!removed && logger) {
            logger->info("DeletePerson: {} not present in table", personId.toStdString());
        }
        return removed;
    } catch (const std::exception& ex) {
        if (logger) { logger->error("deletePersonFromDatabase exception: {}", ex.what()); }
    } catch (...) {
        if (logger) { logger->error("deletePersonFromDatabase unknown exception"); }
    }
    return false;
}

/**
 * @brief 全量加载人员表到内存缓存。
 */
HRCode OpenCVDlibBackend::Impl::loadPersonsFromStorage() {
    if (!storageReady && !ensureStorageReady()) return HRCode::ModelLoadFailed;
    const QString table =
        personsTable.isEmpty() ? QString::fromLatin1(kDefaultPersonsTable) : personsTable;
    QVector<PersonInfo> loaded;
    try {
        QSqlDatabase& db = storage::DbManager::instance().db();
        QSqlQuery query(db);
        // 这里暂不分页，假设人员库规模相对可控，如需扩展可改用增量加载。
        if (!query.exec(QStringLiteral("SELECT id, name, metadata_json, feature_json FROM %1")
                            .arg(table))) {
            if (logger) {
                logger->error("Failed to query persons from {}: {}",
                              table.toStdString(),
                              query.lastError().text().toStdString());
            }
            return HRCode::ModelLoadFailed;
        }

        while (query.next()) {
            PersonInfo info;
            info.id = query.value(0).toString();
            info.name = query.value(1).toString();
            // 元数据与特征字段以 JSON 字符串存储，这里需做解析。
            info.metadata = deserializeMetadata(query.value(2).toString());
            auto feat = deserializeFeature(query.value(3).toString());
            if (feat.has_value()) info.canonicalFeature = feat;
            loaded.append(info);
        }
    } catch (const std::exception& ex) {
        if (logger) { logger->error("loadPersonsFromStorage exception: {}", ex.what()); }
        return HRCode::ModelLoadFailed;
    } catch (...) {
        if (logger) { logger->error("loadPersonsFromStorage unknown exception"); }
        return HRCode::ModelLoadFailed;
    }

    {
        std::scoped_lock lock(mutex);
        persons.clear();
        // 使用哈希表缓存，便于 O(1) 查询；忽略空 id 以保持数据一致性。
        for (const PersonInfo& info : loaded) {
            if (!info.id.isEmpty()) { persons.insert(info.id, info); }
        }
    }

    if (logger) {
        logger->info("Loaded {} persons from table {}",
                     static_cast<int>(loaded.size()),
                     table.toStdString());
    }
    return HRCode::Ok;
}

/**
 * @brief 兼容旧版 JSON 人员库并导入到数据库。
 */
HRCode OpenCVDlibBackend::Impl::loadPersonDatabase(const QString& jsonPath) {
    if (logger) { logger->info("Importing legacy JSON database from {}", jsonPath.toStdString()); }
    QFile file(jsonPath);
    if (!file.open(QIODevice::ReadOnly)) { return HRCode::ModelLoadFailed; }

    const QByteArray raw = file.readAll();
    const QJsonDocument doc = QJsonDocument::fromJson(raw);
    if (!doc.isObject()) return HRCode::ModelLoadFailed;
    const QJsonArray arr = doc.object().value(QStringLiteral("people")).toArray();

    QVector<PersonInfo> parsed;
    parsed.reserve(arr.size());
    for (const auto& item : arr) {
        // JSON 中不一定保证键存在，因此取值前均带默认值。
        const QJsonObject obj = item.toObject();
        PersonInfo info;
        info.id = obj.value(QStringLiteral("id")).toString();
        info.name = obj.value(QStringLiteral("name")).toString();
        info.metadata = obj.value(QStringLiteral("metadata")).toObject();

        const QJsonObject featureObj = obj.value(QStringLiteral("feature")).toObject();
        const QJsonArray valueArray = featureObj.value(QStringLiteral("values")).toArray();
        if (!valueArray.isEmpty()) {
            FaceFeature feat;
            feat.values.resize(valueArray.size());
            for (int i = 0; i < valueArray.size(); ++i) {
                feat.values[i] = static_cast<float>(valueArray.at(i).toDouble());
            }
            feat.version = featureObj.value(QStringLiteral("version")).toString();
            if (featureObj.contains(QStringLiteral("norm"))) {
                feat.norm = static_cast<float>(featureObj.value(QStringLiteral("norm")).toDouble());
            }
            info.canonicalFeature = feat;
        }

        if (!info.id.isEmpty()) parsed.append(info);
    }

    if (!storageReady && !ensureStorageReady()) return HRCode::ModelLoadFailed;

    const int parsedCount = parsed.size();
    try {
        QSqlDatabase& db = storage::DbManager::instance().db();
        // 批量导入使用事务，保证错误时能完全回滚。
        if (!db.transaction()) {
            if (logger) {
                logger->error("Failed to start transaction for person import: {}",
                              db.lastError().text().toStdString());
            }
            return HRCode::ModelLoadFailed;
        }

        for (const PersonInfo& person : parsed) {
            if (!persistPerson(person, true)) {
                // 任意一条写入失败，立即回滚保持数据一致。
                db.rollback();
                return HRCode::ModelLoadFailed;
            }
        }

        if (!db.commit()) {
            // 提交失败时需要回滚以释放锁。
            if (logger) {
                logger->error("Failed to commit person import: {}",
                              db.lastError().text().toStdString());
            }
            db.rollback();
            return HRCode::ModelLoadFailed;
        }
    } catch (const std::exception& ex) {
        if (logger) { logger->error("Exception while importing persons: {}", ex.what()); }
        return HRCode::ModelLoadFailed;
    } catch (...) {
        if (logger) { logger->error("Unknown exception while importing persons"); }
        return HRCode::ModelLoadFailed;
    }

    if (logger) {
        logger->info(
            "Imported {} persons from legacy database {}", parsedCount, jsonPath.toStdString());
    }
    return loadPersonsFromStorage();
}

/**
 * @brief 将当前缓存内容写入 JSON 文件，便于导出/调试。
 */
HRCode OpenCVDlibBackend::Impl::savePersonDatabase(const QString& jsonPath) {
    const HRCode syncResult = loadPersonsFromStorage();
    if (syncResult != HRCode::Ok) return syncResult;

    QJsonArray peopleArray;
    {
        std::scoped_lock lock(mutex);
        for (auto it = persons.cbegin(); it != persons.cend(); ++it) {
            const PersonInfo& person = it.value();
            QJsonObject obj;
            obj.insert(QStringLiteral("id"), person.id);
            obj.insert(QStringLiteral("name"), person.name);
            obj.insert(QStringLiteral("metadata"), person.metadata);

            if (person.canonicalFeature.has_value()) {
                const FaceFeature& feat = person.canonicalFeature.value();
                QJsonObject featObj;
                QJsonArray values;
                for (float v : feat.values) values.append(v);
                featObj.insert(QStringLiteral("values"), values);
                featObj.insert(QStringLiteral("version"), feat.version);
                if (feat.norm.has_value()) featObj.insert(QStringLiteral("norm"), *feat.norm);
                obj.insert(QStringLiteral("feature"), featObj);
            }

            peopleArray.append(obj);
        }
    }

    QJsonObject root;
    root.insert(QStringLiteral("people"), peopleArray);
    const QJsonDocument doc(root);

    QSaveFile file(jsonPath);
    if (!file.open(QIODevice::WriteOnly)) { return HRCode::ModelSaveFailed; }
    if (file.write(doc.toJson(QJsonDocument::Indented)) < 0) return HRCode::ModelSaveFailed;
    if (!file.commit()) return HRCode::ModelSaveFailed;
    if (logger) {
        logger->info(
            "Exported {} persons to legacy JSON {}", peopleArray.size(), jsonPath.toStdString());
    }
    return HRCode::Ok;
}

}  // namespace HumanRecognition

#endif  // defined(HAS_OPENCV) && defined(HAS_DLIB)
