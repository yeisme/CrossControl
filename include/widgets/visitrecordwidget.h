#ifndef VISITRECORDWIDGET_H
#define VISITRECORDWIDGET_H

#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui {
class VisitRecordWidget;
}
QT_END_NAMESPACE

class VisitRecordWidget : public QWidget {
    Q_OBJECT

   public:
    VisitRecordWidget(QWidget* parent = nullptr);
    ~VisitRecordWidget();

   signals:
    void backToMain();

   private slots:
    void on_btnBackFromVisit_clicked();

   private:
    Ui::VisitRecordWidget* ui;
};

#endif  // VISITRECORDWIDGET_H