/**
 * @file types.h
 * @brief 数据类型定义（枚举、结构体）
 */
#pragma once

#include <QImage>
#include <QJsonObject>
#include <QPointF>
#include <QRect>
#include <QString>
#include <QVector>
#include <optional>

namespace HumanRecognition {

/**
 * @enum HRCode
 * @brief 人脸识别模块通用错误/状态码
 *
 * 表示模块操作的返回状态，供上层与后端统一使用。
 */
enum class HRCode {
    Ok = 0,
    InvalidImage,
    ModelLoadFailed,
    ModelSaveFailed,
    DetectFailed,
    ExtractFeatureFailed,
    CompareFailed,
    TrainFailed,
    PersonExists,
    PersonNotFound,
    UnknownError
};

/**
 * @struct FaceBox
 * @brief 表示一次人脸检测得到的框与关键点
 *
 * @var FaceBox::rect 人脸所在的像素矩形
 * @var FaceBox::score 检测置信度（通常 0.0 - 1.0）
 * @var FaceBox::landmarks 可选的人脸关键点集合
 */
struct FaceBox {
    QRect rect;                  ///< 人脸所在的像素矩形（x,y,width,height）
    float score = 0.0f;          ///< 检测置信度（通常范围 0.0 - 1.0）
    QVector<QPointF> landmarks;  ///< 可选关键点集合（例如 5 点或 68 点）
};

/**
 * @struct FaceFeature
 * @brief 人脸特征向量（用于比对与检索）
 *
 * @var FaceFeature::values 特征向量数值（float 列表）
 * @var FaceFeature::version 特征版本/格式标识
 * @var FaceFeature::norm 可选的向量范数（例如 L2）
 */
struct FaceFeature {
    QVector<float> values;      ///< 特征向量数值数组（浮点）
    QString version;            ///< 特征向量格式或版本标识，用于兼容性检查
    std::optional<float> norm;  ///< 可选的向量范数（例如 L2），如果提供可避免重复计算
};

/**
 * @struct RecognitionMatch
 * @brief 表示一次识别/检索得到的匹配信息
 *
 * 包括匹配到的人员 ID、名称以及用于度量相似度的数值。
 */
struct RecognitionMatch {
    QString personId;    ///< 匹配到的人员唯一 ID
    QString personName;  ///< 匹配到的人员显示名称
    float distance = 0;  ///< 特征距离或相似度度量（语义由后端决定，通常值越小表示越相似）
    float score = 0;     ///< 归一化置信度分数（可选，越大越可信）
};

/**
 * @struct RecognitionResult
 * @brief 单次检测（人脸）对应的完整识别结果
 *
 * 包含检测框、可选的特征向量与可选的匹配信息。
 */
struct RecognitionResult {
    FaceBox face;                           ///< 检测到的人脸框及关键点
    std::optional<FaceFeature> feat;        ///< 可选的特征向量（如果已提取）
    std::optional<RecognitionMatch> match;  ///< 可选的识别匹配信息（如果已检索）
};

/**
 * @struct PersonInfo
 * @brief 描述登记在库中的人员信息
 *
 * 包含人员唯一 ID、姓名、可扩展元数据以及可选的标准特征。
 */
struct PersonInfo {
    QString id;                                   ///< 人员唯一标识符
    QString name;                                 ///< 人员名称或备注
    QJsonObject metadata;                         ///< 可扩展元数据（JSON 对象）
    std::optional<FaceFeature> canonicalFeature;  ///< 可选的标准特征向量，用于快速比对
};

/**
 * @struct DetectOptions
 * @brief 人脸检测的可选参数
 *
 * detectLandmarks: 是否同时检测关键点；minScore: 置信度阈值；resizeTo: 可选缩放尺寸。
 */
struct DetectOptions {
    bool detectLandmarks = true;   ///< 是否同时检测关键点（默认开启）
    float minScore = 0.3f;         ///< 最低置信度阈值，低于该值的检测结果可被忽略
    QSize resizeTo = QSize(0, 0);  ///< 可选的缩放尺寸；(0,0) 表示不缩放
};

}  // namespace HumanRecognition
