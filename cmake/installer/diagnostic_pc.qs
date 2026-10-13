// Diagnostic PC component: register the PassThru DLL so diagnostic programs
// list it (HKLM\SOFTWARE\WOW6432Node\PassThruSupport.04.04\XplatformsPassThruOverIP).
// The selected interface is left as it is; the App asks for it on first start.
// Uninstalling deletes that key and the per-user settings.
function Component()
{
}

Component.prototype.createOperations = function()
{
    component.createOperations();
    var app = "@TargetDir@/x64/bin/j2534_OverIP_App.exe";
    component.addElevatedOperation("Execute", "{0}",
        app, "--apply-config",
        "--function-library", "@TargetDir@/x86/j2534_OverIP.dll",
        "--config-application", app,
        "UNDOEXECUTE",
        app, "--apply-config", "--remove");
}
