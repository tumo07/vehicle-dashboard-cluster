import QtQuick 2.15
import QtQuick.Effects

Item {
    property string fontColor: "#f0eded"
    Rectangle {
        id: decrementBtn
        anchors {
            left: parent.left
            top: parent.top
            bottom: parent.bottom
        }
        width: height / 2
        color: "black"
        Text {
            id: decrementTxt
            anchors.centerIn: parent
            text: qsTr("<")
            font.pixelSize: 20
            color: fontColor
        }
        MouseArea {
            anchors.fill: parent
            onClicked: {
                controller.model.setVolumeLevel(controller.model.volumeLevel - 1)
            }
        }
    }
    Image {
        id: volumeIcon
        anchors {
            left: decrementBtn.right
            leftMargin: 15
            verticalCenter: parent.verticalCenter
        }
        source: controller.model.volumeLevel <= 0
                ? "../assets/0-volume-white.png"
                : controller.model.volumeLevel <= 30
                  ? "../assets/1-volume-white.png"
                  : controller.model.volumeLevel <= 50
                    ? "../assets/2-volume-white.png"
                    : "../assets/3-volume-white.png"
        width: 30
        height: 30
        fillMode: Image.PreserveAspectFit
        MouseArea {
            anchors.fill: parent
            onClicked: {
                controller.model.setVolumeLevel(
                    controller.model.volumeLevel === 0 ? 50 : 0
                )
            }
        }
    }
    Rectangle {
        id: incrementBtn
        anchors {
            left: volumeIcon.right
            leftMargin: 15
            top: parent.top
            bottom: parent.bottom
        }
        width: height / 2
        color: "black"
        Text {
            id: incrementTxt
            anchors.centerIn: parent
            text: qsTr(">")
            font.pixelSize: 20
            color: fontColor
        }
        MouseArea {
            anchors.fill: parent
            onClicked: {
                controller.model.setVolumeLevel(controller.model.volumeLevel + 1)
            }
        }
    }
}
