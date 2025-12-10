//这段代码实现了一个聊天消息气泡控件 ChatMessage，继承自 QWidget

#include "chatmessage.h"
#include "QFontMetricsF"    // 用于计算文本尺寸
#include <QDateTime>        // 时间格式化
#include <QPainter>         // 绘图工具
// 构造函数，初始化聊天消息控件
ChatMessage::ChatMessage(QWidget *parent)
    : QWidget{parent}
{// 加载左右头像图片（默认使用同一张图片）
    _leftPix = QPixmap(":/1.jpg");
    _rightPix = QPixmap(":/1.jpg");

    //设置字体样式
    QFont ft = this->font();    // 获取当前字体
    ft.setFamily("MicrosoftYaHei"); // 设置字体家族
    ft.setPointSize(12);        // 设置字体大小
    this->setFont(ft);          // 应用字体设置
    // 初始化加载动画
    _movieLoading = new QMovie(":/3.gif");  // 创建动画对象
    _movie = new QLabel(this);      // 创建用于显示动画的标签
    _movie->setMovie(_movieLoading);    // 将动画设置到标签
    _movie->resize(40,40);          // 设置动画大小
    _movie->setScaledContents(true);//自动缩放自适应大小
    _movie->setAttribute(Qt::WA_TranslucentBackground);//背景透明
}
// 消息发送成功时调用，停止加载动画
void ChatMessage::SendTextSuccess()
{
    //发送成功关闭动画
    _movie->setHidden(true);    // 隐藏动画
    _movieLoading->stop();      // 停止动画
    qDebug()<<"关闭动画";       // 调试输出
}   
// 设置消息内容、时间、IP和发送者类型
void ChatMessage::SetText(QString text,QString time, QString ip, User_Type type)
{
    _msg = text;        // 保存消息内容
    _time = time;       // 保存时间戳
    _ip = ip;           // 保存IP地址
    _type = type;       // 保存发送者类型
    qDebug()<<"type : "<<type;  // 调试输出类型
    _fmt_time = QDateTime::fromTime_t(time.toInt()).toString("ddd hh:mm");  // 将时间戳转换为格式化时间字符串（如 "Mon 14:30"）
    // 如果是自己发送的消息，显示加载动画
    if(_type == USER_SELF)
    {
        //qDebug()<<"_isSendSuccess : "<<_isSendSuccess;
        qDebug()<<"加载动画启动";
        // 计算动画位置（放在气泡左侧）
        _movie->move(_bubbleRightRect.x() - _movie->width(),_bubbleRightRect.y() + _bubbleRightRect.height()/2 - _movie->height()/2 + 2);
        _movieLoading->start(); // 启动动画
        _movie->show();         // 显示动画
    }
    else
    {
        _movieLoading->stop();  // 停止动画
        _movie->hide();         // 隐藏动画
        //qDebug()<<"无需启动动画";
    }
    this->update();//启动paintEvent重新绘制
}

QSize ChatMessage::getTextSize(QString text)
{
    //用于测量文本的大小
    //qDebug()<<"text : "<<text;
    QFontMetricsF ft(this->font());     // 获取字体度量信息
    _lineHeigth = ft.lineSpacing();     // 获取行高
    int nCount = text.count("\n");//统计输入有几个换行
    int MaxWidth = 0;       // 初始化最大宽度
    int retHei = nCount;        // 初始化返回高度（基于换行符数量）
    // 如果没有换行符
    if(nCount == 0)
    {
        MaxWidth = ft.width(text);      // 计算文本宽度
        //qDebug()<<"ft.width(text)"<<ft.width(text);
        //没有换行符 但可能长度很长
        if(MaxWidth > _textMaxWidth)//进行切割分行
        {
            MaxWidth = _textMaxWidth;
            //统计分为几行
            int num = ft.width(text) / _textMaxWidth;   // 计算需要换行的次数
            retHei += num;      // 增加高度
        }
    }
    else
    {
        QStringList strlist = text.split("\n"); // 分割文本
        for(int i = 0;i < nCount + 1;i++)   // 遍历每一行
        {
            QString value = strlist.at(i);      // 更新最大宽度
            MaxWidth = ft.width(value) > MaxWidth ? ft.width(value) : MaxWidth;     // 如果某行过长
            if(ft.width(value) > _textMaxWidth)
            {
                //文本行只要有一行超过了 就用_textMaxWidth
                MaxWidth = _textMaxWidth;
                int num = ft.width(value) / _textMaxWidth;
                retHei += num;
            }
        }
    }
    //qDebug()<<"ret MaxWidth : "<<MaxWidth<<" _spaceWidth : "<<_spaceWidth;
    return QSize(MaxWidth + _spaceWidth,(retHei + 1) * _lineHeigth + 2 * _lineHeigth);  // 返回文本尺寸（宽度 + 边距，高度基于行数和行高）
}

// 计算各元素的布局（头像、气泡、文本等）
QSize ChatMessage::fontRect(QString text,QString ip)
{
    int minHei = 30;//如果是单行最小高度
    int iconWH = 40;//头像宽高
    int iconSpaceW = 20;//头像与边缘间距
    int iconRectW = 5;  // 头像与气泡间距
    int triangleW = 6;//小三角宽度
    int triangleH = 6;//小三角高度  实际高度没有作用
    int iconTMPH = 10;//头像和气泡相差 高度
    int textSpace = 12;//气泡内部文本的边距
    int bubbleTMP = 20;// 气泡与边缘的间距
    // 计算气泡最大宽度
    _bubbleWidth = this->width() - bubbleTMP - 2*(iconWH + iconRectW + iconSpaceW);
    _textMaxWidth = _bubbleWidth - 2*textSpace; // 文本最大宽度
    _spaceWidth = this->width() - _textMaxWidth;// 边距宽度

    // 设置头像位置
    _iconLeftRect = QRect(iconSpaceW,iconTMPH + 10,iconWH,iconWH);
    _iconRightRect = QRect(this->width() - iconWH - iconSpaceW,iconTMPH + 10,iconWH,iconWH);
    //qDebug()<<"this->width : "<<this->width();
    // 计算文本尺寸
    QSize size = getTextSize(text);
    //qDebug()<<"getTextSize size : "<<size;
    int hei = size.height() < minHei ? minHei : size.height();//计算出size后，取出size的高度和最小高度的较大值(气泡一行信息时的高度，不能再小)

    //小三角
    _triangleLeftRect = QRect(iconRectW + iconWH + iconSpaceW,_lineHeigth/2 + 10,triangleW,triangleH);
    _triangleRightRect = QRect(this->width() - iconWH - iconSpaceW - iconRectW - triangleW,_lineHeigth/2+10,triangleW,triangleH);

    //气泡框
    int textWidthCompensation = 0;//当文本太长需要换行就会导致偏左 加上这个补偿使其更美观
    if(size.width() < _spaceWidth + _textMaxWidth)
    {
        //qDebug()<<"if(size.width() < _spaceWidth + _textMaxWidth)";
        // 短文本布局
        _bubbleLeftRect = QRect(_triangleLeftRect.x() + triangleW,_lineHeigth/4*3 + 10,size.width() - _spaceWidth + 2*textSpace,hei-_lineHeigth);
        _bubbleRightRect = QRect(this->width() - iconRectW - iconSpaceW - iconWH - 2*textSpace - triangleW - size.width() + _spaceWidth,_lineHeigth/4*3 + 10,size.width() - _spaceWidth + 2*textSpace,hei-_lineHeigth);
        //qDebug()<<"_bubbleRightRect : "<<_bubbleRightRect;
    }
    else
    {
        //qDebug()<<"else";
        // 长文本布局
        _bubbleLeftRect = QRect(_triangleLeftRect.x() + triangleW,_lineHeigth/4*3 + 10,_bubbleWidth,hei-_lineHeigth);
        _bubbleRightRect = QRect(iconRectW + iconSpaceW + iconWH - triangleW + bubbleTMP,_lineHeigth/4*3+10,_bubbleWidth,hei-_lineHeigth);
        textWidthCompensation = 6;
    }

    //设置文本位置
    _textLeftRect = QRect(_bubbleLeftRect.x() + textSpace + textWidthCompensation,_bubbleLeftRect.y() + iconTMPH,_bubbleLeftRect.width() - 2*textSpace,_bubbleLeftRect.height() - 2*iconTMPH);
    _textRightRect = QRect(_bubbleRightRect.x() + textSpace + textWidthCompensation,_bubbleRightRect.y() + iconTMPH,_bubbleRightRect.width() - 2*textSpace,_bubbleRightRect.height() - 2*iconTMPH);
    //qDebug()<<"_textRightRect : "<<_textRightRect;
    // 设置IP位置
    QFontMetricsF ft(this->font());
    _ipLeftRect = QRect(iconRectW + iconSpaceW + iconWH + triangleW,_bubbleLeftRect.y() - iconTMPH - 10,ft.width(ip),20);
    _ipRightRect = QRect(this->width() - iconRectW - iconWH - iconSpaceW - triangleW - ft.width(ip),_bubbleRightRect.y() - iconTMPH - 10,ft.width(ip),20);
    //qDebug()<<"_ipRightRect : "<<_ipRightRect;
    return QSize(size.width(),hei+15);//返回一个尺寸 宽为消息总大小，高加15为了让QListWidgetItem自动调整消息间距
}


// 绘制消息气泡
void ChatMessage::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);    //忽略未使用的参数
    //qDebug()<<"paintEvent";
    QPainter painter(this); // 创建绘图对象
    painter.setPen(Qt::NoPen);//无边框
    painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);//抗锯齿和平滑图片缩放

    if(_type == USER_OTHER)//绘制对方信息
    {
        //头像
        painter.drawPixmap(_iconLeftRect,_leftPix);
        //气泡
        QPen bub_pen(Qt::gray);
        painter.setPen(bub_pen);
        QColor bub_col(Qt::white);
        painter.setBrush(bub_col);
        painter.drawRoundedRect(_bubbleLeftRect,4,4);

        //三角
        QPointF points[3] = {
            QPoint(_triangleLeftRect.x() + _triangleLeftRect.width(),35),
            QPoint(_triangleLeftRect.x() + _triangleLeftRect.width(),45),
            QPoint(_triangleLeftRect.x(),40)
        };
        QPen triangle_pen(bub_col);
        painter.setPen(triangle_pen);
        painter.drawPolygon(points,3);
        //三角形加边
        QPen triangle_line_pen(Qt::gray);
        painter.setPen(triangle_line_pen);
        painter.drawLine(QPoint(_triangleLeftRect.x(),40),QPoint(_triangleLeftRect.x() + _triangleLeftRect.width(),35));
        painter.drawLine(QPoint(_triangleLeftRect.x(),40),QPoint(_triangleLeftRect.x() + _triangleLeftRect.width(),45));
        //绘制ip地址
        QPen ip_pen;
        ip_pen.setColor(Qt::black);
        painter.setPen(ip_pen);
        QFont ft_ip(this->font());
        ft_ip.setFamily("MicrosoftYaHei");
        ft_ip.setPointSize(10);
        painter.setFont(ft_ip);
        QTextOption ip_op(Qt::AlignHCenter | Qt::AlignVCenter);
        painter.drawText(_ipLeftRect,_ip,ip_op);
        //绘制文本
        QPen text_pen(Qt::black);
        painter.setPen(text_pen);
        painter.setFont(this->font());
        QTextOption text_op(Qt::AlignLeft | Qt::AlignVCenter);
        text_op.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
        painter.drawText(_textLeftRect,_msg,text_op);
    }
    else if(_type == USER_SELF)//绘制自己信息
    {
        //头像
        painter.drawPixmap(_iconRightRect,_rightPix);
        qDebug()<<"_iconRightRect : "<<_iconRightRect;
        //气泡
        QColor bub_col(75,164,242);
        painter.setBrush(QBrush(bub_col));
        painter.drawRoundedRect(_bubbleRightRect,4,4);

        //三角
        QPointF points[3] = {
            QPoint(_triangleRightRect.x(),35),
            QPoint(_triangleRightRect.x(),45),
            QPoint(_triangleRightRect.x() + _triangleRightRect.width(),40)
        };
        QPen triangle_pen;
        triangle_pen.setColor(bub_col);
        painter.setPen(triangle_pen);
        painter.drawPolygon(points,3);
        //ip
        QPen ip_pen;
        ip_pen.setColor(Qt::black);
        painter.setPen(ip_pen);
        QFont ft_ip(this->font());
        ft_ip.setFamily("MicrosoftYaHei");
        ft_ip.setPointSize(10);
        painter.setFont(ft_ip);
        QTextOption ip_op(Qt::AlignHCenter | Qt::AlignVCenter);
        painter.drawText(_ipRightRect,_ip,ip_op);
        //文本
        QPen text_pen;
        text_pen.setColor(Qt::white);
        painter.setPen(text_pen);
        QTextOption text_op(Qt::AlignLeft | Qt::AlignVCenter);
        text_op.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
        painter.setFont(this->font());
        painter.drawText(_textRightRect,_msg,text_op);
        //qDebug()<<"_msg : "<<_msg;
    }
    else if(_type == USER_TIME)//绘制时间信息
    {
        QPen time_pen(QColor(153,153,153));
        painter.setPen(time_pen);
        QFont time_ft(this->font());
        time_ft.setFamily("MicrosoftYaHei");
        time_ft.setPointSize(10);
        painter.setFont(time_ft);
        QTextOption time_op(Qt::AlignCenter);
        time_op.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
        painter.drawText(this->rect(),_fmt_time,time_op);
    }
}

