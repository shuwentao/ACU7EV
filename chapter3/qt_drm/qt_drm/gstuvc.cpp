/*
 * gstuvc.cpp —— 用 GStreamer 把 USB 摄像头(UVC)画面直接送到 DRM 显示层
 *
 * 作用：在独立线程里启动一条 GStreamer pipeline：
 *     v4l2src(MJPEG) -> jpegdec -> videoconvert -> kmssink
 * kmssink 直接输出到 DRM plane（不经过 X11），因此视频不占用 GPU/CPU 合成开销，
 * Qt Quick 界面再通过 Xorg 叠加在上层显示。
 */
#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <glib.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>

#include <QByteArray>
#include <QString>

/* 当前内核（2022 SDK）DP 节点为 display@fd4a0000，故默认 bus-id 为 fd4a0000.display；
   老内核为 zynqmp-display@fd4a0000，可用环境变量 KMS_BUS_ID 覆盖 */
#ifndef DEFAULT_KMS_BUS_ID
#define DEFAULT_KMS_BUS_ID "fd4a0000.display"
#endif

/*
 * GStreamer 总线消息回调：处理播放结束(EOS)与错误(ERROR)
 * 两者都会退出 GLib 主循环，使线程函数得以继续往下做清理。
 */
gboolean sink_message(GstBus *, GstMessage *message, GMainLoop *loop)
{
    GError *err = NULL;
    gchar *debug = NULL;

    switch (GST_MESSAGE_TYPE(message)) {
    case GST_MESSAGE_EOS:
        printf("Finished play\r\n");
        g_main_loop_quit(loop);
        break;
    case GST_MESSAGE_ERROR:
        // 解析并打印错误信息，便于定位 pipeline 构建/运行失败的原因
        gst_message_parse_error(message, &err, &debug);
        g_print("get error: %s\n", err->message);
        g_error_free(err);
        g_free(debug);
        printf("play error\r\n");
        g_main_loop_quit(loop);
        break;
    default:
        break;
    }
    return TRUE;
}

/* 读取环境变量，未设置或为空时返回默认值（用于把关键参数外置，避免改代码重编译） */
static QString envOr(const char *key, const QString &defaultValue)
{
    QByteArray value = qgetenv(key);
    return value.isEmpty() ? defaultValue : QString::fromLocal8Bit(value);
}

/* 不同版本 / Xilinx 补丁的 kmssink 属性并不一致，
   先探测属性是否存在再设置，不存在则跳过，避免整条 pipeline 失效 */
static void set_sink_prop(GstElement *element, const char *name, const QString &value)
{
    if (!element)
        return;

    QByteArray v = value.toUtf8();
    if (!g_object_class_find_property(G_OBJECT_GET_CLASS(element), name)) {
        printf("kmssink: property \"%s\" not supported, skip\n", name);
        return;
    }
    gst_util_set_object_arg(G_OBJECT(element), name, v.constData());
}

/*
 * GStreamer 采集/播放线程主函数
 * 所有参数均可由环境变量覆盖：
 *   UVC_DEVICE  摄像头设备节点（默认 /dev/video0）
 *   UVC_WIDTH / UVC_HEIGHT / UVC_FPS  采集分辨率与帧率
 *   KMS_BUS_ID  kmssink 的 DRM 设备 bus-id
 *   KMS_FULLSCREEN_OVERLAY  是否全屏叠加
 *   KMS_PLANE_ID / KMS_SINK_TYPE  可选，一般不设置
 *   GST_PIPELINE  直接给出完整的 pipeline 描述，覆盖默认 pipeline
 */
void *task_gst_uvc(void *)
{
    // GStreamer 初始化（可重复调用，内部做了保护）
    gst_init(NULL, NULL);

    const QString device = envOr("UVC_DEVICE", "/dev/video0");
    const QString width = envOr("UVC_WIDTH", "1920");
    const QString height = envOr("UVC_HEIGHT", "1080");
    const QString fps = envOr("UVC_FPS", "30");
    const QString busId = envOr("KMS_BUS_ID", QStringLiteral(DEFAULT_KMS_BUS_ID));
    const QString fullscreen = envOr("KMS_FULLSCREEN_OVERLAY", "true");

    /* 默认 pipeline 与板端验证通过的命令一致：
       v4l2src ! image/jpeg ... ! jpegdec ! videoconvert ! video/x-raw ... ! kmssink
       如需整体替换，可用环境变量 GST_PIPELINE 直接给完整描述 */
    QString desc = envOr("GST_PIPELINE", QString());
    if (desc.isEmpty()) {
        desc = QStringLiteral("v4l2src device=%1 ! image/jpeg,width=%2,height=%3,framerate=%4/1 ! "
                              "jpegdec ! videoconvert ! video/x-raw,width=%2,height=%3 ! "
                              "kmssink name=kms")
                   .arg(device).arg(width).arg(height).arg(fps);
    }
    printf("pipeline: %s\n", desc.toUtf8().constData());

    // GLib 主循环：用于接收总线消息并在 EOS/ERROR 时退出
    GMainLoop *loop = g_main_loop_new(NULL, FALSE);

    GError *error = NULL;
    // 用文本描述构建 pipeline（等价于命令行 gst-launch-1.0）
    GstElement *pipeline = gst_parse_launch(desc.toUtf8().constData(), &error);
    if (!pipeline) {
        printf("gst_parse_launch fail: %s\n", error ? error->message : "unknown error");
        g_clear_error(&error);
        g_main_loop_unref(loop);
        return NULL;
    }

    // 取出名为 kms 的 kmssink 元素，再设置它的属性
    GstElement *kms = gst_bin_get_by_name(GST_BIN(pipeline), "kms");
    set_sink_prop(kms, "bus-id", busId);
    set_sink_prop(kms, "fullscreen-overlay", fullscreen);
    printf("kmssink: bus-id=%s fullscreen-overlay=%s\n",
           busId.toUtf8().constData(), fullscreen.toUtf8().constData());

    // 这两个属性默认不设置（命令行验证通过的用法里没有），需要时用环境变量打开
    QString planeId = envOr("KMS_PLANE_ID", QString());
    if (!planeId.isEmpty())
        set_sink_prop(kms, "plane-id", planeId);
    QString sinkType = envOr("KMS_SINK_TYPE", QString());
    if (!sinkType.isEmpty())
        set_sink_prop(kms, "sink-type", sinkType);

    if (kms)
        gst_object_unref(kms);

    // 监听总线消息，出错时能及时退出主循环
    GstBus *bus = gst_element_get_bus(pipeline);
    gst_bus_add_watch(bus, (GstBusFunc)sink_message, loop);
    gst_object_unref(bus);

    // 启动播放；失败则直接清理退出
    if (gst_element_set_state(pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
        printf("gst set playing failed\r\n");
        gst_object_unref(pipeline);
        g_main_loop_unref(loop);
        return NULL;
    }

    // 阻塞在此，直到 EOS 或 ERROR 触发 g_main_loop_quit
    g_main_loop_run(loop);

    printf("playing out\r\n");

    // 收尾：停止 pipeline 并释放资源
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    g_main_loop_unref(loop);

    return NULL;
}

/* 启动 GStreamer 采集线程：创建后立即 detach，线程结束由系统回收资源 */
void start_gst_uvc(void)
{
    pthread_t pid;

    pthread_create(&pid, NULL, task_gst_uvc, NULL);
    pthread_detach(pid);
}
