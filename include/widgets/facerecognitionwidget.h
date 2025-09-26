#pragma once

#include <QImage>
#include <QWidget>

#include "modules/HumanRecognition/humanrecognition.h"

class QPushButton;
class QLineEdit;
class QTextEdit;
class QListWidget;
class QLabel;
class QComboBox;
class FaceRecognitionWidget : public QWidget {
    Q_OBJECT
   public:
    explicit FaceRecognitionWidget(QWidget* parent = nullptr);

   signals:
    void backToMain();

   private slots:
    void onLoadImage();
    void onEnroll();
    void onMatch();
    void onRefreshList();
    void onDeletePerson();

   private:
    HumanRecognition* m_hr = nullptr;
    QImage m_currentImage;

    // UI
    QPushButton* m_btnLoad;
    QPushButton* m_btnEnroll;
    QPushButton* m_btnMatch;
    QPushButton* m_btnRefresh;
    QPushButton* m_btnDelete;
    QPushButton* m_btnCapture;
    QLineEdit* m_editName;
    QLineEdit* m_editPhone;
    QLineEdit* m_editNote;
    QComboBox* m_comboFrequency;
    QListWidget* m_listPersons;
    QLabel* m_imgPreview;
    QTextEdit* m_log;
    QImage m_lastFrame;

    void log(const QString& s);
};
