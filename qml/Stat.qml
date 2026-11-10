import QtQuick
import SysquillUi
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    property string label
    property string value
    property color accent: Theme.text
    property int valueSize: 17
    spacing: 1

    Label {
        text: parent.label
        color: Theme.muted
        font.pixelSize: 12
    }
    Label {
        text: parent.value
        color: parent.accent
        font.pixelSize: parent.valueSize
        font.weight: Font.DemiBold
    }
}
