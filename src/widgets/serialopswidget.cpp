#include "serialopswidget.h"

#include <spdlog/spdlog.h>

#include <QBoxLayout>
#include <QFormLayout>
#include <QInputDialog>
#include <QLabel>
#include <QMessageBox>
#include <QProgressBar>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QSpinBox>
#include <QTimer>
#include <QtSql/QSqlError>
#include <QtSql/QSqlQuery>

#include "modules/Storage/dbmanager.h"
#include "modules/Storage/storage.h"
#include "ui_serialopswidget.h"
#include "widgets/actionmanager.h"

SerialOpsWidget::SerialOpsWidget(QWidget* parent) : QWidget(parent), ui(new Ui::SerialOpsWidget) {
    ui->setupUi(this);
    // Add short explanatory labels above some dropdowns so users know what values mean
    auto insertAbove = [this](QWidget* target, QLabel* label) {
        if (!target || !label) return;
        QWidget* parentWidget = target->parentWidget();
        if (!parentWidget) parentWidget = this;
        QLayout* layout = parentWidget->layout();
        if (auto box = qobject_cast<QBoxLayout*>(layout)) {
            int idx = box->indexOf(target);
            if (idx >= 0)
                box->insertWidget(idx, label);
            else
                box->addWidget(label);
        } else if (auto form = qobject_cast<QFormLayout*>(layout)) {
            int rows = form->rowCount();
            bool inserted = false;
            for (int r = 0; r < rows; ++r) {
                QLayoutItem* fieldItem = form->itemAt(r, QFormLayout::FieldRole);
                QWidget* wField = fieldItem ? fieldItem->widget() : nullptr;
                QLayoutItem* labelItem = form->itemAt(r, QFormLayout::LabelRole);
                QWidget* wLabel = labelItem ? labelItem->widget() : nullptr;
                if (wField == target || wLabel == target) {
                    form->insertRow(r, label);
                    inserted = true;
                    break;
                }
            }
            if (!inserted) form->addRow(label);
        } else {
            // fallback: parentless or unknown layout, just attach and show above target
            label->setParent(parentWidget);
            label->show();
        }
    };

    // explanatory labels (small, gray text). Keep them short and translatable.
    QLabel* lblPortHelp = new QLabel(
        QCoreApplication::translate("SerialOpsWidget",
                                    "Port (e.g. COM5): the system name of the serial device."),
        this);
    lblPortHelp->setWordWrap(true);
    lblPortHelp->setObjectName(QStringLiteral("labelPortHelp"));
    lblPortHelp->setStyleSheet("color:gray; font-size:11px;");

    QLabel* lblBaudHelp = new QLabel(
        QCoreApplication::translate("SerialOpsWidget",
                                    "Baud (e.g. 115200): communication speed in bits per second."),
        this);
    lblBaudHelp->setWordWrap(true);
    lblBaudHelp->setObjectName(QStringLiteral("labelBaudHelp"));
    lblBaudHelp->setStyleSheet("color:gray; font-size:11px;");

    // Try to insert above the known combo boxes. If insertion fails, labels will still be shown.
    insertAbove(ui->comboPort, lblPortHelp);
    insertAbove(ui->comboBaud, lblBaudHelp);

    // Additional short help labels for other serial options.
    QLabel* lblDataBitsHelp =
        new QLabel(QCoreApplication::translate(
                       "SerialOpsWidget", "Data bits (e.g. 8): number of data bits per frame."),
                   this);
    lblDataBitsHelp->setWordWrap(true);
    lblDataBitsHelp->setObjectName(QStringLiteral("labelDataBitsHelp"));
    lblDataBitsHelp->setStyleSheet("color:gray; font-size:11px;");
    // try to place above the combo for data bits; use findChild because autogen timing may vary
    insertAbove(this->findChild<QWidget*>(QStringLiteral("comboDataBits")), lblDataBitsHelp);

    QLabel* lblParityHelp =
        new QLabel(QCoreApplication::translate(
                       "SerialOpsWidget",
                       "Parity (None/Even/Odd): simple error-checking bit used by some devices."),
                   this);
    lblParityHelp->setWordWrap(true);
    lblParityHelp->setObjectName(QStringLiteral("labelParityHelp"));
    lblParityHelp->setStyleSheet("color:gray; font-size:11px;");
    insertAbove(this->findChild<QWidget*>(QStringLiteral("comboParity")), lblParityHelp);

    QLabel* lblStopBitsHelp = new QLabel(
        QCoreApplication::translate("SerialOpsWidget",
                                    "Stop bits (1 / 1.5 / 2): marks the end of a data frame."),
        this);
    lblStopBitsHelp->setWordWrap(true);
    lblStopBitsHelp->setObjectName(QStringLiteral("labelStopBitsHelp"));
    lblStopBitsHelp->setStyleSheet("color:gray; font-size:11px;");
    insertAbove(this->findChild<QWidget*>(QStringLiteral("comboStopBits")), lblStopBitsHelp);

    QLabel* lblTimeoutHelp = new QLabel(
        QCoreApplication::translate("SerialOpsWidget",
                                    "Timeout (ms): how long reads/writes wait before giving up."),
        this);
    lblTimeoutHelp->setWordWrap(true);
    lblTimeoutHelp->setObjectName(QStringLiteral("labelTimeoutHelp"));
    lblTimeoutHelp->setStyleSheet("color:gray; font-size:11px;");
    // prefer the spin control if present, otherwise the label
    QWidget* timeoutTarget = this->findChild<QWidget*>(QStringLiteral("spinTimeout"));
    if (!timeoutTarget) timeoutTarget = this->findChild<QWidget*>(QStringLiteral("labelTimeout"));
    insertAbove(timeoutTarget, lblTimeoutHelp);
    // Buttons use auto-connect naming (on_<object>_<signal>), no manual connect needed

    // populate port list
    ui->comboPort->clear();
    const auto ports = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo& info : ports) ui->comboPort->addItem(info.portName());

    // populate baudrates
    ui->comboBaud->clear();
    QList<int> bauds = {115200, 57600, 38400, 19200, 9600, 4800, 2400, 1200};
    for (int b : bauds) ui->comboBaud->addItem(QString::number(b));

    // defaults for data/parity/stop (use findChild to be robust against autogen timing)
    if (auto cb = this->findChild<QComboBox*>(QStringLiteral("comboDataBits")))
        cb->setCurrentText("8");
    if (auto cb = this->findChild<QComboBox*>(QStringLiteral("comboParity")))
        cb->setCurrentText("None");
    if (auto cb = this->findChild<QComboBox*>(QStringLiteral("comboStopBits")))
        cb->setCurrentText("1");

    // activity timer to decay progress
    m_activityTimer = new QTimer(this);
    m_activityTimer->setInterval(150);
    connect(m_activityTimer, &QTimer::timeout, this, [this]() {
        if (auto pb = this->findChild<QProgressBar*>(QStringLiteral("progressActivity"))) {
            int v = pb->value();
            v = qMax(0, v - 5);
            pb->setValue(v);
            if (v == 0) m_activityTimer->stop();
        }
    });

    setConnected(false);
}

SerialOpsWidget::~SerialOpsWidget() {
    delete ui;
}

static QString mapSerialErrorToEnglish(QSerialPort::SerialPortError err) {
    switch (err) {
        case QSerialPort::NoError:
            return QStringLiteral("No error");
        case QSerialPort::DeviceNotFoundError:
            return QStringLiteral("Device not found");
        case QSerialPort::PermissionError:
            return QStringLiteral("Permission denied");
        case QSerialPort::OpenError:
            return QStringLiteral("Failed to open port");
        case QSerialPort::NotOpenError:
            return QStringLiteral("Port not open");
        case QSerialPort::ReadError:
            return QStringLiteral("Read error");
        case QSerialPort::WriteError:
            return QStringLiteral("Write error");
        case QSerialPort::ResourceError:
            return QStringLiteral("Resource error");
        case QSerialPort::UnsupportedOperationError:
            return QStringLiteral("Unsupported operation");
        case QSerialPort::UnknownError:
        default:
            return QStringLiteral("Unknown serial port error");
    }
}

void SerialOpsWidget::on_btnOpen_clicked() {
    if (m_port) return;
    QString portName = ui->comboPort->currentText();
    if (portName.isEmpty()) return;
    m_port = new QSerialPort(portName, this);
    m_port->setBaudRate(ui->comboBaud->currentText().toInt());
    // apply data bits
    if (auto dbcb = this->findChild<QComboBox*>(QStringLiteral("comboDataBits"))) {
        QString db = dbcb->currentText();
        if (db == "8")
            m_port->setDataBits(QSerialPort::Data8);
        else if (db == "7")
            m_port->setDataBits(QSerialPort::Data7);
        else if (db == "6")
            m_port->setDataBits(QSerialPort::Data6);
        else
            m_port->setDataBits(QSerialPort::Data5);
    }
    // parity
    if (auto pcb = this->findChild<QComboBox*>(QStringLiteral("comboParity"))) {
        QString p = pcb->currentText();
        if (p == "None")
            m_port->setParity(QSerialPort::NoParity);
        else if (p == "Even")
            m_port->setParity(QSerialPort::EvenParity);
        else if (p == "Odd")
            m_port->setParity(QSerialPort::OddParity);
        else
            m_port->setParity(QSerialPort::SpaceParity);
    }
    // stop bits
    if (auto sbcb = this->findChild<QComboBox*>(QStringLiteral("comboStopBits"))) {
        QString sb = sbcb->currentText();
        if (sb == "1")
            m_port->setStopBits(QSerialPort::OneStop);
        else if (sb == "1.5")
            m_port->setStopBits(QSerialPort::OneAndHalfStop);
        else
            m_port->setStopBits(QSerialPort::TwoStop);
    }
    // timeout label
    if (auto spin = this->findChild<QSpinBox*>(QStringLiteral("spinTimeout"))) {
        if (auto lbl = this->findChild<QLabel*>(QStringLiteral("labelTimeout")))
            lbl->setText(QCoreApplication::translate("SerialOpsWidget", "Timeout (ms): %1")
                             .arg(spin->value()));
    }
    if (!m_port->open(QIODevice::ReadWrite)) {
        spdlog::error("Failed to open serial {}: {}",
                      portName.toStdString(),
                      m_port->errorString().toStdString());
        // Use English, mapped message to avoid localized/encoded errorString() causing garbled text
        QString msg = mapSerialErrorToEnglish(m_port->error());
        QMessageBox::warning(
            this,
            QCoreApplication::translate("SerialOpsWidget", "Serial"),
            QCoreApplication::translate("SerialOpsWidget", "Failed to open port: %1").arg(msg));
        delete m_port;
        m_port = nullptr;
        return;
    }
    connect(m_port, &QSerialPort::readyRead, this, [this]() {
        QByteArray d = m_port->readAll();
        ui->teRecv->append(QString::fromUtf8(d));
        // bump activity
        if (auto pb = this->findChild<QProgressBar*>(QStringLiteral("progressActivity"))) {
            pb->setValue(qMin(100, pb->value() + 20));
            if (!m_activityTimer->isActive()) m_activityTimer->start();
        }
    });
    setConnected(true);
}

void SerialOpsWidget::on_btnClose_clicked() {
    if (!m_port) return;
    m_port->close();
    delete m_port;
    m_port = nullptr;
    setConnected(false);
}

void SerialOpsWidget::on_btnSend_clicked() {
    if (!m_port) {
        QMessageBox::warning(this,
                             QCoreApplication::translate("SerialOpsWidget", "Serial"),
                             QCoreApplication::translate("SerialOpsWidget", "Port not open"));
        return;
    }
    QString txt = ui->editSend->text();
    if (txt.isEmpty()) return;
    QByteArray b = txt.toUtf8();
    if (ui->chkAppendNewline->isChecked()) b.append('\n');
    qint64 n = m_port->write(b);
    spdlog::info("Serial write {} bytes", n);
}

void SerialOpsWidget::on_btnSaveAction_clicked() {
    QString txt = ui->editSend->text();
    if (txt.isEmpty()) return;
    bool ok;
    QString name =
        QInputDialog::getText(this,
                              QCoreApplication::translate("SerialOpsWidget", "Save Serial Action"),
                              QCoreApplication::translate("SerialOpsWidget", "Action name:"),
                              QLineEdit::Normal,
                              QString(),
                              &ok);
    if (!ok || name.isEmpty()) return;
    using namespace storage;
    if (!DbManager::instance().initialized()) {
        QString defaultDb =
            QCoreApplication::applicationDirPath() + QLatin1String("/crosscontrol.db");
        if (!Storage::initSqlite(defaultDb)) {
            QMessageBox::warning(
                this,
                QCoreApplication::translate("SerialOpsWidget", "Storage"),
                QCoreApplication::translate("SerialOpsWidget", "Storage not initialized."));
            return;
        }
    }
    if (!Storage::db().isOpen()) {
        QMessageBox::warning(
            this,
            QCoreApplication::translate("SerialOpsWidget", "Storage"),
            QCoreApplication::translate("SerialOpsWidget", "Storage not initialized."));
        return;
    }
    if (!Storage::saveAction("serial", name, txt.toUtf8()))
        QMessageBox::warning(
            this,
            QCoreApplication::translate("SerialOpsWidget", "Save"),
            QCoreApplication::translate("SerialOpsWidget", "Failed to save action"));
    else
        QMessageBox::information(
            this,
            QCoreApplication::translate("SerialOpsWidget", "Saved"),
            QCoreApplication::translate("SerialOpsWidget", "Saved action '%1'").arg(name));
}

void SerialOpsWidget::on_btnLoadAction_clicked() {
    // Load a saved serial action (raw send text) and restore to editSend
    ActionManager dlg(QStringLiteral("serial"), this);
    connect(&dlg,
            &ActionManager::actionLoaded,
            this,
            [this](const QString& name, const QByteArray& payload) {
                ui->editSend->setText(QString::fromUtf8(payload));
                if (auto lbl = this->findChild<QLabel*>(QStringLiteral("labelLoadedAction")))
                    lbl->setText(name);
            });
    dlg.exec();
}

void SerialOpsWidget::on_btnClear_clicked() {
    if (auto te = this->findChild<QTextEdit*>(QStringLiteral("teRecv"))) te->clear();
    if (auto ed = this->findChild<QLineEdit*>(QStringLiteral("editSend"))) ed->clear();
    if (auto lbl = this->findChild<QLabel*>(QStringLiteral("labelLoadedAction"))) lbl->clear();
}

void SerialOpsWidget::setConnected(bool connected) {
    ui->btnOpen->setEnabled(!connected);
    ui->btnClose->setEnabled(connected);
    ui->btnSend->setEnabled(connected);
    if (connected)
        ui->labelStatus->setText(QCoreApplication::translate("SerialOpsWidget", "Open"));
    else
        ui->labelStatus->setText(QCoreApplication::translate("SerialOpsWidget", "Closed"));
}
