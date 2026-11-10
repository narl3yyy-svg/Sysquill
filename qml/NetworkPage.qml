import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SysquillUi

ScrollView {
    id: page
    contentWidth: availableWidth
    clip: true
    property bool showVirtual: false

    ColumnLayout {
        x: 20
        width: page.availableWidth - 40
        spacing: 16

        Item { Layout.preferredHeight: 4 }

        RowLayout {
            Layout.fillWidth: true
            PageHeader {
                title: qsTr("Network")
                subtitle: qsTr("Download %1   ·   Upload %2").arg(Sys.formatRate(Sys.netRx)).arg(Sys.formatRate(Sys.netTx))
            }
            CheckBox {
                text: qsTr("Show virtual interfaces (Docker, bridges, VPN)")
                checked: page.showVirtual
                onToggled: page.showVirtual = checked
            }
        }

        Repeater {
            model: Sys.nets.length
            delegate: Card {
                id: nc
                required property int index
                readonly property var n: Sys.nets[index] || ({})
                visible: n.physical || page.showVirtual
                Layout.fillWidth: true
                iconName: n.wireless ? "network-wireless" : "network-wired"
                title: n.name || ""
                subtitle: [n.physical ? (n.wireless ? qsTr("Wi-Fi") : qsTr("Ethernet")) : qsTr("virtual"),
                           n.state, n.speed > 0 ? (n.speed >= 1000 ? (n.speed / 1000) + " Gbit/s" : n.speed + " Mbit/s") : ""]
                          .filter(x => x).join("   ·   ")
                headerRight: [
                    LegendDot { dotColor: Theme.rx; text: "↓ " + Sys.formatRate(nc.n.rxRate) },
                    LegendDot { dotColor: Theme.tx; text: "↑ " + Sys.formatRate(nc.n.txRate) }
                ]
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 16
                    GraphBox {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 120
                        values: nc.n.rxHistory || []
                        values2: nc.n.txHistory || []
                        color: Theme.rx
                        color2: Theme.tx
                        minAutoMax: 1024
                        scaleFormatter: (m) => Sys.formatRate(m)
                    }
                    GridLayout {
                        Layout.alignment: Qt.AlignTop
                        Layout.preferredWidth: 330
                        columns: 2
                        columnSpacing: 12
                        rowSpacing: 3
                        Label { text: qsTr("Received"); color: Theme.muted }
                        Label { text: Sys.formatBytes(nc.n.rxTotal) }
                        Label { text: qsTr("Sent"); color: Theme.muted }
                        Label { text: Sys.formatBytes(nc.n.txTotal) }
                        Label { text: qsTr("MAC"); color: Theme.muted }
                        Label { text: nc.n.mac || "–" }
                        Label { text: qsTr("Addresses"); color: Theme.muted; Layout.alignment: Qt.AlignTop }
                        TextEdit {
                            text: (nc.n.addresses || []).length ? nc.n.addresses.join("\n") : "–"
                            readOnly: true
                            selectByMouse: true
                            color: Theme.text
                            font: Qt.application.font
                            Layout.fillWidth: true
                            wrapMode: TextEdit.WrapAnywhere
                        }
                    }
                }
            }
        }
    }
}
