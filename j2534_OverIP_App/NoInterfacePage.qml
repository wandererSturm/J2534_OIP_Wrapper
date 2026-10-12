import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Shown when ExSelectedInterface is not set in the registry: the DLL then
// falls back to ExJ2534SimpleLogger and there is no module view to show.
Pane
{
    id: _no_interface_page
    property int name_id: 0

    background: Rectangle { color: "white" }

    ColumnLayout
    {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 12

        Label
        {
            text: "No PassThru interface selected"
            font.pixelSize: 20
            font.bold: true
        }

        Label
        {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            color: "#555555"
            text: "The registry value ExSelectedInterface is not set, so the PassThru DLL only logs calls "
                  + "(SimpleLogger) and this App has nothing to display.\n"
                  + "Set it from an administrator command prompt, then restart the App and the host application:"
        }

        Repeater
        {
            model: [
                { name: "J2534Cannelloni", what: "CAN over UDP (Cannelloni)" },
                { name: "J2534DllWrapper", what: "wrap and log a real J2534 driver (also set WrappedDll to the driver's DLL path)" },
                { name: "J2534Emulator",   what: "built-in emulator" }
            ]

            delegate: ColumnLayout
            {
                id: option
                Layout.fillWidth: true
                spacing: 2

                required property var modelData
                readonly property string registryKey: "HKLM\\SOFTWARE\\WOW6432Node\\PassThruSupport.04.04\\XplatformsPassThruOverIP"

                Label
                {
                    text: option.modelData.name + " - " + option.modelData.what
                    font.bold: true
                }

                // Read-only and wrapped so the whole command is visible and can be copied.
                TextArea
                {
                    Layout.fillWidth: true
                    readOnly: true
                    selectByMouse: true
                    wrapMode: TextEdit.WrapAnywhere
                    font.family: "Consolas"
                    text: "reg add \"" + option.registryKey + "\" /v ExSelectedInterface /t REG_SZ /d "
                          + option.modelData.name + " /f"
                }
            }
        }

        Item { Layout.fillHeight: true }
    }
}
