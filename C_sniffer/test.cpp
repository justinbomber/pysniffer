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
#include <mutex>
#include <pqxx/pqxx>
#include <filesystem>

#include "PcapFileDevice.h"
int count = 0;
pqxx::connection *connection = nullptr;
std::queue<nlohmann::json> jsonQueue;

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

std::string byteArrayToHexString(const uint8_t *byteArray, size_t arraySize)
{
    std::ostringstream stream;
    for (size_t i = 0; i < arraySize; ++i)
    {
        stream << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byteArray[i]);
    }
    return stream.str();
}

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

void onPacketArrives(pcpp::RawPacket *rawPacket, pcpp::PcapLiveDevice* dev, void* cookie) {
    // 將 RawPacket 轉換為解析後的封包
    pcpp::Packet packet(rawPacket);
    pcpp::IPv4Layer *ipLayer = packet.getLayerOfType<pcpp::IPv4Layer>();
    if (ipLayer != nullptr)
    {
        uint8_t *payload = packet.getLayerOfType<pcpp::IPv4Layer>()->getLayerPayload();
        size_t payloadLength = packet.getLayerOfType<pcpp::IPv4Layer>()->getLayerPayloadSize();
        uint8_t PID_ORIGINAL_bytes[] = {0x61, 0x00, 0x18, 0x00};
        int M = sizeof(PID_ORIGINAL_bytes) / sizeof(PID_ORIGINAL_bytes[0]);
        int N = payloadLength;
        int rtpsguid_idx = KMPSearch(PID_ORIGINAL_bytes, M, payload, N);
        nlohmann::json json_obj;
        json_obj["src"] = ipLayer->getSrcIPAddress().toString();
        jsonQueue.push(json_obj);

        if (rtpsguid_idx != -1)
        {
            uint8_t searchBytes[] = {0x52, 0x54, 0x50, 0x53};
            int M_r = sizeof(searchBytes) / sizeof(searchBytes[0]);
            int N_r = payloadLength;
            int index = KMPSearch(searchBytes, M_r, payload, N_r);

            std::cout << "{ rtps index: " << index << " ,org index: " << rtpsguid_idx <<
                    " } " << ": src --->" << ipLayer->getSrcIPAddress().toString()
                    << ", dst --->" << ipLayer->getDstIPAddress().toString()
                    << ", guid --->" << byteArrayToHexString(payload + rtpsguid_idx + 4, 12) << std::endl;
            count++;
        }
    }
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
                jsonArray.push_back(jsonQueue.front());
                jsonQueue.pop();
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
        std::ofstream newfile(filelst[index]);
    }
}

int main()
{
    /*
    // .pcap 文件的路徑
    std::string pcapFilePath = "enspcap_n.pcap";

    // 打開 .pcap 文件
    pcpp::IFileReaderDevice *reader = pcpp::IFileReaderDevice::getReader(pcapFilePath);

    if (!reader->open())
    {
        std::cerr << "Error opening the pcap file." << std::endl;
        return 1;
    }


    std::string sqlcmd = "SELECT distinct org_v_guid, dev_partition FROM vw_dds_devices_for_flow_cal "
                 "WHERE org_v_guid IS NOT NULL "
                 "AND dev_partition IS NOT NULL";

    if (opendatabase("postgresql://postgres:admin@140.110.7.17:5433/postgres"))
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

            std::cout << "guid: " << v_guid << " ---> dev_partition: " << dev_partition << std::endl;
        }

        // if(closedatabase())
        // std::cout << "Close database successfully." << std::endl;
        // else
        // std::cout << "Can't close database." << std::endl;
    }
    pcpp::RawPacket rawPacket;

    // 讀取和處理封包
    while (reader->getNextPacket(rawPacket))
    {
        // 將 RawPacket 轉換為解析後的封包
        pcpp::Packet packet(&rawPacket);
        pcpp::IPv4Layer *ipLayer = packet.getLayerOfType<pcpp::IPv4Layer>();
        if (ipLayer != nullptr)
        {
            uint8_t *payload = packet.getLayerOfType<pcpp::IPv4Layer>()->getLayerPayload();
            size_t payloadLength = packet.getLayerOfType<pcpp::IPv4Layer>()->getLayerPayloadSize();
            uint8_t PID_ORIGINAL_bytes[] = {0x61, 0x00, 0x18, 0x00};
            int M = sizeof(PID_ORIGINAL_bytes) / sizeof(PID_ORIGINAL_bytes[0]);
            int N = payloadLength;
            int rtpsguid_idx = KMPSearch(PID_ORIGINAL_bytes, M, payload, N);

            // 在此處處理封包
            // 例如，您可以印出封包的一些基本資訊
            // std::cout << count << ": " << "Packet Size: " << packet->getRawDataLen() << " bytes" << std::endl;
            if (rtpsguid_idx != -1)
            {
                std::cout << count << ": src --->" << ipLayer->getSrcIPAddress().toString()
                        << ", dst --->" << ipLayer->getDstIPAddress().toString()
                        << ", guid --->" << byteArrayToHexString(payload + rtpsguid_idx + 4, 12) << std::endl;
                count++;
            }
        }
    }

    // 關閉 .pcap 文件
    reader->close();
    delete reader;
    */
    auto write_to_file_func = std::bind(&write_to_file, 15000, "");
    std::thread write_to_file_thread(write_to_file_func);
    write_to_file_thread.detach();
    std::string sqlcmd = "SELECT distinct org_v_guid, dev_partition FROM vw_dds_devices_for_flow_cal "
                 "WHERE org_v_guid IS NOT NULL "
                 "AND dev_partition IS NOT NULL";

    if (opendatabase("postgresql://postgres:admin@140.110.7.17:5433/postgres"))
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

            std::cout << "guid: " << v_guid << " ---> dev_partition: " << dev_partition << std::endl;
        }

        // if(closedatabase())
        // std::cout << "Close database successfully." << std::endl;
        // else
        // std::cout << "Can't close database." << std::endl;
    }

    pcpp::PcapLiveDevice *dev = pcpp::PcapLiveDeviceList::getInstance().getPcapLiveDeviceByIpOrName("any");

    // 開啟網卡
    if (!dev->open()) {
        std::cerr << "Cannot open device" << std::endl;
        return 1;
    }

    // 開始抓取封包
    dev->startCapture(onPacketArrives, nullptr);

    // 等待，直到您準備好停止抓取
    std::cout << "Press enter to stop capturing..." << std::endl;
    std::cin.get();

    // 停止抓取並關閉網卡
    dev->stopCapture();
    dev->close();
    return 0;
}
