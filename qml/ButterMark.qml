import QtQuick

Item {
    id: root
    implicitWidth: 42
    implicitHeight: 42

    ButterGlyph {
        width: 42
        height: 31
        anchors.centerIn: parent
        variant: "dish"
        lineColor: appTheme.accent
    }
}
