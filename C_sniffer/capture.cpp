#include <iostream>
#include <fstream>
#include <chrono>
#include <sys/stat.h>
#include <vector>
#include <thread>
#include <pcapplusplus/PcapLiveDevice.h>
#include <pcapplusplus/PcapLiveDeviceList.h>
#include <pcapplusplus/SystemUtils.h>
#include <pcapplusplus/IPv4Layer.h>
#include <map>
#include <string>
#include <nlohmann/json.hpp>
#include <queue>

int64_t packetCount = 0;
time_t lasttimestamp = 0;
std::map<std::string, std::string> partitionmap;
std::queue<nlohmann::json> jsonQueue;

std::string hex_to_ascii(const std::string& hex_string) {
    std::string start_pattern = "07:00:00:00";
    std::string end_pattern = "00:00:01";

    size_t start_index = hex_string.find(start_pattern) + start_pattern.length() + 1; // +1 to skip the colon after start_pattern
    size_t end_index = hex_string.find(end_pattern, start_index);

    if (start_index == std::string::npos || end_index == std::string::npos) {
        return "no_partition";
    }

    std::string hex_substring = hex_string.substr(start_index, end_index - start_index);
    hex_substring.erase(std::remove(hex_substring.begin(), hex_substring.end(), ':'), hex_substring.end());

    std::string ascii_string;
    for (size_t i = 0; i < hex_substring.length(); i += 2) {
        std::string hex_byte = hex_substring.substr(i, 2);
        char ascii_char = static_cast<char>(std::stoi(hex_byte, nullptr, 16));
        ascii_string += ascii_char;
    }

    return ascii_string;
}

static void start_subcap(std::string ipaddr);
struct PacketStats
{
    void dictcallback(pcpp::Packet &packet)
    {
        if (packet.isPacketOfType(pcpp::IPv4))
        {
            pcpp::IPv4Layer *ipLayer = packet.getLayerOfType<pcpp::IPv4Layer>();
            if (ipLayer != nullptr)
            {
                uint8_t *payload = ipLayer->getLayerPayload();
                size_t payloadLength = ipLayer->getLayerPayloadSize();
                std::string payloadStr(reinterpret_cast<char *>(payload), payloadLength);

                if (payloadStr.find("RTPS") != std::string::npos)
                {
                    //TODO : getpartition
                    std::string ipaddr = ipLayer->getSrcIPAddress().toString();
                    std::string partition = hex_to_ascii(payloadStr);
                    if (partition == "no_partition")
                        return;
                    else if (partitionmap.find(partition) == partitionmap.end())
                    {
                        PacketStats stats;
                        partitionmap[ipaddr] = partition;
                        auto rtps_func = std::bind(&start_subcap, ipaddr);
                        std::thread rtpscapturethread(rtps_func);
                        rtpscapturethread.detach();
                        std::cout << "start capture, ip = " << ipaddr << ",partition = " <<  partition << std::endl;
                    }
                }
            }
        }

    }

    void rtpscallback(pcpp::Packet &packet, std::string ipaddr)
    {
        if (packet.isPacketOfType(pcpp::IPv4))
        {
            // 检查封包是否包含 IPv4 层
            pcpp::IPv4Layer *ipLayer = packet.getLayerOfType<pcpp::IPv4Layer>();
            if (ipLayer != nullptr)
            {
                if (ipLayer->getSrcIPAddress().toString().compare(ipaddr) == 0)
                {
                    pcpp::iphdr *ipv4Header = ipLayer->getIPv4Header();
                    uint8_t *payload = ipLayer->getLayerPayload();
                    size_t payloadLength = ipLayer->getLayerPayloadSize();
                    std::string payloadStr(reinterpret_cast<char *>(payload), payloadLength);
                    std::string ipaddr = ipLayer->getSrcIPAddress().toString();

                    if (payloadStr.find("RTPS") != std::string::npos)
                    {
                        if (partitionmap.find(ipaddr) != partitionmap.end())
                        {
                            nlohmann::json json_obj;
                            // TODO : add json
                            uint32_t timestamp;
                            int32_t rtps_content;
                            int total_traffic = timestamp + rtps_content + 36;
                            json_obj["timestamp"] = timestamp;
                            json_obj["rtps_content"] = rtps_content;
                            json_obj["total_traffic"] = total_traffic;
                            jsonQueue.push(json_obj);
                        }
                        else
                            return;
                        // TODO : packet calculator func
                    }
                }
            }
        }
    }
};

struct CallbackData {
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

static void onRTPSArrives(pcpp::RawPacket *packet, pcpp::PcapLiveDevice *dev, void *cookie)
{
    // 轉換 cookie 為 CallbackData 結構
    CallbackData* data = static_cast<CallbackData*>(cookie);

    // 把 RawPacket 變成分析過的 Packet
    pcpp::Packet parsedPacket(packet);

    // 讓 PacketStats 去做統計
    data->stats->rtpscallback(parsedPacket, data->ipaddr);
}

static void start_subcap(std::string ipaddr)
{
    pcpp::PcapLiveDevice *dev = pcpp::PcapLiveDeviceList::getInstance().getPcapLiveDeviceByIpOrName("eno1");

    if (!dev->open())
    {
        throw(std::runtime_error("cannot open device, try with sudo?"));
    }

    PacketStats stats;

    std::cout << std::endl
              << "Starting async capture..." << std::endl;

    CallbackData data;
    data.stats = &stats;
    data.ipaddr = ipaddr;
    dev->startCapture(onRTPSArrives, &data);

    while (1)
    {
        // pcpp::multiPlatformSleep(1);
    }

    dev->stopCapture();
}

void remove_last_comma(const std::string& filename) {
    std::fstream json_file;
    json_file.open(filename, std::ios::in | std::ios::out);

    if (json_file.is_open()) {
        // 移動到文件末尾前兩個字符
        json_file.seekg(-2, std::ios_base::end);
        char last_char = json_file.peek();
        if (last_char == ',') {
            // 移動回同一位置並截斷文件
            json_file.seekp(-2, std::ios_base::end);
            json_file.put(' '); // 替換逗號
            json_file.put('\n'); // 保持格式
        }
        json_file.close();
    }
}

void write_to_file(int filesize, const std::string& filepath) {
    std::vector<std::string> filelst = {filepath + "traffic_details1.json", filepath + "traffic_details2.json"};
    int index = 0;

    // 初始化文件
    for (const auto& file : filelst) {
        std::ofstream json_file(file, std::ofstream::out);
        json_file << "[\n";
    }

    while (true) {
        struct stat stat_buf;
        int rc = stat(filelst[index].c_str(), &stat_buf);
        if (rc == 0 && stat_buf.st_size <= filesize) {
            std::ofstream json_file(filelst[index], std::ofstream::app);
            while (!jsonQueue.empty()) {
                nlohmann::json packet = jsonQueue.front();
                jsonQueue.pop();
                json_file << packet.dump(4) << ",\n";
            }
        } else {
            // 移除最後一個逗號並關閉文件
            // 注意：這裡需要實現移除逗號的邏輯
            remove_last_comma(filelst[index]); // 這是假定的函數，需要您自己實現

            std::ofstream json_file(filelst[index], std::ofstream::app);
            json_file.seekp(-2, std::ios_base::end); // 回退文件指針
            json_file << "\n]";
            index = (index + 1) % 2;
            std::ofstream next_file(filelst[index], std::ofstream::out);
            next_file << "[\n";
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

int main(int argc, char *argv[])
{
    // 可以用 ifconfig 找一下網卡的名字，例如 lo, eth0
    pcpp::PcapLiveDevice *dev = pcpp::PcapLiveDeviceList::getInstance().getPcapLiveDeviceByIpOrName("eno1");

    if (!dev->open())
    {
        throw(std::runtime_error("cannot open device, try with sudo?"));
    }

    auto write_to_file_func = std::bind(&write_to_file, 10000000, "/");
    std::thread write_to_file_thread(write_to_file_func);
    write_to_file_thread.detach();

    PacketStats stats;

    std::cout << std::endl
              << "Starting async capture..." << std::endl;

    dev->startCapture(onPacketArrives, &stats);

    while (1)
    {
        // pcpp::multiPlatformSleep(1);
    }

    dev->stopCapture();
    return 0;
}