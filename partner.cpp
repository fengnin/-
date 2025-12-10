#include "partner.h"
#include <QHostAddress>
Partner::Partner(QWidget *parent, uint32_t ip)
    :QLabel(parent)
    ,_ip(ip)
{
    this->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Minimum);//垂直方向最小不能小于最小长度
    //qDebug()<<""
    //_width = ((QWidget*)this->parent())->size().width();
    _width = ((QWidget *)this->parent())->size().width();
    // qDebug()<<"_width : "<<_width;
    // qDebug()<<"this->parent())->size() : "<<((QWidget *)this->parent())->size();
    this->setPixmap(QPixmap::fromImage(QImage(":/1.jpg").scaled(_width - 10,_width - 10)));
    // QImage img(":/1.jpg");
    // qDebug()<< "img w : "<<img.width()<<"img h : "<<img.height();
    //this->setPixmap(QPixmap::fromImage(QImage(":/1.jpg").scaled(_width, _width)));
    //绘制边框
    this->setFrameShape(QFrame::Box);
    this->setStyleSheet("border: 1px solid blue;");

    //设置悬浮ip提示 鼠标指向图片位置时
    this->setToolTip(QHostAddress(ip).toString());
}

void Partner::setPic(QImage img)
{
    // qDebug()<<"setPic";
    // qDebug()<<"setPic : width : "<<_width;
    this->setPixmap(QPixmap::fromImage(img).scaled(_width - 10, _width - 10));
}

void Partner::mousePressEvent(QMouseEvent *e)
{

    emit switchMain(_ip);
}

