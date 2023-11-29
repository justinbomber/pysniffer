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
#include <filesystem>

int64_t packetCount = 0;
time_t lasttimestamp = 0;
std::queue<nlohmann::json> jsonQueue;
std::map<std::string, std::string> partitionmap;

std::string extractData(uint8_t *payload, size_t length)
{
    std::vector<uint8_t> startPattern = {0x07, 0x00, 0x00, 0x00};
    std::vector<uint8_t> endPattern = {0x00, 0x00, 0x01, 0x00, 0x00};

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

void rtpscallback(pcpp::Packet &packet, std::string ipaddr)
{
    // get time
    timespec rawtime = packet.getRawPacket()->getPacketTimeStamp();
    time_t timestamp = rawtime.tv_sec;

    // get traffic
    int32_t rtps_content = packet.getRawPacket()->getRawDataLen();

    nlohmann::json json_obj;
    json_obj["timestamp"] = timestamp;
    json_obj["dev_partition"] = partitionmap[ipaddr];
    json_obj["total_traffic"] = rtps_content;
    jsonQueue.push(json_obj);
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
                std::map<std::string, std::string>::iterator it = partitionmap.find(ipaddr);

                if (it != partitionmap.end())
                {
                    rtpscallback(packet, ipaddr);
                    return;
                }
                if (ipaddr == "127.0.0.1")
                    return;
                uint8_t *payload = ipLayer->getLayerPayload();
                size_t payloadLength = ipLayer->getLayerPayloadSize();
                std::string partition = extractData(payload, payloadLength);

                if (partition == "no_partition")
                    return;
                else if (it == partitionmap.end())
                {
                    partitionmap.emplace(ipaddr, partition);
                    std::cout << "start capture, ip = " << ipaddr << ",partition = " << partition << std::endl;
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
              << "  -h, --help           Display this help message.\n";
}

void capturePackets(std::string interfaceName) {


}

int main(int argc, char *argv[])
{
    std::string interfaceName = "any";
    int packetCount = 15000;
    std::string filepath = "";

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

    pcpp::PcapLiveDevice *dev = pcpp::PcapLiveDeviceList::getInstance().getPcapLiveDeviceByIpOrName(interfaceName);

    if (!dev->open())
    {
        throw(std::runtime_error("cannot open device, try with sudo?"));
    }

    PacketStats stats;

    std::cout << std::endl
              << dev->getName() << " ---> Starting async capture..." << std::endl;

    dev->startCapture(onPacketArrives, &stats);

    while (1)
    {
        std::this_thread::sleep_for(std::chrono::seconds(20));
    }

    dev->stopCapture();
    return 0;
}