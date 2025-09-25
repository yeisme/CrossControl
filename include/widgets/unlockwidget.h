#ifndef UNLOCKWIDGET_H
#define UNLOCKWIDGET_H

#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui {
class UnlockWidget;
}
QT_END_NAMESPACE

class UnlockWidget : public QWidget {
    Q_OBJECT

   public:
    UnlockWidget(QWidget* parent = nullptr);
    ~UnlockWidget();

   signals:
    void cancelUnlock();

   private slots:
    void on_btnConfirmUnlock_clicked();
    void on_btnCancelUnlock_clicked();

   private:
    Ui::UnlockWidget* ui;
};

#endif  // UNLOCKWIDGET_H