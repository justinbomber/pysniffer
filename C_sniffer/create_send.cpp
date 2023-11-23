#include <iostream>
#include <pcapplusplus/PcapLiveDevice.h>
#include <pcapplusplus/PcapLiveDeviceList.h>
#include <pcapplusplus/SystemUtils.h>
#include <pcapplusplus/IPv4Layer.h>
#include <pcapplusplus/EthLayer.h>
#include <pcapplusplus/UdpLayer.h>
#include <pcapplusplus/DnsLayer.h>

int main(int argc, char *argv[])
{
    // 可以用 ifconfig 找一下網卡的名字，例如 lo, eth0
    pcpp::PcapLiveDevice *dev = pcpp::PcapLiveDeviceList::getInstance().getPcapLiveDeviceByIpOrName("lo");

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

    // 建立 Ethernet layer
    pcpp::EthLayer newEthernetLayer(pcpp::MacAddress("00:50:43:11:22:33"), pcpp::MacAddress("aa:bb:cc:dd:ee"));

    // 建立IPv4 layer
    pcpp::IPv4Layer newIPLayer(pcpp::IPv4Address("192.168.1.1"), pcpp::IPv4Address("192.168.1.3"));
    newIPLayer.getIPv4Header()->timeToLive = 64;

    // 建立 UDP layer
    pcpp::UdpLayer newUdpLayer(12345, 53);

    //建立 DNS layer
    pcpp::DnsLayer newDnsLayer;
    newDnsLayer.addQuery("localhost", pcpp::DNS_TYPE_A, pcpp::DNS_CLASS_IN);

    // 建立一個 Packet，預設 Size 是 100，概念跟 std::string 一樣
    pcpp::Packet newPacket(100);

    // 把 Layer 都加上 Packet
    newPacket.addLayer(&newEthernetLayer);
    newPacket.addLayer(&newIPLayer);
    newPacket.addLayer(&newUdpLayer);
    newPacket.addLayer(&newDnsLayer);

    // 去計算要怎麼把 Layer 加上 Packet
    newPacket.computeCalculateFields();

    // 發送 Packet
    dev->sendPacket(&newPacket);
    return 0;
}