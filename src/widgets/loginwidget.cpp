#include "loginwidget.h"

#include <qcoreapplication.h>
#include <qwindowdefs_win.h>

#include <QAction>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QRegularExpression>
#include <QSettings>
#include <QVariant>

#include "ui_loginwidget.h"

LoginWidget::LoginWidget(QWidget* parent) : QWidget(parent), ui(new Ui::LoginWidget) {
    ui->setupUi(this);
    // 在左侧 frame 动态添加品牌图标
    if (auto frame = findChild<QFrame*>(QStringLiteral("frameLeft"))) {
        if (auto vlay = frame->findChild<QVBoxLayout*>(QStringLiteral("verticalLayoutLeft"))) {
            auto* logo = new QLabel(frame);
            logo->setObjectName("labelBrandIcon");
            logo->setAlignment(Qt::AlignHCenter);
            logo->setPixmap(QIcon(":/icons/icons/brand.svg").pixmap(72, 72));
            vlay->insertWidget(1, logo);  // 放在顶部 spacer 与文字之间
        }
    }

    // Connect real-time validation
    connect(ui->lineEditLoginEmail,
            &QLineEdit::textChanged,
            this,
            &LoginWidget::on_lineEditLoginEmail_textChanged);
    connect(ui->lineEditLoginPassword,
            &QLineEdit::textChanged,
            this,
            &LoginWidget::on_lineEditLoginPassword_textChanged);
    connect(ui->lineEditRegisterEmail,
            &QLineEdit::textChanged,
            this,
            &LoginWidget::on_lineEditRegisterEmail_textChanged);
    connect(ui->lineEditRegisterPassword,
            &QLineEdit::textChanged,
            this,
            &LoginWidget::on_lineEditRegisterPassword_textChanged);

    // 可选：在用户界面更新之前，确认密码字段可能不存在；稍后通过 QObject::findChild 进行保护。
    if (auto confirm = findChild<QLineEdit*>(QStringLiteral("lineEditRegisterPasswordConfirm"));
        confirm) {
        connect(confirm,
                &QLineEdit::textChanged,
                this,
                &LoginWidget::on_lineEditRegisterPasswordConfirm_textChanged);
    }

    // 如果存在，设置密码可见性切换操作
    auto setupPasswordToggle = [](QLineEdit* le) {
        if (!le) return;
        auto makeIcon = [le]() {
            QColor base = le->palette().color(QPalette::Text);
            if (base.lightness() < 128)
                base = base.lighter(160);
            else
                base = base.darker(140);
            QPixmap pm(18, 18);
            pm.fill(Qt::transparent);
            QPainter p(&pm);
            p.setRenderHint(QPainter::Antialiasing, true);
            QPen pen(base);
            pen.setWidth(2);
            p.setPen(pen);
            QRectF r(3, 5, 12, 8);
            p.drawArc(r, 30 * 16, 120 * 16);
            p.drawArc(r, (180 + 30) * 16, 120 * 16);
            p.setBrush(base);
            p.drawEllipse(QPointF(9, 9), 2.4, 2.4);
            return QIcon(pm);
        };
        auto action = le->addAction(makeIcon(), QLineEdit::TrailingPosition);
        action->setCheckable(true);
        QObject::connect(action, &QAction::toggled, le, [le](bool checked) {
            le->setEchoMode(checked ? QLineEdit::Normal : QLineEdit::Password);
        });
        action->setToolTip(QCoreApplication::translate("LoginWidget", "Show/Hide password"));
    };
    setupPasswordToggle(ui->lineEditLoginPassword);
    setupPasswordToggle(ui->lineEditRegisterPassword);
    if (auto confirm = findChild<QLineEdit*>(QStringLiteral("lineEditRegisterPasswordConfirm"));
        confirm) {
        setupPasswordToggle(confirm);
    }

    // Pre-fill login email if we have a stored one
    QString savedEmail, savedHash;
    if (loadAccount(savedEmail, savedHash)) { ui->lineEditLoginEmail->setText(savedEmail); }

    updateLoginButtonState();
    updateRegisterButtonState();
}

LoginWidget::~LoginWidget() {
    delete ui;
}

void LoginWidget::on_pushButtonLogin_clicked() {
    QString email = ui->lineEditLoginEmail->text().trimmed();
    QString password = ui->lineEditLoginPassword->text();

    if (!isEmailValid(email) || !isPasswordValid(password)) {
        showLoginError(QCoreApplication::translate(
            "LoginWidget", "Please enter a valid email and a password with at least 8 characters"));
        return;  // Buttons should already be disabled, just guard
    }

    QString savedEmail, savedHash;
    if (!loadAccount(savedEmail, savedHash)) {
        // 弹窗提示未注册
        QMessageBox::warning(
            this,
            QCoreApplication::translate("LoginWidget", "Login Failed"),
            QCoreApplication::translate("LoginWidget",
                                        "This email is not registered. Please register first."));
        showLoginError(QCoreApplication::translate("LoginWidget", "Email not registered"));
        return;
    }

    if (email.compare(savedEmail, Qt::CaseInsensitive) != 0) {
        QMessageBox::warning(this,
                             QCoreApplication::translate("LoginWidget", "Login Failed"),
                             QCoreApplication::translate(
                                 "LoginWidget", "Email does not match the registered account."));
        showLoginError(QCoreApplication::translate("LoginWidget", "Email mismatch"));
        return;
    }

    const QString inputHash = hashPassword(password);
    if (inputHash == savedHash) {
        clearLoginError();
        emit loginSuccess();
    } else {
        QMessageBox::warning(this, tr("Login Failed"), tr("Incorrect password. Please try again."));
        showLoginError(tr("Incorrect password"));
    }
}

void LoginWidget::on_pushButtonRegister_clicked() {
    const QString email = ui->lineEditRegisterEmail->text().trimmed();
    const QString password = ui->lineEditRegisterPassword->text();
    const QLineEdit* confirm =
        findChild<QLineEdit*>(QStringLiteral("lineEditRegisterPasswordConfirm"));
    const QString confirmPwd = confirm ? confirm->text() : QString();

    if (!isEmailValid(email) || !isPasswordValid(password)) {
        showRegisterError(QCoreApplication::translate(
            "LoginWidget", "Please enter a valid email and a password with at least 8 characters"));
        return;
    }

    if (confirm && password != confirmPwd) {
        QMessageBox::warning(this,
                             QCoreApplication::translate("LoginWidget", "Registration Failed"),
                             QCoreApplication::translate("LoginWidget", "Passwords do not match."));
        showRegisterError(QCoreApplication::translate("LoginWidget", "Passwords do not match"));
        return;
    }

    const QString pwdHash = hashPassword(password);
    saveAccount(email, pwdHash);

    // Auto switch to login and prefill
    ui->lineEditLoginEmail->setText(email);
    ui->lineEditLoginPassword->setText(password);
    updateLoginButtonState();
    clearRegisterError();
    emit loginSuccess();
}

// Realtime validation slots
void LoginWidget::on_lineEditLoginEmail_textChanged(const QString& text) {
    Q_UNUSED(text);
    updateLoginButtonState();
}
void LoginWidget::on_lineEditLoginPassword_textChanged(const QString& text) {
    Q_UNUSED(text);
    updateLoginButtonState();
}
void LoginWidget::on_lineEditRegisterEmail_textChanged(const QString& text) {
    Q_UNUSED(text);
    updateRegisterButtonState();
}
void LoginWidget::on_lineEditRegisterPassword_textChanged(const QString& text) {
    Q_UNUSED(text);
    updateRegisterButtonState();
}
void LoginWidget::on_lineEditRegisterPasswordConfirm_textChanged(const QString& text) {
    Q_UNUSED(text);
    updateRegisterButtonState();
}

// 邮箱正则验证
bool LoginWidget::isEmailValid(const QString& email) const {
    static const QRegularExpression re(R"(^[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}$)");
    return re.match(email).hasMatch();
}

// 密码至少8位
bool LoginWidget::isPasswordValid(const QString& password) const {
    return password.size() >= 8;
}

void LoginWidget::updateLoginButtonState() {
    const bool ok = isEmailValid(ui->lineEditLoginEmail->text().trimmed()) &&
                    isPasswordValid(ui->lineEditLoginPassword->text());
    ui->pushButtonLogin->setEnabled(ok);
}

void LoginWidget::updateRegisterButtonState() {
    const bool emailOk = isEmailValid(ui->lineEditRegisterEmail->text().trimmed());
    const bool pwdOk = isPasswordValid(ui->lineEditRegisterPassword->text());
    bool confirmOk = true;
    if (auto confirm = findChild<QLineEdit*>(QStringLiteral("lineEditRegisterPasswordConfirm"));
        confirm) {
        confirmOk = (ui->lineEditRegisterPassword->text() == confirm->text());
    }
    const bool ok = emailOk && pwdOk && confirmOk;
    ui->pushButtonRegister->setEnabled(ok);
}

QString LoginWidget::hashPassword(const QString& password) const {
    const QByteArray hashed =
        QCryptographicHash::hash(password.toUtf8(), QCryptographicHash::Sha256);
    return QString::fromLatin1(hashed.toHex());
}

void LoginWidget::saveAccount(const QString& email, const QString& passwordHash) {
    QSettings settings("CrossControl", "Auth");
    settings.setValue("email", email);
    settings.setValue("passwordHash", passwordHash);
}

bool LoginWidget::loadAccount(QString& emailOut, QString& passwordHashOut) const {
    QSettings settings("CrossControl", "Auth");
    emailOut = settings.value("email").toString();
    passwordHashOut = settings.value("passwordHash").toString();
    return !emailOut.isEmpty() && !passwordHashOut.isEmpty();
}

void LoginWidget::showLoginError(const QString& msg) {
    if (auto label = findChild<QLabel*>(QStringLiteral("labelLoginError")); label) {
        label->setText(msg);
    }
}

void LoginWidget::clearLoginError() {
    if (auto label = findChild<QLabel*>(QStringLiteral("labelLoginError")); label) {
        label->clear();
    }
}

void LoginWidget::showRegisterError(const QString& msg) {
    if (auto label = findChild<QLabel*>(QStringLiteral("labelRegisterError")); label) {
        label->setText(msg);
    }
}

void LoginWidget::clearRegisterError() {
    if (auto label = findChild<QLabel*>(QStringLiteral("labelRegisterError")); label) {
        label->clear();
    }
}
