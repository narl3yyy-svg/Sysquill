import QtQuick
import QtQuick.Layouts
import SysquillUi

// An icon from the desktop icon theme (Breeze on Plasma).
Image {
    property string name
    property int size: 22
    source: name !== "" ? "image://icon/" + name : ""
    sourceSize.width: size
    sourceSize.height: size
    width: size
    height: size
    Layout.preferredWidth: size
    Layout.preferredHeight: size
    fillMode: Image.PreserveAspectFit
    smooth: true
}
