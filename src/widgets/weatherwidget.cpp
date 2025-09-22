#include "weatherwidget.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>

#include "ui_weatherwidget.h"

WeatherWidget::WeatherWidget(QWidget *parent)
    : QWidget(parent), ui(new Ui::WeatherWidget) {
    ui->setupUi(this);
    networkManager = new QNetworkAccessManager(this);
    // 完成请求执行展示
    connect(networkManager,
            &QNetworkAccessManager::finished,
            this,
            &WeatherWidget::onWeatherDataReceived);

    // 定时更新天气，每小时更新一次
    QTimer *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &WeatherWidget::updateWeather);
    timer->start(3600000);  // 1小时 = 3600000毫秒

    updateWeather();  // 初始更新
}

WeatherWidget::~WeatherWidget() { delete ui; }

void WeatherWidget::updateWeather() {
    // 使用 wttr.in API 获取广州的天气（可以根据需要修改城市）
    QNetworkRequest request(QUrl("https://wttr.in/Guangzhou?format=j1"));
    networkManager->get(request);
}

void WeatherWidget::onWeatherDataReceived(QNetworkReply *reply) {
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray data = reply->readAll();
        QString jsonString = QString::fromUtf8(data);
        parseWeatherData(jsonString);
    } else {
        ui->labelWeather->setText("获取天气失败");
    }
    reply->deleteLater();
}

void WeatherWidget::parseWeatherData(const QString &data) {
    try {
        QJsonDocument doc = QJsonDocument::fromJson(data.toUtf8());
        if (!doc.isNull() && doc.isObject()) {
            QJsonObject obj = doc.object();
            QJsonArray weatherArray = obj["weather"].toArray();
            if (!weatherArray.isEmpty()) {
                QJsonObject currentWeather = weatherArray[0].toObject();
                QString tempMin = currentWeather["mintempC"].toString();
                QString tempMax = currentWeather["maxtempC"].toString();
                QString weatherDesc;
                QJsonArray descArray = currentWeather["weatherDesc"].toArray();
                if (!descArray.isEmpty()) {
                    weatherDesc = descArray[0].toObject()["value"].toString();
                } else {
                    weatherDesc = "未知";
                }
                QString airQuality =
                    "空气质量：未知";  // wttr.in 可能不提供空气质量，这里简化

                QString weatherText =
                    QString("%1℃~%2℃ %3 %4")
                        .arg(tempMin, tempMax, weatherDesc, airQuality);
                ui->labelWeather->setText(weatherText);
            } else {
                ui->labelWeather->setText("无天气数据");
            }
        } else {
            ui->labelWeather->setText("解析天气数据失败");
        }
    } catch (...) {
        ui->labelWeather->setText("天气数据异常");
    }
}
