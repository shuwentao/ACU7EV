#include "capture_video.h"

capture_video::capture_video(QObject *parent) :
    QThread(parent)
{
    // 初始化互斥锁，并把三个环形缓冲指针都归零（空缓冲状态）
    pthread_mutex_init(&mutex, NULL);
    imgIn = 0;
    imgOut = 0;
    imgUse = 0;
}

void capture_video::run()
{
    cv::Mat *pFrame;

    //打开摄像头：板子上USB摄像头通常对应 /dev/video0 或 /dev/video1（节点号可能变化）
    //强制使用 V4L2 后端，避免 OpenCV 先试 GStreamer 后端报 uridecodebin 异常
    cv::VideoCapture capture;
    const char* cam_devs[] = {"/dev/video0", "/dev/video1"};
    bool cam_ok = false;
    for (int i = 0; i < 2; i++)
    {
        if (capture.open(cam_devs[i], cv::CAP_V4L2) && capture.isOpened())
        {
            cam_ok = true;
            break;
        }
    }

    //未能打开摄像头，打印错误并退出程序
    if(!cam_ok)
    {
        printf("open video fail\r\n");
        exit(1);
    }

    //设置摄像头分辨率（OpenCV4 已移除 CV_CAP_PROP_*，改用 cv::CAP_PROP_*）
    capture.set(cv::CAP_PROP_FRAME_WIDTH, IMG_SIZE_WIDTH);
    capture.set(cv::CAP_PROP_FRAME_HEIGHT, IMG_SIZE_HEIGHT);

    // 先读取一帧用于初始化缓冲（预热）
    capture >> imgMat[0];
    // 采集主循环：不断写入新帧到空闲缓存
    while(1)
    {
        // 取一块空闲缓存（若缓冲满则覆盖最旧位置）
        pFrame = getFreeMat();
        // 从摄像头读取一帧到该缓存
        capture >> *pFrame;
        // 读取失败（空帧）则退出循环
        if(pFrame->empty())
        {
            break;
        }
    }
}

// 生产者：返回一块用于写入的缓存。采用环形缓冲策略，
// 当缓冲写满（pos 追平 imgOut）时直接覆盖最新位置，不阻塞。
cv::Mat *capture_video::getFreeMat(void)
{
    int pos;

    pthread_mutex_lock(&mutex);
    // 计算下一个写入位置
    pos = (imgIn+1)%IMG_MAT_CACHE_NUM;
    // 仅当该位置尚未被消费者释放（未与 imgOut 冲突）时才真正推进写入指针
    if(pos != imgOut)
    {
        imgIn = pos;
    }
    pthread_mutex_unlock(&mutex);
    // 返回当前写入指针所指的缓存
    return &imgMat[imgIn];
}

// 消费者（静态函数，需传入对象指针）：取下一块已写入、尚未处理的帧
cv::Mat *capture_video::getImgMat(capture_video *pthis)
{
    cv::Mat *pMat;

    pthread_mutex_lock(&pthis->mutex);

    // 若取出指针已追平写入指针，说明没有新帧可读，返回 NULL
    if(pthis->imgUse == pthis->imgIn)
    {
        pMat = NULL;
    }
    else
    {
        // 取出当前帧，并推进取出指针（环形）
        pMat = &pthis->imgMat[pthis->imgUse];
        pthis->imgUse = (pthis->imgUse+1)%IMG_MAT_CACHE_NUM;
    }
    pthread_mutex_unlock(&pthis->mutex);
    return pMat;
}

// 消费者（静态函数）：当前帧在界面处理完毕后，释放该缓存供生产者复用
void capture_video::freeImgMat(capture_video *pthis)
{
    pthread_mutex_lock(&pthis->mutex);
    // 仅当还有未释放的帧（imgOut 未追平 imgIn）时推进释放指针
    if(pthis->imgOut != pthis->imgIn)
    {
        pthis->imgOut = (pthis->imgOut+1)%IMG_MAT_CACHE_NUM;
    }
    pthread_mutex_unlock(&pthis->mutex);
}
