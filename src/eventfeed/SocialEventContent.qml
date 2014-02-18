import QtQuick 2.0
import Sailfish.Silica 1.0

Column {
    id: container

    property Page parentPage
    property alias avatar: socialAvatar.source
    property alias fallbackAvatar: socialAvatar.fallbackSource
    property string source
    property date timestamp
    property alias body: body.text
    property alias description: description.text
    property alias title: headerText.title
    property string startTime
    property string endTime
    property alias location: locationLabel.text
    property alias fullRowSocialButtons: fullRowSocialButtonsContainer.children
    property bool connectedToNetwork
    property alias rsvpStatus: statusLabel.text
    property alias personsGoing: personsGoingLabel.text
    property alias eventImageSource: coverImage.source

    property string _formattedStartTime: Format.formatDate(startTime, Formatter.TimeValue)
    property string _formattedEndTime: Format.formatDate(endTime, Formatter.TimeValue)
    property string _formattedStartDate: Format.formatDate(startTime, Formatter.DateMedium)
    property string _formattedEndDate: Format.formatDate(endTime, Formatter.DateMedium)

    Rectangle {
        id: header
        color: Theme.rgba(Theme.highlightColor, 0.20)
        width: parent.width
        height: Theme.itemSizeLarge

        PageHeader {
            id: headerText
        }

        Label {
            id: sourceLabel
            color: Theme.highlightColor
            font.pixelSize: Theme.fontSizeExtraSmall
            y: header.height - Theme.paddingLarge - Theme.paddingSmall
            anchors {
                right: parent.right
                rightMargin: Theme.paddingLarge
            }
            text: container.source
        }
    }

    Rectangle {
        color: Theme.rgba(Theme.highlightColor, 0.20)
        width: parent.width
        height: Math.max(socialAvatar.height + Theme.paddingLarge, contentColumn.height)

        Behavior on height { FadeAnimation {} }

        SocialAvatar {
            id: socialAvatar
            connectedToNetwork: container.connectedToNetwork
        }

        Column {
            id: contentColumn
            anchors {
                left: socialAvatar.right
                leftMargin: Theme.paddingMedium
                right: parent.right
                rightMargin: Theme.paddingLarge
            }

            SocialLabel {
                id: body
            }

            SocialLabel {
                id: description
                color: Theme.highlightColor
            }

            Label {
                color: Theme.highlightColor
                width: parent.width
                opacity: 0.6
                font.pixelSize: Theme.fontSizeExtraSmall
                y: header.height - Theme.paddingLarge - Theme.paddingSmall
                text: Format.formatDate(container.timestamp, Formatter.DurationElapsed)
            }

            Item {
                width: 1
                height: Theme.paddingMedium
            }
        }
    }

    Rectangle {
        id: details
        color: Theme.rgba(Theme.highlightColor, 0.1)
        width: parent.width
        height: Math.max(coverImage.height, detailsItemsColumn.height)

        Behavior on height { FadeAnimation {} }

        Row {
            height: parent.height

            SocialImage {
                id: coverImage
                width: visible ? Screen.width / 2 : 0
                height: width
                visible: source.toString().length > 0
                connectedToNetwork: container.connectedToNetwork
            }

            Item {
                width: coverImage.visible ? Theme.paddingMedium : Theme.paddingLarge
                height: 1
            }

            Column {
                id: detailsItemsColumn
                width: container.width - coverImage.width - Theme.paddingLarge * 2
                anchors {
                    top: parent.top
                    topMargin: Theme.paddingMedium
                }

                SocialLabel {
                    color: Theme.highlightColor
                    font.pixelSize: Theme.fontSizeExtraSmall
                    text: endDateLabel.visible && !startTimeLabel.visible ? container._formattedStartDate + " -"
                                                                          : container._formattedStartDate
                }

                SocialLabel {
                    id: startTimeLabel
                    color: Theme.highlightColor
                    font.pixelSize: Theme.fontSizeSmall
                    text: endDateLabel.visible ? container._formattedStartTime + " -" : container._formattedStartTime
                }

                Item {
                    width: 1
                    height: Theme.paddingSmall
                }

                SocialLabel {
                    id: endDateLabel
                    color: Theme.highlightColor
                    font.pixelSize: Theme.fontSizeExtraSmall
                    text: container._formattedEndDate
                }

                SocialLabel {
                    color: Theme.highlightColor
                    font.pixelSize: Theme.fontSizeSmall
                    text: container._formattedEndTime
                }

                Item {
                    width: 1
                    height: Theme.paddingSmall
                    visible: locationLabel.visible
                }

                SocialLabel {
                    id: locationLabel
                    color: Theme.highlightColor
                    font.pixelSize: Theme.fontSizeExtraSmall
                }

                Item {
                    width: 1
                    height: Theme.paddingSmall
                }

                SocialLabel {
                    id: personsGoingLabel
                    color: Theme.highlightColor
                    font.pixelSize: Theme.fontSizeExtraSmall
                    maxOpacity: .6
                }

                SocialLabel {
                    id: statusLabel
                    color: Theme.highlightColor
                    font.pixelSize: Theme.fontSizeExtraSmall
                    maxOpacity: .6
                }

                Item {
                    width: 1
                    height: Theme.paddingLarge
                }
            }
        }
    }

    Rectangle {
        id: fullRowSocialButtonsContainer
        color: Theme.rgba(Theme.highlightColor, 0.1)
        width: parent.width
        height: childrenRect.height

        Behavior on height { FadeAnimation {} }
    }
}
