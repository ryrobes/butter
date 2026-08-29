import QtQuick

Item {
    id: root
    property real percent: 0
    property color progressColor: appTheme.accent
    property string primary: ""
    property string secondary: "free"
    property string tertiary: ""

    implicitWidth: 176
    implicitHeight: 176

    Canvas {
        id: canvas
        anchors.fill: parent
        antialiasing: true
        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()
            var c = width / 2
            var r = Math.min(width, height) / 2 - 14
            ctx.lineWidth = 11
            ctx.lineCap = "round"
            ctx.strokeStyle = appTheme.alpha(appTheme.foreground, 0.10)
            ctx.beginPath()
            ctx.arc(c, height / 2, r, -Math.PI * 0.78, Math.PI * 0.78)
            ctx.stroke()
            ctx.strokeStyle = root.progressColor
            ctx.beginPath()
            ctx.arc(c, height / 2, r, -Math.PI * 0.78,
                    -Math.PI * 0.78 + Math.PI * 1.56 *
                    Math.max(0, Math.min(100, root.percent)) / 100)
            ctx.stroke()
        }
        Connections {
            target: appTheme
            function onChanged() { canvas.requestPaint() }
        }
    }

    Column {
        anchors.centerIn: parent
        anchors.verticalCenterOffset: 4
        spacing: 1

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.primary
            color: appTheme.brightForeground
            font.family: appTheme.sansFont
            font.pixelSize: 25
            font.weight: Font.DemiBold
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.secondary
            color: appTheme.muted
            font.family: appTheme.sansFont
            font.pixelSize: 11
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: root.tertiary.length > 0
            text: root.tertiary
            color: appTheme.alpha(appTheme.foreground, 0.58)
            font.family: appTheme.monoFont
            font.pixelSize: 10
        }
    }

    onPercentChanged: canvas.requestPaint()
    onProgressColorChanged: canvas.requestPaint()
}
