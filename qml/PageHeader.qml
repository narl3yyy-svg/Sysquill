import QtQuick
import SysquillUi
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    property string title
    property string subtitle
    spacing: 2
    Layout.fillWidth: true

    Label {
        text: parent.title
        font.pixelSize: 24
        font.bold: true
    }
    Label {
        visible: text !== ""
        text: parent.subtitle
        color: Theme.muted
        elide: Text.ElideRight
        Layout.fillWidth: true
    }
}
