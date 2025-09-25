#ifndef MONITORWIDGET_H
#define MONITORWIDGET_H

#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui {
class MonitorWidget;
}
QT_END_NAMESPACE

class MonitorWidget : public QWidget {
    Q_OBJECT

   public:
    MonitorWidget(QWidget* parent = nullptr);
    ~MonitorWidget();

   signals:
    void backToMain();

   private slots:
    void on_btnBackFromMonitor_clicked();

   private:
    Ui::MonitorWidget* ui;
};

#endif  // MONITORWIDGET_H