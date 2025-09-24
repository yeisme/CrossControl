#include "settingwidget.h"

#include "ui_settingwidget.h"

SettingWidget::SettingWidget(QWidget* parent) : QWidget(parent), ui(new Ui::SettingWidget) { ui->setupUi(this); }

SettingWidget::~SettingWidget() { delete ui; }

void SettingWidget::on_btnBackFromSetting_clicked() { emit backToMain(); }
