#pragma once

#include <QImage>
#include <QStringList>
#include <QVector>
#include <QVideoFrame>
#include <QWidget>
#include <optional>

#include "modules/HumanRecognition/types.h"

class QCamera;
class QCheckBox;
class QItemSelectionModel;
class QLineEdit;
class QListWidget;
class QMediaCaptureSession;
class QMediaPlayer;
class QMovie;
// ...existing code...
class QPushButton;
class QStandardItem;
class QSlider;
class QSplitter;
class QStandardItemModel;
class QTableView;
class QTextEdit;
class QVideoSink;
class QLabel;

enum class FaceSourceMode { None, Image, Animated, Camera, Video };

struct DetectionEntry {
    HumanRecognition::FaceBox box;
    std::optional<HumanRecognition::FaceFeature> feature;
    std::optional<HumanRecognition::RecognitionMatch> match;
};

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
    void onLoadAnimated();
    void onLoadModel();
    void onToggleCamera();
    void onCaptureFrame();
    void onDetectFaces();
    void onClearResults();
    void onRegisterSelectedFace();
    void onMatchSelectedFace();
    void onResultSelectionChanged();
    void onRemovePerson();
    void onRefreshPersons();
    void onCameraFrame(const QVideoFrame& frame);
    void onVideoFrame(const QVideoFrame& frame);
    void onAnimatedFrame(int frameNumber);
    void onDatabaseItemChanged(QStandardItem* item);

   private:
    bool ensureBackendReady(bool warnAboutModel = true);
    void ensureUiState();
    void updatePreview(const QImage& image, const QVector<HumanRecognition::FaceBox>& boxes = {});
    void appendResult(std::size_t index, const DetectionEntry& entry);
    void resetDetections();
    void showError(const QString& message);
    void showInfo(const QString& message);
    bool ensureCamera();
    void stopCamera();
    void teardownAnimated();
    void teardownVideo();
    void refreshDatabase(bool warnAboutModel = false);
    bool selectedDetection(DetectionEntry& entry) const;
    HumanRecognition::HRCode extractFeatureForBox(const QImage& image,
                                                  const HumanRecognition::FaceBox& box,
                                                  DetectionEntry& entry);
    void populateDatabaseModel(const QVector<HumanRecognition::PersonInfo>& persons);
    void enableRegistrationButtons(bool enabled);
    void enableMatchButtons(bool enabled);
    void updateSourceBanner(const QString& description);
    QString resolveModelDestinationRoot() const;
    bool prepareModelDestination(const QString& destinationPath);
    bool copyModelArtifacts(const QString& sourcePath,
                            const QString& destinationDir,
                            QString& outLoadTarget,
                            QStringList& errors);
    bool loadModelFromPath(const QString& path,
                           bool persistConfig,
                           bool showSuccessMessage,
                           bool showFailureMessage);
    void updateModelStatus(const QString& path, bool loaded, const QString& detail = QString());

    QPushButton* btnBack_{nullptr};
    QPushButton* btnLoadImage_{nullptr};
    QPushButton* btnLoadAnimated_{nullptr};
    QPushButton* btnLoadModel_{nullptr};
    QPushButton* btnToggleCamera_{nullptr};
    QPushButton* btnCapture_{nullptr};
    QPushButton* btnDetect_{nullptr};
    QPushButton* btnClear_{nullptr};
    QPushButton* btnRegister_{nullptr};
    QPushButton* btnMatch_{nullptr};
    QPushButton* btnRefreshDb_{nullptr};
    QPushButton* btnRemovePerson_{nullptr};
    QLabel* previewLabel_{nullptr};
    QLabel* sourceBanner_{nullptr};
    QLabel* modelStatusLabel_{nullptr};
    QListWidget* resultsList_{nullptr};
    QTableView* databaseView_{nullptr};
    QStandardItemModel* databaseModel_{nullptr};
    QLineEdit* personIdEdit_{nullptr};
    QLineEdit* personNameEdit_{nullptr};
    QPushButton* btnGenerateUuid_{nullptr};
    QCheckBox* autoMatchCheck_{nullptr};

    QImage currentImage_;
    QVector<DetectionEntry> detections_;
    int selectedDetectionIndex_{-1};
    QVector<HumanRecognition::FaceBox> lastBoxes_;

    FaceSourceMode currentSource_{FaceSourceMode::None};
    QString currentSourceDescription_;
    bool backendReady_{false};
    bool cameraActive_{false};
    bool modelLoaded_{false};
    QString currentModelPath_;
    bool suppressDatabaseModelSignals_{false};

    QCamera* camera_{nullptr};
    QMediaCaptureSession* cameraSession_{nullptr};
    QVideoSink* cameraSink_{nullptr};
    QMediaPlayer* videoPlayer_{nullptr};
    QVideoSink* videoSink_{nullptr};
    QMovie* animatedMovie_{nullptr};

    QImage lastCameraFrame_;
    QImage lastVideoFrame_;
    QImage lastAnimatedFrame_;
};
