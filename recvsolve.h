//这是一个继承自 QThread 的 RecvSolve 类头文件，用于处理网络数据的接收和分发
#ifndef RECVSOLVE_H
#define RECVSOLVE_H

#include <QThread>      //继承自 QThread，实现多线程接收数据
#include <QMutex>       // 使用 QMutex 控制线程停止标志
#include "netheader.h"      // 包含消息队列和类型定义
 
class RecvSolve : public QThread        // RecvSolve 类：负责接收网络数据并分发
{
    Q_OBJECT                // 启用 Qt 信号槽机制
public:
    explicit RecvSolve(QObject *parent = nullptr);      // 构造函数
    // parent: 父对象指针
    void stopImmediately();       // 立即停止线程运行
private:
    void run();     // 线程主循环（重写 QThread::run）
signals:
    void dataDispatch(MESG*);           // 信号：分发解析后的消息  // msg: 解析后的消息指针（MESG 结构体）
private:
    volatile bool _is_canRun;           // 线程运行标志（volatile 防止编译器优化）
    QMutex _iscanRun_mutex;             // 保护 _is_canRun 的互斥锁
};

#endif // RECVSOLVE_H
