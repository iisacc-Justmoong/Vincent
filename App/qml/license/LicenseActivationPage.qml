import QtQuick
import QtQuick.Layouts
import LVRS 1.0 as LV

Item {
    id: activationPage

    function submitLicense() {
        if (!VincentLicenseManager.verifying) {
            VincentLicenseManager.validateLicense(emailInput.text, licenseKeyInput.text);
        }
    }

    function resultMessage() {
        if (VincentLicenseManager.resultCode === "invalid_input") {
            return qsTr("Enter the verified account email and the complete Vincent license key.");
        }
        if (VincentLicenseManager.resultCode === "invalid_license") {
            return qsTr("That email and license key do not match an active Vincent license.");
        }
        if (VincentLicenseManager.resultCode === "verification_unavailable") {
            return VincentLicenseManager.hasStoredLicense ? qsTr("License verification is temporarily unavailable. Your saved license is safe; reconnect and try again.") : qsTr("License verification is temporarily unavailable. Check your connection and try again.");
        }
        if (VincentLicenseManager.resultCode === "secure_storage_unavailable") {
            return qsTr("Secure credential storage is unavailable. You can continue this session, but Vincent cannot remember the license.");
        }
        if (VincentLicenseManager.resultCode === "stored_license_removed") {
            return qsTr("The saved license was unreadable and has been removed. Enter it again to continue.");
        }
        return "";
    }

    Rectangle {
        anchors.fill: parent
        color: LV.Theme.window
    }

    LV.AppCard {
        id: activationCard
        objectName: "licenseActivationCard"
        anchors.centerIn: parent
        title: VincentLicenseManager.hasStoredLicense ? qsTr("Verify saved license") : qsTr("Activate Vincent")
        subtitle: VincentLicenseManager.hasStoredLicense ? qsTr("Reconnect to verify the license already saved on this device.") : qsTr("Verify this copy before opening the canvas.")

        ColumnLayout {
            width: parent.width
            height: implicitHeight
            spacing: LV.Theme.gap12

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: LV.Theme.controlHeightMd
                radius: LV.Theme.radiusControl
                color: LV.Theme.subSurface

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: LV.Theme.gap12
                    anchors.rightMargin: LV.Theme.gap12

                    LV.Label {
                        text: qsTr("Product")
                        style: description
                        color: LV.Theme.textSecondary
                    }

                    Item {
                        Layout.fillWidth: true
                    }

                    LV.Label {
                        objectName: "licenseProductName"
                        text: qsTr("Vincent")
                        style: body
                        color: LV.Theme.textPrimary
                    }
                }
            }

            LV.Label {
                Layout.fillWidth: true
                visible: !VincentLicenseManager.hasStoredLicense
                text: qsTr("Use the verified email from your iisacc account and the license key in your receipt or private dashboard.")
                style: description
                color: LV.Theme.textSecondary
                wrapMode: Text.WordWrap
            }

            LV.InputField {
                id: emailInput
                objectName: "licenseEmailInput"
                Layout.fillWidth: true
                visible: !VincentLicenseManager.hasStoredLicense
                enabled: !VincentLicenseManager.verifying
                placeholderText: qsTr("Verified account email")
                maximumLength: 254
                inputMethodHints: Qt.ImhEmailCharactersOnly | Qt.ImhNoAutoUppercase
                clearButtonVisible: true
                Accessible.name: qsTr("Verified account email")
                onAccepted: licenseKeyInput.forceInputFocus()
            }

            LV.InputField {
                id: licenseKeyInput
                objectName: "licenseKeyInput"
                Layout.fillWidth: true
                visible: !VincentLicenseManager.hasStoredLicense
                enabled: !VincentLicenseManager.verifying
                placeholderText: qsTr("IIL… license key")
                maximumLength: 64
                echoMode: TextInput.Password
                passwordMaskDelay: 0
                inputMethodHints: Qt.ImhSensitiveData | Qt.ImhNoPredictiveText
                clearButtonVisible: true
                Accessible.name: qsTr("Vincent license key")
                onAccepted: activationPage.submitLicense()
            }

            LV.Label {
                Layout.fillWidth: true
                visible: VincentLicenseManager.verifying || activationPage.resultMessage().length > 0
                text: VincentLicenseManager.verifying ? qsTr("Verifying securely…") : activationPage.resultMessage()
                style: description
                color: VincentLicenseManager.resultCode === "invalid_license" || VincentLicenseManager.resultCode === "invalid_input" ? LV.Theme.danger : LV.Theme.textSecondary
                wrapMode: Text.WordWrap
            }

            LV.LabelButton {
                objectName: "activateLicenseButton"
                Layout.fillWidth: true
                visible: !VincentLicenseManager.hasStoredLicense
                text: VincentLicenseManager.verifying ? qsTr("Verifying…") : qsTr("Activate Vincent")
                enabled: !VincentLicenseManager.verifying && emailInput.text.trim().length > 0 && licenseKeyInput.text.trim().length > 0
                Accessible.name: qsTr("Activate Vincent")
                onClicked: activationPage.submitLicense()
            }

            LV.LabelButton {
                objectName: "retryStoredLicenseButton"
                Layout.fillWidth: true
                visible: VincentLicenseManager.hasStoredLicense
                text: VincentLicenseManager.verifying ? qsTr("Verifying…") : qsTr("Retry saved license")
                enabled: !VincentLicenseManager.verifying
                Accessible.name: qsTr("Retry saved Vincent license")
                onClicked: VincentLicenseManager.retryStoredLicense()
            }

            LV.LabelButton {
                objectName: "forgetStoredLicenseButton"
                Layout.alignment: Qt.AlignHCenter
                visible: VincentLicenseManager.hasStoredLicense
                enabled: !VincentLicenseManager.verifying
                text: qsTr("Use another license")
                Accessible.name: qsTr("Forget saved license and use another")
                onClicked: {
                    VincentLicenseManager.forgetLicense();
                    Qt.callLater(function () {
                        emailInput.forceInputFocus();
                    });
                }
            }

            LV.Label {
                Layout.fillWidth: true
                visible: !VincentLicenseManager.persistenceSupported
                text: qsTr("Secure license memory is available on macOS and Windows. This platform requires activation each time Vincent starts.")
                style: description
                color: LV.Theme.textTertiary
                wrapMode: Text.WordWrap
            }

            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: LV.Theme.gap4

                LV.LabelButton {
                    text: qsTr("Open account")
                    onClicked: Qt.openUrlExternally("https://iisacc.com/Account/Dashboard")
                }

                LV.Label {
                    text: "·"
                    style: description
                    color: LV.Theme.textTertiary
                }

                LV.LabelButton {
                    text: qsTr("Get Vincent")
                    onClicked: Qt.openUrlExternally("https://iisacc.com/Store/Vincent")
                }
            }
        }
    }

    Connections {
        target: VincentLicenseManager

        function onValidationFinished(valid) {
            if (valid) {
                emailInput.text = "";
                licenseKeyInput.text = "";
            }
        }
    }

    Component.onCompleted: emailInput.forceInputFocus()
}
