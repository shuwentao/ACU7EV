# 工程模板：app 表示生成可执行应用程序
TEMPLATE = app

# 头文件搜索路径（适配本版本交叉编译工具链：OpenCV4 头文件位于 /usr/include/opencv4）
# 注意 "=" 前缀是 qmake 的“相对 sysroot 的字面量”标记，保留原样
INCLUDEPATH += \
        =/usr/include/opencv4 \
        =/usr/include

# 链接的 OpenCV 库：
#   opencv_core     - 核心数据结构(Mat 等)
#   opencv_highgui  - 图像显示/窗口(namedWindow/imshow/waitKey)
#   opencv_videoio  - 视频捕获(VideoCapture)
#   opencv_imgproc  - 图像处理(灰度化、滤波、Canny/Sobel/Laplacian)
LIBS += \
        -lopencv_core \
        -lopencv_highgui \
        -lopencv_videoio \
        -lopencv_imgproc

# 部署到板子的 /home/root 目录
target.path=/home/root
INSTALLS += target

SOURCES += \
        main.cpp
