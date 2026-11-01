import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: lobby
    property string fontFamily: "Liberation Serif"

    Image {
        anchors.fill: parent
        source: "qrc:/assets/ui/table.jpg"
        fillMode: Image.PreserveAspectCrop
        opacity: 0.45
    }

    Rectangle {
        anchors.fill: parent
        color: "#C00c0907"
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 28
        anchors.rightMargin: 28
        anchors.topMargin: 22
        anchors.bottomMargin: 96
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            Text {
                text: Game.bskyTable ? "BLUESKY TABLE" : "THE WARFRONT"
                color: "#e2b657"
                font.family: lobby.fontFamily
                font.pixelSize: 36
                font.letterSpacing: 3
            }
            Item { Layout.fillWidth: true }
            AshButton {
                label: "Buy me a coffee"
                fontFamily: lobby.fontFamily
                implicitWidth: 210
                onClicked: Game.openCoffee()
            }
            AshButton {
                label: "Leave"
                fontFamily: lobby.fontFamily
                implicitWidth: 140
                onClicked: Game.backToMenu()
            }
        }

        Text {
            Layout.fillWidth: true
            text: Game.bskyTable
                  ? "Sign in with Bluesky. Both commanders type each other's handle and sit. Each side watches the other's public firehose. A move is a public post, and it is deleted once the other side has taken it. One of you keeps the side you picked. The other takes the opposite side."
                  : "One commander waits and shares the warcode. The other pastes it and marches. The march tries the local network, then a port the router opened, then a UDP punch."
            color: "#f3e6c8"
            wrapMode: Text.WordWrap
            font.pixelSize: 15
            font.family: "Liberation Serif"
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Repeater {
                model: [
                    { name: "Hold Alliance", side: 0, tint: "#2f86ea" },
                    { name: "Hold Horde", side: 1, tint: "#8e1620" }
                ]
                Rectangle {
                    required property var modelData
                    Layout.preferredWidth: 180
                    Layout.preferredHeight: 40
                    color: Game.faction === modelData.side ? modelData.tint : "#140e0a"
                    border.color: "#e2b657"
                    Text {
                        anchors.centerIn: parent
                        text: modelData.name
                        color: "#f3e6c8"
                        font.family: lobby.fontFamily
                        font.pixelSize: 15
                    }
                    MouseArea {
                        anchors.fill: parent
                        enabled: !Game.waiting && Game.screen === 1
                        onClicked: {
                            Game.playSound("click")
                            Game.faction = modelData.side
                        }
                    }
                }
            }
            Item { Layout.fillWidth: true }
        }

        ColumnLayout {
            visible: Game.bskyTable
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 10

            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                TextField {
                    id: bskyHandle
                    visible: !Game.bskyLoggedIn
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
                    placeholderText: "you.bsky.social"
                    color: "#f3e6c8"
                    placeholderTextColor: "#8a7760"
                    font.family: "DejaVu Sans Mono"
                    font.pixelSize: 15
                    background: Rectangle { color: "#140e0a"; border.color: "#6d5a3a" }
                    onAccepted: Game.bskyLogin(text)
                }
                AshButton {
                    visible: !Game.bskyLoggedIn
                    label: Game.bskyBusy ? "Waiting on Bluesky…" : "Sign in"
                    primary: true
                    fontFamily: lobby.fontFamily
                    implicitWidth: 220
                    enabled: !Game.bskyBusy
                    onClicked: Game.bskyLogin(bskyHandle.text)
                }
                Text {
                    visible: Game.bskyLoggedIn
                    text: "Signed in as @" + Game.bskyHandle
                    color: "#6eb2ff"
                    font.pixelSize: 16
                    font.family: lobby.fontFamily
                    Layout.fillWidth: true
                }
                AshButton {
                    visible: Game.bskyLoggedIn
                    label: "Sign out"
                    fontFamily: lobby.fontFamily
                    implicitWidth: 140
                    onClicked: Game.bskyLogout()
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                TextField {
                    id: bskyOpp
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
                    placeholderText: "their-handle.bsky.social"
                    color: "#f3e6c8"
                    placeholderTextColor: "#8a7760"
                    font.family: "DejaVu Sans Mono"
                    font.pixelSize: 15
                    enabled: Game.bskyLoggedIn && !Game.bskyBusy
                    background: Rectangle { color: "#140e0a"; border.color: "#6d5a3a" }
                    onAccepted: Game.bskySit(text)
                }
                AshButton {
                    label: "Sit at their firehose"
                    primary: true
                    fontFamily: lobby.fontFamily
                    implicitWidth: 240
                    enabled: Game.bskyLoggedIn && !Game.bskyBusy
                    onClicked: Game.bskySit(bskyOpp.text)
                }
            }
        }

        Rectangle {
            visible: !Game.bskyTable
            Layout.fillWidth: true
            Layout.preferredHeight: codeText.implicitHeight + 28
            color: "#140e0a"
            border.color: "#e2b657"
            border.width: 1
            Text {
                id: codeText
                anchors.fill: parent
                anchors.margins: 14
                text: Game.warcode.length ? Game.warcode : "Forging a warcode…"
                color: "#f3e6c8"
                wrapMode: Text.WrapAnywhere
                font.family: "DejaVu Sans Mono"
                font.pixelSize: 18
            }
        }

        RowLayout {
            visible: !Game.bskyTable
            AshButton {
                label: "Copy warcode"
                fontFamily: lobby.fontFamily
                implicitWidth: 200
                primary: true
                onClicked: Game.copyText(Game.warcode)
            }
            Text {
                Layout.fillWidth: true
                text: Game.endpointLabel
                color: "#f3e6c8"
                wrapMode: Text.WordWrap
                font.pixelSize: 14
                font.family: "Liberation Serif"
            }
        }

        Text {
            text: Game.statusLine
            color: "#e2b657"
            font.pixelSize: 16
            font.family: lobby.fontFamily
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 16

            ColumnLayout {
                visible: !Game.bskyTable
                Layout.fillWidth: true
                Layout.fillHeight: true
                Text {
                    text: "Commanders on this network"
                    color: "#e2b657"
                    font.family: lobby.fontFamily
                    font.pixelSize: 16
                }
                ListView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: Game.lanHosts
                    spacing: 6
                    delegate: Rectangle {
                        required property var modelData
                        width: ListView.view.width
                        height: 44
                        color: "#1a140f"
                        border.color: "#6d5a3a"
                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 8
                            Text {
                                Layout.fillWidth: true
                                color: "#f3e6c8"
                                font.pixelSize: 14
                                text: modelData.name + "  ·  " + (modelData.faction === 1 ? "Horde" : "Alliance") + "  ·  " + modelData.ip
                                elide: Text.ElideRight
                            }
                            AshButton {
                                label: "March"
                                implicitWidth: 110
                                implicitHeight: 32
                                fontFamily: lobby.fontFamily
                                onClicked: Game.joinLan(modelData.ip, modelData.port)
                            }
                        }
                    }
                    Text {
                        anchors.centerIn: parent
                        visible: parent.count === 0
                        text: "None yet. A code still works across the room."
                        color: "#8a7760"
                        font.pixelSize: 14
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Text {
                    text: Game.bskyTable ? "Firehose" : "March on a code or host:port"
                    color: "#e2b657"
                    font.family: lobby.fontFamily
                    font.pixelSize: 16
                }
                TextField {
                    id: codeField
                    visible: !Game.bskyTable
                    Layout.fillWidth: true
                    placeholderText: "AC-…. or 192.168.1.20:41777"
                    color: "#f3e6c8"
                    placeholderTextColor: "#8a7760"
                    font.family: "DejaVu Sans Mono"
                    font.pixelSize: 14
                    background: Rectangle {
                        color: "#140e0a"
                        border.color: "#6d5a3a"
                    }
                    onAccepted: Game.march(text)
                }
                AshButton {
                    visible: !Game.bskyTable
                    label: Game.joining ? "Marching…" : "March"
                    primary: true
                    fontFamily: lobby.fontFamily
                    Layout.fillWidth: true
                    enabled: !Game.joining
                    onClicked: Game.march(codeField.text)
                }
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: "#100c09"
                    border.color: "#3a2c1c"
                    ListView {
                        anchors.fill: parent
                        anchors.margins: 8
                        clip: true
                        model: Game.netLines
                        spacing: 2
                        onCountChanged: positionViewAtEnd()
                        delegate: Text {
                            required property string modelData
                            width: ListView.view.width
                            text: modelData
                            color: "#cbb892"
                            wrapMode: Text.Wrap
                            font.family: "DejaVu Sans Mono"
                            font.pixelSize: 12
                        }
                    }
                }
            }
        }
    }
}
