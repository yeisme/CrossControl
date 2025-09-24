#include "messagewidget.h"

#include "ui_messagewidget.h"

MessageWidget::MessageWidget(QWidget *parent) : QWidget(parent), ui(new Ui::MessageWidget) { ui->setupUi(this); }

MessageWidget::~MessageWidget() { delete ui; }

void MessageWidget::on_btnSendMessage_clicked() {
    QString message = ui->lineEditNewMessage->text();
    if (!message.isEmpty()) {
        ui->textEditMessages->append(message);
        ui->lineEditNewMessage->clear();
    }
}

void MessageWidget::on_btnBackFromMessage_clicked() { emit backToMain(); }