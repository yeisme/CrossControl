#pragma once

#include <memory>

#include "modules/HumanRecognition/ihumanrecognitionbackend.h"

namespace HumanRecognition {

namespace test {
class ImplAccessor;
}  // namespace test

/**
 * @brief 基于 OpenCV + dlib 的人脸识别后端实现
 *
 * 负责整合 dlib 的人脸检测、特征提取网络以及 OpenCV 的图像预处理能力，
 * 提供对接口 `IHumanRecognitionBackend` 的完整实现。
 */
/**
 * @class OpenCVDlibBackend
 * @brief 使用 OpenCV 进行图像预处理、结合 dlib 深度网络做特征提取的人脸识别后端。
 *
 * 该类仅承担轻量的门面角色，所有核心逻辑都委托给内部的 `Impl`。
 * 通过这种方式可以在不暴露实现细节的情况下向外提供稳定接口。
 */
class OpenCVDlibBackend : public IHumanRecognitionBackend {
   public:
    /**
     * @brief 构建后端对象并初始化内部实现。
     */
    OpenCVDlibBackend();
    /**
     * @brief 释放内部资源。
     */
    ~OpenCVDlibBackend() override;

    /**
     * @brief 按照配置初始化后端。
     *
     * @param config 通过 JSON 传入的初始化参数，例如模型目录、阈值等。
     * @return HRCode 初始化结果。
     */
    HRCode initialize(const QJsonObject& config) override;
    /**
     * @brief 关闭后端并释放模型、缓存等资源。
     */
    HRCode shutdown() override;

    /**
     * @brief 从磁盘加载 dlib 模型文件及人员数据库。
     *
     * @param modelPath 模型目录或具体文件路径。
     */
    HRCode loadModel(const QString& modelPath) override;
    /**
     * @brief 将当前人员库导出为 JSON 文件。
     *
     * @param modelPath 目标目录或文件路径。
     */
    HRCode saveModel(const QString& modelPath) override;

    /**
     * @brief 在图片中检测人脸。
     *
     * @param image 输入图像。
     * @param opts 检测参数，例如最小置信度、是否输出关键点。
     * @param outBoxes 检测结果列表。
     */
    HRCode detect(const QImage& image,
                  const DetectOptions& opts,
                  QVector<FaceBox>& outBoxes) override;

    /**
     * @brief 对指定人脸区域提取 128 维特征向量。
     */
    HRCode extractFeature(const QImage& image,
                          const FaceBox& box,
                          FaceFeature& outFeature) override;

    /**
     * @brief 计算两个特征向量之间的特征距离。
     */
    HRCode compare(const FaceFeature& a, const FaceFeature& b, float& outDistance) override;

    /**
     * @brief 在人员库中查找与指定特征最相似的人员。
     */
    HRCode findNearest(const FaceFeature& feature, RecognitionMatch& outMatch) override;

    /**
     * @brief 将人员信息写入数据库并刷新缓存。
     */
    HRCode registerPerson(const PersonInfo& person) override;
    /**
     * @brief 从数据库和缓存中删除指定人员。
     */
    HRCode removePerson(const QString& personId) override;
    /**
     * @brief 根据 id 查询人员信息。
     */
    HRCode getPerson(const QString& personId, PersonInfo& outPerson) override;

    /**
     * @brief 列出当前缓存中的所有人员记录。
     */
    HRCode listPersons(QVector<PersonInfo>& outPersons) override;

    /**
     * @brief 预留的训练接口，目前未实现。
     */
    HRCode train(const QString& datasetPath) override;

    /**
     * @brief 返回后端名称标识。
     */
    QString backendName() const override;

   private:
    class Impl;
    std::unique_ptr<Impl> d;

    friend class test::ImplAccessor;
};

}  // namespace HumanRecognition
