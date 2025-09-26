/**
 * @file humanrecognition.h
 * @brief HumanRecognition类的头文件，提供人类检测和识别功能。
 *
 * 此文件定义了HumanRecognition类，这是一个基于Qt的模块，用于处理
 * 人类检测、人脸识别和模型训练。
 *
 * @author yefun2004@gmail.com
 * @date 2025
 * @version 1.0
 */

#ifndef HUMANRECOGNITION_H
#define HUMANRECOGNITION_H

#include <QImage>
#include <QObject>
#include <QRect>
#include <QString>
#include <memory>
#include <vector>

#include "iface_human_recognition.h"
#include "logging.h"

#ifdef HumanRecognition_EXPORTS
#define HUMANRECOGNITION_EXPORT Q_DECL_EXPORT
#else
#define HUMANRECOGNITION_EXPORT Q_DECL_IMPORT
#endif

static constexpr const char* LogModuleName = "HumanRecognition";

/**
 * @brief 日志记录器的引用。
 *
 * 此引用用于记录HumanRecognition模块中的日志消息。这里需要使用 inline 保证
 * 日志记录器在头文件中的唯一性。需要 C++17 支持。
 */
inline auto MCLog = *logging::LoggerManager::instance().getLogger(LogModuleName);

/**
 * @class HumanRecognition
 * @brief 人类检测和识别功能的类。
 *
 * HumanRecognition类提供了在图像中检测人类、从人脸图像识别个人以及训练识别模型的方法。
 * 目前实现为带有模拟功能的占位符。
 *
 * 它继承自QObject以支持Qt的信号-槽机制，可以在多线程环境中使用。
 *
 * @note 这是一个占位符实现。实际的计算机视觉算法应该在未来版本中集成。
 */
class HUMANRECOGNITION_EXPORT HumanRecognition : public QObject {
    Q_OBJECT

   public:
    /**
     * @brief HumanRecognition的构造函数。
     *
     * 初始化人类识别模块。目前执行基本设置
     * 和日志初始化。
     *
     * @param parent 父QObject。默认为nullptr。
     */
    explicit HumanRecognition(QObject* parent = nullptr);

    /**
     * @brief 检测给定图像中的人类。
     *
     * 此方法分析输入图像以检测人类的存在。
     * 目前实现为一个模拟检测的占位符。
     *
     * @param image 要分析的人类检测输入图像。
     * @return 如果检测到人类（模拟），则返回true，否则返回false。
     *
     * @note 这是一个占位符实现。在真实实现中，
     *       这将使用计算机视觉算法，如HOG、CNN等。
     */
    bool detectHumans(const QImage& image);  // 兼容旧接口（仅返回是否检测到 >=1）

    /**
     * @brief 从人脸图像中识别个人。
     *
     * 通过将提供的人脸图像与识别数据库中的已知人脸进行比较，
     * 尝试识别图像中的个人。
     *
     * @param faceImage 包含要识别的人脸的图像。
     * @return 包含被识别个人姓名的QString，如果未识别则返回“Unknown Person”。
     *
     * @note 这是一个占位符实现。真实的识别将涉及
     *       人脸嵌入提取和数据库匹配。
     */
    QString recognizePerson(const QImage& faceImage);

    // 一张图片内检测 + 识别所有人脸
    std::vector<FaceDetectionResult> detectAndRecognize(const QImage& image);

    // 注册(训练) —— 传入人员信息和该人员的人脸图片集合
    int enroll(const PersonInfo& info, const std::vector<QImage>& faces);

    // 阈值调节 & 视频帧处理封装
    void setThreshold(float t) {
        if (m_service) m_service->setThreshold(t);
    }
    float threshold() const { return m_service ? m_service->threshold() : 0.f; }
    std::vector<FaceDetectionResult> processFrame(const QImage& frame, bool recognize = true) {
        if (!m_isInitialized) return {};
        return m_service->processFrame(frame, recognize);
    }

    bool save();
    bool load();

    // 人员信息管理（UI 使用）
    std::vector<PersonInfo> listPersons() {
        return m_service ? m_service->listPersons() : std::vector<PersonInfo>{};
    }
    bool updatePersonInfo(const PersonInfo& p) {
        return m_service ? m_service->updatePersonInfo(p) : false;
    }
    bool deletePerson(const QString& id) { return m_service ? m_service->deletePerson(id) : false; }
    std::vector<FaceEmbedding> getPersonFeatures(const QString& id) {
        return m_service ? m_service->getPersonFeatures(id) : std::vector<FaceEmbedding>{};
    }

    /**
     * @brief 使用数据集训练识别模型。
     *
     * 此方法将使用提供的数据集训练基础机器学习模型。
     * 目前实现为一个占位符。
     *
     * @param datasetPath 文件系统中训练数据集目录的路径。
     *
     * @note 这是一个占位符实现。实际训练需要
     *       大量数据集和显著的计算资源。
     */
    void trainModel(const QString& datasetPath);

   signals:
    /**
     * @brief 检测到图像中的人类时发出的信号。
     *
     * 此信号在进行人类检测时发出，以通知监听者
     * 有关检测到的人类边界框的信息。
     *
     * @param boundingBox 包含检测到人类的矩形区域。
     */
    void humanDetected(const QRect& boundingBox);

    /**
     * @brief 个人识别完成时发出的信号。
     *
     * 此信号在尝试从人脸图像识别个人后发出，
     * 提供结果。
     *
     * @param personName 被识别个人的姓名或"Unknown Person"。
     */
    void recognitionCompleted(const QString& personName);

   private:
    /**
     * @brief 私有成员，指示模块是否已初始化。
     *
     * 跟踪识别模块的初始化状态。
     */
    bool m_isInitialized;
    std::unique_ptr<HumanRecognitionService> m_service;  // 后端服务（OpenCV 等）
};

#endif  // HUMANRECOGNITION_H