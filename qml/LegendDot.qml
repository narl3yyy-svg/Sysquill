import QtQuick
import SysquillUi
import QtQuick.Controls

Row {
    property color dotColor: Theme.accent
    property string text
    spacing: 6
    Rectangle {
        width: 10; height: 10; radius: 5
        color: parent.dotColor
        anchors.verticalCenter: parent.verticalCenter
    }
    Label {
        text: parent.text
        anchors.verticalCenter: parent.verticalCenter
    }
}
