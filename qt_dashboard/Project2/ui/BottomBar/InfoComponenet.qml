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
            leftMargin: 30
            rightMargin: 30
        }
        RowLayout {
            anchors.centerIn: parent
            spacing: 25
            Image {
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
                id: iconTurnLeft
                Layout.preferredWidth: 30
                Layout.preferredHeight: 30
                fillMode: Image.PreserveAspectFit
            }

            // Simulate only
            // Image {
            //     id: iconTurnLeft

            //     Layout.preferredWidth: 30
            //     Layout.preferredHeight: 30
            //     fillMode: Image.PreserveAspectFit

            //     property bool testBlink: true
            //     readonly property bool turnActive:
            //         testBlink || controller.dashboardModel.data.turnLeftOn

            //     property bool blinkOn: false

            //     source: turnActive && blinkOn
            //             ? "../assets/turn-left-on.png"
            //             : "../assets/turn-left-off.png"

            //     onTurnActiveChanged: blinkOn = turnActive
            //     Component.onCompleted: blinkOn = turnActive

            //     Timer {
            //         interval: 500
            //         running: iconTurnLeft.turnActive
            //         repeat: true

            //         onTriggered: iconTurnLeft.blinkOn = !iconTurnLeft.blinkOn
            //     }
            // }

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
                source: "../assets/lowbeam-off.png"
                fillMode: Image.PreserveAspectFit
            }

            Image {
                id: iconHighBeam
                Layout.preferredWidth: 30
                Layout.preferredHeight: 30
                source: "../assets/highbeam-off.png"
                fillMode: Image.PreserveAspectFit
            }

            Image {
                id: iconABS
                Layout.preferredWidth: 30
                Layout.preferredHeight: 30
                source: "../assets/abs-off.png"
                fillMode: Image.PreserveAspectFit
            }

            Image {
                id: iconTurnRight
                Layout.preferredWidth: 30
                Layout.preferredHeight: 30
                source: "../assets/turn-right-off.png"
                fillMode: Image.PreserveAspectFit
            }
        }
    }
}
