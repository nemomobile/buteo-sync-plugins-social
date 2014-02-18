import QtQuick 2.0

QtObject {
    id: facebookEvent

    property string eventId
    property string userId
    property string accessToken
    property string name
    property string description
    property string startTime
    property string endTime
    property string ownerId
    property string ownerName
    property string location
    property string picture
    property string rsvpString
    property string rsvpStatus
    property string personsGoing
    property string myName

    signal loaded()

    onMyNameChanged: {
        if (myName !== "") {
            queryPersonsGoing()
        }
    }

    onUserIdChanged: checkRsvpStatus("attending")

    function load() {
        if (accessToken === "")
            return

        var doc = new XMLHttpRequest()
        doc.onreadystatechange = function() {
            if (doc.readyState === XMLHttpRequest.DONE) {
                if (doc.status === 200) {
                    var event = JSON.parse(doc.responseText)
                    facebookEvent.ownerId = event.owner.id
                    facebookEvent.ownerName = event.owner.name
                    facebookEvent.name = event.name
                    facebookEvent.description = event.description
                    facebookEvent.startTime = event.start_time
                    facebookEvent.endTime = typeof event.end_time !== "undefined" ? event.end_time : ""
                    facebookEvent.location = typeof event.location !== "undefined" ? event.location : ""
                    if (typeof event.picture !== "undefined" && !event.picture.data.is_silhouette) {
                        facebookEvent.picture = "https://graph.facebook.com/" + eventId + "/picture?type=large&access_token=" + accessToken
                    }
                    facebookEvent.loaded()
                } else {
                    console.log("Failed to load Facebook event, error: " + doc.status)
                }
            }
        }

        var postData = "access_token=" + accessToken
        var url = "https://graph.facebook.com/" + eventId + "?fields=id,description,name,picture,end_time,start_time,location,owner&access_token=" + accessToken
        doc.open("GET", url)
        doc.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded')
        doc.setRequestHeader('Content-Length', postData.length)
        doc.send(postData)
    }

    function rsvpEvent(rsvpAction) {
        if (accessToken === "" || eventId === "")
            return

        var doc = new XMLHttpRequest()
        doc.onreadystatechange = function() {
            if (doc.readyState === XMLHttpRequest.DONE) {
                if (doc.status === 200) {
                    setRsvpStatusString(rsvpAction)
                } else {
                    console.log("Failed to set Facebook event rsvp status, error: " + doc.status)
                }
            }
        }

        var postData = "access_token=" + accessToken
        var url = "https://graph.facebook.com/" + eventId + "/" + rsvpAction + "?" + "access_token=" + accessToken
        doc.open("POST", url)
        doc.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded')
        doc.setRequestHeader('Content-Length', postData.length)
        doc.send(postData)
    }

    function attend() {
        rsvpEvent("attending")
    }

    function maybe() {
        rsvpEvent("maybe")
    }

    function decline() {
        rsvpEvent("declined")
    }

    function checkRsvpStatus(rsvpAction) {
        if (accessToken === "" || eventId === "" || userId === "")
            return

        var doc = new XMLHttpRequest()
        doc.onreadystatechange = function() {
            if (doc.readyState === XMLHttpRequest.DONE) {
                if (doc.status === 200) {
                    var parsed = JSON.parse(doc.responseText)
                    if (parsed.data.length > 0) {
                        setRsvpStatusString(rsvpAction)
                    } else {
                        if (rsvpAction === "attending") {
                            checkRsvpStatus("maybe")
                        } else if (rsvpAction === "maybe") {
                            checkRsvpStatus("declined")
                        } else {
                            //: Indicates that a person has not responded to Facebook event invitation.
                            //% "You have not responded"
                            rsvpString = qsTrId("lipstick-jolla-home-la-facebook_event_not_responded")
                            rsvpStatus = "notresponded"
                        }
                    }
                } else {
                    console.log("Facebook rsvp status check error: " + doc.status)
                }
            }
        }

        var postData = "access_token=" + accessToken
        var url = "https://graph.facebook.com/" + eventId + "/" + rsvpAction + "/" + userId + "?access_token=" + accessToken
        doc.open("GET", url)
        doc.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded')
        doc.setRequestHeader('Content-Length', postData.length)
        doc.send(postData)
    }

    function queryPersonsGoing(rsvpAction) {
        if (accessToken === "" || eventId === "")
            return

        var doc = new XMLHttpRequest()
        doc.onreadystatechange = function() {
            if (doc.readyState === XMLHttpRequest.DONE) {
                if (doc.status === 200) {
                    var parsed = JSON.parse(doc.responseText)

                    //: Number of persons attending Facebook event
                    //% "%1 attending"
                    facebookEvent.personsGoing = qsTrId("lipstick-jolla-home-facebook_la_persons_attending").arg(parsed.data.length)
                } else {
                    console.log("Facebook event query attending count error: " + doc.status)
                }
            }
        }

        var postData = "access_token=" + accessToken
        var url = "https://graph.facebook.com/" + eventId + "/attending?access_token=" + accessToken
        doc.open("GET", url)
        doc.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded')
        doc.setRequestHeader('Content-Length', postData.length)
        doc.send(postData)
    }

    function setRsvpStatusString(rsvpAction) {
        if (rsvpAction === "attending") {
            //: Indicates that a person is going to attend Facebook event.
            //% "You are going"
            rsvpString = qsTrId("lipstick-jolla-home-la-facebook_event_attending")
            rsvpStatus = "attending"
        } else if (rsvpAction === "maybe") {
            //: Indicates that a person may be attending Facebook event.
            //% "You may be going"
            rsvpString = qsTrId("lipstick-jolla-home-la-facebook_event_maybe_attending")
            rsvpStatus = "maybe"
        } else {
            //: Indicates that a person has declined to attend Facebook event.
            //% "You have declined"
            rsvpString = qsTrId("lipstick-jolla-home-la-facebook_event_declined")
            rsvpStatus = "declined"
        }
    }
}
