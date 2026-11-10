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
            title: qsTr("Sensors")
            subtitle: qsTr("%1 temperature readings").arg(Sys.sensorCount)
        }

        Card {
            visible: Sys.sensorCount === 0
            Layout.fillWidth: true
            title: qsTr("No temperature sensors found")
            Label {
                text: qsTr("Install lm-sensors and run “sudo sensors-detect” to load the right drivers.")
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
        }

        GridLayout {
            Layout.fillWidth: true
            columns: page.availableWidth > 900 ? 2 : 1
            columnSpacing: 16
            rowSpacing: 16

            Repeater {
                model: Sys.sensorGroups.length
                delegate: Card {
                    id: grpCard
                    required property int index
                    readonly property var grp: Sys.sensorGroups[index] || ({ sensors: [] })
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    Layout.alignment: Qt.AlignTop
                    title: grp.title || ""
                    subtitle: [grp.device, grp.chip !== grp.title ? grp.chip : ""].filter(x => x).join("  ·  ")
                    Repeater {
                        model: (grpCard.grp.sensors || []).length
                        delegate: RowLayout {
                            required property int index
                            readonly property var t: (grpCard.grp.sensors || [])[index] || ({ celsius: 0, crit: 0 })
                            readonly property real limit: t.crit > 0 ? t.crit : 100
                            Layout.fillWidth: true
                            spacing: 12
                            Label { text: t.label || ""; Layout.preferredWidth: 150; elide: Text.ElideRight }
                            UsageBar {
                                Layout.fillWidth: true
                                value: Math.max(0, t.celsius) / limit
                                barColor: Theme.tempColor(t.celsius)
                            }
                            Label {
                                text: t.celsius.toFixed(0) + " °C"
                                Layout.preferredWidth: 56
                                horizontalAlignment: Text.AlignRight
                                font.weight: Font.DemiBold
                                color: Theme.tempColor(t.celsius)
                            }
                        }
                    }
                }
            }
        }

        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: Theme.muted
            text: qsTr("Tip: hard-drive temperatures appear here after loading the kernel's drivetemp module (sudo modprobe drivetemp).")
        }
    }
}
