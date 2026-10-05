#include "mainwindow.h"
#include <unistd.h>
#include <QApplication>

// 程序入口：负责初始化嵌入式 Linux 的图形显示环境，
// 再启动 Qt 应用并创建主窗口。
int main(int argc, char *argv[])
{
    // 设置 X11 显示环境变量（指定输出到 0 号显示器的 0 号屏幕）
    setenv("DISPLAY", ":0.0", 0);
    // 先探测 X 是否已可达：xset q 返回 0 表示 X 已在运行，则不再重复启动，
    // 避免 "Server is already active for display 0" 报错
    if (system("xset q >/dev/null 2>&1") != 0)
    {
        // 启动无显示管理器(Display Manager)的轻量级 X Server，适用于嵌入式设备直接接屏
        system("/etc/init.d/xserver-nodm start");
        sleep(2); // 等待 X Server 启动完成
    }

    //取消显示输出电源管理(屏保)与 DPMS 节能模式，
    //避免长时间静止后屏幕熄灭
    system("xset s 0 0");        // 关闭屏保
    system("xset dpms 0 0 0");   // 关闭 DPMS 休眠

    //设置显示器分辨率（输出接口 DP-1，分辨率 1280x720）
    system("xrandr --output DP-1 --mode 1280x720");

    //等待分辨率设置完成（等显示器重新同步）
    sleep(3);

    // 创建 Qt 应用对象（管理 GUI 事件循环）
    QApplication a(argc, argv);
    // 创建并显示主窗口
    MainWindow w;
    w.show();

    // 进入 Qt 事件循环，程序在此处持续运行直到窗口关闭
    return a.exec();
}
