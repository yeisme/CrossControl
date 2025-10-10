#include "widgets/facerecognitionwidget.h"

#include <QAbstractItemView>
#include <QCamera>
#include <QCameraDevice>
#include <QCheckBox>
#include <QCoreApplication>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMediaCaptureSession>
#include <QMediaDevices>
#include <QMediaPlayer>
#include <QMessageBox>
#include <QMovie>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSize>
#include <QSplitter>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QStringList>
#include <QTableView>
#include <QTextEdit>
#include <QUuid>
#include <QVBoxLayout>
#include <QVideoFrame>
#include <QVideoSink>
#include <cstddef>
#include <filesystem>

#include "modules/Config/config.h"
#include "modules/HumanRecognition/humanrecognition.h"

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

QImage toDisplayImage(const QImage& src) {
    if (src.isNull()) return {};
    if (src.format() == QImage::Format_ARGB32 || src.format() == QImage::Format_RGB32) return src;
    return src.convertToFormat(QImage::Format_ARGB32_Premultiplied);
}

// ... metadata removed ...

bool copyDirectoryRecursive(const QString& sourcePath,
                            const QString& destinationPath,
                            QStringList& errors) {
    QDir sourceDir(sourcePath);
    if (!sourceDir.exists()) {
        errors << QCoreApplication::translate("FaceRecognitionWidget",
                                              "Source directory does not exist: %1")
                      .arg(QDir::toNativeSeparators(sourcePath));
        return false;
    }

    const QFileInfoList entries = sourceDir.entryInfoList(
        QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
    for (const QFileInfo& entry : entries) {
        const QString targetPath = QDir(destinationPath).filePath(entry.fileName());
        if (entry.isDir()) {
            if (!QDir().mkpath(targetPath)) {
                errors << QCoreApplication::translate("FaceRecognitionWidget",
                                                      "Unable to create directory: %1")
                              .arg(QDir::toNativeSeparators(targetPath));
                return false;
            }
            if (!copyDirectoryRecursive(entry.absoluteFilePath(), targetPath, errors)) {
                return false;
            }
        } else {
            if (QFile::exists(targetPath) && !QFile::remove(targetPath)) {
                errors << QCoreApplication::translate("FaceRecognitionWidget",
                                                      "Unable to overwrite file: %1")
                              .arg(QDir::toNativeSeparators(targetPath));
                return false;
            }
            if (!QFile::copy(entry.absoluteFilePath(), targetPath)) {
                errors << QCoreApplication::translate("FaceRecognitionWidget",
                                                      "Failed to copy '%1' to '%2'")
                              .arg(QDir::toNativeSeparators(entry.absoluteFilePath()),
                                   QDir::toNativeSeparators(targetPath));
                return false;
            }
        }
    }
    return true;
}

QString selectShapeModel(const QStringList& files) {
    for (const QString& file : files) {
        const QString lower = file.toLower();
        if (lower.contains(QStringLiteral("shape_predictor"))) { return file; }
    }
    return QString();
}

QString selectRecognitionModel(const QStringList& files) {
    for (const QString& file : files) {
        const QString lower = file.toLower();
        if (lower.contains(QStringLiteral("recognition")) ||
            lower.contains(QStringLiteral("resnet"))) {
            return file;
        }
    }
    return QString();
}

QStringList listDatFiles(const QDir& dir) {
    QStringList files = dir.entryList(QStringList() << QStringLiteral("*.dat"), QDir::Files);
    files.sort(Qt::CaseInsensitive);
    return files;
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

    sourceBanner_ =
        new QLabel(QCoreApplication::translate("FaceRecognitionWidget", "No source active"), this);
    sourceBanner_->setObjectName("faceRecSourceBanner");
    sourceBanner_->setStyleSheet(
        "padding: 6px 10px; border-radius: 6px; background-color: rgba(59,130,246,0.1); color: "
        "#1d4ed8;");
    outer->addWidget(sourceBanner_);

    auto* modelLayout = new QHBoxLayout();
    modelStatusLabel_ =
        new QLabel(QCoreApplication::translate("FaceRecognitionWidget", "Model: not loaded"), this);
    modelStatusLabel_->setObjectName("faceRecModelStatus");
    btnLoadModel_ =
        new QPushButton(QCoreApplication::translate("FaceRecognitionWidget", "Load Model"), this);
    btnLoadModel_->setCursor(Qt::PointingHandCursor);
    modelLayout->addWidget(modelStatusLabel_);
    modelLayout->addStretch(1);
    modelLayout->addWidget(btnLoadModel_);
    outer->addLayout(modelLayout);

    auto* toolbar = new QHBoxLayout();
    btnLoadImage_ =
        new QPushButton(QCoreApplication::translate("FaceRecognitionWidget", "Load Image"), this);
    btnLoadAnimated_ = new QPushButton(
        QCoreApplication::translate("FaceRecognitionWidget", "Load Animated"), this);
    btnToggleCamera_ =
        new QPushButton(QCoreApplication::translate("FaceRecognitionWidget", "Start Camera"), this);
    btnCapture_ =
        new QPushButton(QCoreApplication::translate("FaceRecognitionWidget", "Capture"), this);
    btnCapture_->setEnabled(false);
    btnDetect_ =
        new QPushButton(QCoreApplication::translate("FaceRecognitionWidget", "Detect Faces"), this);
    btnDetect_->setEnabled(false);
    btnClear_ =
        new QPushButton(QCoreApplication::translate("FaceRecognitionWidget", "Clear"), this);
    btnClear_->setEnabled(false);

    toolbar->addWidget(btnLoadImage_);
    toolbar->addWidget(btnLoadAnimated_);
    toolbar->addWidget(btnToggleCamera_);
    toolbar->addWidget(btnCapture_);
    toolbar->addSpacing(6);
    toolbar->addWidget(btnDetect_);
    toolbar->addWidget(btnClear_);
    toolbar->addStretch(1);
    outer->addLayout(toolbar);

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    outer->addWidget(splitter, 1);

    auto* leftPanel = new QWidget(splitter);
    auto* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(10);

    previewLabel_ = new QLabel(
        QCoreApplication::translate("FaceRecognitionWidget", "No image loaded"), leftPanel);
    previewLabel_->setMinimumSize(420, 280);
    previewLabel_->setAlignment(Qt::AlignCenter);
    previewLabel_->setStyleSheet(
        "border: 1px solid rgba(59,130,246,0.35); background-color: rgba(15,23,42,0.26);");
    previewLabel_->setScaledContents(true);
    leftLayout->addWidget(previewLabel_, 1);

    auto* resultsHeader = new QHBoxLayout();
    auto* resultsTitle =
        new QLabel(QCoreApplication::translate("FaceRecognitionWidget", "Detections"), leftPanel);
    resultsTitle->setStyleSheet("font-weight: 600; font-size: 14px;");
    resultsHeader->addWidget(resultsTitle);
    resultsHeader->addStretch(1);
    autoMatchCheck_ = new QCheckBox(
        QCoreApplication::translate("FaceRecognitionWidget", "Auto-match"), leftPanel);
    autoMatchCheck_->setChecked(true);
    resultsHeader->addWidget(autoMatchCheck_);
    leftLayout->addLayout(resultsHeader);

    resultsList_ = new QListWidget(leftPanel);
    resultsList_->setObjectName("faceRecResults");
    resultsList_->setSelectionMode(QAbstractItemView::SingleSelection);
    leftLayout->addWidget(resultsList_, 1);

    auto* actionRow = new QHBoxLayout();
    btnRegister_ = new QPushButton(QCoreApplication::translate("FaceRecognitionWidget", "Register"),
                                   leftPanel);
    btnRegister_->setEnabled(false);
    btnMatch_ =
        new QPushButton(QCoreApplication::translate("FaceRecognitionWidget", "Match"), leftPanel);
    btnMatch_->setEnabled(false);
    actionRow->addWidget(btnRegister_);
    actionRow->addWidget(btnMatch_);
    actionRow->addStretch(1);
    leftLayout->addLayout(actionRow);

    splitter->addWidget(leftPanel);

    auto* rightPanel = new QWidget(splitter);
    auto* rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(10);

    auto* dbHeader = new QHBoxLayout();
    auto* dbTitle = new QLabel(
        QCoreApplication::translate("FaceRecognitionWidget", "Face Database"), rightPanel);
    dbTitle->setStyleSheet("font-weight: 600; font-size: 14px;");
    dbHeader->addWidget(dbTitle);
    dbHeader->addStretch(1);
    btnRefreshDb_ = new QPushButton(QCoreApplication::translate("FaceRecognitionWidget", "Refresh"),
                                    rightPanel);
    btnRemovePerson_ = new QPushButton(
        QCoreApplication::translate("FaceRecognitionWidget", "Remove Selected"), rightPanel);
    btnRemovePerson_->setEnabled(false);
    btnGenerateUuid_ = new QPushButton(
        QCoreApplication::translate("FaceRecognitionWidget", "Generate UUID"), rightPanel);
    dbHeader->addWidget(btnRefreshDb_);
    dbHeader->addWidget(btnRemovePerson_);
    dbHeader->addWidget(btnGenerateUuid_);
    rightLayout->addLayout(dbHeader);

    databaseModel_ = new QStandardItemModel(this);
    databaseView_ = new QTableView(rightPanel);
    databaseView_->setModel(databaseModel_);
    databaseView_->setSelectionBehavior(QAbstractItemView::SelectRows);
    databaseView_->setSelectionMode(QAbstractItemView::SingleSelection);
    // Allow in-place editing by double-clicking
    databaseView_->setEditTriggers(QAbstractItemView::DoubleClicked |
                                   QAbstractItemView::SelectedClicked);
    databaseView_->horizontalHeader()->setStretchLastSection(true);
    rightLayout->addWidget(databaseView_, 1);

    auto* formLayout = new QVBoxLayout();
    auto* idRow = new QHBoxLayout();
    auto* idLabel =
        new QLabel(QCoreApplication::translate("FaceRecognitionWidget", "Person ID"), rightPanel);
    personIdEdit_ = new QLineEdit(rightPanel);
    idRow->addWidget(idLabel);
    idRow->addWidget(personIdEdit_);
    formLayout->addLayout(idRow);

    auto* nameRow = new QHBoxLayout();
    auto* nameLabel =
        new QLabel(QCoreApplication::translate("FaceRecognitionWidget", "Name"), rightPanel);
    personNameEdit_ = new QLineEdit(rightPanel);
    nameRow->addWidget(nameLabel);
    nameRow->addWidget(personNameEdit_);
    formLayout->addLayout(nameRow);

    rightLayout->addLayout(formLayout);

    splitter->addWidget(rightPanel);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);

    connect(btnBack_, &QPushButton::clicked, this, [this]() { emit backToMain(); });
    connect(btnLoadModel_, &QPushButton::clicked, this, &FaceRecognitionWidget::onLoadModel);
    connect(btnLoadImage_, &QPushButton::clicked, this, &FaceRecognitionWidget::onLoadImage);
    connect(btnLoadAnimated_, &QPushButton::clicked, this, &FaceRecognitionWidget::onLoadAnimated);
    connect(btnToggleCamera_, &QPushButton::clicked, this, &FaceRecognitionWidget::onToggleCamera);
    connect(btnCapture_, &QPushButton::clicked, this, &FaceRecognitionWidget::onCaptureFrame);
    connect(btnDetect_, &QPushButton::clicked, this, &FaceRecognitionWidget::onDetectFaces);
    connect(btnClear_, &QPushButton::clicked, this, &FaceRecognitionWidget::onClearResults);
    connect(
        btnRegister_, &QPushButton::clicked, this, &FaceRecognitionWidget::onRegisterSelectedFace);
    connect(btnMatch_, &QPushButton::clicked, this, &FaceRecognitionWidget::onMatchSelectedFace);
    connect(btnRefreshDb_, &QPushButton::clicked, this, &FaceRecognitionWidget::onRefreshPersons);
    connect(btnRemovePerson_, &QPushButton::clicked, this, &FaceRecognitionWidget::onRemovePerson);
    connect(btnGenerateUuid_, &QPushButton::clicked, this, [this]() {
        const QString uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
        personIdEdit_->setText(uuid);
    });
    connect(resultsList_,
            &QListWidget::itemSelectionChanged,
            this,
            &FaceRecognitionWidget::onResultSelectionChanged);
    connect(personIdEdit_, &QLineEdit::textChanged, this, [this]() { ensureUiState(); });
    connect(personNameEdit_, &QLineEdit::textChanged, this, [this]() { ensureUiState(); });
    connect(databaseModel_,
            &QStandardItemModel::itemChanged,
            this,
            &FaceRecognitionWidget::onDatabaseItemChanged);

    QString storedModelPath = config::ConfigManager::instance().getString(
        QStringLiteral("HumanRecognition/ModelFile"), QString());
    if (storedModelPath.isEmpty()) {
        storedModelPath = config::ConfigManager::instance().getString(
            QStringLiteral("HumanRecognition/ModelDirectory"), QString());
    }

    if (!storedModelPath.isEmpty()) {
        QFileInfo info(storedModelPath);
        if (info.exists()) {
            currentModelPath_ = info.absoluteFilePath();
            updateModelStatus(currentModelPath_, true);
        } else {
            currentModelPath_ = info.absoluteFilePath();
            updateModelStatus(currentModelPath_, false);
        }
    } else {
        updateModelStatus(QString(), false);
    }

    if (auto* sel = databaseView_->selectionModel()) {
        connect(sel, &QItemSelectionModel::selectionChanged, this, [this]() { ensureUiState(); });
    }

    refreshDatabase(false);
    ensureUiState();
}

FaceRecognitionWidget::~FaceRecognitionWidget() {
    stopCamera();
    teardownAnimated();
    teardownVideo();
}

void FaceRecognitionWidget::onLoadImage() {
    stopCamera();
    teardownAnimated();

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

    currentImage_ = toDisplayImage(image);
    currentSource_ = FaceSourceMode::Image;
    currentSourceDescription_ = file;
    updateSourceBanner(
        QCoreApplication::translate("FaceRecognitionWidget", "Source: Image (%1)").arg(file));
    resetDetections();
    updatePreview(currentImage_);
    ensureUiState();
}

void FaceRecognitionWidget::onLoadAnimated() {
    stopCamera();
    teardownAnimated();

    const QString file = QFileDialog::getOpenFileName(
        this,
        QCoreApplication::translate("FaceRecognitionWidget", "Select Animated Image"),
        QString(),
        QCoreApplication::translate("FaceRecognitionWidget", "Animated (*.gif *.apng)"));
    if (file.isEmpty()) return;

    animatedMovie_ = new QMovie(file);
    if (!animatedMovie_->isValid()) {
        showError(QCoreApplication::translate("FaceRecognitionWidget",
                                              "Unable to load animated image: %1")
                      .arg(file));
        delete animatedMovie_;
        animatedMovie_ = nullptr;
        return;
    }

    animatedMovie_->setCacheMode(QMovie::CacheAll);
    connect(animatedMovie_, &QMovie::frameChanged, this, &FaceRecognitionWidget::onAnimatedFrame);
    animatedMovie_->start();
    currentSource_ = FaceSourceMode::Animated;
    currentSourceDescription_ = file;
    updateSourceBanner(
        QCoreApplication::translate("FaceRecognitionWidget", "Source: Animation (%1)").arg(file));
    resetDetections();
    ensureUiState();
}

void FaceRecognitionWidget::onLoadModel() {
    QString initialDir;
    if (!currentModelPath_.isEmpty()) {
        QFileInfo info(currentModelPath_);
        initialDir = info.isDir() ? info.absoluteFilePath() : info.absolutePath();
    }
    if (initialDir.isEmpty()) { initialDir = QCoreApplication::applicationDirPath(); }

    const QString selectedFile = QFileDialog::getOpenFileName(
        this,
        QCoreApplication::translate("FaceRecognitionWidget", "Select Model File"),
        initialDir,
        QCoreApplication::translate("FaceRecognitionWidget", "Dlib model (*.dat)"));
    if (selectedFile.isEmpty()) return;

    ensureBackendReady(false);

    const QString destinationRoot = resolveModelDestinationRoot();
    if (destinationRoot.isEmpty()) {
        showError(QCoreApplication::translate("FaceRecognitionWidget",
                                              "Unable to resolve destination directory."));
        return;
    }

    const QString cleanDestination = QDir::cleanPath(destinationRoot);
    QFileInfo selectedInfo(selectedFile);
    const QString sourceDirPath = QDir::cleanPath(selectedInfo.dir().absolutePath());
    const QString destinationDirPath = QDir::cleanPath(QDir(cleanDestination).absolutePath());

    if (!sourceDirPath.isEmpty() && sourceDirPath == destinationDirPath) {
        if (loadModelFromPath(selectedInfo.absoluteFilePath(), true, true, true)) {
            refreshDatabase(true);
        }
        return;
    }

    if (!prepareModelDestination(cleanDestination)) {
        showError(QCoreApplication::translate("FaceRecognitionWidget",
                                              "Unable to prepare model directory: %1")
                      .arg(QDir::toNativeSeparators(cleanDestination)));
        return;
    }

    QString loadTarget;
    QStringList errors;
    if (!copyModelArtifacts(selectedFile, cleanDestination, loadTarget, errors)) {
        const QString detail = errors.join(QLatin1Char('\n'));
        updateModelStatus(loadTarget.isEmpty() ? cleanDestination : loadTarget, false, detail);
        showError(QCoreApplication::translate("FaceRecognitionWidget",
                                              "Failed to prepare model files:\n%1")
                      .arg(detail));
        return;
    }

    if (loadModelFromPath(loadTarget, true, true, true)) { refreshDatabase(true); }
}

void FaceRecognitionWidget::onToggleCamera() {
    if (cameraActive_) {
        stopCamera();
        updateSourceBanner(QCoreApplication::translate("FaceRecognitionWidget", "Camera stopped"));
        currentSource_ = FaceSourceMode::None;
        ensureUiState();
        return;
    }

    if (!ensureCamera()) { return; }

    currentSource_ = FaceSourceMode::Camera;
    currentSourceDescription_ = QCoreApplication::translate("FaceRecognitionWidget", "Live Camera");
    updateSourceBanner(QCoreApplication::translate("FaceRecognitionWidget", "Source: Camera"));
    resetDetections();
    ensureUiState();
}

void FaceRecognitionWidget::onCaptureFrame() {
    if (!cameraActive_) return;
    if (lastCameraFrame_.isNull()) {
        showError(QCoreApplication::translate(
            "FaceRecognitionWidget", "No camera frame available. Wait for camera to initialize."));
        return;
    }

    currentImage_ = lastCameraFrame_;
    currentSource_ = FaceSourceMode::Camera;
    updateSourceBanner(
        QCoreApplication::translate("FaceRecognitionWidget", "Source: Camera snapshot"));
    resetDetections();
    updatePreview(currentImage_);
    ensureUiState();
}

void FaceRecognitionWidget::onDetectFaces() {
    if (currentImage_.isNull()) {
        showError(
            QCoreApplication::translate("FaceRecognitionWidget", "Please provide an image first."));
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
    const auto detectCode = hr.detect(currentImage_, opts, boxes);
    if (detectCode != HumanRecognition::HRCode::Ok) {
        showError(QCoreApplication::translate("FaceRecognitionWidget", "Detection failed: %1")
                      .arg(hrCodeToString(detectCode)));
        return;
    }

    if (boxes.isEmpty()) {
        showInfo(QCoreApplication::translate("FaceRecognitionWidget", "No faces detected."));
        resetDetections();
        ensureUiState();
        return;
    }

    detections_.clear();
    resultsList_->clear();
    detections_.reserve(boxes.size());

    for (int i = 0; i < boxes.size(); ++i) {
        DetectionEntry entry;
        entry.box = boxes[i];
        if (autoMatchCheck_->isChecked()) {
            extractFeatureForBox(currentImage_, entry.box, entry);
            if (entry.feature.has_value()) {
                HumanRecognition::RecognitionMatch match;
                if (hr.findNearest(entry.feature.value(), match) == HumanRecognition::HRCode::Ok) {
                    entry.match = match;
                }
            }
        }
        detections_.push_back(entry);
        appendResult(static_cast<std::size_t>(i), detections_.back());
    }

    resultsList_->setCurrentRow(0);
    lastBoxes_ = boxes;
    updatePreview(currentImage_, boxes);
    ensureUiState();
}

void FaceRecognitionWidget::onClearResults() {
    resetDetections();
    ensureUiState();
    if (!currentImage_.isNull()) { updatePreview(currentImage_); }
}

void FaceRecognitionWidget::onRegisterSelectedFace() {
    DetectionEntry entry;
    if (!selectedDetection(entry)) {
        showError(
            QCoreApplication::translate("FaceRecognitionWidget", "Select a detection first."));
        return;
    }

    if (personIdEdit_->text().trimmed().isEmpty()) {
        // Auto-generate UUID when not provided by user
        const QString gen = QUuid::createUuid().toString(QUuid::WithoutBraces);
        personIdEdit_->setText(gen);
    }

    if (!entry.feature.has_value()) {
        if (extractFeatureForBox(currentImage_, entry.box, entry) != HumanRecognition::HRCode::Ok) {
            showError(
                QCoreApplication::translate("FaceRecognitionWidget", "Failed to extract feature."));
            return;
        }
        detections_[selectedDetectionIndex_] = entry;
        appendResult(static_cast<std::size_t>(selectedDetectionIndex_), entry);
    }

    HumanRecognition::PersonInfo person;
    person.id = personIdEdit_->text().trimmed();
    person.name = personNameEdit_->text().trimmed();

    // No metadata field: ensure empty metadata
    person.metadata = QJsonObject();

    person.canonicalFeature = entry.feature;

    if (!ensureBackendReady()) return;
    auto& hr = HumanRecognition::HumanRecognition::instance();
    const auto code = hr.registerPerson(person);
    if (code != HumanRecognition::HRCode::Ok) {
        showError(QCoreApplication::translate("FaceRecognitionWidget", "Register failed: %1")
                      .arg(hrCodeToString(code)));
        return;
    }

    showInfo(
        QCoreApplication::translate("FaceRecognitionWidget", "Person registered successfully."));
    refreshDatabase(true);
    ensureUiState();
}

void FaceRecognitionWidget::onMatchSelectedFace() {
    DetectionEntry entry;
    if (!selectedDetection(entry)) {
        showError(
            QCoreApplication::translate("FaceRecognitionWidget", "Select a detection first."));
        return;
    }
    if (!ensureBackendReady()) return;

    if (!entry.feature.has_value()) {
        if (extractFeatureForBox(currentImage_, entry.box, entry) != HumanRecognition::HRCode::Ok) {
            showError(
                QCoreApplication::translate("FaceRecognitionWidget", "Failed to extract feature."));
            return;
        }
    }

    HumanRecognition::RecognitionMatch match;
    auto& hr = HumanRecognition::HumanRecognition::instance();
    const auto code = hr.findNearest(entry.feature.value(), match);
    if (code != HumanRecognition::HRCode::Ok) {
        showError(QCoreApplication::translate("FaceRecognitionWidget", "Match failed: %1")
                      .arg(hrCodeToString(code)));
        return;
    }

    entry.match = match;
    detections_[selectedDetectionIndex_] = entry;
    appendResult(static_cast<std::size_t>(selectedDetectionIndex_), entry);
    ensureUiState();
}

void FaceRecognitionWidget::onResultSelectionChanged() {
    selectedDetectionIndex_ = resultsList_->currentRow();
    DetectionEntry entry;
    const bool hasSelection = selectedDetection(entry);
    enableRegistrationButtons(hasSelection);
    enableMatchButtons(hasSelection && !currentImage_.isNull());
    ensureUiState();
}

void FaceRecognitionWidget::onRemovePerson() {
    if (!ensureBackendReady()) return;
    auto selectionModel = databaseView_->selectionModel();
    if (!selectionModel) return;

    const auto indexes = selectionModel->selectedRows();
    if (indexes.isEmpty()) return;

    auto& hr = HumanRecognition::HumanRecognition::instance();
    for (const QModelIndex& idx : indexes) {
        const QString personId =
            databaseModel_->data(databaseModel_->index(idx.row(), 0)).toString();
        if (personId.isEmpty()) continue;
        const auto code = hr.removePerson(personId);
        if (code != HumanRecognition::HRCode::Ok) {
            showError(
                QCoreApplication::translate("FaceRecognitionWidget", "Failed to remove %1: %2")
                    .arg(personId, hrCodeToString(code)));
            return;
        }
    }

    refreshDatabase(true);
}

void FaceRecognitionWidget::onRefreshPersons() {
    refreshDatabase(true);
}

void FaceRecognitionWidget::onCameraFrame(const QVideoFrame& frame) {
    if (!cameraActive_) return;

    QVideoFrame copy(frame);
    if (!copy.isValid()) return;

    const QImage image = copy.toImage();
    if (image.isNull()) return;

    lastCameraFrame_ = toDisplayImage(image);
    if (currentSource_ == FaceSourceMode::Camera) { currentImage_ = lastCameraFrame_; }

    if (detections_.isEmpty()) {
        updatePreview(lastCameraFrame_);
    } else {
        updatePreview(lastCameraFrame_, lastBoxes_);
    }
}

void FaceRecognitionWidget::onVideoFrame(const QVideoFrame& frame) {
    Q_UNUSED(frame);
}

void FaceRecognitionWidget::onAnimatedFrame(int) {
    if (!animatedMovie_) return;
    const QImage frame = animatedMovie_->currentImage();
    if (frame.isNull()) return;

    lastAnimatedFrame_ = toDisplayImage(frame);
    currentImage_ = lastAnimatedFrame_;

    if (detections_.isEmpty()) {
        updatePreview(currentImage_);
    } else {
        updatePreview(currentImage_, lastBoxes_);
    }

    ensureUiState();
}

bool FaceRecognitionWidget::ensureBackendReady(bool warnAboutModel) {
    constexpr auto backendName = "opencv_dlib";
    auto& hr = HumanRecognition::HumanRecognition::instance();

    if (!hr.hasBackend() || hr.currentBackendName() != QString::fromLatin1(backendName)) {
        auto code = hr.setBackend(QString::fromLatin1(backendName));
        if (code != HumanRecognition::HRCode::Ok) {
            showError(QCoreApplication::translate("FaceRecognitionWidget",
                                                  "Failed to activate backend '%1': %2")
                          .arg(QString::fromLatin1(backendName), hrCodeToString(code)));
            backendReady_ = false;
            return false;
        }
        backendReady_ = true;
        modelLoaded_ = false;
    } else if (!backendReady_) {
        backendReady_ = true;
    }

    if (!backendReady_) return false;
    if (modelLoaded_) return true;

    if (currentModelPath_.isEmpty()) {
        QString storedPath = config::ConfigManager::instance().getString(
            QStringLiteral("HumanRecognition/ModelFile"), QString());
        if (storedPath.isEmpty()) {
            storedPath = config::ConfigManager::instance().getString(
                QStringLiteral("HumanRecognition/ModelDirectory"), QString());
        }
        if (!storedPath.isEmpty()) {
            QFileInfo info(storedPath);
            currentModelPath_ = info.absoluteFilePath();
        }
    }

    if (!currentModelPath_.isEmpty()) {
        QFileInfo info(currentModelPath_);
        const QString candidate = info.absoluteFilePath();
        if (info.exists()) {
            if (loadModelFromPath(candidate, false, false, warnAboutModel)) { return true; }
        } else {
            updateModelStatus(candidate, false);
        }
    } else {
        updateModelStatus(QString(), false);
    }

    if (warnAboutModel) {
        showError(QCoreApplication::translate(
            "FaceRecognitionWidget",
            "Face recognition model is not loaded. Please use 'Load Model'."));
    }
    return false;
}

void FaceRecognitionWidget::ensureUiState() {
    const bool hasImage = !currentImage_.isNull();
    btnDetect_->setEnabled(hasImage);
    btnClear_->setEnabled(!detections_.isEmpty());
    btnMatch_->setEnabled(selectedDetectionIndex_ >= 0 && hasImage);
    // Allow registering selected detection; generate ID automatically if empty
    btnRegister_->setEnabled(selectedDetectionIndex_ >= 0);
    btnCapture_->setEnabled(cameraActive_);
    btnToggleCamera_->setText(
        cameraActive_ ? QCoreApplication::translate("FaceRecognitionWidget", "Stop Camera")
                      : QCoreApplication::translate("FaceRecognitionWidget", "Start Camera"));

    if (auto* sel = databaseView_->selectionModel()) {
        btnRemovePerson_->setEnabled(sel->hasSelection());
    } else {
        btnRemovePerson_->setEnabled(false);
    }
}

void FaceRecognitionWidget::updatePreview(const QImage& image,
                                          const QVector<HumanRecognition::FaceBox>& boxes) {
    if (image.isNull()) {
        previewLabel_->setPixmap(QPixmap());
        previewLabel_->setText(
            QCoreApplication::translate("FaceRecognitionWidget", "No image loaded"));
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
                for (const auto& pt : box.landmarks) painter.drawPoint(pt);
                painter.setPen(pen);
            }
        }
    }

    previewLabel_->setPixmap(QPixmap::fromImage(annotated));
    previewLabel_->setText(QString());
}

void FaceRecognitionWidget::appendResult(std::size_t index, const DetectionEntry& entry) {
    QString text =
        QCoreApplication::translate("FaceRecognitionWidget", "Face %1: (%2,%3) %4x%5 | score=%6")
            .arg(static_cast<int>(index) + 1)
            .arg(entry.box.rect.x())
            .arg(entry.box.rect.y())
            .arg(entry.box.rect.width())
            .arg(entry.box.rect.height())
            .arg(entry.box.score, 0, 'f', 3);

    if (entry.feature.has_value()) {
        text += QCoreApplication::translate("FaceRecognitionWidget", "\nFeature length: %1")
                    .arg(entry.feature->values.size());
        if (entry.feature->norm.has_value()) {
            text += QCoreApplication::translate("FaceRecognitionWidget", " | norm=%1")
                        .arg(*entry.feature->norm, 0, 'f', 3);
        }
    }

    if (entry.match.has_value()) {
        text += QCoreApplication::translate("FaceRecognitionWidget",
                                            "\nMatch: %1 (%2) | distance=%3 | score=%4")
                    .arg(entry.match->personName.isEmpty()
                             ? QCoreApplication::translate("FaceRecognitionWidget", "Unnamed")
                             : entry.match->personName)
                    .arg(entry.match->personId)
                    .arg(entry.match->distance, 0, 'f', 3)
                    .arg(entry.match->score, 0, 'f', 3);
    }

    if (index < static_cast<std::size_t>(resultsList_->count())) {
        auto* item = resultsList_->item(static_cast<int>(index));
        item->setText(text);
    } else {
        auto* item = new QListWidgetItem(text, resultsList_);
        item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        resultsList_->addItem(item);
    }
}

void FaceRecognitionWidget::resetDetections() {
    detections_.clear();
    resultsList_->clear();
    selectedDetectionIndex_ = -1;
    lastBoxes_.clear();
    enableRegistrationButtons(false);
    enableMatchButtons(false);
}

void FaceRecognitionWidget::showError(const QString& message) {
    QMessageBox::warning(
        this, QCoreApplication::translate("FaceRecognitionWidget", "Face Recognition"), message);
}

void FaceRecognitionWidget::showInfo(const QString& message) {
    QMessageBox::information(
        this, QCoreApplication::translate("FaceRecognitionWidget", "Face Recognition"), message);
}

bool FaceRecognitionWidget::ensureCamera() {
    if (cameraActive_) return true;

    const auto devices = QMediaDevices::videoInputs();
    if (devices.isEmpty()) {
        showError(
            QCoreApplication::translate("FaceRecognitionWidget", "No camera device available."));
        return false;
    }

    stopCamera();

    camera_ = new QCamera(devices.front());
    cameraSession_ = new QMediaCaptureSession(this);
    cameraSink_ = new QVideoSink(this);
    connect(
        cameraSink_, &QVideoSink::videoFrameChanged, this, &FaceRecognitionWidget::onCameraFrame);
    cameraSession_->setCamera(camera_);
    cameraSession_->setVideoSink(cameraSink_);

    camera_->start();
    cameraActive_ = camera_->isActive();
    if (!cameraActive_) {
        showError(QCoreApplication::translate("FaceRecognitionWidget", "Failed to start camera."));
        stopCamera();
        return false;
    }

    return true;
}

void FaceRecognitionWidget::stopCamera() {
    if (camera_) {
        camera_->stop();
        delete camera_;
        camera_ = nullptr;
    }
    delete cameraSession_;
    cameraSession_ = nullptr;
    delete cameraSink_;
    cameraSink_ = nullptr;
    cameraActive_ = false;
    lastCameraFrame_ = QImage();
    ensureUiState();
}

void FaceRecognitionWidget::teardownAnimated() {
    if (!animatedMovie_) return;
    animatedMovie_->stop();
    animatedMovie_->deleteLater();
    animatedMovie_ = nullptr;
    lastAnimatedFrame_ = QImage();
}

void FaceRecognitionWidget::teardownVideo() {
    if (videoPlayer_) {
        videoPlayer_->stop();
        videoPlayer_->deleteLater();
        videoPlayer_ = nullptr;
    }
    if (videoSink_) {
        videoSink_->deleteLater();
        videoSink_ = nullptr;
    }
    lastVideoFrame_ = QImage();
}

void FaceRecognitionWidget::refreshDatabase(bool warnAboutModel) {
    if (!ensureBackendReady(warnAboutModel)) return;

    auto& hr = HumanRecognition::HumanRecognition::instance();
    QVector<HumanRecognition::PersonInfo> persons;
    const auto code = hr.listPersons(persons);
    if (code != HumanRecognition::HRCode::Ok) {
        showError(QCoreApplication::translate("FaceRecognitionWidget", "Load database failed: %1")
                      .arg(hrCodeToString(code)));
        return;
    }

    populateDatabaseModel(persons);
    ensureUiState();
}

bool FaceRecognitionWidget::selectedDetection(DetectionEntry& entry) const {
    if (selectedDetectionIndex_ < 0 ||
        selectedDetectionIndex_ >= static_cast<int>(detections_.size()))
        return false;
    entry = detections_.at(selectedDetectionIndex_);
    return true;
}

HumanRecognition::HRCode FaceRecognitionWidget::extractFeatureForBox(
    const QImage& image, const HumanRecognition::FaceBox& box, DetectionEntry& entry) {
    if (!ensureBackendReady()) return HumanRecognition::HRCode::UnknownError;
    auto& hr = HumanRecognition::HumanRecognition::instance();
    HumanRecognition::FaceFeature feature;
    const auto code = hr.extractFeature(image, box, feature);
    if (code == HumanRecognition::HRCode::Ok) { entry.feature = feature; }
    return code;
}

void FaceRecognitionWidget::populateDatabaseModel(
    const QVector<HumanRecognition::PersonInfo>& persons) {
    // prevent reacting to programmatic model updates
    suppressDatabaseModelSignals_ = true;
    databaseModel_->clear();
    databaseModel_->setHorizontalHeaderLabels(
        {QCoreApplication::translate("FaceRecognitionWidget", "ID"),
         QCoreApplication::translate("FaceRecognitionWidget", "Name"),
         QCoreApplication::translate("FaceRecognitionWidget", "Feature")});

    for (const auto& person : persons) {
        QList<QStandardItem*> row;
        auto* idItem = new QStandardItem(person.id);
        auto* nameItem = new QStandardItem(person.name);
        // allow editing of ID and Name
        idItem->setEditable(true);
        nameItem->setEditable(true);
        QString featureInfo;
        if (person.canonicalFeature.has_value()) {
            featureInfo = QCoreApplication::translate("FaceRecognitionWidget", "%1 values")
                              .arg(person.canonicalFeature->values.size());
        }
        auto* featureItem = new QStandardItem(featureInfo);
        row << idItem << nameItem << featureItem;
        databaseModel_->appendRow(row);
    }

    databaseView_->resizeColumnsToContents();
    databaseView_->clearSelection();
    suppressDatabaseModelSignals_ = false;
}

void FaceRecognitionWidget::onDatabaseItemChanged(QStandardItem* item) {
    if (suppressDatabaseModelSignals_) return;
    if (!item) return;

    // Columns: 0 = ID, 1 = Name, 2 = Feature
    const int row = item->row();
    const int col = item->column();
    if (row < 0) return;
    const QString id = databaseModel_->data(databaseModel_->index(row, 0)).toString().trimmed();
    const QString name = databaseModel_->data(databaseModel_->index(row, 1)).toString().trimmed();

    if (id.isEmpty()) {
        showError(QCoreApplication::translate("FaceRecognitionWidget", "Person ID is required."));
        return;
    }

    // Build PersonInfo from row and persist to backend. We will try to get existing feature if
    // present.
    HumanRecognition::PersonInfo person;
    person.id = id;
    person.name = name;

    // No metadata column: keep metadata empty
    person.metadata = QJsonObject();

    // try to preserve existing canonical feature: query backend
    if (ensureBackendReady()) {
        auto& hr = HumanRecognition::HumanRecognition::instance();
        HumanRecognition::PersonInfo existing;
        if (hr.getPerson(id, existing) == HumanRecognition::HRCode::Ok) {
            person.canonicalFeature = existing.canonicalFeature;
        }

        // If person exists, we perform remove+register if ID changed, otherwise replace
        const auto codeRegister = hr.registerPerson(person);
        if (codeRegister == HumanRecognition::HRCode::PersonExists) {
            // try replace by allowing persist to replace via backend internal API: remove then
            // register
            const auto codeRemove = hr.removePerson(id);
            if (codeRemove != HumanRecognition::HRCode::Ok) {
                showError(
                    QCoreApplication::translate("FaceRecognitionWidget", "Failed to update %1: %2")
                        .arg(id, hrCodeToString(codeRemove)));
                return;
            }
            const auto codeRetry = hr.registerPerson(person);
            if (codeRetry != HumanRecognition::HRCode::Ok) {
                showError(
                    QCoreApplication::translate("FaceRecognitionWidget", "Failed to update %1: %2")
                        .arg(id, hrCodeToString(codeRetry)));
                return;
            }
        } else if (codeRegister != HumanRecognition::HRCode::Ok) {
            showError(QCoreApplication::translate("FaceRecognitionWidget", "Failed to save %1: %2")
                          .arg(id, hrCodeToString(codeRegister)));
            return;
        }

        // Refresh database view to reflect any changes (IDs may have been normalized)
        refreshDatabase(true);
    }
}

void FaceRecognitionWidget::enableRegistrationButtons(bool enabled) {
    btnRegister_->setEnabled(enabled);
}

void FaceRecognitionWidget::enableMatchButtons(bool enabled) {
    btnMatch_->setEnabled(enabled);
}

void FaceRecognitionWidget::updateSourceBanner(const QString& description) {
    sourceBanner_->setText(description);
}

QString FaceRecognitionWidget::resolveModelDestinationRoot() const {
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString rawPath = QDir(appDir).absoluteFilePath(QStringLiteral("../model"));
    return QDir::cleanPath(rawPath);
}

bool FaceRecognitionWidget::prepareModelDestination(const QString& destinationPath) {
    const QString cleanPath = QDir::cleanPath(destinationPath);
    QDir destDir(cleanPath);
    if (destDir.exists()) {
        if (!destDir.removeRecursively()) { return false; }
    }
    return QDir().mkpath(cleanPath);
}

bool FaceRecognitionWidget::copyModelArtifacts(const QString& sourcePath,
                                               const QString& destinationDir,
                                               QString& outLoadTarget,
                                               QStringList& errors) {
    outLoadTarget.clear();
    QFileInfo sourceInfo(sourcePath);
    if (!sourceInfo.exists()) {
        errors << QCoreApplication::translate("FaceRecognitionWidget",
                                              "Source path does not exist: %1")
                      .arg(QDir::toNativeSeparators(sourcePath));
        return false;
    }

    const QString normalizedDestination = QDir::cleanPath(destinationDir);
    if (!QDir().mkpath(normalizedDestination)) {
        errors << QCoreApplication::translate("FaceRecognitionWidget",
                                              "Unable to create destination directory: %1")
                      .arg(QDir::toNativeSeparators(normalizedDestination));
        return false;
    }

    if (sourceInfo.isDir()) {
        if (!copyDirectoryRecursive(sourceInfo.absoluteFilePath(), normalizedDestination, errors)) {
            return false;
        }
        QDir destDir(normalizedDestination);
        const QStringList datFiles = listDatFiles(destDir);
        if (datFiles.isEmpty()) {
            errors << QCoreApplication::translate("FaceRecognitionWidget",
                                                  "No .dat model files found in %1")
                          .arg(QDir::toNativeSeparators(normalizedDestination));
            return false;
        }
        QString recognitionFile = selectRecognitionModel(datFiles);
        if (recognitionFile.isEmpty()) { recognitionFile = datFiles.first(); }
        outLoadTarget = destDir.filePath(recognitionFile);
        return true;
    }

    QDir sourceDir = sourceInfo.dir();
    QStringList datFiles = listDatFiles(sourceDir);
    if (!datFiles.contains(sourceInfo.fileName())) { datFiles.prepend(sourceInfo.fileName()); }

    QString recognitionFile = selectRecognitionModel(datFiles);
    if (recognitionFile.isEmpty()) { recognitionFile = sourceInfo.fileName(); }
    QString shapeFile = selectShapeModel(datFiles);

    bool missingCritical = false;
    if (shapeFile.isEmpty()) {
        errors << QCoreApplication::translate(
            "FaceRecognitionWidget",
            "Missing shape predictor model file (e.g. shape_predictor_5_face_landmarks.dat).");
        missingCritical = true;
    }
    if (recognitionFile.isEmpty()) {
        errors << QCoreApplication::translate("FaceRecognitionWidget",
                                              "Missing face recognition model file (e.g. "
                                              "dlib_face_recognition_resnet_model_v1.dat).");
        missingCritical = true;
    }

    const QStringList compressedFiles =
        sourceDir.entryList(QStringList() << QStringLiteral("*.bz2"), QDir::Files);
    for (const QString& compressed : compressedFiles) {
        const QString lower = compressed.toLower();
        if (!(lower.contains(QStringLiteral("shape_predictor")) ||
              lower.contains(QStringLiteral("recognition")) ||
              lower.contains(QStringLiteral("resnet")))) {
            continue;
        }

        QString candidateDat = compressed;
        candidateDat.chop(4);  // remove ".bz2"
        if (!candidateDat.endsWith(QStringLiteral(".dat"), Qt::CaseInsensitive)) {
            candidateDat.append(QStringLiteral(".dat"));
        }

        if (!datFiles.contains(candidateDat, Qt::CaseInsensitive)) {
            errors << QCoreApplication::translate(
                          "FaceRecognitionWidget",
                          "Compressed model file detected: %1. Please decompress it using the "
                          "provided download script before loading.")
                          .arg(QDir::toNativeSeparators(sourceDir.filePath(compressed)));
            missingCritical = true;
        }
    }

    if (missingCritical) { return false; }

    QStringList toCopy;
    toCopy << recognitionFile;
    if (!shapeFile.isEmpty()) { toCopy << shapeFile; }
    if (!toCopy.contains(sourceInfo.fileName())) { toCopy << sourceInfo.fileName(); }

    const QString legacyJson = QStringLiteral("people.json");
    if (sourceDir.exists(legacyJson)) { toCopy << legacyJson; }

    toCopy.removeDuplicates();

    for (const QString& fileName : toCopy) {
        const QString src = sourceDir.filePath(fileName);
        if (!QFile::exists(src)) {
            errors << QCoreApplication::translate("FaceRecognitionWidget",
                                                  "Required file missing: %1")
                          .arg(QDir::toNativeSeparators(src));
            return false;
        }
        const QString dest = QDir(normalizedDestination).filePath(fileName);
        if (QFile::exists(dest) && !QFile::remove(dest)) {
            errors << QCoreApplication::translate("FaceRecognitionWidget",
                                                  "Unable to overwrite file: %1")
                          .arg(QDir::toNativeSeparators(dest));
            return false;
        }
        if (!QFile::copy(src, dest)) {
            errors << QCoreApplication::translate("FaceRecognitionWidget",
                                                  "Failed to copy '%1' to '%2'")
                          .arg(QDir::toNativeSeparators(src), QDir::toNativeSeparators(dest));
            return false;
        }
    }

    outLoadTarget = QDir(normalizedDestination).filePath(recognitionFile);
    return true;
}

bool FaceRecognitionWidget::loadModelFromPath(const QString& path,
                                              bool persistConfig,
                                              bool showSuccessMessage,
                                              bool showFailureMessage) {
    QFileInfo info(path);
    const QString absolutePath = info.absoluteFilePath();
    if (!info.exists()) {
        modelLoaded_ = false;
        updateModelStatus(absolutePath, false);
        if (showFailureMessage) {
            showError(QCoreApplication::translate("FaceRecognitionWidget",
                                                  "Model path does not exist: %1")
                          .arg(QDir::toNativeSeparators(absolutePath)));
        }
        return false;
    }

    auto& hr = HumanRecognition::HumanRecognition::instance();
    const std::filesystem::path fsPath = std::filesystem::path(absolutePath.toStdWString());
    const auto code = hr.loadModel(fsPath);

    if (code != HumanRecognition::HRCode::Ok) {
        modelLoaded_ = false;
        const QString reason = hrCodeToString(code);
        updateModelStatus(absolutePath, false, reason);
        if (showFailureMessage) {
            showError(QCoreApplication::translate("FaceRecognitionWidget",
                                                  "Failed to load model from %1: %2")
                          .arg(QDir::toNativeSeparators(absolutePath), reason));
        }
        return false;
    }

    currentModelPath_ = absolutePath;
    modelLoaded_ = true;
    updateModelStatus(currentModelPath_, true);
    if (persistConfig) {
        auto& cfg = config::ConfigManager::instance();
        cfg.setValue(QStringLiteral("HumanRecognition/ModelFile"), currentModelPath_);
        cfg.remove(QStringLiteral("HumanRecognition/ModelDirectory"));
        cfg.sync();
    }
    if (showSuccessMessage) {
        showInfo(QCoreApplication::translate("FaceRecognitionWidget",
                                             "Model loaded successfully from %1.")
                     .arg(QDir::toNativeSeparators(currentModelPath_)));
    }
    return true;
}

void FaceRecognitionWidget::updateModelStatus(const QString& path,
                                              bool loaded,
                                              const QString& detail) {
    if (!modelStatusLabel_) return;

    QString text;
    QString normalizedPath;
    if (!path.isEmpty()) {
        QFileInfo info(path);
        normalizedPath = QDir::toNativeSeparators(info.absoluteFilePath());
    }

    if (loaded && !normalizedPath.isEmpty()) {
        text =
            QCoreApplication::translate("FaceRecognitionWidget", "Model: %1").arg(normalizedPath);
    } else if (!normalizedPath.isEmpty()) {
        text = QCoreApplication::translate("FaceRecognitionWidget", "Model: %1 (unavailable)")
                   .arg(normalizedPath);
    } else {
        text = QCoreApplication::translate("FaceRecognitionWidget", "Model: not loaded");
    }

    if (!detail.isEmpty()) { text += QStringLiteral(" — %1").arg(detail); }

    const QString color =
        loaded ? QStringLiteral("#15803d")
               : (normalizedPath.isEmpty() ? QStringLiteral("#f97316") : QStringLiteral("#dc2626"));
    modelStatusLabel_->setText(text);
    modelStatusLabel_->setStyleSheet(QStringLiteral("color: %1;").arg(color));

    QString tooltip;
    if (!normalizedPath.isEmpty()) { tooltip = normalizedPath; }
    if (!detail.isEmpty()) {
        if (!tooltip.isEmpty()) tooltip.append(QStringLiteral("\n"));
        tooltip.append(detail);
    }
    modelStatusLabel_->setToolTip(tooltip);
}
