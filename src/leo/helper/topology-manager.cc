// topology-manager.cc
#include "topology-manager.h"
#include "ns3/simulator.h"
#include "ns3/log.h"
#include "ns3/point-to-point-net-device.h"
#include "ns3/net-device-container.h"
#include "ns3/channel.h"
#include "ns3/string.h"
#include "ns3/ip-assignment.h"


namespace ns3 {
namespace leo {

NS_LOG_COMPONENT_DEFINE("TopologyManager");
const double speedOfLight = 299792.4580;

TopologyManager::TopologyManager(AnimationInterface& anim)
    : m_networkState(NetworkState::GetInstance()), m_anim(anim) {}


void TopologyManager::ScheduleAllEvents(const std::map<double, std::vector<FileReader::ConstellationEvent>>& constellation_events_map) {
    for (const auto& events : constellation_events_map) {
        for (const auto& event : events.second) {
            Simulator::Schedule(Seconds(events.first),
                                &TopologyManager::ApplyEvent, this, event);
        }
    }
}
void TopologyManager::UpdateLinkDistances(const std::vector<FileReader::Edge>& dynamicEdges) {
    for (const auto& edge : dynamicEdges) {
        NS_LOG_DEBUG("Updating link distance for edge: " << edge.source << " -> " << edge.target
                     << " with weight (distance): " << edge.weight << " at time: " << Simulator::Now().GetSeconds());

        // Normalize the key to ensure consistency in accessing m_links
        auto key = m_networkState.NormalizeKey(edge.source, edge.target);

        LinkInfo& linkInfo = m_networkState.GetLinkInfo(key.first, key.second);
        // Ensure the link is valid
        if (linkInfo.IsValid()) {
            // Update the channel's delay based on the weight (distance)
            Ptr<PointToPointNetDevice> deviceA = DynamicCast<PointToPointNetDevice>(linkInfo.deviceA);
            Ptr<PointToPointNetDevice> deviceB = DynamicCast<PointToPointNetDevice>(linkInfo.deviceB);

            if (deviceA && deviceB) {
                Ptr<Channel> channel = linkInfo.channel;
                if (channel) {
                    double delayInSeconds = edge.weight / speedOfLight;
                    std::ostringstream delayStream;
                    // Delay in milliseconds
                    delayStream << (delayInSeconds * 1e3) << "ms";
                    // Update the delay on the channel
                    Time delay = Seconds(edge.weight / speedOfLight);
                    deviceA->GetChannel()->SetAttribute("Delay", StringValue(delayStream.str()));
                    deviceB->GetChannel()->SetAttribute("Delay", StringValue(delayStream.str()));

                    NS_LOG_DEBUG("Updated channel delay for link: " << edge.source << " -> " << edge.target
                                    << " to " << delay.GetSeconds() << " seconds");
                } else {
                    NS_LOG_WARN("Channel is null for link: " << edge.source << " -> " << edge.target);
                }
            } else {
                NS_LOG_WARN("Devices are not PointToPointNetDevices for link: " << edge.source << " -> " << edge.target);
            }
        } else {
            NS_LOG_WARN("Invalid link for edge: " << edge.source << " -> " << edge.target);
        }
    }
}

void TopologyManager::ScheduleLinkDistanceUpdates(const std::map<std::pair<double,double>, std::vector<FileReader::Edge>>& edgesByValidityPeriod,
                                                  const std::chrono::system_clock::time_point& simulationStart) {
    for (const auto& [validityPeriod, edges] : edgesByValidityPeriod) {
        double simTimeStart = validityPeriod.first;
        double simTimeEnd  = validityPeriod.second;
        NS_LOG_DEBUG("Scheduling link distance updates for period: " << simTimeStart << " to " << simTimeEnd);
        // Schedule the update at the start of the validity period
        Simulator::Schedule(Seconds(simTimeStart), &TopologyManager::UpdateLinkDistances, this, edges);
    }
}

void TopologyManager::ApplyEvent(FileReader::ConstellationEvent& event) {
    if (event.action == FileReader::ConstellationEvent::Action::LINK_DOWN) {
        NS_LOG_DEBUG("Disabeling Link" << " - " << event.from << " - " << event.to << " - " << event.weight);
        m_networkState.DisableLink(event.from, event.to);
    } else if (event.action == FileReader::ConstellationEvent::Action::LINK_UP) {
        NS_LOG_DEBUG("Enabeling Link" << " - " << event.from << " - " << event.to << " - " << event.weight);
        m_networkState.EnableLink(event.from, event.to, event.weight);
    }
    }
}
}
