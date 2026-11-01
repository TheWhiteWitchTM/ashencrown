import QtQuick
import QtQuick.Controls

ApplicationWindow {
    id: root
    width: 1440
    height: 900
    minimumWidth: 1100
    minimumHeight: 700
    visible: true
    title: "Ashen Crown"
    color: "#0c0907"

    FontLoader {
        id: cinzel
        source: "qrc:/assets/fonts/Cinzel-Variable.ttf"
    }

    readonly property string titleFont: cinzel.status === FontLoader.Ready ? cinzel.name : "Liberation Serif"

    StartScreen {
        anchors.fill: parent
        visible: Game.screen === 0
        fontFamily: root.titleFont
    }

    WarLobby {
        anchors.fill: parent
        visible: Game.screen === 1
        fontFamily: root.titleFont
    }

    GameView {
        anchors.fill: parent
        visible: Game.screen === 2
        fontFamily: root.titleFont
    }

    FightScene {
        id: fightScene
        anchors.fill: parent
        z: 20
        visible: Game.fightActive
        fontFamily: root.titleFont
    }

    Shortcut {
        sequence: "Escape"
        enabled: Game.coffeeOpen || Game.fightActive
        onActivated: {
            if (Game.coffeeOpen) Game.closeCoffee()
            else fightScene.closeFight()
        }
    }

    Loader {
        id: coffeeLoader
        anchors.fill: parent
        z: 40
        active: Game.coffeeOpen && Game.coffeeEmbedded
        source: "CoffeePane.qml"
    }
}
