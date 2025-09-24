#include "unlockwidget.h"

#include <QMessageBox>

#include "ui_unlockwidget.h"

UnlockWidget::UnlockWidget(QWidget *parent) : QWidget(parent), ui(new Ui::UnlockWidget) { ui->setupUi(this); }

UnlockWidget::~UnlockWidget() { delete ui; }

void UnlockWidget::on_btnConfirmUnlock_clicked() {
    QMessageBox::information(this, tr("Unlock Successful"), tr("The door has been remotely unlocked"));
    emit cancelUnlock();  // Back to main
}

void UnlockWidget::on_btnCancelUnlock_clicked() { emit cancelUnlock(); }
