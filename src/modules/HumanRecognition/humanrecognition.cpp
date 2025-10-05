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

QPair<HRCode, QString> HumanRecognition::loadModelFromDir(const path& modelDir) {
    // 简单实现：尝试使用当前后端加载目录下的模型文件名
    if (!hasBackend()) return {HRCode::UnknownError, QString()};
    // 这里只调用 loadModel 并返回当前后端名
    auto code = loadModel(modelDir);
    return qMakePair(code, currentBackendName());
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

HRCode HumanRecognition::train(const QString& datasetPath) {
    std::lock_guard<std::mutex> lock(m_impl->mtx);
    if (!m_impl->backend) return HRCode::UnknownError;
    return m_impl->backend->train(datasetPath);
}

}  // namespace HumanRecognition
