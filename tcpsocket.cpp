//主要用于客户端与服务器之间的消息收发
#include "tcpsocket.h"
#include <QHostAddress>
#include <QDebug>
#include <QtEndian>
#include <string>
// 声明全局消息队列（发送、接收、音频接收）
extern MSG_Queue<MESG> queue_send;
extern MSG_Queue<MESG> queue_recv;
extern MSG_Queue<MESG> audio_recv;

// 构造函数，初始化TCP套接字和线程
TcpSocket::TcpSocket(QObject *parent)
    : QThread{parent}
{
    _tcpsocket = nullptr;   // 初始化套接字指针为空
    _sockThread = new QThread();    // 创建专用线程处理套接字
    this->moveToThread(_sockThread);    // 将当前对象移动到新线程

     //当线程收到quit退出请求时，不会立刻退出会将手上的循环事件处理完毕再退出 所以可以利用这个机制让_sockThread顺手关闭套接字
    // 线程结束时自动关闭套接字
    connect(_sockThread,SIGNAL(finished()),this,SLOT(CloseSock()));
    // 注册SocketError类型，用于信号槽传递
    qRegisterMetaType<QAbstractSocket::SocketError>("QAbstractSocket::SocketError");
    // 分配发送和接收缓冲区（4MB大小）
    _sendbuf = (uchar*)malloc(4*MB);
    _recvbuf = (uchar*)malloc(4*MB);
    memset(_sendbuf,0,4*MB);    // 清空发送缓冲区
    memset(_recvbuf,0,4*MB);    // 清空接收缓冲区
    _hasread = 0;   // 已读取字节数初始化为0
}
// 析构函数，释放资源
TcpSocket::~TcpSocket()
{
    qDebug()<<_sockThread->isRunning(); // 调试输出线程状态
    delete _sockThread;     // 删除线程
    delete _tcpsocket;      // 删除套接字
    free(_sendbuf);         // 释放发送缓冲区
    free(_recvbuf);         // 释放接收缓冲区
}

//当连接失败时 run函数就没有启动，而外部管理线程关闭等待的线程却需要调用stopImmediately函数来回收_sockThread, 但需要run线程启动才能回收
//可能会导致_sockThread没有退出并回收，这时需要一个disconnect函数

// 连接到服务器
bool TcpSocket::ConnectServer(QString ip, QString port,QIODevice::OpenMode flag)
{
    //qDebug()<<"ConnectServer";
    //将任务跨线程派发给_sockThread线程执行

    _sockThread->start();
    //qDebug()<<"_sockThread启动";
    //将连接任务跨线程给_sockThread执行
    bool ret = false;
    //qDebug()<<"_sockThread run state : "<<_sockThread->isRunning();
    //阻塞等待连接
    QMetaObject::invokeMethod(
        this,                         // 任务接收对象（当前TcpSocket实例）
        "ConnectToServer",            // 要执行的方法名
        Qt::BlockingQueuedConnection, // 任务传递方式（阻塞式队列传递）
        Q_RETURN_ARG(bool, ret),      // 接收返回值的容器（把结果存入ret变量）
        Q_ARG(QString, ip),           // 参数1：服务器IP（自动转成Qt可传输格式）
        Q_ARG(QString, port),         // 参数2：端口号
        Q_ARG(QIODevice::OpenMode, flag) // 参数3：打开模式（如读写模式）
    );
    //qDebug()<<"invokeMethod";
    //连接成功
    if(ret)
    {
        qDebug()<<"服务器连接成功";
        this->start();//启动run线程
        //qDebug()<<"tcp run线程启动";
        _is_canRun = true;
    }
    else
    {
        qDebug()<<"服务器连接失败";
    }
    return ret;
}

// 立即停止线程
void TcpSocket::stopImmediately()
{
    //qDebug()<<"tcpThread stopImmediately";
    {
        QMutexLocker locker(&_iscanRun_mutex);      // 加锁保护共享变量
        _is_canRun = false;     // 设置停止标志
    }
    //qDebug()<<"_is_canRun : "<<_is_canRun;
    _sockThread->quit();        // 请求线程退出
    _sockThread->wait();        // 等待线程真正退出
    //qDebug()<<"_sockThread wait";
}

// 断开与服务器的连接
void TcpSocket::disconnectFromHost()
{
    if(this->isRunning())   // 如果本对象线程正在运行
    {
        QMutexLocker locker(&_iscanRun_mutex);
        _is_canRun = false;
    }

    if(_sockThread->isRunning())    // 如果套接字线程正在运行
    {
        _sockThread->quit();
        _sockThread->wait();
        qDebug()<<"_sockThread线程退出";
    }

    //断开readReady的连接
    disconnect(_tcpsocket,SIGNAL(readyRead()),this,SLOT(RecvFromSocket()));
    // 清空消息队列
    queue_recv.clear();
    queue_send.clear();
    audio_recv.clear();
}
// 获取本地IPv4地址
uint32_t TcpSocket::getLocalIPv4()
{
    //qDebug()<<"_tcpsocket->localAddress().toIPv4Address() : "<<QHostAddress(_tcpsocket->localAddress().toIPv4Address()).toString();
    //qDebug()<<"_tcpsocket->localAddress() : "<<QHostAddress(_tcpsocket->localAddress()).toString();
    if(_tcpsocket->isOpen())
        return _tcpsocket->localAddress().toIPv4Address();
    return 0;
}
// 获取错误字符串
QString TcpSocket::getErrorString()
{
    return _tcpsocket->errorString();
}
// 线程主函数（用于发送消息）
void TcpSocket::run()
{
    //----Degug----
    // MESG* data = (MESG*)malloc(sizeof(MESG));
    // qDebug()<<"data : "<<data;
    // data->data = nullptr;
    // data->data_len = 0;
    // data->msg_type = DEBUG;
    // queue_send.push_msg(data);
    //-------------
    //将队列中数据取出放到sendData中发送
    //qDebug()<<"TcpSocket::run()";
    while(true)
    {
        {
            QMutexLocker locker(&_iscanRun_mutex);
            if(_is_canRun == false) //检查运行标志
                return;
        }
        // 从发送队列取出消息
        MESG* send = queue_send.pop_msg();
        //qDebug()<<"send pop:"<<send;
        if(send == nullptr) // 队列为空则继续循环
            continue;
        //sleep(1000);
        //将数据丢入senddata中让_sockThread执行发送

        // 跨线程调用发送方法
        QMetaObject::invokeMethod(this,"SendData",Q_ARG(MESG*,send));
    }
}

//_sockThread执行接收服务器数据
void TcpSocket::RecvFromSocket()
{
    //$ type2 ip4 len4 data #
    //获取有多少数据可读
    qint64 availebytes = _tcpsocket->bytesAvailable();
    //qDebug()<<"availebytes : "<<availebytes;
    if(availebytes <= 0)
        return;
    //读取
    qint64 ret = _tcpsocket->read((char*)_recvbuf + _hasread,availebytes);
    if(ret <= 0)
    {
        WRITER_LOG("socket read failed");
        return;
    }
    _hasread += ret;    // 更新已读取字节数
    //qDebug()<<"_hasread : "<<_hasread;
    if(_hasread <= HEAD_MIN)//未达到一个包长度
    {
        WRITER_LOG("数据未达到一个包");
        return;
    }
    else
    {
        //取出data_size
        // 解析数据包长度
        uint32_t data_size = 0;
        qFromBigEndian<uint32_t>((char*)_recvbuf + 7,4,&data_size);
        //qDebug()<<"data_size : "<<data_size;

        // 分配消息结构体
        MESG* msg = (MESG*)malloc(sizeof(MESG));
        if(msg == nullptr)
        {
            WRITER_LOG("msg malloc failed");
            qDebug()<<"msg malloc failed";
            return;
        }

        // 检查是否收到完整数据包
        if(_hasread < HEAD_MIN + data_size + 1)
        {
            qDebug()<<"数据包未收全";
            qDebug()<<"_hasread : "<<_hasread <<" HEAD_MIN + data_size : "<<HEAD_MIN + data_size;
            return;
        }
        memset(msg,0,sizeof(MESG));
        //qDebug()<<"_recvbuf[0] : "<<_recvbuf[0]<<" _recvbuf[HEAD_MIN + data_size] : "<<_recvbuf[HEAD_MIN + data_size];

        // 检查包头包尾标记
        if(_recvbuf[0] == '$' && _recvbuf[HEAD_MIN + data_size] == '#')
        {
            //收到一个完整数据包
            WRITER_LOG("收到一个完整数据包");
            //注意 ！！！
            //调用qFromBigEndian时，不能使用MSG_TYPE类型枚举类型的type进去提取，枚举类型底层是int是4个字节，而uint16_t是两个字节，
            //这样会导致存进去低2字节是提取出来的类型，但高2字节是垃圾值

            // 解析消息类型
            uint16_t type;
            qFromBigEndian<uint16_t>((char*)_recvbuf + 1,2,&type);
            //qDebug()<<"收到一个完整数据包 type : "<<type;
            //创建/加入响应

            // 处理不同类型消息
            if(type == CREATE_MEETING_RESPONSE)
            {
                //创建和加入都是只有房间号 区别是加入房间的房间号可能是-1表示房间已满
                //qDebug()<<"CREATE_MEETING_RESPONSE || JOIN_MEETING_RESPONSE";

                // 处理会议创建/加入响应
                qint32 roomno = 0;
                qFromBigEndian<qint32>((char*)_recvbuf + HEAD_MIN,4,&roomno);
                msg->msg_type = (MSG_TYPE)type;
                msg->data_len = data_size;
                msg->data = (uchar*)malloc(data_size);
                if(msg->data == nullptr)
                {
                    free(msg);
                    WRITER_LOG("msg->data malloc failed");
                    qDebug()<<"msg->data malloc failed";
                    return;
                }
                memset(msg->data,0,data_size);
                memcpy(msg->data,&roomno,data_size);
                queue_recv.push_msg(msg);
            }
            else if(type == JOIN_MEETING_RESPONSE)
            {
                //服务器已经转为主机字节序了ntohl 到客户端直接提取就可以
                qint32 roomno = 0;
                memcpy(&roomno,(char*)_recvbuf + HEAD_MIN,data_size);
                msg->msg_type = (MSG_TYPE)type;
                msg->data_len = data_size;
                msg->data = (uchar*)malloc(data_size);
                if(msg->data == nullptr)
                {
                    free(msg);
                    WRITER_LOG("msg->data malloc failed");
                    qDebug()<<"msg->data malloc failed";
                    return;
                }
                memset(msg->data,0,data_size);
                memcpy(msg->data,&roomno,data_size);
                queue_recv.push_msg(msg);
            }
            else if(type == PARTNER_EXIT || type == PARTNER_JOIN_OTHER || type == CLOSE_CAMERA)
            {
                // 处理伙伴退出/加入其他/关闭摄像头通知
                //qDebug()<<"PARTNER_EXIT || PARTNER_JOIN_OTHER || CLOSE_CAMERA";
                uint32_t ip = 0;
                qFromBigEndian<uint32_t>((char*)_recvbuf + 3,4,&ip);
                msg->msg_type = (MSG_TYPE)type;
                msg->data_len = 0;
                msg->ip = ip;
                msg->data = nullptr;
                queue_recv.push_msg(msg);
            }
            //自己加入 需要获取其他人的ip
            else if(type == PARTNER_JOIN_SELF)
            {
                // 处理自己加入会议（获取其他参与者IP）
                msg->msg_type = (MSG_TYPE)type;
                uint32_t ip = 0;
                uint32_t pos = 0;
                msg->data_len = data_size;
                msg->data = (uchar*)malloc(data_size);
                if(msg->data == nullptr)
                {
                    free(msg);
                    WRITER_LOG("msg->data malloc failed");
                    qDebug()<<"msg->data malloc failed";
                    return;
                }
                memset(msg->data,0,data_size);
                //将data中所有的ip都解析出来
                for(uint32_t i = 0;i < data_size / sizeof(uint32_t);i++)
                {
                    qFromBigEndian<uint32_t>((char*)_recvbuf + pos + HEAD_MIN,4,&ip);
                    memcpy(msg->data,&ip,sizeof(uint32_t));
                    pos += sizeof(uint32_t);
                }
                queue_recv.push_msg(msg);
            }
            else if(type == IMG_RECV || type == AUDIO_RECV)
            {
                qDebug()<<"IMG_RECV || AUDIO_RECV";
                // 处理图像/音频数据
                msg->msg_type = (MSG_TYPE)type;
                QByteArray data((char*)_recvbuf + HEAD_MIN,data_size);
                QByteArray data_base64 = QByteArray::fromBase64(data);
                QByteArray data_unCompress = qUncompress(data_base64); //解压
                qDebug()<<"data_unCompress size :"<<data_unCompress.size();
                uint32_t ip = 0;
                qFromBigEndian<uint32_t>((char*)_recvbuf + 3,4,&ip);
                qDebug()<<"ip : "<<QHostAddress(ip).toString();
                msg->ip = ip;
                msg->data_len = data_unCompress.size();
                msg->data = (uchar*)malloc(msg->data_len);
                if(msg->data == nullptr)
                {
                    free(msg);
                    WRITER_LOG("msg->data malloc failed");
                    qDebug()<<"msg->data malloc failed";
                    return;
                }
                qDebug()<<"malloc success";
                memset(msg->data,0,msg->data_len);
                memcpy_s(msg->data,msg->data_len,data_unCompress.data(),msg->data_len);
                qDebug()<<"memcpy_s(msg->data,msg->data_len,data_unCompress.data(),msg->data_len)";
                if(type == AUDIO_RECV)
                {
                    qDebug()<<"AUDIO_RECV";
                    audio_recv.push_msg(msg);
                }
                else
                {
                    qDebug()<<"IMG_RECV";
                    queue_recv.push_msg(msg);
                }
            }
            else if(type == TEXT_RECV)
            {
                // 处理文本消息
                msg->msg_type = (MSG_TYPE)type;
                QByteArray data((char*)_recvbuf + HEAD_MIN,data_size);
                std::string data_unCompress = qUncompress(data).toStdString();
                uint32_t ip = 0;
                qFromBigEndian<uint32_t>((char*)_recvbuf + 3,4,&ip);
                msg->ip = ip;
                msg->data_len = data_unCompress.size();
                msg->data = (uchar*)malloc(msg->data_len);
                if(msg->data == nullptr)
                {
                    free(msg);
                    WRITER_LOG("msg->data malloc failed");
                    qDebug()<<"msg->data malloc failed";
                    return;
                }
                memset(msg->data,0,msg->data_len);
                memcpy_s(msg->data,msg->data_len,data_unCompress.data(),msg->data_len);
                queue_recv.push_msg(msg);
            }
        }
        else
        {
            WRITER_LOG("数据包格式错误");
            qDebug()<<"数据包格式错误";
            return;
        }
        //()<<"_hasread - (HEAD_MIN + 1 + (quint64)data_size)):"<<_hasread - (HEAD_MIN + 1 + (quint64)data_size);
        //if(_hasread - (HEAD_MIN + 1 + (quint64)data_size) > 0)
        // 移动剩余数据到缓冲区开头
        memmove_s(_recvbuf,4*MB,_recvbuf + HEAD_MIN + 1 + data_size,_hasread - (HEAD_MIN + 1 + (quint64)data_size));
        // else
        // {
        //     //qDebug()<<"清空缓冲区";
        //     memset(_recvbuf, 0, 4 * MB); // 清空缓冲区
        // }
        _hasread -= (HEAD_MIN + 1 + (quint64)data_size);
        //qDebug()<<"减后的_hasread : "<<_hasread;
    }
}

//_sockThread执行连接 // 连接到服务器（实际执行）
bool TcpSocket::ConnectToServer(QString ip, QString port,QIODevice::OpenMode flag)
{
    //qDebug()<<"ConnectToServer";
    if(_tcpsocket == nullptr)
        _tcpsocket = new QTcpSocket(this);
    //连接错误处理
    connect(_tcpsocket,SIGNAL(error(QAbstractSocket::SocketError)),this,SLOT(ErrorHandle(QAbstractSocket::SocketError)));
    // 尝试连接
    _tcpsocket->connectToHost(ip,port.toUShort(),flag);
    //qDebug()<<"connect";
    if(_tcpsocket->waitForConnected(5000))  // 等待5秒
    {
        //连接有数据可读取  // 连接成功，设置数据接收信号槽
        connect(_tcpsocket,SIGNAL(readyRead()),this,SLOT(RecvFromSocket()),Qt::UniqueConnection);
        return true;
    }
    WRITER_LOG("ConnectToServer 连接服务器失败");
    _tcpsocket->close();
    return false;

}

//_sockThread执行发送
// 发送数据
void TcpSocket::SendData(MESG *send)
{
    //queue_send.debugPrintf();
    if(_tcpsocket->state() == QTcpSocket::UnconnectedState)
    {
        qDebug()<<"未连接服务器";
        emit sendTextOver();
        if(send->data)
            free(send->data);
        if(send)
            free(send);
    }
    //格式$1+类型2+ip4+data_len4+data+#1
    //qDebug()<<"SendData";
    qint64 pos = 0;//已经写入的数据

    //$
    _sendbuf[pos++] = '$';
    //type  // 写入消息类型
    qToBigEndian<uint16_t>(send->msg_type,_sendbuf + pos);
    pos += 2;
    //获取本机ip
    uint32_t ip = _tcpsocket->localAddress().toIPv4Address();
    // 写入数据长度
    qToBigEndian<uint32_t>(ip,_sendbuf + pos);
    pos += 4;
    //data_len
    qToBigEndian<uint32_t>(send->data_len,_sendbuf + pos);
    pos += 4;

    //如果是加入会议需要把会议号转大端  // 特殊处理加入会议消息（转换会议号为网络字节序）
    if(send->msg_type == JOIN_MEETING)
    {
        uint32_t room;
        memcpy(&room,send->data,sizeof(uint32_t));
        qToBigEndian<uint32_t>(room,send->data);
    }
    //data  写入数据
    memcpy(_sendbuf + pos,send->data,send->data_len);
    pos += send->data_len;

    //#
    _sendbuf[pos++] = '#';
    //qDebug()<<"byte write : "<<pos;


    //发送数据
    qint64 hastowrite = pos;
    qint64 ret = 0,haswrite = 0;
    while(haswrite < hastowrite)
    {
        //qDebug()<<"start _tcpsocket->write";
        ret = _tcpsocket->write((char*)_sendbuf + haswrite,hastowrite - haswrite);
        //qDebug()<<"stop _tcpsocket->write";
        if(ret == -1)
        {
            if(_tcpsocket->error() == QTcpSocket::TemporaryError)
            {
                ret = 0;
                continue;
            }
            else
            {
                WRITER_LOG("发送失败");
                qDebug()<<"发送失败";
                break;
            }
        }
        haswrite += ret;
        //qDebug()<<"haswrite :" <<haswrite;
    }

    _tcpsocket->waitForBytesWritten();      // 等待数据发送完成
    //qDebug()<<"waitForBytesWritten";

    // 如果是文本消息，发送完成信号
    if(send->msg_type == TEXT_SEND)
    {
        emit sendTextOver();
    }
    // 释放资源
    if(send->data)
        free(send->data);
    //qDebug()<<"send : "<<send;
    if(send)
        free(send);
    //qDebug()<<"结束";
}

//_sockThread执行 // 关闭套接字
void TcpSocket::CloseSock()
{
    if(_tcpsocket && _tcpsocket->isOpen())
        _tcpsocket->close();
}
// 错误处理
void TcpSocket::ErrorHandle(QAbstractSocket::SocketError err)
{
    MESG* err_msg = (MESG*)malloc(sizeof(MESG));
    if(err_msg == nullptr)
    {
        WRITER_LOG("err_msg malloc failed");
        return;
    }
    memset(err_msg,0,sizeof(MESG));
    err_msg->data = nullptr;
    err_msg->data_len = 0;
    // 根据错误类型设置消息   
    if(err == QAbstractSocket::RemoteHostClosedError)
    {
        WRITER_LOG("远端关闭连接");
        err_msg->msg_type = REMOVE_HOST_CLOSE_ERROR;
    }
    else
    {
        WRITER_LOG("其他网络错误");
        err_msg->msg_type = OTHER_NET_ERROR;
    }
    queue_recv.push_msg(err_msg);
}
