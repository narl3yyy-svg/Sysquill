import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SysquillUi

Item {
    id: page
    property bool active: false
    property int selectedPid: -1
    readonly property var proc: Sys.processes

    // Column widths: everything fixed except the command line, which takes the rest
    readonly property var fixedWidths: [210, 70, 104, 76, 92, 100, 100, 66, 70, 92]
    function colWidth(c) {
        if (c < fixedWidths.length) return fixedWidths[c]
        let sum = 0
        for (let i = 0; i < fixedWidths.length; ++i) sum += fixedWidths[i]
        return Math.max(180, table.width - sum)
    }

    function askEnd(force) {
        if (selectedPid <= 0) return
        confirm.force = force
        confirm.pid = selectedPid
        confirm.name = proc.nameOfPid(selectedPid)
        confirm.user = proc.userOfPid(selectedPid)
        confirm.open()
    }

    Shortcut {
        sequence: StandardKey.Delete
        enabled: page.active && page.selectedPid > 0
        onActivated: page.askEnd(false)
    }
    Shortcut {
        sequences: [StandardKey.Find]
        enabled: page.active
        onActivated: search.forceActiveFocus()
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            PageHeader {
                title: qsTr("Processes")
                subtitle: qsTr("%1 shown of %2  ·  CPU is a share of the whole machine").arg(page.proc.count).arg(Sys.procCount)
            }
            TextField {
                id: search
                placeholderText: qsTr("Search name, command, user or PID…")
                Layout.preferredWidth: 320
                onTextChanged: page.proc.filterText = text
                Keys.onEscapePressed: text = ""
            }
            CheckBox {
                text: qsTr("Kernel threads")
                checked: page.proc.showKernel
                onToggled: page.proc.showKernel = checked
            }
            Button {
                text: qsTr("End process")
                icon.name: "process-stop"
                enabled: page.selectedPid > 0
                onClicked: page.askEnd(false)
            }
            Button {
                text: qsTr("Force kill")
                icon.name: "edit-delete"
                enabled: page.selectedPid > 0
                onClicked: page.askEnd(true)
            }
        }

        Label {
            visible: !Sys.privileged
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: Theme.muted
            text: qsTr("Disk and GPU columns show “–” for processes owned by other users. Click “Unlock all processes” at the top to see everything.")
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: Theme.card
            radius: 8
            border.width: 1
            border.color: Theme.border
            clip: true

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 1
                spacing: 0

                // Header row (click to sort)
                Item {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 34
                    clip: true
                    Rectangle { anchors.fill: parent; color: Theme.subtle }
                    Row {
                        x: -table.contentX
                        height: parent.height
                        Repeater {
                            model: page.proc.columns
                            delegate: Rectangle {
                                id: hdr
                                required property int index
                                width: page.colWidth(index)
                                height: parent.height
                                color: hdrMouse.containsMouse ? Theme.subtle : "transparent"
                                readonly property bool sorted: page.proc.sortColumnIndex === index
                                Label {
                                    anchors.fill: parent
                                    anchors.leftMargin: 10
                                    anchors.rightMargin: 10
                                    verticalAlignment: Text.AlignVCenter
                                    horizontalAlignment: page.proc.columnNumeric(hdr.index) ? Text.AlignRight : Text.AlignLeft
                                    text: page.proc.columnTitle(hdr.index) + (hdr.sorted ? (page.proc.descending ? "  ▾" : "  ▴") : "")
                                    font.weight: Font.DemiBold
                                    color: hdr.sorted ? Theme.accent : Theme.text
                                    elide: Text.ElideRight
                                }
                                Rectangle { anchors.right: parent.right; width: 1; height: parent.height; color: Theme.border }
                                MouseArea {
                                    id: hdrMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: page.proc.sortBy(hdr.index)
                                }
                            }
                        }
                    }
                    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.border }
                }

                TableView {
                    id: table
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: page.proc
                    boundsBehavior: Flickable.StopAtBounds
                    reuseItems: true
                    columnWidthProvider: (c) => page.colWidth(c)
                    onWidthChanged: forceLayout()
                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                    ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AsNeeded }

                    delegate: Rectangle {
                        id: cell
                        required property int row
                        required property int column
                        required property string display
                        required property int pid
                        required property real heat
                        required property bool numeric
                        implicitHeight: 28
                        readonly property bool isSel: pid === page.selectedPid
                        color: isSel ? Theme.accent
                             : heat > 0.01 ? Qt.rgba(0.96, 0.65, 0.14, 0.10 + heat * 0.35)
                             : (row % 2 ? Theme.subtle : "transparent")
                        Label {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10
                            verticalAlignment: Text.AlignVCenter
                            horizontalAlignment: cell.numeric ? Text.AlignRight : Text.AlignLeft
                            text: cell.display
                            elide: cell.column === 10 ? Text.ElideMiddle : Text.ElideRight
                            color: cell.isSel ? Theme.accentText : (cell.column === 10 ? Theme.muted : Theme.text)
                            font.weight: cell.column === 0 ? Font.DemiBold : Font.Normal
                        }
                        MouseArea {
                            anchors.fill: parent
                            acceptedButtons: Qt.LeftButton | Qt.RightButton
                            onClicked: (mouse) => {
                                page.selectedPid = cell.pid
                                if (mouse.button === Qt.RightButton) menu.popup()
                            }
                        }
                    }
                }
            }
        }
    }

    Menu {
        id: menu
        MenuItem { text: qsTr("End process"); icon.name: "process-stop"; onTriggered: page.askEnd(false) }
        MenuItem { text: qsTr("Force kill"); icon.name: "edit-delete"; onTriggered: page.askEnd(true) }
    }

    Dialog {
        id: confirm
        property bool force: false
        property int pid: -1
        property string name
        property string user
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        width: 460
        title: force ? qsTr("Force kill “%1”?").arg(name) : qsTr("End “%1”?").arg(name)
        standardButtons: Dialog.Yes | Dialog.Cancel
        Label {
            width: parent.width
            wrapMode: Text.WordWrap
            text: (confirm.force
                   ? qsTr("The process (PID %1, user %2) will be stopped immediately. Unsaved work in it will be lost.")
                   : qsTr("The process (PID %1, user %2) will be asked to close."))
                  .arg(confirm.pid).arg(confirm.user)
                  + (confirm.user !== "" && !Sys.privileged ? "\n\n" + qsTr("If it belongs to another user you will be asked for your password.") : "")
        }
        onAccepted: confirm.force ? Sys.killProcess(confirm.pid) : Sys.endProcess(confirm.pid)
    }
}
