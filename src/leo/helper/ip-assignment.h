#ifndef NS3_IP_ASSIGNMENT_H
#define NS3_IP_ASSIGNMENT_H

#include <unordered_map>
#include <string>
#include "ns3/ipv4-address.h"
#include "ns3/node.h"
#include "ns3/net-device-container.h"
#include "ns3/file-reader.h"

namespace ns3 {
namespace leo {

class IpAssignmentHelper {
public:
    static std::unordered_map<std::string, std::vector<Ipv4Address>> AssignIpAddresses(
        const std::vector<leo::FileReader::Edge>& edges,
        std::unordered_map<std::string, Ptr<Node>>& sourceIdNsNodeMap
    );
};

} // namespace leo
} // namespace ns3

#endif // NS3_IP_ASSIGNMENT_H
