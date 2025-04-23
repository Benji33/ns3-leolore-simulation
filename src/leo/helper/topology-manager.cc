// topology-manager.cc
#include "topology-manager.h"
#include "ns3/simulator.h"
#include "ns3/log.h"
#include "ns3/point-to-point-net-device.h"
#include "ns3/net-device-container.h"
#include "ns3/channel.h"


namespace ns3 {
namespace leo {

NS_LOG_COMPONENT_DEFINE("TopologyManager");

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
