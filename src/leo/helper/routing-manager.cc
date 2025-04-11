// routing-manager.cc
#include "routing-manager.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include <sstream>
#include <chrono>
#include <iomanip>
#include <ctime>
#include "ns3/custom-node-data.h"

NS_LOG_COMPONENT_DEFINE("RoutingManager");

namespace ns3
{
namespace leo
{
// Helper to parse time string to ns3::Time
Time ParseTimeString(const std::string& timeStr, const std::chrono::system_clock::time_point& simStart) {
    std::istringstream ss(timeStr);
    std::tm tm = {};
    char dot; // to eat the '.' before microseconds
    int microseconds = 0;

    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");

    if (ss.peek() == '.') {
        ss >> dot >> microseconds;
    }

    std::time_t timeT = timegm(&tm); // assumes UTC
    auto tp = std::chrono::system_clock::from_time_t(timeT) +
              std::chrono::microseconds(microseconds);

    std::chrono::duration<double> diff = tp - simStart;
    return Seconds(diff.count());
}

void RoutingManager::ResolveSwitchingTables(
    const std::vector<FileReader::RawSwitchingTable>& raw_tables,
    const std::unordered_map<std::string, std::vector<Ipv4Address>>& nodeIdToIpMap,
    const std::chrono::system_clock::time_point& simulationStart) {

    for (const auto& table : raw_tables) {
        // Create a local routing table
        std::unordered_map<ns3::Ipv4Address, ns3::Ipv4Address> routing_table = {
            {Ipv4Address::GetLoopback(), Ipv4Address::GetLoopback()} // Initialize with loopback address
        };

        // Resolve the routing table entries
        for (const auto& route : table.table_data) {
            auto destIt = nodeIdToIpMap.find(route.first);
            auto nextHopIt = nodeIdToIpMap.find(route.second);

            if (destIt == nodeIdToIpMap.end() || nextHopIt == nodeIdToIpMap.end()) {
                NS_LOG_WARN("Invalid route entry: " << route.first << " -> " << route.second);
                continue;
            }

            // Use the first IP for now
            Ipv4Address destIp = destIt->second.front();
            Ipv4Address nextHopIp = nextHopIt->second.front();
            routing_table[destIp] = nextHopIp;

        }

        // Create the SwitchingTable object using the constructor
        SwitchingTable resolved(
            table.node,
            ParseTimeString(table.valid_from, simulationStart),
            ParseTimeString(table.valid_until, simulationStart),
            std::move(routing_table)
        );

        // Delete loopback address from routing table
        routing_table.erase(Ipv4Address::GetLoopback());

        // Add the resolved SwitchingTable to the vector
        switching_tables.push_back(std::move(resolved));

    }
}
void RoutingManager::AttachSwitchingTablesToNodes(
    const std::unordered_map<std::string, Ptr<Node>>& sourceIdNsNodeMap) const
{
    for (const auto& table : switching_tables) {
        std::string nodeId = table.node_id;
        auto it = sourceIdNsNodeMap.find(nodeId);
        if (it != sourceIdNsNodeMap.end()) {
            Ptr<Node> node = it->second;
            Ptr<leo::ConstellationNodeData> data = node->GetObject<leo::ConstellationNodeData>();
            if (data) {
                data->SetSwitchingTable(table);
                NS_LOG_INFO("Attached switching table to node " << nodeId);
            } else {
                NS_LOG_WARN("No ConstellationNodeData found on node " << nodeId);
            }
        } else {
            NS_LOG_WARN("No node found for nodeId " << nodeId);
        }
    }
}
}
} // namespace ns3
