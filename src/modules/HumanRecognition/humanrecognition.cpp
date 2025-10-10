#include "humanrecognition.h"

#include <mutex>

#include "factory.h"

namespace HumanRecognition {

struct HumanRecognition::Impl {
    std::unique_ptr<IHumanRecognitionBackend> backend;
    QString backendName;
    std::mutex mtx;
};

HumanRecognition& HumanRecognition::instance() {
    static HumanRecognition g_instance;
    return g_instance;
}

HumanRecognition::HumanRecognition(const QList<QPair<QString, BackendFactory>>& backendFactories)
    : m_impl(std::make_unique<Impl>()) {
    for (const auto& p : backendFactories) { registerBackendFactory(p.first, p.second); }
}

HumanRecognition::~HumanRecognition() = default;

HRCode HumanRecognition::loadModel(const path& modelPath) {
    std::lock_guard<std::mutex> lock(m_impl->mtx);
    if (!m_impl->backend) return HRCode::UnknownError;
    return m_impl->backend->loadModel(QString::fromStdWString(modelPath.wstring()));
}

HRCode HumanRecognition::saveModel(const path& modelPath) {
    std::lock_guard<std::mutex> lock(m_impl->mtx);
    if (!m_impl->backend) return HRCode::UnknownError;
    return m_impl->backend->saveModel(QString::fromStdWString(modelPath.wstring()));
}

QPair<HRCode, QString> HumanRecognition::loadModelFromDir(const path& modelDir,
                                                          const QString& ext) {
    // 检查后端
    if (!hasBackend()) return qMakePair(HRCode::UnknownError, QString());

    // 验证路径
    try {
        if (!exists(modelDir) || !is_directory(modelDir)) {
            return qMakePair(HRCode::ModelLoadFailed, QString());
        }
    } catch (...) { return qMakePair(HRCode::ModelLoadFailed, QString()); }

    // 规范化扩展名（去掉前导点并小写）
    QString wantedExt;
    if (!ext.isEmpty()) {
        wantedExt = ext;
        if (wantedExt.startsWith('.')) wantedExt.remove(0, 1);
        wantedExt = wantedExt.toLower();
    }

    // 遍历目录，尝试用当前后端逐个加载符合扩展名的文件，遇到成功则返回
    for (const auto& entry : std::filesystem::directory_iterator(modelDir)) {
        try {
            if (!entry.is_regular_file()) continue;
            const path p = entry.path();
            // 如果指定了扩展名，跳过不匹配的文件
            if (!wantedExt.isEmpty()) {
                QString fileExt = QString::fromStdWString(p.extension().wstring());
                if (fileExt.startsWith('.')) fileExt.remove(0, 1);
                if (fileExt.toLower() != wantedExt) continue;
            }

            auto code = loadModel(p);
            if (code == HRCode::Ok) { return qMakePair(code, currentBackendName()); }
        } catch (...) {
            // 忽略单个文件的异常，继续尝试下一个
            continue;
        }
    }

    return qMakePair(HRCode::ModelLoadFailed, QString());
}

QVector<QString> HumanRecognition::availableBackends() const {
    return listRegisteredBackends();
}

HRCode HumanRecognition::setBackend(const QString& backendName) {
    std::lock_guard<std::mutex> lock(m_impl->mtx);
    auto b = createBackendInstance(backendName);
    if (!b) return HRCode::UnknownError;
    // 关闭旧后端
    if (m_impl->backend) m_impl->backend->shutdown();
    m_impl->backend = std::move(b);
    m_impl->backendName = backendName;
    return HRCode::Ok;
}

QPair<IHumanRecognitionBackend*, QString> HumanRecognition::backend() {
    std::lock_guard<std::mutex> lock(m_impl->mtx);
    return qMakePair(m_impl->backend.get(), m_impl->backendName);
}

QString HumanRecognition::currentBackendName() const {
    std::lock_guard<std::mutex> lock(m_impl->mtx);
    return m_impl->backendName;
}

bool HumanRecognition::hasBackend() const {
    std::lock_guard<std::mutex> lock(m_impl->mtx);
    return static_cast<bool>(m_impl->backend);
}

HRCode HumanRecognition::resetBackend() {
    std::lock_guard<std::mutex> lock(m_impl->mtx);
    if (m_impl->backend) {
        m_impl->backend->shutdown();
        m_impl->backend.reset();
        m_impl->backendName.clear();
    }
    return HRCode::Ok;
}

HRCode HumanRecognition::detect(const QImage& image,
                                const DetectOptions& opts,
                                QVector<FaceBox>& outBoxes) {
    std::lock_guard<std::mutex> lock(m_impl->mtx);
    if (!m_impl->backend) return HRCode::UnknownError;
    return m_impl->backend->detect(image, opts, outBoxes);
}

HRCode HumanRecognition::extractFeature(const QImage& image,
                                        const FaceBox& box,
                                        FaceFeature& outFeature) {
    std::lock_guard<std::mutex> lock(m_impl->mtx);
    if (!m_impl->backend) return HRCode::UnknownError;
    return m_impl->backend->extractFeature(image, box, outFeature);
}

HRCode HumanRecognition::compare(const FaceFeature& a, const FaceFeature& b, float& outDistance) {
    std::lock_guard<std::mutex> lock(m_impl->mtx);
    if (!m_impl->backend) return HRCode::UnknownError;
    return m_impl->backend->compare(a, b, outDistance);
}

HRCode HumanRecognition::findNearest(const FaceFeature& feature, RecognitionMatch& outMatch) {
    std::lock_guard<std::mutex> lock(m_impl->mtx);
    if (!m_impl->backend) return HRCode::UnknownError;
    return m_impl->backend->findNearest(feature, outMatch);
}

HRCode HumanRecognition::registerPerson(const PersonInfo& person) {
    std::lock_guard<std::mutex> lock(m_impl->mtx);
    if (!m_impl->backend) return HRCode::UnknownError;
    return m_impl->backend->registerPerson(person);
}

HRCode HumanRecognition::removePerson(const QString& personId) {
    std::lock_guard<std::mutex> lock(m_impl->mtx);
    if (!m_impl->backend) return HRCode::UnknownError;
    return m_impl->backend->removePerson(personId);
}

HRCode HumanRecognition::getPerson(const QString& personId, PersonInfo& outPerson) {
    std::lock_guard<std::mutex> lock(m_impl->mtx);
    if (!m_impl->backend) return HRCode::UnknownError;
    return m_impl->backend->getPerson(personId, outPerson);
}

HRCode HumanRecognition::listPersons(QVector<PersonInfo>& outPersons) {
    std::lock_guard<std::mutex> lock(m_impl->mtx);
    if (!m_impl->backend) return HRCode::UnknownError;
    return m_impl->backend->listPersons(outPersons);
}

HRCode HumanRecognition::train(const QString& datasetPath) {
    std::lock_guard<std::mutex> lock(m_impl->mtx);
    if (!m_impl->backend) return HRCode::UnknownError;
    return m_impl->backend->train(datasetPath);
}

}  // namespace HumanRecognition
