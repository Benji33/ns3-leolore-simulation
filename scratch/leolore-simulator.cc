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
#include <regex>
#include <iomanip>
#include <boost/functional/hash.hpp>
#include "ns3/flow-monitor-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LeoLoreSimulator");

std::chrono::system_clock::time_point ParseFolderNameToTimePoint(const std::string& folderName) {
    // Replace underscores with spaces and dashes with colons for parsing
    std::string formatted = folderName;
    std::replace(formatted.begin(), formatted.end(), '_', 'T'); // Replace '_' with 'T'

    // Replace dashes in the time portion with colons
    size_t timeStart = formatted.find('T') + 1;
    std::replace(formatted.begin() + timeStart, formatted.end(), '-', ':');

    // Parse the formatted string into a std::tm structure
    std::tm tm = {};
    std::istringstream ss(formatted);
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    if (ss.fail()) {
        throw std::runtime_error("Failed to parse folder name: " + folderName);
    }

    // Convert the std::tm structure to a time_point
    std::time_t t = timegm(&tm);
    return std::chrono::system_clock::from_time_t(t);
}

int main(int argc, char *argv[]) {
    std::ofstream nullStream("/dev/null");
    std::streambuf* oldCerrStreamBuf = std::cerr.rdbuf(nullStream.rdbuf());
    LogComponentEnable("LeoLoreSimulator", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    LogComponentEnable("CustomRoutingProtocol", LOG_LEVEL_INFO);
    LogComponentEnable("IpAssignmentHelper", LOG_LEVEL_INFO);
    LogComponentEnable("TopologyManager", LOG_LEVEL_DEBUG);
    LogComponentEnable("RoutingManager", LOG_LEVEL_INFO);
    LogComponentEnable("FileReader", LOG_LEVEL_INFO);
    LogComponentEnable("NetworkState", LOG_LEVEL_INFO);
    LogComponentEnable("TrafficManager", LOG_LEVEL_INFO);
    //LogComponentEnable("DefaultSimulatorImpl", LOG_LEVEL_DEBUG);

    std::string gs1Id = "632430d9e1196";
    std::string gs2Id = "632430d9e10d6";

    // Step 1: Parse in graph JSON file
    ns3::leo::FileReader reader;
    //uint64_t simulationStart = 1742599254; // 2025-03-21T11:20:54
    // 1742556073 , "2025-03-21 11:21:13+00:00"
    // Generation Timestamp:
    /*std::string input = "2025-03-21T11:20:30"; //"2025-03-21 11:21:13"
    std::string formatted = input.substr(0, 19); // Extract "2025-03-21 11:21:13"
    std::replace(formatted.begin(), formatted.end(), ' ', 'T'); // Replace space with 'T'
    std::tm tm = {};
    std::istringstream ss(formatted);//"2025-03-21T11:20:30");
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    std::time_t t = timegm(&tm);
    auto simulationStart = std::chrono::system_clock::from_time_t(t);
    //std::cout << "SimulationStart: " << simulationStart << std::endl;
    std::time_t simulationStartTimeT = std::chrono::system_clock::to_time_t(simulationStart);
    std::cout << "Simulation start time: "
    << std::put_time(std::gmtime(&simulationStartTimeT), "%Y-%m-%d %H:%M:%S UTC")
    << std::endl;

    std::string base_path = "/home/benji/Documents/Uni/Master/Simulation/leo_generation/output/";
    std::string file_name = std::to_string(t);//"1742556030";
    */
    std::string folderName = "2025-03-21_11-24-13";
    double simulationEndTime = 15.0;
    // Parse the folder name into a time_point
    auto simulationStart = ParseFolderNameToTimePoint(folderName);

    // Convert the time_point back to a time_t for logging
    std::time_t simulationStartTimeT = std::chrono::system_clock::to_time_t(simulationStart);
    std::cout << "Simulation start time: "
                << std::put_time(std::gmtime(&simulationStartTimeT), "%Y-%m-%d %H:%M:%S UTC")
                << std::endl;

    // Use the folder name as the file_name
    std::string base_path = "/home/benji/Documents/Uni/Master/Simulation/leo_generation/output/";
    std::string file_name = folderName;

    // Example usage of the file_name
    std::cout << "Base path: " << base_path << ", File name: " << file_name << std::endl;

    const bool simpleLoopAvoidance = true;


    //TODO: Get JSON file path through command line argument
    reader.readGraphFromJson(base_path + file_name + "/leo_constellation.json");
    //reader.printGraph();

    //reader.readSwitchingTableFromJson("/home/benji/Documents/Uni/Master/Simulation/leo_generation/output/1742556054/switching_tables.json");
    reader.readAllSwitchingTablesFromFolder(base_path + file_name + "/updated_switching_tables");
    //reader.printSwitchtingTables();

    // Events
    reader.readConstellationEvents(base_path + file_name + "/events.json", simulationStart, false);
    //reader.printConstellationEvents();

    // Failures
    reader.readConstellationEvents(base_path + file_name + "/failures.json", simulationStart, true);

    // Traffic
    reader.readTrafficFromJson(base_path + file_name + "/traffic.json");

    // Dynamic Edges
    reader.readDynamicEdgesFromFolder(base_path + file_name + "/dynamic_edge_weights", simulationStart);
    //reader.printDynamicEdges();
    // Step 2: Create containers & nodes for ns-3 nodes
    // Map source IDs to ns-3 nodes
    // Create a network state object to manage the network state
    leo::NetworkState& networkState = leo::NetworkState::GetInstance();


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

    // Traffic settings
    for (const auto& traffic : reader.GetTraffic()) {
        NS_LOG_DEBUG("Traffic: " << traffic.src_node_id << " → " << traffic.dst_node_id << ", Protocol: " << traffic.protocol
                                << ", Start Time: " << traffic.start_time << ", Duration: " << traffic.duration
                                << ", Packet Size: " << traffic.packet_size << ", Rate: " << traffic.rate);
    }
    leo::TrafficManager trafficManager(reader.GetTraffic());

    internetStack.Install(networkState.GetNodes());

    // Step 4: Attach CustomRoutingProtocol to all nodes
    // install custom forwarding logic - TODO: extract to populateForwardingTable function
    std::unordered_map<std::string, Ptr<leo::CustomRoutingProtocol>> customRoutingProtocols;
    for (const auto& [srcId, ns3Id] : networkState.GetSourceIdToNs3Id()) {
        Ptr<leo::CustomRoutingProtocol> customRouting = CreateObject<leo::CustomRoutingProtocol>(networkState.GetNodeBySourceId(srcId),
                                                                                                trafficManager,
                                                                                                simpleLoopAvoidance);
        Ptr<Ipv4> ipv4 = networkState.GetNodeBySourceId(srcId)->GetObject<Ipv4>();
        if (ipv4) {
            customRouting->SetIpv4(ipv4);
            ipv4->SetRoutingProtocol(customRouting);
            customRoutingProtocols[srcId] = customRouting; // Store the protocol for later use
            NS_LOG_DEBUG("Custom routing protocol attached to node " << srcId);
        }
    }

    // Step 5: Assign IP addresses and collect IP map
    leo::IpAssignmentHelper ipAssignmentHelper;
    NS_LOG_INFO("Number of edges: " << reader.GetEdges().size());
    /*std::unordered_map<std::string, std::vector<Ipv4Address>> nodeIdToIpMap =
        ipAssignmentHelper.AssignIpAddresses(reader.GetEdges(), networkState);
    */
    ipAssignmentHelper.PrecreateAllLinks(reader.GetAllUniqueLinks(), networkState, reader.dataRateIslMpbs, reader.dataRateFeederMpbs);

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
    networkState,
    simulationStart);

    // Append switching tables to nodes
    routingManager.AttachSwitchingTablesToNodes(networkState);

    // Step 7: Set the switching table for each CustomRoutingProtocol
    for (const auto& [srcId, ns3Id] : networkState.GetSourceIdToNs3Id()) {
        Ptr<leo::CustomRoutingProtocol> customRouting = customRoutingProtocols[srcId];
        Ptr<leo::ConstellationNodeData> nodeData = networkState.GetNodeBySourceId(srcId)->GetObject<leo::ConstellationNodeData>();
        if (customRouting && nodeData) {
            customRouting->SetSwitchingTables(nodeData->GetSwitchingTablesRef());
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
    leo::TopologyManager topologyManager(anim);
    // Schedule all events and failures
    topologyManager.ScheduleAllEvents(reader.GetConstellationEvents());
    topologyManager.ScheduleAllEvents(reader.GetFailures());
    topologyManager.ScheduleLinkDistanceUpdates(reader.GetEdgesByValidityPeriod(), simulationStart);

    //Ptr<FlowMonitor> flowMonitor;
    //FlowMonitorHelper flowHelper;
    //flowMonitor = flowHelper.InstallAll();
    // Step 10: Run the simulation
    Simulator::Stop(Seconds(simulationEndTime));
    Simulator::Run();
    // Check for lost packets
    //flowMonitor->CheckForLostPackets();
    //flowMonitor->SerializeToXmlFile("TestFlowMonitor.xml", true, true);


    Simulator::Destroy();
    std::cerr.rdbuf(oldCerrStreamBuf);

    return 0;
}