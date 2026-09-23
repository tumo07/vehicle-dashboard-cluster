import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "."
import "ui/BottomBar"
import "ui/RightScreen"
import "ui/LeftScreen"

Window {
    width: 960
    height: 540
    minimumWidth: 960
    maximumWidth: 960
    minimumHeight: 540
    maximumHeight: 540
    visible: true
    title: qsTr("Car cluster")

    LeftScreen {
        id: leftScreen
    }

    RightScreen {
        id: rightScreen
    }

    BottomBar {
        id: bottomBar
    }
}
