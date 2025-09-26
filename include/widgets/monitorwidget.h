#ifndef MONITORWIDGET_H
#define MONITORWIDGET_H

#include <QByteArray>
#include <QPixmap>
#include <QWidget>
#include <cstdint>

class TcpConnect;
class QThread;

QT_BEGIN_NAMESPACE
namespace Ui {
class MonitorWidget;
}
QT_END_NAMESPACE

class MonitorWidget : public QWidget {
    Q_OBJECT

   public:
    MonitorWidget(QWidget* parent = nullptr);
    ~MonitorWidget();

   signals:
    void backToMain();

   private slots:
    void on_btnBackFromMonitor_clicked();
    void on_btnConnect_clicked();
    void on_btnStop_clicked();
    void on_btnChoosePath_clicked();
    void on_btnSaveDevice_clicked();
    void on_comboDevices_currentIndexChanged(int index);
    void onDataReady(const QByteArray& data);

   private:
    Ui::MonitorWidget* ui;
    // connection and receive buffer
    // synchronous connect worker running in background thread
    TcpConnect* m_syncConn = nullptr;
    QThread* m_workerThread = nullptr;
    QByteArray m_recvBuffer;
    QPixmap m_lastPixmap;

    // device edit fields live in UI: editHost, editPort

    uint16_t crc16_ccitt(const uint8_t* buf, uint32_t len);
};

#endif  // MONITORWIDGET_H