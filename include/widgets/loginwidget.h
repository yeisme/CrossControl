#ifndef LOGINWIDGET_H
#define LOGINWIDGET_H

#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui {
class LoginWidget;
}
QT_END_NAMESPACE

class LoginWidget : public QWidget {
    Q_OBJECT

   public:
    LoginWidget(QWidget *parent = nullptr);
    ~LoginWidget();

   signals:
    void loginSuccess();

   private slots:
    void on_pushButtonLogin_clicked();

   private:
    Ui::LoginWidget *ui;
};

#endif  // LOGINWIDGET_H