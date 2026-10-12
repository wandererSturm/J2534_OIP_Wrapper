#include "ExDbg.h"
#include "j2534dllwrapper.h"

#include <cstring>

namespace {

std::string not_found(const char* function)
{
    return std::string("Function ").append(function).append(" not found in DLL.");
}

// Fixed-size char arrays from drivers are not always null-terminated.
std::string bounded(const char* s, size_t max_len)
{
    return s == nullptr ? std::string() : std::string(s, strnlen(s, max_len));
}

} // namespace

J2534DllWrapper::J2534DllWrapper()
{
    this->_p_pipe = new ExPipeClient("\\\\.\\pipe\\exj2534_overip_pipe");
    this->_p_pipe->connect();

    this->p_path = ExDbg::getProcessPath();
    this->p_id = ExDbg::crc32(ExDbg::getProcessPath());

    auto msg = message("J2534DllWrapper Verbunden!");
    auto& started = msg.select(j2534_pipe_PipeMessage_module_started_tag, msg.msg().payload.module_started);
    started.instance = reinterpret_cast<uintptr_t>(this);
    send(msg);
}

std::string J2534DllWrapper::getIName()
{
    char pname[1024] = {0};
    sprintf(pname, "J2534DllWrapper %08x", (unsigned int)this);
    return std::string(pname);
}

PipeMessageBuilder J2534DllWrapper::message(std::string text)
{
    return PipeMessageBuilder(j2534_pipe_Module_MODULE_DLL_WRAPPER, this->p_id, this->p_path, std::move(text));
}

void J2534DllWrapper::send(PipeMessageBuilder& message)
{
    this->_p_pipe->send(message);
}

bool J2534DllWrapper::loadDll(const std::wstring& dllPath)
{
    std::vector<char> buf(dllPath.size());
    std::use_facet<std::ctype<wchar_t>>(std::locale{}).narrow(dllPath.data(), dllPath.data() + dllPath.size(), '?', buf.data());
    auto dllPathA = std::string(buf.data(), buf.size());

    auto msg = message();
    auto& loaded = msg.select(j2534_pipe_PipeMessage_dll_loaded_tag, msg.msg().payload.dll_loaded);
    loaded.path = msg.str(dllPathA);

    HMODULE hModule = LoadLibraryW(dllPath.c_str());
    if (!hModule)
    {
        msg.msg().text = msg.str(std::string("loadDll"));
        loaded.error = msg.str(std::string("Can't load DLL ").append(dllPathA));
        send(msg);
        return false;
    }

    m_dllHandle.reset(hModule);

    std::vector<PipeMessageBuilder::DllFunction> report;

    m_PassThruOpen = loadFunction<PTOPEN>("PassThruOpen", &report);
    m_PassThruClose = loadFunction<PTCLOSE>("PassThruClose", &report);
    m_PassThruConnect = loadFunction<PTCONNECT>("PassThruConnect", &report);
    m_PassThruDisconnect = loadFunction<PTDISCONNECT>("PassThruDisconnect", &report);
    m_PassThruReadMsgs = loadFunction<PTREADMSGS>("PassThruReadMsgs", &report);
    m_PassThruWriteMsgs = loadFunction<PTWRITEMSGS>("PassThruWriteMsgs", &report);
    m_PassThruStartPeriodicMsg = loadFunction<PTSTARTPERIODICMSG>("PassThruStartPeriodicMsg", &report);
    m_PassThruStopPeriodicMsg = loadFunction<PTSTOPPERIODICMSG>("PassThruStopPeriodicMsg", &report);
    m_PassThruStartMsgFilter = loadFunction<PTSTARTMSGFILTER>("PassThruStartMsgFilter", &report);
    m_PassThruStopMsgFilter = loadFunction<PTSTOPMSGFILTER>("PassThruStopMsgFilter", &report);
    m_PassThruSetProgrammingVoltage = loadFunction<PTSETPROGRAMMINGVOLTAGE>("PassThruSetProgrammingVoltage", &report);
    m_PassThruReadVersion = loadFunction<PTREADVERSION>("PassThruReadVersion", &report);
    m_PassThruGetLastError = loadFunction<PTGETLASTERROR>("PassThruGetLastError", &report);
    m_PassThruIoctl = loadFunction<PTIOCTL>("PassThruIoctl", &report);

    // Drew Tech specific function calls
    m_PassThruLoadFirmware = loadFunction<PTLOADFIRMWARE>("PassThruLoadFirmware", &report);
    m_PassThruRecoverFirmware = loadFunction<PTRECOVERFIRMWARE>("PassThruRecoverFirmware", &report);
    m_PassThruReadIPSetup = loadFunction<PTREADIPSETUP>("PassThruReadIPSetup", &report);
    m_PassThruWriteIPSetup = loadFunction<PTWRITEIPSETUP>("PassThruWriteIPSetup", &report);
    m_PassThruReadPCSetup = loadFunction<PTREADPCSETUP>("PassThruReadPCSetup", &report);
    m_PassThruGetPointer = loadFunction<PTGETPOINTER>("PassThruGetPointer", &report);
    m_PassThruGetNextCarDAQ = loadFunction<PTGETNEXTCARDAQ>("PassThruGetNextCarDAQ", &report);

    m_PassThruExConfigureWiFi = loadFunction<PTEXCONFIGWIFI>("PassThruExConfigureWiFi", &report);
    m_PassThruExDeviceWatchdog = loadFunction<PTEXDEVWATCHDOG>("PassThruExDeviceWatchdog", &report);
    m_PassThruExDownloadCypheredFlashData = loadFunction<PTEXDLCYPHFLASHDATA>("PassThruExDownloadCypheredFlashData", &report);
    m_PassThruExEraseFlash = loadFunction<PTEXERASEFLASH>("PassThruExEraseFlash", &report);
    m_PassThruExInitiateCypheredFlashDownload = loadFunction<PTEXINITCYPHFLASHDL>("PassThruExInitiateCypheredFlashDownload", &report);
    m_PassThruExReadFlash = loadFunction<PTEXREADFLASH>("PassThruExReadFlash", &report);
    m_PassThruExResetFlash = loadFunction<PTEXRESETFLASH>("PassThruExResetFlash", &report);
    m_PassThruExRunSelfTest = loadFunction<PTEXRINSELFTEST>("PassThruExRunSelfTest", &report);
    m_PassThruExWriteFlash = loadFunction<PTEXWRITEFLASH>("PassThruExWriteFlash", &report);
    m_PassThruQueueMsgs = loadFunction<PTQUEUEMSGS>("PassThruQueueMsgs", &report);
    m_PassThruReadDetails = loadFunction<PTREADDETAILS>("PassThruReadDetails", &report);

    ///v05.00
    m_PassThruScanForDevices = loadFunction<PTSCANFORDEVICES>("PassThruScanForDevices", &report);
    m_PassThruGetNextDevice = loadFunction<PTGETNEXTDEV>("PassThruGetNextDevice", &report);
    m_PassThruSelect = loadFunction<PTSELECT>("PassThruSelect", &report);

    msg.msg().text = msg.str(std::string("wrapped functions"));
    loaded.functions = msg.dll_functions(std::move(report));
    send(msg);

    return true;
}

void J2534DllWrapper::unloadDll() {
    m_dllHandle.reset();
}

template <typename FunctionType>
FunctionType J2534DllWrapper::loadFunction(const char* functionName, std::vector<PipeMessageBuilder::DllFunction>* report) {
    auto proc = reinterpret_cast<FunctionType>(GetProcAddress(reinterpret_cast<HMODULE>(m_dllHandle.get()), functionName));
    if (report != nullptr)
        report->push_back({functionName, proc != nullptr});
    return proc;
}

long J2534DllWrapper::PassThruOpen(const void *pName, unsigned long *pDeviceID)
{
    long retval = ERR_NOT_SUPPORTED;

    auto msg = message("PassThruOpen");
    auto& call = msg.select(j2534_pipe_PipeMessage_open_tag, msg.msg().payload.open);
    call.name = msg.str(static_cast<const char*>(pName));

    if (this->m_PassThruOpen) {
        retval = this->m_PassThruOpen(pName, pDeviceID);
        if (pDeviceID) call.device_id = *pDeviceID;
        msg.result(call, retval);
    } else {
        msg.result(call, retval, not_found(__FUNCTION__));
    }

    send(msg);
    return retval;
}

long J2534DllWrapper::PassThruClose(unsigned long DeviceID)
{
    long retval = ERR_NOT_SUPPORTED;

    auto msg = message("PassThruClose");
    auto& call = msg.select(j2534_pipe_PipeMessage_close_tag, msg.msg().payload.close);
    call.device_id = DeviceID;

    if (this->m_PassThruClose) {
        retval = this->m_PassThruClose(DeviceID);
        msg.result(call, retval);
    } else {
        msg.result(call, retval, not_found(__FUNCTION__));
    }

    send(msg);
    return retval;
}

long J2534DllWrapper::PassThruConnect(unsigned long DeviceID, unsigned long ProtocolID, unsigned long Flags, unsigned long Baudrate, unsigned long *pChannelID)
{
    long retval = ERR_NOT_SUPPORTED;

    auto msg = message("PassThruConnect");
    auto& call = msg.select(j2534_pipe_PipeMessage_connect_tag, msg.msg().payload.connect);
    call.device_id = DeviceID;
    call.protocol_id = ProtocolID;
    call.flags = Flags;
    call.baudrate = Baudrate;

    if (this->m_PassThruConnect) {
        retval = this->m_PassThruConnect(DeviceID, ProtocolID, Flags, Baudrate, pChannelID);
        if (pChannelID) call.channel_id = *pChannelID;
        msg.result(call, retval);
    } else {
        msg.result(call, retval, not_found(__FUNCTION__));
    }

    send(msg);
    return retval;
}

long J2534DllWrapper::PassThruDisconnect(unsigned long ChannelID)
{
    long retval = ERR_NOT_SUPPORTED;

    auto msg = message("PassThruDisconnect");
    auto& call = msg.select(j2534_pipe_PipeMessage_disconnect_tag, msg.msg().payload.disconnect);
    call.channel_id = ChannelID;

    if (this->m_PassThruDisconnect) {
        retval = this->m_PassThruDisconnect(ChannelID);
        msg.result(call, retval);
    } else {
        msg.result(call, retval, not_found(__FUNCTION__));
    }

    send(msg);
    return retval;
}

long J2534DllWrapper::PassThruReadMsgs(unsigned long ChannelID, PASSTHRU_MSG *pMsg, unsigned long *pNumMsgs, unsigned long Timeout)
{
    long retval = ERR_NOT_SUPPORTED;

    auto msg = message("PassThruReadMsgs");
    auto& call = msg.select(j2534_pipe_PipeMessage_read_msgs_tag, msg.msg().payload.read_msgs);
    call.channel_id = ChannelID;
    call.num_msgs_requested = pNumMsgs ? *pNumMsgs : 0;
    call.timeout = Timeout;

    if (this->m_PassThruReadMsgs) {
        retval = this->m_PassThruReadMsgs(ChannelID, pMsg, pNumMsgs, Timeout);
        // On return *pNumMsgs holds how many messages the driver filled in.
        call.msgs = msg.msgs(pMsg, pNumMsgs ? *pNumMsgs : 0);
        msg.result(call, retval);
    } else {
        EXDBG_LOG << "Function " << __FUNCTION__ << " not found in DLL." << EXDBG_END;
        msg.result(call, retval, not_found(__FUNCTION__));
    }

    send(msg);
    return retval;
}

long J2534DllWrapper::PassThruQueueMsgs(unsigned long ChannelID, PASSTHRU_MSG *pMsg, unsigned long *pNumMsgs)
{
    long retval = ERR_NOT_SUPPORTED;

    auto msg = message("PassThruQueueMsgs");
    auto& call = msg.select(j2534_pipe_PipeMessage_queue_msgs_tag, msg.msg().payload.queue_msgs);
    call.channel_id = ChannelID;
    call.msgs = msg.msgs(pMsg, pNumMsgs ? *pNumMsgs : 0);

    if (this->m_PassThruQueueMsgs) {
        retval = this->m_PassThruQueueMsgs(ChannelID, pMsg, pNumMsgs);
        if (pNumMsgs) call.num_msgs_queued = *pNumMsgs;
        msg.result(call, retval);
    } else {
        msg.result(call, retval, not_found(__FUNCTION__));
    }

    send(msg);
    return retval;
}

long J2534DllWrapper::PassThruWriteMsgs(unsigned long ChannelID, const PASSTHRU_MSG *pMsg, unsigned long *pNumMsgs, unsigned long Timeout)
{
    long retval = ERR_NOT_SUPPORTED;

    auto msg = message("PassThruWriteMsgs");
    auto& call = msg.select(j2534_pipe_PipeMessage_write_msgs_tag, msg.msg().payload.write_msgs);
    call.channel_id = ChannelID;
    call.msgs = msg.msgs(pMsg, pNumMsgs ? *pNumMsgs : 0);
    call.timeout = Timeout;

    if (this->m_PassThruWriteMsgs) {
        retval = this->m_PassThruWriteMsgs(ChannelID, (void*)pMsg, pNumMsgs, Timeout);
        if (pNumMsgs) call.num_msgs_written = *pNumMsgs;
        msg.result(call, retval);
    } else {
        msg.result(call, retval, not_found(__FUNCTION__));
    }

    send(msg);
    return retval;
}

long J2534DllWrapper::PassThruStartPeriodicMsg(unsigned long ChannelID, const PASSTHRU_MSG *pMsg, unsigned long *pMsgID, unsigned long TimeInterval)
{
    long retval = ERR_NOT_SUPPORTED;

    auto msg = message("PassThruStartPeriodicMsg");
    auto& call = msg.select(j2534_pipe_PipeMessage_start_periodic_msg_tag, msg.msg().payload.start_periodic_msg);
    call.channel_id = ChannelID;
    msg.passthru_msg(call.has_msg, call.msg, pMsg);
    call.time_interval = TimeInterval;

    if (this->m_PassThruStartPeriodicMsg) {
        retval = this->m_PassThruStartPeriodicMsg(ChannelID, (void*)pMsg, pMsgID, TimeInterval);
        if (pMsgID) call.msg_id = *pMsgID;
        msg.result(call, retval);
    } else {
        msg.result(call, retval, not_found(__FUNCTION__));
    }

    send(msg);
    return retval;
}

long J2534DllWrapper::PassThruStopPeriodicMsg(unsigned long ChannelID, unsigned long MsgID)
{
    long retval = ERR_NOT_SUPPORTED;

    auto msg = message("PassThruStopPeriodicMsg");
    auto& call = msg.select(j2534_pipe_PipeMessage_stop_periodic_msg_tag, msg.msg().payload.stop_periodic_msg);
    call.channel_id = ChannelID;
    call.msg_id = MsgID;

    if (this->m_PassThruStopPeriodicMsg) {
        retval = this->m_PassThruStopPeriodicMsg(ChannelID, MsgID);
        msg.result(call, retval);
    } else {
        msg.result(call, retval, not_found(__FUNCTION__));
    }

    send(msg);
    return retval;
}

long J2534DllWrapper::PassThruStartMsgFilter(unsigned long ChannelID, unsigned long FilterType, const PASSTHRU_MSG *pMaskMsg, const PASSTHRU_MSG *pPatternMsg, const PASSTHRU_MSG *pFlowControlMsg, unsigned long *pMsgID)
{
    long retval = ERR_NOT_SUPPORTED;

    auto msg = message("PassThruStartMsgFilter");
    auto& call = msg.select(j2534_pipe_PipeMessage_start_msg_filter_tag, msg.msg().payload.start_msg_filter);
    call.channel_id = ChannelID;
    call.filter_type = FilterType;
    msg.passthru_msg(call.has_mask, call.mask, pMaskMsg);
    msg.passthru_msg(call.has_pattern, call.pattern, pPatternMsg);
    msg.passthru_msg(call.has_flow_control, call.flow_control, pFlowControlMsg);

    if (this->m_PassThruStartMsgFilter) {
        retval = this->m_PassThruStartMsgFilter(ChannelID, FilterType, (void*)pMaskMsg, (void*)pPatternMsg, (void*)pFlowControlMsg, pMsgID);
        if (pMsgID) call.filter_id = *pMsgID;
        msg.result(call, retval);
    } else {
        msg.result(call, retval, not_found(__FUNCTION__));
    }

    send(msg);
    return retval;
}

long J2534DllWrapper::PassThruStopMsgFilter(unsigned long ChannelID, unsigned long MsgID)
{
    long retval = ERR_NOT_SUPPORTED;

    auto msg = message("PassThruStopMsgFilter");
    auto& call = msg.select(j2534_pipe_PipeMessage_stop_msg_filter_tag, msg.msg().payload.stop_msg_filter);
    call.channel_id = ChannelID;
    call.filter_id = MsgID;

    if (this->m_PassThruStopMsgFilter) {
        retval = this->m_PassThruStopMsgFilter(ChannelID, MsgID);
        msg.result(call, retval);
    } else {
        msg.result(call, retval, not_found(__FUNCTION__));
    }

    send(msg);
    return retval;
}

long J2534DllWrapper::PassThruSetProgrammingVoltage(unsigned long DeviceID, unsigned long Pin, unsigned long Voltage)
{
    long retval = ERR_NOT_SUPPORTED;

    auto msg = message("PassThruSetProgrammingVoltage");
    auto& call = msg.select(j2534_pipe_PipeMessage_set_programming_voltage_tag, msg.msg().payload.set_programming_voltage);
    call.device_id = DeviceID;
    call.pin = Pin;
    call.voltage = Voltage;

    if (this->m_PassThruSetProgrammingVoltage) {
        retval = this->m_PassThruSetProgrammingVoltage(DeviceID, Pin, Voltage);
        msg.result(call, retval);
    } else {
        msg.result(call, retval, not_found(__FUNCTION__));
    }

    send(msg);
    return retval;
}

long J2534DllWrapper::PassThruReadVersion(unsigned long DeviceID, char * pFirmwareVersion, char *pDllVersion, char * pApiVersion)
{
    long retval = ERR_NOT_SUPPORTED;

    auto msg = message("PassThruReadVersion");
    auto& call = msg.select(j2534_pipe_PipeMessage_read_version_tag, msg.msg().payload.read_version);
    call.device_id = DeviceID;

    if (this->m_PassThruReadVersion) {
        retval = this->m_PassThruReadVersion(DeviceID, pFirmwareVersion, pDllVersion, pApiVersion);
        // J2534 version strings are at most 80 chars including the terminator.
        call.firmware_version = msg.str(bounded(pFirmwareVersion, 80));
        call.dll_version = msg.str(bounded(pDllVersion, 80));
        call.api_version = msg.str(bounded(pApiVersion, 80));
        msg.result(call, retval);
    } else {
        msg.result(call, retval, not_found(__FUNCTION__));
    }

    send(msg);
    return retval;
}

long J2534DllWrapper::PassThruGetLastError(char *pErrorDescription)
{
    long retval = ERR_NOT_SUPPORTED;

    auto msg = message("PassThruGetLastError");
    auto& call = msg.select(j2534_pipe_PipeMessage_get_last_error_tag, msg.msg().payload.get_last_error);

    if (this->m_PassThruGetLastError) {
        retval = this->m_PassThruGetLastError(pErrorDescription);
        call.error_description = msg.str(bounded(pErrorDescription, 80));
        msg.result(call, retval);
    } else {
        msg.result(call, retval, not_found(__FUNCTION__));
    }

    send(msg);
    return retval;
}

long J2534DllWrapper::PassThruIoctl(unsigned long ChannelID, unsigned long IoctlID, const void *pInput, void *pOutput)
{
    long retval = ERR_NOT_SUPPORTED;

    auto msg = message("PassThruIoctl");
    auto& call = msg.select(j2534_pipe_PipeMessage_ioctl_tag, msg.msg().payload.ioctl);
    call.channel_id = ChannelID;
    call.ioctl_id = IoctlID;

    if (this->m_PassThruIoctl) {
        retval = this->m_PassThruIoctl(ChannelID, IoctlID, pInput, pOutput);
        msg.result(call, retval);
    } else {
        msg.result(call, retval, not_found(__FUNCTION__));
    }

    send(msg);
    return retval;
}

///DREW Tech extra functions
///
long J2534DllWrapper::callVendor(j2534_pipe_VendorFunction function, long (CALLBACK *fn)(void), const char* name)
{
    long retval = ERR_NOT_SUPPORTED;

    auto msg = message(name);
    auto& call = msg.select(j2534_pipe_PipeMessage_vendor_tag, msg.msg().payload.vendor);
    call.vendor_function = function;

    if (fn) {
        retval = fn();
        msg.result(call, retval);
    } else {
        msg.result(call, retval, not_found(name));
    }

    send(msg);
    return retval;
}

long J2534DllWrapper::PassThruExConfigureWiFi ( void )
{
    return callVendor(j2534_pipe_VendorFunction_VENDOR_FUNCTION_EX_CONFIGURE_WIFI, this->m_PassThruExConfigureWiFi, "PassThruExConfigureWiFi");
}

long J2534DllWrapper::PassThruExDeviceWatchdog ( void )
{
    return callVendor(j2534_pipe_VendorFunction_VENDOR_FUNCTION_EX_DEVICE_WATCHDOG, this->m_PassThruExDeviceWatchdog, "PassThruExDeviceWatchdog");
}

long J2534DllWrapper::PassThruExDownloadCypheredFlashData ( void )
{
    return callVendor(j2534_pipe_VendorFunction_VENDOR_FUNCTION_EX_DOWNLOAD_CYPHERED_FLASH_DATA, this->m_PassThruExDownloadCypheredFlashData, "PassThruExDownloadCypheredFlashData");
}

long J2534DllWrapper::PassThruExEraseFlash ( void )
{
    return callVendor(j2534_pipe_VendorFunction_VENDOR_FUNCTION_EX_ERASE_FLASH, this->m_PassThruExEraseFlash, "PassThruExEraseFlash");
}

long J2534DllWrapper::PassThruExInitiateCypheredFlashDownload ( void )
{
    return callVendor(j2534_pipe_VendorFunction_VENDOR_FUNCTION_EX_INITIATE_CYPHERED_FLASH_DOWNLOAD, this->m_PassThruExInitiateCypheredFlashDownload, "PassThruExInitiateCypheredFlashDownload");
}

long J2534DllWrapper::PassThruExReadFlash ( void )
{
    return callVendor(j2534_pipe_VendorFunction_VENDOR_FUNCTION_EX_READ_FLASH, this->m_PassThruExReadFlash, "PassThruExReadFlash");
}

long J2534DllWrapper::PassThruExResetFlash ( void )
{
    return callVendor(j2534_pipe_VendorFunction_VENDOR_FUNCTION_EX_RESET_FLASH, this->m_PassThruExResetFlash, "PassThruExResetFlash");
}

long J2534DllWrapper::PassThruExRunSelfTest ( void )
{
    return callVendor(j2534_pipe_VendorFunction_VENDOR_FUNCTION_EX_RUN_SELF_TEST, this->m_PassThruExRunSelfTest, "PassThruExRunSelfTest");
}

long J2534DllWrapper::PassThruExWriteFlash ( void )
{
    return callVendor(j2534_pipe_VendorFunction_VENDOR_FUNCTION_EX_WRITE_FLASH, this->m_PassThruExWriteFlash, "PassThruExWriteFlash");
}

long J2534DllWrapper::PassThruLoadFirmware(void)
{
    return callVendor(j2534_pipe_VendorFunction_VENDOR_FUNCTION_LOAD_FIRMWARE, this->m_PassThruLoadFirmware, "PassThruLoadFirmware");
}

long J2534DllWrapper::PassThruRecoverFirmware(void)
{
    return callVendor(j2534_pipe_VendorFunction_VENDOR_FUNCTION_RECOVER_FIRMWARE, this->m_PassThruRecoverFirmware, "PassThruRecoverFirmware");
}

long J2534DllWrapper::PassThruReadIPSetup(unsigned long DeviceID, char *host_name, char *ip_addr, char *subnet_mask, char *gateway, char *dhcp_addr)
{
    long retval = ERR_NOT_SUPPORTED;

    auto msg = message("PassThruReadIPSetup");
    auto& call = msg.select(j2534_pipe_PipeMessage_read_ip_setup_tag, msg.msg().payload.read_ip_setup);
    call.device_id = DeviceID;

    if (this->m_PassThruReadIPSetup) {
        retval = this->m_PassThruReadIPSetup(DeviceID, host_name, ip_addr, subnet_mask, gateway, dhcp_addr);
        call.has_setup = true;
        call.setup.host_name = msg.str(static_cast<const char*>(host_name));
        call.setup.ip_addr = msg.str(static_cast<const char*>(ip_addr));
        call.setup.subnet_mask = msg.str(static_cast<const char*>(subnet_mask));
        call.setup.gateway = msg.str(static_cast<const char*>(gateway));
        call.setup.dhcp_addr = msg.str(static_cast<const char*>(dhcp_addr));
        msg.result(call, retval);
    } else {
        msg.result(call, retval, not_found(__FUNCTION__));
    }

    send(msg);
    return retval;
}

long J2534DllWrapper::PassThruWriteIPSetup(unsigned long DeviceID, char *host_name, char *ip_addr, char *subnet_mask, char *gateway, char *dhcp_addr)
{
    long retval = ERR_NOT_SUPPORTED;

    auto msg = message("PassThruWriteIPSetup");
    auto& call = msg.select(j2534_pipe_PipeMessage_write_ip_setup_tag, msg.msg().payload.write_ip_setup);
    call.device_id = DeviceID;
    call.has_setup = true;
    call.setup.host_name = msg.str(static_cast<const char*>(host_name));
    call.setup.ip_addr = msg.str(static_cast<const char*>(ip_addr));
    call.setup.subnet_mask = msg.str(static_cast<const char*>(subnet_mask));
    call.setup.gateway = msg.str(static_cast<const char*>(gateway));
    call.setup.dhcp_addr = msg.str(static_cast<const char*>(dhcp_addr));

    if (this->m_PassThruWriteIPSetup) {
        retval = this->m_PassThruWriteIPSetup(DeviceID, host_name, ip_addr, subnet_mask, gateway, dhcp_addr);
        msg.result(call, retval);
    } else {
        msg.result(call, retval, not_found(__FUNCTION__));
    }

    send(msg);
    return retval;
}

long J2534DllWrapper::PassThruReadPCSetup(char *host_name, char *ip_addr)
{
    long retval = ERR_NOT_SUPPORTED;

    auto msg = message("PassThruReadPCSetup");
    auto& call = msg.select(j2534_pipe_PipeMessage_read_pc_setup_tag, msg.msg().payload.read_pc_setup);

    if (this->m_PassThruReadPCSetup) {
        retval = this->m_PassThruReadPCSetup(host_name, ip_addr);
        call.host_name = msg.str(static_cast<const char*>(host_name));
        call.ip_addr = msg.str(static_cast<const char*>(ip_addr));
        msg.result(call, retval);
    } else {
        msg.result(call, retval, not_found(__FUNCTION__));
    }

    send(msg);
    return retval;
}

long J2534DllWrapper::PassThruGetPointer(long vb_pointer)
{
    long retval = ERR_NOT_SUPPORTED;

    auto msg = message("PassThruGetPointer");
    auto& call = msg.select(j2534_pipe_PipeMessage_get_pointer_tag, msg.msg().payload.get_pointer);
    call.vb_pointer = vb_pointer;

    if (this->m_PassThruGetPointer) {
        retval = this->m_PassThruGetPointer(vb_pointer);
        msg.result(call, retval);
    } else {
        msg.result(call, retval, not_found(__FUNCTION__));
    }

    send(msg);
    return retval;
}

long J2534DllWrapper::PassThruGetNextCarDAQ(char **name, unsigned long *version, char **addr)
{
    long retval = ERR_NOT_SUPPORTED;

    auto msg = message("PassThruGetNextCarDAQ");
    auto& call = msg.select(j2534_pipe_PipeMessage_get_next_cardaq_tag, msg.msg().payload.get_next_cardaq);

    if (this->m_PassThruGetNextCarDAQ) {
        retval = this->m_PassThruGetNextCarDAQ(name, version, addr);
        // *name is null once there are no more devices.
        if (name) call.name = msg.str(static_cast<const char*>(*name));
        if (version) call.version = *version;
        if (addr) call.addr = msg.str(static_cast<const char*>(*addr));
        msg.result(call, retval);
    } else {
        msg.result(call, retval, not_found(__FUNCTION__));
    }

    send(msg);
    return retval;
}

///v05.00
long J2534DllWrapper::PassThruScanForDevices(unsigned long *pDeviceCount)
{
    long retval = ERR_NOT_SUPPORTED;

    auto msg = message("PassThruScanForDevices");
    auto& call = msg.select(j2534_pipe_PipeMessage_scan_for_devices_tag, msg.msg().payload.scan_for_devices);

    if (this->m_PassThruScanForDevices) {
        retval = this->m_PassThruScanForDevices(pDeviceCount);
        if (pDeviceCount) call.device_count = *pDeviceCount;
        msg.result(call, retval);
    } else {
        msg.result(call, retval, not_found(__FUNCTION__));
    }

    send(msg);
    return retval;
}

long J2534DllWrapper::PassThruGetNextDevice(SDEVICE *psDevice)
{
    long retval = ERR_NOT_SUPPORTED;

    auto msg = message("PassThruGetNextDevice");
    auto& call = msg.select(j2534_pipe_PipeMessage_get_next_device_tag, msg.msg().payload.get_next_device);

    if (this->m_PassThruGetNextDevice) {
        retval = this->m_PassThruGetNextDevice(psDevice);
        if (psDevice) {
            call.has_device = true;
            call.device.name = msg.str(bounded(psDevice->DeviceName, sizeof(psDevice->DeviceName)));
            call.device.available = psDevice->DeviceAvailable;
            call.device.dll_fw_status = psDevice->DeviceDLLFWStatus;
            call.device.connect_media = psDevice->DeviceConnectMedia;
            call.device.connect_speed = psDevice->DeviceConnectSpeed;
            call.device.signal_quality = psDevice->DeviceSignalQuality;
            call.device.signal_strength = psDevice->DeviceSignalStrength;
        }
        msg.result(call, retval);
    } else {
        msg.result(call, retval, not_found(__FUNCTION__));
    }

    send(msg);
    return retval;
}

long J2534DllWrapper::PassThruSelect(SCHANNELSET *ChannelSetPtr, unsigned long SelectType, unsigned long Timeout)
{
    long retval = ERR_NOT_SUPPORTED;

    auto msg = message("PassThruSelect");
    auto& call = msg.select(j2534_pipe_PipeMessage_select_tag, msg.msg().payload.select);
    if (ChannelSetPtr) {
        call.channel_ids = msg.uint32_list(ChannelSetPtr->ChannelList, ChannelSetPtr->ChannelCount);
        call.channel_threshold = ChannelSetPtr->ChannelThreshold;
    }
    call.select_type = SelectType;
    call.timeout = Timeout;

    if (this->m_PassThruSelect) {
        retval = this->m_PassThruSelect(ChannelSetPtr, SelectType, Timeout);
        msg.result(call, retval);
    } else {
        msg.result(call, retval, not_found(__FUNCTION__));
    }

    send(msg);
    return retval;
}

long J2534DllWrapper::PassThruReadDetails(unsigned long* pName)
{
    long retval = ERR_NOT_SUPPORTED;

    auto msg = message("PassThruReadDetails");
    auto& call = msg.select(j2534_pipe_PipeMessage_read_details_tag, msg.msg().payload.read_details);
    if (pName) call.name = *pName;

    if (this->m_PassThruReadDetails) {
        retval = this->m_PassThruReadDetails(pName);
        if (pName) call.name = *pName;
        msg.result(call, retval);
    } else {
        msg.result(call, retval, not_found(__FUNCTION__));
    }

    send(msg);
    return retval;
}
