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
    void printModelOutputDetails(float* outputData, const Ort::TensorTypeAndShapeInfo& outputShapeInfo);
    float iou(const cv::Rect& box1, const cv::Rect& box2);
    std::vector<int> nms(const std::vector<cv::Rect>& boxes, const std::vector<float>& scores, float iouThreshold);
    cv::Rect scaleBox(const cv::Rect& box);
    std::tuple<std::vector<cv::Rect>, std::vector<int>, std::vector<float>>
predict(int width, int height, float* confidences, float* boxes, const std::vector<int64_t>& confidenceDims,
        const std::vector<int64_t>& boxDims, float probThreshold, float iouThreshold);
    std::vector<cv::Rect> getBoxes(float* boxes, const std::vector<int64_t>& boxDims, float* confidences, float confThreshold, float nmsThreshold);
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
    const cv::Size modelShape = {640, 480};
    };
#endif // CHATCLIENT_H
