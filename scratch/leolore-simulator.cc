#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/file-reader.h"
#include "ns3/custom-node-data.h"
#include "ns3/netanim-module.h"
#include "ns3/ip-assignment.h"
#include "ns3/routing-manager.h"
#include "ns3/constant-position-mobility-model.h"
#include <ctime>
#include <chrono>
#include <iomanip>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LeoLoreSimulator");

int main(int argc, char *argv[]) {
    std::ofstream nullStream("/dev/null");
    std::streambuf* oldCerrStreamBuf = std::cerr.rdbuf(nullStream.rdbuf());

    LogComponentEnable("LeoLoreSimulator", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

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
    leo::IpAssignmentHelper ipAssignmentHelper;

    NS_LOG_INFO("Number of edges: " << reader.GetEdges().size());

    // Step 5: Assign IP addresses and collect IP map
    std::unordered_map<std::string, Ipv4Address> nodeIdToIpMap =
    ipAssignmentHelper.AssignIpAddresses(reader.GetEdges(), sourceIdNsNodeMap);

    NS_LOG_INFO("Printing nodeIdToIpMap:");
    for (const auto& pair : nodeIdToIpMap) {
        NS_LOG_INFO("Node ID: " << pair.first << ", IP Address: " << pair.second);
    }

    // Resolve Switching tables = map node ids to ip addresses
    leo::RoutingManager routingManager;
    std::vector<leo::SwitchingTable> resolvedTables = routingManager.ResolveSwitchingTables(reader.GetRawSwitchingTables(),
                                                                                            nodeIdToIpMap,
                                                                                            simulationStart);

    // Print one of the resolvedTables
    /*if (!resolvedTables.empty()) {
        const leo::SwitchingTable& table = resolvedTables[0]; // Access the first table
        NS_LOG_INFO("Switching Table for Node IP: " << table.node_ip);
        NS_LOG_INFO("Valid From: " << table.valid_from.GetSeconds() << " seconds");
        NS_LOG_INFO("Valid Until: " << table.valid_until.GetSeconds() << " seconds");
        NS_LOG_INFO("Routing Table:");
        for (const auto& entry : table.ip_routing_table) {
            NS_LOG_INFO("  Destination: " << entry.first << ", Next Hop: " << entry.second);
        }
    } else {
        NS_LOG_WARN("No resolved switching tables available.");
    }*/

    Ptr<Node> gs1 = sourceIdNsNodeMap[gs1Id];
    Ptr<Node> gs2 = sourceIdNsNodeMap[gs2Id];

    // Step 6: Retrieve IP address of gs);
    Ipv4Address gs1Address =  nodeIdToIpMap[gs1Id];
    Ipv4Address gs2Address =  nodeIdToIpMap[gs2Id];

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

    // To get rid of mobility warnings for animator
    for (const auto& node_ptr : reader.GetNodes()) {
        Ptr<Node> node = sourceIdNsNodeMap[node_ptr->id];
        if (!node->GetObject<MobilityModel>()) {
            Ptr<ConstantPositionMobilityModel> mobility = CreateObject<ConstantPositionMobilityModel>();
            node->AggregateObject(mobility);
        }
    }
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

    std::cerr.rdbuf(oldCerrStreamBuf);
    return 0;
}