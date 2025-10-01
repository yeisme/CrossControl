#include "settingwidget.h"

#include <qcoreapplication.h>

#include <QCheckBox>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QTimer>
#include <thread>
#include <QSpinBox>
#include <QTableWidget>

#include "logging/logging.h"
#include "modules/Config/config.h"
#include "modules/DeviceGateway/device_gateway.h"
#include "spdlog/spdlog.h"
#include "ui_settingwidget.h"

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

        // Storage controls: show current DB file and allow selecting new one
        auto* storageBtn = new QPushButton(
            QCoreApplication::translate("SettingWidget", "Configure Storage"), this);
        storageBtn->setObjectName("btnConfigureStorage");
        addSettingRow(QCoreApplication::translate("SettingWidget", "Storage"), storageBtn);
        connect(storageBtn,
                &QPushButton::clicked,
                this,
                &SettingWidget::on_btnConfigureStorage_clicked);

        // Accounts management button
        auto* accountsBtn =
            new QPushButton(QCoreApplication::translate("SettingWidget", "Manage Accounts"), this);
        accountsBtn->setObjectName("btnManageAccounts");
        addSettingRow(QCoreApplication::translate("SettingWidget", "Accounts"), accountsBtn);
        connect(
            accountsBtn, &QPushButton::clicked, this, &SettingWidget::on_btnManageAccounts_clicked);

        // Advanced: JSON editor for power users
        auto* jsonEditBtn = new QPushButton(
            QCoreApplication::translate("SettingWidget", "Advanced: Edit JSON"), this);
        jsonEditBtn->setObjectName("btnJsonEditor");
        addSettingRow(QCoreApplication::translate("SettingWidget", "Advanced"), jsonEditBtn);
        connect(jsonEditBtn, &QPushButton::clicked, this, &SettingWidget::on_btnJsonEditor_clicked);
    }
    spdlog::debug("SettingWidget initialized");

    // initialize REST controls (created but inactive until gateway set)
    initRestControls();

    // wire save/reload buttons
    auto* reloadBtn = findChild<QPushButton*>(QStringLiteral("btnReloadSettings"));
    auto* saveBtn = findChild<QPushButton*>(QStringLiteral("btnSaveSettings"));
    if (reloadBtn)
        connect(
            reloadBtn, &QPushButton::clicked, this, &SettingWidget::on_btnReloadSettings_clicked);
    if (saveBtn)
        connect(saveBtn, &QPushButton::clicked, this, &SettingWidget::on_btnSaveSettings_clicked);

    // prepare table
    auto* table = findChild<QTableWidget*>(QStringLiteral("tableSettings"));
    if (table) {
        table->setColumnCount(2);
        table->setHorizontalHeaderLabels({QCoreApplication::translate("SettingWidget", "Key"),
                                          QCoreApplication::translate("SettingWidget", "Value")});
        table->horizontalHeader()->setStretchLastSection(true);
        table->verticalHeader()->setVisible(false);
        table->setSelectionBehavior(QAbstractItemView::SelectRows);
        table->setEditTriggers(QAbstractItemView::DoubleClicked |
                               QAbstractItemView::SelectedClicked);
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

void SettingWidget::on_btnConfigureStorage_clicked() {
    auto& cfg = config::ConfigManager::instance();
    const QString current = cfg.getString("Storage/database", QString("crosscontrol.db"));
    QString file = QFileDialog::getSaveFileName(
        this,
        QCoreApplication::translate("SettingWidget", "Select database file"),
        QDir::current().filePath(current),
        QCoreApplication::translate("SettingWidget", "SQLite DB (*.db);;All Files (*)"));
    if (file.isEmpty()) return;
    cfg.setValue("Storage/database", file);
    cfg.sync();
    QMessageBox::information(
        this,
        QCoreApplication::translate("SettingWidget", "Storage"),
        QCoreApplication::translate("SettingWidget", "Storage database path updated."));
    populateSettingsTable();
}

void SettingWidget::initRestControls() {
    // create controls: port spinbox, start/stop button and small log viewer
    auto* portSpin = new QSpinBox(this);
    portSpin->setRange(1, 65535);
    portSpin->setValue(8080);
    portSpin->setObjectName("sbRestPort");

    auto* restBtn =
        new QPushButton(QCoreApplication::translate("SettingWidget", "Start REST"), this);
    restBtn->setObjectName("btnRestStartStop");

    // add rows into settings list: only port and control button. We do NOT create
    // a per-REST log widget here—the REST handlers write to the named logger
    // "REST" and LoggerManager ensures those messages appear in the main
    // LogWidget's Qt sink.
    addSettingRow(QCoreApplication::translate("SettingWidget", "REST Server Port"), portSpin);
    addSettingRow(QCoreApplication::translate("SettingWidget", "REST Control"), restBtn);

    // connect local signals
    connect(restBtn, &QPushButton::clicked, this, &SettingWidget::on_btnRestStartStop_clicked);
    connect(portSpin,
            qOverload<int>(&QSpinBox::valueChanged),
            this,
            &SettingWidget::on_restPortChanged);

    // restore saved config values
    auto& cfg = config::ConfigManager::instance();
    const int port = cfg.getInt("REST/port", 8080);
    portSpin->setValue(port);
    const bool enabled = cfg.getBool("REST/enabled", false);
    // if enabled, we will not start automatically here unless gateway is set; just update button
    // text
    if (enabled) restBtn->setText(QCoreApplication::translate("SettingWidget", "Stop REST"));

    // No per-REST sinks or duplication UI: REST logs are handled by the named
    // logger "REST" and will appear in the main LogWidget via LoggerManager.
}

void SettingWidget::on_btnRestStartStop_clicked() {
    auto* restBtn = findChild<QPushButton*>(QStringLiteral("btnRestStartStop"));
    auto* portSpin = findChild<QSpinBox*>(QStringLiteral("sbRestPort"));
    if (!restBtn || !portSpin) return;
    if (!gateway_) {
        QMessageBox::warning(
            this,
            QCoreApplication::translate("SettingWidget", "REST"),
            QCoreApplication::translate("SettingWidget", "Device gateway not available"));
        return;
    }

    // If REST is running, request stop in a background thread and disable the
    // button until the server has fully stopped. Poll isRestRunning() via QTimer
    // so the UI remains responsive.
    if (gateway_->isRestRunning()) {
        restBtn->setEnabled(false);
        restBtn->setText(QCoreApplication::translate("SettingWidget", "Stopping..."));

        // Dispatch stop in background so UI thread is not blocked
        std::thread([this]() {
            if (gateway_) gateway_->stopRest();
        }).detach();

        // Poll for shutdown completion and re-enable the button when done
        QTimer* poll = new QTimer(this);
        poll->setInterval(250);
        connect(poll, &QTimer::timeout, this, [this, poll, restBtn]() {
            if (!gateway_ || !gateway_->isRestRunning()) {
                poll->stop();
                poll->deleteLater();
                restBtn->setText(QCoreApplication::translate("SettingWidget", "Start REST"));
                restBtn->setEnabled(true);
                config::ConfigManager::instance().setValue("REST/enabled", false);
                config::ConfigManager::instance().sync();
            }
        });
        poll->start();
    } else {
        const int port = portSpin->value();
        // persist port
        config::ConfigManager::instance().setValue("REST/port", port);
        config::ConfigManager::instance().setValue("REST/enabled", true);
        config::ConfigManager::instance().sync();

        // attempt to start/init gateway
        bool ok = gateway_->init();
        if (ok) {
            restBtn->setText(QCoreApplication::translate("SettingWidget", "Stop REST"));
        } else {
            // Starting may fail if Drogon was previously started in this process
            QMessageBox::warning(
                this,
                QCoreApplication::translate("SettingWidget", "REST"),
                QCoreApplication::translate(
                    "SettingWidget",
                    "Failed to start REST server. If the embedded REST server was already started in this process it cannot be restarted; please restart the application."));
            // Ensure persisted flag reflects actual running state
            config::ConfigManager::instance().setValue("REST/enabled", false);
            config::ConfigManager::instance().sync();
            restBtn->setText(QCoreApplication::translate("SettingWidget", "Start REST"));
        }
    }
}

void SettingWidget::on_restPortChanged(int port) {
    // persist change immediately
    config::ConfigManager::instance().setValue("REST/port", port);
    config::ConfigManager::instance().sync();
}

void SettingWidget::updateRestControls() {
    auto* restBtn = findChild<QPushButton*>(QStringLiteral("btnRestStartStop"));
    if (!restBtn) return;
    if (gateway_ && gateway_->isRestRunning()) {
        restBtn->setText(QCoreApplication::translate("SettingWidget", "Stop REST"));
    } else {
        restBtn->setText(QCoreApplication::translate("SettingWidget", "Start REST"));
    }
}

// Optional binder for gateway so main window can wire the gateway instance
void SettingWidget::bindDeviceGateway(DeviceGateway::DeviceGateway* gw) {
    gateway_ = gw;
    updateRestControls();
    // if config says REST should be enabled, try to start it on configured port
    auto& cfg = config::ConfigManager::instance();
    const bool shouldEnable = cfg.getBool("REST/enabled", false);
    const int port = cfg.getInt("REST/port", 8080);
    if (shouldEnable && gateway_ && !gateway_->isRestRunning()) {
        const bool ok = gateway_->startRest(static_cast<unsigned short>(port));
        if (!ok) {
            QMessageBox::warning(
                this,
                QCoreApplication::translate("SettingWidget", "REST"),
                QCoreApplication::translate("SettingWidget",
                                            "Failed to auto-start REST server on saved port"));
            cfg.setValue("REST/enabled", false);
            cfg.sync();
        }
        updateRestControls();
    }
}

void SettingWidget::on_btnManageAccounts_clicked() {
    // For now, manage simple saved Auth keys under Auth/*
    auto& cfg = config::ConfigManager::instance();
    QStringList keys = cfg.allKeys();
    QStringList authKeys;
    for (const QString& k : keys)
        if (k.startsWith("Auth/")) authKeys << k;

    if (authKeys.empty()) {
        QMessageBox::information(
            this,
            QCoreApplication::translate("SettingWidget", "Accounts"),
            QCoreApplication::translate("SettingWidget", "No saved accounts found."));
        return;
    }

    // Build a simple dialog to list keys and allow removal
    QDialog dlg(this);
    dlg.setWindowTitle(QCoreApplication::translate("SettingWidget", "Manage Accounts"));
    QVBoxLayout* layout = new QVBoxLayout(&dlg);
    QListWidget* list = new QListWidget(&dlg);
    for (const QString& k : authKeys) {
        QString display = k + " = " + cfg.getString(k, QString());
        list->addItem(display);
    }
    layout->addWidget(list);
    QHBoxLayout* h = new QHBoxLayout();
    QPushButton* btnRemove =
        new QPushButton(QCoreApplication::translate("SettingWidget", "Remove Selected"), &dlg);
    QPushButton* btnClose =
        new QPushButton(QCoreApplication::translate("SettingWidget", "Close"), &dlg);
    h->addWidget(btnRemove);
    h->addStretch(1);
    h->addWidget(btnClose);
    layout->addLayout(h);

    connect(btnClose, &QPushButton::clicked, &dlg, &QDialog::accept);
    connect(btnRemove, &QPushButton::clicked, [&]() {
        auto items = list->selectedItems();
        if (items.isEmpty()) return;
        for (QListWidgetItem* it : items) {
            const QString text = it->text();
            const QString key = text.section(' ', 0, 0);
            cfg.remove(key);
            delete it;
        }
        cfg.sync();
    });

    dlg.exec();
    populateSettingsTable();
}

void SettingWidget::on_btnJsonEditor_clicked() {
    auto& cfg = config::ConfigManager::instance();
    const QString currentJson = cfg.toJsonString();
    QDialog dlg(this);
    dlg.setWindowTitle(QCoreApplication::translate("SettingWidget", "Edit Configuration JSON"));
    QVBoxLayout* layout = new QVBoxLayout(&dlg);
    QTextEdit* editor = new QTextEdit(&dlg);
    editor->setPlainText(currentJson);
    layout->addWidget(editor);
    QHBoxLayout* h = new QHBoxLayout();
    QPushButton* btnOk =
        new QPushButton(QCoreApplication::translate("SettingWidget", "Apply"), &dlg);
    QPushButton* btnCancel =
        new QPushButton(QCoreApplication::translate("SettingWidget", "Cancel"), &dlg);
    h->addWidget(btnOk);
    h->addStretch(1);
    h->addWidget(btnCancel);
    layout->addLayout(h);

    connect(btnCancel, &QPushButton::clicked, &dlg, &QDialog::reject);
    connect(btnOk, &QPushButton::clicked, [&]() {
        const QString newJson = editor->toPlainText();
        if (!cfg.loadFromJsonString(newJson, true)) {
            QMessageBox::warning(
                &dlg,
                QCoreApplication::translate("SettingWidget", "JSON Error"),
                QCoreApplication::translate("SettingWidget",
                                            "Failed to parse provided JSON. No changes applied."));
            return;
        }
        cfg.sync();
        dlg.accept();
    });

    if (dlg.exec() == QDialog::Accepted) {
        QMessageBox::information(
            this,
            QCoreApplication::translate("SettingWidget", "JSON"),
            QCoreApplication::translate("SettingWidget", "Configuration updated from JSON."));
        populateSettingsTable();
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
            spdlog::error("Failed to persist key {} (expected '{}' got '{}')",
                          key.toStdString(),
                          expected.toStdString(),
                          actual.toStdString());
            allOk = false;
        }
    }
    if (!allOk) {
        QMessageBox::warning(
            this,
            QCoreApplication::translate("SettingWidget", "Save Failed"),
            QCoreApplication::translate(
                "SettingWidget",
                "Failed to save some configuration keys. Check permissions or file locks."));
    } else {
        QMessageBox::information(
            this,
            QCoreApplication::translate("SettingWidget", "Saved"),
            QCoreApplication::translate("SettingWidget", "Settings saved successfully."));
    }
}
