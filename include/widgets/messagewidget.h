#ifndef MESSAGEWIDGET_H
#define MESSAGEWIDGET_H

#include <QWidget>

QT_BEGIN_NAMESPACE

namespace Ui {
class MessageWidget;
}

QT_END_NAMESPACE

class MessageWidget : public QWidget {
    Q_OBJECT

public:
    explicit MessageWidget(QWidget* parent = nullptr);
    ~MessageWidget();

signals:
    void backToMain();

private slots:
    void on_btnSendMessage_clicked();
    void on_btnBackFromMessage_clicked();

private:
    Ui::MessageWidget* ui;
};

#endif  // MESSAGEWIDGET_H