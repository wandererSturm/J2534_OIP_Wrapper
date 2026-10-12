#ifndef J2534DLLWRAPPER_H
#define J2534DLLWRAPPER_H


#include <windows.h>
#include <iostream>
#include <memory>
#include "ExDbg.h"
#include "Exj2534WrapperInterface.h"
#include "j2534_dev_typedefs.h"
#include "j2534_defs.h"
#include "expipeclient.h"
#include "exj2534wrapperInterface.h"

// Custom deleter
struct DllDeleter {
    typedef HMODULE pointer;
    void operator()(HMODULE hModule) const {
        FreeLibrary(hModule);
    }
};

#define DECLARE_AND_DEFINE_WRAPPER_FUNCTION(FunctionType, FunctionName) \
private: \
    FunctionType m_##FunctionName = nullptr; \
    public: \
    template<typename... Args> \
    long FunctionName(Args... args) { \
        /*EXDBG_LOG << "Function called: " #FunctionName << EXDBG_END;*/ \
        if (m_##FunctionName) { \
            return m_##FunctionName(args...); \
    } else { \
            /*EXDBG_LOG << "Function " #FunctionName " not found in DLL." << EXDBG_END;*/ \
            return ERR_NOT_SUPPORTED; \
    } \
}

class J2534DllWrapper: public ExJ2534WrapperInterface
{
public:
    J2534DllWrapper();

    std::string getIName();

    bool loadDll(const std::wstring& dllPath);
    void unloadDll();

    long PassThruOpen(const void *pName, unsigned long *pDeviceID);
    long PassThruClose(unsigned long DeviceID);
    long PassThruConnect(unsigned long DeviceID, unsigned long ProtocolID, unsigned long Flags, unsigned long Baudrate, unsigned long *pChannelID);
    long PassThruDisconnect(unsigned long ChannelID);
    long PassThruReadMsgs(unsigned long ChannelID, PASSTHRU_MSG *pMsg, unsigned long *pNumMsgs, unsigned long Timeout);
    long PassThruWriteMsgs(unsigned long ChannelID, const PASSTHRU_MSG *pMsg, unsigned long *pNumMsgs, unsigned long Timeout);
    long PassThruStartPeriodicMsg(unsigned long ChannelID,const PASSTHRU_MSG *pMsg, unsigned long *pMsgID, unsigned long TimeInterval);
    long PassThruStopPeriodicMsg(unsigned long ChannelID, unsigned long MsgID);
    long PassThruStartMsgFilter(unsigned long ChannelID, unsigned long FilterType, const PASSTHRU_MSG *pMaskMsg, const PASSTHRU_MSG *pPatternMsg, const PASSTHRU_MSG *pFlowControlMsg, unsigned long *pMsgID);
    long PassThruStopMsgFilter(unsigned long ChannelID, unsigned long MsgID);
    long PassThruSetProgrammingVoltage(unsigned long DeviceID, unsigned long Pin, unsigned long Voltage);
    long PassThruReadVersion(unsigned long DeviceID, char * pFirmwareVersion, char *pDllVersion, char * pApiVersion);
    long PassThruGetLastError(char *pErrorDescription);
    long PassThruIoctl(unsigned long ChannelID, unsigned long IoctlID, const void *pInput, void *pOutput);

    ///DREW Tech extra functions
    ///
    long PassThruExConfigureWiFi ( void );
    long PassThruExDeviceWatchdog ( void );
    long PassThruExDownloadCypheredFlashData ( void );
    long PassThruExEraseFlash ( void );
    long PassThruExInitiateCypheredFlashDownload ( void );
    long PassThruExReadFlash ( void );
    long PassThruExResetFlash ( void );
    long PassThruExRunSelfTest ( void );
    long PassThruExWriteFlash ( void );

    long PassThruLoadFirmware(void);
    long PassThruRecoverFirmware(void);
    long PassThruReadIPSetup(unsigned long DeviceID, char *host_name, char *ip_addr, char *subnet_mask, char *gateway, char *dhcp_addr);
    long PassThruWriteIPSetup(unsigned long DeviceID, char *host_name, char *ip_addr, char *subnet_mask, char *gateway, char *dhcp_addr);
    long PassThruReadPCSetup(char *host_name, char *ip_addr);
    long PassThruGetPointer(long vb_pointer);
    long PassThruGetNextCarDAQ(char **name, unsigned long *version, char **addr);

    ///v05.00
    long PassThruScanForDevices(unsigned long *pDeviceCount);
    long PassThruGetNextDevice(SDEVICE *psDevice);

    long PassThruSelect(SCHANNELSET *ChannelSetPtr, unsigned long SelectType, unsigned long Timeout);
    long PassThruReadDetails(unsigned long* pName);

    long PassThruQueueMsgs(unsigned long ChannelID, PASSTHRU_MSG *pMsg, unsigned long *pNumMsgs);

private:
    template <typename FunctionType>
    FunctionType loadFunction(const char* functionName, std::vector<PipeMessageBuilder::DllFunction>* report = nullptr);
    PipeMessageBuilder message(std::string text = {});
    // Parameterless vendor functions share one message type.
    long callVendor(j2534_pipe_VendorFunction function, long (CALLBACK *fn)(void), const char* name);
    void send(PipeMessageBuilder& message);
    std::unique_ptr<HMODULE, DllDeleter> m_dllHandle;

private:
    PTOPEN m_PassThruOpen = nullptr;
    PTCLOSE m_PassThruClose = nullptr;
    PTCONNECT m_PassThruConnect = nullptr;
    PTDISCONNECT m_PassThruDisconnect = nullptr;
    PTREADMSGS m_PassThruReadMsgs = nullptr;
    PTWRITEMSGS m_PassThruWriteMsgs = nullptr;
    PTSTARTPERIODICMSG m_PassThruStartPeriodicMsg = nullptr;
    PTSTOPPERIODICMSG m_PassThruStopPeriodicMsg = nullptr;
    PTSTARTMSGFILTER m_PassThruStartMsgFilter = nullptr;
    PTSTOPMSGFILTER m_PassThruStopMsgFilter = nullptr;
    PTSETPROGRAMMINGVOLTAGE m_PassThruSetProgrammingVoltage = nullptr;
    PTREADVERSION m_PassThruReadVersion = nullptr;
    PTGETLASTERROR m_PassThruGetLastError = nullptr;
    PTIOCTL m_PassThruIoctl = nullptr;

    PTEXCONFIGWIFI m_PassThruExConfigureWiFi = nullptr;
    PTEXDEVWATCHDOG m_PassThruExDeviceWatchdog = nullptr;
    PTEXDLCYPHFLASHDATA m_PassThruExDownloadCypheredFlashData = nullptr;
    PTEXERASEFLASH m_PassThruExEraseFlash = nullptr;
    PTEXINITCYPHFLASHDL m_PassThruExInitiateCypheredFlashDownload = nullptr;
    PTEXREADFLASH m_PassThruExReadFlash = nullptr;
    PTEXRESETFLASH m_PassThruExResetFlash = nullptr;
    PTEXRINSELFTEST m_PassThruExRunSelfTest = nullptr;
    PTEXWRITEFLASH m_PassThruExWriteFlash = nullptr;
    PTLOADFIRMWARE m_PassThruLoadFirmware = nullptr;
    PTRECOVERFIRMWARE m_PassThruRecoverFirmware = nullptr;
    PTREADIPSETUP m_PassThruReadIPSetup = nullptr;
    PTWRITEIPSETUP m_PassThruWriteIPSetup = nullptr;
    PTREADPCSETUP m_PassThruReadPCSetup = nullptr;
    PTGETPOINTER m_PassThruGetPointer = nullptr;
    PTGETNEXTCARDAQ m_PassThruGetNextCarDAQ = nullptr;

    PTSCANFORDEVICES m_PassThruScanForDevices = nullptr;
    PTGETNEXTDEV m_PassThruGetNextDevice = nullptr;
    PTSELECT m_PassThruSelect = nullptr;
    PTREADDETAILS m_PassThruReadDetails = nullptr;

    PTQUEUEMSGS m_PassThruQueueMsgs = nullptr;

private:
    ExPipeClient *          _p_pipe;
    std::string             p_path;
    uint32_t                p_id;

};





#endif // J2534DLLWRAPPER_H
