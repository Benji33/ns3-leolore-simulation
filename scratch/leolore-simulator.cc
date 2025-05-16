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
#include <filesystem>
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
void LogSimulationTime() {
    NS_LOG_INFO("Current simulation time: " << Simulator::Now().GetSeconds() << " seconds");

    // Schedule the next log event after 1 second
    Simulator::Schedule(Seconds(1.0), &LogSimulationTime);
}

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
    //LogComponentEnable("DefaultSimulatorImpl", LOG_LEVEL_DEBUG);

    // Step 1: Parse in graph JSON file
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
    std::string folderName = "2025-03-21_11-28-00";
    double simulationEndTime = 10.0;
    bool printToCsv = true;
    bool enableAnimation = false;

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

    // Step 1: Initialize FileReader and read input files that stay constant across different simulations
    ns3::leo::FileReader reader;
    reader.readGraphFromJson(base_path + file_name + "/leo_constellation.json");
    reader.readConstellationEvents(base_path + file_name + "/events.json", simulationStart, false);
    reader.readTrafficFromJson(base_path + file_name + "/traffic.json");
    reader.readDynamicEdgesFromFolder(base_path + file_name + "/dynamic_edge_weights", simulationStart);

    std::vector<std::string> failureFiles = reader.GetFileNamesInFolder(base_path + file_name + "/failures");

    for (int run = 1; run <= 5; ++run) {
        NS_LOG_INFO("Starting simulation run " << run);
        /*if (run == 2){
            break;
        }*/
        std::string outputFolder = "/home/benji/Documents/Uni/Master/Results/" + folderName + "/run_" + std::to_string(run);
        if (!std::filesystem::exists(outputFolder)) {
            std::filesystem::create_directories(outputFolder);
        }

        int dev_iter = 0;
        // run for each failure scenario found in failure folder
        for (const auto& failureFile : failureFiles) {
            dev_iter++;
            if (dev_iter > 2) {
                break;
            }
            // Extract the failure number from the file name
            int failureNumber = reader.ExtractFailureNumber(failureFile);
            NS_LOG_INFO("Running with failure scenario: " << failureNumber);

            if(run > 1){
                // Apply the current failure scenario
                reader.readConstellationEvents(base_path + file_name + "/failures/" + failureFile, simulationStart, true);
            }

            // Consider updated switching tables from run 3 on
            if (run >= 3) {
                reader.readAllSwitchingTablesFromFolder(base_path + file_name + "/updated_switching_tables/scenario_" + std::to_string(failureNumber));
            } else {
                reader.readAllSwitchingTablesFromFolder(base_path + file_name + "/switching_tables");
            }
            //reader.printSwitchtingTables();
            // Set up simulation-specific configurations
            bool useBackupPath = (run >= 4);
            bool simpleLoopAvoidance = (run >= 5);

            /*
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
            */
            // Step 2: Create containers & nodes for ns-3 nodes
            // Map source IDs to ns-3 nodes
            // Create a network state object to manage the network state
            NS_LOG_INFO("Running simulation with failure scenario: " << failureNumber);
            NS_LOG_INFO("Simulation start time: " << std::put_time(std::gmtime(&simulationStartTimeT), "%Y-%m-%d %H:%M:%S UTC"));
            NS_LOG_INFO("Simulation duration: " << simulationEndTime);

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

            // Traffic settings
            for (const auto& traffic : reader.GetTraffic()) {
                NS_LOG_DEBUG("Traffic: " << traffic.src_node_id << " → " << traffic.dst_node_id << ", Protocol: " << traffic.protocol
                                        << ", Start Time: " << traffic.start_time << ", Duration: " << traffic.duration
                                        << ", Packet Size: " << traffic.packet_size << ", Rate: " << traffic.rate);
            }
            leo::TrafficManager trafficManager(reader.GetTraffic(), networkState);
            // Opts out here somewhere
            internetStack.Install(networkState.GetNodes());

            // Step 4: Attach CustomRoutingProtocol to all nodes
            // install custom forwarding logic - TODO: extract to populateForwardingTable function
            std::unordered_map<std::string, Ptr<leo::CustomRoutingProtocol>> customRoutingProtocols;
            for (const auto& [srcId, ns3Id] : networkState.GetSourceIdToNs3Id()) {
                Ptr<leo::CustomRoutingProtocol> customRouting = CreateObject<leo::CustomRoutingProtocol>(networkState.GetNodeBySourceId(srcId),
                                                                                                        trafficManager,
                                                                                                        networkState,
                                                                                                        simpleLoopAvoidance,
                                                                                                        useBackupPath);
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

            trafficManager.ScheduleTraffic(outputFolder, run, printToCsv, failureNumber);
            // Set up animation for nodes
            if (enableAnimation) {
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
            }
            leo::TopologyManager topologyManager(networkState);

            // Schedule all events and failures
            topologyManager.ScheduleAllEvents(reader.GetConstellationEvents());
            topologyManager.ScheduleAllEvents(reader.GetFailures());
            topologyManager.ScheduleLinkDistanceUpdates(reader.GetEdgesByValidityPeriod(), simulationStart);

            //Ptr<FlowMonitor> flowMonitor;
            //FlowMonitorHelper flowHelper;
            //flowMonitor = flowHelper.InstallAll();
            // Step 10: Run the simulation
            Simulator::Schedule(Seconds(1.0), &LogSimulationTime);
            Simulator::Stop(Seconds(simulationEndTime));
            Simulator::Run();
            // Check for lost packets
            //flowMonitor->CheckForLostPackets();
            //flowMonitor->SerializeToXmlFile("TestFlowMonitor.xml", true, true);


            Simulator::Destroy();
            std::cerr.rdbuf(oldCerrStreamBuf);

            // For the first run we only need one iteration since it is without failures
            if (run == 1){
                break;
            }

        }
    }
    return 0;
}