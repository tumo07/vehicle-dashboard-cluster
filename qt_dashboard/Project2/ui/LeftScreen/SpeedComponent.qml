import QtQuick 2.15

Column {
    anchors.centerIn: parent
    spacing: 0
    // property int simulatedSpeed: 50
    // property int direction: 1
    // id: speedDisplay
    // Timer {
    //     interval: 500
    //     running: true
    //     repeat: true

    //     onTriggered: {
    //         if (speedDisplay.simulatedSpeed === 100)
    //             speedDisplay.direction = -1
    //         else if (speedDisplay.simulatedSpeed === 50)
    //             speedDisplay.direction = 1

    //         speedDisplay.simulatedSpeed += speedDisplay.direction
    //     }
    // }
    Text {
        anchors.horizontalCenter: parent.horizontalCenter
        text: controller.dashboardModel.data.speedKmh + " km/h" //controller.dashboardModel.data.speedKmh
        color: speedDisplay.simulatedSpeed > 90 ? "red"
                                                : speedDisplay.simulatedSpeed > 80 ? "yellow"
                                                                                   : "#dddddd"
        font.pixelSize: 28
        font.bold: true
    }
}
