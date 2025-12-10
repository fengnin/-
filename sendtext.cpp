#include "sendtext.h"
#include <QDebug>   // 引入 Qt 调试输出功能
// 声明全局消息队列，用于跨线程通信
extern MSG_Queue<MESG> queue_send;
// 构造函数，初始化 QThread 父类
SendText::SendText(QObject *parent)
    : QThread{parent}   // 调用基类 QThread 的构造函数
{}
// 线程停止方法，设置标志位并通知线程退出
void SendText::stopImmediately()
{
    //qDebug()<<"SendTextThread stopImmediately _is_canRun : "<<_is_canRun;
    {
        // 使用互斥锁保护共享变量 _is_canRun，避免多线程竞争
        QMutexLocker locker(&_iscanRun_mutex);
        _is_canRun = false; // 设置停止标志位
        //qDebug()<<"SendTextThread stopImmediately _is_canRun : "<<_is_canRun;
    }

}

// 生产者方法：将文本消息推入队列
void SendText::Push_Text(MSG_TYPE msg_type, QString msg)//生产者
{
    //qDebug()<<"Push_Text";
    //qDebug()<<"Push_Text msg->type : "<<msg_type;
    //将数据包装插入textqueue
    // 加锁保护队列操作
    _textqueue_mutex.lock();
    // 如果队列已满，等待消费者处理（避免无限增长）
    while(_textqueue.size() >= MSG_QUEUE_MAX_CAP)
    {
        _textqueue_cond.wait(&_textqueue_mutex);    // 释放锁并等待条件变量
    }
    // 将消息包装后推入队列
    _textqueue.push_back(M(msg_type,msg));
    //qDebug()<<"_textqueue 数据为 ："<<_textqueue.size();
    // 通知消费者有新数据（即使队列未满也唤醒，避免死锁）
    _textqueue_cond.wakeOne();
    // 解锁队列
    _textqueue_mutex.unlock();
}

// 消费者方法：线程主循环，从队列取消息并发送
void SendText::run()//消费者
{
    //将数据发送到queue_send
    _is_canRun = true;      // 初始化运行标志位
    while(true)
    {

        //错误写法  不应该超时取不到数据就return 会导致后面的数据都无法发送
        // {
        //     QMutexLocker locker(&_iscanRun_mutex);
        //     if(_is_canRun == false)
        //     {
        //         qDebug()<<"_is_canRun : "<<_is_canRun;
        //         break;
        //     }
        // }
        // qDebug()<<"run1";
        // //取数据
        // _textqueue_mutex.lock();
        // while(_textqueue.isEmpty())
        // {
        //     bool flag = _textqueue_cond.wait(&_textqueue_mutex,WAITTIME * 1000);
        //     if(flag == false)
        //     {
        //         _textqueue_mutex.unlock();
        //         return;
        //     }
        // }

        _textqueue_mutex.lock();// 加锁保护队列操作
        while(_textqueue.isEmpty())     // 如果队列为空，等待数据或超时
        {
            bool flag = _textqueue_cond.wait(&_textqueue_mutex,WAITTIME * 1000);    // 等待 WAITTIME 秒，返回 false 表示超时
            if(flag == false)//超时判断一次线程是否退出
            {
                //必须将判断线程是否停止判断写在这里
                {
                    QMutexLocker locker(&_iscanRun_mutex);
                    if(_is_canRun == false)
                    {
                        qDebug()<<"_textqueue _is_canRun : "<<_is_canRun;
                        //_textqueue_mutex.unlock();
                        qDebug()<<"_sendtext 线程退出";
                        return;
                    }
                }
            }
        }
        //qDebug()<<"_textqueue size :"<<_textqueue.size();
        // 从队列头部取出消息
        M m = _textqueue.front();
        _textqueue.pop_front();
        // 通知生产者可以继续推送（可选优化）
        _textqueue_cond.wakeOne();
        // 解锁队列（允许生产者继续操作）
        _textqueue_mutex.unlock();
        //qDebug()<<"取出数据 _textqueue 数据为 ："<<_textqueue.size();
        //封装成MESG
        //qDebug()<<"run2";

        // 动态分配消息结构体内存
        MESG* send = (MESG*)malloc(sizeof(MESG));
        if(send == nullptr)
        {
            qDebug()<<"MESG send malloc failed";
            break;
        }
        // 初始化消息结构体
        memset(send,0,sizeof(MESG));
        // 根据消息类型处理不同逻辑
        if(m._type == CREATE_MEETING || m._type == CLOSE_CAMERA)
        {
            //不需房间号
            //qDebug()<<"queue send push success";
            // 无需附加数据，直接设置类型
            send->data = nullptr;
            send->data_len = 0;
            send->msg_type = m._type;
            // 推入全局发送队列（假设 queue_send 接管内存管理）
            queue_send.push_msg(send);
            //queue_send.debugPrintf();
        }
        else if(m._type == JOIN_MEETING)
        {
            //qDebug()<<"type为JOIN_MEETING";
            // 需要附加房间号（4字节）
            send->msg_type = JOIN_MEETING;
            send->data_len = 4;//房间号占四个字节
            send->data = (uchar*)malloc(send->data_len + 8);    // 分配内存存储房间号（额外+8字节可能是对齐或预留空间）
            if(send->data == nullptr)
            {
                qDebug()<<"send->data malloc failed";
                free(send); // 释放已分配的 send 结构体
                continue;   // 跳过当前消息
            }
            // 清零内存并写入房间号
            memset(send->data,0,send->data_len + 8);
            uint roomno = m._msg.toUInt();  // 将 QString 转为无符号整型
            memcpy(send->data,&roomno,sizeof(roomno));
            // 推入全局队列
            queue_send.push_msg(send);
            //queue_send.debugPrintf();
        }
        else if(m._type == TEXT_SEND)
            // 文本消息需要压缩后发送
        {
            send->msg_type = TEXT_SEND;
            //压缩发送
            // 使用 Qt 的 qCompress 压缩文本数据
            QByteArray data = qCompress(QByteArray::fromStdString(m._msg.toStdString()));
            send->data_len = data.size();
            // 分配内存存储压缩后的数据
            send->data = (uchar*)malloc(send->data_len);
            if(send->data == nullptr)
            {
                qDebug()<<"send->data malloc failed";
                WRITER_LOG("send->data malloc failed");     // 自定义日志
                continue;
            }
            // 拷贝压缩数据到消息结构体
            memset(send->data,0,send->data_len);
            memcpy_s(send->data,send->data_len,data.data(),data.size());
            // 推入全局队列
            queue_send.push_msg(send);
        }
        //qDebug()<<"run3";
    }
}


