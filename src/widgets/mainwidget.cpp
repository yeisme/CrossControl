#include "mainwidget.h"

#include "ui_mainwidget.h"
#include "weatherwidget.h"

/**
 * @brief Construct a new Main Widget:: Main Widget object
 *
 *
 * @param parent
 */
MainWidget::MainWidget(QWidget* parent) : QWidget(parent), ui(new Ui::MainWidget) {
    ui->setupUi(this);
    weatherWidget = new WeatherWidget(this);
    // 将天气组件添加到顶部布局中，替换原来的 labelWeather
    ui->verticalLayoutTop->replaceWidget(ui->labelWeather, weatherWidget);
    delete ui->labelWeather;  // 删除原来的标签

    // 作为 Dashboard 展示，隐藏功能网格区域（按钮已迁移到侧边栏）
    if (ui->scrollArea) ui->scrollArea->setVisible(false);
}

MainWidget::~MainWidget() {
    delete ui;
}
