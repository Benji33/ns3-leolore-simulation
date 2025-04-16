#ifndef NS3_IP_ASSIGNMENT_H
#define NS3_IP_ASSIGNMENT_H

#include <unordered_map>
#include <string>
#include "ns3/ipv4-address.h"
#include "ns3/node.h"
#include "ns3/net-device-container.h"
#include "ns3/file-reader.h"
#include "ns3/network-state.h"

namespace ns3 {
namespace leo {

class IpAssignmentHelper {
public:
    std::unordered_map<std::string, std::vector<Ipv4Address>> AssignIpAddresses(
        const std::vector<leo::FileReader::Edge>& edges,
        leo::NetworkState& networkState
    );
    void PrecreateAllLinks(const std::map<std::pair<std::string, std::string>, double>& allLinks, NetworkState& networkState);
    std::pair<Ipv4Address, Ipv4Address> GetIpPair(const std::string& sourceId, const std::string& targetId) const;
    std::unordered_map<std::string, std::pair<Ipv4Address, Ipv4Address>> GetIpMappingsForSource(const std::string& sourceId) const;

private:
    // List of connected interfaces - source node ID and target node ID as keys to store the corresponding IP address pair.
    std::unordered_map<std::string, std::unordered_map<std::string, std::pair<Ipv4Address, Ipv4Address>>> m_nodeToNodeIpMap;

};

} // namespace leo
} // namespace ns3

#endif // NS3_IP_ASSIGNMENT_H
