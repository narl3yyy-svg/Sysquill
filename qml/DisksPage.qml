import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SysquillUi

ScrollView {
    id: page
    contentWidth: availableWidth
    clip: true

    ColumnLayout {
        x: 20
        width: page.availableWidth - 40
        spacing: 16

        Item { Layout.preferredHeight: 4 }

        PageHeader {
            title: qsTr("Disks")
            subtitle: qsTr("%1 mounted file systems").arg(Sys.disks.length)
        }

        Card {
            Layout.fillWidth: true
            title: qsTr("All drives")
            subtitle: qsTr("combined physical read and write")
            headerRight: [
                LegendDot { dotColor: Theme.read; text: qsTr("Read %1").arg(Sys.formatRate(Sys.diskRead)) },
                LegendDot { dotColor: Theme.write; text: qsTr("Write %1").arg(Sys.formatRate(Sys.diskWrite)) }
            ]
            GraphBox {
                Layout.fillWidth: true
                Layout.preferredHeight: 130
                values: Sys.diskReadHistory
                values2: Sys.diskWriteHistory
                color: Theme.read
                color2: Theme.write
                minAutoMax: 1024 * 1024
                scaleFormatter: (m) => Sys.formatRate(m)
            }
        }

        Repeater {
            model: Sys.disks.length
            delegate: Card {
                id: dc
                required property int index
                readonly property var d: Sys.disks[index] || ({})
                property bool expanded: false
                Layout.fillWidth: true
                iconName: d.removable ? "drive-removable-media" : (d.mount === "/" ? "drive-harddisk-root" : "drive-harddisk")
                title: d.label ? d.label : d.mount
                subtitle: [d.mount, d.device, d.fstype, d.model].filter(x => x && x !== dc.title).join("   ·   ")

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    UsageBar {
                        Layout.fillWidth: true
                        implicitHeight: 12
                        value: dc.d.usedFraction || 0
                        barColor: Theme.usageColor(dc.d.usedFraction || 0)
                    }
                    Label {
                        text: ((dc.d.usedFraction || 0) * 100).toFixed(0) + " %"
                        font.weight: Font.DemiBold
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 16
                    Flow {
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignTop
                        spacing: 28
                        Stat { label: qsTr("Used"); value: Sys.formatBytes(dc.d.used) }
                        Stat { label: qsTr("Free"); value: Sys.formatBytes(dc.d.avail); accent: Theme.good }
                        Stat { label: qsTr("Size"); value: Sys.formatBytes(dc.d.total) }
                        Stat { label: qsTr("Read"); value: Sys.formatRate(dc.d.readRate); accent: Theme.read }
                        Stat { label: qsTr("Write"); value: Sys.formatRate(dc.d.writeRate); accent: Theme.write }
                        Stat { label: qsTr("Active time"); value: (dc.d.busy || 0).toFixed(0) + " %" }
                        Stat { label: qsTr("Type"); value: dc.d.rotational ? qsTr("Hard disk") : qsTr("SSD / flash") }
                    }
                    GraphBox {
                        Layout.preferredWidth: Math.min(360, page.availableWidth * 0.35)
                        Layout.preferredHeight: 96
                        values: dc.d.readHistory || []
                        values2: dc.d.writeHistory || []
                        color: Theme.read
                        color2: Theme.write
                        minAutoMax: 1024 * 1024
                        scaleFormatter: (m) => Sys.formatRate(m)
                    }
                }

                GridLayout {
                    columns: 2
                    columnSpacing: 14
                    rowSpacing: 2
                    Label { text: qsTr("Mount points"); color: Theme.muted }
                    Label { text: (dc.d.mountPoints || []).join(",  "); wrapMode: Text.WrapAnywhere; Layout.fillWidth: true }
                    Label { text: qsTr("Device"); color: Theme.muted }
                    Label { text: dc.d.device + (dc.d.parent && dc.d.parent !== dc.d.kname ? qsTr("  (on %1)").arg("/dev/" + dc.d.parent) : "") }
                    Label { text: qsTr("Label"); color: Theme.muted }
                    Label { text: dc.d.label || "–" }
                    Label { text: qsTr("UUID"); color: Theme.muted }
                    TextEdit {
                        text: dc.d.uuid || "–"
                        readOnly: true
                        selectByMouse: true
                        color: Theme.text
                        font: Qt.application.font
                    }
                    Label { text: qsTr("File system"); color: Theme.muted }
                    Label { text: dc.d.fstype || "–" }
                    Label { text: qsTr("Model"); color: Theme.muted; visible: !!dc.d.model }
                    Label { text: dc.d.model || ""; visible: !!dc.d.model }
                }

                ToolButton {
                    text: (dc.expanded ? "▾  " : "▸  ")
                          + qsTr("Processes using this drive (%1)").arg((dc.d.processes || []).length)
                    onClicked: dc.expanded = !dc.expanded
                    enabled: (dc.d.processes || []).length > 0
                }

                ColumnLayout {
                    visible: dc.expanded
                    Layout.fillWidth: true
                    spacing: 0
                    Label {
                        visible: !Sys.privileged
                        text: qsTr("Only your own processes are listed. Unlock all processes to include system services and other users' programs.")
                        color: Theme.muted
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                        Layout.bottomMargin: 6
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: qsTr("Process"); font.weight: Font.DemiBold; Layout.fillWidth: true }
                        Label { text: qsTr("PID"); font.weight: Font.DemiBold; Layout.preferredWidth: 70; horizontalAlignment: Text.AlignRight }
                        Label { text: qsTr("User"); font.weight: Font.DemiBold; Layout.preferredWidth: 110 }
                        Label { text: qsTr("Read (all drives)"); font.weight: Font.DemiBold; Layout.preferredWidth: 130; horizontalAlignment: Text.AlignRight }
                        Label { text: qsTr("Write (all drives)"); font.weight: Font.DemiBold; Layout.preferredWidth: 130; horizontalAlignment: Text.AlignRight }
                    }
                    Repeater {
                        model: dc.expanded ? (dc.d.processes || []).length : 0
                        delegate: RowLayout {
                            required property int index
                            readonly property var p: (dc.d.processes || [])[index] || ({})
                            Layout.fillWidth: true
                            Layout.preferredHeight: 24
                            Label { text: p.name || ""; elide: Text.ElideRight; Layout.fillWidth: true }
                            Label { text: p.pid || ""; color: Theme.muted; Layout.preferredWidth: 70; horizontalAlignment: Text.AlignRight }
                            Label { text: p.user || ""; color: Theme.muted; Layout.preferredWidth: 110; elide: Text.ElideRight }
                            Label { text: Sys.formatRate(p.read); Layout.preferredWidth: 130; horizontalAlignment: Text.AlignRight }
                            Label { text: Sys.formatRate(p.write); Layout.preferredWidth: 130; horizontalAlignment: Text.AlignRight }
                        }
                    }
                }
            }
        }
    }
}
