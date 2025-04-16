#include "ns3/ip-assignment.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/string.h"
#include "ns3/log.h"
#include "ns3/custom-ipv4-l3-protocol.h"
#include <sstream>

namespace ns3 {
namespace leo {

constexpr double speedOfLight = 299792.4580;

NS_LOG_COMPONENT_DEFINE("IpAssignmentHelper");

std::unordered_map<std::string, std::vector<Ipv4Address>> IpAssignmentHelper::AssignIpAddresses(
    const std::vector<leo::FileReader::Edge>& edges,
    leo::NetworkState& networkState)
    //td::unordered_map<std::string, Ptr<Node>>& sourceIdNsNodeMap)
{
    // Speed of light in km/s
    const double speedOfLight = 299792.4580;

    // Each node can have mulitple links (ISLs, feeder) so they need multiple IPs
    std::unordered_map<std::string, std::vector<Ipv4Address>> nodeIdToIpMap;

    Ipv4AddressHelper ipv4;
    int maj_subnet_counter = 1;
    int min_subnet_counter = 0;

    for (const auto& edge : edges) {
        auto sourceNode = networkState.GetNodeBySourceId(edge.source);
        auto targetNode = networkState.GetNodeBySourceId(edge.target);

        if (sourceNode == nullptr) {
            printf("Source node not found: %s\n", edge.source.c_str());
            NS_LOG_ERROR("Source node not found: " << edge.source);
            continue;
        }
        if (targetNode == nullptr) {
            printf("Target node not found: %s\n", edge.target.c_str());
            NS_LOG_ERROR("Target node not found: " << edge.target);
            continue;
        }

        // Consider delay based on distance between nodes
        PointToPointHelper p2p;
        // Delay in seconds
        double delayInSeconds = edge.weight / speedOfLight;
        std::ostringstream delayStream;
         // Delay in milliseconds
        delayStream << (delayInSeconds * 1e3) << "ms";
        p2p.SetChannelAttribute("Delay", StringValue(delayStream.str()));

        // data rate - should maybe be configured based on distance as well later
        p2p.SetDeviceAttribute("DataRate", StringValue("10Gbps"));

        // Create a point-to-point link between the source and target nodes
        //NS_LOG_INFO("Creating link between " << edge.source << " and " << edge.target);
        NetDeviceContainer devices = p2p.Install(sourceNode, targetNode);

        // Assign IP addresses to the link
        if (maj_subnet_counter > 255) {
            NS_LOG_ERROR("Exceeded maximum number of subnets");
            break;
        }

        std::ostringstream subnetStream;
        subnetStream << "10." << maj_subnet_counter << "." << min_subnet_counter << ".0";
        ipv4.SetBase(subnetStream.str().c_str(), "255.255.255.0");
        Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

        // Store the assigned IP addresses of the connection
        m_nodeToNodeIpMap[edge.source][edge.target] = {interfaces.GetAddress(0), interfaces.GetAddress(1)};
        m_nodeToNodeIpMap[edge.target][edge.source] = {interfaces.GetAddress(1), interfaces.GetAddress(0)};

        Ipv4Address srcIp = interfaces.GetAddress(0);
        Ipv4Address dstIp = interfaces.GetAddress(1);

        nodeIdToIpMap[edge.source].push_back(srcIp);
        nodeIdToIpMap[edge.target].push_back(dstIp);

        Ptr<Channel> channel = devices.Get(0)->GetChannel();
        networkState.RegisterLink(edge.source, edge.target, devices.Get(0), devices.Get(1), channel, srcIp, dstIp);

        min_subnet_counter++;
        if (min_subnet_counter == 255) {
            maj_subnet_counter++;
            min_subnet_counter = 0;
        }
    }

    return nodeIdToIpMap;
}

void IpAssignmentHelper::PrecreateAllLinks(const std::map<std::pair<std::string, std::string>, double>& allLinks, NetworkState& networkState) {
    Ipv4AddressHelper ipv4;
    int maj_subnet_counter = 1;
    int min_subnet_counter = 0;

    for (const auto& [link, weight] : allLinks) {
        const std::string& sourceId = link.first;
        const std::string& targetId = link.second;

        Ptr<Node> sourceNode = networkState.GetNodeBySourceId(sourceId);
        Ptr<Node> targetNode = networkState.GetNodeBySourceId(targetId);

        if (!sourceNode || !targetNode) {
            NS_LOG_WARN("Nodes not found for link: " << sourceId << " ↔ " << targetId);
            continue;
        }

        // Get Delay
        double delayInSeconds = weight / speedOfLight;
        std::ostringstream delayStream;
         // Delay in milliseconds
        delayStream << (delayInSeconds * 1e3) << "ms";

        // Create the link
        PointToPointHelper p2p;
        p2p.SetDeviceAttribute("DataRate", StringValue("10Gbps"));
        p2p.SetChannelAttribute("Delay", StringValue(delayStream.str()));

        NetDeviceContainer devices = p2p.Install(sourceNode, targetNode);

        // Assign IP addresses
        if (maj_subnet_counter > 255) {
            NS_LOG_ERROR("Exceeded maximum number of subnets");
            break;
        }
        std::ostringstream subnetStream;
        subnetStream << "10." << maj_subnet_counter << "." << min_subnet_counter << ".0";
        ipv4.SetBase(subnetStream.str().c_str(), "255.255.255.0");
        Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);
        //NS_LOG_INFO("Assigned IP addresses: " << interfaces.GetAddress(0) << " and " << interfaces.GetAddress(1));
        // Register the link in NetworkState
        Ptr<Channel> channel = devices.Get(0)->GetChannel();
        networkState.RegisterLink(sourceId, targetId, devices.Get(0), devices.Get(1), channel, interfaces.GetAddress(0), interfaces.GetAddress(1));

        // Update subnet counters
        min_subnet_counter++;
        if (min_subnet_counter == 255) {
            maj_subnet_counter++;
            min_subnet_counter = 0;
        }
    }
}


std::pair<Ipv4Address, Ipv4Address> IpAssignmentHelper::GetIpPair(const std::string& sourceId, const std::string& targetId) const {
    auto sourceIt = m_nodeToNodeIpMap.find(sourceId);
    if (sourceIt != m_nodeToNodeIpMap.end()) {
        auto targetIt = sourceIt->second.find(targetId);
        if (targetIt != sourceIt->second.end()) {
            return targetIt->second;
        }
    }
    NS_LOG_WARN("No IP pair found for source: " << sourceId << " and target: " << targetId);
    return {Ipv4Address(), Ipv4Address()};
}
std::unordered_map<std::string, std::pair<Ipv4Address, Ipv4Address>> IpAssignmentHelper::GetIpMappingsForSource(const std::string& sourceId) const {
    auto sourceIt = m_nodeToNodeIpMap.find(sourceId);
    if (sourceIt != m_nodeToNodeIpMap.end()) {
        return sourceIt->second; // Return all mappings for the source node
    }

    NS_LOG_WARN("No IP mappings found for source node: " << sourceId);
    return {}; // Return an empty map if no mappings are found
}

} // namespace leo
} // namespace ns3
