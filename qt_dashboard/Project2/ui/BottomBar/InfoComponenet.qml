import QtQuick 2.15
import QtQuick.Layouts

Item {
    Rectangle {
        id: centerBar
        anchors {
            left: driverHvacComponent.right
            right: passengerHvacComponent.left
            top: parent.top
            bottom: parent.bottom
            leftMargin: 50
        }
        RowLayout {
            anchors.centerIn: parent
            spacing: 25
            Image {
                id: iconTurnLeft
                readonly property bool turnActive: controller.dashboardModel.data.turnLeftOn
                property bool blinkOn: false
                source: turnActive && blinkOn
                        ? "../assets/turn-left-on.png"
                        : "../assets/turn-left-off.png"
                onTurnActiveChanged: {
                    blinkOn = turnActive
                }
                Timer {
                    interval: 500
                    running: iconTurnLeft.turnActive
                    repeat: true
                    triggeredOnStart: true
                    onTriggered: iconTurnLeft.blinkOn = !iconTurnLeft.blinkOn
                }                
                Layout.preferredWidth: 30
                Layout.preferredHeight: 30
                fillMode: Image.PreserveAspectFit
            }
            Image {
                id: iconWiper
                Layout.preferredWidth: 30
                Layout.preferredHeight: 30
                source: controller.dashboardModel.data.wiperMode !== 0
                        ? "../assets/wiper-on.png"
                        : "../assets/wiper-off.png"
                fillMode: Image.PreserveAspectFit
            }
            Image {
                id: iconLowBeam
                Layout.preferredWidth: 30
                Layout.preferredHeight: 30
                source: controller.dashboardModel.data.headlightOn !== false
                        ? "../assets/lowbeam-on.png"
                        : "../assets/lowbeam-off.png"
                fillMode: Image.PreserveAspectFit
            }
            Image {
                id: iconHighBeam
                Layout.preferredWidth: 30
                Layout.preferredHeight: 30
                source: controller.dashboardModel.data.highBeamOn !== false
                        ? "../assets/highbeam-on.png"
                        : "../assets/highbeam-off.png"
                fillMode: Image.PreserveAspectFit
            }
            Image {
                id: iconABS
                Layout.preferredWidth: 30
                Layout.preferredHeight: 30
                source: controller.dashboardModel.data.brakeOn !== false
                        ? "../assets/abs-on.png"
                        : "../assets/abs-off.png"
                fillMode: Image.PreserveAspectFit
            }
            Image {
                id: iconTrunk
                readonly property bool trunkOpen:
                    controller.dashboardModel.data.trunkAjar
                Layout.preferredWidth: 30
                Layout.preferredHeight: 30
                source: trunkOpen
                        ? "../assets/trunk-open.png"
                        : "../assets/trunk-close.png"
                fillMode: Image.PreserveAspectFit
            }
            Image {
                id: iconReverse
                readonly property int parkingLevel:
                    controller.dashboardModel.data.parkingLevel
                Layout.preferredWidth: 30
                Layout.preferredHeight: 30
                source: {
                    switch (parkingLevel) {
                    case 0x00: // PARKING_CLEAR: > 100 cm
                        return "../assets/reverse-far.png"

                    case 0x01: // PARKING_CAUTION: 61–100 cm
                        return "../assets/reverse-med.png"

                    case 0x02: // PARKING_WARNING: 31–60 cm
                    case 0x03: // PARKING_CRITICAL: 0–30 cm
                        return "../assets/reverse-close.png"

                    default:
                        return "../assets/reverse-off.png"
                    }
                }
                fillMode: Image.PreserveAspectFit
            }
            Image {
                id: iconDTC
                readonly property bool dtcActive:
                    controller.dashboardModel.data.dtcActive
                Layout.preferredWidth: 30
                Layout.preferredHeight: 30
                source: dtcActive
                        ? "../assets/mil-on.png"
                        : "../assets/mil-off.png"
                fillMode: Image.PreserveAspectFit
            }
            Image {
                id: iconTurnRight
                readonly property bool turnActive:
                    controller.dashboardModel.data.turnRightOn
                property bool blinkOn: false
                Layout.preferredWidth: 30
                Layout.preferredHeight: 30
                source: turnActive && blinkOn
                        ? "../assets/turn-right-on.png"
                        : "../assets/turn-right-off.png"
                fillMode: Image.PreserveAspectFit
                onTurnActiveChanged: {
                    blinkOn = turnActive
                }
                Timer {
                    interval: 500
                    running: iconTurnRight.turnActive
                    repeat: true
                    triggeredOnStart: false
                    onTriggered: {
                        iconTurnRight.blinkOn = !iconTurnRight.blinkOn
                    }
                }
            }
        }
    }
}
