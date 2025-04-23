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
    TopologyManager(AnimationInterface& anim);

    void ScheduleAllEvents(const std::map<double, std::vector<FileReader::ConstellationEvent>>& constellation_events_map);

private:
    ns3::NodeContainer m_nodes;
    leo::NetworkState& m_networkState;
    AnimationInterface& m_anim;

    void ApplyEvent(FileReader::ConstellationEvent& event);
};
}
}
