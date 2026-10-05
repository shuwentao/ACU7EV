#ifndef DRM_INIT_H
#define DRM_INIT_H

/*
 * 初始化 DRM/KMS 显示通道（打开 /dev/dri/card0、设置分辨率、配置 plane 层级）
 * 返回值： 0  成功
 *         -1 致命错误（打开设备或获取资源失败），无需重试
 *         -2 未检测到显示器（connector 无可用的显示模式），调用方可延时后重试
 * 注意：成功时 card0 的文件描述符会保持打开，不要在使用期间关闭，
 *       否则释放 DRM master 会导致已生效的显示设置被内核撤销。
 */
int init_drm(void);

#endif // DRM_INIT_H
