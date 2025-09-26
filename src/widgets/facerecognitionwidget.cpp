#include "facerecognitionwidget.h"

#include <qcoreapplication.h>

#include <QBoxLayout>
#include <QComboBox>
#include <QFileDialog>
#include <QFrame>
#include <QImageReader>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPixmap>
#include <QPushButton>
#include <QSize>
#include <QTimer>

// OpenCV not required for widget UI; backend handles minimal OpenCV usage where needed.

#include "modules/HumanRecognition/humanrecognition.h"

// Qt Multimedia is required for camera support
#include <QCamera>
#include <QMediaCaptureSession>
#include <QMediaDevices>
#include <QVideoSink>
// avoid requiring the QVideoFrame header here; use auto in the slot

FaceRecognitionWidget::FaceRecognitionWidget(QWidget* parent) : QWidget(parent) {
    m_hr = new HumanRecognition(this);

    m_btnLoad =
        new QPushButton(QCoreApplication::translate("FaceRecognitionWidget", "Load Image"), this);
    m_btnLoadForEnroll = new QPushButton(
        QCoreApplication::translate("FaceRecognitionWidget", "Load Image for Enroll"), this);
    m_btnCapture =
        new QPushButton(QCoreApplication::translate("FaceRecognitionWidget", "Capture"), this);
    m_btnEnroll =
        new QPushButton(QCoreApplication::translate("FaceRecognitionWidget", "Enroll"), this);
    m_btnMatch =
        new QPushButton(QCoreApplication::translate("FaceRecognitionWidget", "Match"), this);
    m_btnRefresh =
        new QPushButton(QCoreApplication::translate("FaceRecognitionWidget", "Refresh"), this);
    m_btnDelete =
        new QPushButton(QCoreApplication::translate("FaceRecognitionWidget", "Delete"), this);
    m_btnEnableCam = new QPushButton(
        QCoreApplication::translate("FaceRecognitionWidget", "Enable Camera"), this);
    m_btnCaptureTest =
        new QPushButton(QCoreApplication::translate("FaceRecognitionWidget", "Capture Test"), this);
    m_btnCaptureForEnroll = new QPushButton(
        QCoreApplication::translate("FaceRecognitionWidget", "Capture & Enroll"), this);
    m_comboCameras = new QComboBox(this);

    m_editName = new QLineEdit(this);
    m_editPhone = new QLineEdit(this);
    m_editNote = new QLineEdit(this);
    m_comboFrequency = new QComboBox(this);
    m_comboFrequency->addItems(
        {QCoreApplication::translate("FaceRecognitionWidget", "Frequent"),
         QCoreApplication::translate("FaceRecognitionWidget", "Occasional")});

    m_listPersons = new QListWidget(this);
    m_imgPreview = new QLabel(this);
    m_imgPreview->setFixedSize(240, 180);
    m_imgPreview->setFrameStyle(QFrame::Box | QFrame::Plain);
    m_log = new QTextEdit(this);
    m_log->setReadOnly(true);

    auto left = new QVBoxLayout();
    left->addWidget(m_imgPreview);
    auto loadRow = new QHBoxLayout();
    loadRow->addWidget(m_btnLoad);
    loadRow->addWidget(m_btnLoadForEnroll);
    left->addLayout(loadRow);
    // camera controls
    left->addWidget(m_comboCameras);
    left->addWidget(m_btnEnableCam);
    auto camRow = new QHBoxLayout();
    camRow->addWidget(m_btnCaptureTest);
    camRow->addWidget(m_btnCaptureForEnroll);
    left->addLayout(camRow);
    m_btnCapture->setVisible(false);  // reserved

    // name + type on one row
    auto rowName = new QHBoxLayout();
    rowName->addWidget(
        new QLabel(QCoreApplication::translate("FaceRecognitionWidget", "Name:"), this));
    rowName->addWidget(m_editName, 1);
    rowName->addWidget(
        new QLabel(QCoreApplication::translate("FaceRecognitionWidget", "Type:"), this));
    rowName->addWidget(m_comboFrequency);
    left->addLayout(rowName);

    // phone and note below
    left->addWidget(
        new QLabel(QCoreApplication::translate("FaceRecognitionWidget", "Phone:"), this));
    left->addWidget(m_editPhone);
    left->addWidget(
        new QLabel(QCoreApplication::translate("FaceRecognitionWidget", "Note:"), this));
    left->addWidget(m_editNote);

    left->addWidget(m_btnEnroll);
    // keep enroll button for multi-image/manual flow
    left->addWidget(m_btnMatch);

    auto right = new QVBoxLayout();
    right->addWidget(new QLabel(
        QCoreApplication::translate("FaceRecognitionWidget", "Registered Persons:"), this));
    right->addWidget(m_listPersons);
    auto h2 = new QHBoxLayout();
    h2->addWidget(m_btnRefresh);
    h2->addWidget(m_btnDelete);
    right->addLayout(h2);
    // detailed person info below the list
    right->addWidget(
        new QLabel(QCoreApplication::translate("FaceRecognitionWidget", "Person Details:"), this));
    m_detailName = new QLabel(this);
    m_detailPhone = new QLabel(this);
    m_detailType = new QLabel(this);
    m_detailNote = new QLabel(this);
    right->addWidget(m_detailName);
    right->addWidget(m_detailPhone);
    right->addWidget(m_detailType);
    right->addWidget(m_detailNote);
    right->addWidget(m_log);

    auto main = new QHBoxLayout(this);
    main->addLayout(left);
    main->addLayout(right);

    connect(m_btnLoad, &QPushButton::clicked, this, &FaceRecognitionWidget::onLoadImage);
    connect(m_btnLoadForEnroll,
            &QPushButton::clicked,
            this,
            &FaceRecognitionWidget::onLoadImageForEnroll);
    connect(m_btnEnroll, &QPushButton::clicked, this, &FaceRecognitionWidget::onEnroll);
    connect(m_btnMatch, &QPushButton::clicked, this, &FaceRecognitionWidget::onMatch);
    connect(m_btnRefresh, &QPushButton::clicked, this, &FaceRecognitionWidget::onRefreshList);
    connect(m_btnDelete, &QPushButton::clicked, this, &FaceRecognitionWidget::onDeletePerson);
    connect(m_btnEnableCam, &QPushButton::clicked, this, &FaceRecognitionWidget::onEnableCamera);
    connect(m_btnCaptureTest, &QPushButton::clicked, this, &FaceRecognitionWidget::onCaptureTest);
    connect(m_btnCaptureForEnroll,
            &QPushButton::clicked,
            this,
            &FaceRecognitionWidget::onCaptureForEnroll);
    connect(
        m_listPersons, &QListWidget::itemClicked, this, &FaceRecognitionWidget::onPersonSelected);

    // camera timer
    m_camTimer = new QTimer(this);
    connect(m_camTimer, &QTimer::timeout, this, &FaceRecognitionWidget::onCameraTimer);

    // enumerate Qt multimedia video inputs
    for (const auto& dev : QMediaDevices::videoInputs()) {
        m_comboCameras->addItem(dev.description(), QVariant::fromValue(dev));
    }
    if (m_comboCameras->count() == 0) {
        m_comboCameras->addItem(
            QCoreApplication::translate("FaceRecognitionWidget", "(No camera found)"), -1);
    }

    onRefreshList();
}

// Camera functionality has been removed; capture button is hidden.

// capture from camera disabled in this build; no implementation

void FaceRecognitionWidget::log(const QString& s) {
    m_log->append(s);
}

void FaceRecognitionWidget::onLoadImage() {
    QString f = QFileDialog::getOpenFileName(
        this,
        QCoreApplication::translate("FaceRecognitionWidget", "Select image"),
        QString(),
        QCoreApplication::translate("FaceRecognitionWidget", "Images (*.png *.jpg *.jpeg)"));
    if (f.isEmpty()) return;
    QImage img(f);
    if (img.isNull()) {
        log(QCoreApplication::translate("FaceRecognitionWidget", "Failed to load image"));
        return;
    }
    m_currentImage = img;
    m_imgPreview->setPixmap(
        QPixmap::fromImage(img.scaled(m_imgPreview->size(), Qt::KeepAspectRatio)));
    this->log(QCoreApplication::translate("FaceRecognitionWidget", "Image loaded: %1").arg(f));
}

void FaceRecognitionWidget::onLoadImageForEnroll() {
    // same as onLoadImage but mark as enrollment image
    QString f = QFileDialog::getOpenFileName(
        this,
        QCoreApplication::translate("FaceRecognitionWidget", "Select image"),
        QString(),
        QCoreApplication::translate("FaceRecognitionWidget", "Images (*.png *.jpg *.jpeg)"));
    if (f.isEmpty()) return;
    QImage img(f);
    if (img.isNull()) {
        log(QCoreApplication::translate("FaceRecognitionWidget", "Failed to load image"));
        return;
    }
    // use this image directly for enroll
    QString name = m_editName->text().trimmed();
    if (name.isEmpty()) {
        log(QCoreApplication::translate("FaceRecognitionWidget", "Please input name"));
        return;
    }
    PersonInfo p;
    p.personId = name;
    p.name = name;
    p.extraJson = QStringLiteral("{\"type\":\"%1\", \"phone\": \"%2\", \"note\": \"%3\"}")
                      .arg(m_comboFrequency->currentText())
                      .arg(m_editPhone->text().trimmed())
                      .arg(m_editNote->text().trimmed());
    std::vector<QImage> faces{img};
    int added = m_hr->enroll(p, faces);
    if (added > 0) {
        this->log(
            QCoreApplication::translate("FaceRecognitionWidget", "Enrolled %1 embeddings for %2")
                .arg(added)
                .arg(name));
    } else {
        this->log(QCoreApplication::translate("FaceRecognitionWidget",
                                              "Enroll failed or no embeddings extracted"));
    }
    onRefreshList();
}

void FaceRecognitionWidget::onCaptureForEnroll() {
    if (m_lastFrame.isNull()) {
        log(QCoreApplication::translate("FaceRecognitionWidget", "No camera frame available"));
        return;
    }
    QString name = m_editName->text().trimmed();
    if (name.isEmpty()) {
        log(QCoreApplication::translate("FaceRecognitionWidget", "Please input name"));
        return;
    }
    PersonInfo p;
    p.personId = name;
    p.name = name;
    p.extraJson = QStringLiteral("{\"type\":\"%1\", \"phone\": \"%2\", \"note\": \"%3\"}")
                      .arg(m_comboFrequency->currentText())
                      .arg(m_editPhone->text().trimmed())
                      .arg(m_editNote->text().trimmed());
    std::vector<QImage> faces{m_lastFrame};
    int added = m_hr->enroll(p, faces);
    if (added > 0) {
        this->log(
            QCoreApplication::translate("FaceRecognitionWidget", "Enrolled %1 embeddings for %2")
                .arg(added)
                .arg(name));
    } else {
        this->log(QCoreApplication::translate("FaceRecognitionWidget",
                                              "Enroll failed or no embeddings extracted"));
    }
    onRefreshList();
}

void FaceRecognitionWidget::onEnroll() {
    if (m_currentImage.isNull()) {
        log(QCoreApplication::translate("FaceRecognitionWidget", "No image loaded"));
        return;
    }
    QString name = m_editName->text().trimmed();
    if (name.isEmpty()) {
        log(QCoreApplication::translate("FaceRecognitionWidget", "Please input name"));
        return;
    }
    PersonInfo p;
    p.personId = name;  // 简化：用 name 作为 id；在真实情形应生成 UUID
    p.name = name;
    p.extraJson = QStringLiteral("{\"type\":\"%1\", \"phone\": \"%2\", \"note\": \"%3\"}")
                      .arg(m_comboFrequency->currentText())
                      .arg(m_editPhone->text().trimmed())
                      .arg(m_editNote->text().trimmed());

    // enroll using single image (could be multiple)
    std::vector<QImage> faces{m_currentImage};
    int added = m_hr->enroll(p, faces);
    if (added > 0) {
        this->log(
            QCoreApplication::translate("FaceRecognitionWidget", "Enrolled %1 embeddings for %2")
                .arg(added)
                .arg(name));
    } else {
        this->log(QCoreApplication::translate("FaceRecognitionWidget",
                                              "Enroll failed or no embeddings extracted"));
    }
    onRefreshList();
}

void FaceRecognitionWidget::onMatch() {
    if (m_currentImage.isNull()) {
        log(QCoreApplication::translate("FaceRecognitionWidget", "No image loaded"));
        return;
    }
    auto res = m_hr->detectAndRecognize(m_currentImage);
    if (res.empty()) {
        log(QCoreApplication::translate("FaceRecognitionWidget", "No faces detected"));
        return;
    }
    for (const auto& r : res) {
        if (!r.matchedPersonName.isEmpty())
            this->log(QCoreApplication::translate("FaceRecognitionWidget", "Matched: %1 (dist=%2)")
                          .arg(r.matchedPersonName)
                          .arg(r.matchedDistance));
        else
            this->log(QCoreApplication::translate("FaceRecognitionWidget", "Face not matched"));
    }
}

void FaceRecognitionWidget::onRefreshList() {
    m_listPersons->clear();
    auto persons = m_hr->listPersons();
    for (const auto& p : persons) {
        QListWidgetItem* it = new QListWidgetItem(p.personId + " - " + p.name);
        it->setData(Qt::UserRole, p.personId);
        m_listPersons->addItem(it);
    }
    this->log(
        QCoreApplication::translate("FaceRecognitionWidget", "Refreshed person list: %1 entries")
            .arg(persons.size()));
}

void FaceRecognitionWidget::onPersonSelected(QListWidgetItem* item) {
    if (!item) return;
    m_selectedPersonId = item->data(Qt::UserRole).toString();
    // populate detail labels
    auto persons = m_hr->listPersons();
    for (const auto& p : persons) {
        if (p.personId == m_selectedPersonId) {
            m_detailName->setText(
                QCoreApplication::translate("FaceRecognitionWidget", "Name: %1").arg(p.name));
            // parse extraJson simple fields (type/phone/note) if present
            // naive parsing: find substrings
            QString type = m_comboFrequency->currentText();
            QString phone;
            QString note;
            // if extraJson contains phone and note, try extract
            if (!p.extraJson.isEmpty()) {
                // very light parse (not full JSON)
                QRegularExpression rePhone("\\\"phone\\\"\\s*:\\s*\\\"([^\\\"]*)\\\"");
                QRegularExpression reNote("\\\"note\\\"\\s*:\\s*\\\"([^\\\"]*)\\\"");
                auto m1 = rePhone.match(p.extraJson);
                if (m1.hasMatch()) phone = m1.captured(1);
                auto m2 = reNote.match(p.extraJson);
                if (m2.hasMatch()) note = m2.captured(1);
            }
            m_detailPhone->setText(
                QCoreApplication::translate("FaceRecognitionWidget", "Phone: %1").arg(phone));
            m_detailType->setText(
                QCoreApplication::translate("FaceRecognitionWidget", "Type: %1").arg(type));
            m_detailNote->setText(
                QCoreApplication::translate("FaceRecognitionWidget", "Note: %1").arg(note));
            break;
        }
    }
}

void FaceRecognitionWidget::onEnableCamera() {
    if (m_qtCamera) {
        // disable
        m_qtCamera->stop();
        delete m_qtVideoSink;
        m_qtVideoSink = nullptr;
        delete m_qtCaptureSession;
        m_qtCaptureSession = nullptr;
        delete m_qtCamera;
        m_qtCamera = nullptr;
        m_btnEnableCam->setText(
            QCoreApplication::translate("FaceRecognitionWidget", "Enable Camera"));
        this->log(QCoreApplication::translate("FaceRecognitionWidget", "Camera disabled"));
        return;
    }
    if (m_comboCameras->count() == 0) {
        this->log(QCoreApplication::translate("FaceRecognitionWidget", "No camera found"));
        return;
    }
    // get selected device
    QVariant v = m_comboCameras->currentData();
    if (!v.isValid()) {
        this->log(QCoreApplication::translate("FaceRecognitionWidget", "Invalid camera selection"));
        return;
    }
    auto device = v.value<QCameraDevice>();
    m_qtCamera = new QCamera(device, this);
    m_qtCaptureSession = new QMediaCaptureSession(this);
    m_qtVideoSink = new QVideoSink(this);
    m_qtCaptureSession->setCamera(m_qtCamera);
    m_qtCaptureSession->setVideoSink(m_qtVideoSink);
    connect(m_qtVideoSink, &QVideoSink::videoFrameChanged, this, [this](const auto& frame) {
        auto copy = frame;
        if (!copy.isValid()) return;
        QImage img = copy.toImage();
        if (img.isNull()) return;
        m_lastFrame = img.copy();
        m_imgPreview->setPixmap(
            QPixmap::fromImage(img.scaled(m_imgPreview->size(), Qt::KeepAspectRatio)));
    });
    m_qtCamera->start();
    m_btnEnableCam->setText(QCoreApplication::translate("FaceRecognitionWidget", "Disable Camera"));
    this->log(
        QCoreApplication::translate("FaceRecognitionWidget", "Camera enabled (Qt Multimedia)"));
}

void FaceRecognitionWidget::onCameraTimer() {
    Q_UNUSED(m_camTimer);
}

void FaceRecognitionWidget::onCaptureTest() {
    if (m_lastFrame.isNull()) {
        this->log(
            QCoreApplication::translate("FaceRecognitionWidget", "No camera frame available"));
        return;
    }
    if (m_selectedPersonId.isEmpty()) {
        this->log(QCoreApplication::translate("FaceRecognitionWidget",
                                              "Please select a registered person to test against"));
        return;
    }
    auto dets = m_hr->detectAndRecognize(m_lastFrame);
    if (dets.empty()) {
        this->log(
            QCoreApplication::translate("FaceRecognitionWidget", "No faces detected in capture"));
        return;
    }
    bool matched = false;
    for (const auto& d : dets) {
        if (!d.matchedPersonId.isEmpty() && d.matchedPersonId == m_selectedPersonId) {
            matched = true;
            this->log(QCoreApplication::translate("FaceRecognitionWidget",
                                                  "Capture matched selected person: %1 (dist=%2)")
                          .arg(d.matchedPersonName)
                          .arg(d.matchedDistance));
        }
    }
    if (!matched)
        this->log(QCoreApplication::translate("FaceRecognitionWidget",
                                              "Capture did not match selected person"));
}

void FaceRecognitionWidget::onDeletePerson() {
    auto it = m_listPersons->currentItem();
    if (!it) return;
    QString id = it->data(Qt::UserRole).toString();
    if (id.isEmpty()) return;
    if (m_hr->deletePerson(id)) {
        this->log(
            QCoreApplication::translate("FaceRecognitionWidget", "Deleted person: %1").arg(id));
    } else {
        this->log(
            QCoreApplication::translate("FaceRecognitionWidget", "Delete failed for %1").arg(id));
    }
    onRefreshList();
}
