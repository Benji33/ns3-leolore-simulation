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

TopologyManager::TopologyManager(const std::map<double, std::vector<FileReader::ConstellationEvent>>& constellation_events_map,
                                leo::NetworkState& networkState,
                                AnimationInterface& anim)
    : m_events(constellation_events_map), m_networkState(networkState), m_anim(anim) {}


void TopologyManager::ScheduleAllEvents() {
    for (const auto& events : m_events) {
        for (const auto& event : events.second) {
            Simulator::Schedule(Seconds(events.first),
                                &TopologyManager::ApplyEvent, this, event);
        }
    }
}

void TopologyManager::ApplyEvent(FileReader::ConstellationEvent& event) {
    if (event.action == FileReader::ConstellationEvent::Action::LINK_DOWN) {
        //NS_LOG_INFO("Disabeling Link" << " - " << event.from << " - " << event.to << " - " << event.weight);
        m_networkState.DisableLink(event.from, event.to);
    } else if (event.action == FileReader::ConstellationEvent::Action::LINK_UP) {
        //NS_LOG_INFO("Enabeling Link" << " - " << event.from << " - " << event.to << " - " << event.weight);
        m_networkState.EnableLink(event.from, event.to, event.weight);
    }
    }
}
}
