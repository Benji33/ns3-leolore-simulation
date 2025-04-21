#ifndef CONSTELLATION_NODE_DATA_H
#define CONSTELLATION_NODE_DATA_H

#include "ns3/object.h"
#include <string>
#include <vector>
#include <algorithm>
#include "ns3/routing-manager.h"

namespace ns3
{
namespace leo
{

class ConstellationNodeData : public Object {
public:
    ConstellationNodeData() : m_switchingTables() {}

    void SetSourceId(std::string id) { m_sourceId = id; }
    std::string GetSourceId() const { return m_sourceId; }

    void SetType(std::string type) { m_type = type; }
    std::string GetType() const { return m_type; }

    void SetTown(std::string town) { m_town = town; }
    std::string GetTown() const { return m_town; }

    void SetOrbit(uint8_t orbit) { m_orbit = orbit; }
    uint8_t GetOrbit() const { return m_orbit; }

    // Add a SwitchingTable and keep the list sorted by valid_from
    void AddSwitchingTable(const ns3::leo::SwitchingTable& table) {
        m_switchingTables.push_back(table);
        std::sort(m_switchingTables.begin(), m_switchingTables.end(),
                  [](const ns3::leo::SwitchingTable& a, const ns3::leo::SwitchingTable& b) {
                      return a.valid_from < b.valid_from;
                  });
    }

    // Get all SwitchingTables
    const std::vector<ns3::leo::SwitchingTable>& GetSwitchingTables() const { return m_switchingTables; }
    std::vector<std::reference_wrapper<const SwitchingTable>> GetSwitchingTablesRef() const {
        std::vector<std::reference_wrapper<const SwitchingTable>> tableRefs;
        for (const auto& table : m_switchingTables) {
            tableRefs.push_back(std::cref(table));
        }
        return tableRefs;
    }

    static TypeId GetTypeId() {
        static TypeId tid = TypeId("ConstellationNodeData")
            .SetParent<Object>()
            .AddConstructor<ConstellationNodeData>();
        return tid;
    }

private:
    std::string m_sourceId;
    std::string m_type;
    std::string m_town;
    uint8_t m_orbit;
    std::vector<ns3::leo::SwitchingTable> m_switchingTables;
};

} // namespace leo
} // namespace ns3

#endif // CONSTELLATION_NODE_DATA_H