import QtQuick 2.15
import QtLocation
import QtPositioning

Rectangle {
    id: rightScreen
    anchors {
        right: parent.right
        top: parent.top
        bottom: bottomBar.top
    }
    Map {
        id: map
        anchors.fill: parent
        plugin: Plugin { name: "osm" }
        center: QtPositioning.coordinate(10.8078, 106.6631) // fpt hau giang
        zoomLevel: 10
        MouseArea {
            anchors.fill: parent
            property real lastX
            property real lastY
            onPressed: {
                lastX = mouse.x
                lastY = mouse.y
            }
            onPositionChanged: {
                map.pan(lastX - mouse.x, lastY - mouse.y)
                lastX = mouse.x
                lastY = mouse.y
            }
        }
        WheelHandler {
            id: wheelHandler
            onWheel: function(event) {
                var pos = event.position;
                var delta = event.angleDelta.y;

                if (delta > 0)
                    map.zoomLevel += 1;
                else
                    map.zoomLevel -= 1;
            }
        }
    }    
    Image {
        id: lockIcon
        anchors {
            left: parent.left
            top: parent.top
            margins: 20
        }
        width: parent.width / 40
        fillMode: Image.PreserveAspectFit
        source: (controller.model.carLocked ? "../assets/lock.png" : "../assets/unlock.png")
        MouseArea {
            anchors.fill: parent
            onClicked: {
                controller.model.setCarLocked(!controller.model.carLocked)
            }
        }
    }
    Text {
        id: dateTimeDisplay
        anchors {
            left: lockIcon.right
            leftMargin: 20
            bottom: lockIcon.bottom
        }
        font.pixelSize: 14
        font.bold: true
        color: "black"
        text: controller.model.currentTime
    }
    Text {
        id: outTempDisplay
        anchors {
            left: dateTimeDisplay.right
            leftMargin: 15
            bottom: lockIcon.bottom
        }
        font.pixelSize: 14
        font.bold: true
        color: "black"
        text: controller.model.outdoorTemp + "°C"
    }
    Text {
        id: userNameDisplay
        anchors {
            left: outTempDisplay.right
            leftMargin: 15
            bottom: lockIcon.bottom
        }
        font.pixelSize: 14
        font.bold: true
        color: "black"
        text: controller.model.userName
    }
    NavigationSearchBox {
        id: navSearchBox
        width: parent.width / 3
        height: parent.height / 12
        anchors {
            left: lockIcon.left
            top: lockIcon.bottom
            topMargin: 15
        }
    }
    width: parent.width * 2/3
}
