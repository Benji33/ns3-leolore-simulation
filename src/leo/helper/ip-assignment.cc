#include "ns3/ip-assignment.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/string.h"
#include "ns3/log.h"

namespace ns3 {
namespace leo {

NS_LOG_COMPONENT_DEFINE("IpAssignmentHelper");

std::unordered_map<std::string, Ipv4Address> IpAssignmentHelper::AssignIpAddresses(
    const std::vector<leo::FileReader::Edge>& edges,
    std::unordered_map<std::string, Ptr<Node>>& sourceIdNsNodeMap)
{
    // Speed of light in km/s
    const double speedOfLight = 299792.4580;

    std::unordered_map<std::string, Ipv4Address> nodeIdToIpMap;

    Ipv4AddressHelper ipv4;
    int maj_subnet_counter = 1;
    int min_subnet_counter = 0;

    for (const auto& edge : edges) {
        if (sourceIdNsNodeMap.find(edge.source) == sourceIdNsNodeMap.end()) {
            printf("Source node not found: %s\n", edge.source.c_str());
            NS_LOG_ERROR("Source node not found: " << edge.source);
            continue;
        }
        if (sourceIdNsNodeMap.find(edge.target) == sourceIdNsNodeMap.end()) {
            printf("Target node not found: %s\n", edge.target.c_str());
            NS_LOG_ERROR("Target node not found: " << edge.target);
            continue;
        }

        auto sourceNode = sourceIdNsNodeMap[edge.source];
        auto targetNode = sourceIdNsNodeMap[edge.target];

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
        NetDeviceContainer devices = p2p.Install(sourceNode, targetNode);

        // Assign IP addresses to the link
        if (maj_subnet_counter > 255) {
            NS_LOG_ERROR("Exceeded maximum number of subnets");
            break;
        }
        std::ostringstream subnetStream;
        subnetStream << "10." << maj_subnet_counter << "." << min_subnet_counter << ".0";
        std::string subnet = subnetStream.str();

        ipv4.SetBase(subnetStream.str().c_str(), "255.255.255.0");
        Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

        Ipv4Address srcIp = interfaces.GetAddress(0);
        Ipv4Address dstIp = interfaces.GetAddress(1);

        if (nodeIdToIpMap.count(edge.source) == 0) {
            nodeIdToIpMap[edge.source] = srcIp;
        }
        if (nodeIdToIpMap.count(edge.target) == 0) {
            nodeIdToIpMap[edge.target] = dstIp;
        }

        min_subnet_counter++;
        if (min_subnet_counter == 255) {
            maj_subnet_counter++;
            min_subnet_counter = 0;
        }
    }

    return nodeIdToIpMap;
}

} // namespace leo
} // namespace ns3
