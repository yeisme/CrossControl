#include "widgets/facerecognitionwidget.h"

#include <QCoreApplication>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QPushButton>
#include <QSize>
#include <QSplitter>
#include <QVBoxLayout>

#include "modules/HumanRecognition/humanrecognition.h"
#include "modules/HumanRecognition/types.h"

namespace {

QString hrCodeToString(HumanRecognition::HRCode code) {
    using HumanRecognition::HRCode;
    switch (code) {
        case HRCode::Ok:
            return QCoreApplication::translate("FaceRecognitionWidget", "Ok");
        case HRCode::InvalidImage:
            return QCoreApplication::translate("FaceRecognitionWidget", "Invalid image");
        case HRCode::ModelLoadFailed:
            return QCoreApplication::translate("FaceRecognitionWidget", "Model load failed");
        case HRCode::ModelSaveFailed:
            return QCoreApplication::translate("FaceRecognitionWidget", "Model save failed");
        case HRCode::DetectFailed:
            return QCoreApplication::translate("FaceRecognitionWidget", "Detection failed");
        case HRCode::ExtractFeatureFailed:
            return QCoreApplication::translate("FaceRecognitionWidget",
                                               "Feature extraction failed");
        case HRCode::CompareFailed:
            return QCoreApplication::translate("FaceRecognitionWidget", "Compare failed");
        case HRCode::TrainFailed:
            return QCoreApplication::translate("FaceRecognitionWidget", "Train failed");
        case HRCode::PersonExists:
            return QCoreApplication::translate("FaceRecognitionWidget", "Person already exists");
        case HRCode::PersonNotFound:
            return QCoreApplication::translate("FaceRecognitionWidget", "Person not found");
        case HRCode::UnknownError:
        default:
            return QCoreApplication::translate("FaceRecognitionWidget", "Unknown error");
    }
}

}  // namespace

FaceRecognitionWidget::FaceRecognitionWidget(QWidget* parent) : QWidget(parent) {
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(16, 16, 16, 16);
    outer->setSpacing(12);

    auto* headerLayout = new QHBoxLayout();
    btnBack_ = new QPushButton(QCoreApplication::translate("FaceRecognitionWidget", "Back"), this);
    btnBack_->setObjectName("faceRecBackButton");
    btnBack_->setCursor(Qt::PointingHandCursor);
    headerLayout->addWidget(btnBack_, 0, Qt::AlignLeft);

    auto* title =
        new QLabel(QCoreApplication::translate("FaceRecognitionWidget", "Face Recognition"), this);
    title->setObjectName("faceRecTitle");
    title->setStyleSheet("font-size: 20px; font-weight: 600;");
    headerLayout->addSpacing(12);
    headerLayout->addWidget(title, 0, Qt::AlignVCenter);
    headerLayout->addStretch(1);
    outer->addLayout(headerLayout);

    auto* toolbar = new QHBoxLayout();
    btnLoad_ =
        new QPushButton(QCoreApplication::translate("FaceRecognitionWidget", "Load Image"), this);
    btnDetect_ =
        new QPushButton(QCoreApplication::translate("FaceRecognitionWidget", "Detect Faces"), this);
    btnDetect_->setEnabled(false);
    btnClear_ = new QPushButton(
        QCoreApplication::translate("FaceRecognitionWidget", "Clear Results"), this);
    btnClear_->setEnabled(false);

    toolbar->addWidget(btnLoad_);
    toolbar->addWidget(btnDetect_);
    toolbar->addWidget(btnClear_);
    toolbar->addStretch(1);
    outer->addLayout(toolbar);

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    previewLabel_ = new QLabel(
        QCoreApplication::translate("FaceRecognitionWidget", "No image loaded"), splitter);
    previewLabel_->setMinimumSize(360, 240);
    previewLabel_->setAlignment(Qt::AlignCenter);
    previewLabel_->setStyleSheet(
        "border: 1px solid rgba(59,130,246,0.35); background-color: rgba(15,23,42,0.3);");
    previewLabel_->setScaledContents(true);

    resultsList_ = new QListWidget(splitter);
    resultsList_->setMinimumWidth(260);
    resultsList_->setObjectName("faceRecResults");

    splitter->addWidget(previewLabel_);
    splitter->addWidget(resultsList_);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);

    outer->addWidget(splitter, 1);

    connect(btnBack_, &QPushButton::clicked, this, [this]() { emit backToMain(); });
    connect(btnLoad_, &QPushButton::clicked, this, &FaceRecognitionWidget::onLoadImage);
    connect(btnDetect_, &QPushButton::clicked, this, &FaceRecognitionWidget::onDetectFaces);
    connect(btnClear_, &QPushButton::clicked, this, &FaceRecognitionWidget::onClearResults);
}

FaceRecognitionWidget::~FaceRecognitionWidget() = default;

bool FaceRecognitionWidget::ensureBackendReady() {
    if (backendReady_) return true;
    constexpr auto backendName = "opencv_dlib";
    auto& hr = HumanRecognition::HumanRecognition::instance();
    if (!hr.hasBackend() || hr.currentBackendName() != QString::fromLatin1(backendName)) {
        auto code = hr.setBackend(QString::fromLatin1(backendName));
        if (code != HumanRecognition::HRCode::Ok) {
            showError(QCoreApplication::translate("FaceRecognitionWidget",
                                                  "Failed to activate backend '%1': %2")
                          .arg(QString::fromLatin1(backendName), hrCodeToString(code)));
            return false;
        }
    }
    backendReady_ = true;
    return true;
}

void FaceRecognitionWidget::onLoadImage() {
    const QString file = QFileDialog::getOpenFileName(
        this,
        QCoreApplication::translate("FaceRecognitionWidget", "Select Image"),
        QString(),
        QCoreApplication::translate("FaceRecognitionWidget",
                                    "Images (*.png *.jpg *.jpeg *.bmp *.webp)"));

    if (file.isEmpty()) return;

    QImage image(file);
    if (image.isNull()) {
        showError(QCoreApplication::translate("FaceRecognitionWidget", "Unable to load image: %1")
                      .arg(file));
        return;
    }

    currentImage_ = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    lastBoxes_.clear();
    resultsList_->clear();
    updatePreview(currentImage_);

    btnDetect_->setEnabled(true);
    btnClear_->setEnabled(false);
}

void FaceRecognitionWidget::onDetectFaces() {
    if (currentImage_.isNull()) {
        showError(
            QCoreApplication::translate("FaceRecognitionWidget", "Please load an image first."));
        return;
    }

    if (!ensureBackendReady()) return;

    auto& hr = HumanRecognition::HumanRecognition::instance();
    HumanRecognition::DetectOptions opts;
    opts.detectLandmarks = true;
    opts.minScore = 0.3f;
    if (currentImage_.width() > 1200 || currentImage_.height() > 1200) {
        opts.resizeTo = QSize(960, 720);
    }

    QVector<HumanRecognition::FaceBox> boxes;
    auto detectCode = hr.detect(currentImage_, opts, boxes);
    if (detectCode != HumanRecognition::HRCode::Ok || boxes.isEmpty()) {
        showError(QCoreApplication::translate("FaceRecognitionWidget", "No faces detected: %1")
                      .arg(hrCodeToString(detectCode)));
        return;
    }

    QVector<HumanRecognition::FaceFeature> features;
    QVector<HumanRecognition::RecognitionMatch> matches;
    features.reserve(boxes.size());
    matches.reserve(boxes.size());

    resultsList_->clear();

    for (const auto& box : boxes) {
        HumanRecognition::FaceFeature feature;
        HumanRecognition::RecognitionMatch match;
        const HumanRecognition::FaceFeature* featurePtr = nullptr;
        const HumanRecognition::RecognitionMatch* matchPtr = nullptr;

        auto featCode = hr.extractFeature(currentImage_, box, feature);
        if (featCode == HumanRecognition::HRCode::Ok) {
            features.push_back(feature);
            featurePtr = &features.back();

            auto matchCode = hr.findNearest(*featurePtr, match);
            if (matchCode == HumanRecognition::HRCode::Ok) {
                matches.push_back(match);
                matchPtr = &matches.back();
            }
        }

        appendResult(box, featurePtr, matchPtr);
    }

    lastBoxes_ = boxes;
    updatePreview(currentImage_, lastBoxes_);
    btnClear_->setEnabled(true);
}

void FaceRecognitionWidget::onClearResults() {
    resultsList_->clear();
    lastBoxes_.clear();
    updatePreview(currentImage_);
    btnClear_->setEnabled(false);
}

void FaceRecognitionWidget::updatePreview(const QImage& image,
                                          const QVector<HumanRecognition::FaceBox>& boxes) {
    if (image.isNull()) {
        previewLabel_->setText(
            QCoreApplication::translate("FaceRecognitionWidget", "No image loaded"));
        previewLabel_->setPixmap({});
        return;
    }

    QImage annotated = image;
    if (!boxes.isEmpty()) {
        QPainter painter(&annotated);
        painter.setRenderHint(QPainter::Antialiasing, true);
        QPen pen(QColor(34, 211, 238));
        pen.setWidth(3);
        painter.setPen(pen);
        for (const auto& box : boxes) {
            painter.drawRect(box.rect);
            if (!box.landmarks.isEmpty()) {
                QPen pointPen(QColor(16, 185, 129));
                pointPen.setWidth(6);
                painter.setPen(pointPen);
                for (const auto& pt : box.landmarks) { painter.drawPoint(pt); }
                painter.setPen(pen);
            }
        }
    }

    previewLabel_->setPixmap(QPixmap::fromImage(annotated));
    previewLabel_->setText(QString());
}

void FaceRecognitionWidget::appendResult(const HumanRecognition::FaceBox& box,
                                         const HumanRecognition::FaceFeature* feature,
                                         const HumanRecognition::RecognitionMatch* match) {
    QString text =
        QCoreApplication::translate("FaceRecognitionWidget", "Face: (%1,%2) %3x%4 | score=%5")
            .arg(box.rect.x())
            .arg(box.rect.y())
            .arg(box.rect.width())
            .arg(box.rect.height())
            .arg(box.score, 0, 'f', 3);
    if (feature) {
        text += QCoreApplication::translate("FaceRecognitionWidget", "\nFeature length: %1")
                    .arg(feature->values.size());
        if (feature->norm.has_value()) {
            text += QCoreApplication::translate("FaceRecognitionWidget", " | norm=%1")
                        .arg(*feature->norm, 0, 'f', 3);
        }
    }
    if (match) {
        text += QCoreApplication::translate("FaceRecognitionWidget",
                                            "\nMatch: %1 (%2) | distance=%3 | score=%4")
                    .arg(match->personName.isEmpty()
                             ? QCoreApplication::translate("FaceRecognitionWidget", "Unnamed")
                             : match->personName)
                    .arg(match->personId)
                    .arg(match->distance, 0, 'f', 3)
                    .arg(match->score, 0, 'f', 3);
    }

    auto* item = new QListWidgetItem(text, resultsList_);
    item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    resultsList_->addItem(item);
}

void FaceRecognitionWidget::showError(const QString& message) {
    QMessageBox::warning(
        this, QCoreApplication::translate("FaceRecognitionWidget", "Face Recognition"), message);
}
