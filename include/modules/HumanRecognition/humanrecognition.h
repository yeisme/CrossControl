/**
 * @file humanrecognition.h
 * @brief HumanRecognition 模块接口与数据结构定义
 *
 * 本文件定义了人脸检测与识别模块所需的公共数据结构以及后端抽象接口
 * `IHumanRecognitionBackend`。上层通过 `HumanRecognition` 包装类持有并调用后端。
 *
 * 设计目标：
 *  - 清晰分离后端实现与上层调用（依赖反转）
 *  - 提供常见的数据结构（检测框、关键点、特征向量、识别结果、人员记录）
 *  - 提供基本的错误/状态码与可扩展配置
 *
 * @author: yefun2004@gmail.com
 * @date: 2025
 */

#pragma once

#include <QImage>
#include <QJsonObject>
#include <QList>
#include <QPair>
#include <QPointF>
#include <QRect>
#include <QString>
#include <QVector>
#include <filesystem>
#include <memory>

#include "factory.h"
#include "ihumanrecognitionbackend.h"
#include "types.h"

namespace HumanRecognition {

using namespace std::filesystem;

/**
 * @class HumanRecognition
 * @brief 人脸识别模块的单例外观（Facade）
 *
 * 该类封装了后端的选择、模型管理、人脸检测/特征/比对以及人员库管理等常用功能。
 * 建议通过 HumanRecognition::instance() 全局访问，并在多线程场景中确保正确调用序列。
 */
class HumanRecognition {
   public:
    /**
     * @brief 获取全局单例（线程安全懒初始化）
     * @return 模块单例引用
     */
    static HumanRecognition& instance();

    /**
     * @brief 构造函数（允许传入初始后端工厂列表）
     * @param backendFactories 可选的后端工厂列表（name, factory）
     */
    HumanRecognition(const QList<QPair<QString, BackendFactory>>& backendFactories = {});

    // 不允许拷贝与赋值
    HumanRecognition(const HumanRecognition&) = delete;
    HumanRecognition& operator=(const HumanRecognition&) = delete;

    /**
     * @brief 加载模型/数据库文件
     * @param modelPath 要加载的模型文件或目录路径
     * @return HRCode 操作结果（例如 HRCode::Ok 表示成功）
     */
    HRCode loadModel(const path& modelPath);

    /**
     * @brief 将当前模型/数据库保存到指定路径
     * @param modelPath 目标路径
     * @return HRCode 操作结果
     */
    HRCode saveModel(const path& modelPath);

    /**
     * @brief 从目录加载模型（尝试按后端约定查找并加载模型文件）
     * @param modelDir 模型目录路径
     * @return QPair<HRCode, QString> 返回操作结果和所用后端名称
     */
    QPair<HRCode, QString> loadModelFromDir(const path& modelDir, const QString& ext = QString());

    /**
     * @brief 列出当前注册的可用后端名称
     * @return QVector<QString> 名称列表（稳定快照）
     */
    QVector<QString> availableBackends() const;

    /**
     * @brief 设置并切换到指定后端
     * @param backendName 后端名称（必须为已注册的名称）
     * @return HRCode 操作结果（失败时通常返回 UnknownError）
     */
    HRCode setBackend(const QString& backendName);

    /**
     * @brief 获取当前后端实例指针与名称
     * @return QPair<IHumanRecognitionBackend*, QString>
     * 指向当前后端的裸指针与其名称（不转移所有权）
     */
    QPair<IHumanRecognitionBackend*, QString> backend();

    /**
     * @brief 获取当前后端名
     * @return QString 当前后端名称（如果未设置则为空）
     */
    QString currentBackendName() const;

    /**
     * @brief 检查是否已设置后端
     * @return true 如果存在后端实例
     */
    bool hasBackend() const;

    /**
     * @brief 重置并关闭当前后端（如果有）
     * @return HRCode 操作结果
     */
    HRCode resetBackend();

    /**
     * @brief 在图片上执行人脸检测
     * @param image 输入图像
     * @param opts 检测参数（是否检测关键点、阈值、缩放等）
     * @param outBoxes 输出检测到的人脸列表（由调用者提供容器）
     * @return HRCode 操作结果
     */
    HRCode detect(const QImage& image, const DetectOptions& opts, QVector<FaceBox>& outBoxes);

    /**
     * @brief 提取指定人脸框的特征向量
     * @param image 原始图像
     * @param box 要提取特征的人脸框
     * @param outFeature 输出特征（由调用者提供）
     * @return HRCode 操作结果
     */
    HRCode extractFeature(const QImage& image, const FaceBox& box, FaceFeature& outFeature);

    /**
     * @brief 比较两个特征向量并返回距离/相似度
     * @param a 特征 A
     * @param b 特征 B
     * @param outDistance 输出距离（语义由后端决定）
     * @return HRCode 操作结果
     */
    HRCode compare(const FaceFeature& a, const FaceFeature& b, float& outDistance);

    /**
     * @brief 在人员库中查找与给定特征最相近的记录
     * @param feature 查询特征
     * @param outMatch 输出匹配信息（id/name/distance/score）
     * @return HRCode 操作结果
     */
    HRCode findNearest(const FaceFeature& feature, RecognitionMatch& outMatch);

    /**
     * @brief 向人员库注册一条人员记录
     * @param person 要注册的人员信息（应包含唯一 id）
     * @return HRCode 操作结果（例如 PersonExists）
     */
    HRCode registerPerson(const PersonInfo& person);

    /**
     * @brief 从人员库移除指定 id 的记录
     * @param personId 要移除的人员 id
     * @return HRCode 操作结果
     */
    HRCode removePerson(const QString& personId);

    /**
     * @brief 查询人员信息
     * @param personId 查询的人员 id
     * @param outPerson 输出找到的人员信息
     * @return HRCode 操作结果（未找到时返回 PersonNotFound）
     */
    HRCode getPerson(const QString& personId, PersonInfo& outPerson);

    /**
     * @brief 触发后端的训练流程（如果后端实现了 train）
     */
    HRCode train(const QString& datasetPath);

    ~HumanRecognition();

   private:
    // pimpl 或者内部状态在 .cpp 中实现
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

}  // namespace HumanRecognition
