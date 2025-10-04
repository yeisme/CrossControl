#ifndef HTTPOPSWIDGET_H
#define HTTPOPSWIDGET_H

#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui {
class HttpOpsWidget;
}
QT_END_NAMESPACE

class QNetworkAccessManager;

class HttpOpsWidget : public QWidget {
    Q_OBJECT
   public:
    explicit HttpOpsWidget(QWidget* parent = nullptr);
    ~HttpOpsWidget();

   private slots:
    void on_btnSend_clicked();
    void on_btnSaveAction_clicked();
    void on_btnLoadAction_clicked();
    void on_btnClear_clicked();
    void on_btnKV_clicked();

   private:
    Ui::HttpOpsWidget* ui;
    QNetworkAccessManager* m_net = nullptr;

    QByteArray serializeHttpAction(const QString& url,
                                   const QString& method,
                                   const QString& token,
                                   bool autoBearer,
                                   const QByteArray& body,
                                   const QString& contentType,
                                   bool sendAsJson);
    bool deserializeHttpAction(const QByteArray& payload,
                               QString& url,
                               QString& method,
                               QString& token,
                               bool& autoBearer,
                               QByteArray& body,
                               QString& contentType,
                               bool& sendAsJson);
};

#endif  // HTTPOPSWIDGET_H
