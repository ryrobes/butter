import QtQuick
import QtQuick.Shapes

Item {
    id: root

    property color glyphColor: appTheme.foreground

    implicitWidth: 14
    implicitHeight: 14

    Shape {
        width: 20
        height: 20
        scale: Math.min(root.width / width, root.height / height)
        x: (root.width - width * scale) / 2
        y: (root.height - height * scale) / 2
        transformOrigin: Item.TopLeft
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            fillColor: "transparent"
            strokeColor: root.glyphColor
            strokeWidth: 1.7
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin

            PathSvg {
                path: "M 2.5 3.5 H 17.5 V 16.5 H 2.5 Z M 5.5 7 L 8.1 9.5 L 5.5 12 M 10.2 12 H 14.2"
            }
        }
    }
}
