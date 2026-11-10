import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SysquillUi

Rectangle {
    id: side
    property int currentIndex: 0
    signal selected(int index)
    color: Theme.sidebar

    readonly property var items: [
        { key: "overview",  title: qsTr("Overview"),  icon: "computer" },
        { key: "processes", title: qsTr("Processes"), icon: "view-list-details" },
        { key: "cpu",       title: qsTr("CPU"),       icon: "cpu" },
        { key: "memory",    title: qsTr("Memory"),    icon: "media-flash" },
        { key: "disks",     title: qsTr("Disks"),     icon: "drive-harddisk" },
        { key: "network",   title: qsTr("Network"),   icon: "network-wired" },
        { key: "gpu",       title: qsTr("GPU"),       icon: "video-display" },
        { key: "sensors",   title: qsTr("Sensors"),   icon: "temperature-normal" }
    ]

    function subtitle(key) {
        switch (key) {
        case "overview": return Sys.osName
        case "processes": return qsTr("%1 processes · %2 threads").arg(Sys.procCount).arg(Sys.threadCount)
        case "cpu": return Sys.cpuUsage.toFixed(0) + " %" + (Sys.cpuTemp > 0 ? "  ·  " + Sys.cpuTemp.toFixed(0) + " °C" : "")
        case "memory": return Sys.formatBytes(Sys.memory.used) + " / " + Sys.formatBytes(Sys.memory.total)
        case "disks": return "R " + Sys.formatRate(Sys.diskRead) + "  W " + Sys.formatRate(Sys.diskWrite)
        case "network": return "↓ " + Sys.formatRate(Sys.netRx) + "  ↑ " + Sys.formatRate(Sys.netTx)
        case "gpu": return Sys.gpu.present ? Sys.gpu.busy.toFixed(0) + " %" + (Sys.gpu.curMhz > 0 ? "  ·  " + Sys.formatMhz(Sys.gpu.curMhz) : "") : qsTr("Not detected")
        case "sensors": return qsTr("%1 temperature sensors").arg(Sys.sensorCount)
        }
        return ""
    }

    ListView {
        id: list
        anchors.fill: parent
        anchors.margins: 8
        spacing: 2
        clip: true
        interactive: contentHeight > height
        model: side.items

        delegate: ItemDelegate {
            id: del
            required property var modelData
            required property int index
            width: ListView.view.width
            height: 60
            highlighted: index === side.currentIndex
            onClicked: side.selected(index)

            background: Rectangle {
                radius: 8
                color: del.highlighted ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.22)
                     : del.hovered ? Theme.subtle : "transparent"
                border.width: del.highlighted ? 1 : 0
                border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.6)
            }

            contentItem: RowLayout {
                spacing: 10
                ThemeIcon { name: del.modelData.icon; size: 24 }
                ColumnLayout {
                    spacing: 1
                    Layout.fillWidth: true
                    Label {
                        text: del.modelData.title
                        font.weight: Font.DemiBold
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                    }
                    Label {
                        text: side.subtitle(del.modelData.key)
                        font.pixelSize: 11
                        color: Theme.muted
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                    }
                }
                SparkGraph {
                    readonly property string k: del.modelData.key
                    visible: k === "cpu" || k === "memory" || k === "disks" || k === "network" || (k === "gpu" && Sys.gpu.present)
                    Layout.preferredWidth: 46
                    Layout.preferredHeight: 26
                    grid: false
                    lineWidth: 1.2
                    capacity: 40
                    values: k === "cpu" ? Sys.cpuHistory : k === "memory" ? Sys.memHistory
                          : k === "disks" ? Sys.diskReadHistory : k === "network" ? Sys.netRxHistory
                          : k === "gpu" ? (Sys.gpu.history || []) : []
                    values2: k === "disks" ? Sys.diskWriteHistory : k === "network" ? Sys.netTxHistory : []
                    color: k === "cpu" ? Theme.cpu : k === "memory" ? Theme.memory : k === "disks" ? Theme.read
                         : k === "network" ? Theme.rx : Theme.gpu
                    color2: k === "disks" ? Theme.write : Theme.tx
                    maxValue: (k === "cpu" || k === "memory" || k === "gpu") ? 100 : 0
                    minAutoMax: 1024
                }
            }
        }
    }

    Label {
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottomMargin: 10
        text: "Sysquill " + Sys.version
        font.pixelSize: 11
        color: Theme.muted
        visible: list.contentHeight + 40 < side.height
    }
}
