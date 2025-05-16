#include "traffic-manager.h"
#include "ns3/log.h"
#include "ns3/ipv4.h"
#include "ns3/inet-socket-address.h"
#include "ns3/on-off-helper.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/string.h"
#include "ns3/uinteger.h"
#include <fstream>
#include <numeric>
#include "ns3/onoff-application.h"
#include "ns3/packet-sink.h"
#include "ns3/ipv4-header.h"
#include "ns3/constellation-node-data.h"

namespace ns3 {
namespace leo {

NS_LOG_COMPONENT_DEFINE("TrafficManager");

TrafficManager::TrafficManager(const std::vector<Traffic>& trafficVector, NetworkState& networkState)
    : m_trafficVector(trafficVector), m_networkState(networkState) {}

void TrafficManager::WriteTrafficSummaryToCsv(const std::string& filePath) const {
    std::ofstream outFile(filePath);

    if (!outFile.is_open()) {
        NS_LOG_ERROR("Failed to open file: " << filePath);
        return;
    }

    // Write the CSV column headers
    outFile << "AppId,Source IP,Source Node,Source Town,Destination IP,Destination Node,Destination Town,Packets Sent,Packets Received,"
            << "Min Latency (ms),Max Latency (ms),Avg Latency (ms),Avg Hop Count,Dropped Packets,Dropped Locations,Backup Path Used,Loop Avoidance Triggered\n";

    for (const auto& [key, stats] : m_trafficStats) {
        std::ostringstream oss;

        // Extract source and destination information
        const auto& appId = key.first;
        const auto& srcIp = key.second.first;
        const auto& dstIp = key.second.second;
        const auto& srcId = m_networkState.GetNodeIdForIp(key.second.first);
        const auto& dstId = m_networkState.GetNodeIdForIp(key.second.second);
        std::string srcTown = "N/A";
        std::string dstTown = "N/A";

        Ptr<Node> srcNode = m_networkState.GetNodeBySourceId(srcId);
        Ptr<Node> dstNode = m_networkState.GetNodeBySourceId(dstId);

        if (srcNode) {
            Ptr<leo::ConstellationNodeData> srcData = srcNode->GetObject<leo::ConstellationNodeData>();
            if (srcData && srcData->GetType() == "ground_station") {
                srcTown = srcData->GetTown();
            }
        }

        if (dstNode) {
            Ptr<leo::ConstellationNodeData> dstData = dstNode->GetObject<leo::ConstellationNodeData>();
            if (dstData && dstData->GetType() == "ground_station") {
                dstTown = dstData->GetTown();
            }
        }
        const auto& sentPackets = stats.packetsSent;
        const auto& receivedPackets = stats.packetsReceived;
        double minLatency = stats.minLatency;
        if (minLatency == 9999.99) {
            minLatency = -1;
        }
        double maxLatency = stats.maxLatency;
        if (maxLatency == 0.0) {
            maxLatency = -1;
        }
        const auto& avgLatency = (stats.packetsReceived > 0) ? stats.totalLatency / stats.packetsReceived : -1;
        double avgHopCount = 0.0;
        if (!stats.packet_hops.empty()) {
            double totalHops = 0.0;
            for (const auto& hop : stats.packet_hops) {
                totalHops += hop.second; // hop.second contains the hop count for a packet
            }
            avgHopCount = totalHops / stats.packet_hops.size();
        }
        const auto& backupPathUsed = stats.backupPathUsed;
        const auto& loopAvoidanceTriggered = stats.loopAvoidanceTriggered;

        // Count total dropped packets
        uint64_t totalDroppedPackets = 0;
        std::ostringstream droppedLocationsStream;
        droppedLocationsStream << "{"; // Start the JSON-like format

        for (const auto& [nodeId, count] : stats.packetsActivelyDroppedOnNode) {
            totalDroppedPackets += count;
            droppedLocationsStream << nodeId << ":" << count << ",";
        }
        // delete trailing comma
        std::string droppedLocations = droppedLocationsStream.str();
        if (!droppedLocations.empty()) {
            droppedLocations.pop_back();
        }
        if(!droppedLocations.empty()) {
            droppedLocations = droppedLocations + "}";
        }

        // Write the row data
        outFile << appId << ',' << srcIp << ',' << srcId << ",\"" << srcTown << "\","
                << dstIp << ',' << dstId << ",\"" << dstTown << "\","
                << sentPackets << ',' << receivedPackets << ','
                << minLatency << ',' << maxLatency << ',' << avgLatency << ','
                << avgHopCount << ',' << totalDroppedPackets << ',\"' << droppedLocations << '\",'
                << backupPathUsed << ',' << loopAvoidanceTriggered << '\n';
    }

    outFile.close();
    NS_LOG_UNCOND("Traffic summary written to CSV: " << filePath);
}

void TrafficManager::PrintTrafficSummary(const std::string& outputFolder, const uint8_t run, bool writeToCsv, int failureScenario) const {

    NS_LOG_UNCOND("Traffic Summary for run " << static_cast<int>(run) << ":");

    uint64_t totalPacketsSent = 0;
    uint64_t totalPacketsReceived = 0;
    std::map<std::pair<int32_t,std::pair<Ipv4Address,Ipv4Address>>, TrafficStats> lost_packets_traffic;
    for (const auto& [key, stats] : m_trafficStats) {
        std::ostringstream oss;
        oss << "AppId: " << key.first
            << ", From: " << key.second.first << " (" << m_networkState.GetNodeIdForIp(key.second.first) << ")"
            << " To: " << key.second.second << " (" << m_networkState.GetNodeIdForIp(key.second.second) << ")"
            << ", Sent: " << stats.packetsSent
            << ", Received: " << stats.packetsReceived;

        if (stats.packetsReceived > 0) {
            double avgLatency = stats.totalLatency / stats.packetsReceived;
            oss << ", Min Latency: " << stats.minLatency << "ms"
                << ", Max Latency: " << stats.maxLatency << "ms"
                << ", Avg Latency: " << avgLatency << "ms";
        }

        for (const auto& [nodeId, count] : stats.packetsActivelyDroppedOnNode) {
            oss << ", Dropped " << count << " packets on node " << nodeId;
        }
        // average hop count
        if (stats.packet_hops.size() > 0) {
            double avgHopCount = 0.0;
            for (const auto& [packetId, hopCount] : stats.packet_hops) {
                avgHopCount += hopCount;
            }
            avgHopCount /= stats.packet_hops.size();
            oss << ", Avg Hop Count: " << avgHopCount;
        }
        NS_LOG_UNCOND(oss.str());

        totalPacketsSent += stats.packetsSent;
        totalPacketsReceived += stats.packetsReceived;
        if (stats.packetsSent > stats.packetsReceived) {
            lost_packets_traffic[key] = stats;
        }
    }
    NS_LOG_UNCOND("Traffic where packets got lost:");

    for (const auto& [key, stats] : lost_packets_traffic) {
        std::ostringstream oss;
        oss << "AppId: " << key.first
            << ", From: " << key.second.first << " (" << m_networkState.GetNodeIdForIp(key.second.first) << ")"
            << " To: " << key.second.second << " (" << m_networkState.GetNodeIdForIp(key.second.second) << ")"
            << ", Sent: " << stats.packetsSent
            << ", Received: " << stats.packetsReceived;

        if (stats.packetsReceived > 0) {
            double avgLatency = stats.totalLatency / stats.packetsReceived;
            oss << ", Min Latency: " << stats.minLatency << "ms"
                << ", Max Latency: " << stats.maxLatency << "ms"
                << ", Avg Latency: " << avgLatency << "ms";
        }

        for (const auto& [nodeId, count] : stats.packetsActivelyDroppedOnNode) {
            oss << ", Dropped " << count << " packets on node " << nodeId;
        }

        NS_LOG_UNCOND(oss.str());
    }

    NS_LOG_UNCOND("Total Packets Sent: " << totalPacketsSent);
    NS_LOG_UNCOND("Total Packets Received: " << totalPacketsReceived);
    NS_LOG_UNCOND("Ratio: " << (totalPacketsReceived / static_cast<double>(totalPacketsSent)) * 100 << "%");

    if (writeToCsv == true)
    {
        // one folder per run
        std::string csvFilePath = outputFolder + "/traffic_summary_run_" + std::to_string(run) + "_failure_scenario_" + std::to_string(failureScenario) + ".csv";
        std::string trafficSummaryFilePath = outputFolder + "/traffic_summary_run_" + std::to_string(run) + ".txt";
        WriteTrafficSummaryToCsv(csvFilePath);
         // Append total statistics to a single CSV file
        std::string totalsFilePath = outputFolder + "/traffic_summary_totals.csv";
        std::ofstream totalsFile(totalsFilePath, std::ios::app); // Open in append mode

        if (!totalsFile.is_open()) {
            NS_LOG_ERROR("Failed to open totals file: " << totalsFilePath);
            return;
        }

        // Write the header if the file is empty
        if (totalsFile.tellp() == 0) {
            totalsFile << "Run,FailureScenario,TotalPacketsSent,TotalPacketsReceived,DeliveryRatio(%)\n";
        }

        // Append the statistics for this run
        totalsFile << static_cast<int>(run) << "," << static_cast<int>(failureScenario) << "," << totalPacketsSent << ","
                << totalPacketsReceived << "," << (totalPacketsReceived / static_cast<double>(totalPacketsSent)) * 100 << "\n";

        totalsFile.close();
    }
}

void TrafficManager::ScheduleTraffic(const std::string& outputFolder, const uint8_t run, bool writeToCsv, int failureScenario) {
    int id_counter = 0;
    for (const auto& traffic : m_trafficVector) {
        //DEBUG  632430d9e114d to 632430d9e1137
        /*if((traffic.src_node_id != "632430d9e114d") ||  (traffic.dst_node_id != "632430d9e1137")){
            continue;
        }
        if (id_counter != 120){
            id_counter++;
            continue;
        }*/
        NS_LOG_DEBUG("Scheduling traffic from " << traffic.src_node_id << " to " << traffic.dst_node_id);
        Simulator::Schedule(Seconds(traffic.start_time), &TrafficManager::ScheduleTrafficEvent, this, traffic, id_counter);
        id_counter++;
        //break;

    }
    // Schedule the traffic summary to be printed after the simulation ends
    Simulator::ScheduleDestroy(&TrafficManager::PrintTrafficSummary, this, outputFolder, run, writeToCsv, failureScenario);
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
    std::pair<Ipv4Address, Ipv4Address> source_destination = std::make_pair(srcAddress, dstAddress);
    std::pair<uint32_t, std::pair<Ipv4Address,Ipv4Address>> flowKey = std::make_pair(counter, source_destination);
    NS_LOG_DEBUG("Scheduling traffic from " << srcAddress << "(" << traffic.src_node_id << ") to " << dstAddress << "(" << traffic.dst_node_id << ")");
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

void TrafficManager::IncreasePacketSentProxy(Ipv4Header ipv4Header, leo::PacketIdTag tag) {
    //NS_LOG_INFO("Packet sent");

    Ipv4Address srcAddress = ipv4Header.GetSource();
    Ipv4Address dstAddress = ipv4Header.GetDestination();
    NS_LOG_DEBUG("Packet sent for App " << tag.GetAppId() << " from " << srcAddress << " to " << dstAddress);
    // Create a key using the source and destination addresses
    std::pair<Ipv4Address,Ipv4Address> source_destination = std::make_pair(srcAddress, dstAddress);
    std::pair<uint32_t, std::pair<Ipv4Address,Ipv4Address>> key = std::make_pair(tag.GetAppId(), source_destination);

    auto it = m_trafficStats.find(key);
    if (it != m_trafficStats.end()) {
        m_trafficStats[key].packetsSent++;
    }

}

void TrafficManager::IncreasePacketReceivedProxy(Ipv4Header ipv4Header, leo::PacketIdTag tag) {
    Ipv4Address srcAddress = ipv4Header.GetSource();
    Ipv4Address dstAddress = ipv4Header.GetDestination();
    NS_LOG_DEBUG("Packet received " << srcAddress << " -> " << dstAddress);
    NS_LOG_DEBUG("DEBUG: Packet received at " << Simulator::Now().GetSeconds()
              << "s, tag timestamp: " << tag.GetTimestamp().GetSeconds()
              << "s, latency = "
              << (Simulator::Now() - tag.GetTimestamp()).GetMilliSeconds()
              << " ms");


    // Create a key using the source and destination addresses
    std::pair<Ipv4Address,Ipv4Address> source_destination = std::make_pair(srcAddress, dstAddress);
    std::pair<uint32_t, std::pair<Ipv4Address,Ipv4Address>> key = std::make_pair(tag.GetAppId(), source_destination);

    auto it = m_trafficStats.find(key);
    if (it != m_trafficStats.end()) {
        m_trafficStats[key].packetsReceived++;
        // Calculate latency
        double latency = (Simulator::Now() - tag.GetTimestamp()).GetMilliSeconds();
        TrafficStats& stats = m_trafficStats[key];
        NS_LOG_DEBUG("Packet with sent timestamp " << tag.GetTimestamp() << " received at " << Simulator::Now() << " for App " << tag.GetAppId() << " from " << srcAddress << " to " << dstAddress
                     << ", with Latency: " << latency << "ms and hop count " << m_trafficStats[key].packet_hops[tag.GetPacketNumber()]);
        stats.minLatency = std::min(stats.minLatency, latency);
        stats.maxLatency = std::max(stats.maxLatency, latency);
        stats.totalLatency += latency;
     }
}
void TrafficManager::IncreasePacketHopCountProxy(Ipv4Header ipv4Header, leo::PacketIdTag tag){
    Ipv4Address srcAddress = ipv4Header.GetSource();
    Ipv4Address dstAddress = ipv4Header.GetDestination();

    // Create a key using the source and destination addresses
    std::pair<Ipv4Address,Ipv4Address> source_destination = std::make_pair(srcAddress, dstAddress);
    std::pair<uint32_t, std::pair<Ipv4Address,Ipv4Address>> key = std::make_pair(tag.GetAppId(), source_destination);

    auto it = m_trafficStats.find(key);
    if (it != m_trafficStats.end()) {
        m_trafficStats[key].packet_hops[tag.GetPacketNumber()]++;
        m_trafficStats[key].minHopCount = std::min(m_trafficStats[key].minHopCount, tag.GetHopCount());
        m_trafficStats[key].maxHopCount = std::max(m_trafficStats[key].maxHopCount, tag.GetHopCount());
    }

}

void TrafficManager::IncreaseActivelyDroppedPacketProxy(Ipv4Header ipv4Header, int appId, std::string nodeId) {
    Ipv4Address srcAddress = ipv4Header.GetSource();
    Ipv4Address dstAddress = ipv4Header.GetDestination();
    NS_LOG_DEBUG("Packet actively dropped " << srcAddress << " -> " << dstAddress);

    // Create a key using the source and destination addresses
    std::pair<Ipv4Address,Ipv4Address> source_destination = std::make_pair(srcAddress, dstAddress);
    std::pair<uint32_t, std::pair<Ipv4Address,Ipv4Address>> key = std::make_pair(appId, source_destination);

    auto it = m_trafficStats.find(key);
    if (it != m_trafficStats.end()) {
        m_trafficStats[key].packetsActivelyDroppedOnNode[nodeId]++;
    }
}
void TrafficManager::IncreaseBackupPathUsedProxy(Ipv4Header ipv4Header, leo::PacketIdTag tag){
    Ipv4Address srcAddress = ipv4Header.GetSource();
    Ipv4Address dstAddress = ipv4Header.GetDestination();

    // Create a key using the source and destination addresses
    std::pair<Ipv4Address,Ipv4Address> source_destination = std::make_pair(srcAddress, dstAddress);
    std::pair<uint32_t, std::pair<Ipv4Address,Ipv4Address>> key = std::make_pair(tag.GetAppId(), source_destination);

    auto it = m_trafficStats.find(key);
    if (it != m_trafficStats.end()) {
        m_trafficStats[key].backupPathUsed++;
    }
}
void TrafficManager::IncreaseLoopAvoidanceTriggeredProxy(Ipv4Header ipv4Header, leo::PacketIdTag tag){
    Ipv4Address srcAddress = ipv4Header.GetSource();
    Ipv4Address dstAddress = ipv4Header.GetDestination();

    // Create a key using the source and destination addresses
    std::pair<Ipv4Address,Ipv4Address> source_destination = std::make_pair(srcAddress, dstAddress);
    std::pair<uint32_t, std::pair<Ipv4Address,Ipv4Address>> key = std::make_pair(tag.GetAppId(), source_destination);

    auto it = m_trafficStats.find(key);
    if (it != m_trafficStats.end()) {
        m_trafficStats[key].loopAvoidanceTriggered++;
    }
}

} // namespace leo
} // namespace ns3