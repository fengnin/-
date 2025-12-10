//这是一个网络通信相关的头文件，定义了消息队列、消息类型和一些宏定义

#ifndef NETHEADER_H
#define NETHEADER_H
#include<QQueue>          // 包含Qt的队列容器
#include<QMutex>          // 包含Qt的互斥锁类
#include<QWaitCondition>    // 包含Qt的等待条件类
#include<QDebug>            // 包含Qt的调试输出功能
//#include "logqueue.h" //不能在头文件包含logqueue会导致递归包含 就算加入#ifndef NETHEADER_H也没办法解决，解决方法在netheader.cpp中包含头文件可以避免递归包含
#ifndef MSG_QUEUE_MAX_CAP
#define MSG_QUEUE_MAX_CAP 1500      // 定义消息队列的最大容量
#endif

#ifndef MB
#define MB 1024 * 1024          // 定义MB和KB的大小
#endif

#ifndef KB
#define KB 1024
#endif

#ifndef WAITTIME
#define WAITTIME 2      // 定义等待时间
#endif

#ifndef OPENCAMERA      // 定义摄像头控制相关的字符串常量
#define OPENCAMERA "开启摄像头"
#endif

#ifndef CLOSECAMERA
#define CLOSECAMERA "关闭摄像头"
#endif

#ifndef OPENAUDIO
#define OPENAUDIO "开启语音"
#endif

#ifndef CLOSEAUDIO
#define CLOSEAUDIO "关闭语音"
#endif

//数据报头最小长度
#ifndef HEAD_MIN
#define HEAD_MIN 11
#endif

enum MSG_TYPE
{
    //视频帧发送/接收
    IMG_SEND = 0,
    IMG_RECV,

    //音频发送/接收
    AUDIO_SEND,
    AUDIO_RECV,

    //文本发送/接收
    TEXT_SEND,
    TEXT_RECV,

    CREATE_MEETING,
    EXIT_MEETING,
    JOIN_MEETING,
    //关闭摄像头 开启摄像头不需要，用IMG_SEND发送视频帧到服务器即可
    CLOSE_CAMERA,

    //服务器回响
    CREATE_MEETING_RESPONSE = 20,
    PARTNER_EXIT,
    PARTNER_JOIN_OTHER,//告诉当前会议里面的所有人新伙伴加入
    JOIN_MEETING_RESPONSE,
    PARTNER_JOIN_SELF,//将会议内其他人的信息告诉自己，便于添加partner

    //服务器出错 不需要经过服务器
    REMOVE_HOST_CLOSE_ERROR = 40,
    OTHER_NET_ERROR,

    //用于Debug
    //DEBUG
};
Q_DECLARE_METATYPE(MSG_TYPE);//将MSG_TYPE支持信号槽参数发送

struct MESG
{
    uint32_t ip;//4字节
    MSG_TYPE msg_type;//4字节
    uint32_t data_len;//4字节
    uchar* data;
};
Q_DECLARE_METATYPE(MESG);

template<class T>
class MSG_Queue
{
public:
    void push_msg(T* msg)//生产
    {
        msg_queue_mutex.lock();
        while(msg_queue.size() >= MSG_QUEUE_MAX_CAP)//无空间
        {
            msg_queue_Cond.wait(&msg_queue_mutex);
        }
        msg_queue.push_back(msg);
        msg_queue_Cond.wakeOne();//唤醒
        msg_queue_mutex.unlock();
    }

    T* pop_msg()//消费
    {
        msg_queue_mutex.lock();
        while(msg_queue.empty())//无数据
        {
            bool flag = msg_queue_Cond.wait(&msg_queue_mutex,WAITTIME * 1000);
            if(flag == false)
            {
                //返回时必须解锁 ！！
                msg_queue_mutex.unlock();
                return nullptr;
            }
        }
        T* msg = msg_queue.front();
        msg_queue.pop_front();
        msg_queue_Cond.wakeOne();
        msg_queue_mutex.unlock();
        return msg;
    }

    void clear()
    {
        QMutexLocker locker(&msg_queue_mutex);
        msg_queue.clear();
    }

    void debugPrintf()
    {
        qDebug()<<"队列数据为 ： "<<msg_queue.size();
    }
private:
    QQueue<T*> msg_queue;
    QMutex msg_queue_mutex;
    QWaitCondition msg_queue_Cond;
};

void log_write(const char*,const char*,const char*,const int,...);

#define WRITER_LOG(LOGTEXT,...) do{\
log_write(LOGTEXT,__FILE__,__FUNCTION__,__LINE__,##__VA_ARGS__);\
}while(0);

#endif // NETHEADER_H

