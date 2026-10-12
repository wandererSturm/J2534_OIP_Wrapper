import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import de.xplatforms.j2534 1.0

// RX and TX traffic of the wrapped J2534 driver on one page.
Rectangle
{
    Layout.fillHeight: true
    Layout.fillWidth: true

    color: "white"

    SplitView
    {
        anchors.fill: parent
        orientation: Qt.Vertical

        ColumnLayout
        {
            SplitView.fillHeight: true
            SplitView.minimumHeight: 80
            spacing: 0

            Label
            {
                Layout.margins: 6
                text: "(RX) ReadMsg's"
                font.bold: true
            }

            WrapperRXView {}
        }

        ColumnLayout
        {
            SplitView.preferredHeight: parent.height / 2
            SplitView.minimumHeight: 80
            spacing: 0

            Label
            {
                Layout.margins: 6
                text: "(TX) WriteMsg's"
                font.bold: true
            }

            WrapperTXView {}
        }
    }
}
