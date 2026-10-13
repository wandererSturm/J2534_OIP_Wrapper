// App component: Start menu shortcut.
function Component()
{
}

Component.prototype.createOperations = function()
{
    component.createOperations();
    if (systemInfo.productType === "windows") {
        component.addOperation("CreateShortcut",
            "@TargetDir@/x64/bin/j2534_OverIP_App.exe",
            "@StartMenuDir@/J2534 Over IP.lnk",
            "workingDirectory=@TargetDir@/x64/bin",
            "description=J2534 Over IP");
    }
}
