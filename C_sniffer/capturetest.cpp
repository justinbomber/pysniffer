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
#include <pcapplusplus/IPv4Layer.h>
#include <pcapplusplus/Packet.h>
#include <map>
#include <string>
#include <nlohmann/json.hpp>
#include <queue>
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <mutex>
#include <pqxx/pqxx>
#include <filesystem>

int curretqueue = 0;
std::string g_ipaddr = "";
std::string g_partition = "";
std::queue<nlohmann::json> jsonQueue;
std::map<std::string, std::string> guidmap;
std::vector<std::queue<pcpp::Packet>> packetQueuesvec;
std::queue<pcpp::Packet> packetQueue1;
std::queue<pcpp::Packet> packetQueue2;
std::queue<pcpp::Packet> packetQueue3;
std::queue<pcpp::Packet> guidmappingQueue;
std::mutex packet_mtx;
std::mutex json_mtx;
pqxx::connection *connection = nullptr;
int queueCount = 0;
short byte_fix = 0;


void packetPush(pcpp::Packet packet, std::queue<pcpp::Packet> &packetQueue)
{
    std::lock_guard<std::mutex> lock(packet_mtx);
    packetQueue.push(packet);
}

pcpp::Packet packetPop(std::queue<pcpp::Packet> &packetQueue)
{
    std::lock_guard<std::mutex> lock(packet_mtx);
    pcpp::Packet packet = packetQueue.front();
    packetQueue.pop();
    return packet;
}

void jsonPush(nlohmann::json json, std::queue<nlohmann::json> &jsonQueue)
{
    std::lock_guard<std::mutex> lock(json_mtx);
    jsonQueue.push(json);
}

nlohmann::json jsonPop(std::queue<nlohmann::json> &jsonQueue)
{
    std::lock_guard<std::mutex> lock(json_mtx);
    nlohmann::json json = jsonQueue.front();
    jsonQueue.pop();
    return json;
}

bool comparertpsPayload(uint8_t *payload) {
    uint8_t target[] = {0x52, 0x54, 0x50, 0x53};

    // 檢查 payload 是否與 target 相同
    for (int i = 0; i < 4; ++i) {
        if (payload[i + 8] != target[i]) {
            return false;
        }
    }
    return true;
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
    // std::string rtps_writer_entitykey;

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
        if (character < 32 || character > 126) {
            return "None";  // 包含非可打印字符，返回 nullptr
        }
        asciiString += character;
    }
    return asciiString;
}

int totalsize_cal(uint16_t rawsize)
{
    int packet_count = ceil(rawsize / 1480);
    int newrawsize = packet_count * 34 + rawsize - byte_fix * packet_count;
    return newrawsize;
}

RTPS_DATA_STRUCTURE convertrtpspacket(pcpp::Packet &packet, int index, int virguid_idx )
{
    RTPS_DATA_STRUCTURE rtps_data;
    const uint8_t *payload = packet.getLayerOfType<pcpp::IPv4Layer>()->getLayerPayload();
    int udpidx = index - 8;
    int rtpsidx = index;
    if (virguid_idx != -1)
    {
        rtpsidx = virguid_idx;
        // rtps guid
        rtps_data.rtps_hostid = byteArrayToHexString(payload + rtpsidx + 4, 4);
        rtps_data.rtps_appid = byteArrayToHexString(payload + rtpsidx + 8, 4);
        rtps_data.rtps_instanceid = byteArrayToHexString(payload + rtpsidx + 12, 4);
        // rtps_data.rtps_writer_entitykey = byteArrayToHexString(payload + index + 44, 4);
    } else {
        rtpsidx = index;
        // rtps guid
        rtps_data.rtps_hostid = byteArrayToHexString(payload + rtpsidx + 8, 4);
        rtps_data.rtps_appid = byteArrayToHexString(payload + rtpsidx + 12, 4);
        rtps_data.rtps_instanceid = byteArrayToHexString(payload + rtpsidx + 16, 4);
        // rtps_data.rtps_writer_entitykey = byteArrayToHexString(payload + index + 44, 4);
    }

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


    rtps_data.timestamp = packet.getRawPacket()->getPacketTimeStamp().tv_sec;

    return rtps_data;
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
        // if (i - j > 50)
        // return -1;
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

static void start_subcap(std::string ipaddr);

void guidmanagercallbak(std::queue<pcpp::Packet> &packetQueue)
{
    while (true)
    {
        if (!packetQueue.empty())
        {
            pcpp::Packet packet = packetPop(packetQueue);

            uint8_t *payload = packet.getLayerOfType<pcpp::IPv4Layer>()->getLayerPayload();
            size_t payloadLength = packet.getLayerOfType<pcpp::IPv4Layer>()->getLayerPayloadSize();
            if (comparertpsPayload(payload))
            {
                uint8_t partitionpre[] = {0x00, 0x00, 0x07, 0x00, 0x00, 0x00};
                uint8_t partitionfter[] = {0x00, 0x00, 0x01, 0x00, 0x00, 0x00};
                int N = payloadLength;
                int preidx = 6;
                int fidx = 6;
                int prepartition_index = KMPSearch(partitionpre, preidx, payload, N);
                int fpartition_index = KMPSearch(partitionfter, fidx, payload, N);
                std::string guid = byteArrayToHexString(payload + 8, 12);
                if (guidmap.find(guid) == guidmap.end())
                {
                    if (prepartition_index != -1 && fpartition_index != -1 && fpartition_index - prepartition_index == 12)
                    {
                        std::string partition = byteArrayToAsciiString(payload + prepartition_index + 6, 6);
                        if (partition != "None")
                        {
                            std::cout << "start capture:" << partition <<std::endl;
                            guidmap.emplace(guid, partition);
                        }
                    }
                }
            }
        }
        else
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }
}

void rtpscallback(std::queue<pcpp::Packet> &packetQueue)
{
    while (true)
    {
        if (!packetQueue.empty())
        {
            pcpp::Packet packet = packetPop(packetQueue);

            uint8_t *payload = packet.getLayerOfType<pcpp::IPv4Layer>()->getLayerPayload();
            size_t payloadLength = packet.getLayerOfType<pcpp::IPv4Layer>()->getLayerPayloadSize();
            if (comparertpsPayload(payload))
            {
                std::string guid = byteArrayToHexString(payload + 8, 12);
                RTPS_DATA_STRUCTURE rtps_data = convertrtpspacket(packet, 8, -1);
                if (guidmap.find(guid) != guidmap.end())
                {
                    // get time
                    time_t timestamp = rtps_data.timestamp;

                    // get traffic
                    int32_t rtps_content = totalsize_cal(rtps_data.udp_length);

                    nlohmann::json json_obj;
                    json_obj["timestamp"] = timestamp;
                    json_obj["dev_partition"] = guidmap[guid];
                    json_obj["total_traffic"] = rtps_content;
                    // std::cout << "save json_obj " << json_obj << std::endl;
                    jsonPush(json_obj, jsonQueue);
                }
            }
        }
        else
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }
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
                // std::cout << "currtqueue " << curretqueue << std::endl;
                // packetQueuesvec[curretqueue].push(packet);
                // curretqueue = (curretqueue + 1) % packetQueuesvec.size();
                switch (queueCount)
                {
                    case 0:
                    {
                        packetPush(packet, packetQueue1);
                        queueCount = 1;
                        break;
                    }
                    case 1:
                    {
                        packetPush(packet, packetQueue2);
                        queueCount = 2;
                        break;
                    }
                    case 2:
                    {
                        packetPush(packet, packetQueue3);
                        queueCount = 0;
                        break;
                    }
                    default:
                    {
                        packetPush(packet, packetQueue3);
                        queueCount = 0;
                        break;
                    }
                }
                packetPush(packet, guidmappingQueue);
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
                    jsonPush(json_obj, jsonQueue);
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
        auto startTime = std::chrono::steady_clock::now();
        nlohmann::json jsonArray;
        while (jsonArray.size() < filesize)
        {
            while (!jsonQueue.empty())
            {
                nlohmann::json json_obj = jsonPop(jsonQueue);
                if (json_obj.is_object()) 
                {
                    jsonArray.push_back(json_obj);
                }
                if (jsonArray.size() > filesize)
                    break;
            }
            auto currentTime = std::chrono::steady_clock::now();
            auto elapsedTime = std::chrono::duration_cast<std::chrono::seconds>(currentTime - startTime).count();

            if (elapsedTime >= 10)
            {
                break;
            }
        }

        if (jsonArray.size() == 0)
            continue;
        // std::cout << "---> jsonArray size: " << jsonArray.size() << std::endl;
        std::ofstream file(filelst[index]);
        if (file.is_open())
        {
            auto now = std::chrono::system_clock::now();

            std::time_t currentTime = std::chrono::system_clock::to_time_t(now);

            std::cout << " save to json file at : " << std::ctime(&currentTime);

            try {
                file << jsonArray.dump(4);
            } catch (const std::exception& e) {
                std::cerr << "An exception occurred: " << e.what() << '\n';
            }
            file.close();
        }
        else
        {
            std::cerr << "failed to open file: " << filelst[index] << std::endl;
        }
        index = (index + 1) % 2;
        file.open(filelst[index]);
        file.close();
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
              << "  -h, --help           Display this help message.\n"
              << "  -c, --connectionurl  Set the PostgreSQL connection URL. Default: postgresql://postgres:njTqJ2cavzJi0PfugfpY1jf61yp5jmoqIB1fFyIGw8w=@paasdb.default:5433/postgres.\n"
              << "  -m, --testmode      Test mode. Default: False.\n"
              << "  -t, --threadcount   Set the number of threads. Default: 3.\n";
}

std::string replacePasswordInURL(const std::string& url, const std::string& oldPassword, const std::string& newPassword) {
    size_t startPos = url.find(oldPassword);
    if (startPos == std::string::npos) {
        return url; // 如果找不到舊密碼，返回原始URL
    }
    return url.substr(0, startPos) + newPassword + url.substr(startPos + oldPassword.length());
}

int main(int argc, char *argv[])
{
    std::string interfaceName = "any";
    int packetCount = 15000;
    std::string filepath = "";
    std::string ipaddr = "";
    std::string partittion = "";
    std::string connection_url = "postgresql://postgres:njTqJ2cavzJi0PfugfpY1jf61yp5jmoqIB1fFyIGw8w=@paasdb.default:5433/postgres";
    // std::string connection_url = "postgresql://postgres:admin@140.110.7.17:5433/postgres";
    bool test_mode = false;
    int thread_count = 3;
    std::string oldPassword = "njTqJ2cavzJi0PfugfpY1jf61yp5jmoqIB1fFyIGw8w=";
    std::string newPassword = "admin";

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
        else if ((arg == "--connectionurl" || arg == "-c") && i + 1 < argc)
        {
            connection_url = argv[++i];
        }
        else if ((arg == "--threadcount" || arg == "-t") && i + 1 < argc)
        {
            thread_count = std::atoi(argv[++i]);
        }
        else if ((arg == "--testmode" || arg == "-m") && i + 1 < argc)
        {
            test_mode = std::atoi(argv[++i]);
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
    
    std::string newURL = replacePasswordInURL(connection_url, oldPassword, newPassword);
    connection_url = newURL;

    auto write_to_file_func = std::bind(&write_to_file, packetCount, filepath);
    std::thread write_to_file_thread(write_to_file_func);
    write_to_file_thread.detach();

    auto packet_queue_parser_func1 = std::bind(&rtpscallback, std::ref(packetQueue1));
    std::thread packet_queue_parser_thread1(packet_queue_parser_func1);
    packet_queue_parser_thread1.detach();
    auto packet_queue_parser_func2 = std::bind(&rtpscallback, std::ref(packetQueue2));
    std::thread packet_queue_parser_thread2(packet_queue_parser_func2);
    packet_queue_parser_thread2.detach();
    auto packet_queue_parser_func3 = std::bind(&rtpscallback, std::ref(packetQueue3));
    std::thread packet_queue_parser_thread3(packet_queue_parser_func3);
    packet_queue_parser_thread3.detach();

    auto guidmanager_func = std::bind(&guidmanagercallbak, std::ref(guidmappingQueue));
    std::thread guidmanager_thread(guidmanager_func);
    guidmanager_thread.detach();


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