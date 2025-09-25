#ifndef LOGINWIDGET_H
#define LOGINWIDGET_H

#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui {
class LoginWidget;
}
QT_END_NAMESPACE

class LoginWidget : public QWidget {
    Q_OBJECT

   public:
    LoginWidget(QWidget* parent = nullptr);
    ~LoginWidget();

   signals:
    void loginSuccess();

   private slots:
    void on_pushButtonLogin_clicked();
    void on_pushButtonRegister_clicked();

    // Realtime validation

    void on_lineEditLoginEmail_textChanged(const QString& text);
    void on_lineEditLoginPassword_textChanged(const QString& text);
    void on_lineEditRegisterEmail_textChanged(const QString& text);
    void on_lineEditRegisterPassword_textChanged(const QString& text);
    void on_lineEditRegisterPasswordConfirm_textChanged(const QString& text);

   private:
    Ui::LoginWidget* ui;

    /**
     * @brief Check if the email is valid
     *
     * @param email
     * @return true
     * @return false
     */
    bool isEmailValid(const QString& email) const;

    /**
     * @brief Check if the password is valid
     *
     * @param password exactly 8 chars
     * @return true
     * @return false
     */
    bool isPasswordValid(const QString& password) const;

    /**
     * @brief Update the state of the login button based on the current input
     *
     */
    void updateLoginButtonState();

    /**
     * @brief Update the state of the register button based on the current input
     *
     */
    void updateRegisterButtonState();
    QString hashPassword(const QString& password) const;

    /**
     * @brief 保存账号信息
     *
     * @param email
     * @param passwordHash
     */
    void saveAccount(const QString& email, const QString& passwordHash);

    /**
     * @brief 加载账号信息，用于登陆时验证
     *
     * @param emailOut
     * @param passwordHashOut
     * @return true if account exists
     * @return false if no account
     */
    bool loadAccount(QString& emailOut, QString& passwordHashOut) const;

    // Helpers for error display
    void showLoginError(const QString& msg);
    void clearLoginError();
    void showRegisterError(const QString& msg);
    void clearRegisterError();
};

#endif  // LOGINWIDGET_H