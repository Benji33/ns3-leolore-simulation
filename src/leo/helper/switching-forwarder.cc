#include "switching-forwarder.h"
#include "ns3/log.h"
#include "ns3/ipv4.h"
#include "ns3/inet-socket-address.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/node.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/custom-node-data.h"

namespace ns3
{
namespace leo
{
NS_LOG_COMPONENT_DEFINE ("SwitchingForwarder");
NS_OBJECT_ENSURE_REGISTERED (SwitchingForwarder);

TypeId SwitchingForwarder::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::SwitchingForwarder")
    .SetParent<Application> ()
    .SetGroupName ("Applications")
    .AddConstructor<SwitchingForwarder> ();
  return tid;
}

SwitchingForwarder::SwitchingForwarder ()
  : m_socket (nullptr),
    m_listeningPort (9999) // Default
{
}

SwitchingForwarder::~SwitchingForwarder ()
{
  m_socket = nullptr;
}

void SwitchingForwarder::SetListeningPort (uint16_t port)
{
  m_listeningPort = port;
}

void SwitchingForwarder::StartApplication ()
{
  if (!m_socket)
  {
    m_socket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
    InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), m_listeningPort);
    m_socket->Bind (local);
    m_socket->SetRecvCallback (MakeCallback (&SwitchingForwarder::HandleRead, this));
    NS_LOG_INFO ("SwitchingForwarder started on port " << m_listeningPort);
  }
}

void SwitchingForwarder::StopApplication ()
{
  if (m_socket)
  {
    m_socket->Close ();
    m_socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket>> ());
    m_socket = nullptr;
  }
}

void SwitchingForwarder::HandleRead (Ptr<Socket> socket)
{
  Address from;
  Ptr<Packet> packet = socket->RecvFrom (from);

  // Extract destination IP from the first 4 bytes of the packet
  uint32_t dstRaw;
  packet->CopyData (reinterpret_cast<uint8_t*>(&dstRaw), sizeof(uint32_t));
  Ipv4Address dstAddr (dstRaw);

  Time now = Simulator::Now ();
  Ipv4Address nextHop = GetNextHopForDestination (dstAddr, now);

  if (nextHop == Ipv4Address::GetAny ())
  {
    NS_LOG_WARN ("No route found for destination " << dstAddr);
    return;
  }

  // Forward packet
  Ptr<Socket> forwardSocket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
  InetSocketAddress remote = InetSocketAddress (nextHop, m_listeningPort);
  forwardSocket->Connect (remote);
  forwardSocket->Send (packet);
  forwardSocket->Close ();

  NS_LOG_INFO ("Forwarded packet to " << nextHop);
}

Ipv4Address SwitchingForwarder::GetNextHopForDestination (Ipv4Address dest, Time now)
{
  auto node = GetNode ();
  if (!node->GetObject<ConstellationNodeData> ())
  {
    return Ipv4Address::GetAny ();
  }

  auto data = node->GetObject<ConstellationNodeData>();
  //for (const auto& table : data->GetSwitchingTables ())
    const auto& table = data->GetSwitchingTable();
    if (now >= table.valid_from && now <= table.valid_until)
    {
        auto it = table.ip_routing_table.find(dest);
        if(it != table.ip_routing_table.end())
        {
            return it->second;
        }
    }
  return Ipv4Address::GetAny ();
}

}
} // namespace ns3
