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
#undef slots
#include <torch/script.h>
#include <torch/torch.h>
#define slots Q_SLOTS
#include <QLabel>

class ChatClient : public QWidget {
    Q_OBJECT

public:
    ChatClient(QWidget *parent = nullptr);

private slots:
    void sendMessage();
    void receiveMessage();
    void onDisconnected();
    void closeEvent(QCloseEvent *event);
    void playRTSPStream();
    void pauseRTSPStream();
    void streamVideoToServer();
    void setPosition(int position);
    void updatePosition(qint64 position);
    void updateDuration(qint64 duration);
    void processFrame();
    std::vector<torch::Tensor> non_max_suppression(torch::Tensor preds, float score_thresh, float iou_thresh);
    void loadModel();


private:
    void login();
    void logout();
    void connectToServer();

    QString username;
    QTcpSocket *socket;
    QTextEdit *chatBox;
    QLineEdit *messageBox;
    QPushButton *sendButton;
    QTimer *frameTimer;
    cv::VideoCapture rtspCapture;
    torch::jit::script::Module torchModel;

    // Video player elements
    QMediaPlayer *mediaPlayer;
    QLabel *videoWidget;
    QLineEdit *rtspUrlBox;

    QPushButton *playButton;
    QPushButton *pauseButton;
    QSlider *positionSlider;
    QPushButton *streamButton;
    };

#endif // CHATCLIENT_H
