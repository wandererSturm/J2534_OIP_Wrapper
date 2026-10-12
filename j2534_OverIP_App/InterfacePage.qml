import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

import de.xplatforms.j2534 1.0

// Chooses what the PassThru DLL does when a diagnostic program loads it
// (ExSelectedInterface). Shown on start when nothing is selected, and from the
// "Change…" button in the module views.
Pane
{
    id: page
    property int name_id: 0

    // Driver picked for the DLL Wrapper card
    property string chosenDriver: InterfaceConfig.wrappedDll
    property string browsedDriver: ""
    property string errorText: ""
    property bool showManual: false

    readonly property bool nothingSelected: InterfaceConfig.selectedModule === 0
    readonly property string driverProblem: InterfaceConfig.checkDriver(page.chosenDriver)

    background: Rectangle { color: "white" }

    Connections
    {
        target: InterfaceConfig
        function onApplyFinished(ok, message) { page.errorText = ok ? "" : message }
        function onBusyChanged() { if (InterfaceConfig.busy) page.errorText = "" }
        function onConfigChanged() { driverCombo.currentIndex = page.indexOfDriver(page.chosenDriver) }
    }

    // [{ text, path }] for the driver combo: installed drivers, the current and
    // browsed DLL if they are not installed ones, then "Browse…" (path "").
    function driverEntries()
    {
        var entries = []
        var paths = []
        var drivers = InterfaceConfig.drivers
        for (var i = 0; i < drivers.length; ++i) {
            entries.push({ text: drivers[i].name + "  —  " + drivers[i].path, path: drivers[i].path })
            paths.push(drivers[i].path.toLowerCase())
        }
        var extra = [InterfaceConfig.wrappedDll, page.browsedDriver]
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

    component InterfaceCard: Frame
    {
        id: card

        required property string interfaceName
        required property string title
        required property string description
        property string driver: ""
        property string problem: ""
        default property alias extra: extraContent.data

        readonly property bool current: InterfaceConfig.selectedInterface === card.interfaceName
        // Re-applying the selected Wrapper makes sense when the driver changed.
        readonly property bool changed: !card.current
                                        || (card.interfaceName === "J2534DllWrapper" && card.driver !== InterfaceConfig.wrappedDll)

        // Equal preferred widths so the grid gives every card the same width.
        Layout.fillWidth: true
        Layout.preferredWidth: 1
        Layout.alignment: Qt.AlignTop

        background: Rectangle
        {
            radius: 6
            color: card.current ? "#eef6fc" : "white"
            border.color: card.current ? "#0078d4" : "#d0d0d0"
            border.width: card.current ? 2 : 1
        }

        ColumnLayout
        {
            anchors.left: parent.left
            anchors.right: parent.right
            spacing: 8

            RowLayout
            {
                Layout.fillWidth: true
                Label
                {
                    Layout.fillWidth: true
                    text: card.title
                    font.pixelSize: 16
                    font.bold: true
                }
                Label
                {
                    visible: card.current
                    text: "Selected"
                    color: "#0078d4"
                    font.bold: true
                }
            }

            Label
            {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                color: "#555555"
                text: card.description
            }

            ColumnLayout
            {
                id: extraContent
                Layout.fillWidth: true
            }

            Label
            {
                Layout.fillWidth: true
                visible: card.problem !== ""
                wrapMode: Text.Wrap
                color: "#c42b1c"
                text: card.problem
            }

            Button
            {
                text: card.current && !card.changed ? "In use" : (card.current ? "Apply" : "Use this")
                highlighted: card.changed
                enabled: card.changed && card.problem === "" && !InterfaceConfig.busy
                         && (card.interfaceName !== "J2534DllWrapper" || card.driver !== "")
                onClicked: InterfaceConfig.apply(card.interfaceName, card.driver)
            }
        }
    }

    FileDialog
    {
        id: driverDialog
        title: "Choose a 32-bit J2534 driver DLL"
        nameFilters: ["J2534 driver (*.dll)", "All files (*)"]
        onAccepted:
        {
            var path = decodeURIComponent(driverDialog.selectedFile.toString().replace(/^file:\/\/\//, "")).replace(/\//g, "\\")
            page.browsedDriver = path
            page.chosenDriver = path
            driverCombo.currentIndex = page.indexOfDriver(path)
        }
        onRejected: driverCombo.currentIndex = page.indexOfDriver(page.chosenDriver)
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
                    text: page.nothingSelected ? "No PassThru interface selected" : "PassThru interface"
                    font.pixelSize: 22
                    font.bold: true
                }

                Button
                {
                    visible: page.StackView.view !== null && page.StackView.view.depth > 1
                    text: "Back"
                    flat: true
                    onClicked: page.StackView.view.pop()
                }
            }

            Label
            {
                Layout.fillWidth: true
                Layout.leftMargin: 24
                Layout.rightMargin: 24
                wrapMode: Text.Wrap
                color: "#555555"
                text: (page.nothingSelected
                       ? "The PassThru DLL is only logging calls right now. "
                       : "")
                      + "Choose what it should do when a diagnostic program opens it. "
                      + "Changing this needs administrator rights (you'll see a Windows prompt)."
            }

            GridLayout
            {
                Layout.fillWidth: true
                Layout.leftMargin: 24
                Layout.rightMargin: 24
                columns: scroll.availableWidth > 1000 ? 3 : 1
                columnSpacing: 16
                rowSpacing: 16

                InterfaceCard
                {
                    interfaceName: "J2534Cannelloni"
                    title: "Cannelloni"
                    description: "Sends and receives CAN frames over UDP to a Cannelloni device (for example an ESP32 CAN bridge). "
                                 + "Host, port and protocol version are on the Cannelloni settings page."
                }

                InterfaceCard
                {
                    interfaceName: "J2534DllWrapper"
                    title: "DLL Wrapper"
                    description: "Passes every call to a real J2534 driver and logs it, so you can see what the diagnostic program does."
                    driver: page.chosenDriver
                    problem: page.chosenDriver === "" ? "" : page.driverProblem

                    Label
                    {
                        text: InterfaceConfig.drivers.length > 0 ? "Driver to wrap:" : "No J2534 drivers are registered on this PC. Browse for the driver DLL:"
                        wrapMode: Text.Wrap
                        Layout.fillWidth: true
                    }

                    ComboBox
                    {
                        id: driverCombo
                        Layout.fillWidth: true
                        model: page.driverEntries()
                        textRole: "text"
                        displayText: currentIndex < 0 ? "Choose a driver…" : currentText
                        Component.onCompleted: currentIndex = page.indexOfDriver(page.chosenDriver)
                        onActivated: function(index)
                        {
                            var entry = page.driverEntries()[index]
                            if (entry.path === "")
                                driverDialog.open()
                            else
                                page.chosenDriver = entry.path
                        }
                    }
                }

                InterfaceCard
                {
                    interfaceName: "J2534Emulator"
                    title: "Emulator"
                    description: "Answers calls with built-in responses. No hardware needed; useful to check that a diagnostic program talks to the DLL."
                }
            }

            RowLayout
            {
                Layout.fillWidth: true
                Layout.leftMargin: 24
                Layout.rightMargin: 24
                visible: InterfaceConfig.busy || page.errorText !== ""
                spacing: 8

                BusyIndicator
                {
                    visible: InterfaceConfig.busy
                    running: InterfaceConfig.busy
                    Layout.preferredWidth: 28
                    Layout.preferredHeight: 28
                }

                Label
                {
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    color: InterfaceConfig.busy ? "#555555" : "#c42b1c"
                    text: InterfaceConfig.busy ? "Waiting for the Windows prompt…" : page.errorText
                }
            }

            Label
            {
                Layout.fillWidth: true
                Layout.leftMargin: 24
                Layout.rightMargin: 24
                wrapMode: Text.Wrap
                color: InterfaceConfig.registered ? "#107c10" : "#555555"
                text: InterfaceConfig.registered
                      ? "✓ Diagnostic programs can find this driver: " + InterfaceConfig.registeredDll
                      : (InterfaceConfig.expectedDllFound
                         ? "Not registered for diagnostic programs yet. Choosing an interface also registers " + InterfaceConfig.expectedDll + "."
                         : "The PassThru DLL was not found at " + InterfaceConfig.expectedDll
                           + ". Install it next to the App (cmake --install for both presets), or register it manually below.")
            }

            Button
            {
                Layout.leftMargin: 18
                flat: true
                text: page.showManual ? "▾ Hide manual commands" : "▸ Show manual commands"
                onClicked: page.showManual = !page.showManual
            }

            TextArea
            {
                Layout.fillWidth: true
                Layout.leftMargin: 24
                Layout.rightMargin: 24
                Layout.bottomMargin: 24
                visible: page.showManual
                readOnly: true
                selectByMouse: true
                wrapMode: TextEdit.WrapAnywhere
                font.family: "Consolas"
                text: "Run in an administrator command prompt, then restart the App:\n\n"
                      + "rem Cannelloni\n" + InterfaceConfig.manualCommands("J2534Cannelloni", "") + "\n\n"
                      + "rem DLL Wrapper\n" + InterfaceConfig.manualCommands("J2534DllWrapper", page.chosenDriver) + "\n\n"
                      + "rem Emulator\n" + InterfaceConfig.manualCommands("J2534Emulator", "")
            }
        }
    }
}
