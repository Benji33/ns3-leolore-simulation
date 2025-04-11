#include "custom-ipv4-l3-protocol.h"
#include "ns3/log.h"
#include "ns3/ipv4-route.h"
#include "ns3/inet-socket-address.h"

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

    // Look up the destination in the switching table
    auto it = m_switchingTable.ip_routing_table.find(dst);
    if (it != m_switchingTable.ip_routing_table.end()) {
        NS_LOG_INFO("Switching table hit: " << dst << " -> " << it->second);

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
    NS_LOG_INFO("RouteInput called for destination: " << dst);

    // Look up the destination in the switching table
    auto it = m_switchingTable.ip_routing_table.find(dst);
    if (it != m_switchingTable.ip_routing_table.end()) {
        NS_LOG_INFO("Switching table hit: " << dst << " -> " << it->second);

        Ptr<Ipv4Route> route = Create<Ipv4Route>();
        route->SetDestination(dst);
        route->SetGateway(it->second);
        route->SetSource(header.GetSource());

        // Check all interfaces on board the node which interface connects to the target address - returns index of the interface
        int32_t interface = m_ipv4->GetInterfaceForAddress(it->second);
        if (interface >= 0) {
            // use index to get the NetDevice
            route->SetOutputDevice(m_ipv4->GetNetDevice(interface));
        } else {
            NS_LOG_WARN("No interface found for next hop: " << it->second);
            return false;
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
    NS_LOG_INFO("Interface " << interface << " is up");
    // Dummy implementation: Add any initialization logic here if needed
}

void CustomRoutingProtocol::NotifyInterfaceDown(uint32_t interface) {
    NS_LOG_INFO("Interface " << interface << " is down");
    // Dummy implementation: Add any cleanup logic here if needed
}

void CustomRoutingProtocol::NotifyAddAddress(uint32_t interface, Ipv4InterfaceAddress address) {
    NS_LOG_INFO("Address " << address.GetLocal() << " added to interface " << interface);

    // Update the map with the new IP-to-device mapping
    Ipv4Address addedIp = address.GetLocal();
    Ptr<NetDevice> device = m_ipv4->GetNetDevice(interface);

    // Assuming the added IP is associated with the current node,
    // update the mapping for this IP to the corresponding output device
    m_nextHopToDeviceMap[addedIp] = device;
}

void CustomRoutingProtocol::NotifyRemoveAddress(uint32_t interface, Ipv4InterfaceAddress address) {
    NS_LOG_INFO("Address " << address.GetLocal() << " removed from interface " << interface);
    // Remove the mapping for the removed IP address
    Ipv4Address removedIp = address.GetLocal();
    m_nextHopToDeviceMap.erase(removedIp);
}

void CustomRoutingProtocol::PrintRoutingTable(Ptr<OutputStreamWrapper> stream, Time::Unit unit) const {
    *stream->GetStream() << "CustomRoutingProtocol routing table:\n";
    for (const auto& entry : m_switchingTable.ip_routing_table) {
        *stream->GetStream() << "  Destination: " << entry.first << ", Next Hop: " << entry.second << "\n";
    }
    // Dummy implementation: Add more details if needed
}
void CustomRoutingProtocol::SetIpv4(Ptr<Ipv4> ipv4) {
    m_ipv4 = ipv4;
}

} // namespace leo
} // namespace ns3