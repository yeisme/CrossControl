#include "modules/HumanRecognition/impl/opencv_dlib/backend.h"

#include "internal/backend_constants.h"
#include "logging/logging.h"
#include "modules/HumanRecognition/factory.h"

namespace {
std::shared_ptr<spdlog::logger> backendLogger() {
    return logging::LoggerManager::instance().getLogger("HumanRecognition.OpenCVDlib");
}
}  // namespace

#if defined(HAS_OPENCV) && defined(HAS_DLIB)

#include "internal/backend_impl.h"

namespace HumanRecognition {

OpenCVDlibBackend::OpenCVDlibBackend() : d(std::make_unique<Impl>()) {}
OpenCVDlibBackend::~OpenCVDlibBackend() = default;

HRCode OpenCVDlibBackend::initialize(const QJsonObject& config) {
    return d->initialize(config);
}

HRCode OpenCVDlibBackend::shutdown() {
    return d->shutdown();
}

HRCode OpenCVDlibBackend::loadModel(const QString& modelPath) {
    return d->loadModel(modelPath);
}

HRCode OpenCVDlibBackend::saveModel(const QString& modelPath) {
    return d->saveModel(modelPath);
}

HRCode OpenCVDlibBackend::detect(const QImage& image,
                                 const DetectOptions& opts,
                                 QVector<FaceBox>& outBoxes) {
    return d->detect(image, opts, outBoxes);
}

HRCode OpenCVDlibBackend::extractFeature(const QImage& image,
                                         const FaceBox& box,
                                         FaceFeature& outFeature) {
    return d->extractFeature(image, box, outFeature);
}

HRCode OpenCVDlibBackend::compare(const FaceFeature& a, const FaceFeature& b, float& outDistance) {
    return d->compare(a, b, outDistance);
}

HRCode OpenCVDlibBackend::findNearest(const FaceFeature& feature, RecognitionMatch& outMatch) {
    return d->findNearest(feature, outMatch);
}

HRCode OpenCVDlibBackend::registerPerson(const PersonInfo& person) {
    return d->registerPerson(person);
}

HRCode OpenCVDlibBackend::removePerson(const QString& personId) {
    return d->removePerson(personId);
}

HRCode OpenCVDlibBackend::getPerson(const QString& personId, PersonInfo& outPerson) {
    return d->getPerson(personId, outPerson);
}

HRCode OpenCVDlibBackend::train(const QString& datasetPath) {
    return d->train(datasetPath);
}

QString OpenCVDlibBackend::backendName() const {
    return d->backendName();
}

namespace {
struct BackendRegistrar {
    BackendRegistrar() {
        registerBackendFactory(QString::fromLatin1(opencv_dlib::kBackendName),
                               []() -> std::unique_ptr<IHumanRecognitionBackend> {
                                   return std::make_unique<OpenCVDlibBackend>();
                               });

        registerBackendFactory(
            QString::fromLatin1(opencv_dlib::kBackendName),
            [](const QJsonObject& config) -> std::unique_ptr<IHumanRecognitionBackend> {
                auto backend = std::make_unique<OpenCVDlibBackend>();
                if (backend->initialize(config) != HRCode::Ok) {
                    if (auto logger = backendLogger()) {
                        logger->error(
                            "OpenCVDlibBackend initialization failed for configured factory");
                    }
                    return {};
                }
                return backend;
            });
    }

    ~BackendRegistrar() {
        unregisterBackendFactory(QString::fromLatin1(opencv_dlib::kBackendName));
    }
};

static BackendRegistrar s_registrar;
}  // namespace

}  // namespace HumanRecognition

#else  // defined(HAS_OPENCV) && defined(HAS_DLIB)

namespace HumanRecognition {

class OpenCVDlibBackend::Impl {};

OpenCVDlibBackend::OpenCVDlibBackend() = default;
OpenCVDlibBackend::~OpenCVDlibBackend() = default;

HRCode OpenCVDlibBackend::initialize(const QJsonObject& config) {
    Q_UNUSED(config);
    if (auto logger = backendLogger()) {
        logger->error("OpenCVDlibBackend unavailable: OpenCV/dlib support disabled at build time");
    }
    return HRCode::UnknownError;
}

HRCode OpenCVDlibBackend::shutdown() {
    return HRCode::UnknownError;
}

HRCode OpenCVDlibBackend::loadModel(const QString&) {
    if (auto logger = backendLogger()) {
        logger->error(
            "OpenCVDlibBackend loadModel called but backend is unavailable at build time");
    }
    return HRCode::ModelLoadFailed;
}

HRCode OpenCVDlibBackend::saveModel(const QString&) {
    if (auto logger = backendLogger()) {
        logger->error(
            "OpenCVDlibBackend saveModel called but backend is unavailable at build time");
    }
    return HRCode::ModelSaveFailed;
}

HRCode OpenCVDlibBackend::detect(const QImage&, const DetectOptions&, QVector<FaceBox>&) {
    if (auto logger = backendLogger()) {
        logger->warn("OpenCVDlibBackend detect invoked without backend availability");
    }
    return HRCode::DetectFailed;
}

HRCode OpenCVDlibBackend::extractFeature(const QImage&, const FaceBox&, FaceFeature&) {
    if (auto logger = backendLogger()) {
        logger->warn("OpenCVDlibBackend extractFeature invoked without backend availability");
    }
    return HRCode::ExtractFeatureFailed;
}

HRCode OpenCVDlibBackend::compare(const FaceFeature&, const FaceFeature&, float& outDistance) {
    outDistance = 0.0f;
    if (auto logger = backendLogger()) {
        logger->warn("OpenCVDlibBackend compare invoked without backend availability");
    }
    return HRCode::CompareFailed;
}

HRCode OpenCVDlibBackend::findNearest(const FaceFeature&, RecognitionMatch&) {
    if (auto logger = backendLogger()) {
        logger->warn("OpenCVDlibBackend findNearest invoked without backend availability");
    }
    return HRCode::PersonNotFound;
}

HRCode OpenCVDlibBackend::registerPerson(const PersonInfo&) {
    if (auto logger = backendLogger()) {
        logger->warn("OpenCVDlibBackend registerPerson invoked without backend availability");
    }
    return HRCode::UnknownError;
}

HRCode OpenCVDlibBackend::removePerson(const QString&) {
    if (auto logger = backendLogger()) {
        logger->warn("OpenCVDlibBackend removePerson invoked without backend availability");
    }
    return HRCode::UnknownError;
}

HRCode OpenCVDlibBackend::getPerson(const QString&, PersonInfo&) {
    if (auto logger = backendLogger()) {
        logger->warn("OpenCVDlibBackend getPerson invoked without backend availability");
    }
    return HRCode::PersonNotFound;
}

HRCode OpenCVDlibBackend::train(const QString&) {
    if (auto logger = backendLogger()) {
        logger->warn("OpenCVDlibBackend train invoked without backend availability");
    }
    return HRCode::UnknownError;
}

QString OpenCVDlibBackend::backendName() const {
    return QString::fromLatin1(opencv_dlib::kBackendName);
}

}  // namespace HumanRecognition

#endif  // defined(HAS_OPENCV) && defined(HAS_DLIB)
