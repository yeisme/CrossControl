#ifndef TCPOPSWIDGET_H
#define TCPOPSWIDGET_H

#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui {
class TcpOpsWidget;
}
QT_END_NAMESPACE

class QTcpSocket;

class TcpOpsWidget : public QWidget {
    Q_OBJECT
   public:
    explicit TcpOpsWidget(QWidget* parent = nullptr);
    ~TcpOpsWidget() override;

   private slots:
    void on_btnConnect_clicked();
    void on_btnDisconnect_clicked();
    void on_btnSend_clicked();
    void on_btnSaveAction_clicked();
    void on_btnLoadAction_clicked();
    void on_btnClear_clicked();

   private:
    Ui::TcpOpsWidget* ui;
    QTcpSocket* m_sock = nullptr;

    void setConnected(bool connected);
    QByteArray serializeTcpAction(const QString& host, int port, const QByteArray& body);
    bool deserializeTcpAction(const QByteArray& payload,
                              QString& host,
                              int& port,
                              QByteArray& body);
};

#endif  // TCPOPSWIDGET_H
