#include "myvideosurface.h"
#include <QVideoSurfaceFormat>
#include <QDebug>
// 构造函数，初始化自定义视频表面
MyVideoSurface::MyVideoSurface(QObject *parent)
    : QAbstractVideoSurface{parent} // 调用基类构造函数
{}

// 返回支持的像素格式列表（根据句柄类型）
QList<QVideoFrame::PixelFormat> MyVideoSurface::supportedPixelFormats(QAbstractVideoBuffer::HandleType type) const
{
    // 如果视频缓冲区不使用特定句柄（如GPU纹理句柄等），返回支持的像素格式列表
    //qDebug()<<"supportedPixelFormats";
    if(type == QAbstractVideoBuffer::NoHandle)//QAbstractVideoBuffer::NoHandle视频缓冲区不再使用任何特定句柄 使用普通像素数据
    {
        return QList<QVideoFrame::PixelFormat>() << QVideoFrame::Format_RGB555  // 15位RGB格式
                                          << QVideoFrame::Format_RGB32          // 32位RGB格式（可能带未使用的Alpha通道）
                                          << QVideoFrame::Format_ARGB32         // 32位ARGB格式（带Alpha通道）
                                          << QVideoFrame::Format_ARGB32_Premultiplied   // 预乘Alpha的ARGB32格式
                                          << QVideoFrame::Format_RGB565;        // 16位RGB格式
    }
    else // 其他句柄类型（如GPU纹理）不支持
    {
        return QList<QVideoFrame::PixelFormat>();
    }
}
// 检查给定的视频格式是否被支持
bool MyVideoSurface::isFormatSupported(const QVideoSurfaceFormat &format) const
{
    // 将视频帧的像素格式转换为QImage格式，检查是否有效
    //qDebug()<<"isFormatSupported";
    return QVideoFrame::imageFormatFromPixelFormat(format.pixelFormat()) != QImage::Format_Invalid;
}
// 启动视频表面，设置视频格式
bool MyVideoSurface::start(const QVideoSurfaceFormat &format)
{
    //qDebug()<<"start";
    // 如果格式被支持或帧大小为空
    if(isFormatSupported(format) || format.frameSize().isEmpty())
    {
        QAbstractVideoSurface::start(format);   // 调用基类启动方法
        return true;    // 启动成功
    }
    return false;   // 格式不支持，启动失败
}
// 处理并呈现视频帧
bool MyVideoSurface::present(const QVideoFrame &frame)
{
    //qDebug()<<"present";
    // 检查帧是否有效
    if(!frame.isValid())
    {
        //qDebug()<<"frame.isValid()";
        stop();     // 停止视频表面
        return false;   // 呈现失败
    }
    // 如果帧已经映射到内存（可直接访问像素数据）
    if(frame.isMapped())//已经映射到内存
    {
        //qDebug()<<"frameAvailable frame";
        emit frameAvailable(frame);     // 发出信号，传递帧引用
    }
    else  // 帧未映射，需要手动映射
    {
        //qDebug()<<"frameAvailable f";
        QVideoFrame f(frame);       // 创建帧的副本（避免修改原始帧）
        // 以只读方式映射帧数据到内存
        f.map(QAbstractVideoBuffer::ReadOnly);
        emit frameAvailable(f); // 发出信号，传递映射后的帧
        // 注意：映射后的帧会在接收端处理完成后自动解映射，或由QVideoFrame析构时处理
    }
    return true;
}

