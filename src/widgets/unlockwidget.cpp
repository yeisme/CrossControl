#include "unlockwidget.h"

#include <QCoreApplication>
#include <QMessageBox>

#include "ui_unlockwidget.h"
#include "spdlog/spdlog.h"

UnlockWidget::UnlockWidget(QWidget* parent) : QWidget(parent), ui(new Ui::UnlockWidget) {
    ui->setupUi(this);
    spdlog::debug("UnlockWidget constructed");
}

UnlockWidget::~UnlockWidget() {
    delete ui;
}

void UnlockWidget::on_btnConfirmUnlock_clicked() {
    QMessageBox::information(
        this,
        QCoreApplication::translate("UnlockWidget", "Unlock Successful"),
        QCoreApplication::translate("UnlockWidget", "The door has been remotely unlocked"));
    emit cancelUnlock();  // Back to main
}

void UnlockWidget::on_btnCancelUnlock_clicked() {
    emit cancelUnlock();
}
