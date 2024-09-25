#include "chatclient.h"
#include "ui_chatclient.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHostAddress>
#include <QMessageBox>
#include <QDialog>
#include <QFormLayout>
#include <QLineEdit>
#include <QPushButton>

ChatClient::ChatClient(QWidget *parent) : QWidget(parent), username("") {
    // Create UI elements
    chatBox = new QTextEdit(this);
    chatBox->setReadOnly(true);

    messageBox = new QLineEdit(this);

    sendButton = new QPushButton("Send", this);

    // Layout setup
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    QHBoxLayout *inputLayout = new QHBoxLayout();
    inputLayout->addWidget(messageBox);
    inputLayout->addWidget(sendButton);

    mainLayout->addWidget(chatBox);
    mainLayout->addLayout(inputLayout);

    setLayout(mainLayout);

    // Create TCP socket
    socket = new QTcpSocket(this);

    // Connect signals and slots
    connect(sendButton, &QPushButton::clicked, this, &ChatClient::sendMessage);
    connect(socket, &QTcpSocket::readyRead, this, &ChatClient::receiveMessage);
    connect(socket, &QTcpSocket::disconnected, this, &ChatClient::onDisconnected);

    // 로그인 과정
    login();
}

void ChatClient::login() {
    // 사용자 아이디 및 비밀번호 입력 화면 생성
    QDialog loginDialog(this);
    QFormLayout form(&loginDialog);

    // 사용자 아이디 입력
    QLineEdit *usernameEdit = new QLineEdit(&loginDialog);
    form.addRow("Username:", usernameEdit);

    // 비밀번호 입력
    QLineEdit *passwordEdit = new QLineEdit(&loginDialog);
    passwordEdit->setEchoMode(QLineEdit::Password);
    form.addRow("Password:", passwordEdit);

    // 로그인 버튼
    QPushButton *loginButton = new QPushButton("Login", &loginDialog);
    form.addRow(loginButton);

    connect(loginButton, &QPushButton::clicked, &loginDialog, &QDialog::accept);

    // 로그인 다이얼로그가 완료될 때까지 대기
    if (loginDialog.exec() == QDialog::Accepted) {
        username = usernameEdit->text();
        QString password = passwordEdit->text();

        // 유효성 검사
        if (username.isEmpty() || password.isEmpty()) {
            QMessageBox::critical(this, "Error", "Username and password cannot be empty.");
            close();
        } else {
            chatBox->append("Welcome, " + username + "!");
            connectToServer();
        }
    } else {
        close(); // 로그인 취소 시 프로그램 종료
    }
}

void ChatClient::connectToServer() {
    // 자동으로 127.0.0.1 서버에 1234 포트로 접속
    socket->connectToHost(QHostAddress("127.0.0.1"), 1234);

    if (!socket->waitForConnected(3000)) {
        QMessageBox::warning(this, "Error", "Failed to connect to server.");
    } else {
        chatBox->append("Connected to server.");
        // 로그인 메시지를 서버에 전송
        QString loginMessage = username + " login!";
        socket->write(loginMessage.toUtf8());
    }
}

void ChatClient::sendMessage() {
    if (messageBox->text().isEmpty()) {
        return;
    }

    QString message = username + ": " + messageBox->text(); // 아이디: 메시지 형태
    socket->write(message.toUtf8());
    chatBox->append("Me: " + messageBox->text());
    messageBox->clear();
}

void ChatClient::receiveMessage() {
    QByteArray message = socket->readAll();
    chatBox->append(QString::fromUtf8(message));
}

void ChatClient::onDisconnected() {
    QString logoutMessage = username + " logout!";
    socket->write(logoutMessage.toUtf8());
    chatBox->append(logoutMessage);
}
