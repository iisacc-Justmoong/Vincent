import QtQuick
import QtQuick.Dialogs as Dialogs
import QtQuick.Layouts
import LVRS 1.0 as LV

LV.Window {
    id: preferencesWindow

    objectName: "preferencesWindow"
    visible: false
    width: 480
    height: 360
    title: qsTr("Preferences")

    readonly property url profileImageSource: VincentProfileImageProcessor.imageSource
    property alias profileName: profileNameField.text
    property alias canInviteOtherUsers: inviteOtherUsersCheckBox.checked

    Dialogs.FileDialog {
        id: profileImageDialog

        title: qsTr("Choose profile image")
        nameFilters: [qsTr("Image files (*.png *.jpg *.jpeg *.bmp *.webp)")]
        onAccepted: VincentProfileImageProcessor.processProfileImage(selectedFile)
    }

    Component {
        id: profileImageMenuItemDelegate

        LV.MenuItem {
            property var modelData: ({})

            label: modelData.label || ""
            itemWidth: profileImageMenu.minimumItemWidth
            state: modelData.state === undefined ? defaultState : modelData.state
            keyVisible: false
            showIconSlot: false
            showChevron: false
            hasChildItems: false
            enabled: modelData.enabled !== false
            Accessible.name: label
            onClicked: modelData.trigger()
        }
    }

    LV.ContextMenu {
        id: profileImageMenu

        itemDelegate: profileImageMenuItemDelegate
        showIconSlot: false
        items: [
            {
                label: qsTr("Select profile image"),
                action: "select",
                showChevron: false
            },
            {
                label: qsTr("Delete profile image"),
                action: "delete",
                showChevron: false,
                enabled: preferencesWindow.profileImageSource.toString().length > 0
            }
        ]
        onItemTriggered: function (index, item) {
            if (item.action === "select") {
                Qt.callLater(function () {
                    profileImageDialog.open();
                });
                return;
            }
            if (item.action === "delete") {
                VincentProfileImageProcessor.clearProfileImage();
            }
        }
    }

    LV.HStack {
        id: profileSectionHeader

        anchors.top: parent.top
        anchors.topMargin: LV.Theme.gap24
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: LV.Theme.gap6

        Image {
            objectName: "profileSectionIcon"
            source: LV.Theme.iconPath("user")
            sourceSize.width: LV.Theme.iconSm
            sourceSize.height: LV.Theme.iconSm
            smooth: true
            mipmap: LV.RenderQuality.mipmapEnabled
            Layout.preferredWidth: LV.Theme.iconSm
            Layout.preferredHeight: LV.Theme.iconSm
            Layout.alignment: Qt.AlignVCenter
            Accessible.ignored: true
        }

        LV.Label {
            objectName: "profileSectionTitle"
            text: qsTr("Profile")
            style: title2
            Layout.alignment: Qt.AlignVCenter
        }
    }

    LV.VStack {
        id: profileSettings

        anchors.top: profileSectionHeader.bottom
        anchors.topMargin: LV.Theme.gap20
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: LV.Theme.gap16

        LV.VStack {
            spacing: LV.Theme.gap6

            LV.Label {
                text: qsTr("Profile image")
                style: description
            }

            LV.IconButton {
                id: profileImageButton

                readonly property int avatarSize: LV.Theme.scaleMetric(64)
                readonly property int avatarInset: Math.max(0, Math.round((avatarSize - iconSize) / 2))

                tone: LV.AbstractButton.Borderless
                shapeStyle: profileImageButton.shapeCylinder
                horizontalPadding: avatarInset
                verticalPadding: avatarInset
                iconName: preferencesWindow.profileImageSource.toString().length === 0 ? "user" : ""
                iconSource: preferencesWindow.profileImageSource
                iconSize: LV.Theme.scaleMetric(44)
                implicitWidth: avatarSize
                implicitHeight: avatarSize
                Layout.preferredWidth: avatarSize
                Layout.preferredHeight: avatarSize
                Accessible.name: qsTr("Profile image options")
                onClicked: profileImageMenu.openFor(profileImageButton, 0, profileImageButton.height)
            }
        }

        LV.VStack {
            alignment: Qt.AlignLeft
            spacing: LV.Theme.gap6

            LV.Label {
                text: qsTr("Profile name")
                style: description
            }

            LV.InputField {
                id: profileNameField

                placeholder: qsTr("Profile name")
                Accessible.name: qsTr("Profile name")
            }
        }

        LV.CheckBox {
            id: inviteOtherUsersCheckBox

            text: qsTr("Allow inviting other users")
            checked: false
        }
    }
}
