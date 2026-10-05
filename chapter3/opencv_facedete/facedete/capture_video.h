#ifndef CAPTURE_VIDEO_H
#define CAPTURE_VIDEO_H

#include <QThread>
#include "opencv2/opencv.hpp"

// 采集图像的目标分辨率
#define IMG_SIZE_WIDTH      640
#define IMG_SIZE_HEIGHT     480
// 环形缓冲区的帧缓存个数（生产者-消费者模型，避免频繁分配内存）
#define IMG_MAT_CACHE_NUM   4

// 摄像头采集线程：作为生产者，不断把摄像头帧写入环形缓冲
class capture_video : public QThread
{
    Q_OBJECT
public:
    explicit capture_video(QObject *parent = 0);
    void run(void);                          // 线程入口：打开摄像头并循环采集
    cv::Mat *getFreeMat(void);               // 生产者取一块空闲缓存用于写入
    static cv::Mat *getImgMat(capture_video *pthis); // 消费者取一块已写入的帧
    static void freeImgMat(capture_video *pthis);    // 消费者用完帧后释放该缓存

private:
    pthread_mutex_t mutex;                   // 保护环形缓冲并发访问的互斥锁
    cv::Mat imgMat[IMG_MAT_CACHE_NUM];       // 帧缓存数组（环形缓冲）
    int imgIn;   // 生产者写入指针：指向下一个可写入位置（已写入最新帧下标）
    int imgOut;  // 消费者释放指针：指向下一个可释放位置
    int imgUse;  // 消费者取出指针：当前已取出待处理的帧下标
};

#endif // CAPTURE_VIDEO_H
