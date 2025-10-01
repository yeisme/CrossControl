#pragma once

#include <QDialog>

#include "modules/DeviceGateway/device_registry.h"

class QLineEdit;
class QTextEdit;

/**
 * @brief 编辑设备信息的对话框。
 *
 */
class DeviceEditDialog : public QDialog {
    Q_OBJECT
   public:
    explicit DeviceEditDialog(QWidget* parent = nullptr);
    void setDevice(const DeviceGateway::DeviceInfo& d);
    DeviceGateway::DeviceInfo device() const;

   private:
    QLineEdit* leId_ = nullptr;
    QLineEdit* leName_ = nullptr;
    QLineEdit* leStatus_ = nullptr;
    QLineEdit* leEndpoint_ = nullptr;
    QLineEdit* leType_ = nullptr;
    QLineEdit* leHw_ = nullptr;
    QLineEdit* leFw_ = nullptr;
    QLineEdit* leOwner_ = nullptr;
    QLineEdit* leGroup_ = nullptr;
    QTextEdit* teMeta_ = nullptr;
};
