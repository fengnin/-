//这是一个继承自 QThread 的 SendImg 类头文件，用于异步发送图像数据（如视频帧）

#ifndef SENDIMG_H
#define SENDIMG_H

#include <QThread>      // 继承自 QThread，实现多线程发送
#include <QMutex>       // 保护共享数据（队列和运行标志）
#include <QQueue>       // 存储待发送的图像数据
#include <QImage>       // 处理图像数据
#include <QWaitCondition>   // 线程间同步（队列空时等待）
class SendImg : public QThread          // SendImg 类：负责异步发送图像数据
{
    Q_OBJECT        // 启用 Qt 信号槽机制
public:
    explicit SendImg(QObject *parent = nullptr);
    void stopImmediately();
public slots:
    void Push_Img(QImage img);       // 槽函数：将图像压入发送队列  img: 待发送的 QImage 对象
private:
    void run();   // 线程主循环（重写 QThread::run）
private:
    volatile bool _is_canRun;       // 线程运行标志（volatile 防止编译器优化）
    QMutex _iscanRun_mutex;         // 保护 _is_canRun 的互斥锁
    QQueue<QByteArray> _img_queue;  // 待发送的图像数据队列（存储序列化后的数据）
    QMutex _imgqueue_mutex;         // 保护队列的互斥锁
    QWaitCondition _imgqueue_Cond;  // 条件变量，队列空时线程等待
};

#endif // SENDIMG_H
