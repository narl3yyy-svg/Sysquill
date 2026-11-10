import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SysquillUi

ScrollView {
    id: page
    contentWidth: availableWidth
    clip: true
    readonly property var g: Sys.gpu

    ColumnLayout {
        x: 20
        width: page.availableWidth - 40
        spacing: 16

        Item { Layout.preferredHeight: 4 }

        PageHeader {
            title: qsTr("GPU")
            subtitle: page.g.present ? page.g.name + "   ·   " + qsTr("driver %1").arg(page.g.driver) : qsTr("No graphics adapter found")
        }

        Card {
            visible: !!page.g.present
            Layout.fillWidth: true
            title: qsTr("Busiest engine")
            subtitle: qsTr("the highest of the engines below")
            GraphBox {
                Layout.fillWidth: true
                Layout.preferredHeight: 170
                values: page.g.history || []
                color: Theme.gpu
                maxValue: 100
                lineWidth: 2
            }
            Flow {
                Layout.fillWidth: true
                spacing: 32
                Stat { label: qsTr("Busy"); value: (page.g.busy || 0).toFixed(0) + " %"; accent: Theme.gpu; valueSize: 22 }
                Stat { label: qsTr("Clock"); value: Sys.formatMhz(page.g.curMhz); valueSize: 22 }
                Stat { label: qsTr("Max clock"); value: Sys.formatMhz(page.g.maxMhz); valueSize: 22 }
                Stat {
                    label: qsTr("Power (uncore)")
                    value: page.g.powerW >= 0 ? page.g.powerW.toFixed(1) + " W" : qsTr("unlock to view")
                    valueSize: 22
                }
                Stat { label: qsTr("PCI slot"); value: page.g.slot || "–"; valueSize: 22 }
            }
        }

        Card {
            visible: !!page.g.present
            Layout.fillWidth: true
            title: qsTr("Engines")
            subtitle: qsTr("Load per hardware engine: 3D, video decode/encode, video enhance and copy")
            Repeater {
                model: (page.g.engines || []).length
                delegate: RowLayout {
                    required property int index
                    readonly property var e: (page.g.engines || [])[index] || ({ util: 0, history: [] })
                    Layout.fillWidth: true
                    spacing: 14
                    Label { text: e.label || e.name || ""; Layout.preferredWidth: 190; font.weight: Font.DemiBold }
                    UsageBar { Layout.fillWidth: true; value: (e.util || 0) / 100; barColor: Theme.gpu; implicitHeight: 10 }
                    Label { text: (e.util || 0).toFixed(0) + " %"; Layout.preferredWidth: 48; horizontalAlignment: Text.AlignRight }
                    SparkGraph {
                        Layout.preferredWidth: 220
                        Layout.preferredHeight: 34
                        values: e.history || []
                        color: Theme.gpu
                        maxValue: 100
                        capacity: 60
                        gridColor: Theme.grid
                        lineWidth: 1.2
                    }
                }
            }
            Label {
                visible: (page.g.engines || []).length === 0
                text: qsTr("No engine data yet. It appears once a program uses the GPU.")
                color: Theme.muted
            }
        }

        Card {
            visible: !!page.g.present
            Layout.fillWidth: true
            title: qsTr("Programs using the GPU")
            subtitle: (page.g.processes || []).length + ""
            Label {
                visible: !Sys.privileged
                text: qsTr("Only your own programs are shown. Unlock all processes to include system services and other users' programs.")
                color: Theme.muted
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            Repeater {
                model: (page.g.processes || []).length
                delegate: RowLayout {
                    required property int index
                    readonly property var p: (page.g.processes || [])[index] || ({})
                    Layout.fillWidth: true
                    Label { text: p.name || ""; Layout.fillWidth: true; elide: Text.ElideRight }
                    Label { text: p.pid || ""; color: Theme.muted; Layout.preferredWidth: 70; horizontalAlignment: Text.AlignRight }
                    Label { text: p.user || ""; color: Theme.muted; Layout.preferredWidth: 110 }
                    Label { text: (p.gpu || 0).toFixed(1) + " %"; Layout.preferredWidth: 70; horizontalAlignment: Text.AlignRight; font.weight: Font.DemiBold }
                }
            }
        }

        Card {
            visible: !page.g.present
            Layout.fillWidth: true
            title: qsTr("No GPU found")
            Label {
                text: qsTr("Sysquill looks for graphics cards in /sys/class/drm. Intel, AMD and other kernel-driver GPUs are supported.")
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
        }
    }
}
