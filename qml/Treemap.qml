import QtQuick

Item {
    id: root
    property var entries: []
    property int hoveredIndex: -1
    property real pointerX: 0
    property real pointerY: 0
    property var laidOut: []
    signal activated(var entry)

    function formatBytes(bytes) {
        var units = ["B", "KB", "MB", "GB", "TB"]
        var value = Number(bytes || 0)
        var unit = 0
        while (value >= 1000 && unit < units.length - 1) {
            value /= 1000
            unit++
        }
        return value.toFixed(value >= 100 || unit === 0 ? 0 : 1) + " " + units[unit]
    }

    function colorFor(entry, index, hovered) {
        if (entry.kind === "build")
            return appTheme.alpha(appTheme.blend(appTheme.accent,
                                                  appTheme.urgent, 0.28),
                                  hovered ? 0.52 : 0.35)
        var turns = [0.08, 0.14, 0.20, 0.27, 0.34, 0.41]
        var color = appTheme.blend(appTheme.darkBackground, appTheme.accent,
                                   turns[index % turns.length])
        return appTheme.alpha(color, hovered ? 0.94 : 0.78)
    }

    function makeLayout() {
        var items = []
        for (var i = 0; i < root.entries.length; ++i) {
            var entry = root.entries[i]
            if (Number(entry.bytes || 0) > 0)
                items.push({ entry: entry, bytes: Number(entry.bytes), index: i })
        }
        items.sort(function(a, b) { return b.bytes - a.bytes })
        var output = []

        function split(group, x, y, width, height, depth) {
            if (!group.length || width < 2 || height < 2)
                return
            if (group.length === 1) {
                output.push({ x: x, y: y, width: width, height: height,
                              entry: group[0].entry, index: group[0].index })
                return
            }

            var total = 0
            for (var j = 0; j < group.length; ++j) total += group[j].bytes
            var firstTotal = 0
            var cut = 1
            var best = Math.abs(total - 2 * group[0].bytes)
            for (var k = 0; k < group.length - 1; ++k) {
                firstTotal += group[k].bytes
                var difference = Math.abs(total - 2 * firstTotal)
                if (difference <= best) {
                    best = difference
                    cut = k + 1
                }
            }
            firstTotal = 0
            for (var n = 0; n < cut; ++n) firstTotal += group[n].bytes
            var ratio = total > 0 ? firstTotal / total : 0.5
            if (width >= height) {
                var firstWidth = width * ratio
                split(group.slice(0, cut), x, y, firstWidth, height, depth + 1)
                split(group.slice(cut), x + firstWidth, y,
                      width - firstWidth, height, depth + 1)
            } else {
                var firstHeight = height * ratio
                split(group.slice(0, cut), x, y, width, firstHeight, depth + 1)
                split(group.slice(cut), x, y + firstHeight,
                      width, height - firstHeight, depth + 1)
            }
        }

        split(items, 0, 0, root.width, root.height, 0)
        return output
    }

    onEntriesChanged: canvas.requestPaint()
    onWidthChanged: canvas.requestPaint()
    onHeightChanged: canvas.requestPaint()

    Canvas {
        id: canvas
        anchors.fill: parent
        renderStrategy: Canvas.Threaded

        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()
            ctx.clearRect(0, 0, width, height)
            root.laidOut = root.makeLayout()
            var gap = 2
            for (var i = 0; i < root.laidOut.length; ++i) {
                var rect = root.laidOut[i]
                var hovered = i === root.hoveredIndex
                var x = rect.x + gap
                var y = rect.y + gap
                var w = Math.max(0, rect.width - gap * 2)
                var h = Math.max(0, rect.height - gap * 2)
                ctx.fillStyle = root.colorFor(rect.entry, rect.index, hovered)
                ctx.fillRect(x, y, w, h)
                ctx.strokeStyle = appTheme.alpha(appTheme.foreground,
                                                  hovered ? 0.40 : 0.12)
                ctx.lineWidth = hovered ? 2 : 1
                ctx.strokeRect(x + 0.5, y + 0.5,
                               Math.max(0, w - 1), Math.max(0, h - 1))

                if (w > 66 && h > 34) {
                    ctx.save()
                    ctx.beginPath()
                    ctx.rect(x + 8, y + 6, w - 16, h - 12)
                    ctx.clip()
                    ctx.fillStyle = appTheme.brightForeground
                    ctx.font = "600 12px " + appTheme.sansFont
                    var label = String(rect.entry.name || "")
                    while (label.length > 2 && ctx.measureText(label).width > w - 18)
                        label = label.slice(0, -2)
                    if (label !== String(rect.entry.name || "")) label += "…"
                    ctx.fillText(label, x + 9, y + 19)
                    if (h > 52) {
                        ctx.fillStyle = appTheme.alpha(appTheme.foreground, 0.72)
                        ctx.font = "10px " + appTheme.monoFont
                        ctx.fillText(root.formatBytes(rect.entry.bytes), x + 9, y + 36)
                    }
                    ctx.restore()
                }
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: root.hoveredIndex >= 0 ? Qt.PointingHandCursor : Qt.ArrowCursor
        onPositionChanged: function(mouse) {
            root.pointerX = mouse.x
            root.pointerY = mouse.y
            var found = -1
            for (var i = 0; i < root.laidOut.length; ++i) {
                var rect = root.laidOut[i]
                if (mouse.x >= rect.x && mouse.x <= rect.x + rect.width &&
                        mouse.y >= rect.y && mouse.y <= rect.y + rect.height) {
                    found = i
                    break
                }
            }
            if (root.hoveredIndex !== found) {
                root.hoveredIndex = found
                canvas.requestPaint()
            }
        }
        onExited: {
            root.hoveredIndex = -1
            canvas.requestPaint()
        }
        onClicked: {
            if (root.hoveredIndex >= 0) {
                var entry = root.laidOut[root.hoveredIndex].entry
                root.activated(entry)
            }
        }
    }

    Rectangle {
        visible: root.hoveredIndex >= 0
        x: Math.min(root.width - width - 6, root.pointerX + 12)
        y: Math.min(root.height - height - 6, root.pointerY + 12)
        width: tipText.implicitWidth + 18
        height: 30
        radius: 7
        color: appTheme.darkerBackground
        border.width: 1
        border.color: appTheme.alpha(appTheme.foreground, 0.24)
        Text {
            id: tipText
            anchors.centerIn: parent
            text: root.hoveredIndex >= 0
                  ? root.laidOut[root.hoveredIndex].entry.name + "  ·  " +
                    root.formatBytes(root.laidOut[root.hoveredIndex].entry.bytes)
                  : ""
            color: appTheme.foreground
            font.family: appTheme.sansFont
            font.pixelSize: 11
        }
    }
}
