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
#include <QTimer>
#include <QLabel>
#include <opencv2/opencv.hpp>
#include <algorithm>
#undef slots
#include <torch/script.h>
#include <torch/torch.h>
#define slots Q_SLOTS

using namespace std;


ChatClient::ChatClient(QWidget *parent) : QWidget(parent), username("") {
    // Create UI elements
    chatBox = new QTextEdit(this);
    chatBox->setReadOnly(true);

    messageBox = new QLineEdit(this);
    sendButton = new QPushButton("Send", this);

    // RTSP video playback elements
    /*
    videoWidget = new QVideoWidget(this);
    mediaPlayer = new QMediaPlayer(this);
    mediaPlayer->setVideoOutput(videoWidget);
    */
    videoWidget = new QLabel(this);
    rtspUrlBox = new QLineEdit(this);

    playButton = new QPushButton("Play Video", this);  // Play 버튼 이름 변경
    pauseButton = new QPushButton("Pause Video", this);  // Pause 버튼 추가
    positionSlider = new QSlider(Qt::Horizontal, this);  // 비디오 위치 조절 슬라이더 추가

    // streamButton 추가
    streamButton = new QPushButton("Stream Video to Server", this);  // streamButton 생성
    QHBoxLayout *mainLayout = new QHBoxLayout(this);  // QVBoxLayout에서 QHBoxLayout으로 변경
    QVBoxLayout *inputLayout = new QVBoxLayout();
    inputLayout->addWidget(chatBox);
    QHBoxLayout *inputMessageLayout = new QHBoxLayout();
    inputMessageLayout->addWidget(messageBox);
    inputMessageLayout->addWidget(sendButton);
    inputLayout->addLayout(inputMessageLayout);

    QVBoxLayout *videoLayout = new QVBoxLayout();
    videoLayout->addWidget(videoWidget);
    videoLayout->addWidget(positionSlider);  // 슬라이더 추가
    videoLayout->addWidget(rtspUrlBox);
    videoLayout->addWidget(playButton);
    videoLayout->addWidget(pauseButton);
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
    connect(pauseButton, &QPushButton::clicked, this, &ChatClient::pauseRTSPStream);
    connect(streamButton, &QPushButton::clicked, this, &ChatClient::streamVideoToServer);

    frameTimer = new QTimer(this);
    connect(frameTimer, &QTimer::timeout, this, &ChatClient::processFrame);

    loadModel();

    // 비디오 위치 조절 슬라이더 연결
    /*
    connect(positionSlider, &QSlider::sliderMoved, this, &ChatClient::setPosition);
    connect(mediaPlayer, &QMediaPlayer::positionChanged, this, &ChatClient::updatePosition);
    connect(mediaPlayer, &QMediaPlayer::durationChanged, this, &ChatClient::updateDuration);
    */

    
    // 로그인 과정
    login();
}

qint64 pausedPosition = 0;
bool isPaused = false;


void ChatClient::playRTSPStream() {
    // Play 버튼을 누르면 서버에 PLAY 요청을 보내고, 서버로부터 RTSP 주소를 받음
    if (isPaused) {
        // 정지된 위치가 있다면 해당 위치에서 재생
	frameTimer->start(67);
	isPaused = false;
    } else {
        // 서버에 PLAY 요청을 보냄
        qInfo() << "play button 클릭, 서버에 PLAY 요청";
        socket->write("PLAY\n");
    }
}

void ChatClient::pauseRTSPStream() {
    // 현재 재생 중인 위치를 저장하고 일시정지
    if(!isPaused){
	frameTimer->stop();
	isPaused = true;
    }
}


void ChatClient::streamVideoToServer() {
    qInfo() << "stream video to server 클릭";
    socket->write("WEBCAM_STREAM\n");  // 메시지의 끝을 '\n'으로 구분
}

void ChatClient::setPosition(int position) {
    if (mediaPlayer->isSeekable()) {
        mediaPlayer->setPosition(position);  // 스트림 위치 설정
    }
}

void ChatClient::updatePosition(qint64 position) {
    positionSlider->setValue(static_cast<int>(position));  // 슬라이더 값 업데이트
}

void ChatClient::updateDuration(qint64 duration) {
    positionSlider->setRange(0, static_cast<int>(duration));  // 슬라이더 범위 설정
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
    QString rtspUrl = QString::fromUtf8(message);

    qDebug() << "Received message: " << rtspUrl;

    // 서버로부터 RTSP 주소를 받으면 재생
    if (rtspUrl.startsWith("rtsp://")) {
        rtspUrlBox->setText(rtspUrl);  // URL을 입력 필드에 표시
	if(rtspCapture.open(rtspUrl.toStdString())){
	    frameTimer->start(67);
	    chatBox->append("Playing RTSP stream from: " + rtspUrl);
	} else{
	    qDebug() << "RTSP 스트림을 열 수 없습니다: " << rtspUrl;
	}

    } else {
        chatBox->append(rtspUrl);  // 일반 메시지 출력
        qDebug() << "Non-stream message: " << rtspUrl;
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

void ChatClient::processFrame() {
    cv::Mat frame;
    if (rtspCapture.read(frame)) {
<<<<<<< HEAD
        qDebug() << "RTSP Capture Succeed!";

        // 원본 이미지 크기 저장
        int original_width = frame.cols;
        int original_height = frame.rows;

        // 프레임을 PyTorch/ONNX 모델로 처리
        cv::cvtColor(frame, frame, cv::COLOR_BGR2RGB);  // RGB로 변환

        // PyTorch 모델에 맞는 텐서로 변환 (640x640으로 크기 변경)
        cv::resize(frame, frame, cv::Size(640, 640));  // 모델이 640x640 크기를 요구한다고 가정
        torch::Tensor imgTensor = torch::from_blob(frame.data, {frame.rows, frame.cols, 3}, torch::kByte);
        imgTensor = imgTensor.permute({2, 0, 1});
        imgTensor = imgTensor.toType(torch::kFloat);
        imgTensor = imgTensor.div(255);
        imgTensor = imgTensor.unsqueeze(0);

        // 모델로 추론
        auto output = torchModel.forward({imgTensor});
        torch::Tensor preds = output.toTensor();
        std::vector<torch::Tensor> dets = non_max_suppression(preds, 0.4, 0.5);

        if (dets.size() > 0){
            for(size_t i = 0; i < dets[0].size(0); ++i){
                // 모델 출력 좌표는 640x640 기준이므로 원본 이미지 크기로 변환
                float left = dets[0][i][0].item().toFloat() * original_width / 640;
                float top = dets[0][i][1].item().toFloat() * original_height / 640;
                float right = dets[0][i][2].item().toFloat() * original_width / 640;
                float bottom = dets[0][i][3].item().toFloat() * original_height / 640;
                float score = dets[0][i][4].item().toFloat();
                int classID = dets[0][i][5].item().toInt();

                // 바운딩 박스 그리기
                cv::rectangle(frame, cv::Rect(left, top, (right - left), (bottom - top)), cv::Scalar(0, 255, 0), 2);
            }
        }

        // OpenCV를 사용해 화면에 출력
        cv::cvtColor(frame, frame, cv::COLOR_RGB2BGR);  // 다시 BGR로 변환 (OpenCV가 BGR을 사용하기 때문)
=======
        // 640x640으로 리사이즈
        cv::resize(frame, frame, cv::Size(640, 640));

        // PyTorch 모델 입력 텐서로 변환
        torch::Tensor imgTensor = torch::from_blob(frame.data, {1, 640, 640, 3}, torch::kByte);
        imgTensor = imgTensor.permute({0, 3, 1, 2}).to(torch::kFloat).div(255);
        cout << "image tensor size: " << imgTensor.sizes() << "\n";

        try {
            cout << "===========================================\n";
            auto output = torchModel.forward({imgTensor});

            // output 타입 확인
            if (output.isTensor()) {
                torch::Tensor preds = output.toTensor();
                std::vector<torch::Tensor> dets = non_max_suppression(preds, 0.4, 0.5);

                // Detected objects 그리기
                if (!dets.empty()) {
                    for (size_t i = 0; i < dets[0].size(0); ++i) {
                        float left = dets[0][i][0].item().toFloat() * frame.cols / 640;
                        float top = dets[0][i][1].item().toFloat() * frame.rows / 640;
                        float right = dets[0][i][2].item().toFloat() * frame.cols / 640;
                        float bottom = dets[0][i][3].item().toFloat() * frame.rows / 640;
                        cv::rectangle(frame, cv::Rect(left, top, right - left, bottom - top), cv::Scalar(0, 255, 0), 2);
                    }
                }
            } else {
                qCritical() << "모델 출력이 예상한 텐서가 아닙니다. 반환된 타입: " << QString::fromStdString(output.type()->str());
            }
        } catch (const c10::Error &e) {
            qCritical() << "모델 추론 중 오류 발생: " << e.what();
        }

        cv::cvtColor(frame, frame, cv::COLOR_RGB2BGR);
>>>>>>> feature
        QImage qFrame(frame.data, frame.cols, frame.rows, frame.step, QImage::Format_RGB888);

        // QPixmap으로 변환하여 QLabel에 표시
        QPixmap pixmap = QPixmap::fromImage(qFrame);
        videoWidget->setPixmap(pixmap);

        // 이미지 파일로 저장 (PNG 또는 JPG 형식)
        QString savePath = "capture.png"; // 저장할 경로 및 파일명
        if (pixmap.save(savePath)) {
            qDebug() << "Image saved successfully to" << savePath;
        } else {
            qDebug() << "Failed to save image to" << savePath;
        }
    }
}

<<<<<<< HEAD
=======


>>>>>>> feature
void ChatClient::loadModel() {
    // PyTorch 모델 로드
    try {
        torchModel = torch::jit::load("/home/mingi/Desktop/workspace/ChatProgram-QT/Client/yolo11n.torchscript");
        torchModel.eval();
    } catch (const c10::Error &e) {
        qDebug() << "PyTorch 모델을 로드하는 데 오류 발생: " << e.what();
    }
}

std::vector<torch::Tensor> ChatClient::non_max_suppression(torch::Tensor preds, float score_thresh, float iou_thresh)

{
        std::vector<torch::Tensor> output;
        for (size_t i=0; i < preds.sizes()[0]; ++i)
        {
            torch::Tensor pred = preds.select(0, i);

            // Filter by scores
            torch::Tensor scores = pred.select(1, 4) * std::get<0>( torch::max(pred.slice(1, 5, pred.sizes()[1]), 1));
            pred = torch::index_select(pred, 0, torch::nonzero(scores > score_thresh).select(1, 0));
            if (pred.sizes()[0] == 0) continue;

            // (center_x, center_y, w, h) to (left, top, right, bottom)
            pred.select(1, 0) = pred.select(1, 0) - pred.select(1, 2) / 2;
            pred.select(1, 1) = pred.select(1, 1) - pred.select(1, 3) / 2;
            pred.select(1, 2) = pred.select(1, 0) + pred.select(1, 2);
            pred.select(1, 3) = pred.select(1, 1) + pred.select(1, 3);

            // Computing scores and classes
            std::tuple<torch::Tensor, torch::Tensor> max_tuple = torch::max(pred.slice(1, 5, pred.sizes()[1]), 1);
            pred.select(1, 4) = pred.select(1, 4) * std::get<0>(max_tuple);
            pred.select(1, 5) = std::get<1>(max_tuple);

            torch::Tensor  dets = pred.slice(1, 0, 6);

            torch::Tensor keep = torch::empty({dets.sizes()[0]});
            torch::Tensor areas = (dets.select(1, 3) - dets.select(1, 1)) * (dets.select(1, 2) - dets.select(1, 0));
            std::tuple<torch::Tensor, torch::Tensor> indexes_tuple = torch::sort(dets.select(1, 4), 0, 1);
            torch::Tensor v = std::get<0>(indexes_tuple);
            torch::Tensor indexes = std::get<1>(indexes_tuple);
            int count = 0;
            while (indexes.sizes()[0] > 0)
            {
                keep[count] = (indexes[0].item().toInt());
                count += 1;

                // Computing overlaps
                torch::Tensor lefts = torch::empty(indexes.sizes()[0] - 1);
                torch::Tensor tops = torch::empty(indexes.sizes()[0] - 1);
                torch::Tensor rights = torch::empty(indexes.sizes()[0] - 1);
                torch::Tensor bottoms = torch::empty(indexes.sizes()[0] - 1);
                torch::Tensor widths = torch::empty(indexes.sizes()[0] - 1);
                torch::Tensor heights = torch::empty(indexes.sizes()[0] - 1);
                for (size_t i=0; i<indexes.sizes()[0] - 1; ++i)
                {
                    lefts[i] = std::max(dets[indexes[0]][0].item().toFloat(), dets[indexes[i + 1]][0].item().toFloat());
                    tops[i] = std::max(dets[indexes[0]][1].item().toFloat(), dets[indexes[i + 1]][1].item().toFloat());
                    rights[i] = std::min(dets[indexes[0]][2].item().toFloat(), dets[indexes[i + 1]][2].item().toFloat());
                    bottoms[i] = std::min(dets[indexes[0]][3].item().toFloat(), dets[indexes[i + 1]][3].item().toFloat());
                    widths[i] = std::max(float(0), rights[i].item().toFloat() - lefts[i].item().toFloat());
                    heights[i] = std::max(float(0), bottoms[i].item().toFloat() - tops[i].item().toFloat());
                }
                torch::Tensor overlaps = widths * heights;

                // FIlter by IOUs
                torch::Tensor ious = overlaps / (areas.select(0, indexes[0].item().toInt()) + torch::index_select(areas, 0, indexes.slice(0, 1, indexes.sizes()[0])) - overlaps);
                indexes = torch::index_select(indexes, 0, torch::nonzero(ious <= iou_thresh).select(1, 0) + 1);
            }
            keep = keep.toType(torch::kInt64);
            output.push_back(torch::index_select(dets, 0, keep.slice(0, 0, count)));
        }
        return output;
}

