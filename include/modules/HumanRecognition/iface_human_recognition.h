/**
 * @file iface_human_recognition.h
 * @author your name (you@domain.com)
 * @brief 人脸/人体识别模块的抽象接口定义，便于支持多种底层实现（OpenCV、ONNX、其他推理引擎等）。
 * @version 0.1
 * @date 2025-09-26
 *
 * @copyright Copyright (c) 2025
 *
 */

#pragma once

#include <QImage>
#include <QRect>
#include <QString>
#include <QVector>
#include <optional>
#include <vector>

// 人脸特征向量类型（可根据需要替换为 Eigen::VectorXf / std::array<float,N>）
using FaceEmbedding = std::vector<float>;

/**
 * @brief 单个人脸检测结果（包含位置与可选特征）
 */
struct FaceDetectionResult {
    QRect box;                    ///< 人脸边界框
    float score{0.f};             ///< 置信度（0~1）
    FaceEmbedding embedding;      ///< 可选特征（如已提取）
    QString matchedPersonId;      ///< 如果已匹配成功，填入人员唯一 ID
    QString matchedPersonName;    ///< 如果已匹配成功，填入人员姓名
    float matchedDistance{-1.f};  ///< 与匹配条目的距离/相似度（数值语义由实现决定）
};

/**
 * @brief 注册(训练)时提供的人员信息
 */
struct PersonInfo {
    QString personId;   ///< 业务唯一 ID（如工号/UUID）
    QString name;       ///< 姓名
    QString extraJson;  ///< 额外信息(JSON 字符串，可为空)
};

/**
 * @brief 抽象接口：人脸识别生命周期 & 功能
 */
class IHumanRecognitionBackend {
   public:
    virtual ~IHumanRecognitionBackend() = default;

    // 初始化资源（模型加载、配置等）
    virtual bool initialize() = 0;

    // 释放资源
    virtual void shutdown() = 0;

    // 从单张图片检测所有人脸（返回 N 个结果，不做身份匹配）
    virtual std::vector<FaceDetectionResult> detectFaces(const QImage& image) = 0;

    // 从裁剪/单独的人脸图像提取特征
    virtual std::optional<FaceEmbedding> extractEmbedding(const QImage& faceImage) = 0;

    // 批量注册：给定一个人员信息与其若干人脸图片（或已经裁剪），返回成功数量
    virtual int enrollPerson(const PersonInfo& info, const std::vector<QImage>& faceImages) = 0;

    // 单次查询：输入原始图像，内部完成检测 + 特征提取 + 匹配，返回带匹配信息的结果
    virtual std::vector<FaceDetectionResult> detectAndRecognize(const QImage& image) = 0;

    // 根据 embedding 与已注册库匹配，返回最匹配条目（如无匹配返回 std::nullopt）
    virtual std::optional<FaceDetectionResult> matchEmbedding(const FaceEmbedding& emb) = 0;

    // 处理视频流帧：recognize=true 表示执行检测+识别，否则只检测
    virtual std::vector<FaceDetectionResult> processFrame(const QImage& frame,
                                                          bool recognize = true) = 0;

    // 识别阈值（距离 <= threshold 视为同一人 / 或者相似度 >= threshold）
    virtual void setRecognitionThreshold(float thr) = 0;
    virtual float recognitionThreshold() const = 0;

    // 持久化当前已注册特征数据（例如落库或写文件）
    virtual bool saveFeatureDatabase() = 0;

    // 从持久化介质重新加载
    virtual bool loadFeatureDatabase() = 0;

    // ----- 人员信息管理接口（高层业务/UI 使用） -----
    // 列出已知人员（从 person 表或从 feature 表去重）
    virtual std::vector<PersonInfo> listPersons() = 0;

    // 更新人员信息（如姓名/extraJson），如果不存在则插入
    virtual bool updatePersonInfo(const PersonInfo& info) = 0;

    // 删除人员（同时删除其对应的 feature 条目）
    virtual bool deletePerson(const QString& personId) = 0;

    // 获取某人的所有特征（用于展示/迁移）
    virtual std::vector<FaceEmbedding> getPersonFeatures(const QString& personId) = 0;
};

/**
 * @brief 用作工厂/策略包装的高层接口（可作为 UI/业务层依赖）
 * 未来可以通过依赖注入替换不同后端
 */
class HumanRecognitionService {
   public:
    explicit HumanRecognitionService(std::unique_ptr<IHumanRecognitionBackend> backend)
        : m_backend(std::move(backend)) {}

    bool init() { return m_backend && m_backend->initialize(); }
    void shutdown() {
        if (m_backend) m_backend->shutdown();
    }

    std::vector<FaceDetectionResult> detect(const QImage& img) {
        return m_backend->detectFaces(img);
    }
    std::vector<FaceDetectionResult> detectAndRecognize(const QImage& img) {
        return m_backend->detectAndRecognize(img);
    }
    int enroll(const PersonInfo& p, const std::vector<QImage>& faces) {
        return m_backend->enrollPerson(p, faces);
    }
    bool save() { return m_backend->saveFeatureDatabase(); }
    bool load() { return m_backend->loadFeatureDatabase(); }

    // Person management proxies
    std::vector<PersonInfo> listPersons() { return m_backend->listPersons(); }
    bool updatePersonInfo(const PersonInfo& p) { return m_backend->updatePersonInfo(p); }
    bool deletePerson(const QString& id) { return m_backend->deletePerson(id); }
    std::vector<FaceEmbedding> getPersonFeatures(const QString& id) {
        return m_backend->getPersonFeatures(id);
    }

    void setThreshold(float t) {
        if (m_backend) m_backend->setRecognitionThreshold(t);
    }
    float threshold() const { return m_backend ? m_backend->recognitionThreshold() : 0.f; }

    std::vector<FaceDetectionResult> processFrame(const QImage& frame, bool recognize = true) {
        return m_backend->processFrame(frame, recognize);
    }

   private:
    std::unique_ptr<IHumanRecognitionBackend> m_backend;
};
