#include "screen.h"
#include <QScreen>
#include <QGuiApplication>
int Screen::Height = -1;     // 存储屏幕高度
int Screen::Width = -1;     // 存储屏幕宽度
Screen::Screen() {  // 构造函数（当前为空实现）

}

// 初始化屏幕尺寸信息
void Screen::init()
{
    // 获取主屏幕对象（QGuiApplication::primaryScreen()返回当前应用的默认屏幕）
    QScreen* sc = QGuiApplication::primaryScreen();

    // 通过屏幕的几何信息获取高度和宽度
    // geometry()返回屏幕在虚拟桌面中的矩形区域（包含可用区域）
    Height = sc->geometry().height();   // 屏幕高度（像素）
    Width = sc->geometry().width();     // 屏幕宽度（像素）
}

