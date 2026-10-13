import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import de.xplatforms.j2534 1.0

// "PassThru interface: X [Change…]" - opens the interface chooser.
RowLayout
{
    spacing: 12

    Label
    {
        text: "PassThru interface:"
        color: "#555555"
    }

    Label
    {
        text: InterfaceConfig.displayName(InterfaceConfig.selectedInterface)
        font.bold: true
    }

    Button
    {
        text: "Change…"
        flat: true
        onClicked: InterfaceConfig.requestChooser()
    }
}
