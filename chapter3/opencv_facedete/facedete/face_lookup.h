#ifndef FACE_LOOKUP_H
#define FACE_LOOKUP_H

#include <QThread>
#include <pthread.h>
#include "opencv2/opencv.hpp"
#include "opencv2/face.hpp"     // 包含人脸/目标检测相关头文件
#include "capture_video.h"

// 人脸检测线程：作为消费者，从采集线程取帧并进行 Haar 级联人脸检测
class face_lookup : public QThread
{
    Q_OBJECT
public:
    explicit face_lookup(QObject *parent = 0);
    void run(void);              // 线程入口：取帧→灰度化→检测→发信号
    void releaseResult(void);   // （预留）释放检测结果的接口，当前未使用
    std::vector<cv::Rect> headers; // 存放本帧检测到的人脸矩形框列表
    cv::Mat imgGrays;            // 灰度化后的图像（检测在此图上进行）
    int init(void);              // 加载 Haar 级联分类器模型文件
    capture_video *pCaptureVideo; // 指向采集线程，用于取出图像帧
    pthread_mutex_t mutex;        // 保护 headers 等结果的互斥锁

private:
    cv::CascadeClassifier face_cascade; // OpenCV Haar 级联人脸分类器

signals:
    void sendImage(cv::Mat *pImg); // 发出已取到的图像，通知界面刷新画面
    void sendFlushFace(void);      // 发出人脸框数据就绪信号，通知界面刷新标记

public slots:
};

#endif // FACE_LOOKUP_H
