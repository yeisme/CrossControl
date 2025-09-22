#include "mainwidget.h"

#include "ui_mainwidget.h"
#include "weatherwidget.h"

MainWidget::MainWidget(QWidget *parent)
    : QWidget(parent), ui(new Ui::MainWidget) {
    ui->setupUi(this);
    weatherWidget = new WeatherWidget(this);
    // 将天气组件添加到顶部布局中，替换原来的 labelWeather
    ui->verticalLayoutTop->replaceWidget(ui->labelWeather, weatherWidget);
    delete ui->labelWeather;  // 删除原来的标签
}

MainWidget::~MainWidget() { delete ui; }

void MainWidget::on_btnMonitor_clicked() { emit showMonitor(); }

void MainWidget::on_btnUnlock_clicked() { emit showUnlock(); }

void MainWidget::on_btnVisitRecord_clicked() { emit showVisitRecord(); }

void MainWidget::on_btnMessage_clicked() { emit showMessage(); }

void MainWidget::on_btnSetting_clicked() { emit showSetting(); }

void MainWidget::on_btnLogout_clicked() { emit logout(); }
