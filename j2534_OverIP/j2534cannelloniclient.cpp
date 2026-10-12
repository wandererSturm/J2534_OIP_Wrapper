#include <winsock2.h>
#include <ws2tcpip.h>  // For InetPton
#pragma comment(lib, "Ws2_32.lib")

#include "ExDbg.h"
#include "j2534cannelloniclient.h"


#include <windows.h>
#include <thread>
#include <mutex>
#include <unordered_map>
#include <atomic>
#include <cstring>
#include <chrono>
#include <cctype>

//20000 125кбит  3/11 пин
//20001 500кбит  6/14 пин - Standart CAN Pins on OBD-II
//20002 500кбит 12/13 пин
enum op_codes {DATA, ACK, NACK};

#pragma pack(push, 1)
struct CannelloniDataPacket {
    uint8_t version;
    uint8_t op_code;
    uint8_t seq_no;
    uint16_t count;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct CannelloniCanFrame {
    uint32_t can_id;
    uint8_t length;
    uint8_t data[MAX_CAN_FRAME_DATA_LEN];
};
#pragma pack(pop)

int J2534CannelloniClient::scanIndex = 0;

J2534CannelloniClient::J2534CannelloniClient()
{
    this->_p_pipe = new ExPipeClient("\\\\.\\pipe\\exj2534_overip_pipe");
    this->_p_pipe->connect();

    this->p_path = ExDbg::getProcessPath();
    this->p_id = ExDbg::crc32(ExDbg::getProcessPath());
    this->cannelloniVersion = 1;

    this->getDefaults();

    auto msg = message();
    auto& started = msg.select(j2534_pipe_PipeMessage_module_started_tag, msg.msg().payload.module_started);
    started.instance = reinterpret_cast<uintptr_t>(this);

    EXDBG_LOG << "[J2534CannelloniClient]: timestamp " << msg.msg().timestamp_ms << EXDBG_END;

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        lastError = "WSAStartup failed";
        lastErrorLong = ERR_DEVICE_NOT_CONNECTED;
        msg.msg().text = msg.str(std::string("J2534CannelloniClient WSAStartup failed!"));
    }
    else
    {
        msg.msg().text = msg.str(std::string("J2534CannelloniClient WSAStartup. Verbunden!"));
    }

    sendPipe(msg);
}

PipeMessageBuilder J2534CannelloniClient::message(std::string text)
{
    return PipeMessageBuilder(j2534_pipe_Module_MODULE_CANNELLONI, this->p_id, this->p_path, std::move(text));
}

void J2534CannelloniClient::sendPipe(PipeMessageBuilder& message)
{
    this->_p_pipe->send(message);
}

namespace {

std::string failure(const char* function, const std::string& reason)
{
    return std::string("Function ").append(function).append(" -> ").append(reason);
}

std::string not_implemented(const char* function)
{
    return std::string("Function ").append(function).append(" not implemented");
}

} // namespace

J2534CannelloniClient::~J2534CannelloniClient()
{
    if (channelMap.empty()) WSACleanup();
    //WSACleanup();
}

std::string J2534CannelloniClient::getIName()
{
    char pname[1024] = {0};
    sprintf(pname, "J2534CannelloniClient %08x", (unsigned int)this);
    return std::string(pname);
}

long J2534CannelloniClient::getDefaults()
{
    int retc = 0;
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\WOW6432Node\\PassThruSupport.04.04\\XplatformsPassThruOverIP", 0, KEY_READ, &hKey) == ERROR_SUCCESS)
    {
        char host[MAX_PATH] = {0};
        DWORD port = 0;
        DWORD version = 0;
        DWORD portSize = sizeof(port);
        DWORD type = 0;

        DWORD hpSize = sizeof(host);
        if (RegQueryValueExA(hKey, "DefaultHost", NULL, NULL, (LPBYTE)host, &hpSize) == ERROR_SUCCESS)
        {
            this->defaultHost = host;
            retc++;
        }
        else
        {
            this->defaultHost = "127.0.0.1";
        }
        if(RegQueryValueExA(hKey,"DefaultPort", NULL, &type, (LPBYTE)&port, &portSize) == ERROR_SUCCESS)
        {
            this->defaultPort = port;
            retc++;
        }
        else
        {
            this->defaultPort = 20000;
        }

        if(RegQueryValueExA(hKey,"CannelloniDataVersion", NULL, &type, (LPBYTE)&version, &portSize) == ERROR_SUCCESS)
        {
            this->cannelloniVersion = version;
            retc++;
        }
        else
        {
            this->cannelloniVersion = 1;
        }

        RegCloseKey(hKey);
    }
    return retc==3?ERROR_SUCCESS:ERROR_ASSERTION_FAILURE;
}

//nc -u -w 1 172.16.90.1 20000 < <(printf '\x02\x00\x00\x00\x01\x80\x07\xE0\x00\x08\x01\x02\x03\x04\x05\x06\x07\x08')
void J2534CannelloniClient::startReceiveThread(ChannelContext& ctx)
{
    ctx.recvThread = std::thread([this, &ctx](){
        constexpr size_t maxPacketSize = 1500;  // UDP MTU
        uint8_t buffer[maxPacketSize] = {0};

        while (!ctx.stopRequested.load())
        {
            fd_set readfds;
            FD_ZERO(&readfds);
            FD_SET(ctx.sockfd, &readfds);
            timeval timeout{0, 1000};  // 100ms poll

            int ret = select(static_cast<int>(ctx.sockfd + 1), &readfds, nullptr, nullptr, &timeout);
            if (ret > 0 && FD_ISSET(ctx.sockfd, &readfds))
            {
                //sockaddr_in fromAddr;
                int bytesReceived = recv(ctx.sockfd, reinterpret_cast<char*>(buffer), maxPacketSize, 0);
                if (bytesReceived > 0)
                {
                    printf("GOT UDP MSG. LEN %d\n", bytesReceived);
                    if (bytesReceived < static_cast<int>(sizeof(CannelloniDataPacket)))
                    {
                        printf("PACKET to Small \n");
                        break;
                    }

                    printf("sizeof(CannelloniDataPacket) %d\n", sizeof(CannelloniDataPacket));
                    printf("sizeof(CannelloniCanFrame) %d\n", sizeof(CannelloniCanFrame));

                    for(int x = 0; x < bytesReceived; x++ )
                    {
                        printf("%02x ", buffer[x]);
                    }
                    printf("\n");

                    CannelloniDataPacket tmp{};
                    memcpy(&tmp, buffer, sizeof(CannelloniDataPacket));
                    //tmp.count = ntohs(tmp.count);
                    const CannelloniDataPacket* pkt = &tmp;//reinterpret_cast<const CannelloniDataPacket*>(buffer);
                    if (pkt->version != this->cannelloniVersion)
                    {
                        printf("NOT CANNELLONI PACKET \n");
                        break;
                    }
                    else
                    {
                        printf("GOT CannelloniDataPacket ver %d op_code %d seq_no %d count %d\n ", pkt->version, pkt->op_code, pkt->seq_no, pkt->count );
                    }

                    size_t frameOffset = sizeof(CannelloniDataPacket);
                    size_t framesSize = bytesReceived - frameOffset;
                    if (framesSize < static_cast<size_t>(pkt->count) * sizeof(CannelloniCanFrame))
                    {
                        printf("Packet size too small for CannelloniCanFrame. Continue. ");
                        break;
                    }

                    const CannelloniCanFrame* frames = reinterpret_cast<const CannelloniCanFrame*>(buffer + frameOffset);
                    {
                        printf("GOT Cannelloni frames %d\n", pkt->count);

                        std::lock_guard<std::mutex> lock(ctx.msgQueueMutex);
                        for (uint16_t i = 0; i < pkt->count; ++i)
                        {
                            const auto& f = frames[i];
                            PASSTHRU_MSG msg{0};

                            msg.RxStatus = 0;  // No errors
                            msg.ProtocolID = CAN;
                            msg.DataSize = 4 + f.length;
                            if (msg.DataSize > sizeof(msg.Data))
                            {
                                printf("FRAME truncated\n");
                                continue;  // Truncate
                            }

                            uint32_t canId = f.can_id;
                            // J2534 CAN format: Byte 0 = flags (e.g., 0x08 extended), ID in 0-3
                            msg.Data[0] = static_cast<uint8_t>(canId & 0xFF);  // Assume 11-bit for now; add flags if extended
                            std::copy_n(reinterpret_cast<uint8_t*>(&canId) + 1, 3, msg.Data + 1);
                            std::copy_n(f.data, f.length, msg.Data + 4);

                            printf("[GOT FRAME] protid %08x rxstatus %08x txflags %08x tstamp %d e_index %08x datasize %d \n [MSG]: ",
                                   msg.ProtocolID,msg.RxStatus, msg.TxFlags,
                                   msg.Timestamp, msg.ExtraDataIndex, msg.DataSize);
                            for(int y = 0; y < msg.DataSize; y++)
                            {
                                printf("%02x ", msg.Data[y]);
                            }
                            printf("\n");

                            ctx.msgQueue.push(msg);
                        }
                    }
                    ctx.msgQueueCv.notify_one();
                }
            }
        }
    });
}

bool J2534CannelloniClient::popParsedMsg(ChannelContext& ctx, PASSTHRU_MSG& outMsg)
{
    std::unique_lock<std::mutex> lock(ctx.msgQueueMutex);
    if (!ctx.msgQueue.empty())
    {
        outMsg = std::move(ctx.msgQueue.front());
        ctx.msgQueue.pop();
        return true;
    }
    else
    {
        printf("[popParsedMsg] msgQueue empty...\n ");
    }
    return false;
}

long J2534CannelloniClient::PassThruOpen(const void* pName, unsigned long* pDeviceID)
{
    std::lock_guard<std::mutex> lock(globalMutex);

    if(pName != nullptr)printf("[PassThruOpen] %s\n", pName);

    auto msg = message("PassThruOpen");
    auto& call = msg.select(j2534_pipe_PipeMessage_open_tag, msg.msg().payload.open);

    if (pDeviceID == nullptr)
    {
        lastError = "pDeviceID is null";
        lastErrorLong = J2534Err::ERR_NULL_PARAMETER;

        msg.result(call, lastErrorLong, failure(__FUNCTION__, lastError));
        sendPipe(msg);

        return lastErrorLong;
    }

    unsigned long port = this->defaultPort;
    const char* host = "127.0.0.1";
    if (pName != nullptr)
    {
        if(isValidHostString(static_cast<const char*>(pName)))host = static_cast<const char*>(pName);
        else host = this->defaultHost.data();
        std::string tmpPort((const char *)pName);
        if(tmpPort.contains("20000"))port = 20000;
        else if(tmpPort.contains("20001"))port = 20001;
        else if(tmpPort.contains("20002"))port = 20002;
    }

    call.name = msg.str(std::string(host));

    static constexpr int numPorts = 3;
    static constexpr unsigned long basePort = 20000;

    if (channelMap.size() >= static_cast<size_t>(numPorts))
    {
        lastError = "All virtual devices are already opened";
        lastErrorLong = J2534Err::ERR_DEVICE_IN_USE;

        msg.result(call, lastErrorLong, failure(__FUNCTION__, lastError));
        sendPipe(msg);

        return lastErrorLong;
    }

    auto candidate = channelMap.find(port);
    if(candidate != channelMap.end())
    {
        printf("ChannelMap already contains port %d\n", port);
        lastError = "No free ports";
        lastErrorLong = J2534Err::ERR_DEVICE_IN_USE;

        msg.result(call, lastErrorLong, failure(__FUNCTION__, lastError));
        sendPipe(msg);

        return lastErrorLong;
    }

    // Create UDP socket
    SOCKET sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sockfd == INVALID_SOCKET)
    {
        lastError = "Socket creation failed: " + std::to_string(WSAGetLastError());
        lastErrorLong = J2534Err::ERR_DEVICE_NOT_CONNECTED;

        msg.result(call, lastErrorLong, failure(__FUNCTION__, lastError));
        sendPipe(msg);

        return lastErrorLong;
    }

    ChannelContext ctx;
    ctx.sockfd = sockfd;

    ctx.targetAddr.sin_family = AF_INET;
    ctx.targetAddr.sin_port = htons(static_cast<uint16_t>(port));

    if (InetPtonA(AF_INET, host, &ctx.targetAddr.sin_addr) != 1)
    {
        closesocket(sockfd);
        lastError = "Invalid host IP: " + std::string(host);
        lastErrorLong = J2534Err::ERR_INVALID_DEVICE_ID;

        msg.result(call, lastErrorLong, failure(__FUNCTION__, lastError));
        sendPipe(msg);

        return lastErrorLong;
    }

    // Connect the socket to remote for default peer
    if (connect(sockfd, reinterpret_cast<sockaddr*>(&ctx.targetAddr), sizeof(ctx.targetAddr)) == SOCKET_ERROR)
    {
        lastError = "Connect failed: " + std::to_string(WSAGetLastError());
        lastErrorLong = J2534Err::ERR_DEVICE_NOT_CONNECTED;

        msg.result(call, lastErrorLong, failure(__FUNCTION__, lastError));
        sendPipe(msg);
        closesocket(sockfd);
        return lastErrorLong;
    }

    // Send initial init packet (empty Cannelloni DATA packet) so ESP32 can capture client address
    CannelloniDataPacket initPkt;
    initPkt.version = this->cannelloniVersion;
    initPkt.op_code = 0;  // DATA = 0
    initPkt.seq_no = 0;
    initPkt.count = 0;
    int sent = send(sockfd, reinterpret_cast<const char*>(&initPkt), sizeof(initPkt), 0);
    if (sent != sizeof(initPkt))
    {
        printf("Warning: Failed to send init packet: %d\n", WSAGetLastError());
    }

    channelMap[port] = std::move(ctx);
    ChannelContext& refCtx = channelMap[port];
    startReceiveThread(refCtx);  // Now parses into PASSTHRU_MSG

    *pDeviceID = port;
    lastErrorLong = J2534Err::STATUS_NOERROR;

    call.device_id = *pDeviceID;
    msg.result(call, lastErrorLong);
    sendPipe(msg);

    return lastErrorLong;
}

long J2534CannelloniClient::PassThruClose(unsigned long DeviceID)
{
    std::lock_guard<std::mutex> lock(globalMutex);

    long retval = STATUS_NOERROR;

    auto msg = message("PassThruClose");
    auto& call = msg.select(j2534_pipe_PipeMessage_close_tag, msg.msg().payload.close);
    call.device_id = DeviceID;

    auto it = channelMap.find(DeviceID);
    if (it == channelMap.end())
    {
        lastError = "DeviceID not found";
        msg.result(call, ERR_INVALID_DEVICE_ID, failure(__FUNCTION__, lastError));
        sendPipe(msg);
        return ERR_INVALID_DEVICE_ID;
    }

    ChannelContext& ctx = it->second;

    ctx.stopRequested = true;

    // Wake up select() if needed by sending dummy data to self
    sockaddr_in dummyAddr = ctx.targetAddr;
    dummyAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    sendto(ctx.sockfd, "", 1, 0, (sockaddr*)&dummyAddr, sizeof(dummyAddr));

    ctx.stopRequested = true;
    if (ctx.recvThread.joinable())ctx.recvThread.join();

    closesocket(ctx.sockfd);

    channelMap.erase(it);

    msg.result(call, retval);
    sendPipe(msg);
    return retval;
}

long J2534CannelloniClient::PassThruConnect(unsigned long DeviceID, unsigned long ProtocolID, unsigned long Flags, unsigned long Baudrate, unsigned long* pChannelID) {
    std::lock_guard<std::mutex> lock(globalMutex);

    printf("[PassThruConnect] devID: %d ProtID: %d Flags: %d Baud: %d\n", DeviceID, ProtocolID, Flags, Baudrate);

    auto msg = message("PassThruConnect");
    auto& call = msg.select(j2534_pipe_PipeMessage_connect_tag, msg.msg().payload.connect);
    call.device_id = DeviceID;
    call.protocol_id = ProtocolID;
    call.flags = Flags;
    call.baudrate = Baudrate;

    auto it = channelMap.find(DeviceID);
    if (it == channelMap.end())
    {
        lastError = "Invalid DeviceID";
        lastErrorLong = J2534Err::ERR_INVALID_DEVICE_ID;
        msg.result(call, lastErrorLong, failure(__FUNCTION__, lastError));
        sendPipe(msg);
        return lastErrorLong;
    }

    // Only CAN and CAN_PS supported
    /*if (ProtocolID != CAN && ProtocolID != CAN_PS)
    {
        lastError = "Unsupported protocol";
        lastErrorLong = J2534Err::ERR_INVALID_PROTOCOL_ID;
        msg.result(call, lastErrorLong, failure(__FUNCTION__, lastError));
        sendPipe(msg);
        return lastErrorLong;
    }*/

    *pChannelID = DeviceID;  // Single channel == device
    lastErrorLong = J2534Err::STATUS_NOERROR;

    call.channel_id = *pChannelID;
    msg.result(call, lastErrorLong);
    sendPipe(msg);

    return lastErrorLong;
}

long J2534CannelloniClient::PassThruDisconnect(unsigned long ChannelID)
{
    long retval = STATUS_NOERROR;

    auto msg = message("PassThruDisconnect");
    auto& call = msg.select(j2534_pipe_PipeMessage_disconnect_tag, msg.msg().payload.disconnect);
    call.channel_id = ChannelID;
    msg.result(call, retval);
    sendPipe(msg);

    return retval;
}

long J2534CannelloniClient::PassThruReadMsgs(unsigned long ChannelID, PASSTHRU_MSG* pMsg, unsigned long* pNumMsgs, unsigned long Timeout)
{
    std::lock_guard<std::mutex> lock(globalMutex);

    auto msg = message("PassThruReadMsgs");
    auto& call = msg.select(j2534_pipe_PipeMessage_read_msgs_tag, msg.msg().payload.read_msgs);
    call.channel_id = ChannelID;
    call.num_msgs_requested = *pNumMsgs;
    call.timeout = Timeout;

    auto it = channelMap.find(ChannelID);
    if (it == channelMap.end())
    {
        lastError = "Invalid ChannelID";
        lastErrorLong = J2534Err::ERR_INVALID_CHANNEL_ID;

        printf("[PassThruReadMsgs] Invalid ChannelID %d\n", ChannelID);

        msg.result(call, lastErrorLong, failure(__FUNCTION__, lastError));
        sendPipe(msg);

        return lastErrorLong;
    }

    ChannelContext& ctx = it->second;
    unsigned long readCount = 0;
    auto startTime = std::chrono::steady_clock::now();

    //if Timeout nr set, just read msgs and return
    if(Timeout <= 0)
    {
        while (readCount < *pNumMsgs)
        {
            PASSTHRU_MSG msg;
            if (popParsedMsg(ctx, msg))
            {
                if (pMsg) pMsg[readCount] = msg;  // Copy
                ++readCount;
            }
            else
            {
                break;
            }
        }
    }
    else
    {
        while (readCount < *pNumMsgs)
        {
            PASSTHRU_MSG msg;
            auto timeoutLeft = Timeout - std::chrono::duration_cast<std::chrono::milliseconds>(
                                             std::chrono::steady_clock::now() - startTime).count();
            if (timeoutLeft <= 0)
            {
                printf("[PassThruReadMsgs] timeout %d \n", Timeout);
                break;
            }

            if (popParsedMsg(ctx, msg))
            {
                if (pMsg) pMsg[readCount] = msg;  // Copy
                ++readCount;
            }
            else
            {
                //if (ctx.stopRequested.load()) break;
                break;
            }
        }
    }

    *pNumMsgs = readCount;
    lastErrorLong = (readCount > 0 || *pNumMsgs == 0) ? J2534Err::STATUS_NOERROR : J2534Err::ERR_BUFFER_EMPTY;

    call.msgs = msg.msgs(pMsg, *pNumMsgs);
    msg.result(call, lastErrorLong);
    sendPipe(msg);

    return lastErrorLong;
}

long J2534CannelloniClient::PassThruQueueMsgs(unsigned long ChannelID, PASSTHRU_MSG *pMsg, unsigned long *pNumMsgs)
{
    long retval = ERR_NOT_SUPPORTED;

    auto msg = message("PassThruQueueMsgs");
    auto& call = msg.select(j2534_pipe_PipeMessage_queue_msgs_tag, msg.msg().payload.queue_msgs);
    call.channel_id = ChannelID;
    call.msgs = msg.msgs(pMsg, pNumMsgs ? *pNumMsgs : 0);
    msg.result(call, retval, not_implemented(__FUNCTION__));
    sendPipe(msg);

    return retval;
}

long J2534CannelloniClient::PassThruWriteMsgs(unsigned long ChannelID, const PASSTHRU_MSG* pMsg, unsigned long* pNumMsgs, unsigned long Timeout) {
    std::lock_guard<std::mutex> lock(globalMutex);

    auto msg = message("PassThruWriteMsgs");
    auto& call = msg.select(j2534_pipe_PipeMessage_write_msgs_tag, msg.msg().payload.write_msgs);
    call.channel_id = ChannelID;
    call.msgs = msg.msgs(pMsg, *pNumMsgs);
    call.timeout = Timeout;

    auto it = channelMap.find(ChannelID);
    if (it == channelMap.end())
    {
        lastError = "Invalid ChannelID";
        lastErrorLong = J2534Err::ERR_INVALID_CHANNEL_ID;

        msg.result(call, lastErrorLong, failure(__FUNCTION__, lastError));
        sendPipe(msg);
        return lastErrorLong;
    }

    auto& ctx = it->second;
    unsigned long sentMsgs = 0;
    std::string invalidMsgError;

    for (unsigned long i = 0; i < *pNumMsgs; ++i)
    {
        const auto& srcMsg = pMsg[i];

        printf("[WRITEMSG] protid %08x rxstatus %08x txflags %08x tstamp %d e_index %08x datasize %d \n [MSG]: ",
               srcMsg.ProtocolID,srcMsg.RxStatus, srcMsg.TxFlags,
               srcMsg.Timestamp, srcMsg.ExtraDataIndex, srcMsg.DataSize);
        for(int y = 0; y < srcMsg.DataSize; y++)
        {
            printf("%02x ", srcMsg.Data[y]);
        }
        printf("\n");

        const bool isCan = srcMsg.ProtocolID == CAN || srcMsg.ProtocolID == CAN_PS;
        if (!isCan || srcMsg.DataSize < 4 || srcMsg.DataSize > 4 + MAX_CAN_FRAME_DATA_LEN)
        {
            lastError = isCan ? "Invalid CAN msg format" : "Message protocol is not CAN";
            printf("[PassThruWriteMsgs] %s\n", lastError.data());
            lastErrorLong = isCan ? J2534Err::ERR_INVALID_MSG : J2534Err::ERR_MSG_PROTOCOL_ID;
            invalidMsgError = failure(__FUNCTION__, lastError);
            break; // Stop at the invalid message; the ones before it were sent
        }

        // Build frame
        CannelloniCanFrame frame{};
        uint32_t canId;
        std::copy_n(srcMsg.Data, 4, reinterpret_cast<uint8_t*>(&canId));  // Extract ID
        // Check for extended frame flag
        if (srcMsg.TxFlags & CAN_29BIT_ID)
        {
            canId |= CAN_EFF_FLAG;  // Set extended frame flag
        }
        frame.can_id = canId;  // Convert to network byte order
        frame.length = static_cast<uint8_t>(srcMsg.DataSize - 4);
        std::copy_n(srcMsg.Data + 4, frame.length, frame.data);

        // Build packet (single frame)
        CannelloniDataPacket pkt{this->cannelloniVersion, op_codes::DATA, ctx.seqNo.fetch_add(1), 1};  // version=1, op=DATA, seq, count=1 (network order)

        // Calculate exact buffer size
        size_t packetSize = sizeof(CannelloniDataPacket) + sizeof(frame.can_id) + sizeof(frame.length) + frame.length;
        std::vector<uint8_t> buffer(packetSize);

        // Copy packet header
        std::copy_n(reinterpret_cast<uint8_t*>(&pkt), sizeof(CannelloniDataPacket), buffer.begin());
        // Copy frame header and data
        size_t offset = sizeof(pkt);
        std::copy_n(reinterpret_cast<uint8_t*>(&frame.can_id), sizeof(frame.can_id), buffer.begin() + offset);
        offset += sizeof(frame.can_id);
        buffer[offset] = frame.length;
        offset += sizeof(frame.length);
        std::copy_n(frame.data, frame.length, buffer.begin() + offset);

        // Send packet
        int sent = sendto(ctx.sockfd, reinterpret_cast<const char*>(buffer.data()), buffer.size(), 0,
                          reinterpret_cast<sockaddr*>(&ctx.targetAddr), sizeof(ctx.targetAddr));
        if (sent == static_cast<int>(buffer.size()))
        {
            ++sentMsgs;
        }
        else
        {
            lastError = "Send failed: " + std::to_string(WSAGetLastError());
            lastErrorLong = J2534Err::ERR_FAILED;
            call.num_msgs_written = sentMsgs;
            msg.result(call, lastErrorLong, failure(__FUNCTION__, lastError));
            sendPipe(msg);
            return lastErrorLong;
        }
    }

    // On return *pNumMsgs is the number of messages actually transmitted.
    *pNumMsgs = sentMsgs;
    if (invalidMsgError.empty())
        lastErrorLong = J2534Err::STATUS_NOERROR;

    call.num_msgs_written = *pNumMsgs;
    msg.result(call, lastErrorLong, invalidMsgError);
    sendPipe(msg);

    return lastErrorLong;
}

long J2534CannelloniClient::PassThruStartPeriodicMsg(unsigned long ChannelID, const PASSTHRU_MSG *pMsg, unsigned long *pMsgID, unsigned long TimeInterval)
{
    long retval = ERR_NOT_SUPPORTED;

    auto msg = message("PassThruStartPeriodicMsg");
    auto& call = msg.select(j2534_pipe_PipeMessage_start_periodic_msg_tag, msg.msg().payload.start_periodic_msg);
    call.channel_id = ChannelID;
    msg.passthru_msg(call.has_msg, call.msg, pMsg);
    call.time_interval = TimeInterval;
    msg.result(call, retval, not_implemented(__FUNCTION__));
    sendPipe(msg);

    return retval;
}

long J2534CannelloniClient::PassThruStopPeriodicMsg(unsigned long ChannelID, unsigned long MsgID)
{
    long retval = ERR_NOT_SUPPORTED;

    auto msg = message("PassThruStopPeriodicMsg");
    auto& call = msg.select(j2534_pipe_PipeMessage_stop_periodic_msg_tag, msg.msg().payload.stop_periodic_msg);
    call.channel_id = ChannelID;
    call.msg_id = MsgID;
    msg.result(call, retval, not_implemented(__FUNCTION__));
    sendPipe(msg);

    return retval;
}

long J2534CannelloniClient::PassThruStartMsgFilter(unsigned long ChannelID, unsigned long FilterType, const PASSTHRU_MSG *pMaskMsg, const PASSTHRU_MSG *pPatternMsg, const PASSTHRU_MSG *pFlowControlMsg, unsigned long *pMsgID)
{
    long retval = ERR_NOT_SUPPORTED;

    auto msg = message("PassThruStartMsgFilter");
    auto& call = msg.select(j2534_pipe_PipeMessage_start_msg_filter_tag, msg.msg().payload.start_msg_filter);
    call.channel_id = ChannelID;
    call.filter_type = FilterType;
    msg.passthru_msg(call.has_mask, call.mask, pMaskMsg);
    msg.passthru_msg(call.has_pattern, call.pattern, pPatternMsg);
    msg.passthru_msg(call.has_flow_control, call.flow_control, pFlowControlMsg);
    msg.result(call, retval, not_implemented(__FUNCTION__));
    sendPipe(msg);

    return retval;
}

long J2534CannelloniClient::PassThruStopMsgFilter(unsigned long ChannelID, unsigned long MsgID)
{
    long retval = ERR_NOT_SUPPORTED;

    auto msg = message("PassThruStopMsgFilter");
    auto& call = msg.select(j2534_pipe_PipeMessage_stop_msg_filter_tag, msg.msg().payload.stop_msg_filter);
    call.channel_id = ChannelID;
    call.filter_id = MsgID;
    msg.result(call, retval, not_implemented(__FUNCTION__));
    sendPipe(msg);

    return retval;
}

long J2534CannelloniClient::PassThruSetProgrammingVoltage(unsigned long DeviceID, unsigned long Pin, unsigned long Voltage)
{
    long retval = ERR_NOT_SUPPORTED;

    auto msg = message("PassThruSetProgrammingVoltage");
    auto& call = msg.select(j2534_pipe_PipeMessage_set_programming_voltage_tag, msg.msg().payload.set_programming_voltage);
    call.device_id = DeviceID;
    call.pin = Pin;
    call.voltage = Voltage;
    msg.result(call, retval, not_implemented(__FUNCTION__));
    sendPipe(msg);

    return retval;
}

long J2534CannelloniClient::PassThruReadVersion(unsigned long DeviceID, char *pFirmwareVersion, char *pDllVersion, char *pApiVersion)
{
    long retval = STATUS_NOERROR;

    char FirmwareVersion[80] = {0};
    char DllVersion[80] = {0};
    char ApiVersion[80] = {0};

    //OpenPort:
    //api:  1.17.4877
    //dll:  1.01.4341 Jul 10 2014 14:16:35
    //firm: 04.04

    memcpy(ApiVersion, "04.04", 5);
    memcpy(DllVersion, "1.0.001 Feb 28 2023 12:46:00", 28);
    memcpy(FirmwareVersion, "04.04", 5);

    memcpy(pApiVersion, ApiVersion, 80);
    memcpy(pDllVersion, DllVersion, 80);
    memcpy(pFirmwareVersion, FirmwareVersion, 80);

    printf("[PassThruReadVersion] DeviceID %lu firmw %s dllver %s api %s\n", DeviceID, pFirmwareVersion, pDllVersion, pApiVersion);

    auto msg = message("PassThruReadVersion");
    auto& call = msg.select(j2534_pipe_PipeMessage_read_version_tag, msg.msg().payload.read_version);
    call.device_id = DeviceID;
    call.firmware_version = msg.str(std::string(FirmwareVersion));
    call.dll_version = msg.str(std::string(DllVersion));
    call.api_version = msg.str(std::string(ApiVersion));
    msg.result(call, retval);
    sendPipe(msg);

    return retval;
}

// In j2534cannelloniclient.cpp (implement/update PassThruGetLastError)
long J2534CannelloniClient::PassThruGetLastError(char* pErrorDescription)
{
    std::lock_guard<std::mutex> lock(globalMutex);  // Thread-safe access to lastError/lastErrorLong

    auto msg = message("PassThruGetLastError");
    auto& call = msg.select(j2534_pipe_PipeMessage_get_last_error_tag, msg.msg().payload.get_last_error);

    if (pErrorDescription == nullptr) {
        lastError = "pErrorDescription is null";
        msg.result(call, J2534Err::ERR_NULL_PARAMETER, failure(__FUNCTION__, lastError));
        sendPipe(msg);
        return J2534Err::ERR_NULL_PARAMETER;
    }

    // Fill buffer with lastError string (up to 80 chars, null-terminated)
    std::strncpy(pErrorDescription, lastError.c_str(), 79);
    pErrorDescription[79] = '\0';

    // Return lastErrorLong (0 for no error, else J2534 error code)
    long retval = (lastErrorLong == 0) ? J2534Err::STATUS_NOERROR : lastErrorLong;

    call.error_description = msg.str(std::string(pErrorDescription));
    msg.result(call, retval);
    sendPipe(msg);

    return retval;
}

long J2534CannelloniClient::PassThruIoctl(unsigned long ChannelID, unsigned long IoctlID, const void *pInput, void *pOutput)
{
    long retval = STATUS_NOERROR;

    auto msg = message("PassThruIoctl");
    auto& call = msg.select(j2534_pipe_PipeMessage_ioctl_tag, msg.msg().payload.ioctl);
    call.channel_id = ChannelID;
    call.ioctl_id = IoctlID;

    unsigned long data_rate = 500000;
    if(ChannelID == 20000)data_rate = 125000;

    // TODO: check IoctlID?
    // 3 READ_VBATT?
    // 2 CONFIG_SET?
    printf("PassThruIoctl Channel or Device %d IoctlID = %08x\n", ChannelID, IoctlID);

    if (IoctlID == READ_VBATT)
    {
        //unsigned int voltageLevel = 14400;
        //memcpy(pOutput, &voltageLevel, sizeof(unsigned int));

        unsigned int *voltageLevel = (unsigned int*)pOutput;
        *voltageLevel = 12800; // The units will be in milli-volts and will be rounded to the nearest tenth of a volt.
    }
    else if(IoctlID == READ_PROG_VOLTAGE)
    {
        printf("[READ_PROG_VOLTAGE] \n");
        unsigned int *voltageLevel = (unsigned int*)pOutput;
        *voltageLevel = 5200; // The units will be in milli-volts and will be rounded to the nearest tenth of a volt.
    }
    else if(IoctlID == READ_PIN_VOLTAGE)
    {
        printf("[READ_PIN_VOLTAGE] \n");
        unsigned int *voltageLevel = (unsigned int*)pOutput;

        if(pInput == NULL)
        {
            printf("READ_PIN_VOLTAGE with no Input? \n");
            *voltageLevel = 0;
        }
        else
        {
            unsigned int *pin = (unsigned int*)pInput;
            printf("[READ_PIN_VOLTAGE] PIN Number: %d \n", *pin);
            *voltageLevel = 5000;
        }
    }
    else if(IoctlID == SET_CONFIG)
    {
        printf("[SET_CONFIG] \n");
        if(pInput == NULL)
        {
            printf("SET_CONFIG with no Input? \n");
        }
        else
        {
            const SCONFIG_LIST * input = reinterpret_cast<const SCONFIG_LIST *>(pInput);
            printf("SCONFIG_LIST NumOfParams %d\n", input->NumOfParams);
            for(int i = 0; i < input->NumOfParams; i++)
            {
                printf("ConfigPtr %d paramter %08x value %08x \n", i, input->ConfigPtr[i].Parameter, input->ConfigPtr[i].Value);
            }
        }
    }
    else if(IoctlID == GET_DEVICE_INFO)
    {
        auto param_list = (SPARAM_LIST*)pOutput;

        printf("[GET_DEVICE_INFO] SPARAM_LIST NumOfParameters %d\n", param_list->NumOfParameters);
        for(int i = 0; i < param_list->NumOfParameters; i++)
        {
            printf("SParamPtr %d paramter %08x value %08x supported %08x \n", i, param_list->SParamPtr[i].Parameter, param_list->SParamPtr[i].Value, param_list->SParamPtr[i].Supported);
            switch(param_list->SParamPtr[i].Parameter)
            {
            case SERIAL_NUMBER:
                printf("[GET_DEVICE_INFO][SERIAL_NUMBER] \n");
                param_list->SParamPtr[i].Supported = 1;
                param_list->SParamPtr[i].Value = 987654321;
                break;
            case CAN_SUPPORTED:
                printf("[GET_DEVICE_INFO][CAN_SUPPORTED] \n");
                param_list->SParamPtr[i].Supported = 1;
                break;
            case SHORT_TO_GND_J1962:
                printf("[GET_DEVICE_INFO][SHORT_TO_GND_J1962] %08x\n", param_list->SParamPtr[i].Value);
                if(param_list->SParamPtr[i].Value == 0)param_list->SParamPtr[i].Supported = 1;
                break;
            case PGM_VOLTAGE_J1962:
                printf("[GET_DEVICE_INFO][PGM_VOLTAGE_J1962] %08x\n", param_list->SParamPtr[i].Value);
                param_list->SParamPtr[i].Supported = 1;
                break;
            }
        }
    }
    else if(IoctlID == GET_CONFIG)
    {
        printf("[GET_CONFIG] \n");
        if(pInput == NULL)
        {
            printf("GET_CONFIG with no Input? \n");
        }
        else
        {
            const SCONFIG_LIST * input = reinterpret_cast<const SCONFIG_LIST *>(pInput);
            printf("GET_CONFIG NumOfParams %d\n", input->NumOfParams);
            for(int i = 0; i < input->NumOfParams; i++)
            {
                printf("ConfigPtr %d paramter %08x value %08x \n", i, input->ConfigPtr[i].Parameter, input->ConfigPtr[i].Value);
                switch(input->ConfigPtr[i].Parameter)
                {
                case ConfigParamId::DATA_RATE:
                    printf("[GET_CONFIG][DATA_RATE] \n");

                    input->ConfigPtr[i].Value = data_rate;
                    break;
                case ConfigParamId::DATA_BITS:
                    printf("[GET_CONFIG][DATA_BITS] \n");
                    break;
                case ConfigParamId::LOOPBACK:
                    printf("[GET_CONFIG][LOOPBACK] \n");
                    break;
                case ConfigParamId::BIT_SAMPLE_POINT:
                    printf("[GET_CONFIG][BIT_SAMPLE_POINT] \n");
                    break;
                case ConfigParamId::SYNC_JUMP_WIDTH:
                    printf("[GET_CONFIG][SYNC_JUMP_WIDTH] \n");
                    break;
                }
            }
        }
    }

    msg.result(call, retval);
    sendPipe(msg);
    return retval;
}




///DREW Tech extra functions
///
long J2534CannelloniClient::vendorNotImplemented(j2534_pipe_VendorFunction function, const char* name)
{
    long retval = ERR_NOT_SUPPORTED;

    auto msg = message(name);
    auto& call = msg.select(j2534_pipe_PipeMessage_vendor_tag, msg.msg().payload.vendor);
    call.vendor_function = function;
    msg.result(call, retval, not_implemented(name));
    sendPipe(msg);

    return retval;
}

long J2534CannelloniClient::PassThruExConfigureWiFi ( void )
{
    return vendorNotImplemented(j2534_pipe_VendorFunction_VENDOR_FUNCTION_EX_CONFIGURE_WIFI, "PassThruExConfigureWiFi");
}

long J2534CannelloniClient::PassThruExDeviceWatchdog ( void )
{
    return vendorNotImplemented(j2534_pipe_VendorFunction_VENDOR_FUNCTION_EX_DEVICE_WATCHDOG, "PassThruExDeviceWatchdog");
}

long J2534CannelloniClient::PassThruExDownloadCypheredFlashData ( void )
{
    return vendorNotImplemented(j2534_pipe_VendorFunction_VENDOR_FUNCTION_EX_DOWNLOAD_CYPHERED_FLASH_DATA, "PassThruExDownloadCypheredFlashData");
}

long J2534CannelloniClient::PassThruExEraseFlash ( void )
{
    return vendorNotImplemented(j2534_pipe_VendorFunction_VENDOR_FUNCTION_EX_ERASE_FLASH, "PassThruExEraseFlash");
}

long J2534CannelloniClient::PassThruExInitiateCypheredFlashDownload ( void )
{
    return vendorNotImplemented(j2534_pipe_VendorFunction_VENDOR_FUNCTION_EX_INITIATE_CYPHERED_FLASH_DOWNLOAD, "PassThruExInitiateCypheredFlashDownload");
}

long J2534CannelloniClient::PassThruExReadFlash ( void )
{
    return vendorNotImplemented(j2534_pipe_VendorFunction_VENDOR_FUNCTION_EX_READ_FLASH, "PassThruExReadFlash");
}

long J2534CannelloniClient::PassThruExResetFlash ( void )
{
    return vendorNotImplemented(j2534_pipe_VendorFunction_VENDOR_FUNCTION_EX_RESET_FLASH, "PassThruExResetFlash");
}

long J2534CannelloniClient::PassThruExRunSelfTest ( void )
{
    return vendorNotImplemented(j2534_pipe_VendorFunction_VENDOR_FUNCTION_EX_RUN_SELF_TEST, "PassThruExRunSelfTest");
}

long J2534CannelloniClient::PassThruExWriteFlash ( void )
{
    return vendorNotImplemented(j2534_pipe_VendorFunction_VENDOR_FUNCTION_EX_WRITE_FLASH, "PassThruExWriteFlash");
}

long J2534CannelloniClient::PassThruLoadFirmware(void)
{
    return vendorNotImplemented(j2534_pipe_VendorFunction_VENDOR_FUNCTION_LOAD_FIRMWARE, "PassThruLoadFirmware");
}

long J2534CannelloniClient::PassThruRecoverFirmware(void)
{
    return vendorNotImplemented(j2534_pipe_VendorFunction_VENDOR_FUNCTION_RECOVER_FIRMWARE, "PassThruRecoverFirmware");
}

long J2534CannelloniClient::PassThruReadIPSetup(unsigned long DeviceID, char *host_name, char *ip_addr, char *subnet_mask, char *gateway, char *dhcp_addr)
{
    long retval = ERR_NOT_SUPPORTED;

    auto msg = message("PassThruReadIPSetup");
    auto& call = msg.select(j2534_pipe_PipeMessage_read_ip_setup_tag, msg.msg().payload.read_ip_setup);
    call.device_id = DeviceID;
    msg.result(call, retval, not_implemented(__FUNCTION__));
    sendPipe(msg);

    return retval;
}

long J2534CannelloniClient::PassThruWriteIPSetup(unsigned long DeviceID, char *host_name, char *ip_addr, char *subnet_mask, char *gateway, char *dhcp_addr)
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
    msg.result(call, retval, not_implemented(__FUNCTION__));
    sendPipe(msg);

    return retval;
}

long J2534CannelloniClient::PassThruReadPCSetup(char *host_name, char *ip_addr)
{
    long retval = ERR_NOT_SUPPORTED;

    auto msg = message("PassThruReadPCSetup");
    auto& call = msg.select(j2534_pipe_PipeMessage_read_pc_setup_tag, msg.msg().payload.read_pc_setup);
    msg.result(call, retval, not_implemented(__FUNCTION__));
    sendPipe(msg);

    return retval;
}

long J2534CannelloniClient::PassThruGetPointer(long vb_pointer)
{
    long retval = ERR_NOT_SUPPORTED;

    auto msg = message("PassThruGetPointer");
    auto& call = msg.select(j2534_pipe_PipeMessage_get_pointer_tag, msg.msg().payload.get_pointer);
    call.vb_pointer = vb_pointer;
    msg.result(call, retval, not_implemented(__FUNCTION__));
    sendPipe(msg);

    return retval;
}

long J2534CannelloniClient::PassThruGetNextCarDAQ(char **name, unsigned long *version, char **addr)
{
    std::lock_guard<std::mutex> lock(globalMutex);

    auto msg = message("PassThruGetNextCarDAQ");
    auto& call = msg.select(j2534_pipe_PipeMessage_get_next_cardaq_tag, msg.msg().payload.get_next_cardaq);

    if (name == nullptr || version == nullptr || addr == nullptr)
    {
        lastError = "Null parameter(s)";
        msg.result(call, J2534Err::ERR_NULLPARAMETER, failure(__FUNCTION__, lastError));
        sendPipe(msg);

        return J2534Err::ERR_NULLPARAMETER;
    }

    static constexpr const char* DEFAULT_HOST = "127.0.0.1";


    if (scanIndex >= static_cast<int>(VIRTUAL_DEVICE_COUNT))
    {
        msg.result(call, ERR_NO_MORE_DEVICES);
        sendPipe(msg);
        return ERR_NO_MORE_DEVICES;
    }

    // Allocate strings with LocalAlloc (caller frees with LocalFree; standard for J2534 extensions)
    // Name: Descriptive string
    unsigned long port = getPortFromIndex(scanIndex);
    std::array<char, 80> deviceName{0};  // Modern fixed buffer
    std::snprintf(deviceName.data(), deviceName.size(), "Cannelloni UDP Port %lu (CAN)", port);

    std::string deviceNameStr;
    deviceNameStr.append(deviceName.data());

    size_t nameLen = deviceNameStr.length() + 1;
    *name = static_cast<char*>(LocalAlloc(LMEM_FIXED, nameLen));
    if (*name == nullptr)
    {
        lastError = "LocalAlloc failed for name";
        msg.result(call, J2534Err::ERR_NULLPARAMETER, failure(__FUNCTION__, lastError));
        sendPipe(msg);
        return J2534Err::ERR_NULLPARAMETER;
    }
    //std::strcpy_s(*name, nameLen, deviceName.c_str());
    std::strncpy(*name, deviceNameStr.c_str(), nameLen - 1);
    (*name)[nameLen - 1] = '\0';

    // Version: Unsigned long, e.g., 100 for 1.00
    *version = 100UL;  // Or tie to DLL version; could be 10404 for API 04.04

    // Addr: IP address string
    std::string deviceAddr = DEFAULT_HOST;  // Default; could scan channelMap for open host if needed
    size_t addrLen = deviceAddr.length() + 1;
    *addr = static_cast<char*>(LocalAlloc(LMEM_FIXED, addrLen));
    if (*addr == nullptr)
    {
        LocalFree(*name);  // Cleanup on failure
        *name = nullptr;
        lastError = "LocalAlloc failed for addr";
        msg.result(call, J2534Err::ERR_NULLPARAMETER, failure(__FUNCTION__, lastError));
        sendPipe(msg);
        return J2534Err::ERR_NULLPARAMETER;
    }
    //std::strcpy_s(*addr, addrLen, deviceAddr.c_str());
    std::strncpy(*addr, deviceAddr.c_str(), addrLen - 1);
    (*addr)[addrLen - 1] = '\0';

    ++scanIndex;

    call.name = msg.str(deviceNameStr);
    call.version = *version;
    call.addr = msg.str(deviceAddr);
    msg.result(call, J2534Err::STATUS_NOERROR);
    sendPipe(msg);

    return J2534Err::STATUS_NOERROR;
}

///v05.00
// In j2534cannelloniclient.cpp
long J2534CannelloniClient::PassThruScanForDevices(unsigned long* pDeviceCount)
{
    std::lock_guard<std::mutex> lock(globalMutex);

    auto msg = message("PassThruScanForDevices");
    auto& call = msg.select(j2534_pipe_PipeMessage_scan_for_devices_tag, msg.msg().payload.scan_for_devices);

    if (pDeviceCount == nullptr) {
        lastError = "pDeviceCount is null";
        msg.result(call, J2534Err::ERR_NULL_PARAMETER, failure(__FUNCTION__, lastError));
        sendPipe(msg);
        return J2534Err::ERR_NULL_PARAMETER;
    }

    *pDeviceCount = VIRTUAL_DEVICE_COUNT;
    scanIndex = 0;  // Reset enumeration

    call.device_count = *pDeviceCount;
    msg.result(call, J2534Err::STATUS_NOERROR);
    sendPipe(msg);

    return J2534Err::STATUS_NOERROR;
}

long J2534CannelloniClient::PassThruGetNextDevice(SDEVICE* psDevice) {
    std::lock_guard<std::mutex> lock(globalMutex);

    auto msg = message("PassThruGetNextDevice");
    auto& call = msg.select(j2534_pipe_PipeMessage_get_next_device_tag, msg.msg().payload.get_next_device);

    if (psDevice == nullptr) {
        lastError = "psDevice is null";
        msg.result(call, J2534Err::ERR_NULL_PARAMETER, failure(__FUNCTION__, lastError));
        sendPipe(msg);
        return J2534Err::ERR_NULL_PARAMETER;
    }

    if (scanIndex >= static_cast<int>(VIRTUAL_DEVICE_COUNT)) {
        msg.result(call, ERR_NO_MORE_DEVICES);
        sendPipe(msg);
        return ERR_NO_MORE_DEVICES;
    }

    // Fill SDEVICE for this virtual device
    unsigned long port = getPortFromIndex(scanIndex);
    bool isOpen = (channelMap.find(port) != channelMap.end());
    std::array<char, 80> name{};  // Modern fixed buffer
    std::snprintf(name.data(), name.size(), "Cannelloni UDP Port %lu (CAN)", port);
    std::strncpy(psDevice->DeviceName, name.data(), sizeof(psDevice->DeviceName) - 1);
    psDevice->DeviceName[sizeof(psDevice->DeviceName) - 1] = '\0';

    psDevice->DeviceAvailable = isOpen ? 0UL : 1UL;  // 1 = free to open
    psDevice->DeviceDLLFWStatus = 0UL;  // Compatible
    psDevice->DeviceConnectMedia = 3UL;  // Ethernet
    psDevice->DeviceConnectSpeed = 1000000000UL;  // 1 Gbps
    psDevice->DeviceSignalQuality = 100UL;  // Perfect (virtual)
    psDevice->DeviceSignalStrength = 100UL;

    ++scanIndex;

    call.has_device = true;
    call.device.name = msg.str(std::string(psDevice->DeviceName));
    call.device.available = psDevice->DeviceAvailable;
    call.device.dll_fw_status = psDevice->DeviceDLLFWStatus;
    call.device.connect_media = psDevice->DeviceConnectMedia;
    call.device.connect_speed = psDevice->DeviceConnectSpeed;
    call.device.signal_quality = psDevice->DeviceSignalQuality;
    call.device.signal_strength = psDevice->DeviceSignalStrength;
    msg.result(call, J2534Err::STATUS_NOERROR);
    sendPipe(msg);

    return J2534Err::STATUS_NOERROR;
}

long J2534CannelloniClient::PassThruSelect(SCHANNELSET* ChannelSetPtr, unsigned long SelectType, unsigned long Timeout) {
    std::lock_guard<std::mutex> lock(globalMutex);  // Note: For polling, release/reacquire per channel if contention

    auto msg = message("PassThruSelect");
    auto& call = msg.select(j2534_pipe_PipeMessage_select_tag, msg.msg().payload.select);
    call.select_type = SelectType;
    call.timeout = Timeout;

    if (ChannelSetPtr == nullptr) {
        lastError = "ChannelSetPtr is null";
        msg.result(call, J2534Err::ERR_NULL_PARAMETER, failure(__FUNCTION__, lastError));
        sendPipe(msg);
        return J2534Err::ERR_NULL_PARAMETER;
    }

    call.channel_ids = msg.uint32_list(ChannelSetPtr->ChannelList, ChannelSetPtr->ChannelCount);
    call.channel_threshold = ChannelSetPtr->ChannelThreshold;

    if (ChannelSetPtr->ChannelCount == 0 || ChannelSetPtr->ChannelList == nullptr) {
        lastError = "Invalid ChannelSet (empty list)";
        msg.result(call, J2534Err::ERR_INVALID_CHANNEL_ID, failure(__FUNCTION__, lastError));
        sendPipe(msg);
        return J2534Err::ERR_INVALID_CHANNEL_ID;
    }

    // Ignore SelectType or validate (e.g., if !=0 return ERR_NOT_SUPPORTED)
    auto startTime = std::chrono::steady_clock::now();
    unsigned long readyCount = 0;
    bool done = false;

    while (!done) {
        readyCount = 0;
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count();

        if (elapsed >= static_cast<long long>(Timeout)) {
            done = true;  // Timeout
        } else {
            // Poll each channel
            for (unsigned long i = 0; i < ChannelSetPtr->ChannelCount; ++i) {
                unsigned long chanId = ChannelSetPtr->ChannelList[i];
                auto it = channelMap.find(chanId);
                if (it != channelMap.end()) {
                    std::lock_guard<std::mutex> chanLock(it->second.msgQueueMutex);
                    if (!it->second.msgQueue.empty()) {
                        ++readyCount;
                    }
                }
            }

            if (readyCount >= ChannelSetPtr->ChannelThreshold) {
                done = true;  // Condition met
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));  // Poll interval
            }
        }
    }

    // Return readyCount (even on timeout, per spec)
    long retval = (readyCount > 0) ? static_cast<long>(readyCount) : J2534Err::ERR_TIMEOUT;

    msg.result(call, retval);
    sendPipe(msg);

    return retval;
}

long J2534CannelloniClient::PassThruReadDetails(unsigned long* pName)
{
    long retval = ERR_NOT_SUPPORTED;

    auto msg = message("PassThruReadDetails");
    auto& call = msg.select(j2534_pipe_PipeMessage_read_details_tag, msg.msg().payload.read_details);
    if (pName) call.name = *pName;
    msg.result(call, retval, not_implemented(__FUNCTION__));
    sendPipe(msg);

    return retval;
}

bool J2534CannelloniClient::isValidHostString(const char* str) const
{
    if (!str) return false;

    // Basic checks: Non-empty, null-terminated, reasonable length (e.g., IP <=15 chars)
    size_t len = strlen(str);
    if (len == 0 || len > 15) return false;

    // Printable ASCII (no control chars/garbage)
    for (size_t i = 0; i < len; ++i) {
        if (!isprint(static_cast<unsigned char>(str[i]))) return false;
    }

    // Optional: Loose IP validation (dots, digits) – skip for now, or use InetPton later
    if (InetPtonA(AF_INET, str, nullptr) != 1) return false;  // Strict IP only

    return true;
}
