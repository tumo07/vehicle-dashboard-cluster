import QtQuick 2.15

Rectangle {
    id: bottomBar
    anchors {
        left: parent.left
        right: parent.right
        bottom: parent.bottom
    }
    height: parent.height / 12
    color: "black"
    Image {
        id: carSettingIcon
        source: "../assets/carIcon.png"
        anchors {
            left: parent.left
            verticalCenter: parent.verticalCenter
            margins: 20
        }
        fillMode: Image.PreserveAspectFit
        height: parent.height * 0.85
    }
    HVACComponenet {
        id: driverHvacComponent
        anchors {
            top: parent.top
            bottom: parent.bottom
            left: carSettingIcon.right
            leftMargin: 50
            verticalCenter: parent.verticalCenter
        }
        height: parent.height * 0.85
        hvacController: controller.modelPassenger
    }
    InfoComponenet {
        id: centerBar
        anchors {
            left: driverHvacComponent.right
            right: passengerHvacComponent.left
            top: parent.top
            bottom: parent.bottom

            leftMargin: 300
            rightMargin: 30
        }
    }
    HVACComponenet {
        id: passengerHvacComponent
        anchors {
            top: parent.top
            bottom: parent.bottom
            right: parent.right
            rightMargin: 255
            verticalCenter: parent.verticalCenter
        }
        height: parent.height * 0.85
        hvacController: controller.model
    }
    VolumeControlComponent {
        id: volumeControl
        anchors {
            top: parent.top
            bottom: parent.bottom
            right: parent.right
            rightMargin: 20
            verticalCenter: parent.verticalCenter
        }
        height: parent.height * 0.85
        width: 100
    }
}
