#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/file-reader.h"
#include "ns3/custom-node-data.h"
#include "ns3/netanim-module.h"
#include "ns3/mobility-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LeoLoreSimulator");

int main(int argc, char *argv[]) {
    LogComponentEnable("LeoLoreSimulator", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    std::string gs1Id = "632430d9e1196";
    std::string gs2Id = "632430d9e10d6";

    // Step 1: Parse in graph JSON file
    ns3::leo::FileReader reader;

    //TODO: Get JSON file path through command line argument
    reader.readGraphFromJson("/home/benji/Documents/Uni/Master/Simulation/leo_generation/output/leo_constellation.json");
    reader.printGraph();

    // Step 2: Create containers & nodes for ns-3 nodes
    NodeContainer groundStations;
    NodeContainer satellites;

    // Map source IDs to ns-3 nodes
    std::unordered_map<std::string, Ptr<Node>> sourceIdNsNodeMap;



    NS_LOG_INFO("Number of nodes: " << reader.GetNodes().size());

    for (const auto& node_ptr : reader.GetNodes()) {
        Ptr<Node> networkNode = CreateObject<Node>();
        Ptr<leo::ConstellationNodeData> data = CreateObject<leo::ConstellationNodeData>();
        data->SetSourceId(node_ptr->id);
        sourceIdNsNodeMap[node_ptr->id] = networkNode;
        data->SetType(node_ptr->type);
        /*Ptr<MobilityModel> mobility = CreateObject<ConstantPositionMobilityModel>();
        networkNode->AggregateObject(mobility);*/

        if (node_ptr->type == "satellite") {
            const auto* satNode = dynamic_cast<const leo::FileReader::SatelliteNode*>(node_ptr.get());
            if (satNode) {
                data->SetOrbit(satNode->orbit);
            } else {
                NS_LOG_ERROR("Failed to cast node to SatelliteNode");
            }
            networkNode->AggregateObject(data);
            satellites.Add(networkNode);
        } else if (node_ptr->type == "ground_station") {
            const auto* gsNode = dynamic_cast<const leo::FileReader::GroundStationNode*>(node_ptr.get());
            if (gsNode) {
                data->SetTown(gsNode->town);
            } else {
                NS_LOG_ERROR("Failed to cast node to GroundStationNode");
            }
            networkNode->AggregateObject(data);
            groundStations.Add(networkNode);
        } else {
            NS_LOG_ERROR("Unknown node type: " << node_ptr->type);
        }
    }

    // Step 3: Install the internet protocol stack
    InternetStackHelper internetStack;
    internetStack.Install(groundStations);
    internetStack.Install(satellites);

    // Step 4: Set up point-to-point links (edges)
    // Step 5: Set IP addresses
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    Ipv4AddressHelper ipv4;
    int maj_subnet_counter = 1;
    int min_subnet_counter = 0;

    NS_LOG_INFO("Number of edges: " << reader.GetEdges().size());

    for (const auto &edge : reader.GetEdges()) {
        if (sourceIdNsNodeMap.find(edge.source) == sourceIdNsNodeMap.end()) {
            printf("Source node not found: %s\n", edge.source.c_str());
            NS_LOG_ERROR("Source node not found: " << edge.source);
            continue;
        }
        if (sourceIdNsNodeMap.find(edge.target) == sourceIdNsNodeMap.end()) {
            printf("Target node not found: %s\n", edge.target.c_str());
            NS_LOG_ERROR("Target node not found: " << edge.target);
            continue;
        }
        auto sourceNode = sourceIdNsNodeMap[edge.source];
        auto targetNode = sourceIdNsNodeMap[edge.target];

        // Create a point-to-point link between the source and target nodes
        NetDeviceContainer devices = p2p.Install(sourceNode, targetNode);

        // Assign IP addresses to the link
        if (maj_subnet_counter > 255) {
            NS_LOG_ERROR("Exceeded maximum number of subnets");
            break;
        }
        std::ostringstream subnetStream;
        subnetStream << "10." << maj_subnet_counter << "." << min_subnet_counter << ".0";
        std::string subnet = subnetStream.str();

        ipv4.SetBase(subnet.c_str(), "255.255.255.0");
        ipv4.Assign(devices);

        // Increment subnet counters
        min_subnet_counter++;
        if (min_subnet_counter == 255) {
            maj_subnet_counter++;
            min_subnet_counter = 0;
        }
    }

    Ptr<Node> gs1 = sourceIdNsNodeMap[gs1Id];
    Ptr<Node> gs2 = sourceIdNsNodeMap[gs2Id];

    // Step 6: Retrieve IP address of gs2
    Ptr<Ipv4> ipv4Gs2 = gs2->GetObject<Ipv4>();
    Ipv4Address gs2Address;
    for (uint32_t i = 0; i < ipv4Gs2->GetNInterfaces(); ++i) {
        for (uint32_t j = 0; j < ipv4Gs2->GetNAddresses(i); ++j) {
            Ipv4Address addr = ipv4Gs2->GetAddress(i, j).GetLocal();
            if (addr != Ipv4Address::GetLoopback()) {
                gs2Address = addr;
                break;
            }
        }
    }

    // Retrieve nodes for gs1 and gs2
    const auto* gs1Node = dynamic_cast<const leo::FileReader::GroundStationNode*>(reader.GetNodeMap().at(gs1Id));
    const auto* gs2Node = dynamic_cast<const leo::FileReader::GroundStationNode*>(reader.GetNodeMap().at(gs2Id));

    if (gs1Node) {
        NS_LOG_INFO("GS1 '" << gs1Id << "', Town: " << gs1Node->town);
    } else {
        NS_LOG_ERROR("Failed to retrieve town for GS1 '" << gs1Id << "'");
    }

    if (gs2Node) {
        NS_LOG_INFO("GS2 '" << gs2Id << "', Town: " << gs2Node->town <<"' IP Address: " << gs2Address);
    } else {
        NS_LOG_ERROR("Failed to retrieve town for GS2 '" << gs2Id << "'");
    }
    NS_LOG_INFO("GS2 '"<< gs2Id <<"' IP Address: " << gs2Address);

    // Step 7: Install a UDP server on gs2
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(gs2);
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Step 8: Install a UDP client on gs1
    UdpEchoClientHelper echoClient(gs2Address, port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(1));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(gs1);
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Step 9: Set up global routing for now
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Set up animation for nodes
    AnimationInterface anim("leolore-simulator.xml");
    anim.EnablePacketMetadata(true);

    for (const auto& node_ptr : reader.GetNodes()) {
        Ptr<Node> node = sourceIdNsNodeMap[node_ptr->id];
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
    // Step 10: Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}