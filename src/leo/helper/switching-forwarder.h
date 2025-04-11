#ifndef SWITCHING_FORWARDER_H
#define SWITCHING_FORWARDER_H

#include "ns3/application.h"
#include "ns3/ipv4-address.h"
#include "ns3/socket.h"
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include <map>

namespace ns3 {

namespace leo {
/*
    Application on top of node simplifes forwarding logic for now without reimplementing the IP stack again.
    Runs in user space in the simulation that acts as a simple router which forwards packets based on the switching table.
    Sockets simulate transport layer endpoints that listens on certain port.
*/
class SwitchingForwarder : public Application
{
public:
  static TypeId GetTypeId (void);
  SwitchingForwarder ();
  virtual ~SwitchingForwarder ();

  void SetListeningPort (uint16_t port);

protected:
  virtual void StartApplication (void) override;
  virtual void StopApplication (void) override;

private:
  void HandleRead (Ptr<Socket> socket);
  Ipv4Address GetNextHopForDestination (Ipv4Address dest, Time now);

  Ptr<Socket> m_socket;
  uint16_t m_listeningPort;
};

}
} // namespace ns3

#endif // SWITCHING_FORWARDER_H
