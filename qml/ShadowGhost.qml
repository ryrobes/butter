pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Shapes

Item {
    id: root

    property string status: "unconfigured"

    readonly property bool isRunning: status === "running"
    readonly property bool isCurrent: status === "current"
    readonly property bool isWaiting: status === "waiting"
    readonly property bool isError: status === "error"
    readonly property color tone: isError
                                  ? appTheme.urgent
                                  : isWaiting || status === "unconfigured"
                                    ? appTheme.muted
                                    : appTheme.accent
    readonly property real gazeX: isRunning ? 1.15
                                  : isCurrent ? -0.55
                                  : 0

    implicitWidth: 27
    implicitHeight: 30
    opacity: isWaiting ? 0.60 : status === "unconfigured" ? 0.68 : 0.94

    Item {
        id: ghost
        width: 27
        height: 30
        anchors.horizontalCenter: parent.horizontalCenter
        y: 0

        Shape {
            anchors.fill: parent
            preferredRendererType: Shape.CurveRenderer

            ShapePath {
                fillColor: appTheme.alpha(
                               appTheme.blend(appTheme.darkBackground,
                                              root.tone,
                                              root.isCurrent ? 0.54
                                              : root.isError ? 0.43
                                              : root.isRunning ? 0.46 : 0.32),
                               root.isWaiting ? 0.62 : 0.92)
                strokeColor: appTheme.alpha(root.tone,
                                            root.isError ? 0.92 : 0.74)
                strokeWidth: 1.25
                capStyle: ShapePath.RoundCap
                joinStyle: ShapePath.RoundJoin

                PathSvg {
                    path: "M 13.5 1.7 C 6.8 1.7 2.3 6.9 2.3 13.6 L 2.3 28.1 L 6.2 24.6 L 9.9 28.1 L 13.6 24.6 L 17.3 28.1 L 21 24.6 L 24.7 28.1 L 24.7 13.6 C 24.7 6.9 20.2 1.7 13.5 1.7 Z"
                }
            }
        }

        Item {
            id: openEyes
            anchors.fill: parent
            visible: !root.isWaiting

            Repeater {
                model: [8.8, 18.2]

                Rectangle {
                    required property real modelData
                    x: modelData - width / 2
                    y: 9.2
                    width: 5.5
                    height: 6.8
                    radius: width / 2
                    color: appTheme.alpha(appTheme.brightForeground, 0.76)

                    Rectangle {
                        width: 2.35
                        height: 3.05
                        radius: width / 2
                        x: (parent.width - width) / 2 + root.gazeX
                        y: root.isError ? 1.25 : 1.85
                        color: appTheme.alpha(
                                   appTheme.blend(appTheme.darkerBackground,
                                                  root.tone, 0.16), 0.96)
                    }
                }
            }
        }

        Item {
            anchors.fill: parent
            visible: root.isWaiting

            Repeater {
                model: [8.8, 18.2]

                Rectangle {
                    required property real modelData
                    x: modelData - width / 2
                    y: 12.4
                    width: 5.2
                    height: 1.25
                    radius: height / 2
                    color: appTheme.alpha(appTheme.foreground, 0.58)
                }
            }
        }

        Shape {
            anchors.fill: parent
            visible: root.isCurrent
            preferredRendererType: Shape.CurveRenderer

            ShapePath {
                fillColor: "transparent"
                strokeColor: appTheme.alpha(appTheme.brightForeground, 0.52)
                strokeWidth: 1.1
                capStyle: ShapePath.RoundCap

                PathSvg { path: "M 10.5 19.1 Q 13.5 21.2 16.5 19.1" }
            }
        }

        Shape {
            anchors.fill: parent
            visible: root.isError
            preferredRendererType: Shape.CurveRenderer

            ShapePath {
                fillColor: "transparent"
                strokeColor: appTheme.alpha(appTheme.brightForeground, 0.55)
                strokeWidth: 1.1
                capStyle: ShapePath.RoundCap

                PathSvg { path: "M 11.1 20.2 Q 13.5 18.3 15.9 20.2" }
            }
        }

        SequentialAnimation {
            id: floatAnimation
            running: root.isRunning
            loops: Animation.Infinite

            NumberAnimation {
                target: ghost
                property: "y"
                from: 0.8
                to: -1.0
                duration: 620
                easing.type: Easing.InOutSine
            }
            NumberAnimation {
                target: ghost
                property: "y"
                from: -1.0
                to: 0.8
                duration: 620
                easing.type: Easing.InOutSine
            }
        }

        Connections {
            target: root
            function onIsRunningChanged() {
                if (!root.isRunning)
                    ghost.y = 0
            }
        }
    }
}
