#pragma once

#include <QImage>
#include <QVector>
#include <QWidget>

#include "modules/HumanRecognition/types.h"

class QLabel;
class QListWidget;
class QPushButton;
class QSplitter;
class QTextEdit;

/**
 * @brief 简易人脸识别展示组件
 *
 * 提供加载图片、调用 HumanRecognition 后端进行检测/特征提取、
 * 并通过列表展示结果的基本界面。主要用于演示 OpenCV + dlib 后端。
 */
class FaceRecognitionWidget : public QWidget {
    Q_OBJECT

   public:
    explicit FaceRecognitionWidget(QWidget* parent = nullptr);
    ~FaceRecognitionWidget() override;

   signals:
    void backToMain();

   private slots:
    void onLoadImage();
    void onDetectFaces();
    void onClearResults();

   private:
    bool ensureBackendReady();
    void updatePreview(const QImage& image, const QVector<HumanRecognition::FaceBox>& boxes = {});
    void appendResult(const HumanRecognition::FaceBox& box,
                      const HumanRecognition::FaceFeature* feature,
                      const HumanRecognition::RecognitionMatch* match);
    void showError(const QString& message);

    QPushButton* btnBack_{nullptr};
    QPushButton* btnLoad_{nullptr};
    QPushButton* btnDetect_{nullptr};
    QPushButton* btnClear_{nullptr};
    QLabel* previewLabel_{nullptr};
    QListWidget* resultsList_{nullptr};

    QImage currentImage_;
    QVector<HumanRecognition::FaceBox> lastBoxes_;
    bool backendReady_{false};
};
