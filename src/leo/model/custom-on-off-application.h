#include "ns3/application.h"
#include "ns3/packet.h"
#include "ns3/ipv4-address.h"
#include "ns3/ipv4.h"
#include "ns3/socket.h"
#include "ns3/udp-socket.h"
#include "ns3/timer.h"

namespace ns3 {
namespace leo {

class CustomOnOffApplication : public Application {
public:
    CustomOnOffApplication();
    virtual ~CustomOnOffApplication();

    void Setup(Ptr<Node> node, Ipv4Address dstAddress, uint16_t srcPort, uint16_t dstPort,
               uint32_t packetSize, std::string rate, double duration, int appId);
    uint64_t ParseRate(const std::string &rateStr) ;
    void SetupReceiver(Ptr<Node> receiverNode, uint16_t listenPort);
    void HandleRead(Ptr<Socket> socket);

protected:
    virtual void StartApplication() override;
    virtual void StopApplication() override;
    void SendPacket();
    void ScheduleNextPacket();

private:
    Ptr<Socket> m_socket;
    Ipv4Address m_dstAddress;
    uint16_t m_srcPort;
    uint16_t m_dstPort;
    uint32_t m_packetSize;
    std::string m_rate;
    Time m_actualStartTime;
    double m_duration;
    EventId m_sendEvent;
    bool m_running;
    uint64_t m_sentPackets;
    Ptr<Node> m_node;
    double m_interval;  // Interval between packets in secondsi
    int m_appId = 0;
};
class PacketIdTag : public Tag {
    public:
        void SetId(uint32_t id) { m_id = id; }
        uint32_t GetId() const { return m_id; }

        void Serialize(TagBuffer i) const override { i.WriteU32(m_id); }
        void Deserialize(TagBuffer i) override { m_id = i.ReadU32(); }
        uint32_t GetSerializedSize() const override { return 4; }
        void Print(std::ostream &os) const override { os << "PacketId=" << m_id; }

        static TypeId GetTypeId() {
            static TypeId tid = TypeId("PacketIdTag")
                .SetParent<Tag>()
                .AddConstructor<PacketIdTag>();
            return tid;
        }
        TypeId GetInstanceTypeId() const override {
            return GetTypeId();
        }

    private:
        uint32_t m_id;
    };

}  // namespace leo
}  // namespace ns3
