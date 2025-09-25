#include "settingwidget.h"

#include <qcoreapplication.h>

#include "ui_settingwidget.h"
#include "spdlog/spdlog.h"

SettingWidget::SettingWidget(QWidget* parent) : QWidget(parent), ui(new Ui::SettingWidget) {
    ui->setupUi(this);
    // 找到设置列表容器布局
    auto* listLayout = findChild<QVBoxLayout*>(QStringLiteral("layoutSettingsList"));
    if (listLayout) {
        // 主题切换按钮
        auto* toggleBtn = new QPushButton(
            QCoreApplication::translate("SettingWidget", "Toggle Light/Dark Theme"), this);
        toggleBtn->setObjectName("btnToggleTheme");
        addSettingRow(QCoreApplication::translate("SettingWidget", "Theme"), toggleBtn);
        connect(toggleBtn, &QPushButton::clicked, this, &SettingWidget::on_btnToggleTheme_clicked);

        auto* placeholder1 = new QPushButton(
            QCoreApplication::translate("SettingWidget", "Configure Storage"), this);
        placeholder1->setEnabled(false);
        addSettingRow(QCoreApplication::translate("SettingWidget", "Storage"), placeholder1);

        auto* placeholder2 =
            new QPushButton(QCoreApplication::translate("SettingWidget", "Manage Accounts"), this);
        placeholder2->setEnabled(false);
        addSettingRow(QCoreApplication::translate("SettingWidget", "Accounts"), placeholder2);
    }
    spdlog::debug("SettingWidget initialized");
}

SettingWidget::~SettingWidget() {
    delete ui;
}

void SettingWidget::on_btnBackFromSetting_clicked() {
    emit backToMain();
}

void SettingWidget::on_btnToggleTheme_clicked() {
    emit toggleThemeRequested();
}

QWidget* SettingWidget::addSettingRow(const QString& title, QWidget* control) {
    auto* listLayout = findChild<QVBoxLayout*>(QStringLiteral("layoutSettingsList"));
    if (!listLayout) return nullptr;
    auto* row = new QWidget(this);
    row->setObjectName("settingRow");
    auto* h = new QHBoxLayout(row);
    h->setContentsMargins(12, 4, 12, 4);
    h->setSpacing(16);
    auto* lbl = new QLabel(title, row);
    lbl->setObjectName("settingLabel");
    lbl->setMinimumWidth(160);
    h->addWidget(lbl);
    h->addStretch(1);
    if (control) {
        control->setMinimumWidth(180);
        h->addWidget(control, 0, Qt::AlignRight);
    }
    listLayout->addWidget(row);
    // 分隔线
    auto* line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    line->setObjectName("settingDivider");
    listLayout->addWidget(line);
    return row;
}
