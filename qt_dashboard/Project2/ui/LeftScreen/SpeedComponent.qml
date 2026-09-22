import QtQuick 2.15

Column {
    anchors.centerIn: parent
    spacing: 0
    Text {
        anchors.horizontalCenter: parent.horizontalCenter
        text: controller.dashboardModel.data.speedKmh + " km/h"
        color: "#dddddd"
        font.pixelSize: 28
        font.bold: true
    }
}
