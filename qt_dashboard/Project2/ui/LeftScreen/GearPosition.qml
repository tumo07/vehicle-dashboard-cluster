import QtQuick 2.15

Rectangle {
    id: root

    implicitWidth: 276
    implicitHeight: 130

    radius: 20
    color: "#182129"
    border.width: 1
    border.color: "#34424d"

    enum Gear {
        Park = 0,
        Reverse = 1,
        Neutral = 2,
        Drive = 3,
        Sport = 4,
        Unknown = 255
    }

    property int rawGear: GearPosition.Unknown

    readonly property int currentGear: {
        switch (rawGear) {
        case GearPosition.Reverse:
        case GearPosition.Neutral:
        case GearPosition.Drive:
        case GearPosition.Sport:
            return rawGear

        default:
            return GearPosition.Park
        }
    }

    readonly property var letters: [
        "P", "R", "N", "D", "S"
    ]

    readonly property var names: [
        "PARK", "REVERSE", "NEUTRAL", "DRIVE", "SPORT"
    ]

    readonly property color accent:
        currentGear === GearPosition.Reverse ? "#ffbc66"
        : currentGear === GearPosition.Sport ? "#ff867a"
        : "#81e6ce"

    // Chữ số và tên chế độ nằm giữa phần trên.
    Item {
        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
            bottom: gearStrip.top
        }

        Row {
            anchors.centerIn: parent
            spacing: 16

            Text {
                anchors.verticalCenter: parent.verticalCenter

                text: root.letters[root.currentGear]
                color: root.accent

                font.pixelSize: 48
                font.weight: Font.DemiBold
            }

            Rectangle {
                anchors.verticalCenter: parent.verticalCenter

                width: 1
                height: 26
                color: "#34424d"
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter

                text: root.names[root.currentGear]
                color: "#edf4f7"

                font.pixelSize: 14
                font.weight: Font.Medium
                font.letterSpacing: 2
            }
        }
    }

    // Thanh P R N D S
    Rectangle {
        id: gearStrip

        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.bottom

            leftMargin: 14
            rightMargin: 14
            bottomMargin: 12
        }

        height: 42
        radius: 12
        color: "#10181f"

        Row {
            id: gearRow

            anchors.fill: parent
            anchors.margins: 4

            Repeater {
                model: root.letters

                delegate: Item {
                    width: gearRow.width / 5
                    height: gearRow.height

                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: 1

                        radius: 8

                        color: index === root.currentGear
                               ? root.accent
                               : "transparent"

                        Behavior on color {
                            ColorAnimation {
                                duration: 160
                            }
                        }
                    }

                    Text {
                        anchors.centerIn: parent

                        text: modelData

                        color: index === root.currentGear
                               ? "#10221f"
                               : "#94a4b1"

                        font.pixelSize: 18
                        font.bold: true
                    }
                }
            }
        }
    }
}
