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
            title: qsTr("CPU")
            subtitle: Sys.cpuModel + "  ·  " + qsTr("%1 cores, %2 threads").arg(Sys.physicalCores).arg(Sys.logicalCpus)
        }

        Card {
            Layout.fillWidth: true
            title: qsTr("Utilization")
            subtitle: qsTr("last 2 minutes")
            GraphBox {
                Layout.fillWidth: true
                Layout.preferredHeight: 210
                values: Sys.cpuHistory
                color: Theme.cpu
                maxValue: 100
                lineWidth: 2
            }
            Flow {
                Layout.fillWidth: true
                spacing: 32
                Stat { label: qsTr("Utilization"); value: Sys.cpuUsage.toFixed(1) + " %"; accent: Theme.cpu; valueSize: 22 }
                Stat { label: qsTr("Speed (average)"); value: Sys.formatMhz(Sys.cpuMhz); valueSize: 22 }
                Stat { label: qsTr("Max speed"); value: Sys.formatMhz(Sys.cpuMaxMhz); valueSize: 22 }
                Stat {
                    label: qsTr("Temperature")
                    value: Sys.cpuTemp > 0 ? Sys.cpuTemp.toFixed(0) + " °C" : "–"
                    accent: Sys.cpuTemp > 0 ? Theme.tempColor(Sys.cpuTemp) : Theme.text
                    valueSize: 22
                }
                Stat {
                    label: qsTr("Package power")
                    value: Sys.cpuPower >= 0 ? Sys.cpuPower.toFixed(1) + " W" : qsTr("unlock to view")
                    valueSize: 22
                }
                Stat { label: qsTr("Load average (1, 5, 15 min)"); value: Sys.loadAvg.map(v => v.toFixed(2)).join("   "); valueSize: 22 }
                Stat { label: qsTr("Processes"); value: Sys.procCount; valueSize: 22 }
                Stat { label: qsTr("Threads"); value: Sys.threadCount; valueSize: 22 }
                Stat { label: qsTr("Uptime"); value: Sys.formatDuration(Sys.uptime); valueSize: 22 }
            }
        }

        Card {
            Layout.fillWidth: true
            title: qsTr("Per thread")
            subtitle: qsTr("usage and current clock of each logical processor")
            GridLayout {
                Layout.fillWidth: true
                columns: Math.max(2, Math.floor((page.availableWidth - 30) / 210))
                columnSpacing: 10
                rowSpacing: 10
                Repeater {
                    model: Sys.cores.length
                    delegate: Rectangle {
                        id: coreBox
                        required property int index
                        readonly property var c: Sys.cores[index] || ({ usage: 0, mhz: -1, history: [] })
                        Layout.fillWidth: true
                        Layout.preferredHeight: 96
                        radius: 8
                        color: Theme.subtle
                        border.width: 1
                        border.color: Theme.border
                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 8
                            spacing: 4
                            RowLayout {
                                Label { text: qsTr("CPU %1").arg(coreBox.index); font.weight: Font.DemiBold }
                                Item { Layout.fillWidth: true }
                                Label { text: coreBox.c.usage.toFixed(0) + " %"; color: Theme.cpu; font.weight: Font.DemiBold }
                            }
                            SparkGraph {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                values: coreBox.c.history
                                color: Theme.cpu
                                maxValue: 100
                                capacity: 60
                                gridColor: Theme.grid
                                lineWidth: 1.2
                            }
                            Label { text: Sys.formatMhz(coreBox.c.mhz); color: Theme.muted; font.pixelSize: 11 }
                        }
                    }
                }
            }
        }
    }
}
