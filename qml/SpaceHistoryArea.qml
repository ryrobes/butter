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
            var minimumMb = Number(root.samples[0].freeMegabytes)
            var maximumMb = minimumMb
            for (var s = 1; s < root.samples.length; ++s) {
                var freeMb = Number(root.samples[s].freeMegabytes)
                minimumMb = Math.min(minimumMb, freeMb)
                maximumMb = Math.max(maximumMb, freeMb)
            }
            var observedRangeMb = maximumMb - minimumMb
            var domainRangeMb = Math.max(1, observedRangeMb)
            var domainPaddingMb = domainRangeMb * 0.14
            var domainHeightMb = domainRangeMb + domainPaddingMb * 2
            var points = []
            for (var i = 0; i < root.samples.length; ++i) {
                var sample = root.samples[i]
                var x = width * (Number(sample.atMs) - firstTime) / timeSpan
                var normalizedMb = Number(sample.freeMegabytes) - minimumMb
                var normalizedRatio = observedRangeMb === 0
                                      ? 0.5
                                      : (normalizedMb + domainPaddingMb) /
                                        domainHeightMb
                points.push({ x: x,
                              y: bottom - normalizedRatio * plotHeight })
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
            ctx.lineWidth = 1.5
            ctx.strokeStyle = appTheme.alpha(root.traceColor, 0.36)
            ctx.stroke()

            ctx.lineTo(width, bottom)
            ctx.lineTo(0, bottom)
            ctx.closePath()
            var wash = ctx.createLinearGradient(0, top, 0, bottom)
            wash.addColorStop(0, appTheme.alpha(root.traceColor, 0.015))
            wash.addColorStop(0.72, appTheme.alpha(root.traceColor, 0.055))
            wash.addColorStop(1, appTheme.alpha(root.traceColor, 0.16))
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
