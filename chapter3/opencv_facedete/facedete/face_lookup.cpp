#include "face_lookup.h"

face_lookup::face_lookup(QObject *parent) :
    QThread(parent)
{
    // 初始化保护检测结果的互斥锁
    pthread_mutex_init(&mutex, NULL);
}

void face_lookup::run()
{
    cv::Mat *pImg;

    // 加载 Haar 级联分类器模型；失败则打印提示（线程仍会运行，但检测无效）
    if(init() < 0)
    {
        qDebug("file load fail");
    }
    // 检测主循环
    while(1)
    {
        //获取图片：自旋等待直到取到一帧（缓冲为空时 getImgMat 返回 NULL）
        while(1)
        {
            pImg = capture_video::getImgMat(pCaptureVideo);
            if(pImg)
            {
                break;
            }
        }
        //图像灰度化：Haar 检测通常在灰度图上进行，提升速度并满足算法要求
        cv::cvtColor(*pImg, imgGrays, cv::COLOR_BGR2GRAY);
        // 发出图像信号，通知主窗口显示当前帧
        sendImage(pImg);
        // 分类器加载失败(模型文件缺失)时跳过检测，避免对空分类器调用 detectMultiScale 崩溃
        if (face_cascade.empty())
        {
            headers.clear();
            pthread_mutex_lock(&mutex);
            sendFlushFace();
            continue;
        }

        // 为提升嵌入式 ARM 平台的检测速度，先按比例缩小灰度图做检测，
        // 再把人脸框坐标放大回原图尺寸（显示/画框仍用原图坐标）
        const double detectScale = 0.5;      // 检测用 1/2 分辨率，耗时约降为 1/4
        cv::Mat graySmall;
        cv::resize(imgGrays, graySmall, cv::Size(), detectScale, detectScale, cv::INTER_LINEAR);
        //查找人脸：在缩小图上做多尺度检测；minSize 设为缩小图上约 20px，跳过小目标以提速
        std::vector<cv::Rect> rects;
        face_cascade.detectMultiScale(graySmall, rects, 1.1, 3, 0, cv::Size(20, 20));
        // 将缩小图检测到的人脸框坐标放大回原图尺寸，写入 headers 供界面绘制
        headers.clear();
        for (size_t i = 0; i < rects.size(); i++)
        {
            headers.push_back(cv::Rect((int)(rects[i].x / detectScale),
                                       (int)(rects[i].y / detectScale),
                                       (int)(rects[i].width / detectScale),
                                       (int)(rects[i].height / detectScale)));
        }

        // 加锁保护 headers，随后发出刷新信号（主窗口在 flushFace 中读取后解锁）
        pthread_mutex_lock(&mutex);
        //通知显示人脸框图
        sendFlushFace();
    }
}

// 初始化：加载 Haar 级联人脸分类器 XML 模型文件
// 依次尝试多个候选路径：运行目录相对路径、板上部署的绝对路径、当前目录
int face_lookup::init()
{
    const char* cascade_paths[] = {
        "haar_train/haarcascade_frontalface_alt.xml",
        "/home/root/haar_train/haarcascade_frontalface_alt.xml",
        "haarcascade_frontalface_alt.xml"
    };
    for (int i = 0; i < 3; i++)
    {
        if (face_cascade.load(cascade_paths[i]))
        {
            qDebug("cascade loaded: %s", cascade_paths[i]);
            return 0; // 加载成功
        }
    }
    return -1; // 全部加载失败
}


