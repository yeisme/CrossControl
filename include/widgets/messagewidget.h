#ifndef MESSAGEWIDGET_H
#define MESSAGEWIDGET_H

#include <QWidget>

QT_BEGIN_NAMESPACE

namespace Ui {
class MessageWidget;
}

QT_END_NAMESPACE

class QListWidgetItem;

/**
 * @brief 留言页面控件
 *
 * 该类封装了留言板的 UI 与功能：
 * - 显示历史留言（姓名、电话、邮箱、时间、内容）
 * - 新增留言（带确认对话框）
 * - 删除选中留言
 *
 * 数据库存取通过项目的 Storage 模块提供的 QSqlDatabase 完成。
 */
class MessageWidget : public QWidget {
    Q_OBJECT

   public:
    /**
     * @brief 构造函数
     * @param parent 父 widget，默认 nullptr
     *
     * 构造时会懒加载并初始化数据库（如果尚未初始化），
     * 并加载 messages 表中已有的留言到列表中。
     */
    explicit MessageWidget(QWidget* parent = nullptr);

    /**
     * @brief 析构函数
     *
     * 释放 UI 相关资源。
     */
    ~MessageWidget();

   signals:
    /**
     * @brief 请求返回主界面（由上层 slot/信号连接使用）
     */
    void backToMain();

   private slots:
    /**
     * @brief 发送留言按钮点击处理
     *
     * 会弹出确认对话框，确认后将留言写入数据库并在 UI 列表中追加条目。
     */
    void on_btnSendMessage_clicked();

    /**
     * @brief 删除选中留言按钮点击处理
     *
     * 会删除数据库中的对应记录并在 UI 中移除列表项。
     */
    void on_btnDeleteMessage_clicked();

    /**
     * @brief 返回主界面按钮点击处理
     */
    void on_btnBackFromMessage_clicked();

    /**
     * @brief 列表当前项变化处理
     * @param current 当前选中项（可能为 nullptr）
     * @param previous 之前的选中项（可能为 nullptr）
     *
     * 该槽用于更新自定义 item widget 的选中样式（高亮）。
     */
    void on_listWidgetMessages_currentItemChanged(QListWidgetItem* current,
                                                  QListWidgetItem* previous);

   private:
    Ui::MessageWidget* ui;

    /**
     * @brief 事件过滤器（用于让点击 item widget 时选中对应的 QListWidgetItem）
     * @param watched 被过滤的对象
     * @param event 发生的事件
     * @return 如果事件被处理返回 true，否则返回 false
     */
    bool eventFilter(QObject* watched, QEvent* event) override;
};

#endif  // MESSAGEWIDGET_H