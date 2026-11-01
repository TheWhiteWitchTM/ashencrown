import QtQuick

Item {
    id: control
    property string label: ""
    property bool primary: false
    property string fontFamily: "Liberation Serif"
    signal clicked()
    implicitWidth: 280
    implicitHeight: 52

    Rectangle {
        anchors.fill: parent
        radius: 2
        color: area.pressed ? "#2a2118" : (area.containsMouse ? "#241910" : "#140e0a")
        border.width: control.primary ? 2 : 1
        border.color: control.primary ? "#e2b657" : "#6d5a3a"

        Text {
            anchors.centerIn: parent
            text: control.label
            color: "#f3e6c8"
            font.family: control.fontFamily
            font.pixelSize: 18
            font.letterSpacing: 1.2
        }
    }

    MouseArea {
        id: area
        anchors.fill: parent
        hoverEnabled: true
        onClicked: {
            Game.playSound("click")
            control.clicked()
        }
    }
}
