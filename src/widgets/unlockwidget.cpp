#include "unlockwidget.h"
#include "ui_unlockwidget.h"
#include <QMessageBox>

UnlockWidget::UnlockWidget(QWidget *parent) : QWidget(parent), ui(new Ui::UnlockWidget) {
    ui->setupUi(this);
}

UnlockWidget::~UnlockWidget() {
    delete ui;
}

void UnlockWidget::on_btnConfirmUnlock_clicked() {
    QMessageBox::information(this, "开锁成功", "门锁已远程开启");
    emit cancelUnlock();  // Back to main
}

void UnlockWidget::on_btnCancelUnlock_clicked() {
    emit cancelUnlock();
}