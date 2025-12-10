//这是一个音频输入处理的C++头文件，使用了Qt框架的音频处理功能


#ifndef AUDIOINPUT_H
#define AUDIOINPUT_H

#include <QObject>      //包含QObject基类，提供Qt对象模型的基础功能
#include <QAudio>       //包含Qt音频处理相关类
class QAudioInput;      //前向声明QAudioInput类，用于音频输入
class QIODevice;        //前向声明QIODevice类，用于I/O设备操作
class AudioInput : public QObject       //AudioInput类继承自QObject，用于处理音频输入
{
    Q_OBJECT        //Qt元对象系统宏，启用信号槽机制等Qt特性
public:
    explicit AudioInput(QObject *parent = nullptr);         //构造函数，explicit防止隐式转换，parent参数用于对象树管理
    ~AudioInput();                // 析构函数，用于清理资源

    QString errorString();//返回一个错误字符串
public slots:               //  公有槽函数，可以被外部直接调用或通过信号触发
    void setVolum(int vol);         // 设置音量大小
    void startCollect();            //开始采集音频数据
    void stopCollect();             //停止采集音频数据
private slots:                      //私有槽函数，通常用于内部处理     // 处理音频设备状态变化的槽函数
    void handleStatusChange(QAudio::State);//当设备状态出现变化 查看是否出错
    void onReadReady();              // 当有音频数据可读时的槽函数
signals:                            //信号，用于对象间通信
    void AudioInputError(QString);          //音频输入错误信号，当发生错误时发射
private:
    QAudioInput* _audio;//控制录音设备
    QIODevice* _audio_device;//录音设备
    char* _recvbuf;         //接收音频数据的缓冲区指针
};

#endif // AUDIOINPUT_H
