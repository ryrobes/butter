import QtQuick

Rectangle {
    id: root
    property bool elevated: false
    property color tint: "transparent"

    radius: appTheme.radius
    color: tint.a > 0
           ? tint
           : appTheme.alpha(elevated ? appTheme.lighterBackground
                                     : appTheme.darkBackground,
                            elevated ? 0.72 : 0.68)
    border.width: 1
    border.color: appTheme.alpha(appTheme.foreground, elevated ? 0.18 : 0.12)
}

