import QtQuick
import SysquillUi
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: card
    property string title
    property string subtitle
    property string iconName
    property bool clickable: false
    default property alias content: body.data
    property alias headerRight: headerRightRow.data
    signal clicked()

    color: Theme.card
    radius: 10
    border.width: 1
    border.color: clickable && hover.hovered ? Theme.accent : Theme.border
    implicitWidth: 320
    implicitHeight: outer.implicitHeight + 28

    HoverHandler { id: hover; enabled: card.clickable; cursorShape: Qt.PointingHandCursor }
    TapHandler { enabled: card.clickable; onTapped: card.clicked() }

    ColumnLayout {
        id: outer
        anchors.fill: parent
        anchors.margins: 14
        spacing: 10

        RowLayout {
            visible: card.title !== ""
            Layout.fillWidth: true
            spacing: 8
            ThemeIcon { visible: card.iconName !== ""; name: card.iconName; size: 20 }
            Label {
                text: card.title
                font.bold: true
                font.pixelSize: 15
            }
            Label {
                text: card.subtitle
                color: Theme.muted
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
            Row { id: headerRightRow; spacing: 8 }
        }

        ColumnLayout {
            id: body
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 8
        }
    }
}
