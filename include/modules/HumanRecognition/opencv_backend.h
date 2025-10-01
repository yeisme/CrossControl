/**
 * @file opencv_backend.h
 * @author your name (you@domain.com)
 * @brief 基于 OpenCV 的人脸检测/识别后端 (简化骨架实现)
 * @version 0.1
 * @date 2025-09-26
 *
 * @copyright Copyright (c) 2025
 *
 */

#pragma once

#include <QVariant>
#include <QtSql/QSqlQuery>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/objdetect.hpp>

#include "iface_human_recognition.h"

// TODO: 使用 opencv dnn 替代人脸识别

class OpenCVHumanRecognitionBackend : public IHumanRecognitionBackend {
   public:
    OpenCVHumanRecognitionBackend();
    ~OpenCVHumanRecognitionBackend() override;

    bool initialize() override;
    void shutdown() override;

    std::vector<FaceDetectionResult> detectFaces(const QImage& image) override;
    std::optional<FaceEmbedding> extractEmbedding(const QImage& faceImage) override;
    int enrollPerson(const PersonInfo& info, const std::vector<QImage>& faceImages) override;
    std::vector<FaceDetectionResult> detectAndRecognize(const QImage& image) override;
    std::optional<FaceDetectionResult> matchEmbedding(const FaceEmbedding& emb) override;
    bool saveFeatureDatabase() override;  // 对 SQLite 来说可以是 NO-OP
    bool loadFeatureDatabase() override;  // 预读缓存
    std::vector<FaceDetectionResult> processFrame(const QImage& frame,
                                                  bool recognize = true) override;
    void setRecognitionThreshold(float thr) override { m_threshold = thr; }
    float recognitionThreshold() const override { return m_threshold; }

   private:
    bool ensureSchema_();
    static cv::Mat qimageToMat_(const QImage& img);
    static QImage matToQImage_(const cv::Mat& mat);

    // 目前使用 Haar 级联做示例
    cv::CascadeClassifier m_faceCascade;
    bool m_initialized{false};

    struct CacheItem {
        FaceEmbedding emb;
        QString personId;
        QString name;
    };
    std::vector<CacheItem> m_featureCache;  ///< 内存缓存，减少频繁 SQL 查询
    float m_threshold{0.6f};                // 欧氏距离阈值（简化）

    template <typename DistFunc>
    std::optional<FaceDetectionResult> matchWithMetric_(const FaceEmbedding& emb,
                                                        DistFunc&& distFunc) {
        if (m_featureCache.empty()) return std::nullopt;
        int bestIdx = -1;
        float bestScore = std::numeric_limits<float>::max();
        for (size_t i = 0; i < m_featureCache.size(); ++i) {
            const auto& c = m_featureCache[i];
            if (c.emb.size() != emb.size()) continue;
            float d = distFunc(emb, c.emb);
            if (d < bestScore) {
                bestScore = d;
                bestIdx = static_cast<int>(i);
            }
        }
        if (bestIdx < 0) return std::nullopt;
        if (bestScore > m_threshold) return std::nullopt;  // 超出阈值视为不匹配
        FaceDetectionResult r;
        r.embedding = m_featureCache[bestIdx].emb;
        r.matchedPersonId = m_featureCache[bestIdx].personId;
        r.matchedPersonName = m_featureCache[bestIdx].name;
        r.matchedDistance = bestScore;
        return r;
    }
    // Person management
    std::vector<PersonInfo> listPersons() override;
    bool updatePersonInfo(const PersonInfo& info) override;
    bool deletePerson(const QString& personId) override;
    std::vector<FaceEmbedding> getPersonFeatures(const QString& personId) override;
};
