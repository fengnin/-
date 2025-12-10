#include "audiooutput.h"
#include <QAudioOutput>
#include <QIODevice>
#include "netheader.h"
#include <QDebug>
#include <QHostAddress>
extern MSG_Queue<MESG> audio_recv;  // 声明全局音频接收消息队列（需确保线程安全）
// 定义 125ms 音频帧长度（1900 字节，基于 8kHz 采样率、16 位单声道）
#ifndef FRAME_LEN_125MS
#define FRAME_LEN_125MS 1900
#endif
AudioOutput::AudioOutput(QObject *parent)   // 构造函数，初始化 AudioOutput 对象（继承自 QThread）
    : QThread{parent}       // 调用基类 QThread 的构造函数
{
    // 配置音频格式
    QAudioFormat audioft;
    audioft.setByteOrder(QAudioFormat::LittleEndian);   // 小端字节序（x86 兼容）
    audioft.setChannelCount(1); // 单声道
    audioft.setCodec("audio/pcm");      // 未压缩的 PCM 格式
    audioft.setSampleRate(8000);        // 8kHz 采样率
    audioft.setSampleSize(16);          // 16 位采样精度
    audioft.setSampleType(QAudioFormat::SignedInt); // 有符号整数格式

    QAudioDeviceInfo info = QAudioDeviceInfo::defaultInputDevice(); // 获取默认音频输入设备
    if(!info.isFormatSupported(audioft))     // 检查设备是否支持当前格式
    {
        qDebug()<<"设备格式不支持";    // 调试输出
        return;
    }
    _audio = new QAudioOutput(audioft,this);     // 创建音频输出对象
    _audio_device = nullptr;    // 初始化音频设备
    // 连接状态变化信号到槽函数
    connect(_audio,SIGNAL(stateChanged(QAudio::State)),this,SLOT(handleStatusChange(QAudio::State)));
}
// 析构函数，释放资源
AudioOutput::~AudioOutput()
{
    if(_audio)
        delete _audio;  // 释放 QAudioOutput 对象
}
// 启动音频播放
void AudioOutput::startPlay()   // 检查是否已在运行
{
    if(_audio->state() == QAudio::ActiveState)
        return;
    _audio_device = _audio->start();    // 启动音频输出设备
    WRITER_LOG("开启播放"); // 记录日志
}
// 停止音频播放
void AudioOutput::stopPlay()    // 检查是否已停止
{
    if(_audio->state() == QAudio::StoppedState)
        return;
    {
        QMutexLocker locker(&_device_mutex);    // 使用互斥锁保护设备指针（线程安全）
        _audio_device = nullptr;    // 清空设备指针
    }
    _audio->stop(); // 停止音频输出
    WRITER_LOG("关闭播放"); // 记录日志
}

void AudioOutput::stopImmediately() // 立即停止线程（不等待当前帧播放完成）
{
    QMutexLocker locker(&_iscanRun_mutex);  // 使用互斥锁保护运行标志
    _is_canRun = false; // 设置运行标志为 false
}

void AudioOutput::setVolum(int vol)
{
    _audio->setVolume(vol / 100.0);
    // 设置音量（0-100）
}

QString AudioOutput::errorString()  // 获取错误描述
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
// 线程主循环（从消息队列读取数据并播放）
void AudioOutput::run()
{
    _is_canRun = true;  // 设置运行标志
    QByteArray pcmBuffer;   // 临时存储 PCM 数据
    while(true)
    {
        // 检查是否需要立即停止
        {
            QMutexLocker locker(&_iscanRun_mutex);
            if(_is_canRun == false)
            {
                stopPlay(); // 停止播放
                return; // 退出线程
            }
        }

        MESG* msg = audio_recv.pop_msg();   // 从消息队列弹出消息
        if(msg == nullptr)
            continue;   // 空消息跳过
        // 检查音频设备是否有效
        if(_audio_device)
        {
            pcmBuffer.append((char*)msg->data,msg->data_len);   // 将消息数据追加到缓冲区
            if(pcmBuffer.size() >= FRAME_LEN_125MS)//数据达到125毫秒才开始播放
            {
                qint64 ret = _audio_device->write(pcmBuffer.data(),FRAME_LEN_125MS);    // 写入音频设备（每次只写入 125ms 数据）s
                if(ret < 0)
                {
                    WRITER_LOG("写入播放设备时出错");
                    qDebug()<<"写入播放设备时出错 "<<_audio_device->errorString();
                    return;
                }
                emit spark(QHostAddress(msg->ip).toString());   // 发射信号（可能是播放进度通知）
                pcmBuffer = pcmBuffer.right(pcmBuffer.size() - ret);    // 移除已播放的数据（保留剩余数据）
            }
        }
        else
        {
            //设备为空 可能出错 或者 已经关闭设备
            audio_recv.clear(); // 设备无效时清空消息队列（可能是错误处理）
        }
        if(msg->data)   // 释放消息内存
            free(msg->data);
        if(msg)
            free(msg);
    }
}
// 处理音频状态变化
void AudioOutput::handleStatusChange(QAudio::State newstate)
{
    switch(newstate)
    {
    case QAudio::ActiveState:
        break;  // 活跃状态（无需处理）
    case QAudio::StoppedState:
        // 检查是否因错误停止
        if(_audio->error() != QAudio::NoError)
        {
            emit AudioOutputError(errorString());   // 发射错误信号
            stopPlay(); // 停止播放
            WRITER_LOG((char*)QString("播放设备出错,%1").arg(errorString()).data());
        }
        break;
    case QAudio::SuspendedState:
        break;  // 挂起状态（暂不处理）
    case QAudio::IdleState:
        break;  // 空闲状态（暂不处理）
    case QAudio::InterruptedState:
        break;  // 中断状态（暂不处理）
    }
}
