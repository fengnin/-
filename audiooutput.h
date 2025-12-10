//这是一个音频输出处理的C++头文件，同样使用了Qt框架的音频处理功能

#ifndef AUDIOOUTPUT_H
#define AUDIOOUTPUT_H

#include <QThread>              // 包含QThread类，用于多线程处理
#include <QAudio>               // 包含Qt音频处理相关类
#include <QMutex>               //包含QMutex类，用于线程同步
class QIODevice;                //前向声明QIODevice类，用于I/O设备操作
class QAudioOutput;             //前向声明QAudioOutput类，用于音频输出
class AudioOutput : public QThread          //AudioOutput类继承自QThread，用于处理音频输出
{
    Q_OBJECT                        // Qt元对象系统宏，启用信号槽机制等Qt特性
public:
    explicit AudioOutput(QObject *parent = nullptr);            // 构造函数，explicit防止隐式转换，parent参数用于对象树管理
    ~AudioOutput();                 // 析构函数，用于清理资源
    void startPlay();               //开始播放音频
    void stopPlay();                //停止播放音频
    void stopImmediately();         //立即停止播放音频
public slots:                       // 公有槽函数，可以被外部直接调用或通过信号触发
    void setVolum(int vol);         // 设置音量大小
private:
    QString errorString();           // 返回错误信息的字符串描述
    void run();                     // 重写的QThread运行函数，线程执行的主要逻辑
private slots:                      // 私有槽函数，通常用于内部处理       // 处理音频设备状态变化的槽函数
    void handleStatusChange(QAudio::State);         //当设备状态改变时被调用，用于检查是否出错
signals:                            //信号，用于对象间通信        
    void AudioOutputError(QString); // 音频输出错误信号，当发生错误时发射
    void spark(QString);            //自定义信号，可能用于触发某些操作或通知
private:
    QAudioOutput* _audio;           // 指向QAudioOutput对象的指针，用于控制播放设备
    QIODevice* _audio_device;       // 指向QIODevice对象的指针，表示播放设备
    QMutex _iscanRun_mutex;         // 互斥锁，用于保护_is_canRun变量的线程安全
    bool _is_canRun;                // 标志位，控制播放是否可以进行
    QMutex _device_mutex;           // 互斥锁，可能用于保护音频设备的线程安全操作
};

#endif // AUDIOOUTPUT_H
