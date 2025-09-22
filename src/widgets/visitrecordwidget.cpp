#include "visitrecordwidget.h"
#include "ui_visitrecordwidget.h"

VisitRecordWidget::VisitRecordWidget(QWidget *parent) : QWidget(parent), ui(new Ui::VisitRecordWidget) {
    ui->setupUi(this);
}

VisitRecordWidget::~VisitRecordWidget() {
    delete ui;
}

void VisitRecordWidget::on_btnBackFromVisit_clicked() {
    emit backToMain();
}