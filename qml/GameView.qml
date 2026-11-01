import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: view
    property string fontFamily: "Liberation Serif"

    readonly property var kind: ["", "pawn", "knight", "bishop", "rook", "queen", "king"]

    function pieceSource(side, type) {
        if (type < 1) return ""
        var who = side === 0 ? "alliance" : "horde"
        return "qrc:/assets/pieces/" + who + "_" + kind[type] + ".png"
    }

    Image {
        anchors.fill: parent
        source: "qrc:/assets/ui/table.jpg"
        fillMode: Image.PreserveAspectCrop
        opacity: 0.92
    }

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#99000000" }
            GradientStop { position: 0.45; color: "#33000000" }
            GradientStop { position: 1.0; color: "#b0000000" }
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        anchors.topMargin: 10
        anchors.bottomMargin: 12
        spacing: 14

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            BoardTable {
                anchors.fill: parent
                fontFamily: view.fontFamily
            }
        }

        Rectangle {
            Layout.preferredWidth: 320
            Layout.fillHeight: true
            color: "#CC140e0a"
            border.color: "#6d5a3a"

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 10

                Text {
                    text: Game.turn === 0 ? "ALLIANCE" : "HORDE"
                    color: Game.turn === 0 ? "#6eb2ff" : "#d43840"
                    font.family: view.fontFamily
                    font.pixelSize: 22
                    font.letterSpacing: 2
                }
                Text {
                    Layout.fillWidth: true
                    text: Game.statusLine
                    wrapMode: Text.WordWrap
                    color: "#f3e6c8"
                    font.family: view.fontFamily
                    font.pixelSize: 15
                }
                Text {
                    Layout.fillWidth: true
                    text: Game.opponentLine
                    color: "#cbb892"
                    wrapMode: Text.WordWrap
                    font.pixelSize: 13
                    font.family: "Liberation Serif"
                }

                Text { text: "Taken by the Alliance"; color: "#6eb2ff"; font.pixelSize: 12; font.family: view.fontFamily }
                Flow {
                    Layout.fillWidth: true
                    spacing: 2
                    Repeater {
                        model: Game.allianceSpoils
                        TeamFigure {
                            required property var modelData
                            source: view.pieceSource(1, modelData)
                            side: 1
                            width: 26
                            height: 26
                        }
                    }
                }
                Text { text: "Taken by the Horde"; color: "#d43840"; font.pixelSize: 12; font.family: view.fontFamily }
                Flow {
                    Layout.fillWidth: true
                    spacing: 2
                    Repeater {
                        model: Game.hordeSpoils
                        TeamFigure {
                            required property var modelData
                            source: view.pieceSource(0, modelData)
                            side: 0
                            width: 26
                            height: 26
                        }
                    }
                }

                Text { text: "Chronicle"; color: "#e2b657"; font.family: view.fontFamily; font.pixelSize: 14 }
                ListView {
                    id: chronicle
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: Math.ceil(Game.moves.length / 2)
                    onCountChanged: positionViewAtEnd()
                    delegate: Text {
                        required property int index
                        width: chronicle.width
                        color: "#f3e6c8"
                        font.family: "DejaVu Sans Mono"
                        font.pixelSize: 13
                        text: {
                            var a = Game.moves[index * 2]
                            var b = (index * 2 + 1 < Game.moves.length) ? Game.moves[index * 2 + 1] : ""
                            return (index + 1) + ".  " + a + (b.length ? "   " + b : "")
                        }
                    }
                }

                ColumnLayout {
                    visible: Game.mode === "net" || Game.mode === "bsky"
                    Layout.fillWidth: true
                    spacing: 4
                    Text { text: "Camp talk"; color: "#e2b657"; font.family: view.fontFamily; font.pixelSize: 14 }
                    ListView {
                        id: chatList
                        Layout.fillWidth: true
                        Layout.preferredHeight: 90
                        clip: true
                        model: Game.chatLines
                        onCountChanged: positionViewAtEnd()
                        delegate: Text {
                            required property string modelData
                            width: chatList.width
                            wrapMode: Text.Wrap
                            color: "#f3e6c8"
                            font.pixelSize: 12
                            text: modelData
                        }
                    }
                    TextField {
                        Layout.fillWidth: true
                        placeholderText: "Speak"
                        color: "#f3e6c8"
                        placeholderTextColor: "#8a7760"
                        font.pixelSize: 13
                        background: Rectangle { color: "#0c0907"; border.color: "#6d5a3a" }
                        onAccepted: {
                            Game.chat(text)
                            text = ""
                        }
                    }
                }

                AshButton {
                    label: "Buy me a coffee"
                    fontFamily: view.fontFamily
                    Layout.fillWidth: true
                    implicitHeight: 40
                    onClicked: Game.openCoffee()
                }

                RowLayout {
                    Layout.fillWidth: true
                    AshButton {
                        label: "Yield"
                        fontFamily: view.fontFamily
                        Layout.fillWidth: true
                        implicitHeight: 40
                        onClicked: Game.resign()
                    }
                    AshButton {
                        label: "Draw"
                        fontFamily: view.fontFamily
                        Layout.fillWidth: true
                        implicitHeight: 40
                        visible: Game.mode !== "ai"
                        onClicked: Game.offerDraw()
                    }
                    AshButton {
                        label: "Leave"
                        fontFamily: view.fontFamily
                        Layout.fillWidth: true
                        implicitHeight: 40
                        onClicked: Game.backToMenu()
                    }
                }
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        visible: Game.promoActive
        color: "#AA0c0907"
        z: 5
        MouseArea { anchors.fill: parent }
        Column {
            anchors.centerIn: parent
            spacing: 16
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "The foot soldier is raised"
                color: "#e2b657"
                font.family: view.fontFamily
                font.pixelSize: 28
            }
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 12
                Repeater {
                    model: [5, 4, 3, 2]
                    Rectangle {
                        required property int modelData
                        width: 110
                        height: 140
                        color: "#1a140f"
                        border.color: "#e2b657"
                        TeamFigure {
                            anchors.fill: parent
                            anchors.margins: 10
                            source: view.pieceSource(Game.promoSide, modelData)
                            side: Game.promoSide
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                Game.playSound("click")
                                Game.choosePromo(modelData)
                            }
                        }
                    }
                }
            }
            AshButton {
                anchors.horizontalCenter: parent.horizontalCenter
                label: "Hold"
                fontFamily: view.fontFamily
                implicitWidth: 160
                onClicked: Game.cancelPromo()
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        visible: Game.drawOffer && !Game.gameOver
        color: "#990c0907"
        z: 6
        MouseArea { anchors.fill: parent }
        Column {
            anchors.centerIn: parent
            spacing: 16
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: Game.statusLine
                color: "#f3e6c8"
                font.family: view.fontFamily
                font.pixelSize: 22
            }
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 12
                AshButton {
                    label: "Share the crown"
                    primary: true
                    fontFamily: view.fontFamily
                    onClicked: Game.answerDraw(true)
                }
                AshButton {
                    label: "Fight on"
                    fontFamily: view.fontFamily
                    onClicked: Game.answerDraw(false)
                }
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        visible: Game.gameOver && !Game.fightActive
        color: "#B30c0907"
        z: 7
        MouseArea { anchors.fill: parent }
        Column {
            anchors.centerIn: parent
            spacing: 18
            width: Math.min(640, parent.width - 80)
            Text {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                text: Game.resultText
                color: "#e2b657"
                font.family: view.fontFamily
                font.pixelSize: 32
            }
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 12
                AshButton {
                    label: "Rematch"
                    primary: true
                    fontFamily: view.fontFamily
                    onClicked: Game.rematch()
                }
                AshButton {
                    label: "Leave the field"
                    fontFamily: view.fontFamily
                    onClicked: Game.backToMenu()
                }
            }
        }
    }
}
