#include "devicemanagementwidget.h"

#include <qcoreapplication.h>

#include <QFileDialog>
#include <QFuture>
#include <QFutureWatcher>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QSplitter>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextEdit>
#include <QTime>
#include <QVBoxLayout>
#include <QtConcurrent>

#include "modules/DeviceGateway/device_gateway.h"  // for gateway and device info
#include "widgets/deviceeditdialog.h"              // for add/edit dialog
#include "widgets/importexportcontrol.h"           // for import/export dialog

// Simple in-place summary widget (no Charts dependency)
class SimpleSummaryWidget : public QWidget {
    Q_OBJECT
   public:
    explicit SimpleSummaryWidget(QWidget* parent = nullptr) : QWidget(parent) {
        setMinimumHeight(140);
    }
    void setCounts(int s, int f, int u) {
        success = s;
        fail = f;
        untested = u;
        update();
    }

   protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.fillRect(rect(), palette().window());
        const int total = success + fail + untested;
        const QRect r = rect().adjusted(8, 8, -8, -8);
        const int barH = qMax(12, r.height() / 6);
        int y = r.top();

        auto drawBar = [&](const QColor& color, int value, const QString& label) {
            double frac = (total == 0) ? 0.0 : double(value) / double(total);
            QRect br(r.left(), y, int(r.width() * frac), barH);
            p.setBrush(color);
            p.setPen(Qt::NoPen);
            p.drawRect(br);
            p.setPen(palette().text().color());
            p.drawText(r.left() + 6,
                       y + barH - 2,
                       QString("%1: %2 (%3%)").arg(label).arg(value).arg(int(frac * 100)));
            y += barH + 8;
        };

        drawBar(QColor("#4CAF50"),
                success,
                QCoreApplication::translate("DeviceManagementWidget", "Success"));
        drawBar(
            QColor("#F44336"), fail, QCoreApplication::translate("DeviceManagementWidget", "Fail"));
        drawBar(QColor("#9E9E9E"),
                untested,
                QCoreApplication::translate("DeviceManagementWidget", "Untested"));
    }

   private:
    int success = 0;
    int fail = 0;
    int untested = 0;
};

DeviceManagementWidget::DeviceManagementWidget(DeviceGateway::DeviceGateway* gateway,
                                               QWidget* parent)
    : QWidget(parent), ui(new Ui::DeviceManagementWidget), gateway_(gateway) {
    ui->setupUi(this);

    // Build UI controls (from .ui widgets)
    // left table from ui
    table_ = findChild<QTableWidget*>("tableWidget");
    if (!table_) { table_ = new QTableWidget(this); }
    table_->setColumnCount(9);
    table_->setHorizontalHeaderLabels(
        {QCoreApplication::translate("DeviceManagementWidget", "ID"),
         QCoreApplication::translate("DeviceManagementWidget", "Name"),
         QCoreApplication::translate("DeviceManagementWidget", "Status"),
         QCoreApplication::translate("DeviceManagementWidget", "Endpoint"),
         QCoreApplication::translate("DeviceManagementWidget", "Type"),
         QCoreApplication::translate("DeviceManagementWidget", "HW"),
         QCoreApplication::translate("DeviceManagementWidget", "FW"),
         QCoreApplication::translate("DeviceManagementWidget", "Owner"),
         QCoreApplication::translate("DeviceManagementWidget", "Group")});
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    // find buttons and controls created by .ui
    btnAdd_ = findChild<QPushButton*>("btnAdd");
    btnEdit_ = findChild<QPushButton*>("btnEdit");
    btnDelete_ = findChild<QPushButton*>("btnDelete");
    btnImport_ = findChild<QPushButton*>("btnImport");
    btnExport_ = findChild<QPushButton*>("btnExport");
    btnRefresh_ = findChild<QPushButton*>("btnRefresh");
    btnTest_ = findChild<QPushButton*>("btnTest");
    btnRest_ = findChild<QPushButton*>("btnRest");
    leFilter_ = findChild<QLineEdit*>("leFilter");
    chartWidget_ = findChild<QWidget*>("chartWidget");
    teTestResults_ = findChild<QTextEdit*>("teTestResults");

    // create summary widget (pure widgets) and insert into chartWidget_
    if (chartWidget_) {
        summaryWidget_ = new SimpleSummaryWidget(chartWidget_);
        if (chartWidget_->layout()) {
            chartWidget_->layout()->addWidget(summaryWidget_);
        } else {
            auto* v = new QVBoxLayout(chartWidget_);
            v->setContentsMargins(4, 4, 4, 4);
            v->addWidget(summaryWidget_);
        }
    } else {
        summaryWidget_ = new SimpleSummaryWidget(this);
    }

    // fallback init if UI doesn't contain them
    if (!btnRefresh_)
        btnRefresh_ =
            new QPushButton(QCoreApplication::translate("DeviceManagementWidget", "Refresh"), this);
    if (!btnImport_)
        btnImport_ =
            new QPushButton(QCoreApplication::translate("DeviceManagementWidget", "Import"), this);
    if (!btnExport_)
        btnExport_ =
            new QPushButton(QCoreApplication::translate("DeviceManagementWidget", "Export"), this);
    if (!btnAdd_)
        btnAdd_ =
            new QPushButton(QCoreApplication::translate("DeviceManagementWidget", "Add"), this);
    if (!btnEdit_)
        btnEdit_ =
            new QPushButton(QCoreApplication::translate("DeviceManagementWidget", "Edit"), this);
    if (!btnDelete_)
        btnDelete_ =
            new QPushButton(QCoreApplication::translate("DeviceManagementWidget", "Delete"), this);
    if (!btnTest_)
        btnTest_ = new QPushButton(
            QCoreApplication::translate("DeviceManagementWidget", "Test Selected"), this);
    if (!btnRest_) {
        // REST control moved to Settings page; create a hidden placeholder with a helpful tooltip
        btnRest_ = new QPushButton(
            QCoreApplication::translate("DeviceManagementWidget", "Start REST"), this);
        btnRest_->setVisible(false);
        btnRest_->setToolTip(QCoreApplication::translate("DeviceManagementWidget",
                                                         "REST control moved to System Settings"));
    }
    if (!leFilter_) leFilter_ = new QLineEdit(this);
    if (!teTestResults_) teTestResults_ = new QTextEdit(this);

    // UI polish: icons, table behavior, and styles
    const QIcon icAdd(":/icons/icons/device.svg");
    const QIcon icEdit(":/icons/icons/setting.svg");
    const QIcon icDelete(":/icons/icons/logs.svg");
    const QIcon icImport(":/icons/icons/brand.svg");
    const QIcon icExport(":/icons/icons/app.svg");
    const QIcon icRefresh(":/icons/icons/monitor.svg");
    const QIcon icTest(":/icons/icons/device.svg");
    if (!icAdd.isNull()) btnAdd_->setIcon(icAdd);
    if (!icEdit.isNull()) btnEdit_->setIcon(icEdit);
    if (!icDelete.isNull()) btnDelete_->setIcon(icDelete);
    if (!icImport.isNull()) btnImport_->setIcon(icImport);
    if (!icExport.isNull()) btnExport_->setIcon(icExport);
    if (!icRefresh.isNull()) btnRefresh_->setIcon(icRefresh);
    if (!icTest.isNull()) btnTest_->setIcon(icTest);
    const QIcon icRest(":/icons/icons/monitor.svg");
    if (!icRest.isNull()) btnRest_->setIcon(icRest);

    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setAlternatingRowColors(true);
    table_->verticalHeader()->setVisible(false);
    // default sensible column widths
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);

    // If mainSplitter exists, prefer left pane (table)
    if (auto sp = findChild<QSplitter*>("mainSplitter")) {
        sp->setStretchFactor(0, 3);
        sp->setStretchFactor(1, 2);
    }

    teTestResults_->setReadOnly(true);
    teTestResults_->setStyleSheet(
        "QTextEdit{background:transparent;border:1px solid #444;padding:6px;font-family: Consolas, "
        "'Courier New', monospace; font-size:11px;}");

    // wire signals
    connect(btnRefresh_, &QPushButton::clicked, this, &DeviceManagementWidget::refresh);
    connect(btnImport_, &QPushButton::clicked, this, &DeviceManagementWidget::onImport);
    connect(btnExport_, &QPushButton::clicked, this, &DeviceManagementWidget::onExport);
    connect(btnAdd_, &QPushButton::clicked, this, &DeviceManagementWidget::onAdd);
    connect(btnEdit_, &QPushButton::clicked, this, &DeviceManagementWidget::onEdit);
    connect(btnDelete_, &QPushButton::clicked, this, &DeviceManagementWidget::onDelete);
    connect(btnTest_, &QPushButton::clicked, this, &DeviceManagementWidget::onTestSelected);
    connect(btnRest_, &QPushButton::clicked, this, &DeviceManagementWidget::onToggleRest);
    connect(leFilter_, &QLineEdit::textChanged, this, &DeviceManagementWidget::onFilterChanged);

    // ensure test results read-only
    teTestResults_->setReadOnly(true);

    refresh();
    updateRestButton();
}

DeviceManagementWidget::~DeviceManagementWidget() {
    delete ui;
}

#if defined(QT_VERSION) && (QT_VERSION >= QT_VERSION_CHECK(5, 0, 0))
#include "devicemanagementwidget.moc"
#endif

void DeviceManagementWidget::refresh() {
    table_->setRowCount(0);
    if (!gateway_) return;
    const auto devs = gateway_->loadDevicesFromStorage();
    for (int i = 0; i < devs.size(); ++i) {
        const auto& d = devs.at(i);
        table_->insertRow(i);
        table_->setItem(i, 0, new QTableWidgetItem(d.id));
        table_->setItem(i, 1, new QTableWidgetItem(d.name));
        table_->setItem(i, 2, new QTableWidgetItem(d.status));
        table_->setItem(i, 3, new QTableWidgetItem(d.endpoint));
        table_->setItem(i, 4, new QTableWidgetItem(d.type));
        table_->setItem(i, 5, new QTableWidgetItem(d.hw_info));
        table_->setItem(i, 6, new QTableWidgetItem(d.firmware_version));
        table_->setItem(i, 7, new QTableWidgetItem(d.owner));
        table_->setItem(i, 8, new QTableWidgetItem(d.group));
    }
    updateChart();
}

void DeviceManagementWidget::onFilterChanged() {
    const QString f = leFilter_->text().trimmed();
    for (int r = 0; r < table_->rowCount(); ++r) {
        bool visible = f.isEmpty();
        if (!visible) {
            for (int c = 0; c < table_->columnCount(); ++c) {
                auto it = table_->item(r, c);
                if (it && it->text().contains(f, Qt::CaseInsensitive)) {
                    visible = true;
                    break;
                }
            }
        }
        table_->setRowHidden(r, !visible);
    }
}

void DeviceManagementWidget::onTestSelected() {
    const auto sel = table_->selectedItems();
    if (sel.isEmpty()) {
        appendTestResult(
            QCoreApplication::translate("DeviceManagementWidget", "No device selected for test."));
        return;
    }
    const int row = sel.first()->row();
    const QString id = table_->item(row, 0)->text();
    const QString endpoint = table_->item(row, 3)->text();

    appendTestResult(
        QCoreApplication::translate("DeviceManagementWidget", "Starting test for device %1 (%2)")
            .arg(id, endpoint));

    // run the connection test asynchronously to avoid blocking UI
    QFuture<void> f = QtConcurrent::run([this, id, endpoint]() {
        QString out;
        if (endpoint.isEmpty()) {
            out = QCoreApplication::translate("DeviceManagementWidget", "Endpoint empty");
        } else {
            // try to create a connect and open
            auto conn = IConnect::createFromEndpoint(endpoint);
            if (!conn) {
                out = QCoreApplication::translate("DeviceManagementWidget",
                                                  "No connector available for endpoint: %1")
                          .arg(endpoint);
            } else {
                const bool ok = conn->open(endpoint);
                if (ok) {
                    conn->close();
                    out = QCoreApplication::translate("DeviceManagementWidget", "Connection OK");
                } else {
                    out =
                        QCoreApplication::translate("DeviceManagementWidget", "Connection Failed");
                }
            }
        }
        // append result back on GUI thread
        QMetaObject::invokeMethod(this, [this, id, endpoint, out]() {
            appendTestResult(QCoreApplication::translate("DeviceManagementWidget", "[%1] %2 -> %3")
                                 .arg(id, endpoint, out));
            updateChart();
        });
    });
}

void DeviceManagementWidget::appendTestResult(const QString& txt) {
    if (!teTestResults_) return;
    teTestResults_->append(QString("%1: %2").arg(QTime::currentTime().toString(), txt));
}

void DeviceManagementWidget::updateChart() {
    if (!chartWidget_) return;
    const int total = table_->rowCount();
    const QString results = teTestResults_ ? teTestResults_->toPlainText() : QString();
    int success = results.count("Connection OK");
    int fail = results.count("Connection Failed") + results.count("No connector available") +
               results.count("Endpoint empty");

    // Always use the lightweight summary widget (no QtCharts dependency)
    if (summaryWidget_) {
        static_cast<SimpleSummaryWidget*>(summaryWidget_)
            ->setCounts(success, fail, qMax(0, total - (success + fail)));
    } else if (chartWidget_) {
        chartWidget_->setUpdatesEnabled(false);
        chartWidget_->setWindowTitle(
            QCoreApplication::translate("DeviceManagementWidget",
                                        "Devices: %1  Success: %2  Fail: %3")
                .arg(total)
                .arg(success)
                .arg(fail));
        chartWidget_->update();
        chartWidget_->setUpdatesEnabled(true);
    }
}

void DeviceManagementWidget::onAdd() {
    DeviceEditDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted) return;
    auto d = dlg.device();
    if (!gateway_->addDeviceWithConnect(d)) {
        QMessageBox::warning(
            this,
            QCoreApplication::translate("DeviceManagementWidget", "Add device"),
            QCoreApplication::translate("DeviceManagementWidget", "Failed to add device."));
    }
    refresh();
}

void DeviceManagementWidget::onEdit() {
    const auto sel = table_->selectedItems();
    if (sel.isEmpty()) return;
    const int row = sel.first()->row();
    DeviceGateway::DeviceInfo d;
    d.id = table_->item(row, 0)->text();
    d.name = table_->item(row, 1)->text();
    d.status = table_->item(row, 2)->text();
    d.endpoint = table_->item(row, 3)->text();
    d.type = table_->item(row, 4)->text();
    d.hw_info = table_->item(row, 5)->text();
    d.firmware_version = table_->item(row, 6)->text();
    d.owner = table_->item(row, 7)->text();
    d.group = table_->item(row, 8)->text();

    DeviceEditDialog dlg(this);
    dlg.setDevice(d);
    if (dlg.exec() != QDialog::Accepted) return;
    auto nd = dlg.device();
    if (!gateway_->persistDevice(nd)) {
        QMessageBox::warning(
            this,
            QCoreApplication::translate("DeviceManagementWidget", "Edit device"),
            QCoreApplication::translate("DeviceManagementWidget", "Failed to persist device."));
    }
    refresh();
}

void DeviceManagementWidget::onDelete() {
    const auto sel = table_->selectedItems();
    if (sel.isEmpty()) return;
    const int row = sel.first()->row();
    const QString id = table_->item(row, 0)->text();
    if (!gateway_->removeDeviceAndClose(id)) {
        QMessageBox::warning(
            this,
            QCoreApplication::translate("DeviceManagementWidget", "Delete device"),
            QCoreApplication::translate("DeviceManagementWidget", "Failed to delete device."));
    }
    refresh();
}

void DeviceManagementWidget::onImport() {
    const QString file = QFileDialog::getOpenFileName(
        this,
        QCoreApplication::translate("DeviceManagementWidget", "Import devices"),
        {},
        QCoreApplication::translate("DeviceManagementWidget",
                                    "CSV Files (*.csv);;NDJSON (*.ndjson);;All Files (*.*)"));
    if (file.isEmpty()) return;
    const auto resp = QMessageBox::question(
        this,
        QCoreApplication::translate("DeviceManagementWidget", "Import options"),
        QCoreApplication::translate("DeviceManagementWidget",
                                    "Automatically create connections for devices with endpoints?"),
        QMessageBox::Yes | QMessageBox::No);
    const bool autoConnect = (resp == QMessageBox::Yes);
    if (file.endsWith(".csv", Qt::CaseInsensitive)) {
        gateway_->importDevicesCsv(file, autoConnect);
    } else {
        gateway_->importDevicesJsonND(file, autoConnect);
    }
    refresh();
}

void DeviceManagementWidget::onExport() {
    const QString file = QFileDialog::getSaveFileName(
        this,
        QCoreApplication::translate("DeviceManagementWidget", "Export devices"),
        {},
        QCoreApplication::translate("DeviceManagementWidget",
                                    "CSV Files (*.csv);;NDJSON (*.ndjson)"));
    if (file.isEmpty()) return;
    if (file.endsWith(".csv", Qt::CaseInsensitive)) {
        gateway_->exportDevicesCsv(file);
    } else {
        gateway_->exportDevicesJsonND(file);
    }
}

void DeviceManagementWidget::onToggleRest() {
    if (!gateway_) return;
    if (gateway_->isRestRunning()) {
        gateway_->shutdown();
        appendTestResult(
            QCoreApplication::translate("DeviceManagementWidget", "REST server stopped"));
    } else {
        const bool ok = gateway_->init();
        if (ok) {
            appendTestResult(
                QCoreApplication::translate("DeviceManagementWidget", "REST server started"));
        } else {
            appendTestResult(QCoreApplication::translate("DeviceManagementWidget",
                                                         "Failed to start REST server"));
        }
    }
    updateRestButton();
}

void DeviceManagementWidget::updateRestButton() {
    if (!btnRest_) return;
    if (gateway_ && gateway_->isRestRunning()) {
        btnRest_->setText(QCoreApplication::translate("DeviceManagementWidget", "Stop REST"));
        btnRest_->setToolTip(
            QCoreApplication::translate("DeviceManagementWidget", "Stop embedded REST server"));
    } else {
        btnRest_->setText(QCoreApplication::translate("DeviceManagementWidget", "Start REST"));
        btnRest_->setToolTip(
            QCoreApplication::translate("DeviceManagementWidget", "Start embedded REST server"));
    }
}
