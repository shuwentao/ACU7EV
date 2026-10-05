#include "opengl_yuv.h"

uOpenglYuv::uOpenglYuv(QWidget *parent)
    : QOpenGLWidget(parent),
      m_program(nullptr)
{
    // 默认 1080p，实际尺寸由主窗口按摄像头分辨率设置
    width  = 1920;
    height  = 1080;
    // 还没有数据时不绘制纹理，先置空
    img_yuv = nullptr;
}

uOpenglYuv::~uOpenglYuv()
{
    // 释放着色器前必须先让当前线程的 OpenGL 上下文变为本部件的上下文
    makeCurrent();
    delete m_program;
    m_program = nullptr;
    doneCurrent();
}

/*
 * 初始化 OpenGL 资源（在部件首次显示时被调用一次）
 * 步骤：编译链接着色器 -> 设置顶点/纹理坐标 -> 取 uniform 位置 -> 创建两张纹理
 */
void uOpenglYuv::initializeGL()
{
    // 加载 OpenGL 函数指针（QOpenGLFunctions 提供的跨平台封装）
    initializeOpenGLFunctions();
    m_program = new QOpenGLShaderProgram();

    // 顶点着色器：
    //   vertexIn  直接作为裁剪空间坐标（下面传入的是覆盖全屏的 [-1,1] 四边形）
    //   textureIn 为纹理坐标，直接透传给片元着色器
    QString vertexStr =
            "attribute highp vec4 vertexIn;"
            "attribute highp vec2 textureIn;"
            "varying vec2 textureOut;"
            "void main(void) "
            "{"
            "gl_Position = vertexIn;"
            "textureOut = textureIn;"
            "}";

    // 片元着色器：从两张纹理中取出 Y/U/V，再用 BT.601 系数矩阵换算成 RGB
    //   1.164383561643836 = 255/219，是 BT.601 中的 Y 缩放系数
    //   yuv.y / yuv.z 取自 UV 纹理的 G、A 通道（即 U、V），减 0.5 是把 [0,1] 平移到 [-0.5,0.5]
    QString fragmentStr =
            "precision mediump float;"
            "varying vec2 textureOut;"
            "uniform sampler2D tex_y;"
            "uniform sampler2D tex_uv;"
            "void main(void)"
            "{"
            "vec3 yuv;"
            "vec3 rgb;"
            "yuv.x = texture2D(tex_y, textureOut).x;"
            "yuv.y = texture2D(tex_uv, textureOut).y-0.5;"
            "yuv.z = texture2D(tex_uv, textureOut).w-0.5;"
            "rgb = mat3("
            "+1.164383561643836,  +1.164383561643836, +1.164383561643836,"
            "+0.0,                -0.391762290094914, +2.017232142857142,"
            "+1.596026785714286,  -0.812967647237771, +0.0) * yuv;"
            "gl_FragColor = vec4(rgb, 1);"
            "}";
    m_program->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexStr);
    m_program->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentStr);
    m_program->link();
    m_program->bind();

    // 取得着色器中两个 attribute 的位置
    int vertexLocation = m_program->attributeLocation("vertexIn");
    int texcoordLocation = m_program->attributeLocation("textureIn");

    // 顶点坐标：覆盖整个视口的四边形，用 TRIANGLE_STRIP 绘制 4 个点即可
    static const GLfloat vertexVertices[] =
    {
        -1.0f, -1.0f,
        +1.0f, -1.0f,
        -1.0f, +1.0f,
        +1.0f, +1.0f,
    };
    // 纹理坐标：V 方向取反（1.0 -> 0.0），用于校正图像上下翻转
    static const GLfloat textureVertices[] =
    {
        0.0f,  1.0f,
        1.0f,  1.0f,
        0.0f,  0.0f,
        1.0f,  0.0f,
    };
    // 每个顶点 2 个 float，步长默认（紧密排列）
    m_program->setAttributeArray(vertexLocation, GL_FLOAT, vertexVertices, 2);
    m_program->enableAttributeArray(vertexLocation);
    m_program->setAttributeArray(texcoordLocation, GL_FLOAT, textureVertices, 2);
    m_program->enableAttributeArray(texcoordLocation);

    // 取两个采样器的 uniform 位置，用于绑定纹理单元
    TextureID_Y = m_program->uniformLocation("tex_y");
    TextureID_UV = m_program->uniformLocation("tex_uv");

    // 创建 Y 纹理并绑定到纹理单元 0
    glGenTextures(1, &textureY);
    m_program->setUniformValue(TextureID_Y, 0);
    glBindTexture(GL_TEXTURE_2D, textureY);
    // 线性过滤 + CLAMP_TO_EDGE，避免边缘采样越界
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (GLfloat)GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (GLfloat)GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // 创建 UV 纹理并绑定到纹理单元 1
    glGenTextures(1, &textureUV);
    m_program->setUniformValue(TextureID_UV, 1);
    glBindTexture(GL_TEXTURE_2D, textureUV);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (GLfloat)GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (GLfloat)GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

/*
 * 每帧绘制：把最新一帧 YUYV 数据上传为两张纹理后画一个全屏四边形。
 *
 * 说明：这里用的是 glTexImage2D（每帧重新分配纹理存储），实现最简单；
 * 若要进一步优化，可改为首帧 glTexImage2D + 后续 glTexSubImage2D 复用存储。
 */
void uOpenglYuv::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // 纹理单元 0：按 GL_LUMINANCE_ALPHA 读取，每个纹素 2 字节 -> 得到每像素的 Y
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureY);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE_ALPHA, width, height, 0, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, img_yuv);

    // 纹理单元 1：按 GL_RGBA 读取，宽度取 width/2 -> 每个纹素正好是一个 YUYV 宏像素，
    // 其 G、A 通道分别是 U、V
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, textureUV);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width/2, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, img_yuv);

    // 4 个顶点以 TRIANGLE_STRIP 方式画成两个三角形，铺满整个视口
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}
