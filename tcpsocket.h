#ifndef TCPSOCKET_H
#define TCPSOCKET_H

#include <QThread>
#include <QMutex>
#include <QTcpSocket>
#include "netheader.h"
class TcpSocket : public QThread
{
    Q_OBJECT
public:
    explicit TcpSocket(QObject *parent = nullptr);
    ~TcpSocket();
    bool ConnectServer(QString ip,QString port,QIODevice::OpenMode);//外部调用连接   连接服务器
    void stopImmediately();         // 立即停止线程
    void disconnectFromHost();      // 断开连接
    uint32_t getLocalIPv4();         // 获取本地 IPv4 地址
    QString getErrorString();       // 获取错误信息
signals:
    void sendTextOver();        // 信号：文本发送完成
private:
    void run();         // 线程主循环
private slots:
    void RecvFromSocket();      // 接收数据
    bool ConnectToServer(QString ip,QString port,QIODevice::OpenMode);//内部让_sockThread执行连接
    void SendData(MESG*);   // 发送数据
    void CloseSock();       // 关闭套接字
    void ErrorHandle(QAbstractSocket::SocketError);     // 错误处理
private:
    QMutex _iscanRun_mutex; // 保护 _is_canRun 的互斥锁
    QThread* _sockThread;    
    volatile bool _is_canRun;   // 线程运行标志
    QTcpSocket* _tcpsocket;     // TCP 套接字核心对象
    uchar* _sendbuf;            // 发送缓冲区
    uchar* _recvbuf;            // 接收缓冲区
    qint64 _hasread;//_recvbuf中已经读取了多少数据 下一次从哪里开始读取
};

#endif // TCPSOCKET_H
