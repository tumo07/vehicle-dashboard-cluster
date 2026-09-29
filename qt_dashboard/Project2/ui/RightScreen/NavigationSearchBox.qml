import QtQuick 2.15

Rectangle {
    id: navSearchBox
    radius: 5
    color: "#f0f0f0"
    Image {
        id: searchIcon
        source: "../assets/search.png"
        anchors {
            left: parent.left
            leftMargin: 10
            verticalCenter: parent.verticalCenter
        }
        height: parent.height * .45
        fillMode: Image.PreserveAspectFit
    }
    Text {
        id: navigationPlaceholderText
        visible: navigationTextInput.text.length === 0
        color: "#373737"
        text: qsTr("Navigation")
        anchors {
            left: searchIcon.right
            leftMargin: 10
            verticalCenter: parent.verticalCenter
        }
    }
    TextInput {
        id: navigationTextInput
        clip: true
        anchors {
            left: searchIcon.right
            right: parent.right
            bottom: parent.bottom
            top: parent.top
            leftMargin: 10
        }
        verticalAlignment: Text.AlignVCenter
        font.pixelSize: 16
    }
}
