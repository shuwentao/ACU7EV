#ifndef OPENGL_YUV_H
#define OPENGL_YUV_H

#include <QtWidgets>
#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLBuffer>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>

/*
 * uOpenglYuv —— 用 OpenGL 渲染 YUYV(4:2:2) 视频帧的窗口部件
 *
 * YUYV 的打包格式是 Y0 U0 Y1 V0（4 字节描述 2 个像素）。这里不把数据转成 RGB，
 * 而是用"同一块数据、两种纹理视角"的技巧把它直接喂给 GPU：
 *   纹理Y : 以 GL_LUMINANCE_ALPHA（每纹素 2 字节）、尺寸 width x height 读取。
 *           第 i 个纹素对应字节 (2i, 2i+1)，于是 L=Y、A=U(偶纹素)/V(奇纹素)，
 *           着色器取 .x 就能得到每个像素正确的 Y 分量。
 *   纹理UV: 以 GL_RGBA（每纹素 4 字节）、尺寸 width/2 x height 读取。
 *           每个纹素正好是一个 YUYV 宏像素：R=Y0, G=U0, B=Y1, A=V0，
 *           着色器取 .y 得 U、.w 得 V（U/V 水平方向天然是 1/2 分辨率）。
 * YUV->RGB 的矩阵运算全部在片元着色器中完成，CPU 端零开销。
 */
class uOpenglYuv : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT

public:
    explicit uOpenglYuv(QWidget *parent = nullptr);
    ~uOpenglYuv();
    unsigned short width, height;  // 视频帧宽高（决定纹理尺寸）

protected:
    void initializeGL() Q_DECL_OVERRIDE; // 首次显示时调用：编译着色器、创建纹理、设置顶点
    void paintGL() Q_DECL_OVERRIDE;      // 每次重绘时调用：上传纹理并绘制

private:
    QOpenGLShaderProgram *m_program; // YUV->RGB 转换的着色器程序

public:
    unsigned char *img_yuv;  // 指向当前要显示的 YUYV 数据（由采集线程提供，不做拷贝）

    GLuint textureY;     // Y 分量纹理对象（对应纹理单元 0）
    GLuint textureUV;    // UV 分量纹理对象（对应纹理单元 1）
    GLuint TextureID_Y;  // 着色器中 tex_y 采样器的 uniform 位置
    GLuint TextureID_UV; // 着色器中 tex_uv 采样器的 uniform 位置
};

#endif // OPENGL_YUV_H
