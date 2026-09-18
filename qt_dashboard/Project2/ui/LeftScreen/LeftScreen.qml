import QtQuick 2.15

Rectangle {
    id: leftScreen
    anchors {
        left: parent.left
        right: rightScreen.left
        bottom: bottomBar.top
        top: parent.top
    }
    color: "white"
    Image {
        id: carRender
        anchors.centerIn: parent
        width: parent.width * .85
        fillMode: Image.PreserveAspectFit
        source: "../assets/carRender.png"
    }
}
