import QtQuick
import QtQuick.Effects

Item {
    id: root
    implicitWidth: 105
    implicitHeight: 25

    Image {
        id: outline
        anchors.fill: parent
        visible: false
        source: "qrc:/qt/qml/Butter/assets/butter-wordmark.svg"
        sourceSize.width: 589
        sourceSize.height: 140
        fillMode: Image.PreserveAspectFit
        smooth: true
        mipmap: true
    }

    MultiEffect {
        anchors.fill: parent
        source: outline
        colorization: 1
        colorizationColor: appTheme.accent
        autoPaddingEnabled: false
    }
}
