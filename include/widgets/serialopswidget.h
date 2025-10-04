#ifndef SERIALOPSWIDGET_H
#define SERIALOPSWIDGET_H

#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui { class SerialOpsWidget; }
QT_END_NAMESPACE

class QSerialPort;
class QProgressBar;
class QTimer;

class SerialOpsWidget : public QWidget {
    Q_OBJECT
public:
    explicit SerialOpsWidget(QWidget* parent = nullptr);
    ~SerialOpsWidget();

private slots:
    void on_btnOpen_clicked();
    void on_btnClose_clicked();
    void on_btnSend_clicked();
    void on_btnSaveAction_clicked();
    void on_btnLoadAction_clicked();
    void on_btnClear_clicked();

private:
    Ui::SerialOpsWidget* ui;
    QSerialPort* m_port = nullptr;
    QTimer* m_activityTimer = nullptr;

    void setConnected(bool connected);
};

#endif // SERIALOPSWIDGET_H
