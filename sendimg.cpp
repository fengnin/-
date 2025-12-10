#include "sendimg.h"
#include <QBuffer>
#include "netheader.h"
// 引用全局发送队列（在netheader.cpp中定义）
extern MSG_Queue<MESG> queue_send;
// 构造函数：初始化线程父对象
SendImg::SendImg(QObject *parent)
    : QThread{parent}   // 继承自QThread
{}


// 立即停止线程运行（通过标志位控制）
void SendImg::stopImmediately()
{
    QMutexLocker locker(&_iscanRun_mutex);
    _is_canRun = false;
}

// 外部接口：将图像压入发送队列（线程安全）
void SendImg::Push_Img(QImage img)
{
    //外部发送视频帧进来
    QByteArray byte;
    //将buf与byte关联 最后数据写入到byte中
    QBuffer buf(&byte);//QBuffer是QByteArray的I/O设备，可以使QByteArray像文件一样操作
    buf.open(QBuffer::WriteOnly);   // 以只写模式打开
    img.save(&buf,"JPEG");      // 将图像以JPEG格式保存到缓冲区

    //压缩转格式
    QByteArray img_compress = qCompress(byte);      // 使用Qt的压缩功能    
    QByteArray img_base64 = img_compress.toBase64();        // 转为Base64编码
    // 3. 线程安全地操作图像队列
    _imgqueue_mutex.lock();
    // 检查队列是否已满（等待或丢弃数据）
    while(_img_queue.size() >= MSG_QUEUE_MAX_CAP)
    {
        // 等待条件变量或超时（WAITTIME秒）
        bool flag = _imgqueue_Cond.wait(&_imgqueue_mutex,WAITTIME * 1000);
        if(flag == false)   // 超时处理
        {
            _imgqueue_mutex.unlock();
            return;     // 直接返回丢弃数据（可根据需求改为覆盖旧数据）
        }
    }
    // 将处理后的图像数据加入队列
    _img_queue.push_back(img_base64);
    _imgqueue_Cond.wakeOne();   // 通知消费者线程有新数据
    _imgqueue_mutex.unlock();   // 解锁互斥量
}
// 线程主函数：从队列取出图像并发送
void SendImg::run()
{
    _is_canRun = true;  // 初始化运行标志
    while(true)     // 线程主循环
    {
        // 1. 线程安全地获取图像数据
        _imgqueue_mutex.lock();
        // 等待队列非空或超时
        while(_img_queue.isEmpty())
        {
            bool flag = _imgqueue_Cond.wait(&_imgqueue_mutex,WAITTIME * 1000);
            if(flag == false)   // 超时处理
            {
                //每超时一次判断一次 // 检查是否需要停止线程
                {
                    QMutexLocker locker(&_iscanRun_mutex);
                    if(_is_canRun == false)
                    {
                        _imgqueue_mutex.unlock();
                        return;     // 退出线程
                    }
                }

            }
        }
        // 取出队列首部图像数据
        QByteArray img_byte = _img_queue.front();
        _img_queue.pop_front();
        _imgqueue_Cond.wakeOne();       // 通知生产者可以继续生产
        _imgqueue_mutex.unlock();       // 解锁互斥量
        // 2. 构造发送消息结构体
        MESG* img_send = (MESG*)malloc(sizeof(MESG));
        if(img_send == nullptr)     // 内存分配失败处理
        {
            qDebug()<<"img_send malloc failed";
            WRITER_LOG("img_send malloc failed");   // 写入日志（在netheader.h中定义的宏）
            return;
        }
        memset(img_send,0,sizeof(MESG));        // 初始化结构体
        // 填充消息内容
        img_send->msg_type = IMG_SEND;      // 设置消息类型为图像发送
        img_send->data_len = img_byte.size();   // 数据长度
        img_send->data = (uchar*)malloc(img_send->data_len);    // 分配数据内存
        if(img_send->data == nullptr)       // 数据内存分配失败处理
        {
            free(img_send);     // 释放已分配的结构体
            qDebug()<<"img_send malloc failed";
            WRITER_LOG("img_send malloc failed");
            return;
        }
        memset(img_send->data,0,img_send->data_len);    // 初始化数据内存
        // 拷贝图像数据到消息结构体
        memcpy_s(img_send->data,img_send->data_len,img_byte.data(),img_send->data_len);
        // 3. 将消息压入全局发送队列
        queue_send.push_msg(img_send);
    }
}

