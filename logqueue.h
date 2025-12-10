//这是一个日志队列处理的C++头文件，使用了Qt框架的多线程功能来实现日志的异步写入。

#ifndef LOGQUEUE_H
#define LOGQUEUE_H
#define _CRT_SECURE_NO_WARNINGS         // 禁用MSVC的安全警告
#include <QThread>          // 包含QThread类，用于多线程处理
#include <QMutex>           // 包含QMutex类，用于线程同步
#include <cstdio>           // 包含标准C库的I/O函数
#include <QDebug>           // 包含Qt的调试输出功能
#include "netheader.h"
struct Log              // 日志结构体，用于存储日志信息
{
    char* buf;          // 日志内容的缓冲区指针
    uint len;           // 日志内容的长度
};

class LogQueue : public QThread         // LogQueue类继承自QThread，用于异步处理日志写入
{
public:
    LogQueue();
    ~LogQueue();
    void push_log(Log* log);             // 向日志队列中添加日志  log: 指向Log结构体的指针，包含日志内容和长度
    void stopImmediately();             // 立即停止线程
private:
    void run();//将log_queue中的数据取出写入文件    重写的QThread运行函数，线程执行的主要逻辑
private:
    MSG_Queue<Log> _log_queue;      // 日志队列，存储待写入的日志
    volatile bool _is_canRun;//判断线程是否还启动着
    FILE* _logfile;         // 文件指针，指向日志文件
    QMutex _iscanRun_mutex;         // 互斥锁，用于保护_is_canRun变量的线程安全

};

#endif // LOGQUEUE_H
