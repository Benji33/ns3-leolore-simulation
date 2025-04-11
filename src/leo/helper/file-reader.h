#ifndef FILEREADER_H
#define FILEREADER_H

#include <string>
#include <chrono>
#include <ctime>
#include <vector>
#include <unordered_map>
#include <memory>
namespace ns3
{
namespace leo
{

class FileReader {
public:
    std::string starttime;
    std::string endtime;

    // Structures to hold node and edge data
    struct Node {
        std::string id;
        std::string type;
        std::pair <float, float> position;

        virtual ~Node() = default;
    };

    struct SatelliteNode : public Node {
        int orbit;
    };

    struct GroundStationNode : public Node {
        std::string town;
    };

    struct Edge {
        // ids of nodes
        std::string source;
        std::string target;
        // weight = distance in km
        float weight;
    };

    struct RawSwitchingTable {
        std::string node;
        std::string valid_from;
        std::string valid_until;
        // routes
        std::unordered_map<std::string, std::string> table_data;
    };

    struct ConstellationEvent {
        enum Action { LINK_UP, LINK_DOWN };
        std::string from;
        std::string to;
        double weight;
        Action action;
    };

    // Constructor
    FileReader() = default;

    //Helper
    std::chrono::_V2::system_clock::time_point parseTimestampToTimePoint(const std::string& timestampStr);
    double secondsSinceStart(const std::tm& t, const std::tm& start);

    //Graph
    void readGraphFromJson(const std::string& filename);

    // Switching tables
    void readSwitchingTableFromJson(const std::string& filename);

    // Events
    void ReadConstellationEvents(const std::string& filename, std::chrono::_V2::system_clock::time_point& startTimeStr);

    // Getters for private members
    const std::vector<std::unique_ptr<Node>>& GetNodes() const { return nodes; }
    const std::vector<Edge>& GetEdges() const { return edges; }
    const std::unordered_map<std::string, Node*>& GetNodeMap() const { return node_map; }
    std::vector<RawSwitchingTable>& GetRawSwitchingTables() { return raw_switching_tables; }

    // Visulization
    void printGraph() const;
    void printSwitchtingTables() const;

private:
    // Vectors to hold nodes and edges
    std::vector<std::unique_ptr<Node>> nodes;
    std::vector<Edge> edges;
    std::vector<RawSwitchingTable> raw_switching_tables;
    std::map<double, std::vector<ConstellationEvent>> constellation_events_map;

    // Map for quick node access by ID
    std::unordered_map<std::string, Node*> node_map;
};

}  // namespace leo
} // namespace ns3

#endif // FILEREADER_H
