import QtQuick

Item {
    id: root
    property var samples: []
    property color traceColor: appTheme.accent

    visible: samples && samples.length > 0

    Canvas {
        id: canvas
        anchors.fill: parent
        antialiasing: true

        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()
            if (!root.samples || root.samples.length < 1)
                return

            var firstTime = Number(root.samples[0].atMs)
            var lastTime = Number(root.samples[root.samples.length - 1].atMs)
            var timeSpan = Math.max(1, lastTime - firstTime)
            var top = 16
            var bottom = height - 1
            var plotHeight = Math.max(1, bottom - top)
            var points = []
            for (var i = 0; i < root.samples.length; ++i) {
                var sample = root.samples[i]
                var x = width * (Number(sample.atMs) - firstTime) / timeSpan
                var ratio = Math.max(0, Math.min(1, Number(sample.freeRatio)))
                points.push({ x: x, y: bottom - ratio * plotHeight })
            }
            if (points.length === 1)
                points.push({ x: width, y: points[0].y })

            function tracePath() {
                ctx.beginPath()
                ctx.moveTo(points[0].x, points[0].y)
                for (var p = 1; p < points.length; ++p) {
                    var previous = points[p - 1]
                    var current = points[p]
                    var middle = (previous.x + current.x) / 2
                    ctx.bezierCurveTo(middle, previous.y, middle, current.y,
                                      current.x, current.y)
                }
            }

            tracePath()
            ctx.lineWidth = 1.25
            ctx.strokeStyle = appTheme.alpha(root.traceColor, 0.24)
            ctx.stroke()

            ctx.lineTo(width, bottom)
            ctx.lineTo(0, bottom)
            ctx.closePath()
            var wash = ctx.createLinearGradient(0, top, 0, bottom)
            wash.addColorStop(0, appTheme.alpha(root.traceColor, 0.01))
            wash.addColorStop(0.72, appTheme.alpha(root.traceColor, 0.035))
            wash.addColorStop(1, appTheme.alpha(root.traceColor, 0.13))
            ctx.fillStyle = wash
            ctx.fill()

            var latest = points[points.length - 1]
            ctx.beginPath()
            ctx.arc(latest.x - 2, latest.y, 2.4, 0, Math.PI * 2)
            ctx.fillStyle = appTheme.alpha(root.traceColor, 0.48)
            ctx.fill()
        }

        Connections {
            target: appTheme
            function onChanged() { canvas.requestPaint() }
        }
    }

    onSamplesChanged: canvas.requestPaint()
    onTraceColorChanged: canvas.requestPaint()
    onWidthChanged: canvas.requestPaint()
    onHeightChanged: canvas.requestPaint()
}
