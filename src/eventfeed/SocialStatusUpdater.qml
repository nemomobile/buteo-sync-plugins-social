import QtQuick 2.0
import Sailfish.Accounts 1.0
import com.jolla.settings.accounts 1.0

Item {
    id: updater

    property string statusUpdate
    property alias accountId: _account.identifier
    property QtObject keyProvider: _keyProvider
    property QtObject account: _account
    property SignInParameters signInParams
    property variant signInData

    signal postRequestDone(Item item)
    signal postRequestError(Item item)

    StoredKeyProvider {
        id: _keyProvider
    }

    Account {
        id: _account
        function performSignIn() {
            if (updater.signInParams === null) {
                console.log("SocialStatusUpdater: No sign-in params provided.")
                updater.postRequestError(updater)
            } else if (status === Account.Initialized && identifier !== -1) {
                updater.signInParams.setParameter("UiPolicy", SignInParameters.NoUserInteractionPolicy)
                signIn("Jolla", "Jolla", updater.signInParams)
            }
        }

        onStatusChanged: performSignIn()
        onSignInResponse: updater.signInData = data
        onSignInError: {
            console.log("SocialStatusUpdater: sign-in error: " + message + "\n")
            updater.postRequestDone(updater)
        }
    }
}
