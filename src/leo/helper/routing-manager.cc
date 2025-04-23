// routing-manager.cc
#include "routing-manager.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include <sstream>
#include <chrono>
#include <iomanip>
#include <ctime>
#include "ns3/constellation-node-data.h"

NS_LOG_COMPONENT_DEFINE("RoutingManager");

namespace ns3
{
namespace leo
{
// Helper to parse time string to ns3::Time
#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <string>

Time ParseTimeString(const std::string& timeStr, const std::chrono::system_clock::time_point& simStart) {
    std::tm tm = {};
    char dot;
    int microseconds = 0;

    std::istringstream ss(timeStr);

    // Parse time up to seconds
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");

    NS_LOG_DEBUG("SS: " << timeStr << " - " << (tm.tm_year + 1900) << "-"
                 << (tm.tm_mon + 1) << "-" << tm.tm_mday << " "
                 << tm.tm_hour << ":" << tm.tm_min << ":" << tm.tm_sec);

    // Microseconds
    if (ss.peek() == '.') {
        ss >> dot;
        std::string microStr;
        while (isdigit(ss.peek()) && microStr.length() < 6) {
            microStr += static_cast<char>(ss.get());
        }
        while (microStr.length() < 6) microStr += '0'; // pad if needed
        microseconds = std::stoi(microStr);
        NS_LOG_DEBUG("Microseconds: " << microseconds);
    }

    // Skip timezone
    if (ss.peek() == '+' || ss.peek() == '-') {
        std::string tzOffset;
        ss >> tzOffset;
        NS_LOG_DEBUG("Timezone offset: " << tzOffset);
    }

    // Now treat the tm struct as UTC and convert to time_t using timegm
    std::time_t utcTimeT = timegm(&tm);

    // Create a time_point in UTC + microseconds
    auto tp = std::chrono::system_clock::from_time_t(utcTimeT)
            + std::chrono::microseconds(microseconds);

    std::chrono::duration<double> diff = tp - simStart;

    NS_LOG_DEBUG("Parsed time: " << timeStr << " -> "
                 << std::chrono::duration_cast<std::chrono::seconds>(diff).count()
                 << " seconds since simulation start at "
                 << simStart.time_since_epoch().count());

    return Seconds(diff.count());
}


void RoutingManager::ResolveSwitchingTables(
    const std::vector<FileReader::RawSwitchingTable>& raw_tables,
    //const std::unordered_map<std::string, std::vector<Ipv4Address>>& nodeIdToIpMap,
    leo::NetworkState& networkState,
    const std::chrono::system_clock::time_point& simulationStart) {

    for (const auto& table : raw_tables) {
      /*  // Create a local routing table
        std::unordered_map<ns3::Ipv4Address, ns3::Ipv4Address> routing_table = {
            {Ipv4Address::GetLoopback(), Ipv4Address::GetLoopback()} // Initialize with loopback address
        };

        // Resolve the routing table entries
        for (const auto& route : table.table_data) {
            const std::string& sourceNodeId = table.node; // Current node id
            const std::string& destNodeId = route.first;  // Destination node id
            const std::string& nextHopNodeId = route.second; // Next hop id

            // Get the next hop device and its IP address
            auto linkInfo = networkState.GetLinkInfo(sourceNodeId, nextHopNodeId);
            if (!linkInfo.IsValid()) {
                NS_LOG_WARN("No link info found for source: " << sourceNodeId << " and next hop: " << nextHopNodeId);
                continue;
            }

            Ipv4Address nextHopIp = networkState.GetIpAddressForDevice(linkInfo.deviceA);

            // Get the destination IP address
            auto destIt = nodeIdToIpMap.find(destNodeId);
            if (destIt == nodeIdToIpMap.end()) {
                NS_LOG_WARN("Invalid destination node ID: " << destNodeId);
                continue;
            }
            Ipv4Address destIp = destIt->second.front(); // Use the first IP for now

            // Add the resolved route to the routing table
            routing_table[destIp] = nextHopIp;
            NS_LOG_INFO("Resolved route: " << destIp << " -> " << nextHopIp);
        }
    */
        // Create the SwitchingTable object using the constructor
        SwitchingTable resolved(
            table.node,
            ParseTimeString(table.valid_from, simulationStart),
            ParseTimeString(table.valid_until, simulationStart),
            std::move(table.table_data)
        );

        // Delete loopback address from routing table
        //routing_table.erase(Ipv4Address::GetLoopback());

        // Add the resolved SwitchingTable to the vector
        switching_tables.push_back(std::move(resolved));
    }
}

void RoutingManager::AttachSwitchingTablesToNodes(
    leo::NetworkState& networkState) const
{
    for (const auto& table : switching_tables) {
        std::string nodeId = table.node_id;
        Ptr<Node> node = networkState.GetNodeBySourceId(nodeId);
        if (node != nullptr) {
            Ptr<leo::ConstellationNodeData> data = node->GetObject<leo::ConstellationNodeData>();
            if (data) {
                data->AddSwitchingTable(table);
                NS_LOG_DEBUG("Attached switching table to node " << nodeId);
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
