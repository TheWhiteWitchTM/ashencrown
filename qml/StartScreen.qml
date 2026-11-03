import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: screen
    property string fontFamily: "Liberation Serif"

    Image {
        anchors.fill: parent
        source: "qrc:/assets/ui/title.jpg"
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
    }

    Rectangle {
        anchors.fill: parent
        color: "#660c0907"
    }

    Repeater {
        model: 16
        Rectangle {
            width: 3
            height: 10
            radius: 1
            color: index % 3 === 0 ? "#e2b657" : "#f3e6c8"
            opacity: 0.0
            x: (index * 89 + 20) % Math.max(1, screen.width)
            SequentialAnimation on y {
                loops: Animation.Infinite
                PauseAnimation { duration: index * 280 }
                NumberAnimation {
                    from: screen.height + 10
                    to: -30
                    duration: 5200 + (index % 5) * 600
                    easing.type: Easing.InQuad
                }
            }
            SequentialAnimation on opacity {
                loops: Animation.Infinite
                PauseAnimation { duration: index * 280 }
                NumberAnimation { from: 0; to: 0.55; duration: 700 }
                NumberAnimation { to: 0; duration: 4200 }
            }
        }
    }

    Rectangle {
        id: menu
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: Math.min(500, parent.width * 0.42)
        color: "#C00c0907"

        ColumnLayout {
            anchors.fill: parent
            anchors.leftMargin: 28
            anchors.rightMargin: 28
            anchors.topMargin: 14
            anchors.bottomMargin: 14
            spacing: 4

            Text {
                text: "ASHEN CROWN"
                color: "#e2b657"
                font.family: screen.fontFamily
                font.pixelSize: 40
                font.letterSpacing: 4
                Layout.fillWidth: true
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: "#e2b657"
            }

            Text {
                text: "Alliance against Horde.\nEvery piece that falls, falls in battle."
                color: "#f3e6c8"
                font.family: screen.fontFamily
                font.pixelSize: 16
                lineHeight: 1.25
                Layout.fillWidth: true
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: 2
                spacing: 8
                Repeater {
                    model: [
                        { name: "Alliance", side: 0, tint: "#2f86ea" },
                        { name: "Horde", side: 1, tint: "#8e1620" }
                    ]
                    Rectangle {
                        required property var modelData
                        Layout.fillWidth: true
                        Layout.preferredHeight: 38
                        color: Game.faction === modelData.side ? modelData.tint : "#140e0a"
                        border.color: Game.faction === modelData.side ? "#e2b657" : "#6d5a3a"
                        border.width: 1
                        Text {
                            anchors.centerIn: parent
                            text: modelData.name
                            color: "#f3e6c8"
                            font.family: screen.fontFamily
                            font.pixelSize: 16
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                Game.playSound("click")
                                Game.faction = modelData.side
                            }
                        }
                    }
                }
            }

            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: "#cbb892"
                font.family: screen.fontFamily
                font.pixelSize: 13
                text: {
                    var lines = [
                        "Novice trains you. A piece you can lose is marked, and a hover names the threat.",
                        "Veteran speaks only when a piece would truly be lost.",
                        "Warlord gives one line, and only for a piece left loose.",
                        "Legend gives no counsel."
                    ]
                    return lines[Game.level] || lines[3]
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 6
                Repeater {
                    model: ["Novice", "Veteran", "Warlord", "Legend"]
                    Rectangle {
                        required property string modelData
                        required property int index
                        Layout.fillWidth: true
                        Layout.preferredHeight: 30
                        color: Game.level === index ? "#3a2c1c" : "#140e0a"
                        border.color: Game.level === index ? "#e2b657" : "#4a3b28"
                        Text {
                            anchors.centerIn: parent
                            text: modelData
                            color: "#f3e6c8"
                            font.pixelSize: 11
                            font.family: "Liberation Serif"
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                Game.playSound("click")
                                Game.level = index
                            }
                        }
                    }
                }
            }

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 32
                Layout.topMargin: 4
                clip: true
                Accessible.role: Accessible.CheckBox
                Accessible.name: "White outlines on the pieces"
                Accessible.checked: Game.figureOutline
                Accessible.onPressAction: Game.figureOutline = !Game.figureOutline

                Rectangle {
                    id: outlineBox
                    width: 22
                    height: 22
                    anchors.verticalCenter: parent.verticalCenter
                    radius: 2
                    color: Game.figureOutline ? "#e2b657" : "#140e0a"
                    border.color: "#f3e6c8"
                    border.width: 2
                    Text {
                        anchors.centerIn: parent
                        visible: Game.figureOutline
                        text: "✓"
                        color: "#140e0a"
                        font.family: "DejaVu Sans"
                        font.pixelSize: 16
                        font.bold: true
                    }
                }
                Text {
                    x: outlineBox.width + 10
                    width: parent.width - x
                    anchors.verticalCenter: parent.verticalCenter
                    text: "White outlines"
                    color: "#f3e6c8"
                    font.family: screen.fontFamily
                    font.pixelSize: 16
                    elide: Text.ElideRight
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        Game.playSound("click")
                        Game.figureOutline = !Game.figureOutline
                    }
                }
            }

            AshButton {
                Layout.fillWidth: true
                Layout.preferredHeight: 40
                Layout.topMargin: 2
                primary: true
                label: Game.faction === 0 ? "Battle the Horde" : "Battle the Alliance"
                fontFamily: screen.fontFamily
                onClicked: Game.startAi(Game.faction, Game.level)
            }

            AshButton {
                Layout.fillWidth: true
                Layout.preferredHeight: 40
                label: "Two at one table"
                fontFamily: screen.fontFamily
                onClicked: Game.startHotseat()
            }

            AshButton {
                Layout.fillWidth: true
                Layout.preferredHeight: 40
                label: "Across the warfront"
                fontFamily: screen.fontFamily
                onClicked: Game.enterLobby(Game.faction)
            }

            AshButton {
                Layout.fillWidth: true
                Layout.preferredHeight: 40
                label: "Over Bluesky"
                fontFamily: screen.fontFamily
                onClicked: Game.enterBsky(Game.faction)
            }

            AshButton {
                Layout.fillWidth: true
                Layout.preferredHeight: 40
                label: "Buy me a coffee"
                fontFamily: screen.fontFamily
                onClicked: Game.openCoffee()
            }

            Item { Layout.fillHeight: true }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                TextField {
                    id: nameField
                    Layout.fillWidth: true
                    Layout.preferredHeight: 36
                    placeholderText: "Your name"
                    color: "#f3e6c8"
                    placeholderTextColor: "#8a7760"
                    font.pixelSize: 15
                    background: Rectangle {
                        color: "#140e0a"
                        border.color: "#6d5a3a"
                    }
                    onEditingFinished: Game.setPlayerName(text)
                    Component.onCompleted: text = Game.playerName
                }
                Slider {
                    id: vol
                    Layout.preferredWidth: 120
                    Layout.preferredHeight: 28
                    from: 0
                    to: 1
                    value: 0.85
                    Component.onCompleted: value = Game.volume
                    onMoved: Game.setVolume(value)
                    background: Rectangle {
                        x: vol.leftPadding
                        y: vol.topPadding + vol.availableHeight / 2 - 2
                        width: vol.availableWidth
                        height: 4
                        color: "#3a2c1c"
                        Rectangle {
                            width: vol.visualPosition * parent.width
                            height: parent.height
                            color: "#e2b657"
                        }
                    }
                    handle: Rectangle {
                        x: vol.leftPadding + vol.visualPosition * (vol.availableWidth - width)
                        y: vol.topPadding + vol.availableHeight / 2 - height / 2
                        width: 14
                        height: 14
                        radius: 7
                        color: "#f3e6c8"
                        border.color: "#e2b657"
                    }
                }
                Rectangle {
                    Layout.preferredWidth: 72
                    Layout.preferredHeight: 36
                    color: "#140e0a"
                    border.color: "#6d5a3a"
                    Text {
                        anchors.centerIn: parent
                        text: Game.musicOn ? "Music" : "Muted"
                        color: "#f3e6c8"
                        font.pixelSize: 12
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: Game.toggleMusic()
                    }
                }
            }
        }
    }

    FightScene {
        anchors.left: menu.right
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.margins: 22
        reel: true
        visible: screen.visible
        fontFamily: screen.fontFamily
    }
}
