#include "custom-ipv4-l3-protocol.h"
#include "ns3/log.h"
#include "ns3/ipv4-route.h"
#include "ns3/inet-socket-address.h"
#include "ns3/custom-node-data.h"

namespace ns3 {
namespace leo {

NS_LOG_COMPONENT_DEFINE("CustomRoutingProtocol");

TypeId CustomRoutingProtocol::GetTypeId() {
    static TypeId tid = TypeId("ns3::leo::CustomRoutingProtocol")
        .SetParent<Ipv4RoutingProtocol>()
        .SetGroupName("Internet")
        .AddConstructor<CustomRoutingProtocol>();
    return tid;
}

CustomRoutingProtocol::CustomRoutingProtocol() {}

CustomRoutingProtocol::~CustomRoutingProtocol() {}

void CustomRoutingProtocol::SetSwitchingTable(const SwitchingTable& table) {
    m_switchingTable = table;
}

Ptr<Ipv4Route> CustomRoutingProtocol::RouteOutput(Ptr<Packet> packet, const Ipv4Header& header,
                                                   Ptr<NetDevice> oif, Socket::SocketErrno& sockerr) {
    // Probably need cutom header for labels later on?
    Ipv4Address dst = header.GetDestination();
    NS_LOG_INFO("RouteOutput called for destination: " << dst);
    // Print m_nextHopToDeviceMap
    /*NS_LOG_INFO("Next hop to device map:");
    for (const auto& entry : m_nextHopToDeviceMap) {
        NS_LOG_INFO("  " << entry.first << " -> " << entry.second->GetAddress());
    }*/
    // Look up the destination in the switching table
    auto it = m_switchingTable.ip_routing_table.find(dst);
    if (it != m_switchingTable.ip_routing_table.end()) {
        //NS_LOG_INFO("Switching table hit: " << dst << " -> " << it->second);

        Ptr<Ipv4Route> route = Create<Ipv4Route>();
        route->SetDestination(dst);
        route->SetGateway(it->second); // Next hop?
        route->SetSource(header.GetSource());

        // Determine the correct output device
        int32_t interface = GetInterfaceForNextHop(it->second);
        if (interface >= 0) {
            route->SetOutputDevice(m_ipv4->GetNetDevice(interface));
        } else {
            NS_LOG_WARN("No interface found for next hop: " << it->second);
            NS_LOG_INFO("Listing all interfaces and their IP addresses for this node:");
            for (uint32_t i = 0; i < m_ipv4->GetNInterfaces(); ++i) {
                for (uint32_t j = 0; j < m_ipv4->GetNAddresses(i); ++j) {
                    NS_LOG_INFO("  Interface " << i << ", Address " << j << ": " << m_ipv4->GetAddress(i, j).GetLocal());
                }
            }
            sockerr = Socket::ERROR_NOROUTETOHOST;
            return nullptr;
        }

        return route;
    }

    NS_LOG_WARN("No route found for destination: " << dst);
    sockerr = Socket::ERROR_NOROUTETOHOST;
    return nullptr;
}

bool CustomRoutingProtocol::RouteInput(Ptr<const Packet> packet, const Ipv4Header& header,
                                        Ptr<const NetDevice> idev, // NetDevice where packet arrived
                                        const Ipv4RoutingProtocol::UnicastForwardCallback& ucb,
                                        const Ipv4RoutingProtocol::MulticastForwardCallback& mcb,
                                        const Ipv4RoutingProtocol::LocalDeliverCallback& lcb,
                                        const Ipv4RoutingProtocol::ErrorCallback& ecb)
{
    Ipv4Address dst = header.GetDestination();
    //NS_LOG_INFO("RouteInput called for destination: " << dst);
    //NS_LOG_INFO("Source IP in packet: " << header.GetSource());

    uint32_t incoming_interface = m_ipv4->GetInterfaceForDevice(idev);
    if (incoming_interface == uint32_t(-1)) { // Check if the interface is invalid
        NS_LOG_WARN("No interface found for the arriving NetDevice.");
        return false;
    }
    Ipv4Address incomingIp = m_ipv4->GetAddress(incoming_interface, 0).GetLocal();
    NS_LOG_INFO("Packet with destination " << dst << " received on interface " << incoming_interface
             << " with IP address: " << incomingIp);

    // Check if the destination is current node/address
    if (m_ipv4->IsDestinationAddress(dst, incoming_interface)) {
        NS_LOG_INFO("Packet is for this node, delivering to application layer...");
        lcb(packet, header, incoming_interface); // Deliver the packet locally
        return true;
    }

    // Look up the destination in the switching table
    auto it = m_switchingTable.ip_routing_table.find(dst);
    if (it != m_switchingTable.ip_routing_table.end()) {
        //NS_LOG_INFO("Switching table hit: " << dst << " -> " << it->second);
        NS_LOG_INFO("Sending packet to: " << it->second);

        Ptr<Ipv4Route> route = Create<Ipv4Route>();
        route->SetDestination(dst);
        route->SetGateway(it->second);
        route->SetSource(header.GetSource());

        // Determine the correct output device
        int32_t outgoing_interface = GetInterfaceForNextHop(it->second);
        if (outgoing_interface >= 0) {
            route->SetOutputDevice(m_ipv4->GetNetDevice(outgoing_interface));
        } else {
            NS_LOG_WARN("No outgoing_interface found for next hop: " << it->second);
            NS_LOG_INFO("Listing all outgoing_interface and their IP addresses for this node:");
            for (uint32_t i = 0; i < m_ipv4->GetNInterfaces(); ++i) {
                for (uint32_t j = 0; j < m_ipv4->GetNAddresses(i); ++j) {
                    NS_LOG_INFO("  outgoing_interface " << i << ", Address " << j << ": " << m_ipv4->GetAddress(i, j).GetLocal());
                }
            }
        }

        // Forward the packet using the UnicastForwardCallback
        ucb(route, packet, header);
        return true;
    }

    NS_LOG_WARN("No route found for destination: " << dst);
    return false;
}
int32_t CustomRoutingProtocol::GetInterfaceForNextHop(Ipv4Address nextHop) {
    // Look up the next-hop IP in the map to find the corresponding output device
    auto it = m_nextHopToDeviceMap.find(nextHop);
    if (it != m_nextHopToDeviceMap.end()) {
        // Return the index of the interface corresponding to this device
        return m_ipv4->GetInterfaceForDevice(it->second);
    }
    // If not found, return a negative value to indicate failure
    return -1;
}
void CustomRoutingProtocol::NotifyInterfaceUp(uint32_t interface) {
    //NS_LOG_INFO("Interface " << interface << " is up");
    // Dummy implementation
}

void CustomRoutingProtocol::NotifyInterfaceDown(uint32_t interface) {
    //NS_LOG_INFO("Interface " << interface << " is down");
    // Dummy implementation
}

void CustomRoutingProtocol::NotifyAddAddress(uint32_t interface, Ipv4InterfaceAddress address) {
    //NS_LOG_INFO("Address " << address.GetLocal() << " added to interface " << interface);

    /*// Update the map with the new IP-to-device mapping
    Ipv4Address addedIp = address.GetLocal();
    Ptr<NetDevice> device = m_ipv4->GetNetDevice(interface);

    // Assuming the added IP is associated with the current node,
    // update the mapping for this IP to the corresponding output device
    m_nextHopToDeviceMap[addedIp] = device;*/
}

void CustomRoutingProtocol::NotifyRemoveAddress(uint32_t interface, Ipv4InterfaceAddress address) {
    //NS_LOG_INFO("Address " << address.GetLocal() << " removed from interface " << interface);
    // Remove the mapping for the removed IP address
    /*Ipv4Address removedIp = address.GetLocal();
    m_nextHopToDeviceMap.erase(removedIp);*/
}

void CustomRoutingProtocol::PrintRoutingTable(Ptr<OutputStreamWrapper> stream, Time::Unit unit) const {
    *stream->GetStream() << "CustomRoutingProtocol routing table:\n";
    for (const auto& entry : m_switchingTable.ip_routing_table) {
        *stream->GetStream() << "  Destination: " << entry.first << ", Next Hop: " << entry.second << "\n";
    }
    // Dummy implementation
}
void CustomRoutingProtocol::SetNextHopToDeviceMap(leo::IpAssignmentHelper& ipAssignmentHelper) {
    // Retrieve the current node
    Ptr<Node> node = m_ipv4->GetObject<Node>();
    if (!node) {
        NS_LOG_WARN("Failed to retrieve the node associated with the Ipv4 object.");
        return;
    }

    // Retrieve the ConstellationNodeData object
    Ptr<leo::ConstellationNodeData> data = node->GetObject<leo::ConstellationNodeData>();
    if (!data) {
        NS_LOG_WARN("Failed to retrieve ConstellationNodeData for the node.");
        return;
    }

    // Retrieve the sourceId from ConstellationNodeData
    std::string sourceId = data->GetSourceId();
    //NS_LOG_INFO("Setting next hop to device map for sourceId: " << sourceId);

    // Iterate over the IP mappings for the current sourceId
    auto ipMappings = ipAssignmentHelper.GetIpMappingsForSource(sourceId);
    for (const auto& [targetId, ipPair] : ipMappings) {
        Ipv4Address sourceIp = ipPair.first;
        Ipv4Address targetIp = ipPair.second;

        // Find the interface for the source IP
        int32_t interface = m_ipv4->GetInterfaceForAddress(sourceIp);
        if (interface >= 0) {
            Ptr<NetDevice> device = m_ipv4->GetNetDevice(interface);
            m_nextHopToDeviceMap[targetIp] = device; // Map the target IP to the output device
            //NS_LOG_INFO("Mapping added: Target IP " << targetIp << " -> Device " << device);
        } else {
            NS_LOG_WARN("No interface found for source IP: " << sourceIp);
        }
    }
}
void CustomRoutingProtocol::AddNextHop(Ipv4Address nextHop, uint32_t interface) {
    //NS_LOG_INFO("Adding next hop: " << nextHop << " on interface " << interface);

    // Get the NetDevice for the interface
    Ptr<NetDevice> device = m_ipv4->GetNetDevice(interface);

    // Update the mapping
    m_nextHopToDeviceMap[nextHop] = device;

    // NS_LOG_INFO("Mapping added: Next Hop " << nextHop << " -> Device " << device);
}

void CustomRoutingProtocol::SetIpv4(Ptr<Ipv4> ipv4) {
    m_ipv4 = ipv4;
}

} // namespace leo
} // namespace ns3