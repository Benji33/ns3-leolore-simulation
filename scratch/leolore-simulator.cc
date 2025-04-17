#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/file-reader.h"
#include "ns3/network-state.h"
#include "ns3/constellation-node-data.h"
#include "ns3/netanim-module.h"
#include "ns3/ip-assignment.h"
#include "ns3/routing-manager.h"
#include "ns3/constant-position-mobility-model.h"
#include "ns3/custom-ipv4-l3-protocol.h"
#include "ns3/topology-manager.h"
#include "ns3/traffic-manager.h"
#include <unordered_set>
#include <ctime>
#include <chrono>
#include <iomanip>
#include <boost/functional/hash.hpp>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LeoLoreSimulator");

int main(int argc, char *argv[]) {
    std::ofstream nullStream("/dev/null");
    std::streambuf* oldCerrStreamBuf = std::cerr.rdbuf(nullStream.rdbuf());

    LogComponentEnable("LeoLoreSimulator", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    LogComponentEnable("CustomRoutingProtocol", LOG_LEVEL_INFO);
    LogComponentEnable("IpAssignmentHelper", LOG_LEVEL_INFO);
    LogComponentEnable("TopologyManager", LOG_LEVEL_INFO);
    LogComponentEnable("RoutingManager", LOG_LEVEL_INFO);
    LogComponentEnable("FileReader", LOG_LEVEL_INFO);
    LogComponentEnable("NetworkState", LOG_LEVEL_INFO);
    LogComponentEnable("TrafficManager", LOG_LEVEL_INFO);

    //LogComponentEnable("Ipv4L3Protocol", LOG_LEVEL_INFO);

    std::string gs1Id = "632430d9e1196";
    std::string gs2Id = "632430d9e10d6";

    // Step 1: Parse in graph JSON file
    ns3::leo::FileReader reader;

    //uint64_t simulationStart = 1742599254; // 2025-03-21T11:20:54
    std::tm tm = {};
    std::istringstream ss("2025-03-21T11:20:54");
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    std::time_t t = timegm(&tm);
    auto simulationStart = std::chrono::system_clock::from_time_t(t);

    //TODO: Get JSON file path through command line argument
    reader.readGraphFromJson("/home/benji/Documents/Uni/Master/Simulation/leo_generation/output/leo_constellation.json");
    //reader.printGraph();

    reader.readSwitchingTableFromJson("/home/benji/Documents/Uni/Master/Simulation/leo_generation/output/1742556054/switching_tables.json");
    //reader.printSwitchtingTables();

    reader.ReadConstellationEvents("/home/benji/Documents/Uni/Master/Simulation/leo_generation/output/1742556054/events.json", simulationStart);
    //reader.printConstellationEvents();

    reader.ReadTrafficFromJson("/home/benji/Documents/Uni/Master/Simulation/leo_generation/output/1742556054/traffic.json");

    // Step 2: Create containers & nodes for ns-3 nodes
    // Map source IDs to ns-3 nodes
    // Create a network state object to manage the network state
    leo::NetworkState networkState;


    NS_LOG_INFO("Number of nodes: " << reader.GetNodes().size());

    for (const auto& node_ptr : reader.GetNodes()) {
        Ptr<Node> networkNode = CreateObject<Node>();
        Ptr<leo::ConstellationNodeData> data = CreateObject<leo::ConstellationNodeData>();
        data->SetSourceId(node_ptr->id);
        data->SetType(node_ptr->type);
        // Needed to get rid of mobility warnings for animator
        Ptr<MobilityModel> mobility = CreateObject<ConstantPositionMobilityModel>();
        networkNode->AggregateObject(mobility);

        if (node_ptr->type == "satellite") {
            const auto* satNode = dynamic_cast<const leo::FileReader::SatelliteNode*>(node_ptr.get());
            if (satNode) {
                data->SetOrbit(satNode->orbit);
            } else {
                NS_LOG_ERROR("Failed to cast node to SatelliteNode");
            }
            networkNode->AggregateObject(data);
            networkState.RegisterNode(networkNode, networkNode->GetId(), node_ptr->id, true);
        } else if (node_ptr->type == "ground_station") {
            const auto* gsNode = dynamic_cast<const leo::FileReader::GroundStationNode*>(node_ptr.get());
            if (gsNode) {
                data->SetTown(gsNode->town);
            } else {
                NS_LOG_ERROR("Failed to cast node to GroundStationNode");
            }
            networkNode->AggregateObject(data);
            networkState.RegisterNode(networkNode, networkNode->GetId(), node_ptr->id, false);

        } else {
            NS_LOG_ERROR("Unknown node type: " << node_ptr->type);
        }
    }

    // Step 3: Install the internet protocol stack (Ipv4 object and Ipv4L3protocol instance)
    InternetStackHelper internetStack;
    // Find node 632430d9e1196 in networkState.GetNodes()
    std::string targetSourceId = "IRIDIUM 145"; //"632430d9e1196";
    bool nodeFound = false;

    NodeContainer nodes = networkState.GetNodes();
    for (uint32_t i = 0; i < nodes.GetN(); ++i) {
        Ptr<Node> node = nodes.Get(i);
        Ptr<leo::ConstellationNodeData> data = node->GetObject<leo::ConstellationNodeData>();
        if (data && data->GetSourceId() == targetSourceId) {
            NS_LOG_INFO("Node with source ID " << targetSourceId << " is present in the NodeContainer.");
            nodeFound = true;
            break;
        }
    }

    if (!nodeFound) {
        NS_LOG_ERROR("Node with source ID " << targetSourceId << " is NOT present in the NodeContainer.");
    }
    // Traffic settings
    for (const auto& traffic : reader.GetTraffic()) {
        NS_LOG_INFO("Traffic: " << traffic.src_node_id << " → " << traffic.dst_node_id << ", Protocol: " << traffic.protocol
                                << ", Start Time: " << traffic.start_time << ", Duration: " << traffic.duration
                                << ", Packet Size: " << traffic.packet_size << ", Rate: " << traffic.rate);
    }
    leo::TrafficManager trafficManager(reader.GetTraffic(), networkState);

    internetStack.Install(networkState.GetNodes());

    // Step 4: Attach CustomRoutingProtocol to all nodes
    // install custom forwarding logic - TODO: extract to populateForwardingTable function
    std::unordered_map<std::string, Ptr<leo::CustomRoutingProtocol>> customRoutingProtocols;
    for (const auto& [srcId, ns3Id] : networkState.GetSourceIdToNs3Id()) {
        Ptr<leo::CustomRoutingProtocol> customRouting = CreateObject<leo::CustomRoutingProtocol>(networkState,
                                                                                                networkState.GetNodeBySourceId(srcId),
                                                                                                trafficManager);
        Ptr<Ipv4> ipv4 = networkState.GetNodeBySourceId(srcId)->GetObject<Ipv4>();
        if (ipv4) {
            customRouting->SetIpv4(ipv4);
            ipv4->SetRoutingProtocol(customRouting);
            customRoutingProtocols[srcId] = customRouting; // Store the protocol for later use
            // NS_LOG_INFO("Custom routing protocol attached to node " << nodeId);
            // Disable ICMPv4
            //ipv4->SetAttribute("Icmp", BooleanValue(false));
            //ipv4->SetAttribute("SendIcmpv4PortUnreachable", BooleanValue(false));
        }
    }

    // Step 5: Assign IP addresses and collect IP map
    leo::IpAssignmentHelper ipAssignmentHelper;
    NS_LOG_INFO("Number of edges: " << reader.GetEdges().size());
    /*std::unordered_map<std::string, std::vector<Ipv4Address>> nodeIdToIpMap =
        ipAssignmentHelper.AssignIpAddresses(reader.GetEdges(), networkState);
    */
    ipAssignmentHelper.PrecreateAllLinks(reader.GetAllUniqueLinks(), networkState);

    // Disable links that are not active at the start of the simulation
    std::vector<ns3::leo::FileReader::Edge> edges = reader.GetEdges(); // Get edges from the FileReader
    std::unordered_set<std::pair<std::string, std::string>, boost::hash<std::pair<std::string, std::string>>> edgeSet;

    // Populate a set for quick lookup of valid edges
    for (const auto& edge : edges) {
        edgeSet.insert({edge.source, edge.target});
        edgeSet.insert({edge.target, edge.source}); // Include reverse direction for undirected links
    }

    // Iterate through all links in the NetworkState
    for (const auto& link : networkState.GetActiveLinks()) {
        const auto& srcId = link.first;
        const auto& dstId = link.second;

        // Check if the link exists in the edges vector
        if (edgeSet.find({srcId, dstId}) == edgeSet.end() && edgeSet.find({dstId, srcId}) == edgeSet.end()) {
            // Disable the link if it is not in the edges vector
            networkState.DisableLink(srcId, dstId);
            NS_LOG_DEBUG("Disabled link between " << srcId << " and " << dstId);
        }
    }

    // Step 6: Resolve Switching tables = map node ids to IP addresses
    leo::RoutingManager routingManager;
    routingManager.ResolveSwitchingTables(reader.GetRawSwitchingTables(),
    //nodeIdToIpMap,
    networkState,
    simulationStart);
    // Append switching tables to nodes
    routingManager.AttachSwitchingTablesToNodes(networkState);

    // Step 7: Set the switching table for each CustomRoutingProtocol
    for (const auto& [srcId, ns3Id] : networkState.GetSourceIdToNs3Id()) {
        Ptr<leo::CustomRoutingProtocol> customRouting = customRoutingProtocols[srcId];
        Ptr<leo::ConstellationNodeData> nodeData = networkState.GetNodeBySourceId(srcId)->GetObject<leo::ConstellationNodeData>();
        if (customRouting && nodeData) {
            customRouting->SetSwitchingTable(nodeData->GetSwitchingTable());
            //customRouting->SetNextHopToDeviceMap(ipAssignmentHelper);
            // NS_LOG_INFO("Switching table set for node " << nodeId);
        }
    }

    Ptr<Node> gs1 = networkState.GetNodeBySourceId(gs1Id);
    Ptr<Node> gs2 = networkState.GetNodeBySourceId(gs2Id);

    // TODO: This needs to be changed: Choose ip of outgoing network device that connects with next hop satellite from switching table


    // Step 6: Retrieve IP address of gs);
    // Get the IP address of gs1 and gs2
    Ipv4Address gs1Address = networkState.GetIpAddressForDevice(gs1->GetDevice(1));
    Ipv4Address gs2Address = networkState.GetIpAddressForDevice(gs2->GetDevice(1));

    // Retrieve nodes for gs1 and gs2
    const auto* gs1Node = dynamic_cast<const leo::FileReader::GroundStationNode*>(reader.GetNodeMap().at(gs1Id));
    const auto* gs2Node = dynamic_cast<const leo::FileReader::GroundStationNode*>(reader.GetNodeMap().at(gs2Id));

    if (gs1Node) {
        NS_LOG_INFO("GS1 '" << gs1Id << "', Town: " << gs1Node->town <<"' IP Address: " << gs1Address);
    } else {
        NS_LOG_ERROR("Failed to retrieve town for GS1 '" << gs1Id << "'");
    }

    if (gs2Node) {
        NS_LOG_INFO("GS2 '" << gs2Id << "', Town: " << gs2Node->town <<"' IP Address: " << gs2Address);
    } else {
        NS_LOG_ERROR("Failed to retrieve town for GS2 '" << gs2Id << "'");
    }
    /*// Print networkState.m_ipToNodeIdMap
    for (const auto& [ip, nodeId] : networkState.GetIpToNodeIdMap()) {
        NS_LOG_INFO("IP: " << ip << ", Node ID: " << nodeId);
    }*/
   // Step 7: Install a UDP server on gs2
   /*uint16_t port = 19;
   UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(gs2);
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));
    */
    // Step 8: Install a UDP client on gs1
    /*UdpEchoClientHelper echoClient(gs2Address, port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(1));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    */
   /*Ptr<Socket> clientSocket = Socket::CreateSocket(gs1, UdpSocketFactory::GetTypeId());

   // Get gs1's Ipv4 object and bind the socket to the correct interface

   Ptr<Ipv4> ipv42 = gs1->GetObject<Ipv4>();
   int32_t ifaceIndex = ipv42->GetInterfaceForAddress(gs1Address);
   NS_ASSERT_MSG(ifaceIndex >= 0, "Interface not found for gs1Address");
   Ptr<NetDevice> netDevice = gs1->GetDevice(ifaceIndex);

   // Bind socket to specific IP and port
   InetSocketAddress localBind(gs1Address, port); // Use gs1's IP
   clientSocket->Bind(localBind);
   clientSocket->BindToNetDevice(netDevice);

   // Connect to gs2
   InetSocketAddress remote(gs2Address, port);
   clientSocket->Connect(remote);

   // Schedule sending packet at 2s
   Simulator::Schedule(Seconds(2.0), [clientSocket]() {
       Ptr<Packet> packet = Create<Packet>(1024);  // 1024-byte payload
       clientSocket->Send(packet);
   });*/
   //Simulator::Schedule(Seconds(2.06), [&networkState]() {networkState.DisableLink("IRIDIUM 134", "IRIDIUM 145");});
    //clientApp.Start(Seconds(2.0));
    //clientApp.Stop(Seconds(10.0));

    // Step 9: Set up global routing for now
    //Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Step 9: Set up traffic manager
    //print traffic

    trafficManager.ScheduleTraffic();
    // Set up animation for nodes
    AnimationInterface anim("leolore-simulator.xml");
    anim.EnablePacketMetadata(true);

    for (const auto& node_ptr : reader.GetNodes()) {
        Ptr<Node> node = networkState.GetNodeBySourceId(node_ptr->id);

        anim.UpdateNodeDescription(node->GetId(), node_ptr->id);
        anim.SetConstantPosition(node, node_ptr->position.first, node_ptr->position.second);

        if (node_ptr->type == "satellite") {
            // Set color for animation to blue for satellites
            anim.UpdateNodeColor(node->GetId(), 0, 0, 255);
        } else if (node_ptr->type == "ground_station") {
            // Set color for animation to red for ground stations
            anim.UpdateNodeColor(node->GetId(), 255, 0, 0);
        }
    }
    leo::TopologyManager topologyManager(reader.GetConstellationEvents(), networkState, anim);
    topologyManager.ScheduleAllEvents();

    // Step 10: Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    std::cerr.rdbuf(oldCerrStreamBuf);
    return 0;
}