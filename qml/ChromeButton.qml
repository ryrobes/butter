import QtQuick

FocusScope {
    id: root
    property string label: ""
    property string glyph: ""
    property bool quiet: false
    signal triggered()

    implicitHeight: 36
    implicitWidth: content.implicitWidth + 24
    activeFocusOnTab: true
    opacity: enabled ? 1 : 0.45

    Rectangle {
        anchors.fill: parent
        radius: appTheme.radius
        color: tap.pressed
               ? appTheme.alpha(appTheme.foreground, 0.16)
               : (hover.hovered || root.activeFocus
                  ? appTheme.alpha(appTheme.foreground, 0.09)
                  : (root.quiet ? "transparent"
                                : appTheme.alpha(appTheme.foreground, 0.04)))
        border.width: 1
        border.color: appTheme.alpha(appTheme.foreground,
                                     hover.hovered || root.activeFocus ? 0.30 : 0.16)
    }

    Row {
        id: content
        anchors.centerIn: parent
        spacing: 7

        Text {
            visible: root.glyph.length > 0
            text: root.glyph
            color: appTheme.foreground
            font.family: appTheme.sansFont
            font.pixelSize: 16
        }
        Text {
            text: root.label
            color: appTheme.foreground
            font.family: appTheme.sansFont
            font.pixelSize: 12
            font.weight: Font.Medium
        }
    }

    HoverHandler { id: hover }
    TapHandler {
        id: tap
        onTapped: {
            root.forceActiveFocus()
            root.triggered()
        }
    }
    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter ||
                event.key === Qt.Key_Space) {
            root.triggered()
            event.accepted = true
        }
    }
}

