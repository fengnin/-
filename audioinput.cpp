// AudioInput 类的实现文件，负责音频采集、格式设置、错误处理和数据发送。

#include "audioinput.h"
#include "netheader.h"      // 网络消息队列相关的头文件
#include <QAudioInput>      // Qt 音频输入类 
#include <QIODevice>        // Qt I/O 设备基类
#include <QDebug>           // Qt 调试输出
extern MSG_Queue<MESG> queue_send;  // 声明全局消息队列（需确保线程安全)

AudioInput::AudioInput(QObject *parent) // 构造函数，初始化 AudioInput 对象
    : QObject{parent}       // 调用基类 QObject 的构造函数
{
    QAudioFormat audioft;        // 配置音频格式
    audioft.setSampleRate(8000);//8000HZ    设置采样率为 8kHz（语音常用）       //sample样本
    audioft.setChannelCount(1);//1为单声道 2为立体声道           //channel 频道
    audioft.setSampleSize(16);//中等音质 还有8、24、32
    audioft.setSampleType(QAudioFormat::SignedInt);  // 有符号整数格式
    audioft.setCodec("audio/pcm");//接收一个未解压的音频，code编码
    audioft.setByteOrder(QAudioFormat::LittleEndian);   // 小端字节序（x86 兼容）

    // 获取默认音频输入设备
    QAudioDeviceInfo info = QAudioDeviceInfo::defaultInputDevice();
    // 检查设备是否支持当前格式
    if(!info.isFormatSupported(audioft))
    {
        qDebug()<<"录音设备不支持设置的格式，正在选择最接近的可用格式";
        audioft = info.nearestFormat(audioft);  // 使用最接近的可用格式
    }
    _audio = new QAudioInput(audioft,this); // 创建音频输入对象
    connect(_audio,SIGNAL(stateChanged(QAudio::State)),this,SLOT(handleStatusChange(QAudio::State)));   // 连接状态变化信号到槽函数
    _recvbuf = (char*)malloc(MB*2); // 分配接收缓冲区（2MB）
}

AudioInput::~AudioInput()   // 析构函数，释放资源
{
    delete _audio;  // 释放 QAudioInput 对象
    free(_recvbuf); // 释放接收缓冲区
}

void AudioInput::startCollect()
// 启动音频采集
{`
    if(_audio->state() == QAudio::ActiveState)//已经启动录音
        return;
    _audio_device = _audio->start();
    connect(_audio_device,SIGNAL(readyRead()),this,SLOT(onReadReady())); // 连接数据就绪信号到槽函数
    WRITER_LOG("启动录音");
}

void AudioInput::stopCollect()  // 停止音频采集
{
    if(_audio->state() == QAudio::StoppedState)//已经关闭
        return;
    _audio->stop(); // 停止音频输入
    disconnect(_audio_device,SIGNAL(readyRead()));  // 断开数据就绪信号
    WRITER_LOG("关闭录音"); // 记录日志
}

QString AudioInput::errorString()   // 获取错误描述
{
    if (!_audio) {
        return tr("Audio input is not initialized.");   // 音频未初始化
    }
    // 根据错误类型返回描述
    switch (_audio->error()) {
    case QAudio::NoError:
        return tr("No error occurred.");
    case QAudio::OpenError:
        return tr("An error occurred while opening the audio device.");
    case QAudio::IOError:
        return tr("An error occurred during read/write operations.");
    case QAudio::UnderrunError:
        return tr("Audio data underrun occurred.");
    case QAudio::FatalError:
        return tr("A fatal error occurred, and the audio device is not usable.");
    default:
        return tr("An unknown error occurred.");
    }
}
// 设置音量（0-100）
void AudioInput::setVolum(int vol)
{
    _audio->setVolume(vol / 100.0);
}
// 处理音频状态变化
void AudioInput::handleStatusChange(QAudio::State newstate)
{
    switch(newstate)
    {
    case QAudio::ActiveState:
        qDebug()<<"启动录音";   // 调试输出
        break;
    case QAudio::StoppedState:  // 检查是否因错误停止
        if(_audio->error() != QAudio::NoError)
        {
            emit AudioInputError(errorString());    // 发射错误信号
            WRITER_LOG((char*)QString("录音设备出错,%1").arg(errorString()).data());
            stopCollect();  // 停止采集
        }
        else
            qDebug()<<"关闭录音";//没出错时无需手动关闭 外部会关闭
        break;
    case QAudio::SuspendedState:
        break;      // 挂起状态（暂不处理）
    case QAudio::IdleState:
        break;      // 空闲状态（暂不处理）
    case QAudio::InterruptedState:
        break;      // 中断状态（暂不处理）
    }
}

void AudioInput::onReadReady()  // 处理音频数据就绪
{
    //当有数据就绪调用这个函数读取
    static int total = 0;   // 当前缓冲区数据量
    static int num = 0;     // 数据积累次数
    if(_audio_device == nullptr)    // 检查设备有效性 
        return;
    qint64 ret = _audio_device->read(_recvbuf + total,2*MB - total);    // 读取音频数据到缓冲区
    if(ret < 0)     // 处理读取错误
    {
        total = 0;  // 重置计数器
        //free(_recvbuf);
        qDebug()<<"_audio_device 出现异常";
        WRITER_LOG((char*)QString("_audio_device 出现异常 %1").arg(_audio_device->errorString()).data());
        return;
    }
    // if(total < KB)//数据量不足 继续接收
    //     return;
    if (num < 2)//积累两次数据后再处理
    {
        total += ret;   // 累加数据量
        num++;          // 增加计数
        return; 
    }
    total += ret;   // 积累足够数据后处理
    MESG* msg = (MESG*)malloc(sizeof(MESG));    // 分配消息结构体内存
    if(msg == nullptr)
    {
        total = 0;  // 重置计数器
        //free(_recvbuf);
        WRITER_LOG("msg malloc failed");
        qDebug()<<"msg malloc failed";
        return;
    }
    memset(msg,0,sizeof(MESG)); // 初始化消息结构体
    // 压缩音频数据
    QByteArray byte(_recvbuf,total);// 原始数据
    QByteArray byte_Compress = qCompress(byte).toBase64();// 压缩并编码
    // 填充消息内容
    msg->data_len = byte_Compress.size();   // 数据长度
    msg->msg_type = AUDIO_SEND;             // 消息类型
    msg->data = (uchar*)malloc(msg->data_len);  // 分配数据内存
    if(msg->data == nullptr)    // 检查数据内存分配
    {
        total = 0;
        //free(_recvbuf);
        WRITER_LOG("msg malloc failed");
        qDebug()<<"msg malloc failed";
        return;
    }
    // 复制压缩后的数据
    memset(msg->data,0,msg->data_len);
    memcpy_s(msg->data,msg->data_len,byte_Compress.data(),msg->data_len);
    // 发送消息到队列
    queue_send.push_msg(msg);
    total = 0;//置空 接收下一段
    num = 0;
}
