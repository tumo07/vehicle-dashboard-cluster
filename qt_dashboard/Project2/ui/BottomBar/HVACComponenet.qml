import QtQuick 2.15

Item {
    property string fontColor: "#f0eded"
    property var hvacController
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
                hvacController.setHVacTemp(hvacController.hVacTemp - 1)
            }
        }
    }
    Text {
        id: targetTempDisplay
        anchors {
            left: decrementBtn.right
            leftMargin: 15
            verticalCenter: parent.verticalCenter
        }
        text: qsTr(hvacController.hVacTemp + "°C")
        color: fontColor
        font.pixelSize: 24
    }
    Rectangle {
        id: incrementBtn
        anchors {
            left: targetTempDisplay.right
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
                hvacController.setHVacTemp(hvacController.hVacTemp + 1)
            }
        }
    }
}
