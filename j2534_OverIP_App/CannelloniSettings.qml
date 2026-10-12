import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import de.xplatforms.j2534 1.0

Rectangle
{
    Layout.fillHeight: true
    Layout.fillWidth: true

    color: "white"

    ColumnLayout
    {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        Label
        {
            text: "Cannelloni settings"
            font.pixelSize: 18
            font.bold: true
        }

        InterfaceRow {}

        Label
        {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            color: "#555555"
            text: ExJ2534Cannelloni.settingsFound
                  ? "Values the PassThru DLL reads when a host application opens a Cannelloni device."
                  : "No Cannelloni settings in the registry. The PassThru DLL uses these defaults."
        }

        GridLayout
        {
            columns: 2
            columnSpacing: 24
            rowSpacing: 6

            Label { text: "Default host"; color: "#555555" }
            Label { text: ExJ2534Cannelloni.defaultHost; font.family: "Consolas" }

            Label { text: "Default port"; color: "#555555" }
            Label { text: ExJ2534Cannelloni.defaultPort; font.family: "Consolas" }

            Label { text: "Cannelloni data version"; color: "#555555" }
            Label { text: ExJ2534Cannelloni.dataVersion; font.family: "Consolas" }
        }

        Label
        {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            color: "#555555"
            text: "Ports 20000 (125 kbit, pins 3/11), 20001 (500 kbit, pins 6/14) and 20002 (500 kbit, pins 12/13) "
                  + "can also be chosen per device by putting the port number in the PassThruOpen name.\n"
                  + "To change the defaults, edit DefaultHost (string), DefaultPort and CannelloniDataVersion (DWORD) in:\n"
                  + ExJ2534Cannelloni.settingsLocation
        }

        RowLayout
        {
            spacing: 12

            Button
            {
                text: "Export capture"
                onClicked:
                {
                    var path = ExJ2534Cannelloni.exportToJSON()
                    exportStatus.text = path.length > 0 ? "Saved to " + path : "Export failed, see the log."
                }
            }

            Label
            {
                id: exportStatus
                color: "#555555"
            }
        }

        Item { Layout.fillHeight: true }
    }
}
