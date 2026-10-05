#include "mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QWidget(parent)
{
    // 创建摄像头采集线程与人脸检测线程
    pCaptureVideo = new capture_video(this);
    pFaceLookup = new face_lookup(this);

    // 把采集线程指针交给检测线程，使其能从中取出图像
    pFaceLookup->pCaptureVideo = pCaptureVideo;
    // 连接信号槽：检测线程发出图像时刷新画面；发出刷新人脸框信号时更新标记
    QObject::connect(pFaceLookup, SIGNAL(sendImage(cv::Mat*)), this, SLOT(showImg(cv::Mat*)));
    QObject::connect(pFaceLookup, SIGNAL(sendFlushFace()), this, SLOT(flushFace()));
    // 启动两个工作线程（各自 run() 开始独立循环）
    pCaptureVideo->start();
    pFaceLookup->start();

    // 创建用于显示图像的标签，并设置左对齐、顶对齐
    pLabelImg = new QLabel;
    pLabelImg->setAlignment(Qt::AlignTop|Qt::AlignLeft);
    // 使用网格布局把图像标签放入主窗口
    QGridLayout *pLayout = new QGridLayout(this);
    pLayout->addWidget(pLabelImg);

    // 预创建 FACE_MAX_NUM 个人脸框标记标签，父控件为图像标签，
    // 这样人脸框坐标相对于图像标签定位；默认隐藏，检测到人脸时才显示
    for(int i=0;i<FACE_MAX_NUM;i++)
    {
        pFaceMarkLabel[i] = new QLabel(pLabelImg);
        pFaceMarkLabel[i]->setStyleSheet("border:1px solid yellow;"); // 黄色边框作为人脸框
        pFaceMarkLabel[i]->hide();
    }
    lastFaceNum = 0; // 初始无已显示的人脸框
}

MainWindow::~MainWindow()
{

}

// 槽函数：收到检测线程发来的图像后，转换为 QImage 并显示到标签上
void MainWindow::showImg(cv::Mat *img)
{
    // 用图像数据构造 QImage（OpenCV 默认 BGR，Qt 需要 RGB，故 rgbSwapped 交换通道）
    QImage tmpimg = QImage(img->data,
                        img->cols,
                        img->rows,
                        QImage::Format_RGB888).rgbSwapped();
    // 通知采集线程：当前帧已使用完毕，可释放环形缓冲中的这块缓存
    pCaptureVideo->freeImgMat(pCaptureVideo);
    // 将 QImage 转成 QPixmap 并设置到图像标签上完成刷新
    pLabelImg->setPixmap(QPixmap::fromImage(tmpimg));
}

// 槽函数：收到刷新人脸框信号后，依据检测结果更新黄色标记框的位置与可见性
void MainWindow::flushFace()
{
    int i=0;
    // 获取本帧检测到的人脸数量，最多不超过界面能容纳的上限
    int num = pFaceLookup->headers.size();
    if(num > FACE_MAX_NUM)
    {
        num = FACE_MAX_NUM;
    }

    // 对每一个人脸，设置对应标记框的几何位置（x,y,width,height）并显示
    for(;i<num;i++)
    {
        pFaceMarkLabel[i]->setGeometry(
                    pFaceLookup->headers[i].x,
                    pFaceLookup->headers[i].y,
                    pFaceLookup->headers[i].width,
                    pFaceLookup->headers[i].height);
        pFaceMarkLabel[i]->show();
    }

    // 人脸检测数据已拷贝完成，释放检测线程的保护锁（由 face_lookup 在发送前加锁）
    pthread_mutex_unlock(&pFaceLookup->mutex);

    // 隐藏本帧不再需要的人脸框（数量比上一帧少的那些）
    for(;i<lastFaceNum;i++)
    {
        pFaceMarkLabel[i]->hide();
    }
    lastFaceNum = num; // 更新已显示人脸框数量
}
