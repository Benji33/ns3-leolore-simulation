#ifndef CUSTOM_NODE_DATA_H
#define CUSTOM_NODE_DATA_H

#include "ns3/object.h"
#include <string>

namespace ns3
{
namespace leo
{

class ConstellationNodeData : public Object {
public:
ConstellationNodeData() {}

    void SetSourceId(std::string id) { m_sourceId = id; }
    std::string GetSourceId() const { return m_sourceId; }

    void SetType(std::string type) { m_type = type; }
    std::string GetType() const { return m_type; }

    void SetTown(std::string town) { m_town = town; }
    std::string GetTown() const { return m_town; }

    void SetOrbit(uint8_t orbit) { m_orbit = orbit; }
    uint8_t GetOrbit() const { return m_orbit; }

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
};

} // namespace leo
} // namespace ns3

#endif // CUSTOM_NODE_DATA_H