/*
 * main.qml —— Qt Quick 叠加层界面
 * 该界面通过 Xorg 输出，叠加在 DRM 底层视频（摄像头画面）之上显示。
 * 根元素背景为全透明，因此未绘制区域可以透出底层的视频画面。
 */
import QtQuick 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: root
    visible: true

    // 从程序资源中加载宋体字库，供中文标题使用
    FontLoader { id: fontFamily; source: "qrc:/simsun.ttc" }

    // #00000000 = ARGB 全透明背景
    color: "#00000000"

    // Numeric panel
    // 左上角的半透明信息面板（半透明绿色背景 + 白色边框）
    Rectangle {
        id: numericPanel
        width: 340
        height: 480
        border.color: "white"
        border.width: 2
        color: "#8000ff00"
        visible: true
        // 相对父窗口左上角定位，留出 20px 边距
        anchors {
            left: parent.left
            top: parent.top
            leftMargin: 20
            topMargin: 20
        }
        ColumnLayout {
            id: numColumnLayout
            width: 340
            spacing: 1
            // 面板顶部的标题栏（白色底 + 中文标题）
            Rectangle {
                width: parent.width
                height: 60
                color: "white"
                Layout.alignment: Qt.AlignTop
                Layout.topMargin: 0
                RowLayout {
                    anchors.rightMargin: 0
                    anchors.bottomMargin: 0
                    anchors.leftMargin: 0
                    anchors.topMargin: 0
                    anchors.fill: parent
                    Text {
                        text: "上海芯驿电子"
                        verticalAlignment: Text.AlignVCenter
                        horizontalAlignment: Text.AlignHCenter
                        Layout.leftMargin: 10
                        Layout.alignment: Qt.AlignLeft
                        font.bold: false
                        font.pointSize: 28
                        font.family: fontFamily.name // 使用上面 FontLoader 加载的字体
                    }
                }
            }
        }
    }
}
