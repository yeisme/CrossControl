#pragma once

#include <QWidget>
#include <ui_devicemanagementwidget.h>

namespace DeviceGateway { class DeviceGateway; }

class DeviceManagementWidget : public QWidget {
    Q_OBJECT
public:
    explicit DeviceManagementWidget(DeviceGateway::DeviceGateway* gateway, QWidget* parent = nullptr);
    ~DeviceManagementWidget();

private:
    Ui::DeviceManagementWidget* ui;
    DeviceGateway::DeviceGateway* gateway_{nullptr};
};
