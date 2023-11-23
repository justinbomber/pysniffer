#include <iostream>
#include <pcapplusplus/PcapLiveDevice.h>
#include <pcapplusplus/PcapLiveDeviceList.h>
#include <pcapplusplus/SystemUtils.h>
#include <pcapplusplus/IPv4Layer.h>
#include <pcapplusplus/EthLayer.h>
#include <pcapplusplus/ArpLayer.h>

static void onPacketArrives(pcpp::RawPacket *packet, pcpp::PcapLiveDevice *dev, void *cookie)
{
    // 把 RawPacket 變成分析過的 Packet
    pcpp::Packet parsedPacket(packet);

    // 檢查是否為 ARP 封包
    if (parsedPacket.isPacketOfType(pcpp::ARP))
    {
        pcpp::ArpLayer *arpLayer = parsedPacket.getLayerOfType<pcpp::ArpLayer>();

        // 檢查 APR 是否為 APR Reply
        if (be16toh(arpLayer->getArpHeader()->opcode) == pcpp::ARP_REPLY) // 要做 be16toh 因為網路的編碼是 Big Endian，一般電腦則是 Little Endian
        {
            std::cout << arpLayer->getSenderIpAddr() << ": " << arpLayer->getSenderMacAddress() << std::endl;
        }
    }
}

void sendARPRequest(const std::string &dstIpAddr, pcpp::PcapLiveDevice *dev)
{
    // Create an ARP packet and change its fields
    pcpp::Packet arpRequest(500);

    // 把來源 IP 換一個跟目標同域的任一 IP
    std::string srcIPAddr = dstIpAddr;
    while (srcIPAddr.back() != '.')
    {
        srcIPAddr.pop_back();
    }
    srcIPAddr.push_back('2');

    pcpp::MacAddress macSrc = dev->getMacAddress();
    pcpp::MacAddress macDst(0xff, 0xff, 0xff, 0xff, 0xff, 0xff);
    pcpp::EthLayer ethLayer(macSrc, macDst, (uint16_t)PCPP_ETHERTYPE_ARP);
    pcpp::ArpLayer arpLayer(pcpp::ARP_REQUEST,
                            macSrc,     // 來源 MAC，不一定要是真的
                            macDst,     // 目標 MAC
                            pcpp::IPv4Address(srcIPAddr),  // 來源 IP，一樣可以造假
                            pcpp::IPv4Address(dstIpAddr)); // 目標 IP

    arpRequest.addLayer(&ethLayer);
    arpRequest.addLayer(&arpLayer);
    arpRequest.computeCalculateFields();

    // 發送 ARP
    dev->sendPacket(&arpRequest);
}

int main(int argc, char *argv[])
{
    if(argc != 3) {
        throw(std::runtime_error("usage: sudo ./arp eth0 172.22.240.1"));
    }
    std::string interfacaName = argv[1];
    std::string targetIp = argv[2];

    // 開啟 eth0 介面
    pcpp::PcapLiveDevice *dev = pcpp::PcapLiveDeviceList::getInstance().getPcapLiveDeviceByIpOrName(interfacaName);

    if (!dev->open())
    {
        throw(std::runtime_error("cannot open device, try with sudo?"));
    }

    // 發 ARP 去查 targetIp
    sendARPRequest(targetIp, dev);

    // 同時開始監聽 Packets
    dev->startCapture(onPacketArrives, nullptr);
    pcpp::multiPlatformSleep(1);
    dev->stopCapture();

    return 0;
}