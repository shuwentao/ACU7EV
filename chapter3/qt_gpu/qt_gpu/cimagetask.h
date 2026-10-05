#ifndef CIMAGETASK_H
#define CIMAGETASK_H

#include <QThread>
#include <qimage.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include <sys/mman.h>
#include <sys/time.h>

// V4L2 申请的 MMAP 缓冲个数：太少容易丢帧，太多会增加延迟
#define CAP_BUF_NUM   6

/*
 * 摄像头采集线程：基于 V4L2 + MMAP 采集 YUYV 图像
 * 每取到一帧就通过 sendImg 信号把缓冲指针发给界面线程做 OpenGL 渲染（零拷贝）。
 */
class cImageCatch : public QThread
{
    Q_OBJECT

public:
    cImageCatch(QObject *parent=nullptr);
    void run(void) Q_DECL_OVERRIDE; // 线程入口：V4L2 采集主循环
    bool runing;   // 线程运行标志
    int imgset;    // 图像参数集索引（预留：用于切换分辨率/格式等参数组）
    int width;     // 采集图像宽度
    int height;    // 采集图像高度

Q_SIGNALS:
    // 送出一帧图像数据（指向 mmap 出来的内核缓冲，不做内存拷贝）
    void sendImg(unsigned char *pData);
};

#endif // CIMAGETASK_H
