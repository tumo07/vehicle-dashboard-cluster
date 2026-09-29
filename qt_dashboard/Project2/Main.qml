import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "."
import "ui/BottomBar"
import "ui/RightScreen"
import "ui/LeftScreen"

Window {
    id: mainWindow

    width: 960
    height: 540
    minimumWidth: 960
    maximumWidth: 960
    minimumHeight: 540
    maximumHeight: 540

    visible: true
    title: qsTr("Car cluster")

    readonly property int wiperMode:
        controller.dashboardModel.data.wiperMode

    function showWiperToast() {
        switch (wiperMode) {
        case 1:
            wiperToast.modeText = "INT"
            break
        case 2:
            wiperToast.modeText = "SLOW"
            break
        case 3:
            wiperToast.modeText = "NORMAL"
            break
        case 4:
            wiperToast.modeText = "FAST"
            break
        case 5:
            wiperToast.modeText = "AUTO"
            break
        default:
            toastTimer.stop()
            wiperToast.showing = false
            return
        }
        wiperToast.showing = true
        toastTimer.restart()
    }

    onWiperModeChanged: {
        showWiperToast()
    }

    // show when start UI with mode != OFF
    Component.onCompleted: {
        showWiperToast()
    }

    LeftScreen {
        id: leftScreen
    }

    RightScreen {
        id: rightScreen
    }

    BottomBar {
        id: bottomBar
    }

    Rectangle {
        id: wiperToast

        property bool showing: false
        property string modeText: ""

        anchors {
            top: parent.top
            topMargin: 24
            horizontalCenter: parent.horizontalCenter
        }

        width: toastContent.implicitWidth + 40
        height: 64

        radius: 16
        color: "#202c38"
        border.width: 1
        border.color: "#456078"

        // Nằm trên các màn hình và thanh điều khiển.
        z: 1000

        opacity: showing ? 1 : 0
        visible: opacity > 0

        Behavior on opacity {
            NumberAnimation {
                duration: 180
            }
        }

        Row {
            id: toastContent
            anchors.centerIn: parent
            spacing: 14
            Image {
                anchors.verticalCenter: parent.verticalCenter
                width: 30
                height: 30
                source: "ui/assets/wiper-on.png"
                fillMode: Image.PreserveAspectFit
            }
            Column {
                anchors.verticalCenter: parent.verticalCenter
                spacing: 3
                Text {
                    text: "WIPER"
                    color: "#a9bac9"
                    font.pixelSize: 11
                    font.letterSpacing: 2
                }
                Text {
                    text: wiperToast.modeText
                    color: "#81e6ce"
                    font.pixelSize: 20
                    font.bold: true
                }
            }
        }
    }
    Timer {
        id: toastTimer
        interval: 2000
        repeat: false
        onTriggered: {
            wiperToast.showing = false
        }
    }
}
