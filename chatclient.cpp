#include "chatclient.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHostAddress>
#include <QMessageBox>
#include <QDialog>
#include <QFormLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QCloseEvent>
#include <QFileDialog>
#include <QMediaPlayer>
#include <QVideoWidget>
#include <QProcess>

ChatClient::ChatClient(QWidget *parent) : QWidget(parent), username("") {
    // Create UI elements
    chatBox = new QTextEdit(this);
    chatBox->setReadOnly(true);

    messageBox = new QLineEdit(this);
    sendButton = new QPushButton("Send", this);

    // RTSP video playback elements
    videoWidget = new QVideoWidget(this);
    mediaPlayer = new QMediaPlayer(this);
    mediaPlayer->setVideoOutput(videoWidget);

    rtspUrlBox = new QLineEdit(this);
    // RTSP 주소 입력 필드에 placeholder 추가
    rtspUrlBox->setPlaceholderText("RTSP Address");

    playButton = new QPushButton("Play RTSP", this);  // Play 버튼 생성
    streamButton = new QPushButton("Stream Video to Server", this);

    QHBoxLayout *mainLayout = new QHBoxLayout(this);  // QVBoxLayout에서 QHBoxLayout으로 변경
    QVBoxLayout *inputLayout = new QVBoxLayout();
    inputLayout->addWidget(chatBox);
    QHBoxLayout *inputMessageLayout = new QHBoxLayout();
    inputMessageLayout->addWidget(messageBox);
    inputMessageLayout->addWidget(sendButton);
    inputLayout->addLayout(inputMessageLayout);

    QVBoxLayout *videoLayout = new QVBoxLayout();
    videoLayout->addWidget(videoWidget);
    videoLayout->addWidget(rtspUrlBox);
    videoLayout->addWidget(playButton);
    videoLayout->addWidget(streamButton);

    mainLayout->addLayout(inputLayout);  // 채팅 레이아웃을 왼쪽에 배치
    mainLayout->addLayout(videoLayout);  // 비디오 레이아웃을 오른쪽에 배치
    setLayout(mainLayout);

    // Create TCP socket
    socket = new QTcpSocket(this);

    // Connect signals and slots
    connect(sendButton, &QPushButton::clicked, this, &ChatClient::sendMessage);
    connect(messageBox, &QLineEdit::returnPressed, this, &ChatClient::sendMessage); // 엔터키로 메시지 전송
    connect(socket, &QTcpSocket::readyRead, this, &ChatClient::receiveMessage);
    connect(socket, &QTcpSocket::disconnected, this, &ChatClient::onDisconnected);

    // Connect RTSP play button
    connect(playButton, &QPushButton::clicked, this, &ChatClient::playRTSPStream);
    connect(streamButton, &QPushButton::clicked, this, &ChatClient::streamVideoToServer);

    // 로그인 과정
    login();
}

void ChatClient::playRTSPStream() {
    qInfo() << "play button 클릭";
    socket->write("PLAY\n");  // 메시지의 끝을 '\n'으로 구분
}

void ChatClient::streamVideoToServer() {
    qInfo() << "stream video to server 클릭";
    socket->write("WEBCAM_STREAM\n");  // 메시지의 끝을 '\n'으로 구분
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
    QString messageStr = QString::fromUtf8(message);

    // 메시지 로그 출력
    qDebug() << "Received message: " << messageStr;

    // RTSP 스트림을 재생하기 위한 메시지 처리
    if (messageStr.startsWith("rtsp://")) {
        // 클라이언트가 RTSP URL을 수신하면 미디어 플레이어로 스트림을 재생
        rtspUrlBox->setText(messageStr);  // URL을 입력 필드에 표시
        mediaPlayer->setSource(QUrl(messageStr));  // QMediaPlayer로 스트림 설정
        mediaPlayer->play();  // 스트림 재생

        chatBox->append("Playing RTSP stream from: " + messageStr);
    } else {
        chatBox->append(messageStr);  // 일반 메시지 출력
        qDebug() << "Non-stream message: " << messageStr;
    }
}

void ChatClient::onDisconnected() {
    chatBox->append("Disconnected from server.");
}

// 로그아웃 시 서버에 메시지 전송
void ChatClient::logout() {
    if (socket->state() == QAbstractSocket::ConnectedState) {
        QString logoutMessage = username + " logout!";
        socket->write(logoutMessage.toUtf8());
        socket->disconnectFromHost();  // 서버에 로그아웃 메시지를 보낸 후 소켓 연결 해제
    }
}

// 창이 닫힐 때 로그아웃 처리
void ChatClient::closeEvent(QCloseEvent *event) {
    logout();  // 창이 닫힐 때 로그아웃
    event->accept();
}
