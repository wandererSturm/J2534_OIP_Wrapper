import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import de.xplatforms.j2534 1.0

// Diagnostic PC (interface J2534Remote): the diagnostic program on this PC
// uses the adapter of a Vehicle PC. Only the connection code is set here; the
// DLL reads it when the diagnostic program opens the device.
Pane
{
    id: page
    property int name_id: 5

    property string savedMessage: ""
    readonly property string problem: InterfaceConfig.checkRemoteCode(codeField.text)
    readonly property bool unsaved: codeField.text.trim() !== InterfaceConfig.remoteCode

    background: Rectangle { color: "white" }

    ScrollView
    {
        id: scroll
        anchors.fill: parent
        contentWidth: availableWidth

        ColumnLayout
        {
            width: scroll.availableWidth
            spacing: 16

            Label
            {
                Layout.fillWidth: true
                Layout.leftMargin: 24
                Layout.rightMargin: 24
                Layout.topMargin: 24
                text: "Use a J2534 adapter on another PC"
                font.pixelSize: 22
                font.bold: true
            }

            InterfaceRow
            {
                Layout.leftMargin: 24
            }

            Frame
            {
                Layout.fillWidth: true
                Layout.leftMargin: 24
                Layout.rightMargin: 24
                padding: 16
                background: Rectangle { radius: 6; color: "white"; border.color: "#0063b1"; border.width: 2 }

                ColumnLayout
                {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    spacing: 10

                    Label
                    {
                        text: "Connection code"
                        font.bold: true
                        font.pixelSize: 16
                    }

                    Label
                    {
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                        color: "#555555"
                        text: "The Vehicle PC shows it after \"Start sharing\". It changes every time sharing starts."
                    }

                    RowLayout
                    {
                        Layout.fillWidth: true
                        spacing: 12

                        TextField
                        {
                            id: codeField
                            Layout.fillWidth: true
                            placeholderText: "e.g. brave-cat-xyz#K7P4-9QXM-2M8D"
                            selectByMouse: true
                            font.family: "Consolas"
                            font.pixelSize: 18
                            text: InterfaceConfig.remoteCode
                            onTextEdited: page.savedMessage = ""
                            onAccepted: saveButton.clicked()
                        }

                        Button
                        {
                            text: "Paste"
                            onClicked:
                            {
                                codeField.selectAll()
                                codeField.paste()
                                page.savedMessage = ""
                            }
                        }

                        Button
                        {
                            id: saveButton
                            text: "Save"
                            highlighted: page.unsaved
                            enabled: page.unsaved && page.problem === ""
                            onClicked:
                            {
                                if (InterfaceConfig.saveRemoteCode(codeField.text))
                                    page.savedMessage = "Saved. The diagnostic program connects when it opens the device."
                                else
                                    page.savedMessage = "Couldn't save the code."
                            }
                        }
                    }

                    Label
                    {
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                        color: page.problem !== "" && codeField.text !== "" ? "#c42b1c"
                             : (page.savedMessage !== "" ? "#107c10" : "#555555")
                        text: codeField.text === "" ? ""
                              : page.problem !== "" ? page.problem
                              : page.savedMessage !== "" ? "✓ " + page.savedMessage
                              : "Connects to " + InterfaceConfig.remoteHost(codeField.text) + (page.unsaved ? " (not saved yet)" : "")
                    }
                }
            }

            Label
            {
                Layout.fillWidth: true
                Layout.leftMargin: 24
                Layout.rightMargin: 24
                Layout.bottomMargin: 24
                wrapMode: Text.Wrap
                textFormat: Text.StyledText
                text: "<b>How to connect</b><br>"
                      + "1. On the Vehicle PC (the PC at the car with the adapter), open this App, choose "
                      + "<i>Share this PC's adapter</i> and click <i>Start sharing</i>.<br>"
                      + "2. Paste the code it shows above and click <i>Save</i>.<br>"
                      + "3. Start the diagnostic program and choose the device <i>J2534 Over IP (Xplatforms)</i>.<br>"
                      + "4. The Vehicle PC clicks <i>Allow</i>. The diagnostic program waits up to two minutes for that.<br><br>"
                      + "If opening the device fails, most diagnostic programs show the reason "
                      + "(wrong or old code, Vehicle PC not sharing, or the connection was rejected)."
            }
        }
    }
}
