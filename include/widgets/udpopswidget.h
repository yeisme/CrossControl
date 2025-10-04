#ifndef UDPOPSWIDGET_H
#define UDPOPSWIDGET_H

#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui {
class UdpOpsWidget;
}
QT_END_NAMESPACE

class QUdpSocket;

class UdpOpsWidget : public QWidget {
    Q_OBJECT
   public:
    explicit UdpOpsWidget(QWidget* parent = nullptr);
    ~UdpOpsWidget() override;

   private slots:
    void on_btnBind_clicked();
    void on_btnSend_clicked();
    void on_btnSaveAction_clicked();
    void on_btnLoadAction_clicked();
    void on_btnClear_clicked();

   private:
    Ui::UdpOpsWidget* ui;
    QUdpSocket* m_sock = nullptr;

    void setBound(bool bound);
    QByteArray serializeUdpAction(const QString& peer, const QByteArray& body);
    bool deserializeUdpAction(const QByteArray& payload, QString& peer, QByteArray& body);
};

#endif  // UDPOPSWIDGET_H
