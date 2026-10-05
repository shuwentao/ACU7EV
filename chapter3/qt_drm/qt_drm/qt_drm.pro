#-------------------------------------------------
#
# Project updated to match QtCreator 6.0.2 (2026-10-05) / Qt 5.15
#
#-------------------------------------------------

# 引入 Qt 核心、GUI 与 QML 模块
QT       += core gui qml quick

# Qt5 及以上需要显式引入 widgets 模块（QApplication 等）
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

# 生成的可执行文件名
TARGET = qt_drm
# 工程模板：app 表示生成应用程序
TEMPLATE = app

# C++ 标准：Qt5 使用 c++11，Qt6 起要求 c++17
greaterThan(QT_MAJOR_VERSION, 5) {
    CONFIG += c++17
} else {
    CONFIG += c++11
}

# The following define makes your compiler emit warnings if you use
# any feature of Qt which as been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += main.cpp \
    gstuvc.cpp \
    drm.cpp

HEADERS += drm_init.h

RESOURCES += qml.qrc

# Additional import path used to resolve QML modules in Qt Creator's code model
QML_IMPORT_PATH =

# Additional import path used to resolve QML modules just for Qt Quick Designer
QML_DESIGNER_IMPORT_PATH =

# 头文件搜索路径（适配本版本交叉编译工具链）
# 注意 "=" 前缀是 qmake 的“相对 sysroot 的字面量”标记，保留原样
INCLUDEPATH += \
        =/usr/include/gstreamer-1.0 \
        =/usr/lib/gstreamer-1.0/include \
        =/usr/include/glib-2.0 \
        =/usr/lib/glib-2.0/include \
        =/usr/include/libdrm \
        =/usr/include/drm \
        =/usr/include

# 链接的库：
#   gstreamer-1.0 - GStreamer 主库(gst_init/gst_parse_launch 等)
#   gstapp-1.0    - appsink/appsrc
#   gobject-2.0   - GObject 类型系统
#   glib-2.0      - GLib 主循环(GMainLoop)
#   drm           - libdrm(xf86drm/xf86drmMode)
# 注意：不要链接 -lkms。本工程只用到 libdrm 的 drmMode*/drmSetClientCap 接口，
# 并未调用任何 kms_* 函数；板子文件系统里通常也没有 libkms.so.1，
# 链接它会让程序启动时因找不到该库而报 exit code 127。
LIBS += \
        -lgstreamer-1.0 \
        -lgstapp-1.0 \
        -lgobject-2.0 \
        -lglib-2.0 \
        -ldrm \
        -lpthread

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /home/root
!isEmpty(target.path): INSTALLS += target
