#include "mainwidget.h"

#include <QDate>
#include <QDateTime>
#include <QFont>
#include <QLabel>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QStringList>
#include <QTimer>
#include <QVBoxLayout>

#include "modules/Storage/storage.h"
#include "spdlog/spdlog.h"
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
    // 将天气组件替换原来的 labelWeather（如果存在）
    QLabel* oldWeather = this->findChild<QLabel*>("labelWeather");
    if (oldWeather) {
        QWidget* parentWidget = oldWeather->parentWidget();
        if (parentWidget) {
            QLayout* parentLayout = parentWidget->layout();
            if (parentLayout) {
                parentLayout->replaceWidget(oldWeather, weatherWidget);
                delete oldWeather;
            }
        }
    }

    // 作为 Dashboard 展示，功能按钮已被放入卡片布局，无需隐藏原滚动区。
    // 清除 .ui 中设置的内联 styleSheet（白底/边框），以便全局 QSS 生效
    auto clearInlineStyles = [this](const QStringList& names) {
        for (const QString& n : names) {
            QWidget* w = this->findChild<QWidget*>(n);
            if (w) w->setStyleSheet(QString());
        }
    };

    clearInlineStyles({"frameWeather",
                       "frameStats",
                       "frameFunctions",
                       "visitorStatsWidget",
                       // buttons
                       "btnMonitor",
                       "btnUnlock",
                       "btnVisitRecord",
                       "btnMessage",
                       "btnSetting",
                       "btnLogout",
                       "btnLogs",
                       // labels that had inline styles
                       "labelDateTime",
                       "labelWeather"});

    spdlog::debug("MainWidget constructed");

    // Start real-time clock (update every second)
    dateTimeTimer = new QTimer(this);
    connect(dateTimeTimer, &QTimer::timeout, this, &MainWidget::updateDateTime);
    dateTimeTimer->start(1000);
    updateDateTime();

    // Fill visitor stats
    try {
        updateVisitorStats();
    } catch (const std::exception& ex) {
        spdlog::warn("Failed to update visitor stats: {}", ex.what());
    }

    // Ensure visitor stats have a unified title above the three metrics
    QWidget* visitorStats = this->findChild<QWidget*>("visitorStatsWidget");
    if (visitorStats) {
        // Build a new vertical layout: [title]
        //                           [hbox: today | week | total]
        // Gather existing labels by objectName and put them into a new HBox so
        // the title will be shown above all three items (avoid nesting old layout)
        QLabel* lblToday = visitorStats->findChild<QLabel*>("labelVisitorsToday");
        QLabel* lblWeek = visitorStats->findChild<QLabel*>("labelVisitorsWeek");
        QLabel* lblTotal = visitorStats->findChild<QLabel*>("labelVisitorsTotal");

        QHBoxLayout* h = new QHBoxLayout();
        h->setContentsMargins(0, 0, 0, 0);
        h->setSpacing(12);

        // Instead of reparenting existing labels (which may lead to layout
        // issues), create fresh QLabel instances with the same objectName so
        // updateVisitorStats() continues to find them. Remove old widgets
        // from the previous layout if present.
        if (lblToday) {
            if (QLayout* ol =
                    lblToday->parentWidget() ? lblToday->parentWidget()->layout() : nullptr) {
                ol->removeWidget(lblToday);
            }
            delete lblToday;
        }
        if (lblWeek) {
            if (QLayout* ol =
                    lblWeek->parentWidget() ? lblWeek->parentWidget()->layout() : nullptr) {
                ol->removeWidget(lblWeek);
            }
            delete lblWeek;
        }
        if (lblTotal) {
            if (QLayout* ol =
                    lblTotal->parentWidget() ? lblTotal->parentWidget()->layout() : nullptr) {
                ol->removeWidget(lblTotal);
            }
            delete lblTotal;
        }

        // Create new labels (same object names) so updateVisitorStats() can find them
        QLabel* newToday = new QLabel(tr("Today: 0"), visitorStats);
        newToday->setObjectName("labelVisitorsToday");
        newToday->setAlignment(Qt::AlignCenter);
        QLabel* newWeek = new QLabel(tr("Week: 0"), visitorStats);
        newWeek->setObjectName("labelVisitorsWeek");
        newWeek->setAlignment(Qt::AlignCenter);
        QLabel* newTotal = new QLabel(tr("Total: 0"), visitorStats);
        newTotal->setObjectName("labelVisitorsTotal");
        newTotal->setAlignment(Qt::AlignCenter);

        h->addWidget(newToday, 1);
        h->addWidget(newWeek, 1);
        h->addWidget(newTotal, 1);

        // Create title and vbox
        QVBoxLayout* v = new QVBoxLayout(visitorStats);
        v->setContentsMargins(0, 0, 0, 0);
        v->setSpacing(6);
        QLabel* title = new QLabel(tr("Visitor Statistics"), visitorStats);
        title->setObjectName("labelVisitorTitle");
        QFont f = title->font();
        f.setPointSize(12);
        f.setBold(true);
        title->setFont(f);
        title->setAlignment(Qt::AlignCenter);
        v->addWidget(title, 0, Qt::AlignHCenter);
        v->addLayout(h);

        // If the widget previously had a layout, delete it to avoid leaks
        QLayout* oldLayout = visitorStats->layout();
        if (oldLayout && oldLayout != v) {
            // take items from oldLayout if any left (safety) then delete
            delete oldLayout;
        }

        visitorStats->setLayout(v);

        // After adding the title, refresh the stats so labels are updated
        try {
            updateVisitorStats();
        } catch (const std::exception& ex) {
            spdlog::warn("Failed to refresh visitor stats after inserting title: {}", ex.what());
        }
    }
}

void MainWidget::updateVisitorStats() {
    // Use messages table as proxy for visits if no dedicated visits table exists
    QSqlDatabase& db = storage::Storage::db();
    // If DB not available, show zeros as fallback rather than leaving labels blank
    if (!db.isOpen()) {
        QLabel* lblTotal = this->findChild<QLabel*>("labelVisitorsTotal");
        QLabel* lblToday = this->findChild<QLabel*>("labelVisitorsToday");
        QLabel* lblWeek = this->findChild<QLabel*>("labelVisitorsWeek");
        if (lblTotal) lblTotal->setText(tr("Total: %1").arg(0));
        if (lblToday) lblToday->setText(tr("Today: %1").arg(0));
        if (lblWeek) lblWeek->setText(tr("Week: %1").arg(0));
        return;
    }

    // Total
    QSqlQuery qTotal(db);
    if (qTotal.exec("SELECT COUNT(*) FROM messages")) {
        if (qTotal.next()) {
            int total = qTotal.value(0).toInt();
            QLabel* lblTotal = this->findChild<QLabel*>("labelVisitorsTotal");
            if (lblTotal) lblTotal->setText(tr("Total: %1").arg(total));
        }
    }

    // Today
    QDate today = QDate::currentDate();
    QString startToday = today.toString("yyyy-MM-dd") + " 00:00:00";
    QString endToday = today.toString("yyyy-MM-dd") + " 23:59:59";
    QSqlQuery qToday(db);
    qToday.prepare("SELECT COUNT(*) FROM messages WHERE created_at BETWEEN :start AND :end");
    qToday.bindValue(":start", startToday);
    qToday.bindValue(":end", endToday);
    if (qToday.exec() && qToday.next()) {
        int todayCount = qToday.value(0).toInt();
        QLabel* lblToday = this->findChild<QLabel*>("labelVisitorsToday");
        if (lblToday) lblToday->setText(tr("Today: %1").arg(todayCount));
    }

    // Week (last 7 days)
    QDate weekStart = today.addDays(-6);
    QString startWeek = weekStart.toString("yyyy-MM-dd") + " 00:00:00";
    QSqlQuery qWeek(db);
    qWeek.prepare("SELECT COUNT(*) FROM messages WHERE created_at BETWEEN :start AND :end");
    qWeek.bindValue(":start", startWeek);
    qWeek.bindValue(":end", endToday);
    if (qWeek.exec() && qWeek.next()) {
        int weekCount = qWeek.value(0).toInt();
        QLabel* lblWeek = this->findChild<QLabel*>("labelVisitorsWeek");
        if (lblWeek) lblWeek->setText(tr("Week: %1").arg(weekCount));
    }
}

MainWidget::~MainWidget() {
    delete ui;
}

void MainWidget::updateDateTime() {
    const QDateTime now = QDateTime::currentDateTime();
    // Format: HH:mm yyyy/MM/dd dddd (localized weekday)
    const QString text = now.toString("HH:mm yyyy/MM/dd dddd");
    QLabel* lbl = this->findChild<QLabel*>("labelDateTime");
    if (lbl) lbl->setText(text);
}
