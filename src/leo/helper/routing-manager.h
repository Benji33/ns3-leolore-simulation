#ifndef ROUTING_MANAGER_H
#define ROUTING_MANAGER_H

#include <unordered_map>
#include <vector>
#include <string>
#include "ns3/ipv4-address.h"
#include "ns3/node.h"
#include "ns3/nstime.h"
#include "file-reader.h"
#include "ipv4-address-hash.h"
#include <chrono>
#include <iomanip>
#include <ctime>

namespace ns3
{
namespace leo
{
struct SwitchingTable {
    ns3::Ipv4Address node_ip; // Resolved IP address of the node
    ns3::Time valid_from;     // Validity start time
    ns3::Time valid_until;    // Validity end time
    std::unordered_map<ns3::Ipv4Address, ns3::Ipv4Address> ip_routing_table; // Destination IP -> Next-hop IP

    // Constructor
    SwitchingTable(ns3::Ipv4Address ip, ns3::Time from, ns3::Time until,
        std::unordered_map<ns3::Ipv4Address, ns3::Ipv4Address> routing_table)
    : node_ip(ip),
    valid_from(from),
    valid_until(until),
    ip_routing_table(std::move(routing_table)) {}

};

class RoutingManager {
public:
    RoutingManager() = default;

    // Resolve raw switching tables into IP-based switching tables
    std::vector<SwitchingTable> ResolveSwitchingTables(
        const std::vector<FileReader::RawSwitchingTable>& raw_tables,
        const std::unordered_map<std::string, ns3::Ipv4Address>& nodeIdToIpMap,
        const std::chrono::system_clock::time_point& simulationStart);

    // Getter for resolved switching tables
    const std::vector<SwitchingTable>& GetSwitchingTables() const { return switching_tables; }

private:
    // Switching table consisting of IP address mappping
    std::vector<SwitchingTable> switching_tables;
};

}
} // namespace ns3

#endif // ROUTING_MANAGER_H