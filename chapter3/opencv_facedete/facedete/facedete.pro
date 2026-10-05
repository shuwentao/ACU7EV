#-------------------------------------------------
#
# Project updated to match QtCreator 6.0.2 (2026-10-04) / OpenCV4
#
#-------------------------------------------------

# 引入 Qt 核心与 GUI 模块
QT       += core gui

# Qt5 及以上需要显式引入 widgets 模块（QWidget/QLabel 等）
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

# 生成的可执行文件名
TARGET = facedete
# 工程模板：app 表示生成应用程序
TEMPLATE = app

# The following define makes your compiler emit warnings if you use
# any feature of Qt which as been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0


SOURCES += main.cpp\
        mainwindow.cpp \
    capture_video.cpp \
    face_lookup.cpp

HEADERS  += mainwindow.h \
    capture_video.h \
    face_lookup.h


# 头文件搜索路径（适配本版本交叉编译工具链：OpenCV4 头文件位于 /usr/include/opencv4）
# 注意 "=" 前缀是 qmake 的“精确字面量”标记，保留原样
INCLUDEPATH += \
        =/usr/include/opencv4 \
        =/usr/include

# 链接的 OpenCV 库：
#   opencv_core     - 核心数据结构(Mat 等)
#   opencv_highgui  - 图像显示/窗口
#   opencv_videoio  - 视频捕获(VideoCapture)
#   opencv_imgproc  - 图像处理(色彩空间转换等)
#   opencv_face     - 人脸相关
#   opencv_objdetect- 目标检测(Haar 级联 CascadeClassifier)
LIBS += \
        -lopencv_core \
        -lopencv_highgui \
        -lopencv_videoio \
        -lopencv_imgproc \
        -lopencv_face \
        -lopencv_objdetect




target.path=/home/root
# 部署 Haar 级联模型文件：让 Qt Creator 一并上传到板子的 /home/root/haar_train
# 注意：INSTALLS 的相对路径相对构建目录解析；haar_train 与 facedete 同级，
# 故用 $$PWD/../ 指向源码目录下实际文件所在位置
haar_files.path = /home/root/haar_train
haar_files.files = $$PWD/../haar_train/haarcascade_frontalface_alt.xml
INSTALLS += target haar_files

RESOURCES +=
