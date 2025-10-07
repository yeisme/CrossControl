#include "internal/backend_impl.h"
#include "logging/logging.h"

#if defined(HAS_OPENCV) && defined(HAS_DLIB)

#include <dlib/image_processing/shape_predictor.h>
#include <dlib/image_transforms.h>

#include <QPointF>
#include <QRect>
#include <QSize>
#include <algorithm>
#include <cmath>
#include <limits>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <vector>

namespace HumanRecognition {

namespace {

/**
 * @brief 将 Qt 的 QImage 转换为 OpenCV 的 BGR 格式 Mat。
 *
 * dlib 的处理流程依赖 RGB 排列，因此后续还会做一次色彩空间转换。
 */
cv::Mat qImageToBgrMat(const QImage& image) {
    if (image.isNull()) return {};
    QImage converted = image.format() == QImage::Format_RGB888
                           ? image
                           : image.convertToFormat(QImage::Format_RGB888);
    cv::Mat rgb(converted.height(),                         // rows
                converted.width(),                          // cols
                CV_8UC3,                                    // 3-channel uchar
                const_cast<uchar*>(converted.constBits()),  // data pointer
                converted.bytesPerLine());                  // stride
    cv::Mat bgr;
    cv::cvtColor(rgb, bgr, cv::COLOR_RGB2BGR);
    return bgr;
}

/**
 * @brief 将检测结果从缩放后的坐标系映射回原图。
 */
QRect scaleRectToOriginal(const dlib::rectangle& rect,
                          double scaleX,
                          double scaleY,
                          const QSize& originalSize) {
    if (scaleX <= 0.0 || scaleY <= 0.0) { return {}; }

    const auto toOriginal = [](double value, double scale, int maxVal) {
        double unscaled = value / scale;
        unscaled = std::clamp(unscaled, 0.0, static_cast<double>(maxVal));
        return static_cast<int>(std::round(unscaled));
    };

    const int left = toOriginal(rect.left(), scaleX, originalSize.width());
    const int top = toOriginal(rect.top(), scaleY, originalSize.height());
    const int right = toOriginal(rect.right(), scaleX, originalSize.width());
    const int bottom = toOriginal(rect.bottom(), scaleY, originalSize.height());
    return QRect(QPoint(left, top), QPoint(right, bottom));
}

/**
 * @brief 还原关键点坐标至原图（带边界夹取）。
 */
QVector<QPointF> scaleLandmarksToOriginal(const dlib::full_object_detection& shape,
                                          double scaleX,
                                          double scaleY,
                                          const QSize& originalSize) {
    QVector<QPointF> pts;
    pts.reserve(shape.num_parts());
    if (scaleX <= 0.0 || scaleY <= 0.0) return pts;

    for (unsigned long i = 0; i < shape.num_parts(); ++i) {
        double x = std::clamp(static_cast<double>(shape.part(i).x()) / scaleX,
                              0.0,
                              static_cast<double>(originalSize.width()));
        double y = std::clamp(static_cast<double>(shape.part(i).y()) / scaleY,
                              0.0,
                              static_cast<double>(originalSize.height()));
        pts.append(QPointF{x, y});
    }
    return pts;
}

/**
 * @brief 将 OpenCV Mat 转换为 dlib RGB 矩阵。
 */
dlib::matrix<dlib::rgb_pixel> matToDlibRgb(const cv::Mat& bgr) {
    if (bgr.empty()) return {};
    dlib::matrix<dlib::rgb_pixel> out(bgr.rows, bgr.cols);
    for (int r = 0; r < bgr.rows; ++r) {
        const auto* rowPtr = bgr.ptr<cv::Vec3b>(r);
        for (int c = 0; c < bgr.cols; ++c) {
            const cv::Vec3b& px = rowPtr[c];
            out(r, c) = dlib::rgb_pixel(px[2], px[1], px[0]);
        }
    }
    return out;
}

/**
 * @brief 将通用特征结构转换为 dlib 的列向量。
 */
dlib::matrix<float, 0, 1> toDlibFeature(const FaceFeature& feature) {
    dlib::matrix<float, 0, 1> mat;
    mat.set_size(feature.values.size());
    for (int i = 0; i < feature.values.size(); ++i) mat(i) = feature.values.at(i);
    return mat;
}

/**
 * @brief 从 dlib 网络输出转回框架统一的特征结构。
 */
FaceFeature fromDlibFeature(const dlib::matrix<float, 0, 1>& feature) {
    FaceFeature out;
    out.values.resize(feature.size());
    for (long i = 0; i < feature.size(); ++i) out.values[i] = feature(i);
    out.version = QStringLiteral("dlib_resnet_128");
    out.norm = static_cast<float>(dlib::length(feature));
    return out;
}

}  // namespace

/**
 * @brief 使用 dlib 正面检测器进行人脸检测。
 *
 * 会根据 DetectOptions 对输入图像缩放，缩放比例再映射回原图坐标。
 */
HRCode OpenCVDlibBackend::Impl::detect(const QImage& image,
                                       const DetectOptions& opts,
                                       QVector<FaceBox>& outBoxes) {
    if (logger) {
        logger->debug("Detect request: image={}x{}, landmarks={}, minScore={}, resize={}x{}",
                      image.width(),
                      image.height(),
                      opts.detectLandmarks,
                      opts.minScore,
                      opts.resizeTo.width(),
                      opts.resizeTo.height());
    }
    if (image.isNull()) {
        if (logger) { logger->warn("Detect aborted: image is null"); }
        return HRCode::InvalidImage;
    }

    cv::Mat originalBgr = qImageToBgrMat(image);
    if (originalBgr.empty()) {
        if (logger) { logger->warn("Detect aborted: failed to convert image to BGR"); }
        return HRCode::InvalidImage;
    }

    cv::Mat working = originalBgr;

    double scaleX = 1.0;
    double scaleY = 1.0;
    if (opts.resizeTo.width() > 0 || opts.resizeTo.height() > 0) {
        // 为了提升检测速度，可按配置对输入进行等比缩放
        cv::Size target = working.size();
        if (opts.resizeTo.width() > 0 && opts.resizeTo.height() > 0) {
            target = cv::Size(opts.resizeTo.width(), opts.resizeTo.height());
        } else if (opts.resizeTo.width() > 0) {
            const double ratio = static_cast<double>(opts.resizeTo.width()) / working.cols;
            target =
                cv::Size(opts.resizeTo.width(), static_cast<int>(std::round(working.rows * ratio)));
        } else {
            const double ratio = static_cast<double>(opts.resizeTo.height()) / working.rows;
            target = cv::Size(static_cast<int>(std::round(working.cols * ratio)),
                              opts.resizeTo.height());
        }
        scaleX = static_cast<double>(target.width) / originalBgr.cols;
        scaleY = static_cast<double>(target.height) / originalBgr.rows;
        cv::resize(originalBgr, working, target, 0, 0, cv::INTER_LINEAR);
        if (logger) {
            logger->debug("Detect resize applied: src={}x{}, dst={}x{}",
                          originalBgr.cols,
                          originalBgr.rows,
                          target.width,
                          target.height);
        }
    }

    dlib::matrix<dlib::rgb_pixel> rgbWorking = matToDlibRgb(working);
    std::vector<dlib::rectangle> faces;
    {
        // dlib 检测器不是线程安全的，共享读取时需要加锁。
        std::scoped_lock lock(mutex);
        faces = detector(rgbWorking);
    }

    outBoxes.clear();
    outBoxes.reserve(static_cast<int>(faces.size()));

    const QSize originalSize = image.size();
    for (const auto& rect : faces) {
        const QRect boxRect = scaleRectToOriginal(rect, scaleX, scaleY, originalSize);
        if (!boxRect.isValid() || boxRect.isEmpty()) continue;

        FaceBox box;
        box.rect = boxRect;
        box.score = 1.0f;

        if (opts.detectLandmarks) {
            std::scoped_lock lock(mutex);
            if (shapePredictor) {
                // 只在启用关键点检测时才运行 68/5 点模型，节约算力。
                const dlib::full_object_detection shape = (*shapePredictor)(rgbWorking, rect);
                box.landmarks = scaleLandmarksToOriginal(shape, scaleX, scaleY, originalSize);
            }
        }

        if (box.score >= opts.minScore) { outBoxes.append(std::move(box)); }
    }

    if (outBoxes.isEmpty()) {
        if (logger) { logger->info("Detect result: no face meets threshold"); }
        return HRCode::DetectFailed;
    }
    if (logger) {
        logger->info("Detect found {} faces (landmarks={})", outBoxes.size(), opts.detectLandmarks);
    }
    return HRCode::Ok;
}

/**
 * @brief 将特定人脸区域裁剪成标准尺寸并送入 dlib 网络提取得分。
 */
HRCode OpenCVDlibBackend::Impl::extractFeature(const QImage& image,
                                               const FaceBox& box,
                                               FaceFeature& outFeature) {
    if (logger) {
        logger->debug("Extract feature request: image={}x{}, box=({}, {}, {}, {})",
                      image.width(),
                      image.height(),
                      box.rect.left(),
                      box.rect.top(),
                      box.rect.width(),
                      box.rect.height());
    }
    if (image.isNull()) {
        if (logger) { logger->warn("ExtractFeature aborted: image is null"); }
        return HRCode::InvalidImage;
    }

    cv::Mat originalBgr = qImageToBgrMat(image);
    if (originalBgr.empty()) {
        if (logger) { logger->warn("ExtractFeature aborted: BGR conversion failed"); }
        return HRCode::InvalidImage;
    }

    dlib::matrix<dlib::rgb_pixel> img = matToDlibRgb(originalBgr);

    const int left = std::clamp(box.rect.left(), 0, image.width() - 1);
    const int top = std::clamp(box.rect.top(), 0, image.height() - 1);
    const int right = std::clamp(box.rect.right(), 0, image.width() - 1);
    const int bottom = std::clamp(box.rect.bottom(), 0, image.height() - 1);
    if (left >= right || top >= bottom) {
        if (logger) { logger->warn("ExtractFeature aborted: invalid ROI bounds"); }
        return HRCode::ExtractFeatureFailed;
    }

    const dlib::rectangle rect(left, top, right, bottom);

    std::scoped_lock lock(mutex);
    if (!shapePredictor || !recognitionNet) {
        if (logger) { logger->error("ExtractFeature aborted: model not loaded"); }
        return HRCode::ExtractFeatureFailed;
    }

    // 先使用关键点对齐，确保输入到 CNN 的人脸姿态统一。
    const dlib::full_object_detection shape = (*shapePredictor)(img, rect);

    dlib::matrix<dlib::rgb_pixel> faceChip;
    extract_image_chip(img, get_face_chip_details(shape, 150, 0.25), faceChip);

    // dlib ResNet 输出 128 维特征向量，后续用于距离计算。
    const dlib::matrix<float, 0, 1> descriptor = (*recognitionNet)(faceChip);
    outFeature = fromDlibFeature(descriptor);
    if (logger) {
        logger->info("Extract feature succeeded: norm={} size={}",
                     outFeature.norm.value_or(0.0f),
                     outFeature.values.size());
    }
    return HRCode::Ok;
}

/**
 * @brief 利用欧氏距离评估两个特征向量的相似度。
 */
HRCode OpenCVDlibBackend::Impl::compare(const FaceFeature& a,
                                        const FaceFeature& b,
                                        float& outDistance) {
    if (logger) {
        logger->debug(
            "Compare request: featureA={}, featureB={}", a.values.size(), b.values.size());
    }
    std::scoped_lock lock(mutex);
    auto dist = computeDistance(a, b);
    if (!dist.has_value()) {
        if (logger) { logger->warn("Compare failed: cannot compute distance"); }
        return HRCode::CompareFailed;
    }
    outDistance = dist.value();
    if (logger) { logger->info("Compare result distance={}", outDistance); }
    return HRCode::Ok;
}

/**
 * @brief 遍历人员缓存，找到与查询特征最接近的记录。
 */
HRCode OpenCVDlibBackend::Impl::findNearest(const FaceFeature& feature,
                                            RecognitionMatch& outMatch) {
    if (logger) { logger->debug("FindNearest request: feature size={}", feature.values.size()); }
    std::scoped_lock lock(mutex);
    if (persons.isEmpty()) {
        if (logger) { logger->info("FindNearest: persons cache empty"); }
        return HRCode::PersonNotFound;
    }

    auto bestDistance = std::numeric_limits<float>::max();
    const PersonInfo* bestPerson = nullptr;

    for (auto it = persons.cbegin(); it != persons.cend(); ++it) {
        if (!it.value().canonicalFeature.has_value()) continue;
        // 对每一条人员都计算距离，特点是实现简单，后续可替换为向量索引以提升性能。
        auto dist = computeDistance(feature, it.value().canonicalFeature.value());
        if (!dist.has_value()) continue;
        if (dist.value() < bestDistance) {
            bestDistance = dist.value();
            bestPerson = &it.value();
        }
    }

    if (!bestPerson) {
        if (logger) { logger->info("FindNearest: no match in {} persons", persons.size()); }
        return HRCode::PersonNotFound;
    }

    if (matchThreshold > 0.0 && bestDistance > matchThreshold) {
        // 距离超过阈值，判定为未知人员，避免误识别
        if (logger) {
            logger->debug("Best match {} rejected: distance {} exceeds threshold {}",
                          bestPerson->id.toStdString(),
                          bestDistance,
                          matchThreshold);
        }
        return HRCode::PersonNotFound;
    }

    outMatch.personId = bestPerson->id;
    outMatch.personName = bestPerson->name;
    outMatch.distance = bestDistance;
    const float norm = matchThreshold > 0.0 ? static_cast<float>(matchThreshold) : 1.2f;
    outMatch.score = std::max(0.0f, 1.0f - bestDistance / norm);
    if (logger) {
        logger->info("FindNearest matched {} ({}) distance={} score={}",
                     outMatch.personId.toStdString(),
                     outMatch.personName.toStdString(),
                     outMatch.distance,
                     outMatch.score);
    }
    return HRCode::Ok;
}

/**
 * @brief 将内部特征向量转为 dlib 矩阵后计算欧氏距离。
 */
std::optional<float> OpenCVDlibBackend::Impl::computeDistance(const FaceFeature& a,
                                                              const FaceFeature& b) const {
    if (a.values.isEmpty() || b.values.isEmpty()) {
        if (logger) { logger->warn("ComputeDistance skipped: empty feature"); }
        return std::nullopt;
    }
    if (a.values.size() != b.values.size()) {
        if (logger) {
            logger->warn("ComputeDistance skipped: size mismatch {} vs {}",
                         a.values.size(),
                         b.values.size());
        }
        return std::nullopt;
    }
    dlib::matrix<float, 0, 1> fa = toDlibFeature(a);
    dlib::matrix<float, 0, 1> fb = toDlibFeature(b);
    return static_cast<float>(dlib::length(fa - fb));
}

}  // namespace HumanRecognition

#endif  // defined(HAS_OPENCV) && defined(HAS_DLIB)
