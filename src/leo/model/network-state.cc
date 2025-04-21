#include "network-state.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"
#include "ns3/callback.h"
#include "ns3/string.h"
#include "ns3/ipv4-l3-protocol.h"

namespace ns3 {
namespace leo {

NS_LOG_COMPONENT_DEFINE("NetworkState");

constexpr double speedOfLight = 299792.4580;

void NetworkState::RegisterNode(Ptr<Node> networkNode, uint32_t ns3NodeId, const std::string& sourceId, bool isSatellite) {
    m_nodes.Add(networkNode);
    if (isSatellite) {
        m_satellites.Add(networkNode);
    } else {
        m_groundStations.Add(networkNode);
    }
    m_sourceIdToNs3Id[sourceId] = ns3NodeId;
    m_ns3IdToSourceId[ns3NodeId] = sourceId;
}

void NetworkState::RegisterInterfaces(uint32_t ns3NodeId, const std::string& sourceId, Ipv4InterfaceContainer interfaces) {
    m_nodeInterfaces[ns3NodeId] = interfaces;
}

void NetworkState::RegisterLink(std::string srcId, std::string dstId, Ptr<NetDevice> deviceA, Ptr<NetDevice> deviceB, Ptr<Channel> channel,
    Ipv4Address ipA, Ipv4Address ipB) {
    auto key = NormalizeKey(srcId, dstId);

    NS_LOG_INFO("Registering link: " << key.first << " ↔ " << key.second);
    m_links[key] = LinkInfo(deviceA, deviceB, channel, ipA, ipB, true);

    m_deviceToIpMap[deviceA] = ipA;
    m_deviceToIpMap[deviceB] = ipB;

    m_ipToNodeIdMap[ipA] = srcId;
    m_ipToNodeIdMap[ipB] = dstId;
}

Ipv4Address NetworkState::GetIpAddressForDevice(Ptr<NetDevice> device) const {
    //print m_deviceToIpMap
    auto it = m_deviceToIpMap.find(device);
    if (it != m_deviceToIpMap.end()) {
        return it->second;
    }
    NS_LOG_WARN("No IP address found for device: " << device);
    return Ipv4Address();
}

std::string NetworkState::GetNodeIdForIp(const Ipv4Address& ip) const {
    auto it = m_ipToNodeIdMap.find(ip);
    if (it != m_ipToNodeIdMap.end()) {
        return it->second;
    }
    return "";
}

Ipv4InterfaceContainer NetworkState::GetInterfaces(uint32_t ns3NodeId) const {
    return m_nodeInterfaces.at(ns3NodeId);
}

Ptr<Node> NetworkState::GetNodeBySourceId(std::string sourceId) const {
    auto it = m_sourceIdToNs3Id.find(sourceId);
    if (it != m_sourceIdToNs3Id.end()) {
        uint32_t ns3NodeId = it->second;
        return m_nodes.Get(ns3NodeId);
    }
    return nullptr;
}

uint32_t NetworkState::GetNs3NodeId(const std::string& sourceId) const {
    return m_sourceIdToNs3Id.at(sourceId);
}

std::string NetworkState::GetSourceId(uint32_t ns3NodeId) const {
    return m_ns3IdToSourceId.at(ns3NodeId);
}

std::vector<std::pair<std::string, std::string>> NetworkState::GetActiveLinks() const {
    std::vector<std::pair<std::string, std::string>> activeLinks;
    for (const auto& [pair, linkInfo] : m_links) {
        if (linkInfo.isActive) {
            activeLinks.push_back(pair);
        }
    }
    return activeLinks;
}

const LinkInfo& NetworkState::GetLinkInfo(const std::string& srcId, const std::string& dstId) const {
    auto key = NormalizeKey(srcId, dstId);
    auto it = m_links.find(key);
    if (it != m_links.end()) {
        return it->second;
    }

    // If the link is not found, throw an exception or return a static invalid LinkInfo
    static const LinkInfo invalidLinkInfo; // Default-constructed invalid LinkInfo
    return invalidLinkInfo;
}

std::pair<Ptr<NetDevice>, Ptr<NetDevice>> NetworkState::GetDevicesForNextHop(const std::string& currentNodeId, const std::string& nextHopNodeId) const {
    const LinkInfo& linkInfo = GetLinkInfo(currentNodeId, nextHopNodeId);
    //NS_LOG_INFO("GetDevicesForNextHop: currentNodeId: " << currentNodeId << ", nextHopNodeId: " << nextHopNodeId);

    if (linkInfo.IsValid()) {
        // Determine the correct devices based on the current node's role
        if (currentNodeId == GetNodeIdForIp(linkInfo.ipA)) {
            //NS_LOG_INFO("Current node is the source, returning deviceA (current node) and deviceB (next hop)");
            return {linkInfo.deviceA, linkInfo.deviceB}; // deviceA is on the current node, deviceB is on the next hop
        } else if (currentNodeId == GetNodeIdForIp(linkInfo.ipB)) {
            //NS_LOG_INFO("Current node is the destination, returning deviceB (current node) and deviceA (next hop)");
            return {linkInfo.deviceB, linkInfo.deviceA}; // deviceB is on the current node, deviceA is on the next hop
        } else {
            NS_LOG_WARN("Current node ID does not match either end of the link.");
        }
    }

    NS_LOG_ERROR("Invalid link or no devices found for currentNodeId: " << currentNodeId << " and nextHopNodeId: " << nextHopNodeId);
    return {nullptr, nullptr}; // Return null pointers if no valid devices are found
}

void NetworkState::EnableLink(std::string srcId, std::string dstId, double weight) {
    if (dstId =="632430d9e1131"){
        NS_LOG_DEBUG("Enabling link: " << srcId << " ↔ " << dstId);
    }
    auto key = NormalizeKey(srcId, dstId);

    auto it = m_links.find(key);
    if (it == m_links.end()) {
        NS_LOG_WARN("Tried to enable a link that was not pre-registered: " << key.first << " → " << key.second);
        return;
    }

    LinkInfo& link = it->second;

    double delayInSeconds = weight / speedOfLight;
    std::ostringstream delayStream;
    delayStream << (delayInSeconds * 1e3) << "ms";
    Ptr<PointToPointChannel> p2pChannel = DynamicCast<PointToPointChannel>(link.channel);
    if (p2pChannel) {
        p2pChannel->SetAttribute("Delay", StringValue(delayStream.str()));
    }

    link.isActive = true;
    NS_LOG_DEBUG("Link between " << srcId << " and " << dstId << " enabled at " << Simulator::Now().GetSeconds());

    }

void NetworkState::DisableLink(std::string srcId, std::string dstId) {
    auto key = NormalizeKey(srcId, dstId);
    auto it = m_links.find(key);
    if (it == m_links.end()) {
        NS_LOG_WARN("No link found between " << key.first << " and " << key.second);
        return;
    }

    LinkInfo& link = it->second;

    // Don't change the reeceive callback since its hard to undo. Just stick to link active or inactive
    //link.deviceA->SetReceiveCallback(MakeCallback(&NetworkState::LinkDownCallback, this));
    //link.deviceB->SetReceiveCallback(MakeCallback(&NetworkState::LinkDownCallback, this));

    link.isActive = false;
    NS_LOG_DEBUG("Link between " << srcId << " and " << dstId << " disabled at " << Simulator::Now().GetSeconds());
}

bool NetworkState::LinkDownCallback(Ptr<NetDevice> device, Ptr<const Packet>, uint16_t, const Address &) {
    NS_LOG_DEBUG("Packet received on disabled link, dropping.");
    return false;
}

bool NetworkState::IsLinkActive(std::string srcId, std::string dstId) const {
    /*if (dstId =="632430d9e1167" || srcId == "632430d9e1167"){
        NS_LOG_INFO("Checking link activity between: " << srcId << " ↔ " << dstId << " at " << Simulator::Now().GetSeconds());
    }*/
    auto key = NormalizeKey(srcId, dstId);
    auto it = m_links.find(key);
    if (it != m_links.end()) {
        //NS_LOG_DEBUG("Checking link activity between: " << key.first << " ↔ " << key.second << " at " << Simulator::Now().GetSeconds()
        //             << " - Active: " << it->second.isActive);
        return it->second.isActive;
    }

    NS_LOG_DEBUG("No link found between " << key.first << " and " << key.second);
    return false;
}

std::pair<std::string, std::string> NetworkState::NormalizeKey(const std::string& a, const std::string& b) const {
    return (a < b) ? std::make_pair(a, b) : std::make_pair(b, a);
}

void NetworkState::DisableNode(uint32_t ns3NodeId){
    return;
}
void NetworkState::SetLinkDelay(uint32_t srcId, uint32_t dstId, Time delay) {
    return;
}


} // namespace leo
} // namespace ns3
