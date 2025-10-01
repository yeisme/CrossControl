#pragma once

#include <ui_devicemanagementwidget.h>

#include <QWidget>

class QTableWidget;
class QPushButton;
class QTableWidgetItem;
class QLineEdit;
class QTextEdit;

namespace DeviceGateway {
class DeviceGateway;
}

/**
 * @brief 设备管理页面
 *
 */
class DeviceManagementWidget : public QWidget {
    Q_OBJECT
   public:
    explicit DeviceManagementWidget(DeviceGateway::DeviceGateway* gateway,
                                    QWidget* parent = nullptr);
    ~DeviceManagementWidget();

   private:
    Ui::DeviceManagementWidget* ui;
    DeviceGateway::DeviceGateway* gateway_{nullptr};

    // UI controls created programmatically
    QTableWidget* table_ = nullptr;      // 左边的设备表
    QPushButton* btnAdd_ = nullptr;      // 添加设备按钮
    QPushButton* btnEdit_ = nullptr;     // 编辑设备按钮
    QPushButton* btnDelete_ = nullptr;   // 删除设备按钮
    QPushButton* btnImport_ = nullptr;   // 导入设备按钮
    QPushButton* btnExport_ = nullptr;   // 导出设备按钮
    QPushButton* btnRefresh_ = nullptr;  // 刷新设备按钮
    QPushButton* btnTest_ = nullptr;     // 测试设备按钮
    QPushButton* btnRest_ = nullptr;     // 重置设备按钮
    QLineEdit* leFilter_ = nullptr;      // 设备过滤器
    QWidget* chartWidget_ = nullptr;     // 右上角的图表区域
    QTextEdit* teTestResults_ = nullptr;
    QWidget* summaryWidget_ = nullptr;  // 纯部件摘要（条形图）

   private slots:
    void refresh();
    void onFilterChanged();
    void onAdd();
    void onEdit();
    void onDelete();
    void onImport();
    void onExport();
    void onTestSelected();
    void updateChart();
    void appendTestResult(const QString& txt);
    void onToggleRest();
    void updateRestButton();
};
