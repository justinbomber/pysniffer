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
std::mutex mtx;
pqxx::connection *connection = nullptr;
int queueCount = 0;
short byte_fix = 0;

bool opendatabase(const std::string &url)
{
    bool result = false;

    try
    {
        connection = new pqxx::connection(url);
        if (connection->is_open())
        {
            result = true;
            // std::cout << "Opened database successfully: " << connection->dbname() << std::endl;
        }
        else
        {
            result = false;
            std::cout << "Can't open database" << std::endl;
        }
    }
    catch (const std::exception &e)
    {
        result = false;
        std::cerr << "Exception occurred: " << e.what() << std::endl;
    }

    return result;
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

bool closedatabase()
{
    bool result = false;

    try
    {
        if (connection->is_open())
            connection->disconnect();

        result = true;
    }
    catch (const std::exception &e)
    {
        result = false;
        std::cerr << e.what() << '\n';
    }

    return result;
}

pqxx::result execute_query(const std::string &query)
{
    if (connection == nullptr || !connection->is_open())
    {
        throw std::runtime_error("Database connection is not open");
    }

    pqxx::work txn(*connection);
    pqxx::result res = txn.exec(query);
    txn.commit();
    return res;
}

void guidmanager(const std::string connection_url, bool test_mode)
{
    while (true)
    {
        //     std::string sqlcmd = "SELECT distinct v_guid, dev_partition FROM vw_dds_devices_for_flow_cal "
        //                          "WHERE v_guid IS NOT NULL "
        //                          "and (dev_eff_date is NOT NULL and dev_eff_date <= now()) "
        //                          "and (dev_end_date is NULL or dev_end_date <= now()) "
        //                          "and (svc_eff_date is NOT NULL and svc_eff_date <= now()) "
        //                          "and (svc_end_date is NULL or svc_end_date <= now()) ";

        std::string sqlcmd = "SELECT distinct org_v_guid, dev_partition FROM vw_dds_devices_for_flow_cal "
                             "WHERE org_v_guid IS NOT NULL "
                             "AND (dev_eff_date is NOT NULL and dev_eff_date <= now()) "
                             "AND (dev_end_date is NULL or dev_end_date <= now()) "
                             "AND (svc_eff_date is NOT NULL and svc_eff_date <= now()) "
                             "AND (svc_end_date is NULL or svc_end_date <= now())";
        if (test_mode)
        {
            sqlcmd = "SELECT distinct org_v_guid, dev_partition FROM vw_dds_devices_for_flow_cal "
                     "WHERE org_v_guid IS NOT NULL "
                     "AND dev_partition IS NOT NULL";
        }
        if (opendatabase(connection_url))
        {
            pqxx::result guidtableresult = execute_query(sqlcmd);
            if (guidtableresult.empty())
            {
                sqlcmd = "SELECT distinct org_v_guid, dev_partition FROM vw_dds_devices_for_flow_cal "
                        "WHERE org_v_guid IS NOT NULL "
                        "AND dev_partition IS NOT NULL";
                guidtableresult = execute_query(sqlcmd);
            }
            for (auto const &row : guidtableresult)
            {
                std::string dev_partition = row[1].as<std::string>();

                std::string v_guid = row[0].as<std::string>();
                v_guid = v_guid.substr(0, v_guid.size() - 8);

                guidmap.emplace(v_guid, dev_partition);
                std::lock_guard<std::mutex> guard(mtx);
                for (auto partitioniter = guidmap.begin(); partitioniter != guidmap.end();)
                {
                    if (partitioniter->second == dev_partition && partitioniter->first != v_guid)
                        partitioniter = guidmap.erase(partitioniter);
                    else
                        ++partitioniter;
                }
                // std::cout << "guid: " << v_guid << " ---> dev_partition: " << dev_partition << std::endl;
            }

            // if(closedatabase())
            // std::cout << "Close database successfully." << std::endl;
            // else
            // std::cout << "Can't close database." << std::endl;
        }
        else
        {
            // std::cout << "Can't open database." << std::endl;
        }
        bool colseit = closedatabase();

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        continue;
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

void rtpscallback(std::queue<pcpp::Packet> &packetQueue)
{
    while (true)
    {
        if (!packetQueue.empty())
        {
            pcpp::Packet packet = packetQueue.front();
            packetQueue.pop();
            // std::string ipaddr = packet.getLayerOfType<pcpp::IPv4Layer>()->getSrcIPAddress().toString();
            // if (ipaddr == "127.0.0.1")
            //     return;
            // uint8_t *payload = packet.getLayerOfType<pcpp::IPv4Layer>()->getLayerPayload();
            // size_t payloadLength = packet.getLayerOfType<pcpp::IPv4Layer>()->getLayerPayloadSize();
            // uint8_t PID_ORIGINAL_bytes[] = {0x61, 0x00, 0x18, 0x00};
            // int M = sizeof(PID_ORIGINAL_bytes) / sizeof(PID_ORIGINAL_bytes[0]);
            // int N = payloadLength;
            // int rtpsguid_idx = KMPSearch(PID_ORIGINAL_bytes, M, payload, N);
            // if (rtpsguid_idx != -1){
            //     std::cout << "org guid --->" << byteArrayToHexString(payload + rtpsguid_idx +4, 12) << std::endl;
            //     continue;
            // }

            uint8_t *payload = packet.getLayerOfType<pcpp::IPv4Layer>()->getLayerPayload();
            size_t payloadLength = packet.getLayerOfType<pcpp::IPv4Layer>()->getLayerPayloadSize();
            uint8_t PID_ORIGINAL_bytes[] = {0x61, 0x00, 0x18, 0x00};
            int M = sizeof(PID_ORIGINAL_bytes) / sizeof(PID_ORIGINAL_bytes[0]);
            int N = payloadLength;
            int rtpsguid_idx = KMPSearch(PID_ORIGINAL_bytes, M, payload, N);
            if (rtpsguid_idx != -1 && comparertpsPayload(payload))
            {
                std::string guid = byteArrayToHexString(payload + rtpsguid_idx + 4, 12);
                // std::cout <<  ": src --->" << ipLayer->getSrcIPAddress().toString()
                //         << ", dst --->" << ipLayer->getDstIPAddress().toString()
                //         << ", guid --->" << guid << std::endl;
                
                RTPS_DATA_STRUCTURE rtps_data = convertrtpspacket(packet, 8, rtpsguid_idx);
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
                    std::cout << "save json_obj " << json_obj << std::endl;
                    jsonQueue.push(json_obj);
                }
            }

            // uint8_t searchBytes[] = {0x52, 0x54, 0x50, 0x53};
            // // int M = sizeof(searchBytes) / sizeof(searchBytes[0]);
            // // int N = payloadLength;
            // int index = KMPSearch(searchBytes, M, payload, N);
            // // std::cout << "index " << index << std::endl;

            // if (index == 8 || index == 36)
            // {
            //     // std::cout << "find rtps bytes" << std::endl;
            //     uint8_t PID_ORIGINAL_bytes[] = {0x61, 0x00, 0x18, 0x00};
            //     int M = sizeof(PID_ORIGINAL_bytes) / sizeof(PID_ORIGINAL_bytes[0]);
            //     int N = payloadLength;
            //     int rtpsguid_idx = KMPSearch(PID_ORIGINAL_bytes, M, payload, N);
            //     if (rtpsguid_idx != -1)
            //         std::cout << "find rtpsguid_idx " << rtpsguid_idx << std::endl;

            //     RTPS_DATA_STRUCTURE rtps_data = convertrtpspacket(packet, index, rtpsguid_idx);
            //     std::string guid = rtps_data.rtps_hostid +
            //                        rtps_data.rtps_appid +
            //                        rtps_data.rtps_instanceid;
            //     if (rtpsguid_idx != -1){
            //         std::cout << "org guid --->" << guid << std::endl;
            //     }
            //     //    rtps_data.rtps_writer_entitykey;
            //     // std::cout <<  " packet length " << rtps_data.udp_length << std::endl;

            //     // if guid in guidmap
            //     if (guidmap.find(guid) != guidmap.end())
            //     {
            //         // get time
            //         time_t timestamp = rtps_data.timestamp;

            //         // get traffic
            //         int32_t rtps_content = totalsize_cal(rtps_data.udp_length);

            //         nlohmann::json json_obj;
            //         json_obj["timestamp"] = timestamp;
            //         json_obj["dev_partition"] = guidmap[guid];
            //         json_obj["total_traffic"] = rtps_content;
            //         std::cout << "save json_obj " << json_obj << std::endl;
            //         jsonQueue.push(json_obj);
            //     }
            // }
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
                        packetQueue1.push(packet);
                        queueCount = 1;
                        break;
                    }
                    case 1:
                    {
                        packetQueue2.push(packet);
                        queueCount = 2;
                        break;
                    }
                    case 2:
                    {
                        packetQueue3.push(packet);
                        queueCount = 0;
                        break;
                    }
                    default:
                    {
                        packetQueue3.push(packet);
                        queueCount = 0;
                        break;
                    }
                }
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
        auto startTime = std::chrono::steady_clock::now();
        nlohmann::json jsonArray;
        while (jsonArray.size() < filesize)
        {
            while (!jsonQueue.empty())
            {
                nlohmann::json json_obj = jsonQueue.front();
                jsonQueue.pop();
                if (json_obj.is_object()) 
                {
                    jsonArray.push_back(json_obj);
                }
                if (jsonArray.size() > filesize)
                    break;
            }
            auto currentTime = std::chrono::steady_clock::now();
            auto elapsedTime = std::chrono::duration_cast<std::chrono::seconds>(currentTime - startTime).count();

            if (elapsedTime >= 30)
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

    auto guidmanager_func = std::bind(&guidmanager, connection_url, test_mode);
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