import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SysquillUi

ApplicationWindow {
    id: win
    width: 1280
    height: 820
    minimumWidth: 960
    minimumHeight: 620
    visible: true
    title: "Sysquill — " + Sys.hostname
    color: Theme.window

    property int currentPage: 0
    property string startPage: ""
    readonly property var pageKeys: ["overview", "processes", "cpu", "memory", "disks", "network", "gpu", "sensors"]
    onStartPageChanged: {
        const i = pageKeys.indexOf(startPage)
        if (i >= 0) currentPage = i
    }

    function showMessage(text) {
        toastLabel.text = text
        toast.opacity = 1
        toastTimer.restart()
    }

    Connections {
        target: Sys
        function onKillFinished(pid, ok, message) {
            if (!ok) win.showMessage(qsTr("Could not end process %1: %2").arg(pid).arg(message))
        }
        function onHelperChanged() {
            if (Sys.helperState === "off" && Sys.helperMessage !== "") win.showMessage(Sys.helperMessage)
        }
    }

    header: ToolBar {
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 8
            spacing: 10

            Image {
                source: "qrc:/qt/qml/SysquillUi/data/io.github.narl3yyy_svg.Sysquill.svg"
                sourceSize.width: 24
                sourceSize.height: 24
                Layout.preferredWidth: 24
                Layout.preferredHeight: 24
            }
            Label {
                text: "Sysquill"
                font.bold: true
                font.pixelSize: 16
            }
            Label {
                text: Sys.hostname + "   ·   " + qsTr("up %1").arg(Sys.formatDuration(Sys.uptime))
                color: Theme.muted
            }
            Item { Layout.fillWidth: true }

            ToolButton {
                text: Sys.privileged ? qsTr("Full access")
                      : Sys.helperState === "starting" ? qsTr("Waiting for password…")
                      : qsTr("Unlock all processes")
                icon.name: Sys.privileged ? "security-high" : "object-unlocked"
                enabled: Sys.helperState === "off"
                onClicked: Sys.unlock()
                ToolTip.visible: hovered
                ToolTip.delay: 600
                ToolTip.text: Sys.privileged
                    ? qsTr("Disk, GPU and power figures include every user's processes.")
                    : qsTr("Asks for your password once, then shows disk and GPU activity of all users' processes and lets you end them.")
            }
            ToolSeparator {}
            Label { text: qsTr("Update every"); color: Theme.muted }
            ComboBox {
                id: intervalBox
                readonly property var values: [500, 1000, 2000, 5000]
                model: ["0.5 s", "1 s", "2 s", "5 s"]
                currentIndex: Math.max(0, values.indexOf(Sys.interval))
                onActivated: (i) => Sys.interval = values[i]
                Layout.preferredWidth: 90
            }
            ToolButton {
                icon.name: Sys.paused ? "media-playback-start" : "media-playback-pause"
                text: Sys.paused ? qsTr("Resume") : qsTr("Pause")
                onClicked: Sys.paused = !Sys.paused
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Sidebar {
            Layout.preferredWidth: 236
            Layout.fillHeight: true
            currentIndex: win.currentPage
            onSelected: (i) => win.currentPage = i
        }
        Rectangle {
            Layout.preferredWidth: 1
            Layout.fillHeight: true
            color: Theme.border
        }
        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: win.currentPage

            OverviewPage { onNavigate: (i) => win.currentPage = i }
            ProcessesPage { active: win.currentPage === 1 }
            CpuPage {}
            MemoryPage {}
            DisksPage {}
            NetworkPage {}
            GpuPage {}
            SensorsPage {}
        }
    }

    // Small notification at the bottom of the window
    Rectangle {
        id: toast
        opacity: 0
        visible: opacity > 0
        z: 100
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 24
        width: Math.min(parent.width - 80, toastLabel.implicitWidth + 32)
        height: toastLabel.implicitHeight + 20
        radius: 8
        color: Theme.dark ? "#31363b" : "#232629"
        Behavior on opacity { NumberAnimation { duration: 200 } }
        Label {
            id: toastLabel
            anchors.centerIn: parent
            width: parent.width - 32
            color: "#fcfcfc"
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
        }
        Timer { id: toastTimer; interval: 5000; onTriggered: toast.opacity = 0 }
    }
}
