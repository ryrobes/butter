import QtQuick

FocusScope {
    id: root
    property string label: ""
    property string glyph: ""
    property bool quiet: false
    signal triggered()

    implicitHeight: 30
    implicitWidth: content.width + 22
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

    Item {
        id: content
        anchors.centerIn: parent
        width: iconCell.visible
               ? iconCell.width + 6 + labelText.implicitWidth
               : labelText.implicitWidth
        height: Math.max(iconCell.visible ? iconCell.height : 0,
                         labelText.implicitHeight)

        Item {
            id: iconCell
            visible: root.glyph.length > 0
            width: 15
            height: 16
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter

            Text {
                anchors.fill: parent
                text: root.glyph
                color: appTheme.foreground
                font.family: appTheme.sansFont
                font.pixelSize: 14
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
        }

        Text {
            id: labelText
            anchors.left: iconCell.visible ? iconCell.right : parent.left
            anchors.leftMargin: iconCell.visible ? 6 : 0
            anchors.verticalCenter: parent.verticalCenter
            text: root.label
            color: appTheme.foreground
            font.family: appTheme.sansFont
            font.pixelSize: 12
            font.weight: Font.Medium
            wrapMode: Text.NoWrap
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
