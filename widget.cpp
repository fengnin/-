#include "widget.h"
#include "ui_widget.h"  // 包含 UI 界面头文件
#include "logqueue.h"   // 日志队列
#include "sendtext.h"   // 文本发送线程
#include "tcpsocket.h"  // TCP 套接字
#include "recvsolve.h"  // 接收数据处理
#include "screen.h"     // 屏幕相关
#include "audioinput.h" // 音频输入
#include "sendimg.h"    // 图片发送
#include "partner.h"    // 伙伴管理
#include "audiooutput.h"    // 音频输出
#include "myvideosurface.h" // 视频表面
#include <QMessageBox>      // 消息框
#include <QSound>           // 声音播放
#include <QDateTime>        // 日期时间
#include <QHostAddress>     // 主机地址
#include <QListWidgetItem>  // 列表项
#include <QNetworkAccessManager>    // 网络访问管理
#include <QNetworkReply>    // 网络回复
#include <QSslSocket>       // SSL 套接字
#include <QJsonDocument>    // JSON 文档

// 全局日志队列指针
extern LogQueue *logqueue;

// 静态成员初始化
QRect Widget::pos = QRect(-1,-1,-1,-1);

// 构造函数
Widget::Widget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Widget)
{
    ui->setupUi(this);  // 设置 UI
    logqueue = new LogQueue();  // 创建日志队列
    logqueue->start();      // 启动日志线程

    // 初始化按钮状态
    ui->exit_Button->setEnabled(false);
    ui->create_Button->setEnabled(false);
    ui->joinButton->setEnabled(false);
    ui->openAudio->setEnabled(true);
    ui->openVideo->setEnabled(false);
    ui->connect_Button->setEnabled(true);
    ui->sendmsg->setEnabled(false);
    ui->horizontalSlider->setEnabled(true);

    // if (!QSslSocket::supportsSsl()) {
    //     qDebug() << "SSL/TLS not supported!";
    //     qDebug() << "SSL version used for build:" << QSslSocket::sslLibraryBuildVersionString();
    //     qDebug() << "SSL version used at runtime:" << QSslSocket::sslLibraryVersionNumber();
    // }
    getPublicIP();//获取公网ip
    _isCreateMeet = false;  // 标记是否创建会议
    _isJoinMeet = false;    // 标记是否加入会议
    _mainip = "";           // 主 IP 初始化
    WRITER_LOG("----------WRITER LOG START----------");     // 写入日志

    //Screen    // 屏幕相关设置
    Widget::pos = QRect(0.1 * Screen::Width,0.1 * Screen::Height,0.8 * Screen::Width,0.8 * Screen::Height);
    this->setGeometry(pos);     // 设置窗口几何
    this->setMinimumSize(QSize(Widget::pos.width() * 0.7,Widget::pos.height() * 0.7));      // 最小尺寸
    this->setMaximumSize(QSize(Widget::pos.width(),Widget::pos.height()));  // 最大尺寸

    //tcp线程
    _tcpsocket = new TcpSocket();
    connect(_tcpsocket,SIGNAL(sendTextOver()),this,SLOT(SendTextOver()));   // 连接信号槽
    //发送文本线程
    _sendtext = new SendText();
    _sendtext->start();     // 启动线程
    _textThread = new QThread();
    _sendtext->moveToThread(_textThread);       // 移动到线程
    _textThread->start();       // 启动线程
    connect(this,SIGNAL(push_text(MSG_TYPE,QString)),_sendtext,SLOT(Push_Text(MSG_TYPE,QString)));  // 连接信号槽

    //myvideosurface 和 配置QCmaera
    // 视频相关初始化
    _myVideoSurface = new MyVideoSurface(this);
    connect(_myVideoSurface,SIGNAL(frameAvailable(QVideoFrame)),this,SLOT(CameraImageCapture(QVideoFrame)));        // 连接信号槽
    _camera = new QCamera(this);    
    _camera->setViewfinder(_myVideoSurface);//将摄像头重定向到_myVideoSurface 设置后会自动调用_myVideoSurface中的supportedPixelFormats 启动摄像头会自动调用设置后会自动调用_myVideoSurface中的start
    _camera->setCaptureMode(QCamera::CaptureStillImage);        // 设置捕获模式
    connect(_camera,SIGNAL(error(QCamera::Error)),this,SLOT(CameraError(QCamera::Error)));  // 连接错误信号
    //发送视频帧/图片线程
    _sendimg = new SendImg();
    _sendimg->start();//执行run发送视频帧  // 启动线程
    _imgThread = new QThread();//执行push_img
    _sendimg->moveToThread(_imgThread); // 移动到线程
    //_imgThread->start()等开启视频再开启 开启摄像头才需要发送视频帧
    connect(this,SIGNAL(push_img(QImage)),_sendimg,SLOT(Push_Img(QImage))); // 连接信号槽

    //接收线程
    _recvThread = new RecvSolve();
    _recvThread->start();       // 启动线程
    //BlockingQueuedConnection用于信号和槽为不同线程  是同步 信号一旦发出会等槽函数执行完毕再继续发
    connect(_recvThread,SIGNAL(dataDispatch(MESG*)),this,SLOT(DataSolve(MESG*)),Qt::BlockingQueuedConnection);  // 连接信号槽

    //录音线程 和 播放
    _audioInput = new AudioInput();
    _audioInputThread = new QThread();
    _audioInput->moveToThread(_audioInputThread);   // 移动到线程
    _audioInputThread->start();//执行监听是否有数据可读取 并且运行startCollect和stopCollect  // 启动线程
    _audioOutput = new AudioOutput();
    _audioOutput->start();
    connect(_audioInput,SIGNAL(AudioInputError(QString)),this,SLOT(AudioError(QString)));   // 连接错误信号
    connect(_audioOutput,SIGNAL(AudioOutputError(QString)),this,SLOT(AudioError(QString))); // 连接错误信号
    connect(this,SIGNAL(startAudio()),_audioInput,SLOT(startCollect()));    // 连接信号槽
    connect(this,SIGNAL(stopAudio()),_audioInput,SLOT(stopCollect()));      // 连接信号槽
    connect(_audioOutput,SIGNAL(spark(QString)),this,SLOT(Spark(QString))); // 连接信号槽

    //文本编辑器回车发送信息
    connect(ui->plaintextedit,SIGNAL(sendmsg()),this,SLOT(on_sendmsg_clicked()));
    //注册自定义类型使其能通过信号发送参数
    qRegisterMetaType<MSG_TYPE>("MSG_TYPE");
    qRegisterMetaType<MESG*>("MESG*");

    //debug
    // QStringList strlist;
    // strlist.push_back("@123456");
    // ui->plaintextedit->setCompletionList(strlist);
}
// 析构函数
Widget::~Widget()
{
    // ----------------用于调试，关闭tcp中的_sockThread----------------
    //_tcpsocket->disconnectFromHost();
    //----------------后续写在退出函数 和 REMOVE_HOST_CLOSE_ERROR 和 OTHER_ERROR
    //qDebug()<<"_tcpsocket->isRunning() : "<<_tcpsocket->isRunning();


    // 停止所有线程并等待
    if(_tcpsocket->isRunning())
    {
        qDebug()<<"_tcpsocket->isRunning()";
        _tcpsocket->stopImmediately();
        _tcpsocket->wait();
        qDebug()<<"_tcpsocket wait";
    }
    if(_sendtext->isRunning())
    {
        _sendtext->stopImmediately();
        _sendtext->wait();
        qDebug()<<"_sendtext wait";
    }
    if(_textThread->isRunning())
    {
        _textThread->quit();
        _textThread->wait();
        qDebug()<<"_textThread wait";
    }
    if(_recvThread->isRunning())
    {
        _recvThread->stopImmediately();
        _recvThread->wait();
        qDebug()<<"_recvThread wait";
    }
    if(_sendimg->isRunning())
    {
        _sendimg->stopImmediately();
        _sendimg->wait();
        qDebug()<<"_sendimg wait";
    }
    if(_imgThread->isRunning())
    {
        _imgThread->quit();
        _imgThread->wait();
        qDebug()<<"_imgThread wait";
    }
    if(_audioInputThread->isRunning())
    {
        _audioInputThread->quit();
        _audioInputThread->wait();
        qDebug()<<"_audioInputThread wait";
    }
    if(_audioOutput->isRunning())
    {
        _audioOutput->stopImmediately();
        _audioOutput->wait();
        qDebug()<<"_audioOutput wait";
    }
    WRITER_LOG("----------WRITER LOG STOP----------");  // 写入日志
    // 停止日志队列
    if(logqueue->isRunning())
    {
        //qDebug()<<"logqueue->isRunning : "<<logqueue->isRunning();
        logqueue->stopImmediately();
        logqueue->wait();
        qDebug()<<"logqueue wait";
    }

    //在这里释放_tcpsocket会导致下面这个原因 一个线程在被销毁时会自动关闭所有关联的定时器，但是一个线程创建在另一个线程销毁 会导致定时器没有正常停止
    //QObject::killTimer: Timers cannot be stopped from another thread
    //QObject::~QObject: Timers cannot be stopped from another thread

     // 释放资源
    delete _tcpsocket;
    _sendimg->deleteLater();
    delete _textThread;
    delete _imgThread;
    _sendtext->deleteLater();
    delete _recvThread;
    delete _myVideoSurface;
    delete _camera;
    //因为_audioInput对象中槽函数都在_audioInputThread线程中执行，所以其他线程不能直接调用_audioInput对象中的方法，delete会调用析构所以崩溃
    //delete _audioInput;
    _audioInput->deleteLater();//这个不会直接删除对象，而是向消息循环线程中发送了一个删除事件，下一次循环就会删除该对象并且置为空
    delete _audioInputThread;
    delete _audioOutput;
    delete ui;
}

// 加入会议按钮点击事件
void Widget::on_joinButton_clicked()
{
    qDebug()<<"on_joinButton_clicked";
    QString meetno = ui->meetno_edit->text();   // 获取会议号
    //正则表达式验证房间号合法性
    QRegularExpression regex("^([1-9]\\d{0,5}|[1-3]\\d{6}|4[0-1]\\d{5}|419[0-3]\\d{4}|41940[0-9]\\d{2}|41941[0-9]\\d{2}|419420[0-9]\\d|419421[0-9]\\d|419422[0-9]\\d|4194230[0-4])$");
    QRegularExpressionMatch match = regex.match(meetno);
    if(!match.hasMatch())
    {
        QMessageBox::warning(this,"房间号不合法","请输入正确的格式");
        return;
    }
    else
    {
        qDebug()<<"发送push_text信号";
        emit push_text(JOIN_MEETING,meetno);
        WRITER_LOG("加入会议中...");
    }
}
// 创建会议按钮点击事件
void Widget::on_create_Button_clicked()
{
    emit push_text(CREATE_MEETING); // 发送创建会议信号
    ui->logout->setText("正在创建房间...");
    WRITER_LOG("创建房间中...");
    //ui->roomno->setText("房间号为 : 45555");
    //ui->mainshow_label->setPixmap(QPixmap::fromImage(QImage(":/1.jpg").scaled(ui->mainshow_label->size())));
}

//退出只需处理完自己客户端即可  发送通知给其他会议伙伴是服务器的事

// 退出会议按钮点击事件
void Widget::on_exit_Button_clicked()
{
    //退出
    // 更新按钮状态
    ui->create_Button->setEnabled(false);
    ui->joinButton->setEnabled(false);
    ui->connect_Button->setEnabled(true);
    ui->openAudio->setEnabled(false);
    ui->openVideo->setEnabled(false);
    ui->logout->setText("断开连接");
    ui->groupBox_6->setTitle("主屏幕");
    ui->roomno->setText("");
    ui->exit_Button->setEnabled(false);
    //断开tcp连接
    _tcpsocket->disconnectFromHost();
    _tcpsocket->wait();//点击连接会启动线程 断开连接需要回收线程
    QMessageBox::information(this,"退出会议","断开服务器连接");
    WRITER_LOG("退出会议,断开服务器连接");
    clearPartner();     // 清除伙伴
    // 清空 IP 列表
    if(!_iplist.isEmpty())
    {
        _iplist.clear();
        ui->plaintextedit->setCompletionList(_iplist);
    }
}
// 连接服务器按钮点击事件
void Widget::on_connect_Button_clicked()
{

    QString ip = ui->ip_edit->text();   // 获取 IP
    QString port = ui->port_edit->text();   // 获取端口
    //ip和port的正则表达式验证合法性
    QRegExp regip("^(25[0-5]|2[0-4][0-9]|1[0-9]{2}|[1-9]?[0-9])(\\.(25[0-5]|2[0-4][0-9]|1[0-9]{2}|[1-9]?[0-9])){3}$");
    QRegExp regport("^([0-9]{1,4}|[1-5][0-9]{4}|6[0-4][0-9]{3}|65[0-4][0-9]{2}|655[0-2][0-9]|6553[0-5])$");
    QRegExpValidator validator_ip(regip),validator_port(regport);
    int pos = 0;

    // 验证 IP
    if(validator_ip.validate(ip,pos) != QRegExpValidator::Acceptable)
    {
        ui->logout->setText("IP错误...");
        QMessageBox::warning(this,"IP格式错误","IP格式不合法");
        return;
    }
    pos = 0;
    // 验证端口
    if(validator_port.validate(port,pos) != QRegExpValidator::Acceptable)
    {
        ui->logout->setText("Port错误...");
        QMessageBox::warning(this,"Port格式错误","Port格式不合法");
        return;
    }

    //验证完毕 开始连接
    //qDebug()<<"格式正确";
    // 尝试连接服务器
    if(_tcpsocket->ConnectServer(ip,port,QIODevice::ReadWrite))
    {
        //连接成功
        ui->logout->setText("连接服务器成功");
        QMessageBox::information(this,"连接成功","连接服务器成功");
        ui->connect_Button->setEnabled(false);
        ui->create_Button->setEnabled(true);
        ui->exit_Button->setEnabled(true);
        ui->joinButton->setEnabled(true);
        ui->sendmsg->setEnabled(true);
        WRITER_LOG("连接服务器成功");
    }
    else
    {
        // 连接失败
        ui->logout->setText("网络错误...");
        QMessageBox::warning(this,"网络错误",_tcpsocket->getErrorString());
        ui->connect_Button->setEnabled(true);
        ui->create_Button->setEnabled(false);
        ui->exit_Button->setEnabled(false);
        ui->joinButton->setEnabled(false);
        ui->sendmsg->setEnabled(false);
        WRITER_LOG("连接服务器失败");
    }
}


// 发送消息按钮点击事件
void Widget::on_sendmsg_clicked()
{
    qDebug()<<"on_sendmsg_clicked";
    QString text = ui->plaintextedit->ToPlainText().trimmed();  // 获取文本并去除空格
    // 检查是否为空
    if(text.isEmpty())
    {
        QMessageBox::warning(this,"错误提醒","无法发送空白内容");
        return;
    }
    qDebug()<<"text : "<<text;
    QString time = QString::number(QDateTime::currentDateTimeUtc().toTime_t());     // 获取当前时间
    dealTime(time);     // 处理时间
    dealMessage(text,QHostAddress(_tcpsocket->getLocalIPv4()).toString(),time,ChatMessage::USER_SELF);  // 处理消息
    ui->plaintextedit->setPlainText("");    // 清空输入框

    emit push_text(TEXT_SEND,text);  // 发送文本
}

// 数据处理
void Widget::DataSolve(MESG *msg)
{
    qDebug()<<"msg->type : "<<msg->msg_type;
    // 处理创建会议响应
    if(msg->msg_type == CREATE_MEETING_RESPONSE)
    {
        qint32 roomno;
        memcpy(&roomno,msg->data,msg->data_len);
        if(roomno > 0)
            // 创建成功
        {
            //static int count = 0;
            //qDebug()<<"CREATE_MEETING_RESPONSE count : "<<++count;
            //创建成功
            //创建成功需要添加partner
            QMessageBox::information(this,"创建成功",QString("房间号为 : %1").arg(roomno));
            _isCreateMeet = true;
            ui->create_Button->setEnabled(false);
            ui->joinButton->setEnabled(false);
            ui->exit_Button->setEnabled(true);
            ui->openAudio->setEnabled(false);
            ui->openVideo->setEnabled(true);
            ui->horizontalSlider->setEnabled(true);
            ui->sendmsg->setEnabled(true);
            ui->logout->setText(QString("房间号为 : %1").arg(roomno));
            ui->roomno->setText(QString("房间号为 : %1").arg(roomno));
            ui->mainshow_label->setPixmap(QPixmap::fromImage(QImage(":/1.jpg").scaled(ui->mainshow_label->size())));
            _mainip = QHostAddress(_tcpsocket->getLocalIPv4()).toString();
            ui->groupBox_6->setTitle(_mainip);
            //添加自己
            addPartner(_tcpsocket->getLocalIPv4());
            ui->plaintextedit->setCompletionList(QStringList());//创建房间后需要可以发信息  清空补全列表
            WRITER_LOG("创建房间成功 房间号为 : %d",roomno);
        }
        else
        {
            //创建失败
            QMessageBox::warning(this,"创建失败","无房间可用");
            _isCreateMeet = false;
            ui->create_Button->setEnabled(true);
            ui->joinButton->setEnabled(true);
            ui->exit_Button->setEnabled(false);
            ui->openAudio->setEnabled(false);
            ui->openVideo->setEnabled(false);
            ui->horizontalSlider->setEnabled(false);
            ui->sendmsg->setEnabled(false);
            ui->logout->setText("无房间可用");
            WRITER_LOG("创建房间失败 无房间可用");
        }
    }
    // 处理加入会议响应
    else if(msg->msg_type == JOIN_MEETING_RESPONSE)
    {
        qint32 roomno;
        qDebug()<<"JOIN_MEETING_RESPONSE len : "<<msg->data_len;

        memcpy(&roomno,msg->data,msg->data_len);
        qDebug()<<"JOIN_MEETING_RESPONSE  roomno : " << roomno;
        if(roomno > 0)
        {
            //加入成功需要添加partner
            QMessageBox::information(this,"加入成功","加入会议成功");
            _isJoinMeet = true;
            ui->create_Button->setEnabled(false);
            ui->joinButton->setEnabled(false);
            ui->exit_Button->setEnabled(true);
            ui->openAudio->setEnabled(true);
            ui->openVideo->setEnabled(true);
            ui->horizontalSlider->setEnabled(true);
            ui->sendmsg->setEnabled(true);
            ui->mainshow_label->setPixmap(QPixmap::fromImage(QImage(":/1.jpg").scaled(ui->mainshow_label->size())));
            _mainip = QHostAddress(_tcpsocket->getLocalIPv4()).toString();
            ui->groupBox_6->setTitle(_mainip);
            ui->logout->setText(QString("房间号为 : %1").arg(roomno));
            ui->roomno->setText(QString("房间号为 : %1").arg(roomno));
            //添加自己
            addPartner(_tcpsocket->getLocalIPv4());
            WRITER_LOG("加入房间成功 房间号为 : %d",roomno);
        }
        else if(roomno <= 0)//失败
        {
            _isJoinMeet = false;
            ui->create_Button->setEnabled(true);
            ui->joinButton->setEnabled(true);
            ui->exit_Button->setEnabled(false);
            ui->openAudio->setEnabled(false);
            ui->openVideo->setEnabled(false);
            ui->horizontalSlider->setEnabled(false);
            ui->sendmsg->setEnabled(false);
            if(roomno == 0)//房间号不存在
            {
                QMessageBox::warning(this,"加入失败","房间号不存在");
                WRITER_LOG("加入失败 房间号不存在");
                ui->logout->setText("房间号不存在");
            }
            else if(roomno == -1)//房间已满
            {
                QMessageBox::warning(this,"加入失败","房间已满");
                WRITER_LOG("加入失败 房间已满");
                ui->logout->setText("房间已满");
            }
        }
    }
    // 处理伙伴退出
    else if(msg->msg_type == PARTNER_EXIT)
    {
        qDebug()<<"PARTNER_EXIT";
        removePartner(msg->ip);
        if(_mainip == msg->ip)
            ui->mainshow_label->setPixmap(QPixmap());
        ui->logout->setText(QString("伙伴%1退出会议").arg(QHostAddress(msg->ip).toString()));
        if(_iplist.contains(QString('@')+QHostAddress(msg->ip).toString()))
        {
            _iplist.removeOne(QString('@')+QHostAddress(msg->ip).toString());
            ui->plaintextedit->setCompletionList(_iplist);
        }
        WRITER_LOG("伙伴%s退出会议",QHostAddress(msg->ip).toString().data());
    }
    //通知其他人有人加入 // 处理其他伙伴加入
    else if(msg->msg_type == PARTNER_JOIN_OTHER)
    {
        Partner* p = addPartner(msg->ip);   // 添加伙伴
        if(p)
        {
            p->setPic(QImage(":/1.jpg"));
            ui->logout->setText(QString("%1 加入房间").arg(msg->ip));
        }
        QString str_ip = QString('@')+QHostAddress(msg->ip).toString();
        _iplist.push_back(str_ip);
        ui->plaintextedit->setCompletionList(_iplist);
        ui->logout->setText(QString("伙伴%1加入会议").arg(QHostAddress(msg->ip).toString()));
        WRITER_LOG("伙伴%s加入会议",QHostAddress(msg->ip).toString().data());
    }
    //告诉自己会议中有谁 // 处理自己加入会议
    else if(msg->msg_type == PARTNER_JOIN_SELF)
    {
        uint32_t ip;
        int pos = 0;
        int count = msg->data_len / sizeof(uint32_t);//ip个数
        for(int i = 0;i < count;i++)
        {
            memcpy_s(&ip,sizeof(uint32_t),msg->data + pos,sizeof(uint32_t));
            QString str_ip = QString('@')+QHostAddress(ip).toString();
            _iplist.push_back(str_ip);
            ui->plaintextedit->setCompletionList(_iplist);
            Partner* p = addPartner(ip);
            if(p)
            {
                qDebug()<<"添加partner成功";
                p->setPic(QImage(":/1.jpg"));
                pos += sizeof(uint32_t);
            }
            else
            {
                qDebug()<<"添加partner失败";
            }
        }
        ui->openAudio->setEnabled(true);
        ui->openVideo->setEnabled(true);
    }
    // 处理关闭摄像头
    else if(msg->msg_type == CLOSE_CAMERA)
    {
        qDebug()<<"CLOSE_CAMERA";
        closeCamera(msg->ip);   // 关闭摄像头
    }
    // 处理图片接收
    else if(msg->msg_type == IMG_RECV)
    {
        QImage img;
        img.loadFromData(msg->data,msg->data_len);  // 加载图片数据
        if(_partner.contains(msg->ip))//已经添加
        {
            Partner* p = _partner[msg->ip];
            if(p)       // 设置图片
                p->setPic(img);
        }
        else
        {
            Partner* p = addPartner(msg->ip);       // 添加伙伴
            if(p)
                p->setPic(img);         // 设置图片
        }
    }
    // 处理文本接收
    else if(msg->msg_type == TEXT_RECV)
    {
        QString text = QString::fromStdString(std::string((char*)msg->data,msg->data_len)); // 转换为字符串
        QString time = QString::number(QDateTime::currentDateTimeUtc().toTime_t()); // 获取当前时间
        dealTime(time); // 处理时间
        dealMessage(text,QHostAddress(msg->ip).toString(),time,ChatMessage::USER_OTHER);    // 处理消息
        //qDebug()<<"_publicIp : "<<_publicIp;
        if(text.contains(QString('@') + _publicIp))//如果@我 特殊音效
        {
            qDebug()<<"播放音效";
            QSound::play(":/2.wav");    // 播放声音
        }
    }
    // 处理主机关闭错误
    else if(msg->msg_type == REMOVE_HOST_CLOSE_ERROR)
    {
        _tcpsocket->disconnectFromHost();
        _tcpsocket->wait();

        if(_isCreateMeet || _isJoinMeet)
        {
            //如果在会议中
            _isCreateMeet = _isJoinMeet = false;
            ui->groupBox_6->setTitle("主窗口");
            ui->roomno->setText("");
            QMessageBox::warning(this,"会议异常结束","断开连接");
            ui->logout->setText("会议异常结束");
            WRITER_LOG("会议异常结束");
        }
        else
        {
            QMessageBox::warning(this,"服务器异常","断开连接");
            ui->logout->setText("远端断开连接");
            WRITER_LOG("服务器断开连接");
        }
        if(!_iplist.isEmpty())
        {
            _iplist.clear();
            ui->plaintextedit->setCompletionList(_iplist);
        }
        ui->create_Button->setEnabled(false);
        ui->joinButton->setEnabled(false);
        ui->exit_Button->setEnabled(false);
        ui->openAudio->setEnabled(false);
        ui->openVideo->setEnabled(false);
        ui->horizontalSlider->setEnabled(false);
        ui->sendmsg->setEnabled(false);
        ui->connect_Button->setEnabled(true);

        //清除聊天记录 和 partner
        clearPartner();
        while(ui->listWidget->count() > 0)
        {
            QListWidgetItem *item = ui->listWidget->takeItem(0);//取出第一个并且删除
            ChatMessage* message = (ChatMessage*)ui->listWidget->itemWidget(item);
            delete item;
            delete message;
        }
    }
    // 处理其他网络错误
    else if(msg->msg_type == OTHER_NET_ERROR)
    {
        QMessageBox::warning(this,"网络异常","网络错误");
        WRITER_LOG("网络异常");
        clearPartner();
        _tcpsocket->disconnectFromHost();
        _tcpsocket->wait();
        ui->logout->setText("网络异常错误");
        ui->create_Button->setEnabled(false);
        ui->joinButton->setEnabled(false);
        ui->exit_Button->setEnabled(false);
        ui->openAudio->setEnabled(false);
        ui->openVideo->setEnabled(false);
        ui->horizontalSlider->setEnabled(false);
        ui->sendmsg->setEnabled(false);
        ui->connect_Button->setEnabled(true);
        while(ui->listWidget->count() > 0)
        {
            QListWidgetItem *item = ui->listWidget->takeItem(0);//取出第一个并且删除
            ChatMessage* message = (ChatMessage*)ui->listWidget->itemWidget(item);
            delete item;
            delete message;
        }
        if(!_iplist.isEmpty())
        {
            _iplist.clear();
            ui->plaintextedit->setCompletionList(_iplist);
        }
    }
    // 释放内存
    if(msg->data)
        free(msg->data);
    if(msg)
        free(msg);
}
// 文本发送完成
void Widget::SendTextOver()
{
    QListWidgetItem* item = ui->listWidget->item(ui->listWidget->count() - 1);
    ChatMessage* message = (ChatMessage*)ui->listWidget->itemWidget(item);
    message->SendTextSuccess(); // 更新消息状态
    ui->sendmsg->setEnabled(true);      // 启用发送按钮
}
// 摄像头图像捕获
void Widget::CameraImageCapture(QVideoFrame frame)
{
    //qDebug()<<"CameraImageCapture";
    if(frame.isMapped() && frame.isValid())
    {
        QImage videoimg = QImage(frame.bits(),frame.width(),frame.height(),QVideoFrame::imageFormatFromPixelFormat(frame.pixelFormat()));
        QTransform matrix;
        matrix.rotate(180.0);   // 旋转 180 度
        QImage img = videoimg.transformed(matrix,Qt::FastTransformation).scaled(ui->mainshow_label->size());
        //当会议不止一人时 发送视频帧给其他人
        if(_partner.size() > 1)
            emit push_img(img);

        // 如果是主 IP，更新主显示
        if(QHostAddress(_tcpsocket->getLocalIPv4()).toString() == _mainip)
            ui->mainshow_label->setPixmap(QPixmap::fromImage(img).scaled(ui->mainshow_label->size()));

        // 更新伙伴图片
        Partner* p = _partner[_tcpsocket->getLocalIPv4()];
        if(p)
            p->setPic(img);
    }
    //释放视频帧
    frame.unmap();  // 释放帧
}

// 打开/关闭摄像头
void Widget::on_openVideo_clicked()
{
    qDebug()<<"on_openVideo_clicked";
    QString text = ui->openVideo->text();
    if(_camera->status() == QCamera::ActiveStatus)//开启中
    {
        _camera->stop();
        WRITER_LOG("关闭摄像头");
        ui->logout->setText("关闭摄像头");
        ui->openVideo->setText(OPENCAMERA);
        _imgThread->quit();
        _imgThread->wait();
        //关闭摄像头后需要提醒其他人关闭摄像头
        emit push_text(CLOSE_CAMERA);       // 发送关闭摄像头信号
        closeCamera(_tcpsocket->getLocalIPv4());    // 关闭自己的摄像头
    }
    else
    {
        // 打开摄像头
        _camera->start();
        if(_camera->error() == QCamera::NoError)
        {
            _imgThread->start();
            WRITER_LOG("开启摄像头");
            ui->logout->setText("开启摄像头");
        }
        ui->openVideo->setText(CLOSECAMERA);
    }
}

// 摄像头错误处理
void Widget::CameraError(QCamera::Error)
{
    QMessageBox::warning(this,"摄像头设备出错",_camera->errorString());
}

// 音频错误处理
void Widget::AudioError(QString errStr)
{
    QMessageBox::warning(this,"音频设备出错",errStr);
}

// 语音提示
void Widget::Spark(QString ip)
{
    ui->logout->setText(QString("%1 正在说话...").arg(ip));
}


// 切换主显示
void Widget::SwitchMainShow(uint32_t ip)
{
    //qDebug()<<"SwitchMainShow";
    QString str_ip = QHostAddress(ip).toString();
    if(_mainip != str_ip)
    {
        _mainip = str_ip;//切换主ip在CameraImageCapture函数里会自动更换视频帧的播放区域
        ui->mainshow_label->setPixmap(QPixmap::fromImage(QImage(":/1.jpg")).scaled(ui->mainshow_label->size()));
        ui->groupBox_6->setTitle(str_ip);
    }
}

// 处理消息
void Widget::dealMessage(QString text, QString ip, QString time, ChatMessage::User_Type type)
{
    ChatMessage* message = new ChatMessage(ui->listWidget);
    qDebug()<<"ui->listWidget width : "<<ui->listWidget->width();
    QListWidgetItem* item = new QListWidgetItem();
    ui->listWidget->addItem(item);
    message->setFixedWidth(ui->listWidget->width());//设置固定宽度 高度不能固定高度需要根据内容调整
    QSize size = message->fontRect(text,ip);
    qDebug()<<"size : "<<size;

    item->setSizeHint(size);//QListWidgetItem本身没有固定大小 设置大小可以符合预期
    message->SetText(text,time,ip,type);
    ui->listWidget->setItemWidget(item,message);
}

// 处理时间
void Widget::dealTime(QString curtimeStr)
{
    _isSendTime = false;
    if(ui->listWidget->count() > 0)
    {
        //判断是否过了一分钟
        QListWidgetItem* lastitem = ui->listWidget->item(ui->listWidget->count() - 1);//取出当前最后一个对象
        ChatMessage* messageTime = (ChatMessage*)ui->listWidget->itemWidget(lastitem);
        int lasttime = messageTime->time().toInt();
        int curtime = curtimeStr.toInt();
        _isSendTime = (curtime - lasttime) > 60;
    }
    else
    {
        //第一条消息需要直接发送时间
        _isSendTime = true;
    }

    if(_isSendTime)
    {
        QListWidgetItem* item = new QListWidgetItem();
        ChatMessage* messagetime = new ChatMessage(ui->listWidget);
        ui->listWidget->addItem(item);
        QSize size = QSize(ui->listWidget->width(),40);
        messagetime->setFixedSize(size);
        item->setSizeHint(size);
        messagetime->SetText(curtimeStr,curtimeStr);
        ui->listWidget->setItemWidget(item,messagetime);
    }
}

// 清除伙伴
void Widget::clearPartner()
{
    ui->mainshow_label->setPixmap(QPixmap());
    if(_partner.isEmpty())
        return;

    QMap<uint32_t,Partner*>::Iterator it = _partner.begin();
    while(it != _partner.end())
    {
        uint32_t ip = it.key();
        it++;
        Partner* p = _partner.take(ip);//移除这个值并且返回一个value
        ui->verticalLayout_5->removeWidget(p);
        delete p;
        p = nullptr;
    }

    ui->openAudio->setText(OPENAUDIO);
    ui->openAudio->setEnabled(false);
    ui->openVideo->setText(OPENCAMERA);
    ui->openVideo->setEnabled(false);
    _audioInput->stopCollect();
    _audioOutput->stopPlay();

    //关闭图片传输线程
    if(_imgThread->isRunning())
    {
        _imgThread->quit();
        _imgThread->wait();
        qDebug()<<"_imgThread线程退出";
    }
}

// 关闭摄像头
void Widget::closeCamera(uint32_t ip)
{
    if(!_partner.contains(ip))
    {
        qDebug()<<"ip不存在容器中";
        return;
    }
    Partner* p = _partner[ip];
    p->setPic(QImage(":/1.jpg"));

    if(QHostAddress(ip).toString() == _mainip)
        ui->mainshow_label->setPixmap(QPixmap::fromImage(QImage(":/1.jpg")).scaled(ui->mainshow_label->size()));
}

// 添加伙伴
Partner *Widget::addPartner(uint32_t ip)
{
    if(_partner.contains(ip))
    {
        qDebug()<<"该ip已经添加过";
        return nullptr;
    }
    Partner* partner = new Partner(ui->scrollAreaWidgetContents,ip);
    qDebug()<<"ui->scrollAreaWidgetContents width : "<<ui->scrollAreaWidgetContents->width();
    if(partner == nullptr)
    {
        qDebug()<<"partner new failed";
        return nullptr;
    }
    _partner.insert(ip,partner);
    ui->verticalLayout_5->addWidget(partner,1);
    connect(partner,SIGNAL(switchMain(uint32_t)),this,SLOT(SwitchMainShow(uint32_t)));
    if(_partner.size() > 1)//会议中人数大于1人
    {
        //启动音频和调节音频功能
        ui->openAudio->setEnabled(true);
        ui->horizontalSlider->setEnabled(true);
        _audioOutput->startPlay();
        connect(this,SIGNAL(volumChange(int)),_audioInput,SLOT(setVolum(int)));
        connect(this,SIGNAL(volumChange(int)),_audioOutput,SLOT(setVolum(int)));
    }
    return partner;
}

// 移除伙伴
void Widget::removePartner(uint32_t ip)
{
    if(!_partner.contains(ip))
    {
        qDebug()<<"该ip不存在";
        return;
    }

    Partner* partner = _partner[ip];
    disconnect(partner,SIGNAL(switchMain(uint32_t)));
    ui->verticalLayout_5->removeWidget(partner);
    _partner.remove(ip);
    delete partner;
    partner = nullptr;

    //只有一人
    if(_partner.size() <= 1)
    {
        //关闭音频功能
        ui->openAudio->setEnabled(false);
        ui->horizontalSlider->setEnabled(false);
        ui->openAudio->setText(OPENAUDIO);
        _audioOutput->stopPlay();
        _audioInput->stopCollect();
        disconnect(_audioInput,SLOT(setVolum(int)));
        disconnect(_audioInput,SLOT(setVolum(int)));
    }
}

// 获取公网 IP
void Widget::getPublicIP()
{
    QNetworkAccessManager *manager = new QNetworkAccessManager(this);

    // 使用 ifconfig.me 获取公网 IP
    QNetworkReply *reply = manager->get(QNetworkRequest(QUrl("http://ifconfig.me/ip")));

    // 连接信号槽，处理响应
    connect(reply, &QNetworkReply::finished, [=]() {
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray data = reply->readAll();
            QString publicIp = QString(data).trimmed(); // 去除换行符和空格
            qDebug() << "Public IP:" << publicIp;
        } else {
            qDebug() << "Error:" << reply->errorString();
        }
        reply->deleteLater();
        manager->deleteLater();
    });
}

// 音量滑动条值变化
void Widget::on_horizontalSlider_valueChanged(int value)
{
    emit volumChange(value);
    qDebug()<<"value : "<<value;
}
// 打开/关闭音频
void Widget::on_openAudio_clicked()
{
    qDebug()<<"on_openAudio_clicked";
    if(ui->openAudio->text() == OPENAUDIO)
    {
        // 开始录音
        emit startAudio();
        ui->openAudio->setText(CLOSEAUDIO);
    }
    else
    {
        // 停止录音
        emit stopAudio();
        ui->openAudio->setText(OPENAUDIO);
    }
}

