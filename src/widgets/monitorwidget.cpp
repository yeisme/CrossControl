#include "monitorwidget.h"

#include "ui_monitorwidget.h"

MonitorWidget::MonitorWidget(QWidget* parent) : QWidget(parent), ui(new Ui::MonitorWidget) {
    ui->setupUi(this);
}

MonitorWidget::~MonitorWidget() {
    delete ui;
}

void MonitorWidget::on_btnBackFromMonitor_clicked() {
    emit backToMain();
}