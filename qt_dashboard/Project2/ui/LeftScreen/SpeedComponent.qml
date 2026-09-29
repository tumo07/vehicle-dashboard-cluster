import QtQuick 2.15

Item {
    id: root
    implicitWidth: 240
    implicitHeight: 64
    property real speedKmh: 0
    Row {
        anchors.centerIn: parent
        spacing: 8
        Text {
            id: valueText
            text: Math.round(root.speedKmh)
            color: root.speedKmh > 90 ? "#ff867a"
                                      : root.speedKmh > 80 ? "#ffce70"
                                                           : "#edf4f7"
            font.pixelSize: 48
            font.weight: Font.DemiBold
        }
        Text {
            anchors.baseline: valueText.baseline
            text: "km/h"
            color: "#a0aeb9"
            font.pixelSize: 15
        }
    }
}
