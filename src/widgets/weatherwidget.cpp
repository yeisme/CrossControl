#include "weatherwidget.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPixmap>
#include <QTimer>

#include "spdlog/spdlog.h"
#include "ui_weatherwidget.h"

WeatherWidget::WeatherWidget(QWidget* parent) : QWidget(parent), ui(new Ui::WeatherWidget) {
    ui->setupUi(this);
    networkManager = new QNetworkAccessManager(this);
    // 完成请求执行展示
    connect(networkManager,
            &QNetworkAccessManager::finished,
            this,
            &WeatherWidget::onWeatherDataReceived);

    spdlog::debug("WeatherWidget constructed, starting periodic updates");

    // 定时更新天气，每小时更新一次
    QTimer* timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &WeatherWidget::updateWeather);
    timer->start(3600000);  // 1小时 = 3600000毫秒

    updateWeather();  // 初始更新
}

WeatherWidget::~WeatherWidget() {
    delete ui;
}

/**
 * @brief 通过网络请求更新天气信息，目前从 wttr.in 获取天气数据
 * @note TODO: 通过和风天气等更专业的天气 API 获取更详细的天气和空气质量数据
 *
 */
void WeatherWidget::updateWeather() {
    // 使用 wttr.in API 获取广州的天气（可以根据需要修改城市）
    QNetworkRequest request(QUrl("https://wttr.in/Guangzhou?format=j1"));
    spdlog::debug("WeatherWidget: requesting weather from {}",
                  request.url().toString().toStdString());
    networkManager->get(request);
}

void WeatherWidget::onWeatherDataReceived(QNetworkReply* reply) {
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray data = reply->readAll();
        QString jsonString = QString::fromUtf8(data);
        parseWeatherData(jsonString);
    } else {
        ui->labelWeather->setText(
            QCoreApplication::translate("WeatherWidget", "Failed to fetch weather"));
        spdlog::warn("WeatherWidget: network error: {}", reply->errorString().toStdString());
    }
    reply->deleteLater();
}

//
void WeatherWidget::parseWeatherData(const QString& data) {
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
                    weatherDesc = QCoreApplication::translate("WeatherWidget", "Unknown");
                }
                QString airQuality = QCoreApplication::translate(
                    "WeatherWidget",
                    "Air quality: Unknown");  // wttr.in 可能不提供空气质量，这里简化

                QString weatherText =
                    QString("%1℃~%2℃ %3 %4").arg(tempMin, tempMax, weatherDesc, airQuality);
                if (ui && ui->labelWeather) ui->labelWeather->setText(weatherText);

                // Choose an icon based on weather description keywords
                QString descLower = weatherDesc.toLower();
                QString iconPath;
                if (descLower.contains("sun") || descLower.contains("clear") ||
                    descLower.contains("hot")) {
                    iconPath = ":/icons/weather_sunny.svg";
                } else if (descLower.contains("rain") || descLower.contains("shower") ||
                           descLower.contains("drizzle")) {
                    iconPath = ":/icons/weather_rainy.svg";
                } else if (descLower.contains("cloud") || descLower.contains("overcast") ||
                           descLower.contains("fog")) {
                    iconPath = ":/icons/weather_cloudy.svg";
                } else {
                    // night fallback if contains night or moon
                    if (descLower.contains("night") || descLower.contains("moon"))
                        iconPath = ":/icons/weather_night.svg";
                    else
                        iconPath = ":/icons/weather_sunny.svg";  // default
                }

                QLabel* iconLbl = this->findChild<QLabel*>("labelIcon");
                if (iconLbl) {
                    QPixmap pm(iconPath);
                    if (!pm.isNull()) {
                        iconLbl->setPixmap(
                            pm.scaled(48, 48, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                    }
                }
            } else {
                ui->labelWeather->setText(
                    QCoreApplication::translate("WeatherWidget", "No weather data"));
            }
        } else {
            ui->labelWeather->setText(
                QCoreApplication::translate("WeatherWidget", "Failed to parse weather data"));
        }
    } catch (...) {
        ui->labelWeather->setText(
            QCoreApplication::translate("WeatherWidget", "Weather data error"));
    }
}
