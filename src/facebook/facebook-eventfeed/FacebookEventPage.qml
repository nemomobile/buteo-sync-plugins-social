import QtQuick 2.0
import Sailfish.Silica 1.0
import org.nemomobile.lipstick 0.1
import org.nemomobile.contacts 1.0
import org.nemomobile.social 1.0
import Sailfish.Accounts 1.0
import Sailfish.TextLinking 1.0
import "shared"

FacebookNotificationPage {
    id: container

    property string bodyText
    property bool hosting: facebookEvent.ownerId !== ""
                             && facebookMe
                             && facebookMe.node
                             && facebookMe.node.identifier === facebookEvent.ownerId

    Component.onCompleted: {
        if (readyToPopulate) {
            facebookEvent.load(container.nodeIdentifier)
        }
    }

    onReadyToPopulateChanged: {
        if (readyToPopulate) {
            facebookEvent.load(container.nodeIdentifier)
        }
    }

    Connections {
        target: container.facebookMe
        onNodeChanged: {
            if (container.facebookMe.node) {
                facebookEvent.myName = container.facebookMe.node.name
                facebookEvent.userId = container.facebook.currentUserIdentifier
            }
        }
    }

    FacebookEvent {
        id: facebookEvent
        accessToken: container.facebook.accessToken
        onLoaded: {
            container.facebookUser.nodeIdentifier = ownerId
            if (container.facebookMe.node) {
                myName = container.facebookMe.node.name
                userId = container.facebook.currentUserIdentifier
            }
        }
    }

    SilicaListView {
        id: view
        anchors.fill: parent
        spacing: Theme.paddingLarge

        header: SocialEventContent {
            width: view.width
            //: Page header indicating this page show event invitaion
            //% "Event invitation"
            title: qsTrId("lipstick-jolla-home-facebook-la-event_invitation")
            connectedToNetwork: container.connectedToNetwork
            avatar: container.facebookUser
                      && container.facebookUser.node !== null
                      && container.facebookUser.node.picture !== null ? container.facebookUser.node.picture.url : ""
            source: facebookEvent.ownerName
            timestamp: model.timestamp
            body: facebookEvent.name
            description: facebookEvent.description
            startTime: facebookEvent.startTime
            endTime: facebookEvent.endTime
            location: facebookEvent.location
            rsvpStatus: container.hosting ?
                        //: User is hosting the Facebook event in question
                        //% "You are hosting"
                        qsTrId("lipstick-jolla-home-facebook-la-user_is_hosting_event") : facebookEvent.rsvpString
            personsGoing: facebookEvent.personsGoing
            eventImageSource: facebookEvent.picture

            fullRowSocialButtons: Item {
                width: view.width
                height: visible ? attendButton.height : 0
                visible: !container.hosting

                SocialToggleButton {
                    id: attendButton
                    width: parent.width / 3
                    enabled: facebookEvent.rsvpStatus.length > 0 && container.connectedToNetwork
                    locked: facebookEvent.rsvpStatus === "attending"
                    //: Press button to attend a Facebook event
                    //% "Attend"
                    text: qsTrId("lipstick-jolla-home-facebook-la-attend_event")
                    onClicked: if (!locked) facebookEvent.attend()
                }

                SocialToggleButton {
                    width: parent.width / 3
                    enabled: facebookEvent.rsvpStatus.length > 0 && container.connectedToNetwork
                    anchors.horizontalCenter: parent.horizontalCenter
                    locked: facebookEvent.rsvpStatus === "maybe"
                    //: Press button to maybe attend a facebook event
                    //% "Maybe"
                    text: qsTrId("lipstick-jolla-home-facebook-la-maybe_attend_event")
                    onClicked: if (!locked) facebookEvent.maybe()
                }

                SocialToggleButton {
                    width: parent.width / 3
                    enabled: facebookEvent.rsvpStatus.length > 0 && container.connectedToNetwork
                    anchors.right: parent.right
                    locked: facebookEvent.rsvpStatus === "declined"
                    //: Press button to decline Facebook event
                    //% "Decline"
                    text: qsTrId("lipstick-jolla-home-facebook-la-maybe_decline_event")
                    onClicked: if (!locked) facebookEvent.decline()
                }
            }
        }

        VerticalScrollDecorator {}

        FacebookAccountMenu {
            link: container.model.link
            pageContainer: container.pageContainer
            onCurrentAccountChanged: container.account.setIdentifiers(currentAccount)
        }
    }
}
