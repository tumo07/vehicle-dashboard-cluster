import QtQuick 2.15

Item {
    id: root

    implicitWidth: 276
    implicitHeight: 88

    property real fuelPercent: 0
    property real coolantTempC: 0
    property real batteryVoltage: 0

    readonly property real fuelLevel:
        Math.max(0, Math.min(100, fuelPercent))

    Row {
        anchors.fill: parent
        spacing: 8

        Repeater {
            model: [
                {
                    label: "FUEL",
                    value: Math.round(root.fuelLevel).toString(),
                    unit: "%",
                    accent: "#81e6ce"
                },
                {
                    label: "COOLANT",
                    value: Math.round(root.coolantTempC).toString(),
                    unit: "°C",
                    accent: "#87c9ff"
                },
                {
                    label: "BATTERY",
                    value: root.batteryVoltage.toFixed(1),
                    unit: "V",
                    accent: "#c4b5fd"
                }
            ]

            delegate: Rectangle {
                id: card

                width: (root.width - 16) / 3
                height: root.height

                radius: 14
                color: "#182129"

                border.width: 1
                border.color: "#34424d"

                // Nhãn thông số
                Text {
                    anchors {
                        top: parent.top
                        topMargin: 12
                        horizontalCenter: parent.horizontalCenter
                    }

                    text: modelData.label
                    color: "#a0aeb9"

                    font.pixelSize: 9
                    font.letterSpacing: 1
                }

                // Giá trị và đơn vị
                Row {
                    anchors {
                        horizontalCenter: parent.horizontalCenter
                        top: parent.top
                        topMargin: 30
                    }

                    spacing: 3

                    Text {
                        id: valueText

                        text: modelData.value
                        color: "#edf4f7"

                        font.pixelSize: 22
                        font.weight: Font.DemiBold
                    }

                    Text {
                        anchors.baseline: valueText.baseline

                        text: modelData.unit
                        color: modelData.accent

                        font.pixelSize: 11
                    }
                }

                // Thanh nhiên liệu
                Rectangle {
                    id: fuelTrack

                    visible: index === 0

                    anchors {
                        left: parent.left
                        right: parent.right
                        bottom: parent.bottom
                        margins: 12
                    }

                    height: 4
                    radius: 2
                    color: "#2c3944"

                    Rectangle {
                        width: fuelTrack.width * root.fuelLevel / 100
                        height: parent.height

                        radius: 2
                        color: "#81e6ce"

                        Behavior on width {
                            NumberAnimation {
                                duration: 250
                                easing.type: Easing.OutCubic
                            }
                        }
                    }
                }

                // Điểm nhấn màu cho nhiệt độ và điện áp
                Rectangle {
                    visible: index !== 0

                    anchors {
                        horizontalCenter: parent.horizontalCenter
                        bottom: parent.bottom
                        bottomMargin: 12
                    }

                    width: 18
                    height: 3
                    radius: 1.5

                    color: modelData.accent
                    opacity: 0.8
                }
            }
        }
    }
}
