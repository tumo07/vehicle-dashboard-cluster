import QtQuick 2.15

Rectangle {
    id: bottomBar
    anchors {
        left: parent.left
        right: parent.right
        bottom: parent.bottom
    }
    height: parent.height / 12
    color: "black"
    Image {
        id: carSettingIcon
        source: "../assets/carIcon.png"
        anchors {
            left: parent.left
            verticalCenter: parent.verticalCenter
            margins: 20
        }
        fillMode: Image.PreserveAspectFit
        height: parent.height * 0.85
    }
}
