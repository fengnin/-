//这是一个聊天消息显示的C++头文件，使用了Qt框架的GUI功能来渲染聊天消息

#ifndef CHATMESSAGE_H
#define CHATMESSAGE_H

#include <QWidget>              //包含QWidget基类，提供基本窗口功能
#include <QPixmap>              // 包含QPixmap类，用于处理图像
#include <QMovie>               // 包含QMovie类，用于显示动画(如加载中...)
#include <QPaintEvent>          // 包含QPaintEvent类，用于处理绘制事件
#include <QLabel>               // 包含QLabel类，用于显示文本或图像

class ChatMessage : public QWidget      // ChatMessage类继承自QWidget，用于显示单条聊天消息
{
    Q_OBJECT                    // Qt元对象系统宏，启用信号槽机制等Qt特性
public:
    explicit ChatMessage(QWidget *parent = nullptr);            // 构造函数，explicit防止隐式转换，parent参数用于对象树管理
    enum User_Type           // 用户类型枚举，区分消息发送者
    {
        USER_SELF,// 自己发送的消息
        USER_OTHER,//对方
        USER_TIME,//时间
    };

public:
    void SendTextSuccess();         // 发送文本消息成功的回调函数
    void SetText(QString text,QString time,QString ip = "",User_Type type = USER_TIME);//外部用来设置文本消息的   // text: 消息内容 time: 时间戳  ip: IP地址  type: 用户类型
    QSize fontRect(QString,QString);//初始化各种尺寸
    inline QString time() {return _time;}       //获取时间字符串的便捷方法
protected:
    void paintEvent(QPaintEvent* event);        // 重写的绘制事件处理函数  event: 绘制事件对象
    QSize getTextSize(QString);                 // 获取文本尺寸  返回文本的尺寸大小

private:
    QPixmap _leftPix;                   // 左侧气泡图像
    QPixmap _rightPix;                  // 右侧气泡图像

    QMovie* _movieLoading;              // 加载动画对象
    QLabel* _movie;                     // 用于显示动画的标签 
    bool _isSendSuccess;                // 消息是否发送成功的标志
        
    QString _msg;                       // 消息内容
    QString _ip;                        // IP地址
    QString _type;                      // 消息类型
    QString _time;//时间戳
    QString _fmt_time;//格式化后的时间

    int _textMaxWidth;//文本最大宽度
    int _lineHeigth;//行宽
    int _spaceWidth;//整个聊天信息区域(包括头像，气泡，和空白区域) 去掉 文本宽度后的宽度 是一个固定宽度   用于如果文本宽度没有超过m_textWidth最大限制 可以将气泡缩小 如果文本宽度超过了  这个宽度加上最大文本宽度就是一个this->width
    int _bubbleWidth;//气泡框宽度
    //ip
    QRect _ipLeftRect;      // 左侧IP显示区域
    QRect _ipRightRect;     // 右侧IP显示区域
    //三角
    QRect _triangleLeftRect;        // 左侧气泡三角形区域
    QRect _triangleRightRect;       // 右侧气泡三角形区域
    //气泡框
    QRect _bubbleLeftRect;          // 左侧气泡区域
    QRect _bubbleRightRect;         // 右侧气泡区域
    //文本框
    QRect _textLeftRect;            // 左侧文本区域
    QRect _textRightRect;           // 右侧文本区域
    //头像
    QRect _iconLeftRect;            // 左侧头像区域
    QRect _iconRightRect;           // 右侧头像区域
};

#endif // CHATMESSAGE_H
