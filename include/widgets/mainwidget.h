#ifndef MAINWIDGET_H
#define MAINWIDGET_H

#include <QTimer>
#include <QWidget>

#include "weatherwidget.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWidget;
}
QT_END_NAMESPACE

class MainWidget : public QWidget {
    Q_OBJECT

   public:
    MainWidget(QWidget* parent = nullptr);
    ~MainWidget();

   private:
    Ui::MainWidget* ui;
    WeatherWidget* weatherWidget;  // 天气组件
    void updateVisitorStats();
    // real-time clock
    QTimer* dateTimeTimer = nullptr;
    void updateDateTime();
};

#endif  // MAINWIDGET_H
