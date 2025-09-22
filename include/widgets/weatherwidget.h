#ifndef WEATHERWIDGET_H
#define WEATHERWIDGET_H

#include <QWidget>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrl>

QT_BEGIN_NAMESPACE
namespace Ui {
class WeatherWidget;
}
QT_END_NAMESPACE

class WeatherWidget : public QWidget {
    Q_OBJECT

public:
    WeatherWidget(QWidget *parent = nullptr);
    ~WeatherWidget();

private slots:
    void onWeatherDataReceived(QNetworkReply *reply);
    void updateWeather();

private:
    Ui::WeatherWidget *ui;
    QNetworkAccessManager *networkManager;
    void parseWeatherData(const QString &data);
};

#endif // WEATHERWIDGET_H
