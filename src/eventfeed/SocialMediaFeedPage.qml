import QtQuick 2.0
import Sailfish.Silica 1.0
import org.nemomobile.socialcache 1.0
import org.nemomobile.configuration 1.0
import org.nemomobile.connectivity 1.0

Page {
    id: page

    property alias listModel: _listView.model
    property alias listDelegate: _listView.delegate
    property alias socialNetwork: syncHelper.socialNetwork
    property string configKey
    property int timestampRole
    property int unseenPostCount
    property string headerTitle
    property SilicaListView listView: _listView
    property alias updating: syncHelper.loading
    property bool syncNotifications
    property bool connectedToNetwork // ALL other connectedToNetwork properties in other pages
                                     // are bound to this property, directly or indirectly.
    property int refreshTimeCount: 1 // Increment this to trigger feed items to refresh times.

    // -------------------------

    property bool _needToSync
    onConnectedToNetworkChanged: {
        if (page.connectedToNetwork && page._needToSync) {
            page._needToSync = false
            page.sync()
        }
    }
    ConnectionHelper {
        id: connectionHelper
        onNetworkConnectivityEstablished: page.connectedToNetwork = true
        onNetworkConnectivityUnavailable: page.connectedToNetwork = false
    }

    Component.onCompleted: {
        // prefill view with initial content
        if (page.listModel) {
            page.listModel.refresh()
        }

        // set up the connection helper.
        // if we have an available connection, attempt to connect to it.
        if (connectionHelper.haveNetworkConnectivity()) {
            connectionHelper.attemptToConnectNetwork()
        }
    }

    onVisibleChanged: {
        if (visible && status === PageStatus.Active) {
            page.refreshTimeCount = page.refreshTimeCount + 1
        }
    }

    Connections {
        target: page.listModel
        onModelUpdated: page.listUpdated()
    }

    Timer {
        interval: 60000
        running: page.visible && page.status === PageStatus.Active
        repeat: true
        onTriggered: page.refreshTimeCount = page.refreshTimeCount + 1
    }

    onStatusChanged: {
        if (status === PageStatus.Active) {
            page.unseenPostCount = 0
            page.setLastSeenTime()
        } else if (status === PageStatus.Activating) {
            page.refreshTimeCount = page.refreshTimeCount + 1
        }
    }

    SyncHelper {
        id: syncHelper
        dataType: SocialSync.Posts
        onLoadingChanged: {
            if (!loading) {
                if (page.listModel) {
                    page.listModel.refresh()
                }

                if (page.connectedToNetwork) {
                    connectionHelper.closeNetworkSession()
                }
            }
        }
    }

    SyncHelper {
        id: syncNotificationsHelper
        socialNetwork: syncHelper.socialNetwork
        dataType: SocialSync.Notifications
    }

    ConfigurationValue {
        id: _lastSeenTime
        key: page.configKey + "_last_seen_time"
    }

    SilicaListView {
        id: _listView
        anchors.fill: parent
        cacheBuffer: Screen.height
        header: PageHeader {
            title: page.headerTitle
        }

        VerticalScrollDecorator {}
    }

    function sync() {
        if (page.connectedToNetwork) {
            syncHelper.sync()
            if (syncNotifications) {
                syncNotificationsHelper.sync()
            }
        } else {
            if (page.listModel) {
                // we may have old data in the database anyway.
                // attempt to refresh the list model with that data.
                page.listModel.refresh()
            }

            // also attempt to connect to the network,
            // and queue a sync for when (if) it succeeds.
            page._needToSync = true
            connectionHelper.attemptToConnectNetwork()
        }
    }

    function positionViewAtBeginning() {
         _listView.positionViewAtBeginning()
    }

    function listUpdated() {
        if (!visible) {
            var result = 0
            var lastTimestamp = _lastSeenTime.value
            for (var i = 0; i < listModel.count; i++) {
                var rawTimestamp = listModel.getField(i, timestampRole).toString()
                var timestamp = new Date(rawTimestamp).getTime()
                if (timestamp <= lastTimestamp) {
                    break
                }
                result++
            }
            page.unseenPostCount = result
        } else if (status === PageStatus.Active) {
            setLastSeenTime()
        }
    }

    function setLastSeenTime() {
        if (listModel.count > 0) {
            var date = new Date()
            _lastSeenTime.value = date.getTime()
        }
    }
}
