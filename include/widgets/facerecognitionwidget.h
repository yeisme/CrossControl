#pragma once

#include <QImage>
#include <QWidget>

#include "modules/HumanRecognition/humanrecognition.h"

class QPushButton;
class QLineEdit;
class QTextEdit;
class QListWidget;
class QListWidgetItem;
class QLabel;
class QComboBox;
class QTimer;
class QCamera;
class QMediaCaptureSession;
class QVideoSink;
class FaceRecognitionWidget : public QWidget {
    Q_OBJECT
   public:
    explicit FaceRecognitionWidget(QWidget* parent = nullptr);

   signals:
    void backToMain();

   private slots:
    void onLoadImage();
    void onLoadImageForEnroll();
    void onEnroll();
    void onCaptureForEnroll();
    void onMatch();
    void onRefreshList();
    void onDeletePerson();
    void onEnableCamera();
    void onCameraTimer();
    void onCaptureTest();
    void onPersonSelected(QListWidgetItem* item);

   private:
    HumanRecognition* m_hr = nullptr;
    QImage m_currentImage;

    // UI
    QPushButton* m_btnLoad;
    QPushButton* m_btnLoadForEnroll;  // new: load image specifically for enrollment via file
    QPushButton* m_btnEnroll;
    QPushButton* m_btnMatch;
    QPushButton* m_btnRefresh;
    QPushButton* m_btnDelete;
    QPushButton* m_btnCapture;
    QPushButton* m_btnEnableCam;
    QPushButton* m_btnCaptureTest;
    QPushButton* m_btnCaptureForEnroll;  // new: capture from camera and use for enroll
    QLineEdit* m_editName;
    QLineEdit* m_editPhone;
    QLineEdit* m_editNote;
    QComboBox* m_comboFrequency;
    QComboBox* m_comboCameras;
    QListWidget* m_listPersons;
    QLabel* m_imgPreview;
    QTextEdit* m_log;
    QImage m_lastFrame;
    QString m_selectedPersonId;

    // right-side detail display for selected person
    QLabel* m_detailName;
    QLabel* m_detailPhone;
    QLabel* m_detailType;
    QLabel* m_detailNote;

    // camera
    QTimer* m_camTimer = nullptr;
    // Qt Multimedia camera objects
    QCamera* m_qtCamera = nullptr;
    QMediaCaptureSession* m_qtCaptureSession = nullptr;
    QVideoSink* m_qtVideoSink = nullptr;

    void log(const QString& s);
};
