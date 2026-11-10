pragma Singleton
import QtQuick

// Colours follow the active Plasma colour scheme (light or dark) through
// SystemPalette; the series colours are fixed so graphs stay recognisable.
QtObject {
    readonly property SystemPalette pal: SystemPalette { colorGroup: SystemPalette.Active }

    readonly property bool dark: pal.window.hslLightness < 0.5
    readonly property color window: pal.window
    readonly property color text: pal.windowText
    readonly property color card: dark ? Qt.lighter(pal.window, 1.25) : Qt.lighter(pal.base, 1.0)
    readonly property color sidebar: dark ? Qt.darker(pal.window, 1.12) : Qt.darker(pal.window, 1.03)
    readonly property color border: Qt.rgba(pal.windowText.r, pal.windowText.g, pal.windowText.b, 0.12)
    readonly property color subtle: Qt.rgba(pal.windowText.r, pal.windowText.g, pal.windowText.b, 0.05)
    readonly property color track: Qt.rgba(pal.windowText.r, pal.windowText.g, pal.windowText.b, 0.10)
    readonly property color grid: Qt.rgba(pal.windowText.r, pal.windowText.g, pal.windowText.b, 0.08)
    readonly property color muted: Qt.rgba(pal.windowText.r, pal.windowText.g, pal.windowText.b, 0.62)
    readonly property color accent: pal.highlight
    readonly property color accentText: pal.highlightedText

    readonly property color cpu: "#3daee9"
    readonly property color memory: "#a56de2"
    readonly property color swap: "#e67e22"
    readonly property color read: "#27ae60"
    readonly property color write: "#f67400"
    readonly property color rx: "#1abc9c"
    readonly property color tx: "#e84393"
    readonly property color gpu: dark ? "#f5c542" : "#d49a00"
    readonly property color heat: "#f6a623"

    readonly property color good: "#27ae60"
    readonly property color warn: "#f67400"
    readonly property color bad: "#da4453"

    function usageColor(fraction, base) {
        if (fraction >= 0.9) return bad
        if (fraction >= 0.75) return warn
        return base === undefined ? accent : base
    }
    function tempColor(c) {
        if (c >= 85) return bad
        if (c >= 70) return warn
        return good
    }
}
