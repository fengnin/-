//这是一个自定义的 Partner 类头文件，继承自 QLabel，用于表示视频会议中的一个参与者（伙伴）

#ifndef PARTNER_H
#define PARTNER_H

#include <QLabel>

class Partner : public QLabel
    // Partner 类：表示视频会议中的一个参与者
{
    Q_OBJECT        // 启用 Qt 的信号槽机制
public:
    explicit Partner(QWidget *parent = nullptr,uint32_t ip = 0);    //parent: 父控件指针  ip: 参与者的 IP 地址（唯一标识）
    void setPic(QImage img);//设置图片和视频帧 用于开启摄像头和关闭摄像头
protected:
    void mousePressEvent(QMouseEvent* e) override;          // 重写鼠标点击事件   e: 鼠标事件对象
private:
    uint32_t _ip;   //参与者的 IP 地址（唯一标识）
    int _width;//根据父控件设置图片宽度
signals:
    void switchMain(uint32_t);      //信号：请求切换主画面
        // 当用户点击该参与者时，发射此信号，通知切换主显示
};

#endif // PARTNER_H
