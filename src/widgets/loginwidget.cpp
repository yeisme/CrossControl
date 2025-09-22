#include "loginwidget.h"
#include "ui_loginwidget.h"

LoginWidget::LoginWidget(QWidget *parent) : QWidget(parent), ui(new Ui::LoginWidget) {
    ui->setupUi(this);
}

LoginWidget::~LoginWidget() {
    delete ui;
}

void LoginWidget::on_pushButtonLogin_clicked() {
    // Simple login logic, in real app check credentials
    QString username = ui->lineEditUsername->text();
    QString password = ui->lineEditPassword->text();
    if (!username.isEmpty() && !password.isEmpty()) {
        emit loginSuccess();
    }
}