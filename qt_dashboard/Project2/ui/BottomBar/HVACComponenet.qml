import QtQuick 2.15

Item {
    id: root

    property string fontColor: "#f0eded"
    property var hvacController
    property int spacing: 20

    implicitWidth: decrementBtn.width
                   + root.spacing
                   + targetTempDisplay.implicitWidth
                   + root.spacing
                   + incrementBtn.width

    implicitHeight: 50

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
            anchors.centerIn: parent
            text: "<"
            font.pixelSize: 20
            color: root.fontColor
        }

        MouseArea {
            anchors.fill: parent

            onClicked: {
                root.hvacController.setHVacTemp(
                            root.hvacController.hVacTemp - 1
                            )
            }
        }
    }

    Text {
        id: targetTempDisplay

        anchors {
            left: decrementBtn.right
            leftMargin: root.spacing
            verticalCenter: parent.verticalCenter
        }

        text: root.hvacController.hVacTemp + "°C"
        color: root.fontColor
        font.pixelSize: 24
    }

    Rectangle {
        id: incrementBtn

        anchors {
            left: targetTempDisplay.right
            leftMargin: root.spacing
            top: parent.top
            bottom: parent.bottom
        }

        width: height / 2
        color: "black"

        Text {
            anchors.centerIn: parent
            text: ">"
            font.pixelSize: 20
            color: root.fontColor
        }

        MouseArea {
            anchors.fill: parent

            onClicked: {
                root.hvacController.setHVacTemp(
                            root.hvacController.hVacTemp + 1
                            )
            }
        }
    }
}
