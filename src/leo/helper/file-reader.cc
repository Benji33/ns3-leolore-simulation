#include "file-reader.h" // Include the header file
#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp> // Include the JSON library

namespace ns3
{
namespace leo
{
using json = nlohmann::json;

// Implementation of readGraphFromJson
void FileReader::readGraphFromJson(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file " << filename << std::endl;
        return;
    }

    json j;
    file >> j;

    starttime = j["starttime"];
    endtime = j["endtime"];

    for (const auto& node_data : j["nodes"]) {
        if (node_data["attributes"]["type"] == "satellite") {
            auto satellite_node = std::make_unique<SatelliteNode>();
            satellite_node->id = node_data["id"];
            const auto& node_attributes = node_data["attributes"];
            satellite_node->type = node_attributes["type"];
            satellite_node->position = {node_attributes["pos"][0], node_attributes["pos"][1]};
            satellite_node->orbit = node_attributes["orbit"];
            node_map[satellite_node->id] = satellite_node.get();
            nodes.push_back(std::move(satellite_node));
        } else if (node_data["attributes"]["type"] == "ground_station") {
            auto ground_station_node = std::make_unique<GroundStationNode>();
            ground_station_node->id = node_data["id"];
            const auto& node_attributes = node_data["attributes"];
            ground_station_node->type = node_attributes["type"];
            ground_station_node->position = {node_attributes["pos"][0], node_attributes["pos"][1]};
            ground_station_node->town = node_attributes["town"];
            node_map[ground_station_node->id] = ground_station_node.get();
            nodes.push_back(std::move(ground_station_node));
        }
    }

    // Parse edges
    for (const auto& edge_data : j["edges"]) {
        Edge edge;
        edge.source = edge_data["source"];
        edge.target = edge_data["target"];
        edge.weight = edge_data["weight"];
        edges.push_back(edge);
    }
}

// Implementation of printGraph
void FileReader::printGraph() const {
    std::cout << "Start Time: " << starttime << std::endl;
    std::cout << "End Time: " << endtime << std::endl;

    std::cout << "Nodes:" << std::endl;
    for (const auto& node_ptr : nodes) {
        // Print common Node attributes
        std::cout << "ID: " << node_ptr->id << ", Type: " << node_ptr->type;

        // Check if the node is a SatelliteNode
        if (const auto* satNode = dynamic_cast<const SatelliteNode*>(node_ptr.get())) {
            std::cout << ", Orbit: " << satNode->orbit;
        }
        // Check if the node is a GroundStationNode
        else if (const auto* gsNode = dynamic_cast<const GroundStationNode*>(node_ptr.get())) {
            std::cout << ", Town: " << gsNode->town;
        }

        std::cout << std::endl;
    }

    std::cout << "\nEdges:" << std::endl;
    for (const auto& edge : edges) {
        std::cout << "Source: " << edge.source << ", Target: " << edge.target
                  << ", Weight: " << edge.weight << std::endl;
    }
}
void FileReader::readSwitchingTableFromJson(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file " << filename << std::endl;
        return;
    }

    json j;
    file >> j;

    // Parse switching tables
    for (auto& table_data : j) {
        RawSwitchingTable table;
        table.node = table_data["node"];
        table.valid_from = table_data["valid_from"];
        table.valid_until = table_data["valid_until"];

        for (auto& entry : table_data["table_data"].items()) {
            table.table_data[entry.key()] = entry.value();
        }

        raw_switching_tables.push_back(table);
    }

}
void FileReader::printSwitchtingTables() const {
    if (raw_switching_tables.empty()) {
        std::cout << "No switching tables available." << std::endl;
        return;
    }

    std::cout << "Switching Tables:" << std::endl;
    for (const auto& table : raw_switching_tables) {
        std::cout << "Node: " << table.node << std::endl;
        std::cout << "Valid From: " << table.valid_from << std::endl;
        std::cout << "Valid Until: " << table.valid_until << std::endl;
        std::cout << "Table Data:" << std::endl;

        for (const auto& entry : table.table_data) {
            std::cout << "  Destination: " << entry.first
                      << ", Next Hop: " << entry.second << std::endl;
        }

        std::cout << "---------------------------------" << std::endl;
    }
}


} // namespace leo
} // namespace ns3