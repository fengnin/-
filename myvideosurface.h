//这是一个自定义视频表面类的C++头文件，继承自Qt的QAbstractVideoSurface，用于处理视频帧的显示或录制


#ifndef MYVIDEOSURFACE_H
#define MYVIDEOSURFACE_H

#include <QAbstractVideoSurface>            // 包含Qt视频表面基类

class MyVideoSurface : public QAbstractVideoSurface         // MyVideoSurface类继承自QAbstractVideoSurface，用于自定义视频帧处理
{
    Q_OBJECT            // Qt元对象系统宏，启用信号槽机制等Qt特性
public:
    explicit MyVideoSurface(QObject *parent = nullptr);         // 构造函数，explicit防止隐式转换，parent参数用于对象树管理
    //返回支持像素格式  在当与QCamera关联后调用start时自动调用
    QList<QVideoFrame::PixelFormat> supportedPixelFormats(QAbstractVideoBuffer::HandleType type = QAbstractVideoBuffer::NoHandle) const override;
    //检测视频流格式是否正确
    bool isFormatSupported(const QVideoSurfaceFormat &format) const override;
    //开始录制视频帧 后会自动调用present
    bool start(const QVideoSurfaceFormat &format) override;
    //发送视频帧
    bool present(const QVideoFrame &frame) override;
signals:
    void frameAvailable(QVideoFrame);       //自定义信号，当有新的视频帧可用时发射   frame: 可用的视频帧
};

#endif // MYVIDEOSURFACE_H
