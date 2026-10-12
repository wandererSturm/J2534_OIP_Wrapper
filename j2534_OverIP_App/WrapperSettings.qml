import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import de.xplatforms.j2534 1.0

Item {

    Component.onCompleted: {
        ExJ2534Wrapper.exportToJSON()
    }

    ColumnLayout
    {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        Label{
            text: "SETTINGS"
        }

        InterfaceRow {}

        Label
        {
            text: "Wrapped driver: " + (InterfaceConfig.wrappedDll === "" ? "not set" : InterfaceConfig.wrappedDll)
            color: "#555555"
        }

        Item { Layout.fillHeight: true }
    }

}
