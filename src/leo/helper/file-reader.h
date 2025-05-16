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
    std::uint16_t dataRateIslMpbs = 1000;
    std::uint16_t dataRateFeederMpbs = 100;

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
        // routes -change to list of strings for backup paths
        std::unordered_map<std::string, std::vector<std::string>> table_data;
    };

    struct ConstellationEvent {
        enum Action { LINK_UP, LINK_DOWN };
        std::string from;
        std::string to;
        double weight;
        Action action;
    };
    struct Traffic {
        double start_time;
        std::string src_node_id;
        std::string dst_node_id;
        int packet_size;
        double duration;
        std::string rate;
        std::string protocol;
        int src_port;
        int dst_port;
    };

    // Constructor
    FileReader() = default;

    //Helper
    std::chrono::_V2::system_clock::time_point parseTimestampToTimePoint(const std::string& timestampStr);
    double secondsSinceStart(const std::tm& t, const std::tm& start);
    std::vector<std::string> GetFileNamesInFolder(const std::string& foldername);
    int ExtractFailureNumber(const std::string& fileName);
    //Graph
    void readGraphFromJson(const std::string& filename);

    // Switching tables
    void readSwitchingTableFromJson(const std::string& filename);
    void readAllSwitchingTablesFromFolder(const std::string& foldername);

    // Events & Failures
    void readConstellationEvents(const std::string& filename, std::chrono::_V2::system_clock::time_point& startTimeStr, bool failures);

    // Traffic
    void readTrafficFromJson(const std::string& filename);

    // Dynamic Edges
    void readDynamicEdgesFromJson(const std::string& filename, std::chrono::_V2::system_clock::time_point& simulationStartTime);
    void readDynamicEdgesFromFolder(const std::string& foldername, std::chrono::_V2::system_clock::time_point& simulationStartTime);


    // Getters for private members
    const std::vector<std::unique_ptr<Node>>& GetNodes() const { return nodes; }
    const std::vector<Edge>& GetEdges() const { return edges; }
    const std::unordered_map<std::string, Node*>& GetNodeMap() const { return node_map; }
    std::vector<RawSwitchingTable>& GetRawSwitchingTables() { return raw_switching_tables; }
    const std::map<double, std::vector<ConstellationEvent>>& GetConstellationEvents() const { return constellation_events_map; }
    const std::map<double, std::vector<ConstellationEvent>>& GetFailures() const { return constellation_failures_map; }
    std::map<std::pair<std::string, std::string>, double> GetAllUniqueLinks() const;
    const std::vector<Traffic>& GetTraffic() const { return traffic_vector; }
    const std::map<std::pair<double, double>, std::vector<Edge>>& GetEdgesByValidityPeriod() const { return edges_by_validity_period; }

    // Visulization
    void printGraph() const;
    void printSwitchtingTables() const;
    void printConstellationEvents() const;
    void printDynamicEdges() const;

private:
    // Vectors to hold nodes and edges
    std::vector<std::unique_ptr<Node>> nodes;
    std::vector<Edge> edges;
    std::vector<RawSwitchingTable> raw_switching_tables;
    std::map<double, std::vector<ConstellationEvent>> constellation_events_map;
    std::map<double, std::vector<ConstellationEvent>> constellation_failures_map;
    std::vector<Traffic> traffic_vector;
    std::map<std::pair<double, double>, std::vector<Edge>> edges_by_validity_period;

    // Map for quick node access by ID
    std::unordered_map<std::string, Node*> node_map;
};

}  // namespace leo
} // namespace ns3

#endif // FILEREADER_H
