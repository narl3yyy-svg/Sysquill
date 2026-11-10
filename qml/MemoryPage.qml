import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SysquillUi

ScrollView {
    id: page
    contentWidth: availableWidth
    clip: true

    readonly property var m: Sys.memory
    readonly property real total: m.total || 1
    readonly property real usedBytes: m.used || 0
    readonly property real freeBytes: m.free || 0
    readonly property real cacheBytes: Math.max(0, (m.available || 0) - freeBytes)

    ColumnLayout {
        x: 20
        width: page.availableWidth - 40
        spacing: 16

        Item { Layout.preferredHeight: 4 }

        PageHeader {
            title: qsTr("Memory")
            subtitle: qsTr("%1 installed").arg(Sys.formatBytes(page.m.total))
        }

        Card {
            Layout.fillWidth: true
            title: qsTr("Memory in use")
            subtitle: (page.usedBytes / page.total * 100).toFixed(0) + " %"
            GraphBox {
                Layout.fillWidth: true
                Layout.preferredHeight: 190
                values: Sys.memHistory
                color: Theme.memory
                maxValue: 100
                lineWidth: 2
            }

            // Composition bar: in use | cache (reclaimable) | free
            Label { text: qsTr("Composition"); color: Theme.muted; font.pixelSize: 12 }
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 22
                radius: 6
                color: Theme.track
                clip: true
                Row {
                    anchors.fill: parent
                    Rectangle { width: parent.width * page.usedBytes / page.total; height: parent.height; color: Theme.memory }
                    Rectangle { width: parent.width * page.cacheBytes / page.total; height: parent.height; color: Qt.rgba(Theme.memory.r, Theme.memory.g, Theme.memory.b, 0.4) }
                }
            }
            RowLayout {
                spacing: 20
                LegendDot { dotColor: Theme.memory; text: qsTr("In use %1").arg(Sys.formatBytes(page.usedBytes)) }
                LegendDot { dotColor: Qt.rgba(Theme.memory.r, Theme.memory.g, Theme.memory.b, 0.4); text: qsTr("Cache %1").arg(Sys.formatBytes(page.cacheBytes)) }
                LegendDot { dotColor: Theme.track; text: qsTr("Free %1").arg(Sys.formatBytes(page.freeBytes)) }
            }

            Flow {
                Layout.fillWidth: true
                Layout.topMargin: 6
                spacing: 32
                Stat { label: qsTr("In use"); value: Sys.formatBytes(page.usedBytes); accent: Theme.memory; valueSize: 22 }
                Stat { label: qsTr("Available"); value: Sys.formatBytes(page.m.available); valueSize: 22 }
                Stat { label: qsTr("Cached"); value: Sys.formatBytes(page.m.cached); valueSize: 22 }
                Stat { label: qsTr("Buffers"); value: Sys.formatBytes(page.m.buffers); valueSize: 22 }
                Stat { label: qsTr("Shared"); value: Sys.formatBytes(page.m.shmem); valueSize: 22 }
                Stat { label: qsTr("Waiting to be written"); value: Sys.formatBytes(page.m.dirty); valueSize: 22 }
                Stat { label: qsTr("Total"); value: Sys.formatBytes(page.m.total); valueSize: 22 }
            }
        }

        Card {
            Layout.fillWidth: true
            title: qsTr("Swap")
            subtitle: page.m.swapTotal > 0 ? (page.m.swapUsed / page.m.swapTotal * 100).toFixed(0) + " %" : qsTr("not configured")
            GraphBox {
                visible: page.m.swapTotal > 0
                Layout.fillWidth: true
                Layout.preferredHeight: 110
                values: Sys.swapHistory
                color: Theme.swap
                maxValue: 100
            }
            Flow {
                Layout.fillWidth: true
                spacing: 32
                Stat { label: qsTr("Used"); value: Sys.formatBytes(page.m.swapUsed); accent: Theme.swap; valueSize: 22 }
                Stat { label: qsTr("Total"); value: Sys.formatBytes(page.m.swapTotal); valueSize: 22 }
                Stat { label: qsTr("Also in RAM"); value: Sys.formatBytes(page.m.swapCached); valueSize: 22 }
            }
        }
    }
}
