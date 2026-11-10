import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SysquillUi

ScrollView {
    id: page
    signal navigate(int index)
    contentWidth: availableWidth
    clip: true

    readonly property int cols: availableWidth > 1050 ? 3 : 2

    ColumnLayout {
        x: 20
        width: page.availableWidth - 40
        spacing: 16

        Item { Layout.preferredHeight: 4 }

        PageHeader {
            title: qsTr("Overview")
            subtitle: Sys.osName + "  ·  Linux " + Sys.kernel + "  ·  " + Sys.cpuModel
        }

        GridLayout {
            Layout.fillWidth: true
            columns: page.cols
            columnSpacing: 16
            rowSpacing: 16

            // CPU
            Card {
                title: qsTr("CPU"); iconName: "cpu"; clickable: true; onClicked: page.navigate(2)
                Layout.fillWidth: true; Layout.preferredWidth: 1; Layout.fillHeight: true
                RowLayout {
                    Label { text: Sys.cpuUsage.toFixed(0) + " %"; font.pixelSize: 30; font.bold: true; color: Theme.cpu }
                    Item { Layout.fillWidth: true }
                    ColumnLayout {
                        spacing: 0
                        Label { Layout.alignment: Qt.AlignRight; text: Sys.formatMhz(Sys.cpuMhz) }
                        Label {
                            Layout.alignment: Qt.AlignRight
                            text: Sys.cpuTemp > 0 ? Sys.cpuTemp.toFixed(0) + " °C" : ""
                            color: Theme.tempColor(Sys.cpuTemp)
                        }
                    }
                }
                GraphBox { Layout.fillWidth: true; Layout.preferredHeight: 90; values: Sys.cpuHistory; color: Theme.cpu; maxValue: 100 }
                Label { text: qsTr("Load %1").arg(Sys.loadAvg.map(v => v.toFixed(2)).join("  ")); color: Theme.muted }
            }

            // Memory
            Card {
                id: memCard; title: qsTr("Memory"); iconName: "media-flash"; clickable: true; onClicked: page.navigate(3)
                Layout.fillWidth: true; Layout.preferredWidth: 1; Layout.fillHeight: true
                readonly property real frac: Sys.memory.total > 0 ? Sys.memory.used / Sys.memory.total : 0
                RowLayout {
                    Label { text: (memCard.frac * 100).toFixed(0) + " %"; font.pixelSize: 30; font.bold: true; color: Theme.memory }
                    Item { Layout.fillWidth: true }
                    Label { text: Sys.formatBytes(Sys.memory.used) + " / " + Sys.formatBytes(Sys.memory.total) }
                }
                GraphBox { Layout.fillWidth: true; Layout.preferredHeight: 90; values: Sys.memHistory; color: Theme.memory; maxValue: 100 }
                Label {
                    text: Sys.memory.swapTotal > 0
                          ? qsTr("Swap %1 / %2").arg(Sys.formatBytes(Sys.memory.swapUsed)).arg(Sys.formatBytes(Sys.memory.swapTotal))
                          : qsTr("No swap")
                    color: Theme.muted
                }
            }

            // GPU
            Card {
                title: qsTr("GPU"); iconName: "video-display"; clickable: true; onClicked: page.navigate(6)
                subtitle: Sys.gpu.present ? Sys.gpu.name : ""
                Layout.fillWidth: true; Layout.preferredWidth: 1; Layout.fillHeight: true
                RowLayout {
                    Label {
                        text: Sys.gpu.present ? Sys.gpu.busy.toFixed(0) + " %" : qsTr("Not detected")
                        font.pixelSize: Sys.gpu.present ? 30 : 18; font.bold: true; color: Theme.gpu
                    }
                    Item { Layout.fillWidth: true }
                    Label { text: Sys.gpu.present ? Sys.formatMhz(Sys.gpu.curMhz) : "" }
                }
                GraphBox { Layout.fillWidth: true; Layout.preferredHeight: 90; values: Sys.gpu.history || []; color: Theme.gpu; maxValue: 100 }
                Label {
                    text: {
                        const e = (Sys.gpu.engines || []).find(x => x.name === "video" || x.name === "vcs")
                        return e ? qsTr("Video engine %1 %").arg(e.util.toFixed(0)) : ""
                    }
                    color: Theme.muted
                }
            }

            // Disks
            Card {
                title: qsTr("Drives"); iconName: "drive-harddisk"; clickable: true; onClicked: page.navigate(4)
                subtitle: "R " + Sys.formatRate(Sys.diskRead) + "   W " + Sys.formatRate(Sys.diskWrite)
                Layout.fillWidth: true; Layout.fillHeight: true; Layout.preferredWidth: 2; Layout.columnSpan: 2
                Repeater {
                    model: Sys.disks.length
                    delegate: RowLayout {
                        required property int index
                        readonly property var d: Sys.disks[index] || ({})
                        Layout.fillWidth: true
                        spacing: 12
                        Label {
                            text: d.label ? d.label : d.mount
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                            Layout.preferredWidth: 130
                        }
                        Label { text: d.mount; color: Theme.muted; elide: Text.ElideMiddle; Layout.preferredWidth: 110 }
                        UsageBar { Layout.fillWidth: true; value: d.usedFraction || 0; barColor: Theme.usageColor(d.usedFraction || 0) }
                        Label {
                            text: qsTr("%1 free of %2").arg(Sys.formatBytes(d.avail)).arg(Sys.formatBytes(d.total))
                            Layout.preferredWidth: 170
                            horizontalAlignment: Text.AlignRight
                        }
                    }
                }
            }

            // Network
            Card {
                title: qsTr("Network"); iconName: "network-wired"; clickable: true; onClicked: page.navigate(5)
                Layout.fillWidth: true; Layout.preferredWidth: 1; Layout.fillHeight: true
                RowLayout {
                    spacing: 18
                    Stat { label: qsTr("Download"); value: Sys.formatRate(Sys.netRx); accent: Theme.rx }
                    Stat { label: qsTr("Upload"); value: Sys.formatRate(Sys.netTx); accent: Theme.tx }
                }
                GraphBox {
                    Layout.fillWidth: true; Layout.preferredHeight: 70
                    values: Sys.netRxHistory; values2: Sys.netTxHistory
                    color: Theme.rx; color2: Theme.tx; minAutoMax: 1024
                    scaleFormatter: (m) => Sys.formatRate(m)
                }
            }

            // Top processes
            Card {
                title: qsTr("Busiest processes"); iconName: "view-list-details"; clickable: true; onClicked: page.navigate(1)
                Layout.fillWidth: true; Layout.preferredWidth: 1; Layout.fillHeight: true
                Repeater {
                    model: Sys.topProcesses.length
                    delegate: RowLayout {
                        required property int index
                        readonly property var p: Sys.topProcesses[index] || ({})
                        Layout.fillWidth: true
                        Label { text: p.name || ""; elide: Text.ElideRight; Layout.fillWidth: true }
                        Label { text: Sys.formatBytes(p.rss); color: Theme.muted }
                        Label {
                            text: (p.cpu || 0).toFixed(1) + " %"
                            Layout.preferredWidth: 56
                            horizontalAlignment: Text.AlignRight
                            font.weight: Font.DemiBold
                        }
                    }
                }
            }

            // System
            Card {
                title: qsTr("System"); iconName: "computer"
                Layout.fillWidth: true; Layout.preferredWidth: 1; Layout.fillHeight: true
                GridLayout {
                    columns: 2
                    columnSpacing: 16
                    rowSpacing: 4
                    Label { text: qsTr("Host"); color: Theme.muted }        Label { text: Sys.hostname }
                    Label { text: qsTr("System"); color: Theme.muted }      Label { text: Sys.osName; elide: Text.ElideRight; Layout.fillWidth: true }
                    Label { text: qsTr("Kernel"); color: Theme.muted }      Label { text: Sys.kernel }
                    Label { text: qsTr("Uptime"); color: Theme.muted }      Label { text: Sys.formatDuration(Sys.uptime) }
                    Label { text: qsTr("Processor"); color: Theme.muted }   Label { text: qsTr("%1 cores, %2 threads").arg(Sys.physicalCores).arg(Sys.logicalCpus) }
                    Label { text: qsTr("Processes"); color: Theme.muted }   Label { text: qsTr("%1 (%2 threads)").arg(Sys.procCount).arg(Sys.threadCount) }
                }
            }
        }
    }
}
