#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QtWidgets>
#include "face_lookup.h"

// 界面上最多可同时标记的人脸框数量（用 QLabel 控件叠加）
#define FACE_MAX_NUM    30

// 主窗口类：继承自 QWidget，作为整个程序的可视化界面载体
class MainWindow : public QWidget
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = 0);
    ~MainWindow();

private:
    face_lookup *pFaceLookup;          // 人脸检测线程对象（消费者）
    capture_video *pCaptureVideo;      // 摄像头采集线程对象（生产者）
    QLabel *pLabelImg;                 // 用于显示摄像头图像的标签控件
    QLabel *pFaceMarkLabel[FACE_MAX_NUM]; // 预创建的黄色人脸框标记控件数组
    int lastFaceNum;                   // 上一帧已显示的人脸框数量（用于隐藏多余框）

public slots:
    void showImg(cv::Mat *img);   // 收到图像信号后刷新画面
    void flushFace(void);         // 收到人脸框信号后刷新人脸标记框
};

#endif // MAINWINDOW_H
