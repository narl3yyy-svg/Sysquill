import QtQuick
import QtQuick.Controls
import SysquillUi

// A framed live graph with an optional scale label in the corner.
Item {
    id: box
    property alias values: graph.values
    property alias values2: graph.values2
    property alias color: graph.color
    property alias color2: graph.color2
    property alias maxValue: graph.maxValue
    property alias minAutoMax: graph.minAutoMax
    property alias lineWidth: graph.lineWidth
    property alias grid: graph.grid
    property bool showScale: true
    property var scaleFormatter: null   // function(max) -> string
    readonly property real effectiveMax: graph.effectiveMax

    implicitHeight: 120
    implicitWidth: 200

    Rectangle {
        id: frame
        anchors.fill: parent
        radius: 8
        color: Theme.subtle
        border.width: 1
        border.color: Theme.border
        clip: true

        SparkGraph {
            id: graph
            anchors.fill: parent
            anchors.margins: 1
            gridColor: Theme.grid
        }
        Label {
            visible: box.showScale && box.scaleFormatter !== null
            text: box.scaleFormatter ? box.scaleFormatter(graph.effectiveMax) : ""
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.margins: 6
            font.pixelSize: 11
            color: Theme.muted
        }
    }
}
