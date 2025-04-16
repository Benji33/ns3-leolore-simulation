#include "custom-ipv4-l3-protocol.h"
#include "ns3/log.h"
#include "ns3/ipv4-route.h"
#include "ns3/inet-socket-address.h"
#include "ns3/constellation-node-data.h"

namespace ns3 {
namespace leo {

NS_LOG_COMPONENT_DEFINE("CustomRoutingProtocol");

TypeId CustomRoutingProtocol::GetTypeId() {
    static TypeId tid = TypeId("ns3::leo::CustomRoutingProtocol")
        .SetParent<Ipv4RoutingProtocol>()
        .SetGroupName("Internet");

    return tid;
}

CustomRoutingProtocol::CustomRoutingProtocol(leo::NetworkState &networkState, Ptr<Node> node)
: m_node(node), m_networkState(networkState) {}

CustomRoutingProtocol::~CustomRoutingProtocol() {}

void CustomRoutingProtocol::SetSwitchingTable(const SwitchingTable& table) {
    m_switchingTable = table;
}

Ptr<Ipv4Route> CustomRoutingProtocol::RouteOutput(Ptr<Packet> packet, const Ipv4Header& header,
                                                   Ptr<NetDevice> oif, Socket::SocketErrno& sockerr) {
    Ipv4Address dst = header.GetDestination();
    NS_LOG_INFO("RouteOutput called for destination: " << dst);

    // Resolve the destination node ID from the IP address
    std::string destNodeId = m_networkState.GetNodeIdForIp(dst);
    if (destNodeId=="") {
        NS_LOG_WARN("No node ID found for destination IP: " << dst);
        sockerr = Socket::ERROR_NOROUTETOHOST;
        return nullptr;
    }

    // Look up the next hop node ID in the switching table
    auto it = m_switchingTable.routing_table.find(destNodeId);
    if (it == m_switchingTable.routing_table.end()) {
        NS_LOG_WARN("No route found for destination node: " << destNodeId);
        sockerr = Socket::ERROR_NOROUTETOHOST;
        return nullptr;
    }
    std::string currentNodeId = m_node->GetObject<leo::ConstellationNodeData>()->GetSourceId();
    std::string nextHopNodeId = it->second;

    // Check if link is active
    if (!m_networkState.IsLinkActive(currentNodeId, nextHopNodeId)) {
        NS_LOG_WARN("Link between " << currentNodeId << " and " << nextHopNodeId << " is inactive.");
        sockerr = Socket::ERROR_NOROUTETOHOST;
        return nullptr;
    }

    // Gets the outgoing device of this node and the incoming device of the next hop node
    std::pair<Ptr<NetDevice>, Ptr<NetDevice>> link_devices = m_networkState.GetDevicesForNextHop(currentNodeId, nextHopNodeId);
    if (!link_devices.first) {
        NS_LOG_WARN("No valid device found on current node that is connecting to next hop node: " << nextHopNodeId);
        sockerr = Socket::ERROR_NOROUTETOHOST;
        return nullptr;
    }
    if (!link_devices.second) {
        NS_LOG_WARN("No valid device found on next node that this node connects to: " << nextHopNodeId);
        sockerr = Socket::ERROR_NOROUTETOHOST;
        return nullptr;
    }
    Ipv4Address nextHopDeviceIp = m_networkState.GetIpAddressForDevice(link_devices.second);
    if (nextHopDeviceIp == Ipv4Address()) {
        NS_LOG_ERROR("GetIpAddressForDevice returned an invalid IP address for device: " << link_devices.second);
        return nullptr;
    }
    NS_LOG_INFO("Next hop IP for destination " << destNodeId << ": " << nextHopDeviceIp);
    // Create the route
    Ptr<Ipv4Route> route = Create<Ipv4Route>();
    route->SetDestination(dst);
    route->SetGateway(nextHopDeviceIp);
    route->SetSource(header.GetSource());

    int32_t interface = m_ipv4->GetInterfaceForDevice(link_devices.first);
        if (interface >= 0) {
            route->SetOutputDevice(m_ipv4->GetNetDevice(interface));
        } else {
            NS_LOG_WARN("No interface found for next hop: " << nextHopNodeId);
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

bool CustomRoutingProtocol::RouteInput(Ptr<const Packet> packet, const Ipv4Header& header,
                                        Ptr<const NetDevice> idev, // NetDevice where packet arrived
                                        const Ipv4RoutingProtocol::UnicastForwardCallback& ucb,
                                        const Ipv4RoutingProtocol::MulticastForwardCallback& mcb,
                                        const Ipv4RoutingProtocol::LocalDeliverCallback& lcb,
                                        const Ipv4RoutingProtocol::ErrorCallback& ecb)
{
    Ipv4Address dst = header.GetDestination();
    NS_LOG_INFO("RouteInput called for destination: " << dst);

    // Get the incoming interface
    uint32_t incoming_interface = m_ipv4->GetInterfaceForDevice(idev);
    if (incoming_interface == uint32_t(-1)) { // Check if the interface is invalid
        NS_LOG_WARN("No interface found for the arriving NetDevice.");
        return false;
    }
    Ipv4Address incomingIp = m_ipv4->GetAddress(incoming_interface, 0).GetLocal();
    NS_LOG_INFO("Packet with destination " << dst << " received on interface " << incoming_interface
                                           << " with IP address: " << incomingIp);

    // Check if the destination is this node
    if (m_ipv4->IsDestinationAddress(dst, incoming_interface)) {
        NS_LOG_INFO("Packet is for this node, delivering to application layer...");
        lcb(packet, header, incoming_interface); // Deliver the packet locally
        return true;
    }

    // Resolve the destination node ID from the IP address
    std::string destNodeId = m_networkState.GetNodeIdForIp(dst);
    NS_LOG_INFO("Destination node ID for " << dst << ": " << destNodeId);
    if (destNodeId == "") {
        NS_LOG_WARN("No node ID found for destination IP: " << dst);
        ecb(packet, header, Socket::ERROR_NOROUTETOHOST);
        return false;
    }

    // Look up the next hop node ID in the switching table
    auto it = m_switchingTable.routing_table.find(destNodeId);
    if (it == m_switchingTable.routing_table.end()) {
        NS_LOG_WARN("No route found for destination node: " << destNodeId);
        ecb(packet, header, Socket::ERROR_NOROUTETOHOST);
        return false;
    }
    std::string currentNodeId = m_node->GetObject<leo::ConstellationNodeData>()->GetSourceId();
    std::string nextHopNodeId = it->second;

    if (!m_networkState.IsLinkActive(currentNodeId, nextHopNodeId)) {
        NS_LOG_WARN("Link between " << currentNodeId << " and " << nextHopNodeId << " is inactive.");
        ecb(packet, header, Socket::ERROR_NOROUTETOHOST);
        return false;
    }
    NS_LOG_INFO("Next hop node ID for destination " << destNodeId << ": " << nextHopNodeId);

    // Resolve the next hop IP address and NetDevice
    std::pair<Ptr<NetDevice>, Ptr<NetDevice>> link_devices = m_networkState.GetDevicesForNextHop(currentNodeId, nextHopNodeId);
    if (!link_devices.first) {
        NS_LOG_WARN("No valid device found on current node that is connecting to next hop node: " << nextHopNodeId);
        ecb(packet, header, Socket::ERROR_NOROUTETOHOST);
        return false;
    }
    if (!link_devices.second) {
        NS_LOG_WARN("No valid device found on next node that this node connects to: " << nextHopNodeId);
        ecb(packet, header, Socket::ERROR_NOROUTETOHOST);
        return false;
    }
    Ipv4Address nextHopDeviceIp = m_networkState.GetIpAddressForDevice(link_devices.second);
    if (nextHopDeviceIp == Ipv4Address()) {
        NS_LOG_ERROR("GetIpAddressForDevice returned an invalid IP address for device: " << link_devices.second);
        ecb(packet, header, Socket::ERROR_NOROUTETOHOST);
        return false;
    }
    NS_LOG_INFO("Next hop IP for destination " << destNodeId << ": " << nextHopDeviceIp);

    // Create the route
    Ptr<Ipv4Route> route = Create<Ipv4Route>();
    route->SetDestination(dst);
    route->SetGateway(nextHopDeviceIp);
    route->SetSource(m_ipv4->GetAddress(incoming_interface, 0).GetLocal());

    int32_t interface = m_ipv4->GetInterfaceForDevice(link_devices.first);
    if (interface >= 0) {
        route->SetOutputDevice(m_ipv4->GetNetDevice(interface));
    } else {
        NS_LOG_WARN("No interface found for next hop: " << nextHopNodeId);
        NS_LOG_INFO("Listing all interfaces and their IP addresses for this node:");
        for (uint32_t i = 0; i < m_ipv4->GetNInterfaces(); ++i) {
            for (uint32_t j = 0; j < m_ipv4->GetNAddresses(i); ++j) {
                NS_LOG_INFO("  Interface " << i << ", Address " << j << ": " << m_ipv4->GetAddress(i, j).GetLocal());
            }
        }
        ecb(packet, header, Socket::ERROR_NOROUTETOHOST);
        return false;
    }

    NS_LOG_INFO("Forwarding packet to: " << nextHopDeviceIp);
    // Forward the packet using the UnicastForwardCallback
    ucb(route, packet, header);
    return true;
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
    for (const auto& entry : m_switchingTable.routing_table) {
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