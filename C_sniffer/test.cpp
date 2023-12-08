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

int64_t packetCount = 0;
time_t lasttimestamp = 0;
std::string g_ipaddr = "";
std::string g_partition = "";
std::queue<nlohmann::json> jsonQueue;
std::map<std::string, std::string> guidmap;
std::queue<pcpp::Packet> packetQueue1;
std::queue<pcpp::Packet> packetQueue2;
std::queue<pcpp::Packet> specialPacketQueue;
std::mutex mtx;
pqxx::connection *connection = nullptr;
short byte_fix = 0;

bool opendatabase(const std::string& url)
{
    bool result = false;

    try {
        connection = new pqxx::connection(url);
        if (connection->is_open()) {
            result = true;
            std::cout << "Opened database successfully: " << connection->dbname() << std::endl;
        } else {
            result = false;
            std::cout << "Can't open database" << std::endl;
        }
    } catch (const std::exception& e) {
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

pqxx::result execute_query(const std::string& query) {
    if (connection == nullptr || !connection->is_open()) {
        throw std::runtime_error("Database connection is not open");
    }

    pqxx::work txn(*connection);
    pqxx::result res = txn.exec(query);
    txn.commit();
    return res;
}

void guidmanager(const std::string connection_url)
{
    while (true)
    {
        // TODO:sql command
        std::string sqlcmd =  "SELECT DISTINCT v_guid, dev_partition FROM vw_dds_devices_for_flow_cal WHERE v_guid IS NOT NULL";
        // TODO  : should change  connection info
        if (opendatabase(connection_url))
        {
            pqxx::result guidtableresult = execute_query(sqlcmd);
            for (auto const & row : guidtableresult)
            {
                // TODO:should insert dev_partition
                std::string dev_partition = row[1].as<std::string>();

                // TODO:should insert guid
                std::string v_guid = row[0].as<std::string>();
                std::cout << "guid: " << v_guid << " ---> dev_partition: " << dev_partition << std::endl;
                v_guid = v_guid.substr(0, v_guid.size() - 8);

                guidmap.emplace(v_guid, dev_partition);
                std::lock_guard<std::mutex> guard(mtx);
                for (auto partitioniter = guidmap.begin(); partitioniter != guidmap.end();)
                {
                    if (partitioniter->second ==  dev_partition && partitioniter->first != v_guid)
                        partitioniter = guidmap.erase(partitioniter);
                    else
                        ++partitioniter;
                }
                std::cout << "guid: " << v_guid << " ---> dev_partition: " << dev_partition << std::endl;
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

int main(){
    
    // generate guidmap
    // for (auto )
    guidmanager("postgresql://dds_paas:postgres@10.1.1.200:5433/paasdb");
    return 0;
}