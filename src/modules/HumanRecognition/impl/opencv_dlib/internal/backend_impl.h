#pragma once

#include "backend_constants.h"
#include "modules/HumanRecognition/impl/opencv_dlib/backend.h"
#if !(defined(HAS_OPENCV) && defined(HAS_DLIB))
namespace HumanRecognition {
namespace detail {
class FaceNet;
}  // namespace detail
}  // namespace HumanRecognition
#endif

#if defined(HAS_OPENCV) && defined(HAS_DLIB)

#ifdef layout
#undef layout
#endif

#include <dlib/image_processing/frontal_face_detector.h>

#include <QFileInfo>
#include <QHash>
#include <QImage>
#include <QJsonObject>
#include <QString>
#include <QVector>
#include <memory>
#include <mutex>
#include <optional>

#include "backend_network.h"

namespace spdlog {
class logger;
}  // namespace spdlog

namespace dlib {
class shape_predictor;
}  // namespace dlib

namespace HumanRecognition {

namespace test {
class ImplAccessor;
}  // namespace test

/**
 * @brief OpenCVDlibBackend 的真正实现体，封装具体算法、数据库交互与日志逻辑。
 *
 * 该类通过 PIMPL 方式隐藏在接口之后，便于后续维护与单元测试。
 * 主要职责包括：
 * - 初始化 dlib 模型（检测器、关键点预测器、特征提取网络）
 * - 维护人员信息缓存，支持增删改查
 * - 处理人脸检测与特征提取请求
 * - 计算特征距离与最近邻查找
 * - 与 SQLite 数据库交互，确保存储与读取人员信息
 * - 支持从旧版 JSON 文件导入与导出人员库
 */
class OpenCVDlibBackend::Impl {
   public:
    /**
     * @brief 构造函数，创建检测器、初始化日志指针等。
     */
    Impl();
    ~Impl();

    /**
     * @brief 根据传入配置初始化后端。
     */
    HRCode initialize(const QJsonObject& config);
    /**
     * @brief 清理资源、重置状态。
     */
    HRCode shutdown();

    /**
     * @brief 加载 dlib 模型以及人员数据库信息。
     */
    HRCode loadModel(const QString& modelPath);
    /**
     * @brief 将当前人员信息导出为 JSON。
     */
    HRCode saveModel(const QString& modelPath);

    /**
     * @brief 执行人脸检测。
     */
    HRCode detect(const QImage& image, const DetectOptions& opts, QVector<FaceBox>& outBoxes);

    /**
     * @brief 提取单个人脸的特征。
     */
    HRCode extractFeature(const QImage& image, const FaceBox& box, FaceFeature& outFeature);

    /**
     * @brief 计算两个特征的距离。
     */
    HRCode compare(const FaceFeature& a, const FaceFeature& b, float& outDistance);
    /**
     * @brief 在人员库中查找最相似的人。
     */
    HRCode findNearest(const FaceFeature& feature, RecognitionMatch& outMatch);

    /**
     * @brief 将新人员写入数据库与缓存。
     */
    HRCode registerPerson(const PersonInfo& person);
    /**
     * @brief 从数据库与缓存移除人员。
     */
    HRCode removePerson(const QString& personId);
    /**
     * @brief 从缓存读取人员信息。
     */
    HRCode getPerson(const QString& personId, PersonInfo& outPerson);

    /**
     * @brief 训练接口（预留）。
     */
    HRCode train(const QString& datasetPath);
    /**
     * @brief 返回后端名称。
     */
    QString backendName() const;

   private:
    struct ModelFiles {
        /// 模型所在目录
        QString baseDirectory;
        /// dlib 关键点模型
        QString shapePredictor;
        /// dlib 人脸特征模型
        QString recognition;
        /// 旧版 JSON 人员数据库
        QString personDatabase;
    };

    /**
     * @brief 根据输入路径推断需要的模型文件。
     */
    ModelFiles resolveModelFiles(const QFileInfo& info) const;
    /**
     * @brief 计算两个特征的欧氏距离。
     */
    std::optional<float> computeDistance(const FaceFeature& a, const FaceFeature& b) const;

    /**
     * @brief 重新读取配置中心的参数。
     */
    void refreshConfiguration();
    /**
     * @brief 确保数据库连接和必要表结构可用。
     */
    bool ensureStorageReady();
    /**
     * @brief 自动建表（当配置允许时）。
     */
    bool ensurePersonsTable();
    /**
     * @brief 将任意字符串净化为合法的表名。
     */
    QString sanitizeTableName(const QString& requested) const;

    /**
     * @brief 将元数据 JSON 序列化为字符串。
     */
    QString serializeMetadata(const QJsonObject& obj) const;
    /**
     * @brief 反序列化人员元数据。
     */
    QJsonObject deserializeMetadata(const QString& json) const;
    /**
     * @brief 序列化特征向量。
     */
    QString serializeFeature(const FaceFeature& feature) const;
    /**
     * @brief 反序列化特征向量。
     */
    std::optional<FaceFeature> deserializeFeature(const QString& json) const;

    /**
     * @brief 将人员写入数据库。
     */
    bool persistPerson(const PersonInfo& person, bool allowReplace);
    /**
     * @brief 从数据库中删除人员。
     */
    bool deletePersonFromDatabase(const QString& personId);
    /**
     * @brief 从数据库拉取全部人员缓存。
     */
    HRCode loadPersonsFromStorage();
    /**
     * @brief 从 JSON 文件导入旧版人员库。
     */
    HRCode loadPersonDatabase(const QString& jsonPath);
    /**
     * @brief 将当前人员库导出到 JSON。
     */
    HRCode savePersonDatabase(const QString& jsonPath);

    dlib::frontal_face_detector detector;
    std::unique_ptr<dlib::shape_predictor> shapePredictor;
    std::unique_ptr<HumanRecognition::detail::FaceNet> recognitionNet;
    mutable std::mutex mutex;
    QHash<QString, PersonInfo> persons;
    QString modelDirectory;
    std::shared_ptr<spdlog::logger> logger;
    QString personsTable;
    double matchThreshold;
    bool autoCreatePersonsTable = true;
    bool storageReady = false;

    friend class test::ImplAccessor;
};

}  // namespace HumanRecognition

#endif  // defined(HAS_OPENCV) && defined(HAS_DLIB)
