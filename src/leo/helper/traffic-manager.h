#ifndef TRAFFIC_MANAGER_H
#define TRAFFIC_MANAGER_H

#include "ns3/network-state.h"
#include "ns3/application-container.h"
#include "ns3/on-off-helper.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/simulator.h"
#include "ns3/file-reader.h"
#include "ns3/custom-on-off-application.h"
#include <vector>

namespace ns3 {
namespace leo {

using Traffic = FileReader::Traffic;


class TrafficManager {

    struct TrafficStats {
        uint32_t packetsSent = 0;
        uint32_t packetsReceived = 0;
        std::map<std::string, uint16_t> packetsActivelyDroppedOnNode = {};
        double minLatency = 9999.99;
        double maxLatency = 0.0;
        double totalLatency = 0.0;
        std::map<uint64_t, uint32_t> packet_hops = {};
        u_int16_t minHopCount = 9999;
        u_int16_t maxHopCount = 0;

    };

public:
    TrafficManager(const std::vector<Traffic>& trafficVector);
    void PrintTrafficSummary() const;
    void ScheduleTraffic();
    void IncreasePacketSentProxy(Ipv4Header ipv4Header, leo::PacketIdTag tag);
    void IncreasePacketReceivedProxy(Ipv4Header ipv4Header, leo::PacketIdTag tag);
    void IncreasePacketHopCountProxy(Ipv4Header ipv4Header, leo::PacketIdTag tag);
    void IncreaseActivelyDroppedPacketProxy(Ipv4Header ipv4Header, int appId, std::string nodeId);
private:
    void ScheduleTrafficEvent(const Traffic& traffic, int counter);

    const std::vector<Traffic>& m_trafficVector;
    NetworkState& m_networkState;
    std::map<std::pair<int32_t,std::pair<Ipv4Address,Ipv4Address>>, TrafficStats> m_trafficStats;

};
} // namespace leo
} // namespace ns3

#endif // TRAFFIC_MANAGER_H