#include "monitorwidget.h"

#include "ui_monitorwidget.h"
#include "spdlog/spdlog.h"

MonitorWidget::MonitorWidget(QWidget* parent) : QWidget(parent), ui(new Ui::MonitorWidget) {
    ui->setupUi(this);
    spdlog::debug("MonitorWidget constructed");
}

MonitorWidget::~MonitorWidget() {
    delete ui;
}

void MonitorWidget::on_btnBackFromMonitor_clicked() {
    emit backToMain();
}