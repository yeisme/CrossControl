#include "opencv_backend.h"

#include <QBuffer>
#include <QImage>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#include <cmath>

#include "logging.h"
#include "storage.h"

static auto HRLog = *logging::LoggerManager::instance().getLogger("HR.OpenCV");

OpenCVHumanRecognitionBackend::OpenCVHumanRecognitionBackend() {}
OpenCVHumanRecognitionBackend::~OpenCVHumanRecognitionBackend() {
    shutdown();
}

bool OpenCVHumanRecognitionBackend::initialize() {
    if (m_initialized) return true;

    if (!ensureSchema_()) return false;

    // 使用 OpenCV 自带 haarcascade (需用户提供或放在运行目录)；这里尝试常见文件名
    const std::vector<std::string> candidates = {
        "haarcascade_frontalface_default.xml",  // 放同目录或 PATH
    };
    bool loaded = false;
    for (const auto& c : candidates) {
        if (m_faceCascade.load(c)) {
            loaded = true;
            break;
        }
    }
    if (!loaded) {
        HRLog.warn("Failed to load Haar cascade, face detection will return empty results");
    }

    loadFeatureDatabase();
    m_initialized = true;
    HRLog.info("OpenCVHumanRecognitionBackend initialized, cache size={}", m_featureCache.size());
    return true;
}

void OpenCVHumanRecognitionBackend::shutdown() {
    m_initialized = false;
    m_featureCache.clear();
}

std::vector<FaceDetectionResult> OpenCVHumanRecognitionBackend::detectFaces(const QImage& image) {
    std::vector<FaceDetectionResult> results;
    if (image.isNull()) return results;
    if (m_faceCascade.empty()) return results;

    cv::Mat mat = qimageToMat_(image);
    cv::Mat gray;
    if (mat.channels() == 3)
        cv::cvtColor(mat, gray, cv::COLOR_BGR2GRAY);
    else if (mat.channels() == 4)
        cv::cvtColor(mat, gray, cv::COLOR_BGRA2GRAY);
    else
        gray = mat;
    cv::equalizeHist(gray, gray);

    std::vector<cv::Rect> faces;
    m_faceCascade.detectMultiScale(gray, faces, 1.1, 3, 0, cv::Size(30, 30));
    for (const auto& f : faces) {
        FaceDetectionResult r;
        r.box = QRect(f.x, f.y, f.width, f.height);
        r.score = 0.5f;  // Haar 无法提供真实 score，放占位
        results.push_back(std::move(r));
    }
    return results;
}

std::optional<FaceEmbedding> OpenCVHumanRecognitionBackend::extractEmbedding(
    const QImage& faceImage) {
    if (faceImage.isNull()) return std::nullopt;
    // 占位：简单将图像缩放到 16x16，然后每通道平均 -> 16*16*3 = 768 维
    QImage scaled = faceImage.convertToFormat(QImage::Format_RGB32).scaled(16, 16);
    FaceEmbedding emb;
    emb.reserve(16 * 16 * 3);
    for (int y = 0; y < scaled.height(); ++y) {
        const QRgb* line = reinterpret_cast<const QRgb*>(scaled.constScanLine(y));
        for (int x = 0; x < scaled.width(); ++x) {
            QRgb px = line[x];
            emb.push_back(static_cast<float>(qRed(px)) / 255.f);
            emb.push_back(static_cast<float>(qGreen(px)) / 255.f);
            emb.push_back(static_cast<float>(qBlue(px)) / 255.f);
        }
    }
    return emb;
}

int OpenCVHumanRecognitionBackend::enrollPerson(const PersonInfo& info,
                                                const std::vector<QImage>& faceImages) {
    if (info.personId.isEmpty()) return 0;
    QSqlDatabase& db = storage::Storage::db();
    QSqlQuery q(db);
    db.transaction();
    int count = 0;
    // Ensure person record exists (upsert) so listPersons() will show this person
    QSqlQuery qPerson(db);
    qPerson.prepare(
        "INSERT INTO persons(person_id,name,extra,created_at) VALUES(?,?,?,strftime('%s','now'))"
        " ON CONFLICT(person_id) DO UPDATE SET name=excluded.name, extra=excluded.extra");
    qPerson.addBindValue(info.personId);
    qPerson.addBindValue(info.name);
    qPerson.addBindValue(info.extraJson);
    if (!qPerson.exec()) {
        HRLog.warn("Upsert person failed during enroll: {}", qPerson.lastError().text().toStdString());
        // continue: still try to insert features
    }
    for (const auto& img : faceImages) {
        auto embOpt = extractEmbedding(img);
        if (!embOpt) continue;
        const auto& emb = *embOpt;

        // 序列化 embedding 为字节（float 数组）
        QByteArray blob(reinterpret_cast<const char*>(emb.data()),
                        static_cast<int>(emb.size() * sizeof(float)));

        q.prepare(
            "INSERT INTO face_features(person_id, name, feature, dim, extra, created_at) "
            "VALUES(?,?,?,?,?,strftime('%s','now'))");
        q.addBindValue(info.personId);
        q.addBindValue(info.name);
        q.addBindValue(blob);
        q.addBindValue(static_cast<int>(emb.size()));
        q.addBindValue(info.extraJson);
        if (!q.exec()) {
            HRLog.warn("Insert feature failed: {}", q.lastError().text().toStdString());
            continue;
        }
        CacheItem item{emb, info.personId, info.name};
        m_featureCache.push_back(std::move(item));
        ++count;
    }
    db.commit();
    HRLog.info("Enroll person {} added {} embeddings", info.personId.toStdString(), count);
    return count;
}

std::vector<FaceDetectionResult> OpenCVHumanRecognitionBackend::detectAndRecognize(
    const QImage& image) {
    auto dets = detectFaces(image);
    for (auto& d : dets) {
        QImage face = image.copy(d.box);
        auto embOpt = extractEmbedding(face);
        if (!embOpt) continue;
        auto match = matchEmbedding(*embOpt);
        if (match) {
            d.matchedPersonId = match->matchedPersonId;
            d.matchedPersonName = match->matchedPersonName;
            d.matchedDistance = match->matchedDistance;
            d.embedding = match->embedding;  // 也可保留自身 embedding
        }
    }
    return dets;
}

std::optional<FaceDetectionResult> OpenCVHumanRecognitionBackend::matchEmbedding(
    const FaceEmbedding& emb) {
    auto metric = [](const FaceEmbedding& a, const FaceEmbedding& b) {
        float dist = 0.f;
        for (size_t k = 0; k < a.size(); ++k) {
            float diff = a[k] - b[k];
            dist += diff * diff;
        }
        return std::sqrt(dist);
    };
    return matchWithMetric_(emb, metric);
}

bool OpenCVHumanRecognitionBackend::saveFeatureDatabase() {
    // 对于 SQLite 后端，数据已经写入 DB；此方法保证事务落盘
    QSqlDatabase& db = storage::Storage::db();
    if (!db.isOpen()) return false;
    // Commit should have been used by enroll; ensure no-op success
    return true;
}

bool OpenCVHumanRecognitionBackend::loadFeatureDatabase() {
    m_featureCache.clear();
    QSqlDatabase& db = storage::Storage::db();
    QSqlQuery q(db);
    if (!q.exec("SELECT person_id,name,feature,dim FROM face_features")) {
        HRLog.warn("Load features failed: {}", q.lastError().text().toStdString());
        return false;
    }
    while (q.next()) {
        CacheItem item;
        item.personId = q.value(0).toString();
        item.name = q.value(1).toString();
        QByteArray blob = q.value(2).toByteArray();
        int dim = q.value(3).toInt();
        item.emb.resize(dim);
        memcpy(item.emb.data(), blob.data(), std::min<int>(blob.size(), dim * sizeof(float)));
        m_featureCache.push_back(std::move(item));
    }
    HRLog.info("Loaded {} embeddings from DB", m_featureCache.size());
    return true;
}

bool OpenCVHumanRecognitionBackend::ensureSchema_() {
    QSqlDatabase& db = storage::Storage::db();
    QSqlQuery q(db);
    // 建表：基础人员表 (person_id 唯一) 与特征表（多条）
    if (!q.exec("CREATE TABLE IF NOT EXISTS persons(\n"
                " person_id TEXT PRIMARY KEY,\n"
                " name TEXT,\n"
                " extra TEXT,\n"
                " created_at INTEGER\n"
                ")")) {
        HRLog.warn("Create table failed: {}", q.lastError().text().toStdString());
        return false;
    }
    if (!q.exec("CREATE TABLE IF NOT EXISTS face_features(\n"
                " id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
                " person_id TEXT NOT NULL,\n"
                " name TEXT,\n"
                " feature BLOB NOT NULL,\n"
                " dim INTEGER NOT NULL,\n"
                " extra TEXT,\n"
                " created_at INTEGER\n"
                ")")) {
        HRLog.warn("Create table failed: {}", q.lastError().text().toStdString());
        return false;
    }
    if (!q.exec(
            "CREATE INDEX IF NOT EXISTS idx_face_features_person ON face_features(person_id)")) {
        HRLog.warn("Create index failed: {}", q.lastError().text().toStdString());
    }
    return true;
}

// ----- Person management -----
std::vector<PersonInfo> OpenCVHumanRecognitionBackend::listPersons() {
    std::vector<PersonInfo> out;
    QSqlDatabase& db = storage::Storage::db();
    QSqlQuery q(db);
    if (!q.exec("SELECT person_id,name,extra FROM persons")) return out;
    while (q.next()) {
        PersonInfo p;
        p.personId = q.value(0).toString();
        p.name = q.value(1).toString();
        p.extraJson = q.value(2).toString();
        out.push_back(std::move(p));
    }
    return out;
}

bool OpenCVHumanRecognitionBackend::updatePersonInfo(const PersonInfo& info) {
    if (info.personId.isEmpty()) return false;
    QSqlDatabase& db = storage::Storage::db();
    QSqlQuery q(db);
    // upsert
    q.prepare(
        "INSERT INTO persons(person_id,name,extra,created_at) VALUES(?,?,?,strftime('%s','now'))"
        " ON CONFLICT(person_id) DO UPDATE SET name=excluded.name, extra=excluded.extra");
    q.addBindValue(info.personId);
    q.addBindValue(info.name);
    q.addBindValue(info.extraJson);
    if (!q.exec()) {
        HRLog.warn("Update person failed: {}", q.lastError().text().toStdString());
        return false;
    }
    return true;
}

bool OpenCVHumanRecognitionBackend::deletePerson(const QString& personId) {
    if (personId.isEmpty()) return false;
    QSqlDatabase& db = storage::Storage::db();
    QSqlQuery q(db);
    db.transaction();
    q.prepare("DELETE FROM face_features WHERE person_id = ?");
    q.addBindValue(personId);
    if (!q.exec()) {
        HRLog.warn("Delete features failed: {}", q.lastError().text().toStdString());
        db.rollback();
        return false;
    }
    q.prepare("DELETE FROM persons WHERE person_id = ?");
    q.addBindValue(personId);
    if (!q.exec()) {
        HRLog.warn("Delete person failed: {}", q.lastError().text().toStdString());
        db.rollback();
        return false;
    }
    db.commit();
    // 清理内存缓存
    m_featureCache.erase(
        std::remove_if(m_featureCache.begin(),
                       m_featureCache.end(),
                       [&](const CacheItem& it) { return it.personId == personId; }),
        m_featureCache.end());
    return true;
}

std::vector<FaceEmbedding> OpenCVHumanRecognitionBackend::getPersonFeatures(
    const QString& personId) {
    std::vector<FaceEmbedding> out;
    if (personId.isEmpty()) return out;
    QSqlDatabase& db = storage::Storage::db();
    QSqlQuery q(db);
    q.prepare("SELECT feature,dim FROM face_features WHERE person_id = ?");
    q.addBindValue(personId);
    if (!q.exec()) return out;
    while (q.next()) {
        QByteArray blob = q.value(0).toByteArray();
        int dim = q.value(1).toInt();
        FaceEmbedding emb;
        emb.resize(dim);
        memcpy(emb.data(), blob.data(), std::min<int>(blob.size(), dim * sizeof(float)));
        out.push_back(std::move(emb));
    }
    return out;
}

std::vector<FaceDetectionResult> OpenCVHumanRecognitionBackend::processFrame(const QImage& frame,
                                                                             bool recognize) {
    if (!recognize) return detectFaces(frame);
    return detectAndRecognize(frame);
}

cv::Mat OpenCVHumanRecognitionBackend::qimageToMat_(const QImage& img) {
    QImage conv = img.convertToFormat(QImage::Format_RGBA8888);
    return cv::Mat(conv.height(),
                   conv.width(),
                   CV_8UC4,
                   const_cast<uchar*>(conv.constBits()),
                   conv.bytesPerLine())
        .clone();
}

QImage OpenCVHumanRecognitionBackend::matToQImage_(const cv::Mat& mat) {
    if (mat.type() == CV_8UC4) {
        return QImage(mat.data,
                      mat.cols,
                      mat.rows,
                      static_cast<int>(mat.step),
                      QImage::Format_RGBA8888)
            .copy();
    } else if (mat.type() == CV_8UC3) {
        cv::Mat rgb;
        cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
        return QImage(
                   rgb.data, rgb.cols, rgb.rows, static_cast<int>(rgb.step), QImage::Format_RGB888)
            .copy();
    } else if (mat.type() == CV_8UC1) {
        return QImage(mat.data,
                      mat.cols,
                      mat.rows,
                      static_cast<int>(mat.step),
                      QImage::Format_Grayscale8)
            .copy();
    }
    return {};
}
