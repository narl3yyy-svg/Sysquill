import QtQuick
import SysquillUi

Rectangle {
    id: bar
    property real value: 0          // 0..1
    property color barColor: Theme.accent
    implicitHeight: 8
    implicitWidth: 120
    radius: height / 2
    color: Theme.track

    Rectangle {
        width: Math.max(bar.value > 0 ? bar.height : 0, Math.min(1, Math.max(0, bar.value)) * bar.width)
        height: parent.height
        radius: parent.radius
        color: bar.barColor
        Behavior on width { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }
    }
}
