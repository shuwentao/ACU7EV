#-------------------------------------------------
#
# Project updated to match QtCreator 6.0.2 (2026-10-05) / Qt 5.15
#
#-------------------------------------------------

# 引入 Qt 核心、GUI 模块
QT       += core gui

# Qt5 起 QOpenGLWidget 位于 widgets 模块中，需要显式引入
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

# Qt5 中 opengl 模块（QtOpenGL / QGL*）仍然存在，Qt6 中已被移除/改名
lessThan(QT_MAJOR_VERSION, 6): QT += opengl

# 生成的可执行文件名
TARGET = qt_gpu
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
    mainwindow.cpp \
    opengl_yuv.cpp \
    cimagetask.cpp

HEADERS += mainwindow.h \
    opengl_yuv.h \
    cimagetask.h

RESOURCES += resource.qrc

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /home/root
!isEmpty(target.path): INSTALLS += target
