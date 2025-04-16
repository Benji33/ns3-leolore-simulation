#ifndef ROUTING_MANAGER_H
#define ROUTING_MANAGER_H

#include <unordered_map>
#include <vector>
#include <string>
#include "ns3/ipv4-address.h"
#include "ns3/node.h"
#include "ns3/nstime.h"
#include "file-reader.h"
#include "ip-assignment.h"
#include "ns3/network-state.h"
#include <chrono>
#include <iomanip>
#include <ctime>

namespace ns3
{
namespace leo
{
struct SwitchingTable {
    std::string node_id; // Resolved IP address of the node
    ns3::Time valid_from;     // Validity start time
    ns3::Time valid_until;    // Validity end time
    std::unordered_map<std::string, std::string> routing_table; // Node ID -> Next-hop node ID
    //std::unordered_map<ns3::Ipv4Address, ns3::Ipv4Address> ip_routing_table; // Destination IP -> Next-hop IP

    // Constructors
    SwitchingTable(std::string nodeId, ns3::Time from, ns3::Time until,
        std::unordered_map<std::string, std::string> table_data)
    : node_id(nodeId),
    valid_from(from),
    valid_until(until),
    routing_table(table_data) {}

    // Default constructor
    SwitchingTable()
    { node_id="";
        valid_from = ns3::Seconds(0);
        valid_until = ns3::Seconds(0);
        routing_table = {};
    }


};

class RoutingManager {
public:
    RoutingManager() = default;

    // Resolve raw switching tables into IP-based switching tables
    void ResolveSwitchingTables(
        const std::vector<FileReader::RawSwitchingTable>& raw_tables,
        //const std::unordered_map<std::string, std::vector<Ipv4Address>>& nodeIdToIpMap,
        leo::NetworkState& networkState,
        const std::chrono::system_clock::time_point& simulationStart);

    void AttachSwitchingTablesToNodes(leo::NetworkState& networkState) const;

    // Getter for resolved switching tables
    const std::vector<SwitchingTable>& GetSwitchingTables() const { return switching_tables; }

private:
    // Switching table consisting of IP address mappping
    std::vector<SwitchingTable> switching_tables;
};

}
} // namespace ns3

#endif // ROUTING_MANAGER_H
