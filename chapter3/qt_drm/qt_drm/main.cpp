#include <QApplication>
#include <QColor>
#include <QFileInfo>
#include <QQuickItem>
#include <QQuickView>
#include <QSurfaceFormat>
#include <QUrl>
#include <unistd.h>

#include "drm_init.h"

void start_gst_uvc(void);

/* 系统开机脚本可能已经启动了 Xorg，重复启动会报
   "Server is already active for display 0"，因此先探测再启动 */
static bool xorgIsRunning()
{
    return QFileInfo::exists(QStringLiteral("/tmp/.X11-unix/X0")) ||
           QFileInfo::exists(QStringLiteral("/tmp/.X0-lock"));
}

int main(int argc, char *argv[])
{
    int ret;

    qputenv("DISPLAY", ":0.0");
    // 先初始化 DRM 显示通道（设置分辨率与 plane 层级）。
    // 返回 -2 表示显示器还没就绪（可能刚上电），每秒重试一次直到成功；
    // 返回其它负值属于致命错误，直接退出。
    while (1) {
        ret = init_drm();
        if (ret < 0) {
            if (ret == -2) {
                sleep(1);
            } else {
                return -1;
            }
        } else {
            break;
        }
    }

    // 启动 Xorg（后台运行），Qt Quick 叠加层通过 X11 输出
    if (!xorgIsRunning()) {
        if (system("/usr/bin/Xorg :0 &") == -1)
            qWarning("start /usr/bin/Xorg failed");
        sleep(1);
    }

    QApplication app(argc, argv);

    // 申请 8 位 alpha 通道，Qt Quick 窗口才能做成透明叠加层
    QSurfaceFormat surfaceFormat;
    surfaceFormat.setAlphaBufferSize(8);

    QQuickView viewer;
    viewer.setFormat(surfaceFormat);
    // setClearBeforeRendering() 自 Qt 5.14 起已废弃，Qt 6 中移除；
    // Qt 5 下保留原有行为（透明叠加需要清屏）
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    viewer.setClearBeforeRendering(true);
#endif
    viewer.setColor(QColor(Qt::transparent));
    viewer.setSource(QUrl(QStringLiteral("qrc:/main.qml")));

    if (viewer.status() != QQuickView::Ready) {
        qWarning("load qrc:/main.qml failed");
        return -1;
    }

    // QML 根元素默认没有固定尺寸，这里显式铺满 1920x1080
    QQuickItem *item = viewer.rootObject();
    if (item) {
        item->setWidth(1920);
        item->setHeight(1080);
        item->update();
    }

    viewer.show();

    // 启动 GStreamer 线程：把摄像头画面输出到 DRM 底层 plane
    start_gst_uvc();

    return app.exec();
}
