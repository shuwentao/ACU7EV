#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QtWidgets>
#include "opengl_yuv.h"
#include "cimagetask.h"

/*
 * 主窗口：承载 OpenGL 视频显示部件，并在其上叠加半透明的信息面板与标题。
 *
 * 数据流向：摄像头采集线程 cImageCatch 每取到一帧就发出 sendImg 信号，
 *          主窗口的 flushimg 槽收到后把数据交给 OpenGL 部件渲染。
 */
class MainWindow : public QWidget
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

public Q_SLOTS:
    // 收到采集线程送来的新一帧（YUYV 数据指针）后刷新画面
    void flushimg(unsigned char *);

private:
    uOpenglYuv *pOpenglYuv; // OpenGL 视频显示部件
};

#endif // MAINWINDOW_H
