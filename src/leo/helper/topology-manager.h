// topology-manager.h
#pragma once

#include "ns3/node-container.h"
#include "ns3/file-reader.h"
#include "ns3/network-state.h"
#include "ns3/netanim-module.h"
#include <vector>
#include <map>

namespace ns3 {
namespace leo {

class TopologyManager {
public:
    TopologyManager(const std::map<double, std::vector<FileReader::ConstellationEvent>>& constellation_events_map,
                    AnimationInterface& anim);

    void ScheduleAllEvents();

private:
    ns3::NodeContainer m_nodes;
    std::map<double, std::vector<FileReader::ConstellationEvent>> m_events;
    leo::NetworkState& m_networkState;
    AnimationInterface& m_anim;

    void ApplyEvent(FileReader::ConstellationEvent& event);
};
}
}
