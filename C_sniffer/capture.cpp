#include <iostream>
#include <pcapplusplus/PcapLiveDevice.h>
#include <pcapplusplus/PcapLiveDeviceList.h>
#include <pcapplusplus/SystemUtils.h>
#include <pcapplusplus/IPv4Layer.h>
#include <map>

int64_t packetCount = 0;
time_t lasttimestamp = 0;
std::map<uint16_t, int32_t> fragMap;
/**
 * A struct for collecting packet statistics
 */
struct PacketStats
{
    int ethPacketCount;
    int ipv4PacketCount;
    int ipv6PacketCount;
    int tcpPacketCount;
    int udpPacketCount;
    int dnsPacketCount;
    int httpPacketCount;
    int sslPacketCount;

    /**
     * Clear all stats
     */
    void clear()
    {
        ethPacketCount = 0;
        ipv4PacketCount = 0;
        ipv6PacketCount = 0;
        tcpPacketCount = 0;
        udpPacketCount = 0;
        tcpPacketCount = 0;
        dnsPacketCount = 0;
        httpPacketCount = 0;
        sslPacketCount = 0;
    }

    /**
     * C'tor
     */
    PacketStats() { clear(); }

    /**
     * Collect stats from a packet
     */
    void consumePacket(pcpp::Packet &packet)
    {
        if (packet.isPacketOfType(pcpp::Ethernet))
            ethPacketCount++;
        if (packet.isPacketOfType(pcpp::IPv4))
        {
            // 检查封包是否包含 IPv4 层
            pcpp::IPv4Layer *ipLayer = packet.getLayerOfType<pcpp::IPv4Layer>();
            if (ipLayer != nullptr)
            {
                // 提取源 IP 地址并打印
                // std::cout << "Source IP: " << ipLayer->getSrcIPAddress().toString() << std::endl;
                if (ipLayer->getSrcIPAddress().toString().compare("10.1.1.106") == 0)
                {
                    // 获取 IPv4 头部
                    pcpp::iphdr *ipv4Header = ipLayer->getIPv4Header();
                    // 获取 RTPS 载荷
                    uint8_t *payload = ipLayer->getLayerPayload();
                    size_t payloadLength = ipLayer->getLayerPayloadSize();
                    // 将载荷转换为字符串，以便进行搜索
                    std::string payloadStr(reinterpret_cast<char *>(payload), payloadLength);

                    // 检查载荷中是否包含 "RTPS"
                    if (payloadStr.find("RTPS") != std::string::npos)
                    {
                        struct timespec timestamp = packet.getRawPacket()->getPacketTimeStamp();
                        // 转换时间戳为可读格式
                        time_t rawTime = timestamp.tv_sec;
                        
                        
                        packetCount ++;
                        std::cout << "package Timestamp: " << rawTime << ", packetCount= " << packetCount << std::endl;
                        
                        lasttimestamp = rawTime;

                    }
                }
            }
        }
        if (packet.isPacketOfType(pcpp::IPv6))
            ipv6PacketCount++;
        if (packet.isPacketOfType(pcpp::TCP))
            tcpPacketCount++;
        if (packet.isPacketOfType(pcpp::UDP))
        {
            ;
        }
        if (packet.isPacketOfType(pcpp::DNS))
            dnsPacketCount++;
        if (packet.isPacketOfType(pcpp::HTTP))
            httpPacketCount++;
        if (packet.isPacketOfType(pcpp::SSL))
            sslPacketCount++;
    }

    /**
     * Print stats to console
     */
    void printToConsole()
    {
        // std::cout
        //     << "Ethernet packet count: " << ethPacketCount << std::endl
        //     << "IPv4 packet count:     " << ipv4PacketCount << std::endl
        //     << "IPv6 packet count:     " << ipv6PacketCount << std::endl
        //     << "TCP packet count:      " << tcpPacketCount << std::endl
        //     << "UDP packet count:      " << udpPacketCount << std::endl
        //     << "DNS packet count:      " << dnsPacketCount << std::endl
        //     << "HTTP packet count:     " << httpPacketCount << std::endl
        //     << "SSL packet count:      " << sslPacketCount << std::endl;
    }
};

static void onPacketArrives(pcpp::RawPacket *packet, pcpp::PcapLiveDevice *dev, void *cookie)
{
    // 把傳入的 cookie 做轉型原本的 PacketStats 物件
    PacketStats *stats = (PacketStats *)cookie;

    // 把 RawPacket 變成分析過的 Packet
    pcpp::Packet parsedPacket(packet);

    // 如果封包是 IPv4
    if (parsedPacket.isPacketOfType(pcpp::IPv4))
    {
        // 找出 Source IP 跟 Destination IP
        pcpp::IPv4Address srcIP = parsedPacket.getLayerOfType<pcpp::IPv4Layer>()->getSrcIPv4Address();
        pcpp::IPv4Address destIP = parsedPacket.getLayerOfType<pcpp::IPv4Layer>()->getDstIPv4Address();

    }

    // 讓 PacketStats 去做統計
    stats->consumePacket(parsedPacket);
}

int main(int argc, char *argv[])
{
    // 可以用 ifconfig 找一下網卡的名字，例如 lo, eth0
    pcpp::PcapLiveDevice *dev = pcpp::PcapLiveDeviceList::getInstance().getPcapLiveDeviceByIpOrName("eno1");

    // 輸出網卡資訊
    std::cout
        << "Interface info:" << std::endl
        << "   Interface name:        " << dev->getName() << std::endl           // get interface name
        << "   Interface description: " << dev->getDesc() << std::endl           // get interface description
        << "   MAC address:           " << dev->getMacAddress() << std::endl     // get interface MAC address
        << "   Default gateway:       " << dev->getDefaultGateway() << std::endl // get default gateway
        << "   Interface MTU:         " << dev->getMtu() << std::endl;           // get interface MTU

    if (dev->getDnsServers().size() > 0)
        std::cout << "   DNS server:            " << dev->getDnsServers().at(0) << std::endl;

    if (!dev->open())
    {
        throw(std::runtime_error("cannot open device, try with sudo?"));
    }

    PacketStats stats;

    std::cout << std::endl
              << "Starting async capture..." << std::endl;

    dev->startCapture(onPacketArrives, &stats);

    // sleep for 10 seconds in main thread, in the meantime packets are captured in the async thread

    while (1)
    {
        // pcpp::multiPlatformSleep(1);
    }
    // stop capturing packets
    dev->stopCapture();

    stats.printToConsole();
    return 0;
}