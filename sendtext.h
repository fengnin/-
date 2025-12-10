//这是一个继承自 QThread 的 SendText 类头文件，用于异步发送文本消息（如聊天消息或控制指令）

#ifndef SENDTEXT_H
#define SENDTEXT_H

#include <QThread>          // 继承自 QThread，实现多线程发送
#include "netheader.h"      // 包含 MSG_TYPE 枚举定义
#include <QMutex>           // 保护共享数据（队列和运行标志）
#include <QQueue>           // 存储待发送的文本消息
#include <QWaitCondition>   // 线程间同步（队列空时等待）
struct M                // 消息结构体：封装消息类型和内容
{
    MSG_TYPE _type;         // 消息类型（如文本、指令等）
    QString _msg;           // 消息内容
    M(MSG_TYPE t,QString msg)
    {
        _type = t;
        _msg = msg;
    }
};

class SendText : public QThread         // SendText 类：负责异步发送文本消息
{
    Q_OBJECT
public:
    explicit SendText(QObject *parent = nullptr);
    void stopImmediately();
public slots:       // 槽函数：将文本消息压入发送队列  t: 消息类型  msg: 消息内容（默认为空字符串）
    void Push_Text(MSG_TYPE,QString = "");
private:
    void run();     // 线程主循环（重写 QThread::run）
private:
    volatile bool _is_canRun;       // 线程运行标志（volatile 防止编译器优化）
    QQueue<M> _textqueue;           // 待发送的文本消息队列
    QMutex _iscanRun_mutex;         // 保护 _is_canRun 的互斥锁
    QMutex _textqueue_mutex;        // 保护队列的互斥锁
    QWaitCondition _textqueue_cond; // 条件变量，队列空时线程等待
};

#endif // SENDTEXT_H
