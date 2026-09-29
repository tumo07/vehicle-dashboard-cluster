import QtQuick 2.15

Rectangle {
    id: leftScreen

    anchors {
        left: parent.left
        right: rightScreen.left
        bottom: bottomBar.top
        top: parent.top
    }

    color: "#10171e"

    readonly property var vehicleData:
        controller.dashboardModel.data

    Item {
        id: dashboardContent

        width: 318
        height: 493

        anchors.centerIn: parent

        scale: Math.min(
            leftScreen.width / width,
            leftScreen.height / height
        )

        SpeedComponent {
            anchors.horizontalCenter: parent.horizontalCenter

            y: 12

            speedKmh:
                Number(leftScreen.vehicleData.speedKmh) || 0
        }

        Image {
            id: carRender

            anchors.horizontalCenter: parent.horizontalCenter

            y: 88
            width: 278
            height: 120

            source: "../assets/carRender.png"
            fillMode: Image.PreserveAspectFit
        }

        GearPosition {
            id: gearDisplay

            anchors.horizontalCenter: parent.horizontalCenter

            y: 222

            rawGear: leftScreen.vehicleData.gear === undefined
                     ? GearPosition.Unknown
                     : leftScreen.vehicleData.gear
        }

        VehicleStatus {
            id: vehicleStatus

            anchors {
                top: gearDisplay.bottom
                topMargin: 14
                horizontalCenter: parent.horizontalCenter
            }

            width: gearDisplay.width

            fuelPercent:
                Number(leftScreen.vehicleData.fuelPercent) || 0

            coolantTempC:
                Number(leftScreen.vehicleData.coolantTempC) || 0

            batteryVoltage:
                Number(leftScreen.vehicleData.batteryVoltage) || 0
        }
    }
}
