import QtQuick 2.0
import Sailfish.Silica 1.0
import Sailfish.Accounts 1.0

PullDownMenu {
    id: container
    property int currentAccount
    property int currentAccountIndex: -1
    property var metaData
    property string selectAccountString
    property string changeToAccountString
    property string accountString
    property var pageContainer
    onMetaDataChanged: refreshAccountList()

    // We distinguish internal.index and container.currentAccountIndex
    // It is because internal.accounts is not matching the accounts
    // data carried by the metadata. This is because accounts might
    // got deleted between two sync, and that metadata contains
    // data that should not exist.
    //
    // internal.index stores the index of the current account in
    // internal.account, while container.currentAccountIndex
    // is used to store the index for the metadata. If metadata
    // carries also information for (for example) avatar, that
    // are indexed, then container.currentAccountIndex contains
    // the good index, even if accounts got removed
    function refreshAccountList() {
        internal.accounts = []
        for (var i = 0; i < container.metaData["accountIdCount"]; i++) {
            var accountData = new Object
            accountData["id"] = container.metaData["accountId" + i]
            var account = accountManager.account(accountData["id"])
            if (account != null) {
                accountData["name"] = account.displayName
                accountData["index"] = i
                internal.accounts.push(accountData)
            }
        }
        internal.accountCount = internal.accounts.length
        container.currentAccount = internal.accounts[0]["id"]
        container.currentAccountIndex = internal.accounts[0]["index"]

        if (internal.accountCount == 2) {
            internal.otherIndex = 1
        }
    }
    resources: [
        QtObject {
            id: internal
            function setIndex(index) {
                internal.index = index
                container.currentAccountIndex = internal.accounts[index]["index"]
                pageContainer.pop()
            }
            property int accountCount
            property var accounts
            property int index
            property int otherIndex
        },
        AccountManager {
            id: accountManager
        }
    ]

    MenuItem {
        text: {
            if (internal.accountCount > 2) {
                container.selectAccountString
            } else if (internal.accountCount == 2) {
                container.changeToAccountString.arg(internal.accounts[internal.otherIndex]["name"])
            } else {
                ""
            }
        }
        visible: internal.accountCount > 1

        onClicked: {
            if (internal.accountCount > 2) {
                var page = container.pageContainer.push(Qt.resolvedUrl("SocialAccountPage.qml"),
                                                        {"accounts": internal.accounts,
                                                         "currentIndex": internal.index,
                                                         "headerText": container.selectAccountString})
                page.indexSelected.connect(internal.setIndex)
            } else {
                internal.index = internal.otherIndex
                container.currentAccount = internal.accounts[internal.index]["id"]
                container.currentAccountIndex = internal.accounts[internal.index]["index"]
                internal.otherIndex = (internal.otherIndex == 0 ? 1 : 0)
            }
        }
    }

    MenuLabel {
        text: container.accountString.arg(internal.accounts[internal.index]["name"])
    }
}
