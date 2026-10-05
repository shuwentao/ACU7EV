/*
 * drm.cpp —— 基于 libdrm 的显示初始化（KMS 设置）
 *
 * 整体思路：
 *   本程序的画面由两层叠加而成——底层是 GStreamer kmssink 输出的摄像头视频（写入 DRM plane），
 *   上层是 Qt Quick 界面（通过 Xorg/X11 输出）。因此在 Qt 界面启动之前，
 *   必须先通过 DRM/KMS 把显示通道（CRTC + Connector）设置到目标分辨率，
 *   并配置两个硬件 plane 的层级(zpos)与全局 alpha，由内核完成硬件合成。
 */
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm_fourcc.h>
#include <QDebug>

#include "drm_init.h"

// 期望设置的显示分辨率与刷新率
// 若显示器不支持该模式，会退化为使用它支持的第一个模式
int set_screen_width = 1920;
int set_screen_height = 1080;
unsigned int set_screen_fps = 60;

/*
 * 按“属性名”设置某个 plane 的 DRM 属性（如 zpos、g_alpha_en 等）
 * 参数：
 *   fd        - /dev/dri/card0 的文件描述符
 *   plane_id  - 目标 plane 的 ID
 *   prop_name - 属性名（字符串，由驱动动态导出）
 *   prop_val  - 要写入的属性值
 * 返回：0 表示成功；负数表示失败（属性不存在或写入失败）
 * 说明：不同版本/厂商的驱动导出的属性集合不同，用“名字查找”比硬编码属性 ID 更稳健。
 */
int drm_set_plane_prop(int fd, unsigned int plane_id, const char *prop_name, int prop_val)
{
    drmModeObjectPropertiesPtr props;
    int ret = -1;

    // 取出该 plane 上所有属性的 ID 列表
    props = drmModeObjectGetProperties(fd, plane_id, DRM_MODE_OBJECT_PLANE);
    if (!props) {
        return ret;
    }

    // 逐个查询属性 ID 对应的名字，找到与 prop_name 匹配的那个
    for (uint32_t i = 0; i < props->count_props; i++) {
        drmModePropertyPtr prop = drmModeGetProperty(fd, props->props[i]);
        if (!prop) {
            continue;
        }

        if (!strcmp(prop->name, prop_name)) {
            // 通过该属性的 ID 写入属性值（注意是 prop_id，不是 prop_val）
            ret = drmModeObjectSetProperty(fd, plane_id,
                                           DRM_MODE_OBJECT_PLANE,
                                           prop->prop_id,
                                           prop_val);
            drmModeFreeProperty(prop);
            break;
        }
        drmModeFreeProperty(prop);
    }
    drmModeFreeObjectProperties(props);

    return ret;
}

/*
 * 初始化 DRM 显示通道
 * 返回： 0  成功
 *       -1 致命错误（打开设备/获取资源失败等），无需重试
 *       -2 未检测到显示器（connector 上没有可用显示模式），调用方可稍后重试
 * 注意：成功时不会关闭 /dev/dri/card0 的文件描述符——一旦释放 DRM master，
 *       内核就会撤销刚才生效的 CRTC/plane 设置，画面随即消失。
 */
int init_drm(void)
{
    int fd = -1;
    int ret = 0;
    drmModeConnector *conn = NULL;
    drmModeRes *res = NULL;
    drmModePlaneRes *plane_res = NULL;

    uint32_t conn_id;
    uint32_t crtc_id;
    uint32_t plane_id0;
    uint32_t plane_id1;

    int screen_index = -1;

    // 打开 DRM 设备节点；O_CLOEXEC 防止该 fd 被后续 exec 的子进程继承
    fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        qDebug("open /dev/dri/card0 fail");
        return -1;
    }

    // 获取全局资源：CRTC / connector / encoder 的 ID 列表
    res = drmModeGetResources(fd);
    if (!res) {
        qDebug("drmModeGetResources fail");
        ret = -1;
        goto ERROR_01; // 尚未申请任何资源，只需关闭 fd
    }
    if (res->count_crtcs <= 0 || res->count_connectors <= 0) {
        qDebug("no crtc/connector found");
        ret = -1;
        goto ERROR_02;
    }
    // 本程序是单屏应用，直接取第一个 CRTC 与第一个 connector
    crtc_id = res->crtcs[0];
    conn_id = res->connectors[0];

    // 打开 UNIVERSAL_PLANES 能力，否则只能看到 primary/cursor plane，
    // 拿不到 overlay plane（本例中的叠加层）
    drmSetClientCap(fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);
    plane_res = drmModeGetPlaneResources(fd);
    if (!plane_res) {
        qDebug("drmModeGetPlaneResources fail");
        ret = -1;
        goto ERROR_02;
    }
    // 仅适配 MPSOC DP：2 个 plane（底层视频层 + 上层叠加层）
    if (plane_res->count_planes != 2) {
        qDebug("this app only fix mpsoc dp");
        ret = -1;
        goto ERROR_03;
    }
    plane_id0 = plane_res->planes[0];
    plane_id1 = plane_res->planes[1];
    // plane_id0 为底层视频层（zpos=0），需要手动指定 plane 时可用 KMS_PLANE_ID 覆盖
    qDebug("planes: video=%u overlay=%u", plane_id0, plane_id1);

    // 获取 connector 信息，主要目的是拿到它支持的显示模式列表（modes）
    conn = drmModeGetConnector(fd, conn_id);
    if (!conn) {
        qDebug("drmModeGetConnector fail");
        ret = -1;
        goto ERROR_03;
    }
    if (conn->count_modes <= 0) {
        qDebug("none monitor found");
        ret = -2; // 显示器未就绪，交给调用方延时重试
        goto ERROR_04;
    }
    // 在显示器支持的模式里挑选：优先挑分辨率匹配的；
    // 若有多个分辨率相同的模式，则再挑刷新率等于 set_screen_fps 的那个
    for (int i = 0; i < conn->count_modes; i++) {
        if ((set_screen_width > 0) && (set_screen_height > 0)) {
            if ((conn->modes[i].hdisplay == set_screen_width) && (conn->modes[i].vdisplay == set_screen_height)) {
                if (screen_index < 0) {
                    screen_index = i;
                } else {
                    if ((set_screen_fps > 0) && (conn->modes[i].vrefresh == set_screen_fps)) {
                        screen_index = i;
                    }
                }
            }
        } else {
            // 未指定分辨率时直接使用显示器报告的第一个（原生）模式
            screen_index = 0;
        }
    }
    if (screen_index < 0) {
        qDebug("mode %dx%d@%d not support, use the first mode",
               set_screen_width, set_screen_height, set_screen_fps);
        screen_index = 0;
    }

    qDebug("set screen:%dx%d@%d",
           conn->modes[screen_index].hdisplay,
           conn->modes[screen_index].vdisplay,
           conn->modes[screen_index].vrefresh);
    // 应用显示模式：把 CRTC 与 connector 绑定并按选定模式输出
    // fb_id 传 -1 表示这里不设置帧缓冲，画面由后续的 plane 提供
    drmModeSetCrtc(fd, crtc_id, -1,
                   0, 0,
                   &conn_id, 1,
                   &conn->modes[screen_index]);

    // plane_id0（视频层）放在最底层，plane_id1（Qt 叠加层）放在它上面
    drm_set_plane_prop(fd, plane_id0, "zpos", 0);
    drm_set_plane_prop(fd, plane_id1, "zpos", 1);
    // 关闭叠加层的全局 alpha，改为按像素 alpha 与底层视频混合，实现透明叠加
    drm_set_plane_prop(fd, plane_id1, "g_alpha_en", 0);

ERROR_04:
    if (conn)
        drmModeFreeConnector(conn);
ERROR_03:
    if (plane_res)
        drmModeFreePlaneResources(plane_res);
ERROR_02:
    if (res)
        drmModeFreeResources(res);
ERROR_01:
    // 出错时关闭 fd；成功时保持打开，避免关闭 DRM master 后设置被内核撤销
    if (ret < 0)
        close(fd);
    return ret;
}
