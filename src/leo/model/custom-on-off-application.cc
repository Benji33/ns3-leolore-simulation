#include "custom-on-off-application.h"

namespace ns3 {
namespace leo {

NS_LOG_COMPONENT_DEFINE("CustomOnOffApplication");

CustomOnOffApplication::CustomOnOffApplication()
    : m_socket(nullptr), m_dstAddress(Ipv4Address::GetAny()), m_dstPort(0),
      m_packetSize(0), m_rate("0bps"), m_duration(0),
      m_running(false), m_sentPackets(0), m_interval(0.0) {}

CustomOnOffApplication::~CustomOnOffApplication() {}

void CustomOnOffApplication::Setup(Ptr<Node> node, Ipv4Address dstAddress, uint16_t srcPort, uint16_t dstPort,
                                    uint32_t packetSize, std::string rate, double duration, int appId) {
    m_node = node;
    m_dstAddress = dstAddress;
    m_srcPort = srcPort;
    m_dstPort = dstPort;
    m_packetSize = packetSize;
    m_rate = rate;
    m_duration = duration;
    m_appId = appId;

    // Calculate the interval based on the rate (in bits per second)
    uint64_t bps = ParseRate(rate);
    NS_LOG_UNCOND("RATE: " << rate << " -> " << bps << " bps");
    m_interval = static_cast<double>(m_packetSize * 8) / bps;
}

void CustomOnOffApplication::StartApplication() {
    NS_LOG_UNCOND("App " << m_appId << " started at " << Simulator::Now());
    m_actualStartTime = Simulator::Now();
    // Set up the socket and connect it
    m_socket = Socket::CreateSocket(m_node, TypeId::LookupByName("ns3::UdpSocketFactory"));

    // Get the source IP address of the node
    Ptr<Ipv4> ipv4 = m_node->GetObject<Ipv4>();
    Ipv4Address srcAddress = Ipv4Address::GetAny();
    for (uint32_t i = 0; i < ipv4->GetNInterfaces(); ++i) {
        for (uint32_t j = 0; j < ipv4->GetNAddresses(i); ++j) {
            Ipv4Address addr = ipv4->GetAddress(i, j).GetLocal();
            if (addr != Ipv4Address::GetLoopback()) {
                srcAddress = addr;
                break;
            }
        }
        if (srcAddress != Ipv4Address::GetAny()) {
            break;
        }
    }

    // Bind the socket to the source address
    InetSocketAddress local = InetSocketAddress(srcAddress, m_srcPort); // Use port 0 for automatic port assignment
    m_socket->Bind(local);

    // Connect the socket to the destination address
    InetSocketAddress remote = InetSocketAddress(m_dstAddress, m_dstPort);
    m_socket->Connect(remote);

    m_running = true;
    m_sentPackets = 0;

    // Schedule packet sending
    ScheduleNextPacket();
}

void CustomOnOffApplication::StopApplication() {
    if (m_socket) {
        m_socket->Close();
    }
    m_running = false;
    Simulator::Cancel(m_sendEvent);
}

void CustomOnOffApplication::SendPacket() {
    if (!m_running) return;

    Ptr<Packet> packet = Create<Packet>(m_packetSize);
    leo::PacketIdTag tag;
    tag.SetId(m_appId);
    packet->AddPacketTag(tag);
    // Send the packet
    NS_LOG_DEBUG("Sending packet at " << Simulator::Now().GetSeconds());

    m_socket->Send(packet);
    m_sentPackets++;

    // Schedule the next packet if the application is still running
    if (Simulator::Now() < m_actualStartTime + Seconds(m_duration + 0.5))
    {
        ScheduleNextPacket();
    }
}

void CustomOnOffApplication::ScheduleNextPacket() {
    if (m_running) {
        m_sendEvent = Simulator::Schedule(Seconds(m_interval), &CustomOnOffApplication::SendPacket, this);
    }
}

void CustomOnOffApplication::SetupReceiver(Ptr<Node> receiverNode, uint16_t listenPort) {
    Ptr<Socket> recvSocket = Socket::CreateSocket(receiverNode, TypeId::LookupByName("ns3::UdpSocketFactory"));

    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), listenPort);
    recvSocket->Bind(local);
    recvSocket->SetRecvCallback(MakeCallback(&CustomOnOffApplication::HandleRead, this));

    NS_LOG_DEBUG("Receiver socket set up on node " << receiverNode->GetId() << " port " << listenPort);
}

void CustomOnOffApplication::HandleRead(Ptr<Socket> socket) {
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom(from))) {
        NS_LOG_DEBUG("Node " << socket->GetNode()->GetId() << " received packet of size " << packet->GetSize());
        // Optionally read packet tags, content, etc.
    }
}

uint64_t CustomOnOffApplication::ParseRate(const std::string &rateStr) {
    std::string numPart;
    std::string unitPart;

    for (char c : rateStr) {
        if (std::isdigit(c) || c == '.') {
            numPart += c;
        } else {
            unitPart += c;
        }
    }

    double value = std::stod(numPart);
    if (unitPart == "bps") return value;
    if (unitPart == "kbps") return value * 1e3;
    if (unitPart == "Mbps") return value * 1e6;
    if (unitPart == "Gbps") return value * 1e9;

    NS_ABORT_MSG("Invalid rate string: " << rateStr);
}


}  // namespace leo
}  // namespace ns3