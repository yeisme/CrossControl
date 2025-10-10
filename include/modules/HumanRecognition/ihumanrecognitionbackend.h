/**
 * @file ihumanrecognitionbackend.h
 * @brief 后端抽象接口，需要由具体实现继承
 */
#pragma once

#include <QJsonObject>

#include "types.h"

namespace HumanRecognition {

class IHumanRecognitionBackend {
   public:
    virtual ~IHumanRecognitionBackend() = default;

    /**
     * @brief 可选的初始化回调
     * @param config JSON 配置对象，后端可从中读取初始化参数
     * @return 返回 HRCode 指示初始化结果
     */
    virtual HRCode initialize(const QJsonObject& config) { return HRCode::Ok; }

    /**
     * @brief 可选的关闭回调，用于释放资源
     */
    virtual HRCode shutdown() { return HRCode::Ok; }

    // 模型管理

    /**
     * @brief 加载模型/数据库
     * @param modelPath 模型文件路径或数据库路径
     * @return HRCode 表示操作结果
     */
    virtual HRCode loadModel(const QString& modelPath) = 0;

    /**
     * @brief 保存模型/数据库
     * @param modelPath 目标路径
     * @return HRCode 表示操作结果
     */
    virtual HRCode saveModel(const QString& modelPath) = 0;

    // 检测/特征/比对/查找

    /**
     * @brief 对输入图像执行人脸检测
     * @param image 输入图像
     * @param opts 检测参数
     * @param outBoxes 输出检测到的人脸列表
     * @return HRCode 表示操作结果
     */
    virtual HRCode detect(const QImage& image,
                          const DetectOptions& opts,
                          QVector<FaceBox>& outBoxes) = 0;

    /**
     * @brief 提取指定人脸框的特征向量
     * @param image 原始图像
     * @param box 人脸框
     * @param outFeature 输出特征
     * @return HRCode 表示操作结果
     */
    virtual HRCode extractFeature(const QImage& image,
                                  const FaceBox& box,
                                  FaceFeature& outFeature) = 0;

    /**
     * @brief 比较两个特征向量并返回距离/相似度
     * @param a 特征 A
     * @param b 特征 B
     * @param outDistance 输出距离（或相似度，语义由后端决定）
     * @return HRCode 表示操作结果
     */
    virtual HRCode compare(const FaceFeature& a, const FaceFeature& b, float& outDistance) = 0;

    /**
     * @brief 在数据库中查找与给定特征最相近的人员
     * @param feature 查询特征
     * @param outMatch 输出匹配信息
     * @return HRCode 表示操作结果
     */
    virtual HRCode findNearest(const FaceFeature& feature, RecognitionMatch& outMatch) = 0;

    // 人员管理

    /**
     * @brief 向数据库中注册人员信息
     * @param person 人员记录（应包含 id）
     * @return HRCode 表示操作结果
     */
    virtual HRCode registerPerson(const PersonInfo& person) = 0;

    /**
     * @brief 从数据库移除指定人员
     * @param personId 人员唯一 ID
     * @return HRCode 表示操作结果
     */
    virtual HRCode removePerson(const QString& personId) = 0;

    /**
     * @brief 查询人员信息
     * @param personId 要查询的人员 ID
     * @param outPerson 输出参数，返回找到的人员信息
     * @return HRCode 表示操作结果
     */
    virtual HRCode getPerson(const QString& personId, PersonInfo& outPerson) = 0;

    /**
     * @brief 列出当前数据库中的所有人员记录。
     * @param outPersons 输出参数，填充为完整的人员信息列表。
     * @return HRCode 表示操作结果。
     */
    virtual HRCode listPersons(QVector<PersonInfo>& outPersons) = 0;

    // 训练（可选实现）

    /**
     * @brief 可选的训练接口，后端可根据需要覆盖实现
     * @param datasetPath 训练数据路径
     * @return HRCode 表示操作结果
     */
    virtual HRCode train(const QString& datasetPath) { return HRCode::UnknownError; }

    /**
     * @brief 返回后端名称（用于显示或调试）
     */
    virtual QString backendName() const = 0;
};

}  // namespace HumanRecognition
