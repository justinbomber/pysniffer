#include <iostream>
#include <netinet/in.h>
#include <thread>
#include <chrono>
#include <time.h>
#include <fstream>
#include <sys/stat.h>
#include <vector>
#include <pcapplusplus/PcapLiveDevice.h>
#include <pcapplusplus/PcapLiveDeviceList.h>
#include <pcapplusplus/SystemUtils.h>
#include <pcapplusplus/IPv4Layer.h>
#include <pcapplusplus/TcpLayer.h>
#include <pcapplusplus/UdpLayer.h>
#include <pcapplusplus/Packet.h>
#include <map>
#include <string>
#include <nlohmann/json.hpp>
#include <queue>
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <pqxx/pqxx>
#include <filesystem>

int64_t packetCount = 0;
time_t lasttimestamp = 0;
std::string g_ipaddr = "";
std::string g_partition = "";
std::queue<nlohmann::json> jsonQueue;
std::map<std::string, std::string> guidmap;
std::queue<pcpp::Packet> packetQueue1;
std::queue<pcpp::Packet> packetQueue2;
std::queue<pcpp::Packet> specialPacketQueue;
pqxx::connection *connection = nullptr;
short byte_fix = 0;

bool opendatabase(std::string dbName, std::string username, std::string password, std::string hostAddress, int port)
{
    bool result = false;

    try
    {
        std::stringstream ss;
        ss << "dbname=" << dbName
           << " user=" << username
           << " password=" << password
           << " hostaddr=" << hostAddress
           << " port=" << std::to_string(port);

        connection = new pqxx::connection(ss.str());
        if (connection->is_open())
        {
            result = true;
            std::cout << "Open database successfully." << std::endl;
        }
        else
        {
            result = false;
            std::cout << "Can't open database." << std::endl;
        }
    }
    catch(const std::exception& e)
    {
        result = false;
        std::cerr << e.what() << '\n';
    }
    
    return result;
}

bool closedatabase()
{
    bool result = false;

    try
    {
        if (connection->is_open()) connection->disconnect();

        result = true;
    }
    catch(const std::exception& e)
    {
        result = false;
        std::cerr << e.what() << '\n';
    }

    return result;
}

pqxx::result executeResultset(std::string command, bool useTransaction)
{
    pqxx::result result;

    try
    {
        if (useTransaction)
        {
            pqxx::work tran(*connection);
            try
            {
                result = tran.exec(command);
                tran.commit();
            }
            catch(const std::exception& e)
            {
                tran.abort();
                std::cerr << e.what() << '\n';
            }
        }
        else
        {
            pqxx::nontransaction nonTran(*connection);
            result = nonTran.exec(command);
        }
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
    }
    
    return result;
}


void guidmanager(const std::string connection_url)
{
    while (true)
    {
        std::string sqlcmd = "";
        if (opendatabase(connection_url, "reader", "reader", "127.0.0.1", 5432))
        {
            pqxx::result guidtableresult = executeResultset(sqlcmd, true);
            // TODO:search database
            for (auto const & row : guidtableresult)
            {
                std::string dev_partition;
                std::string guid;
                guidmap.emplace(guid, dev_partition);
                for (auto partitioniter = guidmap.begin(); partitioniter != guidmap.end();)
                {
                    if (partitioniter->second ==  dev_partition && partitioniter->first != guid)
                        partitioniter = guidmap.erase(partitioniter);
                    else
                        ++partitioniter;
                }
            }

            if(closedatabase())
                std::cout << "Close database successfully." << std::endl;
            else
                std::cout << "Can't close database." << std::endl;
        } else {
            std::cout << "Can't open database." << std::endl;
            continue;
        }
        
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
}

struct RTPS_DATA_STRUCTURE
{
    // udp info
    // uint16_t udp_src_port;
    // uint16_t udp_dst_port;
    uint16_t udp_length;
    // std::string udp_checksum;
    // magic
    // std::string rtps_magic;
    // rtps
    // uint8_t rtps_ver_major;
    // uint8_t rtps_ver_minor;
    // std::string rtps_vendorId;
    // guid
    std::string rtps_hostid;
    std::string rtps_appid;
    std::string rtps_instanceid;
    std::string rtps_writer_entitykey;

    time_t timestamp;
};

std::string byteArrayToHexString(const uint8_t *byteArray, size_t arraySize)
{
    std::ostringstream stream;
    for (size_t i = 0; i < arraySize; ++i)
    {
        stream << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byteArray[i]);
    }
    return stream.str();
}

uint64_t byteArrayToDecimal(const uint8_t *bytes, size_t arraySize)
{
    uint64_t total = 0;
    for (size_t i = 0; i < arraySize; ++i)
    {
        total = (total << 8) | bytes[i];
    }
    return total;
}

std::string byteArrayToAsciiString(const uint8_t *byteArray, size_t arraySize)
{
    std::string asciiString;
    for (size_t i = 0; i < arraySize; ++i)
    {
        char character = static_cast<char>(byteArray[i]);
        asciiString += character;
    }
    return asciiString;
}

int totalsize_cal(uint16_t rawsize)
{
    int packet_count = floor(rawsize / 1480);
    int newrawsize = (packet_count + 1) * 34 + rawsize - byte_fix * (packet_count + 1);
    return newrawsize;
}

RTPS_DATA_STRUCTURE convertrtpspacket(pcpp::Packet &packet, int index)
{
    RTPS_DATA_STRUCTURE rtps_data;
    int udpidx = index - 8;

    const uint8_t *payload = packet.getLayerOfType<pcpp::IPv4Layer>()->getLayerPayload();
    // udp info
    // rtps_data.udp_src_port = byteArrayToDecimal(payload + udpidx, 2);
    // rtps_data.udp_dst_port = byteArrayToDecimal(payload + udpidx + 2, 2);
    rtps_data.udp_length = byteArrayToDecimal(payload + udpidx + 4, 2);
    // rtps_data.udp_checksum = byteArrayToHexString(payload + udpidx + 6, 2);

    // rtps info
    // rtps_data.rtps_magic = byteArrayToHexString(payload + index, 4);
    // rtps_data.rtps_ver_major = payload[index + 4];
    // rtps_data.rtps_ver_minor = payload[index + 5];
    // rtps_data.rtps_vendorId = byteArrayToHexString(payload + index + 6, 2);

    // rtps guid
    rtps_data.rtps_hostid = byteArrayToHexString(payload + index + 8, 4);
    rtps_data.rtps_appid = byteArrayToHexString(payload + index + 12, 4);
    rtps_data.rtps_instanceid = byteArrayToHexString(payload + index + 16, 4);
    rtps_data.rtps_writer_entitykey = byteArrayToHexString(payload + index + 44, 4);

    rtps_data.timestamp = packet.getRawPacket()->getPacketTimeStamp().tv_sec;

    return rtps_data;
}

std::string extractData(uint8_t *payload, size_t length)
{
    std::vector<uint8_t> startPattern = {0x07, 0x00, 0x00, 0x00};
    std::vector<uint8_t> endPattern = {0x00, 0x00, 0x01, 0x00};

    // 尋找開始模式
    size_t startPos = std::string::npos;
    for (size_t i = 0; i <= length - startPattern.size(); ++i)
    {
        if (std::equal(startPattern.begin(), startPattern.end(), &payload[i]))
        {
            startPos = i + startPattern.size();
            break;
        }
    }

    // 若未找到開始模式，返回空字符串
    if (startPos == std::string::npos)
    {
        return "no_partition";
    }

    // 尋找結束模式
    size_t endPos = std::string::npos;
    for (size_t i = startPos; i <= length - endPattern.size(); ++i)
    {
        if (std::equal(endPattern.begin(), endPattern.end(), &payload[i]))
        {
            endPos = i;
            break;
        }
    }

    // 若未找到結束模式，或者開始和結束位置重疊，返回空字符串
    if (endPos == std::string::npos || endPos <= startPos)
    {
        return "no_partition";
    }

    // 提取並轉換數據
    std::string result;
    for (size_t i = startPos; i < endPos; ++i)
    {
        char ch = static_cast<char>(payload[i]);

        // 檢查是否為 ASCII 字符
        if (ch >= 0 && ch <= 127)
        {
            // 是 ASCII 字符
            result += ch;
        }
        else
        {
            // 不是 ASCII 字符
            return "no_partition";
        }
    }

    return result;
}

static void start_subcap(std::string ipaddr);

void rtpscallback(int idx, std::queue<pcpp::Packet> &packetQueue)
{
    while (true)
    {
        if (!packetQueue.empty())
        {
            pcpp::Packet packet = packetQueue.front();
            packetQueue.pop();
            RTPS_DATA_STRUCTURE rtps_data = convertrtpspacket(packet, idx);

            std::string guid = rtps_data.rtps_hostid +
                               rtps_data.rtps_appid +
                               rtps_data.rtps_instanceid +
                               rtps_data.rtps_writer_entitykey;

            // get time
            time_t timestamp = rtps_data.timestamp;

            // get traffic
            int32_t rtps_content = totalsize_cal(rtps_data.udp_length);

            nlohmann::json json_obj;
            json_obj["timestamp"] = timestamp;
            json_obj["dev_partition"] = guid;
            json_obj["total_traffic"] = rtps_content;
            jsonQueue.push(json_obj);
        }
        else
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }
}

std::vector<int> computeLPSArray(const uint8_t *pattern, int M)
{
    std::vector<int> lps(M, 0);
    int len = 0;
    int i = 1;
    while (i < M)
    {
        if (pattern[i] == pattern[len])
        {
            lps[i] = ++len;
            ++i;
        }
        else
        {
            if (len != 0)
            {
                len = lps[len - 1];
            }
            else
            {
                lps[i] = 0;
                ++i;
            }
        }
    }
    return lps;
}

int KMPSearch(const uint8_t *pattern, int M, const uint8_t *txt, int N)
{
    std::vector<int> lps = computeLPSArray(pattern, M);

    int i = 0;
    int j = 0;
    while (i < N)
    {
        if (i - j > 50)
            return -1;
        if (pattern[j] == txt[i])
        {
            ++j;
            ++i;
        }
        if (j == M)
        {
            // std::cout << "Pattern found at index " << i - j << std::endl;
            return i - j;
            j = lps[j - 1];
        }
        else if (i < N && pattern[j] != txt[i])
        {
            if (j != 0)
            {
                j = lps[j - 1];
            }
            else
            {
                ++i;
            }
        }
    }
    return -1;
}

struct PacketStats
{
    void dictcallback(pcpp::Packet &packet)
    {
        if (packet.isPacketOfType(pcpp::IPv4))
        {
            pcpp::IPv4Layer *ipLayer = packet.getLayerOfType<pcpp::IPv4Layer>();
            if (ipLayer != nullptr)
            {
                std::string ipaddr = ipLayer->getSrcIPAddress().toString();
                if (ipaddr == "127.0.0.1")
                    return;

                uint8_t *payload = ipLayer->getLayerPayload();
                size_t payloadLength = ipLayer->getLayerPayloadSize();
                time_t timestamp = packet.getRawPacket()->getPacketTimeStamp().tv_sec;

                uint8_t searchBytes[] = {0x52, 0x54, 0x50, 0x53};
                int M = sizeof(searchBytes) / sizeof(searchBytes[0]);
                int N = payloadLength;
                int index = KMPSearch(searchBytes, M, payload, N);

                if (index == 8)
                {
                    if (timestamp % 2 == 0)
                    {
                        packetQueue1.push(packet);
                        // std::cout << "packetQueue1 size: " << packetQueue1.size() << std::endl;
                    }
                    else
                    {
                        packetQueue2.push(packet);
                        // std::cout << "packetQueue2 size: " << packetQueue2.size() << std::endl;
                    }
                }
                if (index == 36)
                {
                    specialPacketQueue.push(packet);
                    std::cout << "specialPacketQueue size: " << specialPacketQueue.size() << std::endl;
                }

                return;
            }
        }
    }

    void specialcallback(pcpp::Packet &packet)
    {
        if (packet.isPacketOfType(pcpp::IPv4))
        {
            pcpp::IPv4Layer *ipLayer = packet.getLayerOfType<pcpp::IPv4Layer>();
            if (ipLayer != nullptr)
            {
                if (ipLayer->getSrcIPAddress().toString() == g_ipaddr)
                {
                    std::string ipaddr = g_ipaddr;
                    time_t timestamp = packet.getRawPacket()->getPacketTimeStamp().tv_sec;
                    int32_t rtps_content = packet.getRawPacket()->getRawDataLen();
                    nlohmann::json json_obj;
                    json_obj["timestamp"] = timestamp;
                    json_obj["dev_partition"] = g_partition;
                    json_obj["total_traffic"] = rtps_content - byte_fix;
                    jsonQueue.push(json_obj);
                }
            }
        }
    }
};

struct CallbackData
{
    PacketStats *stats;
    std::string ipaddr;
};

static void onPacketArrives(pcpp::RawPacket *packet, pcpp::PcapLiveDevice *dev, void *cookie)
{
    // 把傳入的 cookie 做轉型原本的 PacketStats 物件
    PacketStats *stats = (PacketStats *)cookie;

    // 把 RawPacket 變成分析過的 Packet
    pcpp::Packet parsedPacket(packet);

    // 讓 PacketStats 去做統計
    stats->dictcallback(parsedPacket);
}

static void onSpecialPacketArrives(pcpp::RawPacket *packet, pcpp::PcapLiveDevice *dev, void *cookie)
{
    // 把傳入的 cookie 做轉型原本的 PacketStats 物件
    PacketStats *stats = (PacketStats *)cookie;

    // 把 RawPacket 變成分析過的 Packet
    pcpp::Packet parsedPacket(packet);

    // 讓 PacketStats 去做統計
    stats->specialcallback(parsedPacket);
}

void write_to_file(int filesize, const std::string &filepath)
{
    int index = 0;
    std::vector<std::filesystem::path> filelst;
    std::string inputfirst = filepath + "traffic_details1.json";
    std::string inputsecond = filepath + "traffic_details2.json";
    std::cout << inputfirst << std::endl;
    std::cout << inputsecond << std::endl;
    std::filesystem::path firstfile = inputfirst;
    std::filesystem::path secondfile = inputsecond;
    filelst.push_back(firstfile);
    filelst.push_back(secondfile);

    for (auto &file : filelst)
    {
        std::ofstream thefile(file);
    }

    while (true)
    {
        nlohmann::json jsonArray;
        while (jsonArray.size() < filesize)
        {
            while (!jsonQueue.empty())
            {
                jsonArray.push_back(jsonQueue.front());
                jsonQueue.pop();
                if (jsonArray.size() > filesize)
                    break;
            }
        }

        if (jsonArray.size() == 0)
            continue;
        std::cout << "---> jsonArray size: " << jsonArray.size() << std::endl;
        std::ofstream file(filelst[index]);
        if (file.is_open())
        {
            // 獲取系統時鐘的當前時間點
            auto now = std::chrono::system_clock::now();

            // 轉換為 std::time_t 類型，便於轉換為本地時間
            std::time_t currentTime = std::chrono::system_clock::to_time_t(now);

            // 將 std::time_t 轉換為字符串形式
            std::cout << "the current time is : " << std::ctime(&currentTime);

            file << jsonArray.dump(4);
            file.close();
        }
        else
        {
            std::cerr << "failed to open file: " << filelst[index] << std::endl;
        }
        index = (index + 1) % 2;
        std::ofstream newfile(filelst[index]);
    }
}

void printUsage()
{
    std::cout << "Usage: [options]\n"
              << "Options:\n"
              << "  -i, --interface      Specify the network interface name. Default: any.\n"
              << "  -p, --packetcount    Set how many packets to process at a time. Default: 15000.\n"
              << "  -j, --jsonpath       Set the relative path for saving JSON file. Default: current directory.\n"
              << "  -a, --ipaddr         Set the IP address of the device. Default: None.\n"
              << "  -b, --partition      Set the partition of the . Default: None.\n"
              << "  -h, --help           Display this help message.\n";
}

void capturePackets(std::string interfaceName)
{
}

int main(int argc, char *argv[])
{
    std::string interfaceName = "any";
    int packetCount = 15000;
    std::string filepath = "";
    std::string ipaddr = "";
    std::string partittion = "";
    std::string connection_url = "postgresql://postgres:postgres@localhost:5432/postgres";

    for (int i = 1; i < argc; i++)
    {
        std::string arg = argv[i];
        if ((arg == "--interface" || arg == "-i") && i + 1 < argc)
        {
            interfaceName = argv[++i];
        }
        else if ((arg == "--packetcount" || arg == "-p") && i + 1 < argc)
        {
            packetCount = std::atoi(argv[++i]);
        }
        else if ((arg == "--jsonpath" || arg == "-j") && i + 1 < argc)
        {
            filepath = argv[++i];
        }
        else if ((arg == "--ipaddr" || arg == "-a") && i + 1 < argc)
        {
            ipaddr = argv[++i];
        }
        else if ((arg == "--partition" || arg == "-b") && i + 1 < argc)
        {
            partittion = argv[++i];
        }
        else if ((arg == "--connection_url" || arg == "-c") && i + 1 < argc)
        {
            connection_url = argv[++i];
        }
        else if (arg == "-h" || arg == "--help")
        {
            printUsage();
            return 0;
        }
        else
        {
            std::cerr << "Unknown argument: " << arg << std::endl;
            return 1;
        }
    }

    if (filepath != "")
        filepath = filepath + "/";

    auto write_to_file_func = std::bind(&write_to_file, packetCount, filepath);
    std::thread write_to_file_thread(write_to_file_func);
    write_to_file_thread.detach();

    auto packet_queue_parser_func1 = std::bind(&rtpscallback, 8, std::ref(packetQueue1));
    std::thread packet_queue_parser_thread1(packet_queue_parser_func1);
    packet_queue_parser_thread1.detach();
    auto packet_queue_parser_func2 = std::bind(&rtpscallback, 8, std::ref(packetQueue2));
    std::thread packet_queue_parser_thread2(packet_queue_parser_func2);
    packet_queue_parser_thread2.detach();
    auto packet_queue_parser_func3 = std::bind(&rtpscallback, 36, std::ref(specialPacketQueue));
    std::thread packet_queue_parser_thread3(packet_queue_parser_func3);
    packet_queue_parser_thread3.detach();

    auto guidmanager_func = std::bind(&guidmanager, connection_url);
    std::thread guidmanager_thread(guidmanager_func);
    // guidmanager_thread.detach();
    

    pcpp::PcapLiveDevice *dev = pcpp::PcapLiveDeviceList::getInstance().getPcapLiveDeviceByIpOrName(interfaceName);
    if (interfaceName == "any")
        byte_fix = 2;
    else
        byte_fix = 0;

    if (!dev->open())
    {
        throw(std::runtime_error("cannot open device, try with sudo?"));
    }

    PacketStats stats;

    std::cout << std::endl
              << dev->getName() << " ---> Starting async capture..." << std::endl;

    if ((ipaddr.size() == 0) && (partittion.size() == 0))
    {
        dev->startCapture(onPacketArrives, &stats);
    }
    else if ((ipaddr.size() == 0) || (partittion.size() == 0))
    {
        std::cout << "Please specify both ipaddr and partition" << std::endl;
        return 0;
    }
    else if ((ipaddr.size() > 0) && (partittion.size() > 0))
    {
        g_ipaddr = ipaddr;
        g_partition = partittion;
        std::cout << "start capture, ip = " << ipaddr << ",partition = " << partittion << std::endl;
        dev->startCapture(onSpecialPacketArrives, &stats);
    }

    while (1)
    {
        std::this_thread::sleep_for(std::chrono::seconds(20));
    }

    dev->stopCapture();
    return 0;
}