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

#include "modules/HumanRecognition/humanrecognition.h"

FaceRecognitionWidget::FaceRecognitionWidget(QWidget* parent) : QWidget(parent) {
    m_hr = new HumanRecognition(this);

    m_btnLoad =
        new QPushButton(QCoreApplication::translate("FaceRecognitionWidget", "Load Image"), this);
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
    left->addWidget(m_btnLoad);
    // Camera not enabled in this build; keep capture button hidden
    m_btnCapture->setVisible(false);

    // name + type on one row
    auto rowName = new QHBoxLayout();
    rowName->addWidget(new QLabel(QCoreApplication::translate("FaceRecognitionWidget", "Name:"), this));
    rowName->addWidget(m_editName, 1);
    rowName->addWidget(new QLabel(QCoreApplication::translate("FaceRecognitionWidget", "Type:"), this));
    rowName->addWidget(m_comboFrequency);
    left->addLayout(rowName);

    // phone and note below
    left->addWidget(new QLabel(QCoreApplication::translate("FaceRecognitionWidget", "Phone:"), this));
    left->addWidget(m_editPhone);
    left->addWidget(new QLabel(QCoreApplication::translate("FaceRecognitionWidget", "Note:"), this));
    left->addWidget(m_editNote);

    left->addWidget(m_btnEnroll);
    left->addWidget(m_btnMatch);

    auto right = new QVBoxLayout();
    right->addWidget(new QLabel(
        QCoreApplication::translate("FaceRecognitionWidget", "Registered Persons:"), this));
    right->addWidget(m_listPersons);
    auto h2 = new QHBoxLayout();
    h2->addWidget(m_btnRefresh);
    h2->addWidget(m_btnDelete);
    right->addLayout(h2);
    right->addWidget(m_log);

    auto main = new QHBoxLayout(this);
    main->addLayout(left);
    main->addLayout(right);

    connect(m_btnLoad, &QPushButton::clicked, this, &FaceRecognitionWidget::onLoadImage);
    connect(m_btnEnroll, &QPushButton::clicked, this, &FaceRecognitionWidget::onEnroll);
    connect(m_btnMatch, &QPushButton::clicked, this, &FaceRecognitionWidget::onMatch);
    connect(m_btnRefresh, &QPushButton::clicked, this, &FaceRecognitionWidget::onRefreshList);
    connect(m_btnDelete, &QPushButton::clicked, this, &FaceRecognitionWidget::onDeletePerson);

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
