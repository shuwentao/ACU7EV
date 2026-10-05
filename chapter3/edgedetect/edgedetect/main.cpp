#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <opencv2/core.hpp>       // OpenCV 核心数据结构(Mat)与基础运算
#include <opencv2/highgui.hpp>     // 图像显示、窗口
#include <opencv2/imgproc.hpp>     // 图像处理(滤波、边缘检测等)
#include <opencv2/videoio.hpp>     // VideoCapture 摄像头捕获(OpenCV4 中需显式包含)
using namespace cv;

// 显示窗口名称
#define CV_WIN_NAME         "ALINX_cvEdgeDete"
// 单路图像(摄像头采集及每种算法结果)的宽、高
#define IMG_SIZE_WIDTH      640
#define IMG_SIZE_HEIGHT     480

/*
 * 边缘检测任务参数结构体
 * 每个算法线程(Canny / Sobel / Laplacian)对应一份该结构体，用于向线程传递数据
 * 并借助两个互斥锁实现主线程与各算法线程之间的"生产-消费"同步
 */
typedef struct
{
    Mat *src;                 // 指向摄像头原始帧(主线程写入，算法线程读取)
    Mat *dst;                 // 指向拼接到一起的显示大图(算法线程写入)
    int posx;                 // 算法结果在大图中的水平起始坐标
    int posy;                 // 算法结果在大图中的垂直起始坐标
    pthread_mutex_t mutex_task;    // "任务"锁：控制算法线程何时开始处理一帧
    pthread_mutex_t mutex_result;  // "结果"锁：控制主线程何时取走算法结果
}EdgeParam_t;

/*
 * Canny 边缘检测线程
 * 同步逻辑：线程启动后立刻被 mutex_task 阻塞(初始为加锁状态)，
 * 主线程采集到一帧后会解锁 mutex_task，本线程随即开始处理，
 * 处理完毕后再解锁 mutex_result，通知主线程取走结果。
 */
void *task_canny(EdgeParam_t *ep)
{
    Mat gray, edge, rgb;

    while(1)
    {
        pthread_mutex_lock(&ep->mutex_task); // 等待主线程投放新的一帧
        //将原图像转换为灰度图像
        cvtColor(*ep->src, gray, COLOR_BGR2GRAY);
        //使用3×3内核降噪
        blur(gray, edge, Size(3, 3));
        //使用canny算子
        Canny(edge, edge, 3, 9, 3);
        //转成方便显示的rgb
        cvtColor(edge, rgb, COLOR_GRAY2RGB);
        //拷贝图像
        rgb.copyTo((*ep->dst)(Rect(ep->posx, ep->posy, IMG_SIZE_WIDTH, IMG_SIZE_HEIGHT)));
        pthread_mutex_unlock(&ep->mutex_result);
    }
    return NULL;
}

/*
 * Sobel 边缘检测线程
 * 计算 x、y 两个方向的梯度后按 0.5/0.5 加权融合，得到近似梯度幅值。
 */
void *task_sobel(EdgeParam_t *ep)
{
    Mat grad_x, grad_y, dst;

    while(1)
    {
        pthread_mutex_lock(&ep->mutex_task); // 等待主线程投放新的一帧
        //x方向梯度
        Sobel(*ep->src, grad_x, CV_16S, 1, 0, 3, 1, 1, BORDER_DEFAULT);
        convertScaleAbs(grad_x, grad_x);
        //y方向梯度
        Sobel(*ep->src, grad_y, CV_16S, 0, 1, 3, 1, 1, BORDER_DEFAULT);
        convertScaleAbs(grad_y, grad_y);
        //合并梯度
        addWeighted(grad_x, 0.5, grad_y, 0.5, 0, dst);
        //拷贝图像
        dst.copyTo((*ep->dst)(Rect(ep->posx, ep->posy, IMG_SIZE_WIDTH, IMG_SIZE_HEIGHT)));
        pthread_mutex_unlock(&ep->mutex_result);
    }
    return NULL;
}

/*
 * Laplacian 边缘检测线程
 * 先用高斯滤波去噪，再求二阶导数(Laplacian)并取绝对值，得到边缘。
 */
void *task_laplacian(EdgeParam_t *ep)
{
    Mat noise, grad, laplace, abs_dst, dst;

    while(1)
    {
        pthread_mutex_lock(&ep->mutex_task); // 等待主线程投放新的一帧
        //使用高斯滤波消除噪声
        GaussianBlur(*ep->src, noise, Size(3, 3), 0, 0, BORDER_DEFAULT);
        //转换为灰度图
        cvtColor(noise, grad, COLOR_RGB2GRAY);
        //使用Laplace函数
        Laplacian(grad, laplace, CV_16S, 3, 1, 0, BORDER_DEFAULT);
        //计算绝对值
        convertScaleAbs(laplace, abs_dst);
        //转成方便显示的rgb
        cvtColor(abs_dst, dst, COLOR_GRAY2RGB);
        //拷贝图像
        dst.copyTo((*ep->dst)(Rect(ep->posx, ep->posy, IMG_SIZE_WIDTH, IMG_SIZE_HEIGHT)));
        pthread_mutex_unlock(&ep->mutex_result);
    }
    return NULL;
}


int main(void)
{
    pthread_t pid;
    Mat imgUvc;   // 摄像头采集的原始帧
    Mat imgShow;  // 拼接后的大图(2x2 布局)，用于统一显示
    EdgeParam_t ep_canny;
    EdgeParam_t ep_sobel;
    EdgeParam_t ep_laplacian;

    // 启动 X 显示服务，供 OpenCV 图形界面使用
    setenv("DISPLAY", ":0.0", 0);
    // 先探测 X 是否已可达：xset q 返回 0 表示 X 已在运行，则不再启动，
    // 避免重复启动 X server 报 "Server is already active for display 0"
    if (system("xset q >/dev/null 2>&1") != 0)
    {
        system("/etc/init.d/xserver-nodm start");
        sleep(2);
    }

    //打开摄像头：板子上USB摄像头通常对应 /dev/video0 或 /dev/video1（节点号可能变化）
    //强制使用 V4L2 后端，避免 OpenCV 先试 GStreamer 后端报 uridecodebin 异常
    VideoCapture capture;
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
    if (!cam_ok)
    {
        printf("open video fail\r\n");
        exit(1);
    }

    //设置摄像头分辨率
    capture.set(CAP_PROP_FRAME_WIDTH, IMG_SIZE_WIDTH);
    capture.set(CAP_PROP_FRAME_HEIGHT, IMG_SIZE_HEIGHT);

    //取消显示输出电源管理与屏保
    system("xset s 0 0");
    system("xset dpms 0 0 0");

    //设置显示器分辨率
    system("xrandr --output DP-1 --mode 1920x1080");

    //等待设置完成
    sleep(3);

    //创建显示窗口
    namedWindow(CV_WIN_NAME, WINDOW_NORMAL);

    //设置显示图片大小：2x2 拼图，宽 1280 高 960，三通道彩色
    imgShow.create(IMG_SIZE_HEIGHT*2, IMG_SIZE_WIDTH*2, CV_8UC3);

    // 初始化 Canny 任务参数：结果贴到右上角(640, 0)
    ep_canny.src = &imgUvc;
    ep_canny.dst = &imgShow;
    ep_canny.posx = IMG_SIZE_WIDTH*1;
    ep_canny.posy = IMG_SIZE_HEIGHT*0;
    // 初始化互斥锁后立刻上锁：使算法线程创建后先阻塞在 mutex_task 上，
    // 等待主线程采集到帧后解锁才会开始处理(完成后再释放 mutex_result)
    pthread_mutex_init(&ep_canny.mutex_task, NULL);
    pthread_mutex_lock(&ep_canny.mutex_task);
    pthread_mutex_init(&ep_canny.mutex_result, NULL);
    pthread_mutex_lock(&ep_canny.mutex_result);
    pthread_create(&pid, NULL, (void*(*)(void*))task_canny, &ep_canny);

    // 初始化 Sobel 任务参数：结果贴到左下角(0, 480)
    ep_sobel.src = &imgUvc;
    ep_sobel.dst = &imgShow;
    ep_sobel.posx = IMG_SIZE_WIDTH*0;
    ep_sobel.posy = IMG_SIZE_HEIGHT*1;
    pthread_mutex_init(&ep_sobel.mutex_task, NULL);
    pthread_mutex_lock(&ep_sobel.mutex_task);
    pthread_mutex_init(&ep_sobel.mutex_result, NULL);
    pthread_mutex_lock(&ep_sobel.mutex_result);
    pthread_create(&pid, NULL, (void*(*)(void*))task_sobel, &ep_sobel);

    // 初始化 Laplacian 任务参数：结果贴到右下角(640, 480)
    ep_laplacian.src = &imgUvc;
    ep_laplacian.dst = &imgShow;
    ep_laplacian.posx = IMG_SIZE_WIDTH*1;
    ep_laplacian.posy = IMG_SIZE_HEIGHT*1;
    pthread_mutex_init(&ep_laplacian.mutex_task, NULL);
    pthread_mutex_lock(&ep_laplacian.mutex_task);
    pthread_mutex_init(&ep_laplacian.mutex_result, NULL);
    pthread_mutex_lock(&ep_laplacian.mutex_result);
    pthread_create(&pid, NULL, (void*(*)(void*))task_laplacian, &ep_laplacian);

    while (1)
    {
        //捕获图像
        capture >> imgUvc;

        //鼠标操作关闭显示窗口时，程序退出
        if (getWindowProperty(CV_WIN_NAME, WND_PROP_AUTOSIZE) == -1)
        {
            break;
        }

        //处理获取的图片
        if(!imgUvc.empty())
        {
            // 解锁三个"任务"锁，唤醒 Canny/Sobel/Laplacian 三个线程开始处理本帧
            pthread_mutex_unlock(&ep_canny.mutex_task);
            pthread_mutex_unlock(&ep_sobel.mutex_task);
            pthread_mutex_unlock(&ep_laplacian.mutex_task);

            waitKey(30); //延时30毫秒(同时处理窗口事件)
            // 将原始帧拷贝到大图左上角(0, 0)
            imgUvc.copyTo(imgShow(Rect(0, 0, IMG_SIZE_WIDTH, IMG_SIZE_HEIGHT)));

            // 阻塞等待三个线程处理完毕(各自释放"结果"锁)，保证拼图完整后再显示
            pthread_mutex_lock(&ep_canny.mutex_result);
            pthread_mutex_lock(&ep_sobel.mutex_result);
            pthread_mutex_lock(&ep_laplacian.mutex_result);

            imshow(CV_WIN_NAME, imgShow);
        }
    }

    return 0;
}
