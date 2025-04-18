#ifndef NETWORK_STATE_H
#define NETWORK_STATE_H

#include <map>
#include <string>
#include <vector>
#include "ns3/ipv4-interface-container.h"
#include "ns3/node-container.h"
#include "ns3/ipv4-address-hash.h"
#include "ns3/channel.h"

namespace ns3 {
namespace leo {

struct LinkInfo {
    Ptr<NetDevice> deviceA;
    Ptr<NetDevice> deviceB;
    Ptr<Channel> channel;
    Ipv4Address ipA;
    Ipv4Address ipB;
    bool isActive = false;

    LinkInfo()
    : deviceA(nullptr), deviceB(nullptr), channel(nullptr), ipA(), ipB(), isActive(false) {}

    LinkInfo(Ptr<NetDevice> devA, Ptr<NetDevice> devB, Ptr<Channel> ch, Ipv4Address ipA, Ipv4Address ipB, bool active)
    : deviceA(devA), deviceB(devB), channel(ch), ipA(ipA), ipB(ipB), isActive(active) {}

    bool IsValid() const {
        return deviceA != nullptr && deviceB != nullptr && channel != nullptr;
    }
};

class NetworkState {
public:
    NetworkState() = default;
    void RegisterNode(Ptr<Node> networkNode, uint32_t ns3NodeId, const std::string& sourceId, bool isSatellite);
    void RegisterInterfaces(uint32_t ns3NodeId, const std::string& sourceId, Ipv4InterfaceContainer interfaces);
    void RegisterLink(std::string srcId, std::string dstId, Ptr<NetDevice> deviceA, Ptr<NetDevice> deviceB, Ptr<Channel> channel,
        Ipv4Address ipA, Ipv4Address ipB);
    NodeContainer GetNodes() const { return m_nodes; }
    NodeContainer GetGroundStations(){ return m_groundStations;}
    NodeContainer GetSatellites(){ return m_satellites;}
    std::map<std::string, uint32_t> GetSourceIdToNs3Id() const { return m_sourceIdToNs3Id; }
    std::map<uint32_t, std::string> GetNs3IdToSourceId() const { return m_ns3IdToSourceId; }
    Ipv4InterfaceContainer GetInterfaces(uint32_t ns3NodeId) const;
    Ipv4Address GetIpAddressForDevice(Ptr<NetDevice> device) const;
    uint32_t GetNs3NodeId(const std::string& sourceId) const;
    std::string GetSourceId(uint32_t ns3NodeId) const;
    Ptr<Node> GetNodeBySourceId(std::string sourceId) const;
    LinkInfo GetLinkInfo(std::string srcId, std::string dstId) const;
    std::pair<Ptr<NetDevice>, Ptr<NetDevice>> GetDevicesForNextHop(const std::string& currentNodeId, const std::string& nextHopNodeId) const;
    std::string GetNodeIdForIp(const Ipv4Address& ip) const;
    std::unordered_map<Ipv4Address, std::string> GetIpToNodeIdMap() const { return m_ipToNodeIdMap; }
    std::vector<std::pair<std::string, std::string>> GetActiveLinks() const;

    void EnableLink(std::string srcId, std::string dstId, double weight);
    void DisableLink(std::string srcId, std::string dstId);
    bool IsLinkActive(std::string srcId, std::string dstId) const;
    void DisableNode(uint32_t ns3NodeId);
    void SetLinkDelay(uint32_t srcId, uint32_t dstId, Time delay);
    bool LinkDownCallback(Ptr<NetDevice> device, Ptr<const Packet>, uint16_t, const Address &);

private:
    NodeContainer m_nodes;
    NodeContainer m_groundStations;
    NodeContainer m_satellites;
    std::map<std::string, uint32_t> m_sourceIdToNs3Id;
    std::map<uint32_t, std::string> m_ns3IdToSourceId;
    std::map<uint32_t, Ipv4InterfaceContainer> m_nodeInterfaces;
    std::map<Ptr<NetDevice>, Ipv4Address> m_deviceToIpMap;
    std::unordered_map<Ipv4Address, std::string> m_ipToNodeIdMap;
    //std::map<uint32_t, std::vector<ns3::Ipv4Address>> m_ipInterfaces;
    std::map<std::pair<std::string, std::string>, LinkInfo> m_links;


};

} // namespace leo
} // namespace ns3

#endif // NETWORK_STATE_H
