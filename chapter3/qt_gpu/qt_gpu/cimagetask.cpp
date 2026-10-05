#include "cimagetask.h"

#include <cerrno>
#include <cstring>
#include <unistd.h>

/*
 * 构造函数：设置采集参数默认值
 * runing - 线程运行标志
 * imgset - 图像参数集索引（预留，用于切换分辨率/格式等参数组）
 */
cImageCatch::cImageCatch(QObject *parent) :
    QThread(parent)
{
    runing = true;
    imgset = 0;

    // 默认采集 1080p
    width = 1920;
    height = 1080;
}

/*
 * 采集线程主函数：使用 V4L2 的 MMAP 方式从摄像头取流
 *
 * 流程：打开设备 -> 查询能力/枚举格式 -> 设置分辨率与像素格式(YUYV) -> 设置帧率
 *       -> 申请并 mmap 缓冲 -> STREAMON -> 循环 DQBUF(取帧)/QBUF(还帧)
 *
 * 最外层的 while(1) 用于容错：摄像头被拔出时 DQBUF 会返回 ENODEV，
 * 此时释放资源并回到最外层重新走一遍初始化，实现热插拔自动恢复。
 */
void cImageCatch::run()
{
    int ret;
    int videoFd = -1;
    struct v4l2_capability caps;
    struct v4l2_format fmt;
    struct v4l2_requestbuffers rqbufs;
    struct v4l2_fmtdesc fmtdesc;

    while(1)
    {
        // 摄像头可能尚未就绪，打开失败则每隔 1 秒重试
        while(1)
        {
            videoFd = open("/dev/video0", O_RDWR);
            if(videoFd >= 0)
            {
                break;
            }
            sleep(1);
        }

        // 查询设备能力（确认它是支持视频采集的 V4L2 设备）
        memset(&caps, 0, sizeof(caps));
        ret = ioctl(videoFd, VIDIOC_QUERYCAP, &caps);
        if(ret < 0)
        {
            return;
        }

        // 枚举设备支持的所有像素格式，直到 ioctl 返回 -1 表示枚举结束
        fmtdesc.index = 0;
        fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        while(ioctl(videoFd, VIDIOC_ENUM_FMT, &fmtdesc) != -1)
        {
            fmtdesc.index++;
        }

        // 先取当前格式：在已有格式的基础上修改再 S_FMT，驱动更容易接受
        memset(&fmt, 0, sizeof(fmt));
        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        ret = ioctl(videoFd, VIDIOC_G_FMT, &fmt);
        if(ret < 0)
        {
            printf("video2 VIDIOC_G_FMT fail %d\n", ret);
        }

        // 设置为 YUYV 4:2:2：每像素 2 字节，故一行字节数 bytesperline = width * 2
        fmt.fmt.pix.width = width;
        fmt.fmt.pix.height = height;
        fmt.fmt.pix.bytesperline = 2 * fmt.fmt.pix.width;
        fmt.fmt.pix.colorspace = V4L2_COLORSPACE_SRGB;
        fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
        ioctl(videoFd, VIDIOC_S_FMT, &fmt);

        // 再读回一次，打印驱动最终真正采用的分辨率（可能被裁剪成它支持的值）
        memset(&fmt, 0, sizeof(fmt));
        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        ret = ioctl(videoFd, VIDIOC_G_FMT, &fmt);
        if(ret < 0)
        {
            printf("video2 VIDIOC_G_FMT fail %d\n", ret);
        }
        else
        {
            printf("video2 VIDIOC_G_FMT %ux%u\n", fmt.fmt.pix.width, fmt.fmt.pix.height);
        }

        struct v4l2_streamparm parm;
        memset(&parm, 0, sizeof(parm));

        // 打印当前帧率（timeperframe 以分数形式表示：numerator/denominator 秒/帧）
        printf("==<>==reay get para==<>===\r\n");
        ioctl(videoFd,VIDIOC_G_PARM,&parm);
        printf("the denominator=%d numerator=%d\r\n", parm.parm.capture.timeperframe.denominator, parm.parm.capture.timeperframe.numerator);

        // 设置为 60fps：1/60 秒每帧
        parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        parm.parm.capture.capturemode = V4L2_MODE_HIGHQUALITY;
        parm.parm.capture.capability = V4L2_CAP_TIMEPERFRAME;
        parm.parm.capture.timeperframe.denominator = 60 ;//时间间隔分母
        parm.parm.capture.timeperframe.numerator = 1;//分子
        if(-1 == ioctl(videoFd,VIDIOC_S_PARM,&parm))
        {
            printf("VIDIOC_S_PARM fail\r\n");
            fflush(stdout);
        }

        ioctl(videoFd,VIDIOC_G_PARM,&parm);
        printf("the fps set=%d %d\r\n", parm.parm.capture.timeperframe.denominator, parm.parm.capture.timeperframe.numerator);

        // 申请 CAP_BUF_NUM 个内核缓冲，采用 MMAP 方式（用户态直接访问，无需拷贝）
        memset(&rqbufs, 0, sizeof rqbufs);
        rqbufs.count = CAP_BUF_NUM;
        rqbufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        rqbufs.memory = V4L2_MEMORY_MMAP;
        ret = ioctl(videoFd, VIDIOC_REQBUFS, &rqbufs);
        if(ret < 0)
        {
            printf("video2 VIDIOC_REQBUFS fail %d\n", ret);
        }

        struct v4l2_buffer buffer;
        unsigned char *pImagAry[CAP_BUF_NUM]; // mmap 到用户空间的缓冲地址表
        enum v4l2_buf_type type;

        // 查询每个缓冲的信息并映射到用户空间，随后全部入队交给驱动填充
        for(int i=0;i<CAP_BUF_NUM;i++)
        {
            memset(&buffer, 0, sizeof(buffer));
            buffer.type  = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buffer.memory = V4L2_MEMORY_MMAP;
            buffer.index = i;
            if (-1 == ioctl (videoFd, VIDIOC_QUERYBUF, &buffer))
            {
                printf("VIDIOC_QUERYBUF fail\n");
                return;
            }
            pImagAry[i] = (unsigned char *)mmap(NULL,buffer.length, PROT_READ|PROT_WRITE, MAP_SHARED, videoFd, buffer.m.offset);
            if(pImagAry[i] == MAP_FAILED)
            {
                printf("mmap fail\n");
            }
            ioctl(videoFd, VIDIOC_QBUF, &buffer);
        }

        // 启动视频流
        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        ret = ioctl(videoFd, VIDIOC_STREAMON, &type);

        memset(&buffer, 0, sizeof(buffer));
        buffer.type  = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buffer.memory = V4L2_MEMORY_MMAP;

        // 取帧循环：DQBUF 阻塞等到一帧就绪，送给界面渲染后再 QBUF 归还给驱动
        while(1)
        {
            ret = ioctl(videoFd, VIDIOC_DQBUF, &buffer);
            if(ret == -1)
            {
                // ENODEV 表示设备已断开（摄像头被拔出），释放资源后回到最外层重开
                if(errno == ENODEV)
                {
                    close(videoFd);
                    for(int i=0;i<CAP_BUF_NUM;i++)
                    {
                        munmap(pImagAry[i], buffer.length);
                    }
                    break;
                }
                qDebug("error VIDIOC_DQBUF %d", errno);
            }
            // 直接把 mmap 缓冲指针发信号出去（YUYV 数据），由 OpenGL 上传为纹理
            emit sendImg(pImagAry[buffer.index]);

            // 归还缓冲；不还回驱动很快就会因为没有空闲缓冲而停止输出
            ioctl(videoFd, VIDIOC_QBUF, &buffer);
        }
    }
}
