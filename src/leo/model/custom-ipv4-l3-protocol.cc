#include "custom-ipv4-l3-protocol.h"
#include "ns3/log.h"
#include "ns3/ipv4-route.h"
#include "ns3/inet-socket-address.h"
#include "ns3/constellation-node-data.h"
#include "ns3/custom-on-off-application.h"

namespace ns3 {
namespace leo {

NS_LOG_COMPONENT_DEFINE("CustomRoutingProtocol");

TypeId CustomRoutingProtocol::GetTypeId() {
    static TypeId tid = TypeId("ns3::leo::CustomRoutingProtocol")
        .SetParent<Ipv4RoutingProtocol>()
        .SetGroupName("Internet");

    return tid;
}

CustomRoutingProtocol::CustomRoutingProtocol(Ptr<Node> node, leo::TrafficManager &trafficManager)
: m_node(node), m_networkState(NetworkState::GetInstance()), m_trafficManager(trafficManager) {}

CustomRoutingProtocol::~CustomRoutingProtocol() {}

void CustomRoutingProtocol::SetSwitchingTables(const std::vector<std::reference_wrapper<const SwitchingTable>>& tables) {
    m_switchingTables = tables;
    NS_LOG_DEBUG("Switching tables set. Total tables: " << m_switchingTables.size());
}
const std::vector<std::reference_wrapper<const SwitchingTable>>& CustomRoutingProtocol::GetSwitchingTables() const {
    return m_switchingTables;
}

const SwitchingTable* CustomRoutingProtocol::GetCurrentValidSwitchingTable(Time currentTime) {
    // Check if the currently valid table is still valid
    if (m_currentValidSwitchingTable != nullptr) {
        if (currentTime >= m_currentValidSwitchingTable->valid_from &&
            currentTime <= m_currentValidSwitchingTable->valid_until) {
            return m_currentValidSwitchingTable;
        }
    }

    // Reset the current valid table
    m_currentValidSwitchingTable = nullptr;

    // Variables to track the closest tables for debugging
    const SwitchingTable* closestBefore = nullptr;
    const SwitchingTable* closestAfter = nullptr;

    // Iterate through the sorted switching tables
    for (const auto& tableRef : m_switchingTables) {
        const SwitchingTable& table = tableRef.get();

        // Log the validity times of each table
        NS_LOG_DEBUG("Checking table: valid_from = " << table.valid_from.GetSeconds()
                      << ", valid_until = " << table.valid_until.GetSeconds());

        // Check if the current time falls within the validity range
        if (currentTime >= table.valid_from && currentTime <= table.valid_until) {
            m_currentValidSwitchingTable = &table;
            NS_LOG_DEBUG("Updated current valid switching table: valid_from = " << table.valid_from.GetSeconds()
                         << ", valid_until = " << table.valid_until.GetSeconds());
            return m_currentValidSwitchingTable;
        }

        // Track the closest tables for debugging
        if (table.valid_until < currentTime) {
            if (closestBefore == nullptr || table.valid_until > closestBefore->valid_until) {
                closestBefore = &table;
            }
        } else if (table.valid_from > currentTime) {
            if (closestAfter == nullptr || table.valid_from < closestAfter->valid_from) {
                closestAfter = &table;
            }
        }
    }

    // Log a warning if no valid table is found
    NS_LOG_WARN("No valid switching table found for current time: " << currentTime.GetSeconds());

    // Log the closest tables for debugging
    if (closestBefore) {
        NS_LOG_WARN("Closest table before current time: valid_from = " << closestBefore->valid_from.GetSeconds()
                     << ", valid_until = " << closestBefore->valid_until.GetSeconds());
    } else {
        NS_LOG_WARN("No table found with valid_until before current time.");
    }

    if (closestAfter) {
        NS_LOG_WARN("Closest table after current time: valid_from = " << closestAfter->valid_from.GetSeconds()
                     << ", valid_until = " << closestAfter->valid_until.GetSeconds());
    } else {
        NS_LOG_WARN("No table found with valid_from after current time.");
    }

    return nullptr;
}

Ptr<Ipv4Route> CustomRoutingProtocol::RouteOutput(Ptr<Packet> packet, const Ipv4Header& header,
    Ptr<NetDevice> oif, Socket::SocketErrno& sockerr) {
        Ipv4Address dst = header.GetDestination();
        m_trafficManager.IncreasePacketSentProxy(header);

    NS_LOG_DEBUG("----------> RouteOutput called for destination: " << dst);
    leo::PacketIdTag tag;
    if (packet->PeekPacketTag(tag)) {
        NS_LOG_DEBUG("Packet Tag ID: " << tag.GetId());
    } else {
        NS_LOG_WARN("No PacketIdTag found on the packet.");
    }
    // Resolve the destination node ID from the IP address
    std::string destNodeId = m_networkState.GetNodeIdForIp(dst);
    if (destNodeId=="") {
        NS_LOG_WARN("No node ID found for destination IP: " << dst);
        sockerr = Socket::ERROR_NOROUTETOHOST;
        return nullptr;
    }

    const SwitchingTable* currentTable = GetCurrentValidSwitchingTable(Simulator::Now());
    // Assume there is always a valid switching table for now
    if (currentTable == nullptr) {
        NS_LOG_ERROR("No valid switching table found for the current time.");
        sockerr = Socket::ERROR_NOROUTETOHOST;
        return nullptr;
    }

    auto nodeData = m_node->GetObject<leo::ConstellationNodeData>();
    if (!nodeData) {
        NS_LOG_ERROR("ConstellationNodeData not found on node");
        sockerr = Socket::ERROR_NOROUTETOHOST;
        return nullptr;
    }
    std::string currentNodeId = nodeData->GetSourceId();

    // Look up the next hop node ID in the switching table
    auto it = currentTable->routing_table.find(destNodeId);
    if (it == currentTable->routing_table.end()) {
        NS_LOG_WARN("No route found at "<< Simulator::Now().GetSeconds() << " from current node " << currentNodeId << " for destination node: " << destNodeId);
        sockerr = Socket::ERROR_NOROUTETOHOST;
        return nullptr;
    }
    std::string nextHopNodeId = it->second;

    // Check if link is active
    if (!m_networkState.IsLinkActive(currentNodeId, nextHopNodeId)) {
        NS_LOG_WARN("Link between " << currentNodeId << " and " << nextHopNodeId << " is inactive at " << Simulator::Now().GetSeconds());
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
    NS_LOG_DEBUG("Next hop IP for destination " << destNodeId << ": " << nextHopDeviceIp);

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
    NS_LOG_DEBUG("----------> RouteInput called for destination: " << dst);

    // Get the incoming interface
    uint32_t incoming_interface = m_ipv4->GetInterfaceForDevice(idev);
    if (incoming_interface == uint32_t(-1)) { // Check if the interface is invalid
        NS_LOG_WARN("No interface found for the arriving NetDevice.");
        return false;
    }
    Ipv4Address incomingIp = m_ipv4->GetAddress(incoming_interface, 0).GetLocal();
    NS_LOG_DEBUG("Packet with destination " << dst << " received on interface " << incoming_interface
                                         << " with IP address: " << incomingIp);


    //print all ip addresses of this node
    /*for (uint32_t i = 0; i < m_ipv4->GetNInterfaces(); ++i) {
        for (uint32_t j = 0; j < m_ipv4->GetNAddresses(i); ++j) {
            Ipv4Address localAddr = m_ipv4->GetAddress(i, j).GetLocal();
            std::cout << localAddr << " ";
        }
    }*/
    // Check if the destination is any of the ip addresses of this node
    for (uint32_t i = 0; i < m_ipv4->GetNInterfaces(); ++i) {
        for (uint32_t j = 0; j < m_ipv4->GetNAddresses(i); ++j) {
            Ipv4Address localAddr = m_ipv4->GetAddress(i, j).GetLocal();
            if (dst == localAddr) {
                NS_LOG_DEBUG("Packet is for this node (interface " << i << "), delivering to application layer...");
                m_trafficManager.IncreasePacketReceivedProxy(header);
                lcb(packet, header, i);
                return true;
            }
        }
    }
    // Resolve the destination node ID from the IP address
    std::string destNodeId = m_networkState.GetNodeIdForIp(dst);
    NS_LOG_DEBUG("Destination node ID for " << dst << ": " << destNodeId);
    if (destNodeId == "") {
        NS_LOG_WARN("No node ID found for destination IP: " << dst);
        ecb(packet, header, Socket::ERROR_NOROUTETOHOST);
        return false;
    }
    const SwitchingTable* currentTable = GetCurrentValidSwitchingTable(Simulator::Now());
    // Assume there is always a valid switching table for now
    if (currentTable == nullptr) {
        NS_LOG_ERROR("No valid switching table found for the current time.");
        ecb(packet, header, Socket::ERROR_NOROUTETOHOST);
        return false;
    }

    auto nodeData = m_node->GetObject<leo::ConstellationNodeData>();
    if (!nodeData) {
        NS_LOG_ERROR("ConstellationNodeData not found on node");
        ecb(packet, header, Socket::ERROR_NOROUTETOHOST);
        return false;
    }
    std::string currentNodeId = nodeData->GetSourceId();

    // Look up the next hop node ID in the switching table
    auto it = currentTable->routing_table.find(destNodeId);
    if (it == currentTable->routing_table.end()) {
        NS_LOG_WARN("No route found at "<< Simulator::Now().GetSeconds() << " from current node " << currentNodeId << " for destination node: " << destNodeId);
        ecb(packet, header, Socket::ERROR_NOROUTETOHOST);
        return false;
    }

    std::string nextHopNodeId = it->second;

    if (!m_networkState.IsLinkActive(currentNodeId, nextHopNodeId)) {
        NS_LOG_WARN("Link between " << currentNodeId << " and " << nextHopNodeId << " is inactive at " << Simulator::Now().GetSeconds());
        ecb(packet, header, Socket::ERROR_NOROUTETOHOST);
        return false;
    }
    NS_LOG_DEBUG("Next hop node ID for destination " << destNodeId << ": " << nextHopNodeId);

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
    NS_LOG_DEBUG("Next hop IP for destination " << destNodeId << ": " << nextHopDeviceIp);

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
        NS_LOG_DEBUG("Listing all interfaces and their IP addresses for this node:");
        for (uint32_t i = 0; i < m_ipv4->GetNInterfaces(); ++i) {
            for (uint32_t j = 0; j < m_ipv4->GetNAddresses(i); ++j) {
                NS_LOG_INFO("  Interface " << i << ", Address " << j << ": " << m_ipv4->GetAddress(i, j).GetLocal());
            }
        }
        ecb(packet, header, Socket::ERROR_NOROUTETOHOST);
        return false;
    }

    NS_LOG_DEBUG("Forwarding packet to ----------> " << nextHopDeviceIp);
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
    /*for (const auto& entry : (GetCurrentValidSwitchingTable(Simulator::Now()))->routing_table) {
        *stream->GetStream() << "  Destination: " << entry.first << ", Next Hop: " << entry.second << "\n";
    }*/
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