#include "visitrecordwidget.h"

#include "spdlog/spdlog.h"
#include "ui_visitrecordwidget.h"

VisitRecordWidget::VisitRecordWidget(QWidget* parent)
    : QWidget(parent), ui(new Ui::VisitRecordWidget) {
    ui->setupUi(this);
    spdlog::debug("VisitRecordWidget constructed");
}

VisitRecordWidget::~VisitRecordWidget() {
    delete ui;
}

void VisitRecordWidget::on_btnBackFromVisit_clicked() {
    emit backToMain();
}