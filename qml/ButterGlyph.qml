import QtQuick
import QtQuick.Shapes

Item {
    id: root
    property string variant: "dish"
    property color lineColor: appTheme.accent
    implicitWidth: 42
    implicitHeight: 31

    Shape {
        width: 120
        height: 88
        scale: Math.min(root.width / width, root.height / height)
        x: (root.width - width * scale) / 2
        y: (root.height - height * scale) / 2
        transformOrigin: Item.TopLeft
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            fillColor: "transparent"
            strokeColor: root.lineColor
            strokeWidth: 5
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin

            PathSvg {
                path: root.variant === "stick"
                      ? "M 56 32 L 95 22 L 108 35 L 69 45 Z M 56 32 V 43 M 60 54 L 69 63 L 108 53 V 35 M 69 45 V 63 M 37 55 L 50 68 L 50 50 L 37 37 Z M 37 37 L 47 34 L 60 47 V 65 L 50 68 M 60 47 L 50 50 M 6 60 L 23 55 L 36 68 L 19 73 Z M 6 60 V 70 L 19 83 L 36 78 V 68 M 19 73 V 83"
                      : "M 33 40 H 16 A 6 6 0 0 0 10 46 V 70 A 6 6 0 0 0 16 76 H 104 A 6 6 0 0 0 110 70 V 46 A 6 6 0 0 0 104 40 H 89 M 50 48 L 33 31 L 72 21 L 89 38 Z M 33 31 V 49 L 50 66 L 89 56 V 38 M 50 48 V 66"
            }
        }
    }
}
