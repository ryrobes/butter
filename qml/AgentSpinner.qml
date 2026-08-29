import QtQuick

Item {
    id: root
    property bool running: false
    property color spinnerColor: appTheme.accent

    implicitWidth: 13
    implicitHeight: 13
    visible: running

    Canvas {
        id: canvas
        anchors.fill: parent
        antialiasing: true

        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()
            var center = width / 2
            var radius = Math.max(1, Math.min(width, height) / 2 - 1.5)

            ctx.beginPath()
            ctx.arc(center, center, radius, -Math.PI * 0.42,
                    Math.PI * 1.16, false)
            ctx.lineWidth = 1.5
            ctx.lineCap = "round"
            ctx.strokeStyle = appTheme.alpha(root.spinnerColor, 0.88)
            ctx.stroke()

            var end = Math.PI * 1.16
            ctx.beginPath()
            ctx.arc(center + Math.cos(end) * radius,
                    center + Math.sin(end) * radius,
                    1.35, 0, Math.PI * 2)
            ctx.fillStyle = root.spinnerColor
            ctx.fill()
        }

        Connections {
            target: appTheme
            function onChanged() { canvas.requestPaint() }
        }
    }

    RotationAnimator on rotation {
        from: 0
        to: 360
        duration: 850
        loops: Animation.Infinite
        running: root.running
    }

    onSpinnerColorChanged: canvas.requestPaint()
}
