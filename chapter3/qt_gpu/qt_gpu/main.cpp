#include <unistd.h>

#include <QApplication>
#include <QFileInfo>
#include <QFontDatabase>

#include "mainwindow.h"

/* 系统开机脚本可能已经启动了 Xorg，重复启动会报
   "Server is already active for display 0"，因此先探测再启动 */
static bool xorgIsRunning()
{
    return QFileInfo::exists(QStringLiteral("/tmp/.X11-unix/X0")) ||
           QFileInfo::exists(QStringLiteral("/tmp/.X0-lock"));
}

int main(int argc, char *argv[])
{
    qputenv("DISPLAY", ":0.0");

    // 启动 Xorg（后台运行），OpenGL 窗口通过 X11 输出
    if (!xorgIsRunning()) {
        if (system("/usr/bin/Xorg :0 &") == -1)
            qWarning("start /usr/bin/Xorg failed");
        sleep(1);
    }

    QApplication a(argc, argv);

    // 从工程资源中加载宋体字库
    if (QFontDatabase::addApplicationFont(QStringLiteral(":/simsun.ttc")) < 0)
        qWarning("load :/simsun.ttc failed");

    QFont font;
    font.setFamily(QStringLiteral("simsun"));
    a.setFont(font);

    MainWindow w;
    w.show();

    return a.exec();
}
