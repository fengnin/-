// Widget 类的头文件，作为主窗口类，集成了网络通信、音视频处理、消息收发等功能

#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>
#include "netheader.h"      // 自定义网络头文件
#include "chatmessage.h"    // 自定义消息显示组件
#include <QVideoFrame>      // 视频帧处理
#include <QCamera>          // 摄像头控制
#include <QMap>             // 存储伙伴（Partner）信息
QT_BEGIN_NAMESPACE
namespace Ui {      // 前置声明（减少编译依赖）
class Widget;
}
QT_END_NAMESPACE
class AudioOutput;
class AudioInput;
class Partner;
class MyVideoSurface;
class SendImg;
class TcpSocket;
class SendText;
class RecvSolve;

class Widget : public QWidget
{
    Q_OBJECT

public:
    Widget(QWidget *parent = nullptr);
    ~Widget();
    static QRect pos;       // 静态成员：可能用于记录窗口位置
signals:
    void push_text(MSG_TYPE,QString = "");      // 发送文本消息（带类型）
    void push_img(QImage);                      // 发送图像
    void startAudio();                          // 启动音频
    void stopAudio();                           // 停止音频
    void volumChange(int vol);                  // 音量变化
protected:
    //void keyPressEvent(QKeyEvent *e) override;
private slots:      // UI 按钮点击事件
    void on_joinButton_clicked();
    void on_create_Button_clicked();
    void on_exit_Button_clicked();
    void on_connect_Button_clicked();
    void on_horizontalSlider_valueChanged(int value);   // 音量滑块
    void on_sendmsg_clicked();                          // 发送消息按钮
    // 功能处理槽函数
    void DataSolve(MESG*);      // 处理接收到的消息（MESG 为自定义结构体）
    void SendTextOver();        // 文本发送完成（可能由 SendText 线程触发）
    void CameraImageCapture(QVideoFrame);   // 摄像头帧捕获
    void on_openVideo_clicked();            // 打开视频
    void on_openAudio_clicked();            // 打开音频
    void CameraError(QCamera::Error);       // 摄像头错误处理
    void AudioError(QString);               // 音频错误处理
    void Spark(QString);                 // 可能用于通知或特效
    void SwitchMainShow(uint32_t);      // 切换主显示（如切换当前聊天对象）
private:        // 内部工具函数
    void dealMessage(QString text,QString ip,QString time,ChatMessage::User_Type type);     
    void dealTime(QString time);        // 处理时间显示
    void clearPartner();//连接出错或者退出时调用
    void closeCamera(uint32_t ip);//关闭指定伙伴的摄像头
    Partner* addPartner(uint32_t ip);       // 添加新伙伴
    void removePartner(uint32_t ip);        // 移除伙伴
    void getPublicIP();                     // 获取公网 IP
private:
    Ui::Widget *ui;         // UI 界面指针
    // 功能模块（多线程设计）
    SendText* _sendtext;    // 文本发送模块
    QThread* _textThread;    // 文本发送线程
    SendImg* _sendimg;      // 图像发送模块
    QThread* _imgThread;    // 图像发送线程
    RecvSolve* _recvThread; // 消息接收与解析线程
    TcpSocket* _tcpsocket;   // TCP 套接字（网络通信）
    // 音视频模块
    MyVideoSurface* _myVideoSurface;    // 视频渲染表面
    QCamera* _camera;                   // 摄像头对象
    AudioInput* _audioInput;            // 音频输入
    QThread* _audioInputThread;         // 音频输入线程
    AudioOutput* _audioOutput;          // 音频输出
    // 状态标志
    bool _isSendTime;                   // 是否发送时间
    bool _isCreateMeet;                 // 是否创建会议
    bool _isJoinMeet;                   // 是否加入会议
    // 数据存储
    QString _mainip;        // 主 IP（可能是会议主持人 IP）
    QMap<uint32_t,Partner*> _partner;       // 伙伴列表（IP 到 Partner 对象的映射）
    QStringList _iplist;        // IP 列表（可能用于历史记录）
    QString _publicIp;//保存公网ip
};
#endif // WIDGET_H
