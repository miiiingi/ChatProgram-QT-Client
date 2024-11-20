#ifndef CHATCLIENT_H
#define CHATCLIENT_H
#include <QWidget>
#include <QTcpSocket>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QMediaPlayer>
#include <QVideoWidget>
#include <QSlider>
#include <opencv2/opencv.hpp>
#include <QLabel>
#include <onnxruntime/core/session/onnxruntime_cxx_api.h>
class ChatClient : public QWidget {
    Q_OBJECT
public:
    ChatClient(QWidget *parent = nullptr);
private slots:
    void streamVideoToServer();
    void loadModel();
private:
    void login();
    void logout();
    void connectToServer();
    void runModelInference();
    QPushButton *modelInferenceButton;  // 모델 추론 버튼
    cv::VideoCapture webCam; // V4L2 백엔드 사용
    bool isWebcamStreaming = false;  // 웹캠 스트리밍 상태
    std::vector<std::string> classNames;  // 클래스 이름 저장
    std::vector<cv::Scalar> classColors;  // 클래스별 색상 저장
    QTimer *frameTimer;
    QLabel videoWidget;
    /*
    QString username;
    QTcpSocket *socket;
    QTextEdit *chatBox;
    QLineEdit *messageBox;
    QPushButton *sendButton;
    cv::VideoCapture rtspCapture;
    // torch::jit::script::Module torchModel;
    // Video player elements
    QMediaPlayer *mediaPlayer;
    QLineEdit *rtspUrlBox;
    QPushButton *playButton;
    QPushButton *pauseButton;
    QSlider *positionSlider;
    */
    QPushButton *streamButton;
    std::unique_ptr<Ort::Session> model;
    };
#endif // CHATCLIENT_H
