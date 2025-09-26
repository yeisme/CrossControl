#include "loginwidget.h"

#include <qcoreapplication.h>

#include <QAction>
#include <QCheckBox>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QRegularExpression>
#include "modules/Config/config.h"
#include <QTimer>
#include <QVariant>

#include "spdlog/spdlog.h"
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

    // 绑定实时校验（输入变化时更新按钮状态）
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

    // 如果之前保存过邮箱，则预填邮箱输入框
    QString savedEmail, savedHash;
    if (loadAccount(savedEmail, savedHash)) { ui->lineEditLoginEmail->setText(savedEmail); }

    // --- 记住密码 / 自动登录 复选框 ---
    // 在登录界面动态添加两个小的 QCheckBox：
    //  * chkRememberPassword：是否在本机保存密码以便下次自动填充
    //  * chkAutoLogin：是否在启动时自动尝试登录
    // 实现要点：
    //  - 我们把控件插入到登录 tab 使用的 QVBoxLayout 中，这样不需要修改 .ui 文件，
    //    Designer 中的布局仍保持不变。插入时优先放在错误信息标签之前；若找不到则
    //    放在密码输入框之后的合理位置。
    //  - 偏好通过 QSettings("CrossControl","Auth") 存储（平台相关的位置）。
    //  - 为了方便（但不安全），当用户勾选“记住密码”时，我们会把明文密码保存到
    //    键 "passwordPlain" 中以便下次预填。生产环境请避免这样做，下面列出了更安全的替代方案。
    // TODO: 考虑使用更安全的存储方式（见下方注释）。
    //  推荐的替代方案包括：
    //  - 使用操作系统提供的安全存储（Windows Credentials, Keychain等），
    //  - 使用用户特定的密钥加密密码（需要密钥管理）。
    //  - 如果启用了自动登录，则确保同时启用记住密码（自动登录需要本地存储的密码）。
    //  - 如果启用了自动登录，则在 UI 初始化完成后安排一次短延迟的尝试登录。
    if (auto vlay = findChild<QVBoxLayout*>(QStringLiteral("verticalLayoutLogin")); vlay) {
        // create a small horizontal container to hold the two checkboxes
        QWidget* boxHolder = new QWidget(this);
        auto* h = new QHBoxLayout(boxHolder);
        h->setContentsMargins(0, 0, 0, 0);
        h->setSpacing(8);

        // Create the "Remember password" checkbox (objectName used later)
        auto* chkRemember = new QCheckBox(
            QCoreApplication::translate("LoginWidget", "RememberPassword"), boxHolder);
        chkRemember->setObjectName(QStringLiteral("chkRememberPassword"));

        // Create the "Auto-login" checkbox
        auto* chkAuto =
            new QCheckBox(QCoreApplication::translate("LoginWidget", "AutoLogin"), boxHolder);
        chkAuto->setObjectName(QStringLiteral("chkAutoLogin"));

        // 使用小号字体以免复选框在视觉上过于突出
        QFont smallF = chkRemember->font();
        smallF.setPointSize(smallF.pointSize() - 1);
        chkRemember->setFont(smallF);
        chkAuto->setFont(smallF);

        // Add into horizontal layout; addStretch to push them to the left
        h->addWidget(chkRemember, 0, Qt::AlignLeft);
        h->addWidget(chkAuto, 0, Qt::AlignLeft);
        h->addStretch(1);

        // Insert the boxHolder into the vertical layout for the login form.
        // We try to place it before the error label if present, otherwise at
        // index 2 (after the two input QLineEdit fields).
        int insertIndex = -1;
        if (auto lblErr = findChild<QLabel*>(QStringLiteral("labelLoginError")); lblErr) {
            for (int i = 0; i < vlay->count(); ++i) {
                QLayoutItem* it = vlay->itemAt(i);
                if (it && it->widget() == lblErr) {
                    insertIndex = i;
                    break;
                }
            }
        }
        if (insertIndex >= 0)
            vlay->insertWidget(insertIndex, boxHolder);
        else
            vlay->insertWidget(2, boxHolder);

        // --- Load saved preferences from QSettings ---
        // QSettings stores simple key/value pairs in a platform-specific
        // location (registry on Windows). We namespace settings with the
        // application and group to keep them organized.
    const bool remembered = config::ConfigManager::instance().getBool("Auth/rememberPassword", false);
    const bool autoLogin = config::ConfigManager::instance().getBool("Auth/autoLogin", false);
        chkRemember->setChecked(remembered);
        chkAuto->setChecked(autoLogin);

        // If rememberPassword was enabled previously, prefill the password.
        // NOTE: storing plain passwords is insecure. This project currently
        // uses plain storage for convenience; recommended alternatives:
        //  - Store only a password hash and use it for verification (safer),
        //  - Use OS-provided secure storage (Windows Credentials, Keychain),
        //  - Encrypt password with a user-specific key (requires key mgmt).
        if (remembered) {
            const QString plain = config::ConfigManager::instance().getString("Auth/passwordPlain", QString());
            if (!plain.isEmpty()) ui->lineEditLoginPassword->setText(plain);
        }

        // 如果之前启用了自动登录，则在 UI 初始化完成后安排一次短延迟的尝试登录。
        // 会先验证输入是否有效，只有当登录按钮可用时才触发登录。
        if (autoLogin) {
            // small delay to let UI complete layout and widget initialization
            QTimer::singleShot(200, this, [this]() {
                updateLoginButtonState();
                if (ui->pushButtonLogin->isEnabled()) on_pushButtonLogin_clicked();
            });
        }

        // 交互逻辑：若启用自动登录，则确保同时启用记住密码（自动登录需要本地存储的密码）。
        connect(chkAuto, &QCheckBox::toggled, this, [chkRemember](bool on) {
            if (on && !chkRemember->isChecked()) chkRemember->setChecked(true);
        });
    }

    updateLoginButtonState();
    updateRegisterButtonState();
    spdlog::debug("LoginWidget initialized");
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
        // Persist remember / auto-login preference and optionally password
        if (auto chkRemember = findChild<QCheckBox*>(QStringLiteral("chkRememberPassword"));
            chkRemember) {
            config::ConfigManager::instance().setValue("Auth/rememberPassword", chkRemember->isChecked());
            if (chkRemember->isChecked()) {
                config::ConfigManager::instance().setValue("Auth/passwordPlain", ui->lineEditLoginPassword->text());
            } else {
                config::ConfigManager::instance().remove("Auth/passwordPlain");
            }
        }
        if (auto chkAuto = findChild<QCheckBox*>(QStringLiteral("chkAutoLogin")); chkAuto) {
            config::ConfigManager::instance().setValue("Auth/autoLogin", chkAuto->isChecked());
            // If auto-login enabled but remember not checked, enable remember
            if (chkAuto->isChecked()) config::ConfigManager::instance().setValue("Auth/rememberPassword", true);
        }
    } else {
        QMessageBox::warning(
            this,
            QCoreApplication::translate("LoginWidget", "Login Failed"),
            QCoreApplication::translate("LoginWidget", "Incorrect password. Please try again."));
        showLoginError(QCoreApplication::translate("LoginWidget", "Incorrect password"));
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

    // 注册成功后自动切换到登录页并预填登录信息
    ui->lineEditLoginEmail->setText(email);
    ui->lineEditLoginPassword->setText(password);
    updateLoginButtonState();
    clearRegisterError();
    emit loginSuccess();
}

// 实时校验槽函数
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
    config::ConfigManager::instance().setValue("Auth/email", email);
    config::ConfigManager::instance().setValue("Auth/passwordHash", passwordHash);
}

bool LoginWidget::loadAccount(QString& emailOut, QString& passwordHashOut) const {
    emailOut = config::ConfigManager::instance().getString("Auth/email", QString());
    passwordHashOut = config::ConfigManager::instance().getString("Auth/passwordHash", QString());
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
