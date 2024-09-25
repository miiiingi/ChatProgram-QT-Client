#ifndef CHATCLIENT_H
#define CHATCLIENT_H

#include <QWidget>
#include <QTcpSocket>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>

class ChatClient : public QWidget {
    Q_OBJECT

public:
    ChatClient(QWidget *parent = nullptr);

private slots:
    void connectToServer();
    void sendMessage();
    void receiveMessage();
    void login();
    void onDisconnected();

private:
    QTcpSocket *socket;
    QTextEdit *chatBox;
    QLineEdit *messageBox;
    QLineEdit *serverAddress;
    QPushButton *sendButton;
    QPushButton *connectButton;
    QString username;
};

#endif // CHATCLIENT_H
