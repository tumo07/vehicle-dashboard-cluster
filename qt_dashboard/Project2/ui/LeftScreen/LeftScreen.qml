import QtQuick 2.15

Rectangle {
    id: leftScreen
    anchors {
        left: parent.left
        right: rightScreen.left
        bottom: bottomBar.top
        top: parent.top
    }
    color: "grey"
    Image {
        id: carRender
        anchors.centerIn: parent
        width: parent.width * .85
        fillMode: Image.PreserveAspectFit
        source: "../assets/carRender.png"
    }
    SpeedComponent {
        id: speedometer
        anchors {
            top: carRender.bottom
            topMargin: 20
            left: parent.left
            right: parent.right
            bottom: parent.bottom
            margins: 20
        }
        height: parent.height / 2.5
    }
}
