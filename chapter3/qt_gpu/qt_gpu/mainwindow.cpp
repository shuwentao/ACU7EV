#include <QGridLayout>
#include <QGuiApplication>
#include <QLabel>
#include <QScreen>

#include "mainwindow.h"

// 摄像头采集/显示的图像分辨率
#define IMG_UVC_WIDTH   1920
#define IMG_UVC_HEIGHT  1080

MainWindow::MainWindow(QWidget *parent)
    : QWidget(parent)
{
    // 使用网格布局，OpenGL 视频部件占据整个窗口
    QGridLayout *layout = new QGridLayout(this);

    // 创建 OpenGL 显示部件，并告诉它视频帧的尺寸（决定纹理大小）
    pOpenglYuv = new uOpenglYuv;
    pOpenglYuv->width = IMG_UVC_WIDTH;
    pOpenglYuv->height = IMG_UVC_HEIGHT;

    // 创建摄像头采集线程，并把它取到的帧接到本窗口的刷新槽上。
    // 使用 Qt5 引入的函数指针形式连接，编译期即可检查信号/槽签名
    cImageCatch *pImageCatch = new cImageCatch;
    connect(pImageCatch, &cImageCatch::sendImg, this, &MainWindow::flushimg);
    layout->addWidget(pOpenglYuv);

    // QDesktopWidget 自 Qt 5.10 起废弃、Qt 6 中已移除，改用 QScreen 获取屏幕尺寸
    QScreen *screen = QGuiApplication::primaryScreen();
    QSize screenSize = screen ? screen->availableGeometry().size()
                              : QSize(IMG_UVC_WIDTH, IMG_UVC_HEIGHT);
    resize(screenSize);

    // 启动采集线程（run() 里完成 V4L2 初始化并开始取流）
    pImageCatch->start();

    // 叠加在视频上的半透明信息面板（背景为半透明绿色 + 白色边框）
    QLabel *labWindows = new QLabel(this);
    labWindows->setGeometry(20, 20, 340, 480);
    labWindows->setStyleSheet("border:2px solid white;background:rgba(0,255,0,128);");
    labWindows->show();

    // 面板顶部的白色标题栏
    QLabel *labTitle = new QLabel(this);
    labTitle->setText("上海芯驿电子");
    labTitle->setGeometry(20, 20, 340, 40);
    labTitle->setStyleSheet("font-size:32px;background:white;");
    labTitle->show();
}

MainWindow::~MainWindow()
{

}

/*
 * 槽函数：收到采集线程送来的新一帧
 * 只保存数据指针并请求重绘，真正的纹理上传在 uOpenglYuv::paintGL() 中完成，
 * 避免在界面线程里做数据拷贝。
 */
void MainWindow::flushimg(unsigned char *img)
{
    pOpenglYuv->img_yuv = img;
    pOpenglYuv->update(); // 触发重绘（异步，会在下一次 paintGL 生效）
}
