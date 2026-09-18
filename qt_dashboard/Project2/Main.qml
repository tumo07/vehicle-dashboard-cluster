import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "."
import "ui/BottomBar"
import "ui/RightScreen"
import "ui/LeftScreen"

Window {
    width: 1280
    height: 720    
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
