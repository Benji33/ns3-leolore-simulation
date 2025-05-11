#ifndef CUSTOM_ON_OFF_APPLICATION_H
#define CUSTOM_ON_OFF_APPLICATION_H

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
        void SetId(int appId, uint64_t packetNumber) {
            m_appId = appId;
            m_packetNumber = packetNumber;
        }

        int GetAppId() const { return m_appId; }
        uint64_t GetPacketNumber() const { return m_packetNumber; }

        void Serialize(TagBuffer i) const override {
            i.WriteU32(m_appId);                     // Serialize App ID
            i.WriteU64(m_packetNumber);              // Serialize Packet Number
            i.WriteU16(m_hop_count);                 // Serialize Hop Count
            i.WriteU64(m_timestamp_sent.GetNanoSeconds()); // Serialize Timestamp as nanoseconds
        }

        void Deserialize(TagBuffer i) override {
            m_appId = i.ReadU32();                   // Deserialize App ID
            m_packetNumber = i.ReadU64();            // Deserialize Packet Number
            m_hop_count = i.ReadU16();               // Deserialize Hop Count
            m_timestamp_sent = NanoSeconds(i.ReadU64()); // Deserialize Timestamp as nanoseconds
        }
        void SetTimestamp(Time t) { m_timestamp_sent = t; }
        Time GetTimestamp() const { return m_timestamp_sent; }

        uint32_t GetSerializedSize() const override {
            return sizeof(uint32_t) + sizeof(uint64_t) + sizeof(uint16_t) + sizeof(uint64_t);
        }
        void IncreaseHopCount() { ++m_hop_count;}
        uint16_t GetHopCount() const { return m_hop_count; }

        void Print(std::ostream& os) const override {
            os << "AppId=" << m_appId << ", PacketNumber=" << m_packetNumber;
        }
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
        int m_appId;
        uint64_t m_packetNumber;
        Time m_timestamp_sent;
        uint16_t m_hop_count = 0;
    };

}  // namespace leo
}  // namespace ns3
#endif