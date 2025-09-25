
#include "messagewidget.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QFileInfo>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QVBoxLayout>
#include <QVariant>

#include "modules/Storage/storage.h"
#include "ui_messagewidget.h"

using namespace storage;

static QString defaultDatabasePath() {
    return QCoreApplication::applicationDirPath() + QLatin1String("/crosscontrol.db");
}

/**
 * @brief 构造函数：初始化 UI、数据库，并加载历史留言
 *
 * - 若 Storage 未初始化，会尝试使用默认路径 lazy 初始化 SQLite 数据库
 * - 检查并创建 messages 表、执行必要的列迁移（visitor_*）以兼容旧库
 * - 将 messages 表中的记录按时间加载到 QListWidget，每条记录用自定义 QWidget
 * 展示姓名/联系方式/时间/内容
 */
MessageWidget::MessageWidget(QWidget* parent) : QWidget(parent), ui(new Ui::MessageWidget) {
    ui->setupUi(this);

    // Lazy initialize DB for the message board
    bool dbOk = true;
    if (!DbManager::instance().initialized()) { dbOk = Storage::initSqlite(defaultDatabasePath()); }

    if (!dbOk) {
        QMessageBox::critical(
            this,
            tr("Database Error"),
            tr("Failed to initialize database. Message saving will be disabled."));
        if (ui->btnSendMessage) ui->btnSendMessage->setEnabled(false);
        if (ui->btnDeleteMessage) ui->btnDeleteMessage->setEnabled(false);
        return;
    }

    if (!Storage::db().isOpen()) {
        QMessageBox::critical(
            this,
            tr("Database Error"),
            tr("Database opened but not available (not open). Message saving will be disabled."));
        if (ui->btnSendMessage) ui->btnSendMessage->setEnabled(false);
        if (ui->btnDeleteMessage) ui->btnDeleteMessage->setEnabled(false);
        return;
    }

    // Diagnostic logs: drivers, db validity and file path/permissions
    qWarning() << "Available QSql drivers:" << QSqlDatabase::drivers();
    qWarning() << "DB isValid:" << Storage::db().isValid() << " isOpen:" << Storage::db().isOpen();
    qWarning() << "DB driver name:" << Storage::db().driverName()
               << " file:" << Storage::db().databaseName();
    QFileInfo dbfi(Storage::db().databaseName());
    if (dbfi.exists()) {
        qWarning() << "DB file exists size(bytes):" << dbfi.size()
                   << " writable:" << dbfi.isWritable();
    } else {
        qWarning() << "DB file does not exist yet:" << dbfi.absoluteFilePath();
    }

    // Ensure messages table exists (extra columns for visitor info)
    QSqlQuery q(Storage::db());
    const QString createSql =
        "CREATE TABLE IF NOT EXISTS messages ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "content TEXT NOT NULL,"
        "created_at TEXT NOT NULL"
        ");";
    if (!q.exec(createSql)) {
        qWarning() << "Failed to create messages table:" << q.lastError().text();
    }

    // Migration: ensure optional visitor columns exist (for older DBs)
    QSet<QString> cols;
    QSqlQuery qi(Storage::db());
    if (qi.exec("PRAGMA table_info('messages')")) {
        while (qi.next()) {
            // PRAGMA table_info returns: cid, name, type, notnull, dflt_value, pk
            cols.insert(qi.value(1).toString().toLower());
        }
    }

    struct AddCol {
        const char* name;
        const char* def;
    } addCols[] = {
        {"visitor_name", "TEXT"},
        {"visitor_phone", "TEXT"},
        {"visitor_email", "TEXT"},
    };
    for (const auto& c : addCols) {
        if (!cols.contains(QString::fromUtf8(c.name))) {
            const QString alter = QString("ALTER TABLE messages ADD COLUMN %1 %2;")
                                      .arg(QString::fromUtf8(c.name), QString::fromUtf8(c.def));
            if (!q.exec(alter)) {
                qWarning() << "Failed to add column" << c.name << ":" << q.lastError().text();
            }
        }
    }

    // Load existing messages into QListWidget; store DB id in UserRole
    const QString selectSql =
        "SELECT id, created_at, visitor_name, visitor_phone, visitor_email, content FROM messages "
        "ORDER BY created_at ASC";
    if (q.exec(selectSql)) {
        while (q.next()) {
            const int id = q.value(0).toInt();
            const QString created = q.value(1).toString();
            const QString name = q.value(2).toString();
            const QString phone = q.value(3).toString();
            const QString email = q.value(4).toString();
            const QString content = q.value(5).toString();

            // Build a rich item widget showing all fields
            QWidget* w = new QWidget();
            w->setAutoFillBackground(true);
            QVBoxLayout* v = new QVBoxLayout(w);
            v->setContentsMargins(6, 6, 6, 6);
            v->setSpacing(4);

            // Top row: name and time
            QHBoxLayout* top = new QHBoxLayout();
            QLabel* lblName = new QLabel();
            lblName->setText(name.isEmpty() ? tr("(Anonymous)") : name);
            QFont bold = lblName->font();
            bold.setBold(true);
            lblName->setFont(bold);
            top->addWidget(lblName, 1);

            QLabel* lblTime = new QLabel(created);
            lblTime->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            QFont small = lblTime->font();
            small.setPointSizeF(small.pointSizeF() - 1.0);
            lblTime->setFont(small);
            top->addWidget(lblTime, 0);

            v->addLayout(top);

            // Middle row: phone and email
            QLabel* lblContact = new QLabel(QString("%1 | %2").arg(
                phone.isEmpty() ? tr("-") : phone, email.isEmpty() ? tr("-") : email));
            lblContact->setWordWrap(false);
            QFont contactFont = lblContact->font();
            contactFont.setPointSizeF(contactFont.pointSizeF() - 0.5);
            lblContact->setFont(contactFont);
            v->addWidget(lblContact);

            // Content row
            QLabel* lblContent = new QLabel(content);
            lblContent->setWordWrap(true);
            v->addWidget(lblContent);

            // Create list item and attach widget
            QListWidgetItem* item = new QListWidgetItem();
            item->setData(Qt::UserRole, id);
            // Ensure the item is selectable and enabled
            item->setFlags(item->flags() | Qt::ItemIsSelectable | Qt::ItemIsEnabled);
            ui->listWidgetMessages->addItem(item);
            ui->listWidgetMessages->setItemWidget(item, w);
            item->setSizeHint(w->sizeHint());
            // Install event filter so clicks on the widget select the list item
            w->installEventFilter(this);
        }
    } else {
        qWarning() << "Failed to load messages:" << q.lastError().text();
    }

    // Connect delete button
    connect(ui->btnDeleteMessage,
            &QPushButton::clicked,
            this,
            &MessageWidget::on_btnDeleteMessage_clicked);
    // Highlight selected item: when selection changes, update widget background
    connect(ui->listWidgetMessages,
            &QListWidget::currentItemChanged,
            this,
            &MessageWidget::on_listWidgetMessages_currentItemChanged);

    // Ensure selection mode is single and widget can get focus
    ui->listWidgetMessages->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->listWidgetMessages->setFocusPolicy(Qt::StrongFocus);
}

/**
 * @brief 析构函数：清理 UI
 */
MessageWidget::~MessageWidget() {
    delete ui;
}

void MessageWidget::on_btnSendMessage_clicked() {
    const QString visitor = ui->lineEditVisitorName->text().trimmed();
    const QString phone = ui->lineEditVisitorPhone->text().trimmed();
    const QString email = ui->lineEditVisitorEmail->text().trimmed();
    const QString message = ui->lineEditNewMessage->text().trimmed();
    if (message.isEmpty()) return;

    // Confirmation dialog
    const QString preview = QString("Visitor: %1\nPhone: %2\nEmail: %3\n\nMessage:\n%4")
                                .arg(visitor, phone, email, message);
    const auto ret = QMessageBox::question(
        this, tr("Confirm Send"), preview, QMessageBox::Yes | QMessageBox::No);
    if (ret != QMessageBox::Yes) return;

    const QString now = QDateTime::currentDateTime().toString(Qt::ISODate);
    QSqlQuery q(Storage::db());
    q.prepare(
        "INSERT INTO messages (visitor_name, visitor_phone, visitor_email, content, created_at) "
        "VALUES (:name, :phone, :email, :content, :created_at)");
    q.bindValue(":name", visitor);
    q.bindValue(":phone", phone);
    q.bindValue(":email", email);
    q.bindValue(":content", message);
    q.bindValue(":created_at", now);
    if (!q.exec()) {
        qWarning() << "Failed to insert message:" << q.lastError().text();
        QMessageBox::warning(
            this,
            tr("Error"),
            tr("Failed to save message to database:\n%1").arg(q.lastError().text()));
        return;
    }

    int id = -1;
    QVariant li = q.lastInsertId();
    if (li.isValid() && !li.isNull()) {
        bool ok = false;
        id = li.toInt(&ok);
        if (!ok) id = -1;
    }
    if (id == -1) {
        // Fallback for SQLite: use last_insert_rowid()
        QSqlQuery q2(Storage::db());
        if (q2.exec("SELECT last_insert_rowid()")) {
            if (q2.next()) id = q2.value(0).toInt();
        }
    }
    // Create a widget for the new message and insert into the list
    QWidget* w = new QWidget();
    w->setAutoFillBackground(true);
    QVBoxLayout* v = new QVBoxLayout(w);
    v->setContentsMargins(6, 6, 6, 6);
    v->setSpacing(4);

    QHBoxLayout* top = new QHBoxLayout();
    QLabel* lblName = new QLabel(visitor.isEmpty() ? tr("(Anonymous)") : visitor);
    QFont bold = lblName->font();
    bold.setBold(true);
    lblName->setFont(bold);
    top->addWidget(lblName, 1);

    QLabel* lblTime = new QLabel(now);
    lblTime->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    QFont small = lblTime->font();
    small.setPointSizeF(small.pointSizeF() - 1.0);
    lblTime->setFont(small);
    top->addWidget(lblTime, 0);

    v->addLayout(top);

    QLabel* lblContact = new QLabel(QString("%1 | %2").arg(phone.isEmpty() ? tr("-") : phone,
                                                           email.isEmpty() ? tr("-") : email));
    QFont contactFont = lblContact->font();
    contactFont.setPointSizeF(contactFont.pointSizeF() - 0.5);
    lblContact->setFont(contactFont);
    v->addWidget(lblContact);

    QLabel* lblContent = new QLabel(message);
    lblContent->setWordWrap(true);
    v->addWidget(lblContent);

    QListWidgetItem* item = new QListWidgetItem();
    item->setData(Qt::UserRole, id);
    item->setFlags(item->flags() | Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    ui->listWidgetMessages->addItem(item);
    ui->listWidgetMessages->setItemWidget(item, w);
    item->setSizeHint(w->sizeHint());
    w->installEventFilter(this);

    // Clear inputs after send
    ui->lineEditNewMessage->clear();
    ui->lineEditVisitorName->clear();
    ui->lineEditVisitorPhone->clear();
    ui->lineEditVisitorEmail->clear();
}

void MessageWidget::on_btnDeleteMessage_clicked() {
    auto* item = ui->listWidgetMessages->currentItem();
    if (!item) {
        QMessageBox::information(this, tr("Delete"), tr("Please select a message to delete."));
        return;
    }

    const int id = item->data(Qt::UserRole).toInt();
    const auto ret = QMessageBox::question(this,
                                           tr("Confirm Delete"),
                                           tr("Delete selected message?"),
                                           QMessageBox::Yes | QMessageBox::No);
    if (ret != QMessageBox::Yes) return;

    QSqlQuery q(Storage::db());
    q.prepare("DELETE FROM messages WHERE id = :id");
    q.bindValue(":id", id);
    if (!q.exec()) {
        qWarning() << "Failed to delete message:" << q.lastError().text();
        QMessageBox::warning(this, tr("Error"), tr("Failed to delete message from database."));
        return;
    }

    // 从 UI 列表中移除并释放对应的 widget 与 item
    const int row = ui->listWidgetMessages->row(item);
    QWidget* w = ui->listWidgetMessages->itemWidget(item);
    QListWidgetItem* taken = ui->listWidgetMessages->takeItem(row);
    if (w) delete w;
    if (taken) delete taken;
}

void MessageWidget::on_btnBackFromMessage_clicked() {
    emit backToMain();
}

void MessageWidget::on_listWidgetMessages_currentItemChanged(QListWidgetItem* current,
                                                             QListWidgetItem* previous) {
    // Reset previous widget style
    if (previous) {
        QWidget* pw = ui->listWidgetMessages->itemWidget(previous);
        if (pw) { pw->setStyleSheet(""); }
    }

    if (current) {
        QWidget* cw = ui->listWidgetMessages->itemWidget(current);
        if (cw) {
            // Use palette highlight color for selected background and ensure text readable
            const QColor bg = palette().color(QPalette::Highlight);
            const QColor fg = palette().color(QPalette::HighlightedText);
            cw->setStyleSheet(
                QString("background-color: %1; color: %2;").arg(bg.name(), fg.name()));
        }
    }
}
/**
 * @brief 事件过滤器：把对 item widget 的点击映射为列表项选择
 * @note 这样可以在点击自定义 widget 的任意位置时，触发 QListWidget 的选中与高亮行为
 */
bool MessageWidget::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::MouseButtonPress) {
        // Find which item has this widget
        for (int i = 0; i < ui->listWidgetMessages->count(); ++i) {
            QListWidgetItem* it = ui->listWidgetMessages->item(i);
            QWidget* w = ui->listWidgetMessages->itemWidget(it);
            if (w == watched) {
                ui->listWidgetMessages->setCurrentItem(it);
                ui->listWidgetMessages->setFocus();
                break;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}
