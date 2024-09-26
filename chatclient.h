#ifndef CHATCLIENT_H
#define CHATCLIENT_H

#include <QWidget>
#include <QTcpSocket>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QMediaPlayer>
#include <QVideoWidget>

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

private:
    void login();
    void logout();
    void connectToServer();

    QString username;
    QTcpSocket *socket;
    QTextEdit *chatBox;
    QLineEdit *messageBox;
    QPushButton *sendButton;

    // Video player elements
    QMediaPlayer *mediaPlayer;
    QVideoWidget *videoWidget;
    QLineEdit *rtspUrlBox;
    QPushButton *playButton;
    };

#endif // CHATCLIENT_H
