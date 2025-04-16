#ifndef CUSTOM_IPV4_L3_PROTOCOL_H
#define CUSTOM_IPV4_L3_PROTOCOL_H

#include "ns3/ipv4-l3-protocol.h"
#include "ns3/routing-manager.h"
#include "ns3/ipv4-routing-protocol.h"
#include "ns3/ipv4-address.h"
#include "ns3/ipv4-header.h"
#include "ns3/packet.h"
#include "ns3/socket.h"
#include "ns3/node.h"
#include "ns3/net-device.h"
#include "ns3/nstime.h"
#include <map>
#include "ns3/ip-assignment.h"
#include "ns3/network-state.h"

namespace ns3 {
namespace leo {

class CustomRoutingProtocol : public Ipv4RoutingProtocol {
public:
    static TypeId GetTypeId();
    CustomRoutingProtocol() = default;
    CustomRoutingProtocol(leo::NetworkState &networkState, Ptr<Node> m_node);
    virtual ~CustomRoutingProtocol();

    // Called when a packet is sent out. Input is not necessarily called before if packet is created on node itself (by an application)
    virtual Ptr<Ipv4Route> RouteOutput(Ptr<Packet> packet, const Ipv4Header &header,
                                       Ptr<NetDevice> oif, Socket::SocketErrno &sockerr) override;
    /*
    In case of switching tables this function gets called first when packets are received through an interface.
    If the destination is found in the switching table, it calls the UnicastForwardCallback to forward the packet.
    The UnicastForwardCallback in turn triggers the RouteOutput function to create a route for the packet.
    */
    virtual bool RouteInput(Ptr<const Packet> p,
                            const Ipv4Header& header,
                            Ptr<const NetDevice> idev,
                            const UnicastForwardCallback& ucb,
                            const MulticastForwardCallback& mcb,
                            const LocalDeliverCallback& lcb,
                            const ErrorCallback& ecb) override;

    int32_t GetInterfaceForNextHop(Ipv4Address nextHop);
    virtual void NotifyInterfaceUp(uint32_t interface) override;
    virtual void NotifyInterfaceDown(uint32_t interface) override;
    virtual void NotifyAddAddress(uint32_t interface, Ipv4InterfaceAddress address) override;
    virtual void NotifyRemoveAddress(uint32_t interface, Ipv4InterfaceAddress address) override;
    virtual void SetIpv4(Ptr<Ipv4> ipv4) override;
    virtual void PrintRoutingTable(Ptr<OutputStreamWrapper> stream,  Time::Unit unit = Time::S) const override;


    void SetSwitchingTable(const SwitchingTable &table);
    void SetNextHopToDeviceMap(leo::IpAssignmentHelper &ipAssignmentHelper);
    void AddNextHop(Ipv4Address nextHop, uint32_t interface);

private:
    Ptr<Ipv4> m_ipv4; // represents the ipv4 stack on node
    Ptr<Node> m_node; // represents the node this protocol is attached to
    SwitchingTable m_switchingTable;
    // map to store the next hop ip to output device ip
    std::map<Ipv4Address, Ptr<NetDevice>> m_nextHopToDeviceMap;
    leo::NetworkState& m_networkState;
};

} // namespace leo
} // namespace ns3

#endif // CUSTOM_ROUTING_PROTOCOL_H