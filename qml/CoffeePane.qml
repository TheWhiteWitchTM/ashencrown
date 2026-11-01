import QtQuick
import QtWebEngine

Item {
    readonly property string fontFamily: Window.window && Window.window.titleFont
            ? Window.window.titleFont
            : "Liberation Serif"

    Rectangle {
        anchors.fill: parent
        color: "#0c0907"
    }

    Rectangle {
        id: coffeeBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 52
        color: "#140e0a"
        border.color: "#e2b657"

        Text {
            anchors.left: parent.left
            anchors.leftMargin: 18
            anchors.verticalCenter: parent.verticalCenter
            text: "Buy me a coffee"
            color: "#e2b657"
            font.family: fontFamily
            font.pixelSize: 18
        }

        Rectangle {
            anchors.right: parent.right
            anchors.rightMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            width: 92
            height: 34
            color: "#0c0907"
            border.color: "#e2b657"
            Text {
                anchors.centerIn: parent
                text: "Close"
                color: "#f3e6c8"
                font.family: fontFamily
                font.pixelSize: 14
            }
            MouseArea {
                anchors.fill: parent
                onClicked: {
                    Game.playSound("click")
                    Game.closeCoffee()
                }
            }
        }
    }

    WebEngineView {
        id: coffeeView
        anchors.top: coffeeBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        url: Game.coffeeUrl
        backgroundColor: "#0c0907"
        onNewWindowRequested: function(request) {
            request.openIn(coffeeView)
        }
        onLoadingChanged: function(info) {
            if (info.status === 3) {
                coffeeFail.text = info.errorString && info.errorString.length
                        ? info.errorString
                        : "The page did not open."
                coffeeFail.visible = true
            } else if (info.status === 2) {
                coffeeFail.visible = false
            }
        }
    }

    Text {
        id: coffeeFail
        visible: false
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: coffeeBar.bottom
        anchors.topMargin: 24
        width: parent.width * 0.7
        wrapMode: Text.WordWrap
        horizontalAlignment: Text.AlignHCenter
        color: "#f3e6c8"
        font.family: fontFamily
        font.pixelSize: 16
    }
}
