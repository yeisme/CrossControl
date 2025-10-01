#include "widgets/devicemanagementwidget.h"

#include <qcoreapplication.h>

#include <QDateTime>
#include <QDebug>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QSize>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QStandardPaths>
#include <QTableWidgetItem>
#include <QTextStream>
#include <QTimer>
#include <thread>

#include "modules/Connect/iface_connect.h"
#include "modules/DeviceGateway/device_gateway.h"
#include "modules/DeviceGateway/device_registry.h"
#include "modules/DeviceGateway/rest_server.h"
#include "widgets/importexportcontrol.h"

DeviceManagementWidget::DeviceManagementWidget(DeviceGateway::DeviceGateway* gateway,
                                               QWidget* parent)
    : QWidget(parent), ui(new Ui::DeviceManagementWidget), gateway_(gateway) {
    ui->setupUi(this);

    // configure table columns (add an Icon column at index 0)
    ui->tableDevices->setColumnCount(9);
    ui->tableDevices->setHorizontalHeaderLabels(
        {QCoreApplication::translate("DeviceManagementWidget", "Icon"),
         QCoreApplication::translate("DeviceManagementWidget", "Device ID"),
         QCoreApplication::translate("DeviceManagementWidget", "HW Info"),
         QCoreApplication::translate("DeviceManagementWidget", "Endpoint"),
         QCoreApplication::translate("DeviceManagementWidget", "Firmware"),
         QCoreApplication::translate("DeviceManagementWidget", "Owner"),
         QCoreApplication::translate("DeviceManagementWidget", "Group"),
         QCoreApplication::translate("DeviceManagementWidget", "Last Seen"),
         QCoreApplication::translate("DeviceManagementWidget", "Test")});
    ui->tableDevices->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableDevices->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->tableDevices->setEditTriggers(QAbstractItemView::DoubleClicked |
                                      QAbstractItemView::SelectedClicked);

    // Refresh helper
    auto refresh = [this]() {
        ui->tableDevices->setRowCount(0);
        if (!gateway_ || !gateway_->registry()) return;
        const auto list = gateway_->registry()->list();
        int r = 0;
        for (const auto& d : list) {
            ui->tableDevices->insertRow(r);
            // icon cell
            QTableWidgetItem* iconItem = new QTableWidgetItem();
            if (!d.iconPath.isEmpty()) iconItem->setIcon(QIcon(d.iconPath));
            iconItem->setFlags(iconItem->flags() & ~Qt::ItemIsEditable);
            ui->tableDevices->setItem(r, 0, iconItem);

            ui->tableDevices->setItem(r, 1, new QTableWidgetItem(d.deviceId));
            ui->tableDevices->setItem(r, 2, new QTableWidgetItem(d.hwInfo));
            ui->tableDevices->setItem(r, 3, new QTableWidgetItem(d.endpoint));
            ui->tableDevices->setItem(r, 4, new QTableWidgetItem(d.firmwareVersion));
            ui->tableDevices->setItem(r, 5, new QTableWidgetItem(d.owner));
            ui->tableDevices->setItem(r, 6, new QTableWidgetItem(d.group));
            ui->tableDevices->setItem(r, 7, new QTableWidgetItem(d.lastSeen.toString(Qt::ISODate)));
            // add Test + Control buttons (in same cell)
            QWidget* btnContainer = new QWidget();
            QHBoxLayout* hlay = new QHBoxLayout(btnContainer);
            hlay->setContentsMargins(0, 0, 0, 0);
            QPushButton* btnTest = new QPushButton(
                QCoreApplication::translate("DeviceManagementWidget", "Test"), btnContainer);
            QPushButton* btnControl = new QPushButton(
                QCoreApplication::translate("DeviceManagementWidget", "Control"), btnContainer);
            btnTest->setProperty("deviceId", d.deviceId);
            btnControl->setProperty("deviceId", d.deviceId);
            btnTest->setCursor(Qt::PointingHandCursor);
            btnControl->setCursor(Qt::PointingHandCursor);
            btnTest->setFixedHeight(22);
            btnControl->setFixedHeight(22);
            hlay->addWidget(btnTest);
            hlay->addWidget(btnControl);
            hlay->addStretch(1);
            ui->tableDevices->setCellWidget(r, 8, btnContainer);

            // connect Test button click -> run in background to avoid blocking UI
            connect(btnTest, &QPushButton::clicked, this, [this, d]() {
                const QString endpoint =
                    (!d.endpoint.trimmed().isEmpty()) ? d.endpoint.trimmed() : d.hwInfo.trimmed();
                if (endpoint.isEmpty()) {
                    QMetaObject::invokeMethod(
                        this,
                        [this, id = d.deviceId]() {
                            QMessageBox::information(
                                this,
                                QCoreApplication::translate("DeviceManagementWidget", "Test"),
                                QCoreApplication::translate("DeviceManagementWidget",
                                                            "No endpoint configured for %1")
                                    .arg(id));
                        },
                        Qt::QueuedConnection);
                    return;
                }
                // run network I/O on a background thread
                std::thread([this, endpoint, id = d.deviceId]() {
                    IConnectPtr conn = IConnect::createFromEndpoint(endpoint);
                    if (!conn) conn = IConnect::createDefault();
                    bool ok = false;
                    try {
                        ok = conn->open(endpoint);
                    } catch (...) { ok = false; }
                    if (ok) conn->close();
                    QMetaObject::invokeMethod(
                        this,
                        [this, ok, id]() {
                            QMessageBox::information(
                                this,
                                QCoreApplication::translate("DeviceManagementWidget",
                                                            "Test Result"),
                                ok ? QCoreApplication::translate("DeviceManagementWidget",
                                                                 "Device %1 reachable")
                                         .arg(id)
                                   : QCoreApplication::translate("DeviceManagementWidget",
                                                                 "Device %1 unreachable")
                                         .arg(id));
                        },
                        Qt::QueuedConnection);
                }).detach();
            });

            // connect Control button click -> prompt for command and send in background
            connect(btnControl, &QPushButton::clicked, this, [this, d]() {
                const QString endpoint =
                    (!d.endpoint.trimmed().isEmpty()) ? d.endpoint.trimmed() : d.hwInfo.trimmed();
                if (endpoint.isEmpty()) {
                    QMessageBox::information(
                        this,
                        QCoreApplication::translate("DeviceManagementWidget", "Control"),
                        QCoreApplication::translate("DeviceManagementWidget",
                                                    "No endpoint configured for %1")
                            .arg(d.deviceId));
                    return;
                }
                bool ok;
                const QString cmd = QInputDialog::getText(
                    this,
                    QCoreApplication::translate("DeviceManagementWidget", "Control Device"),
                    QCoreApplication::translate("DeviceManagementWidget",
                                                "Command (text or JSON):"),
                    QLineEdit::Normal,
                    QString(),
                    &ok);
                if (!ok || cmd.isEmpty()) return;

                // send command on background thread
                std::thread([this, endpoint, id = d.deviceId, cmd]() {
                    IConnectPtr conn = IConnect::createFromEndpoint(endpoint);
                    if (!conn) conn = IConnect::createDefault();
                    bool opened = false;
                    qint64 sent = -1;
                    QString reply;
                    try {
                        opened = conn->open(endpoint);
                        if (opened) {
                            sent = conn->send(cmd.toUtf8());
                            // try to read an immediate reply (non-blocking semantics assumed)
                            QByteArray out;
                            qint64 r = conn->receive(out, 4096);
                            if (r > 0) reply = QString::fromUtf8(out);
                            conn->close();
                        }
                    } catch (...) { opened = false; }
                    QMetaObject::invokeMethod(
                        this,
                        [this, opened, sent, id, reply]() {
                            if (!opened) {
                                QMessageBox::warning(
                                    this,
                                    QCoreApplication::translate("DeviceManagementWidget",
                                                                "Control"),
                                    QCoreApplication::translate("DeviceManagementWidget",
                                                                "Failed to open connection to %1")
                                        .arg(id));
                                return;
                            }
                            if (sent < 0) {
                                QMessageBox::warning(
                                    this,
                                    QCoreApplication::translate("DeviceManagementWidget",
                                                                "Control"),
                                    QCoreApplication::translate("DeviceManagementWidget",
                                                                "Failed to send command to %1")
                                        .arg(id));
                                return;
                            }
                            if (!reply.isEmpty()) {
                                QMessageBox::information(
                                    this,
                                    QCoreApplication::translate("DeviceManagementWidget",
                                                                "Control"),
                                    QCoreApplication::translate("DeviceManagementWidget",
                                                                "Sent %1 bytes to %2. Reply: %3")
                                        .arg(sent)
                                        .arg(id)
                                        .arg(reply));
                            } else {
                                QMessageBox::information(
                                    this,
                                    QCoreApplication::translate("DeviceManagementWidget",
                                                                "Control"),
                                    QCoreApplication::translate(
                                        "DeviceManagementWidget",
                                        "Sent %1 bytes to %2. No immediate reply")
                                        .arg(sent)
                                        .arg(id));
                            }
                        },
                        Qt::QueuedConnection);
                }).detach();
            });
            ++r;
        }
    };

    // connect refresh button
    connect(ui->btnRefresh, &QPushButton::clicked, this, refresh);

    // double-click editing: when a cell is edited, commit back to registry on editingFinished
    connect(ui->tableDevices, &QTableWidget::itemChanged, this, [this](QTableWidgetItem* item) {
        if (!gateway_ || !gateway_->registry() || !item) return;
        int row = item->row();
        // deviceId is now column 1
        QTableWidgetItem* idItem = ui->tableDevices->item(row, 1);
        if (!idItem) return;
        QString id = idItem->text();
        if (id.isEmpty()) return;
        DeviceGateway::DeviceInfo d;
        d.deviceId = id;
        d.hwInfo = ui->tableDevices->item(row, 2)->text();
        d.endpoint = ui->tableDevices->item(row, 3)->text();
        d.firmwareVersion = ui->tableDevices->item(row, 4)->text();
        d.owner = ui->tableDevices->item(row, 5)->text();
        d.group = ui->tableDevices->item(row, 6)->text();
        // lastSeen is column 6 and not editable here
        // keep existing iconPath from registry if present
        auto maybe = gateway_->registry()->get(id);
        if (maybe) d.iconPath = maybe->iconPath;
        gateway_->registry()->upsert(d);
    });

    // Add device dialog
    // new buttons may not be present in generated ui header if uic wasn't rerun
    // so find them dynamically to avoid build-time dependency on regenerated ui header
    if (auto addBtn = findChild<QPushButton*>("btnAddDevice")) {
        // use device management icon on add button
        addBtn->setIcon(QIcon(":/icons/monitor.svg"));
        addBtn->setIconSize(QSize(16, 16));

        connect(addBtn, &QPushButton::clicked, this, [this, refresh]() {
            // build a small dialog with form fields so user can provide more metadata
            QDialog dlg(this);
            dlg.setWindowTitle(QCoreApplication::translate("DeviceManagementWidget", "Add Device"));
            QFormLayout* form = new QFormLayout(&dlg);

            QLineEdit* edtId = new QLineEdit(&dlg);
            QLineEdit* edtHw = new QLineEdit(&dlg);
            QLineEdit* edtEndpoint = new QLineEdit(&dlg);
            QLineEdit* edtFw = new QLineEdit(&dlg);
            QLineEdit* edtOwner = new QLineEdit(&dlg);
            QLineEdit* edtGroup = new QLineEdit(&dlg);

            // icon chooser (optional, not persisted to registry currently)
            QPushButton* btnChooseIcon = new QPushButton(
                QCoreApplication::translate("DeviceManagementWidget", "Choose Icon..."), &dlg);
            QLabel* lblIconPreview = new QLabel(&dlg);
            lblIconPreview->setFixedSize(32, 32);
            lblIconPreview->setScaledContents(true);
            QString chosenIconPath;

            connect(
                btnChooseIcon,
                &QPushButton::clicked,
                &dlg,
                [&dlg, &chosenIconPath, lblIconPreview]() {
                    const QString f = QFileDialog::getOpenFileName(
                        &dlg,
                        QCoreApplication::translate("DeviceManagementWidget", "Choose device icon"),
                        {},
                        QCoreApplication::translate("DeviceManagementWidget",
                                                    "Images (*.png *.svg *.ico);;All Files (*)"));
                    if (f.isEmpty()) return;
                    chosenIconPath = f;
                    QPixmap pm(f);
                    if (!pm.isNull())
                        lblIconPreview->setPixmap(pm.scaled(
                            lblIconPreview->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
                });

            QWidget* iconRow = new QWidget(&dlg);
            QHBoxLayout* h = new QHBoxLayout(iconRow);
            h->setContentsMargins(0, 0, 0, 0);
            h->addWidget(btnChooseIcon);
            h->addWidget(lblIconPreview);

            form->addRow(QCoreApplication::translate("DeviceManagementWidget", "Device ID:"),
                         edtId);
            form->addRow(QCoreApplication::translate("DeviceManagementWidget", "HW Info:"), edtHw);
            form->addRow(QCoreApplication::translate("DeviceManagementWidget", "Endpoint:"),
                         edtEndpoint);
            form->addRow(QCoreApplication::translate("DeviceManagementWidget", "Firmware:"), edtFw);
            form->addRow(QCoreApplication::translate("DeviceManagementWidget", "Owner:"), edtOwner);
            form->addRow(QCoreApplication::translate("DeviceManagementWidget", "Group:"), edtGroup);
            form->addRow(QCoreApplication::translate("DeviceManagementWidget", "Icon:"), iconRow);

            QDialogButtonBox* bb =
                new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
            form->addRow(bb);
            connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
            connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

            if (dlg.exec() == QDialog::Accepted) {
                const QString id = edtId->text().trimmed();
                if (id.isEmpty()) {
                    QMessageBox::warning(
                        this,
                        QCoreApplication::translate("DeviceManagementWidget", "Add Device"),
                        QCoreApplication::translate("DeviceManagementWidget",
                                                    "Device ID is required"));
                    return;
                }
                DeviceGateway::DeviceInfo d;
                d.deviceId = id;
                d.hwInfo = edtHw->text().trimmed();
                d.endpoint = edtEndpoint->text().trimmed();
                d.firmwareVersion = edtFw->text().trimmed();
                d.owner = edtOwner->text().trimmed();
                d.group = edtGroup->text().trimmed();
                d.lastSeen = QDateTime::currentDateTime();
                if (!chosenIconPath.isEmpty()) {
                    // copy chosen icon to app data device_icons folder
                    const QString destDir =
                        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
                    QDir dd(destDir);
                    if (!dd.exists()) dd.mkpath(".");
                    const QString iconsDir = dd.filePath("device_icons");
                    QDir idd(iconsDir);
                    if (!idd.exists()) idd.mkpath(".");
                    const QString baseName = QFileInfo(chosenIconPath).fileName();
                    QString outPath = idd.filePath(baseName);
                    // if file exists, generate unique name
                    int i = 1;
                    while (QFile::exists(outPath)) {
                        outPath = idd.filePath(QStringLiteral("%1_%2.%3")
                                                   .arg(QFileInfo(baseName).baseName())
                                                   .arg(i)
                                                   .arg(QFileInfo(baseName).suffix()));
                        ++i;
                    }
                    if (QFile::copy(chosenIconPath, outPath)) { d.iconPath = outPath; }
                }
                if (gateway_ && gateway_->registry()) { gateway_->registry()->upsert(d); }
                refresh();
            }
        });
    }

    // Delete selected device
    if (auto delBtn = findChild<QPushButton*>("btnDeleteDevice")) {
        connect(delBtn, &QPushButton::clicked, this, [this, refresh]() {
            if (!gateway_ || !gateway_->registry()) return;
            auto sel = ui->tableDevices->selectedItems();
            if (sel.isEmpty()) {
                QMessageBox::information(this, "Delete", "No device selected");
                return;
            }
            int row = sel.first()->row();
            QString id = ui->tableDevices->item(row, 1)->text();
            if (id.isEmpty()) return;
            const auto ret =
                QMessageBox::question(this, "Delete", QString("Delete device %1?").arg(id));
            if (ret == QMessageBox::Yes) {
                gateway_->registry()->remove(id);
                refresh();
            }
        });
    }

    // export/import: use centralized ImportExportControl when available
    ImportExportControl* iec = new ImportExportControl(this);
    // attempt to place new control into bottom layout near other controls
    if (auto bottom = findChild<QHBoxLayout*>("bottomLayout")) {
        // insert before horizontal spacer if possible
        // create a small container widget to host vertical buttons
        QWidget* host = new QWidget(this);
        QVBoxLayout* vl = new QVBoxLayout(host);
        vl->setContentsMargins(0, 0, 0, 0);
        vl->addWidget(iec);
        int insertIndex = -1;
        // try to find spacer index by searching children
        // best-effort: add widget to layout
        bottom->addWidget(host);
    } else {
        // fallback: add to main layout
        this->layout()->addWidget(iec);
    }

    // hide legacy buttons if exist (they may come from different ui variants)
    if (auto w = findChild<QPushButton*>("btnExportDb")) w->hide();
    if (auto w = findChild<QPushButton*>("btnImportDb")) w->hide();
    if (auto w = findChild<QPushButton*>("btnExportCsv")) w->hide();
    if (auto w = findChild<QPushButton*>("btnImportCsv")) w->hide();
    if (auto w = findChild<QPushButton*>("btnExportNd")) w->hide();
    if (auto w = findChild<QPushButton*>("btnImportNd")) w->hide();

    connect(iec, &ImportExportControl::exportRequested, this, [this](const QString& method) {
        if (!gateway_ || !gateway_->registry()) {
            QMessageBox::warning(this, "Export", "Device registry not available");
            return;
        }
        if (method.contains("SQLite", Qt::CaseInsensitive)) {
            QString file = QFileDialog::getSaveFileName(
                this,
                QCoreApplication::translate("DeviceManagementWidget", "Export devices DB"),
                {},
                QCoreApplication::translate("DeviceManagementWidget",
                                            "SQLite DB (*.db);;All Files (*)"));
            if (file.isEmpty()) return;
            const int written = gateway_->registry()->exportToDatabaseFile(file);
            if (written < 0)
                QMessageBox::critical(this, "Export", "Failed to export devices to file");
            else
                QMessageBox::information(this,
                                         "Export",
                                         QCoreApplication::translate("DeviceManagementWidget",
                                                                     "Exported %1 devices to %2")
                                             .arg(written)
                                             .arg(file));
        } else if (method.contains("CSV", Qt::CaseInsensitive)) {
            QString file = QFileDialog::getSaveFileName(
                this,
                QCoreApplication::translate("DeviceManagementWidget", "Export CSV"),
                {},
                QCoreApplication::translate("DeviceManagementWidget",
                                            "CSV Files (*.csv);;All Files (*)"));
            if (file.isEmpty()) return;
            const QString csv = gateway_->registry()->exportCsv();
            QFile f(file);
            if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                QMessageBox::critical(this, "Export CSV", "Failed to open file for writing");
                return;
            }
            QTextStream ts(&f);
            ts << csv;
            f.close();
            QMessageBox::information(this, "Export CSV", "Exported devices to CSV");
        } else if (method.contains("JSONND", Qt::CaseInsensitive) ||
                   method.contains("NDJSON", Qt::CaseInsensitive)) {
            QString file = QFileDialog::getSaveFileName(
                this,
                QCoreApplication::translate("DeviceManagementWidget", "Export JSONND"),
                {},
                QCoreApplication::translate("DeviceManagementWidget",
                                            "JSONND Files (*.ndjson *.jsonnd);;All Files (*)"));
            if (file.isEmpty()) return;
            const QString nd = gateway_->registry()->exportJsonNd();
            QFile f(file);
            if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                QMessageBox::critical(this, "Export JSONND", "Failed to open file for writing");
                return;
            }
            QTextStream ts(&f);
            ts << nd;
            f.close();
            QMessageBox::information(this, "Export JSONND", "Exported devices to JSONND");
        }
    });

    connect(
        iec, &ImportExportControl::importRequested, this, [this, refresh](const QString& method) {
            if (!gateway_ || !gateway_->registry()) {
                QMessageBox::warning(this, "Import", "Device registry not available");
                return;
            }
            if (method.contains("SQLite", Qt::CaseInsensitive)) {
                QString file = QFileDialog::getOpenFileName(
                    this,
                    QCoreApplication::translate("DeviceManagementWidget", "Import devices DB"),
                    {},
                    QCoreApplication::translate("DeviceManagementWidget",
                                                "SQLite DB (*.db);;All Files (*)"));
                if (file.isEmpty()) return;
                const int imported = gateway_->registry()->importFromDatabaseFile(file, true);
                if (imported < 0)
                    QMessageBox::critical(this, "Import", "Failed to import devices from file");
                else {
                    QMessageBox::information(
                        this,
                        "Import",
                        QCoreApplication::translate("DeviceManagementWidget",
                                                    "Imported %1 devices from %2")
                            .arg(imported)
                            .arg(file));
                    if (gateway_ && gateway_->registry()) refresh();
                }
            } else if (method.contains("CSV", Qt::CaseInsensitive)) {
                QString file = QFileDialog::getOpenFileName(
                    this,
                    QCoreApplication::translate("DeviceManagementWidget", "Import CSV"),
                    {},
                    QCoreApplication::translate("DeviceManagementWidget",
                                                "CSV Files (*.csv);;All Files (*)"));
                if (file.isEmpty()) return;
                QFile f(file);
                if (!f.open(QIODevice::ReadOnly)) {
                    QMessageBox::critical(this, "Import CSV", "Failed to open file");
                    return;
                }
                const QString content = QString::fromUtf8(f.readAll());
                f.close();
                const int imported = gateway_->registry()->importCsv(content);
                if (imported < 0)
                    QMessageBox::critical(this, "Import CSV", "Failed to import devices from CSV");
                else {
                    QMessageBox::information(
                        this, "Import CSV", QString("Imported %1 devices").arg(imported));
                    if (gateway_ && gateway_->registry()) refresh();
                }
            } else if (method.contains("JSONND", Qt::CaseInsensitive) ||
                       method.contains("NDJSON", Qt::CaseInsensitive)) {
                QString file = QFileDialog::getOpenFileName(
                    this,
                    QCoreApplication::translate("DeviceManagementWidget", "Import JSONND"),
                    {},
                    QCoreApplication::translate("DeviceManagementWidget",
                                                "JSONND Files (*.ndjson *.jsonnd);;All Files (*)"));
                if (file.isEmpty()) return;
                QFile f(file);
                if (!f.open(QIODevice::ReadOnly)) {
                    QMessageBox::critical(this, "Import JSONND", "Failed to open file");
                    return;
                }
                const QString content = QString::fromUtf8(f.readAll());
                f.close();
                const int imported = gateway_->registry()->importJsonNd(content);
                if (imported < 0)
                    QMessageBox::critical(
                        this, "Import JSONND", "Failed to import devices from JSONND");
                else {
                    QMessageBox::information(
                        this, "Import JSONND", QString("Imported %1 devices").arg(imported));
                    if (gateway_ && gateway_->registry()) refresh();
                }
            }
        });

    // set window icon to device management icon from resources
    this->setWindowIcon(QIcon(":/icons/monitor.svg"));

    // add Test All button to bottom layout or top-right area
    QPushButton* btnTestAll =
        new QPushButton(QCoreApplication::translate("DeviceManagementWidget", "Test All"), this);
    btnTestAll->setObjectName("btnTestAll");
    btnTestAll->setCursor(Qt::PointingHandCursor);
    if (auto bottom = findChild<QHBoxLayout*>("bottomLayout")) {
        bottom->addWidget(btnTestAll);
    } else if (this->layout()) {
        // try to insert at bottom of main layout
        this->layout()->addWidget(btnTestAll);
    }

    connect(btnTestAll, &QPushButton::clicked, this, [this]() {
        if (!gateway_ || !gateway_->registry()) return;
        const auto list = gateway_->registry()->list();
        // run tests in background
        std::thread([this, list]() {
            int success = 0;
            int total = list.size();
            for (const auto& d : list) {
                const QString endpoint =
                    (!d.endpoint.trimmed().isEmpty()) ? d.endpoint.trimmed() : d.hwInfo.trimmed();
                if (endpoint.isEmpty()) continue;
                IConnectPtr conn = IConnect::createFromEndpoint(endpoint);
                if (!conn) conn = IConnect::createDefault();
                bool ok = false;
                try {
                    ok = conn->open(endpoint);
                } catch (...) { ok = false; }
                if (ok) {
                    ++success;
                    conn->close();
                }
            }
            QMetaObject::invokeMethod(
                this,
                [this, success, total]() {
                    QMessageBox::information(
                        this,
                        QCoreApplication::translate("DeviceManagementWidget", "Test All"),
                        QCoreApplication::translate("DeviceManagementWidget",
                                                    "%1/%2 devices reachable")
                            .arg(success)
                            .arg(total));
                },
                Qt::QueuedConnection);
        }).detach();
    });

    // Start REST server via DeviceGateway
    // Use RestServer callbacks to react to actual start/stop events instead of
    // relying on an arbitrary timeout.
    if (gateway_ && gateway_->restServer()) {
        // ensure callbacks are cleared if this widget is destroyed
        auto setCallbacks = [this](DeviceGateway::RestServer* s) {
            s->setOnStateChanged([this](DeviceGateway::RestServer::State st) {
                QMetaObject::invokeMethod(this, [this, st]() {
                    // Lock UI during transitions
                    const bool locked = (st == DeviceGateway::RestServer::State::Starting ||
                                         st == DeviceGateway::RestServer::State::Stopping);
                    ui->btnStartRest->setEnabled(!locked && st != DeviceGateway::RestServer::State::Running);
                    ui->btnStopRest->setEnabled(!locked && st == DeviceGateway::RestServer::State::Running);
                    if (st == DeviceGateway::RestServer::State::Running) {
                        ui->btnStartRest->setStyleSheet("background-color: #28a745; color: white;");
                        ui->btnStopRest->setStyleSheet("background-color: #dc3545; color: white;");
                    } else {
                        ui->btnStartRest->setStyleSheet("background-color: none;");
                        ui->btnStopRest->setStyleSheet("background-color: none;");
                    }
                }, Qt::QueuedConnection);
            });
        };
        setCallbacks(gateway_->restServer());
    }

    connect(ui->btnStartRest, &QPushButton::clicked, this, [this]() {
        if (!gateway_) {
            QMessageBox::warning(this, "REST", "DeviceGateway not available");
            return;
        }
        gateway_->start();
    });

    connect(ui->btnStopRest, &QPushButton::clicked, this, [this]() {
        if (!gateway_) {
            QMessageBox::warning(this, "REST", "DeviceGateway not available");
            return;
        }
        gateway_->stop();
    });

    // Initialize REST button states based on current server state
    // Initialize REST button states based on current server state
    if (gateway_ && gateway_->restServer() && gateway_->restServer()->running()) {
        ui->btnStartRest->setEnabled(false);
        ui->btnStartRest->setStyleSheet("background-color: #28a745; color: white;");
        ui->btnStopRest->setEnabled(true);
        ui->btnStopRest->setStyleSheet("background-color: #dc3545; color: white;");
    } else {
        ui->btnStartRest->setEnabled(true);
        ui->btnStartRest->setStyleSheet("background-color: none;");
        ui->btnStopRest->setEnabled(false);
        ui->btnStopRest->setStyleSheet("background-color: none;");
    }

    // populate table initially
    refresh();
}

DeviceManagementWidget::~DeviceManagementWidget() {
    // Clear RestServer callbacks to avoid invoking lambdas that capture this
    // after the widget has been destroyed.
    if (gateway_ && gateway_->restServer()) {
        gateway_->restServer()->setOnStateChanged(nullptr);
    }

    delete ui;
}
