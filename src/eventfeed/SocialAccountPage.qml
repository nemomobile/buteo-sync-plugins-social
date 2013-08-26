import QtQuick 2.0
import Sailfish.Silica 1.0


Page {
    id: container
    signal indexSelected(int index)
    property int currentIndex
    property variant accounts
    property string headerText

    ListModel {
        id: model
    }

    Component.onCompleted: {
        for (var i = 0; i < container.accounts.length; i++) {
            model.append({"name": container.accounts[i]["name"]})
        }
    }

    SilicaListView {
        anchors.fill: parent
        model: model

        header: PageHeader {
            title: container.headerText
        }

        delegate: BackgroundItem {
            id: listItem
            onClicked: {
                container.currentIndex = model.index
                container.indexSelected(container.currentIndex)
            }
            Label {
                anchors {
                    left: parent.left
                    leftMargin: Theme.paddingLarge
                    right: parent.left
                    rightMargin: Theme.paddingLarge
                    verticalCenter: parent.verticalCenter
                }
                text: model.name
                color: (listItem.highlighted || model.index == container.currentIndex) ? Theme.highlightColor
                                                                                       : Theme.primaryColor
            }
        }


        VerticalScrollDecorator {}
    }
}
