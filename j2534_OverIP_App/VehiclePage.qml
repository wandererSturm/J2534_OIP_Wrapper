import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

import de.xplatforms.j2534 1.0

// Vehicle PC: shares the J2534 adapter connected to this PC. Shows the
// connection code for the Diagnostic PC and asks before anyone uses it.
Pane
{
    id: page
    property int name_id: 10

    readonly property int sharingState: VehicleSharing.state
    readonly property bool sharing: sharingState !== VehicleSharing.Idle && sharingState !== VehicleSharing.Failed
    readonly property string driverProblem: VehicleSharing.driverPath === "" ? "" : VehicleSharing.checkDriver(VehicleSharing.driverPath)
    property string browsedDriver: ""

    readonly property color accent: "#ca5010"

    background: Rectangle { color: "white" }

    // [{ text, path }]: installed drivers, the saved/browsed DLL, then "Browse…" (path "").
    function driverEntries()
    {
        var entries = []
        var paths = []
        var drivers = VehicleSharing.drivers
        for (var i = 0; i < drivers.length; ++i) {
            entries.push({ text: drivers[i].name + "  —  " + drivers[i].path, path: drivers[i].path })
            paths.push(drivers[i].path.toLowerCase())
        }
        var extra = [VehicleSharing.driverPath, page.browsedDriver]
        for (var j = 0; j < extra.length; ++j) {
            if (extra[j] !== "" && paths.indexOf(extra[j].toLowerCase()) < 0) {
                entries.push({ text: extra[j], path: extra[j] })
                paths.push(extra[j].toLowerCase())
            }
        }
        entries.push({ text: "Browse…", path: "" })
        return entries
    }

    function indexOfDriver(path)
    {
        var entries = driverEntries()
        for (var i = 0; i < entries.length; ++i) {
            if (path !== "" && entries[i].path.toLowerCase() === path.toLowerCase())
                return i
        }
        return -1
    }

    FileDialog
    {
        id: driverDialog
        title: "Choose the adapter's 32-bit J2534 driver DLL"
        nameFilters: ["J2534 driver (*.dll)", "All files (*)"]
        onAccepted:
        {
            var path = decodeURIComponent(driverDialog.selectedFile.toString().replace(/^file:\/\/\//, "")).replace(/\//g, "\\")
            page.browsedDriver = path
            VehicleSharing.driverPath = path
            driverCombo.currentIndex = page.indexOfDriver(path)
        }
        onRejected: driverCombo.currentIndex = page.indexOfDriver(VehicleSharing.driverPath)
    }

    component Section: Frame
    {
        id: section
        default property alias content: column.data
        property color borderColor: "#d0d0d0"
        property color fillColor: "white"

        Layout.fillWidth: true
        Layout.leftMargin: 24
        Layout.rightMargin: 24
        padding: 16

        background: Rectangle { radius: 6; color: section.fillColor; border.color: section.borderColor }

        ColumnLayout
        {
            id: column
            anchors.left: parent.left
            anchors.right: parent.right
            spacing: 10
        }
    }

    ScrollView
    {
        id: scroll
        anchors.fill: parent
        contentWidth: availableWidth

        ColumnLayout
        {
            width: scroll.availableWidth
            spacing: 16

            RowLayout
            {
                Layout.fillWidth: true
                Layout.leftMargin: 24
                Layout.rightMargin: 24
                Layout.topMargin: 24

                Label
                {
                    Layout.fillWidth: true
                    text: "Share this PC's J2534 adapter"
                    font.pixelSize: 22
                    font.bold: true
                }

                Button
                {
                    text: "This is not the Vehicle PC"
                    flat: true
                    onClicked: VehicleSharing.leaveVehicleMode()
                }
            }

            Label
            {
                Layout.fillWidth: true
                Layout.leftMargin: 24
                Layout.rightMargin: 24
                wrapMode: Text.Wrap
                color: "#555555"
                text: "Connect the adapter to the car and to this PC, start sharing, and send the connection code to the person "
                      + "at the Diagnostic PC. The connection goes through Cloudflare, so neither PC needs a static IP address "
                      + "or open ports. You'll be asked before anyone uses the adapter."
            }

            // Missing parts of the installation
            Label
            {
                Layout.fillWidth: true
                Layout.leftMargin: 24
                Layout.rightMargin: 24
                visible: !VehicleSharing.cloudflaredFound || !VehicleSharing.deviceHostFound
                wrapMode: Text.Wrap
                color: "#c42b1c"
                text: (!VehicleSharing.cloudflaredFound
                       ? "cloudflared.exe was not found next to the App or on PATH. " : "")
                      + (!VehicleSharing.deviceHostFound
                         ? "j2534_device_host.exe was not found (it belongs in the x86 folder next to the App). " : "")
                      + "Reinstall with the \"Vehicle PC\" component."
            }

            // 1. Adapter
            Section
            {
                Label
                {
                    text: "1. Adapter"
                    font.bold: true
                    font.pixelSize: 16
                }

                Label
                {
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    text: VehicleSharing.drivers.length > 0
                          ? "J2534 driver of the adapter connected to this PC:"
                          : "No J2534 drivers are registered on this PC. Install the adapter's driver, or browse for its DLL:"
                }

                ComboBox
                {
                    id: driverCombo
                    Layout.fillWidth: true
                    enabled: !page.sharing
                    model: page.driverEntries()
                    textRole: "text"
                    displayText: currentIndex < 0 ? "Choose the adapter's driver…" : currentText
                    Component.onCompleted: currentIndex = page.indexOfDriver(VehicleSharing.driverPath)
                    onActivated: function(index)
                    {
                        var entry = page.driverEntries()[index]
                        if (entry.path === "")
                            driverDialog.open()
                        else
                            VehicleSharing.driverPath = entry.path
                    }
                }

                Label
                {
                    Layout.fillWidth: true
                    visible: page.driverProblem !== ""
                    wrapMode: Text.Wrap
                    color: "#c42b1c"
                    text: page.driverProblem
                }
            }

            // 2. Sharing
            Section
            {
                borderColor: page.sharing ? page.accent : "#d0d0d0"

                RowLayout
                {
                    Layout.fillWidth: true

                    Label
                    {
                        Layout.fillWidth: true
                        text: "2. Connection"
                        font.bold: true
                        font.pixelSize: 16
                    }

                    Button
                    {
                        text: page.sharing ? "Stop sharing" : "Start sharing"
                        highlighted: !page.sharing
                        enabled: page.sharing || (VehicleSharing.driverPath !== "" && page.driverProblem === "")
                        onClicked: page.sharing ? VehicleSharing.stop() : VehicleSharing.start()
                    }
                }

                // Status line
                RowLayout
                {
                    Layout.fillWidth: true
                    spacing: 8

                    BusyIndicator
                    {
                        visible: running
                        running: page.sharingState === VehicleSharing.Starting
                                 || page.sharingState === VehicleSharing.Waiting
                        Layout.preferredWidth: 28
                        Layout.preferredHeight: 28
                    }

                    Label
                    {
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                        color: page.sharingState === VehicleSharing.Failed ? "#c42b1c"
                             : page.sharingState === VehicleSharing.Connected ? "#107c10" : "#333333"
                        font.bold: page.sharingState === VehicleSharing.Connected
                        text: {
                            switch (page.sharingState) {
                            case VehicleSharing.Idle: return "Not sharing."
                            case VehicleSharing.Starting: return "Opening a tunnel through Cloudflare…"
                            case VehicleSharing.Waiting: return "Waiting for the Diagnostic PC to connect…"
                            case VehicleSharing.AskingToAllow: return VehicleSharing.clientName + " wants to use the adapter."
                            case VehicleSharing.Connected: return VehicleSharing.clientName + " is using " + VehicleSharing.adapterName + "."
                            case VehicleSharing.Failed: return VehicleSharing.errorText
                            }
                            return ""
                        }
                    }
                }

                // Connection code
                ColumnLayout
                {
                    Layout.fillWidth: true
                    visible: VehicleSharing.connectionCode !== ""
                    spacing: 6

                    Label
                    {
                        text: "Connection code for the Diagnostic PC:"
                        color: "#555555"
                    }

                    RowLayout
                    {
                        Layout.fillWidth: true
                        spacing: 12

                        TextField
                        {
                            id: codeField
                            Layout.fillWidth: true
                            readOnly: true
                            selectByMouse: true
                            text: VehicleSharing.connectionCode
                            font.family: "Consolas"
                            font.pixelSize: 20
                        }

                        Button
                        {
                            id: copyButton
                            property bool copied: false
                            text: copied ? "Copied ✓" : "Copy"
                            onClicked:
                            {
                                VehicleSharing.copyCode()
                                copied = true
                                copiedTimer.restart()
                            }
                            Timer { id: copiedTimer; interval: 2000; onTriggered: copyButton.copied = false }
                        }
                    }

                    Label
                    {
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                        color: "#555555"
                        text: "Send it to the person at the Diagnostic PC (chat, e-mail…). It is valid until you stop sharing; "
                              + "a new one is made every time you start."
                    }
                }

                // Allow / Reject
                Frame
                {
                    Layout.fillWidth: true
                    visible: page.sharingState === VehicleSharing.AskingToAllow
                    padding: 12
                    background: Rectangle { radius: 6; color: "#fff4ce"; border.color: page.accent; border.width: 2 }

                    RowLayout
                    {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        spacing: 12

                        Label
                        {
                            Layout.fillWidth: true
                            wrapMode: Text.Wrap
                            text: "<b>" + VehicleSharing.clientName + "</b> has the right code and wants to use the adapter. "
                                  + "Allow it only if you expect this connection."
                        }

                        Button
                        {
                            text: "Allow"
                            highlighted: true
                            onClicked: VehicleSharing.allow()
                        }

                        Button
                        {
                            text: "Reject"
                            onClicked: VehicleSharing.reject()
                        }
                    }
                }

                Label
                {
                    Layout.fillWidth: true
                    visible: page.sharingState === VehicleSharing.Connected
                    wrapMode: Text.Wrap
                    color: "#555555"
                    text: "Keep the adapter connected and the ignition as the Diagnostic PC needs it. "
                          + "\"Stop sharing\" ends the session and closes the adapter."
                }
            }

            // Log
            Button
            {
                Layout.leftMargin: 18
                flat: true
                text: logArea.visible ? "▾ Hide details" : "▸ Show details"
                onClicked: logArea.visible = !logArea.visible
            }

            TextArea
            {
                id: logArea
                Layout.fillWidth: true
                Layout.leftMargin: 24
                Layout.rightMargin: 24
                Layout.bottomMargin: 24
                visible: false
                readOnly: true
                selectByMouse: true
                wrapMode: TextEdit.Wrap
                font.family: "Consolas"
                text: VehicleSharing.log.join("\n")
            }
        }
    }
}
