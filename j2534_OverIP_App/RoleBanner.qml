import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import de.xplatforms.j2534 1.0

// Coloured strip at the top of the window saying which side of a remote
// session this PC is: orange = Vehicle PC (adapter at the car),
// blue = Diagnostic PC (diagnostic program). Hidden otherwise.
Rectangle
{
    id: banner

    readonly property bool vehicle: VehicleSharing.vehicleMode
    readonly property bool diagnostic: !vehicle && InterfaceConfig.selectedModule === 5

    // For the window title
    readonly property string roleName: vehicle ? "Vehicle PC" : (diagnostic ? "Diagnostic PC" : "")

    visible: roleName !== ""
    implicitHeight: visible ? 40 : 0
    color: vehicle ? "#ca5010" : "#0063b1"

    RowLayout
    {
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        spacing: 12

        Label
        {
            text: banner.roleName.toUpperCase()
            color: "white"
            font.bold: true
            font.pixelSize: 16
            font.letterSpacing: 1
        }

        Label
        {
            Layout.fillWidth: true
            elide: Text.ElideRight
            color: "white"
            text: banner.vehicle
                  ? "The J2534 adapter is connected here, at the car. A Diagnostic PC uses it over the internet."
                  : "The diagnostic program runs here. It uses the J2534 adapter of a Vehicle PC over the internet."
        }
    }
}
