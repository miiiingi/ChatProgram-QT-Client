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
#include <QLabel>
#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp>
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
    QLabel *videoWidget;
    std::vector<std::string> classes{"person", "bicycle", "car", "motorcycle", "airplane", "bus", "train", "truck", "boat", "traffic light", "fire hydrant", "stop sign", "parking meter", "bench", "bird", "cat", "dog", "horse", "sheep", "cow", "elephant", "bear", "zebra", "giraffe", "backpack", "umbrella", "handbag", "tie", "suitcase", "frisbee", "skis", "snowboard", "sports ball", "kite", "baseball bat", "baseball glove", "skateboard", "surfboard", "tennis racket", "bottle", "wine glass", "cup", "fork", "knife", "spoon", "bowl", "banana", "apple", "sandwich", "orange", "broccoli", "carrot", "hot dog", "pizza", "donut", "cake", "chair", "couch", "potted plant", "bed", "dining table", "toilet", "tv", "laptop", "mouse", "remote", "keyboard", "cell phone", "microwave", "oven", "toaster", "sink", "refrigerator", "book", "clock", "vase", "scissors", "teddy bear", "hair drier", "toothbrush"};
    QPushButton *streamButton;
    std::unique_ptr<Ort::Session> model;
    };
#endif // CHATCLIENT_H
