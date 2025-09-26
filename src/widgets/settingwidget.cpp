#include "settingwidget.h"

#include <qcoreapplication.h>

#include "spdlog/spdlog.h"
#include "ui_settingwidget.h"
#include "modules/Config/config.h"
#include <QTableWidget>
#include <QHeaderView>
#include <QMessageBox>


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

    // wire save/reload buttons
    auto* reloadBtn = findChild<QPushButton*>(QStringLiteral("btnReloadSettings"));
    auto* saveBtn = findChild<QPushButton*>(QStringLiteral("btnSaveSettings"));
    if (reloadBtn) connect(reloadBtn, &QPushButton::clicked, this, &SettingWidget::on_btnReloadSettings_clicked);
    if (saveBtn) connect(saveBtn, &QPushButton::clicked, this, &SettingWidget::on_btnSaveSettings_clicked);

    // prepare table
    auto* table = findChild<QTableWidget*>(QStringLiteral("tableSettings"));
    if (table) {
        table->setColumnCount(2);
        table->setHorizontalHeaderLabels({tr("Key"), tr("Value")});
        table->horizontalHeader()->setStretchLastSection(true);
        table->verticalHeader()->setVisible(false);
        table->setSelectionBehavior(QAbstractItemView::SelectRows);
        table->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::SelectedClicked);
    }

    // initial population
    populateSettingsTable();
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

void SettingWidget::populateSettingsTable() {
    auto* table = findChild<QTableWidget*>(QStringLiteral("tableSettings"));
    if (!table) return;
    table->setRowCount(0);
    auto& cfg = config::ConfigManager::instance();
    const QStringList keys = cfg.allKeys();
    int r = 0;
    for (const QString& k : keys) {
        table->insertRow(r);
        auto* itemKey = new QTableWidgetItem(k);
        itemKey->setFlags(itemKey->flags() & ~Qt::ItemIsEditable);
        table->setItem(r, 0, itemKey);
        QVariant v = cfg.getValue(k, QVariant());
        QString s = v.toString();
        auto* itemVal = new QTableWidgetItem(s);
        table->setItem(r, 1, itemVal);
        ++r;
    }
}

void SettingWidget::on_btnReloadSettings_clicked() {
    populateSettingsTable();
}

void SettingWidget::on_btnSaveSettings_clicked() {
    auto* table = findChild<QTableWidget*>(QStringLiteral("tableSettings"));
    if (!table) return;
    auto& cfg = config::ConfigManager::instance();
    bool allOk = true;
    // write all rows to config
    for (int r = 0; r < table->rowCount(); ++r) {
        QTableWidgetItem* keyIt = table->item(r, 0);
        QTableWidgetItem* valIt = table->item(r, 1);
        if (!keyIt || !valIt) continue;
        const QString key = keyIt->text();
        const QString val = valIt->text();
        // attempt to write
        cfg.setValue(key, val);
    }
    // force sync and verify by reloading keys
    cfg.sync();
    // verify every key we wrote matches
    for (int r = 0; r < table->rowCount(); ++r) {
        QTableWidgetItem* keyIt = table->item(r, 0);
        QTableWidgetItem* valIt = table->item(r, 1);
        if (!keyIt || !valIt) continue;
        const QString key = keyIt->text();
        const QString expected = valIt->text();
        const QString actual = cfg.getString(key, QString());
        if (actual != expected) {
            spdlog::error("Failed to persist key {} (expected '{}' got '{}')", key.toStdString(), expected.toStdString(), actual.toStdString());
            allOk = false;
        }
    }
    if (!allOk) {
        QMessageBox::warning(this, tr("Save Failed"), tr("Failed to save some configuration keys. Check permissions or file locks."));
    } else {
        QMessageBox::information(this, tr("Saved"), tr("Settings saved successfully."));
    }
}
