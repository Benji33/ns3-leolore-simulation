#include "traffic-manager.h"
#include "ns3/log.h"
#include "ns3/ipv4.h"
#include "ns3/inet-socket-address.h"
#include "ns3/on-off-helper.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/string.h"
#include "ns3/uinteger.h"
#include "ns3/onoff-application.h"
#include "ns3/packet-sink.h"
#include "ns3/ipv4-header.h"
#include "ns3/custom-on-off-application.h"

namespace ns3 {
namespace leo {

NS_LOG_COMPONENT_DEFINE("TrafficManager");

TrafficManager::TrafficManager(const std::vector<Traffic>& trafficVector, NetworkState& networkState)
    : m_trafficVector(trafficVector), m_networkState(networkState) {}

void TrafficManager::PrintTrafficSummary() const {
    NS_LOG_UNCOND("Traffic Summary:");
    for (const auto& [key, stats] : m_trafficStats) {
        NS_LOG_UNCOND("From: " << key.first << " To: " << key.second
                       << " Sent: " << stats.packetsSent
                       << " Received: " << stats.packetsReceived);
    }
}

void TrafficManager::ScheduleTraffic() {
    int id_counter = 0;
    for (const auto& traffic : m_trafficVector) {
        Simulator::Schedule(Seconds(traffic.start_time), &TrafficManager::ScheduleTrafficEvent, this, traffic, id_counter);
        id_counter++;
    }
    // Schedule the traffic summary to be printed after the simulation ends
    Simulator::ScheduleDestroy(&TrafficManager::PrintTrafficSummary, this);
}

void TrafficManager::ScheduleTrafficEvent(const Traffic& traffic, int counter) {
    // Retrieve source and destination nodes
    Ptr<Node> srcNode = m_networkState.GetNodeBySourceId(traffic.src_node_id);
    Ptr<Node> dstNode = m_networkState.GetNodeBySourceId(traffic.dst_node_id);

    if (!srcNode || !dstNode) {
        NS_LOG_ERROR("Invalid source or destination node for traffic: " << traffic.src_node_id << " → " << traffic.dst_node_id);
        return;
    }

    // Retrieve the destination IP address
    Ptr<Ipv4> ipv4 = dstNode->GetObject<Ipv4>();
    Ipv4Address dstAddress = Ipv4Address::GetAny();
    for (uint32_t i = 0; i < ipv4->GetNInterfaces(); ++i) {
        for (uint32_t j = 0; j < ipv4->GetNAddresses(i); ++j) {
            Ipv4Address addr = ipv4->GetAddress(i, j).GetLocal();
            if (addr != Ipv4Address::GetLoopback()) {
                dstAddress = addr;
                break;
            }
        }
        if (dstAddress != Ipv4Address::GetAny()) {
            break;
        }
    }

    // Get source address from srcNode (similar to how you do dstAddress)
    Ptr<Ipv4> srcIpv4 = srcNode->GetObject<Ipv4>();
    Ipv4Address srcAddress = Ipv4Address::GetAny();
    for (uint32_t i = 0; i < srcIpv4->GetNInterfaces(); ++i) {
        for (uint32_t j = 0; j < srcIpv4->GetNAddresses(i); ++j) {
            Ipv4Address addr = srcIpv4->GetAddress(i, j).GetLocal();
            if (addr != Ipv4Address::GetLoopback()) {
                srcAddress = addr;
                break;
            }
        }
        if (srcAddress != Ipv4Address::GetAny()) {
            break;
        }
    }
    std::pair<Ipv4Address, Ipv4Address> flowKey = std::make_pair(srcAddress, dstAddress);
    NS_LOG_INFO("Scheduling traffic from " << srcAddress << " to " << dstAddress);
    m_trafficStats[flowKey] = TrafficStats{};
    // Configure custom OnOff application (instead of using OnOffHelper)
    Ptr<CustomOnOffApplication> customApp = CreateObject<CustomOnOffApplication>();
    customApp->Setup(srcNode, dstAddress, traffic.src_port, traffic.dst_port, traffic.packet_size, traffic.rate,
                     traffic.duration, counter);

    // Install the custom OnOff application on the source node
    customApp->SetStartTime(Seconds(0));
    customApp->SetStopTime(Seconds(traffic.duration));
    customApp->SetupReceiver(dstNode, traffic.dst_port);
    srcNode->AddApplication(customApp);

}

void TrafficManager::IncreasePacketSentProxy(Ipv4Header ipv4Header) {
    //NS_LOG_INFO("Packet sent");

    Ipv4Address srcAddress = ipv4Header.GetSource();
    Ipv4Address dstAddress = ipv4Header.GetDestination();
    NS_LOG_DEBUG("Packet sent from " << srcAddress << " to " << dstAddress);
    // Create a key using the source and destination addresses
    std::pair<Ipv4Address,Ipv4Address> key = std::make_pair(srcAddress, dstAddress);

    auto it = m_trafficStats.find(key);
    if (it != m_trafficStats.end()) {
        m_trafficStats[key].packetsSent++;
    }

}

void TrafficManager::IncreasePacketReceivedProxy(Ipv4Header ipv4Header) {
    Ipv4Address srcAddress = ipv4Header.GetSource();
    Ipv4Address dstAddress = ipv4Header.GetDestination();
    NS_LOG_DEBUG("Packet received " << srcAddress << " -> " << dstAddress);

    // Create a key using the source and destination addresses
    std::pair<Ipv4Address,Ipv4Address> key = std::make_pair(srcAddress, dstAddress);

    auto it = m_trafficStats.find(key);
    if (it != m_trafficStats.end()) {
        m_trafficStats[key].packetsReceived++;
    }
}

} // namespace leo
} // namespace ns3