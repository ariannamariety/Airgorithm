// Sema Akkaya
// 10/26/2025

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <queue>
#include <iomanip>
#include <limits>
#include <cmath>
#include <algorithm>
#include <chrono>
using namespace std;

// "\N" or empty means "missing" (common in OpenFlights-style data).
static inline bool missing(const string& s) {
    return s.empty() || s == "\\N";
}

// reads each line
static vector<string> parseCsvLine(const string& line) {
    vector<string> out;
    string cur;
    bool inq = false;
    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (c == '"') {
            if (inq && i + 1 < line.size() && line[i + 1] == '"') {
                cur.push_back('"'); // escaped quote
                ++i;
            } else {
                inq = !inq;
            }
        } else if (c == ',' && !inq) {
            out.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    out.push_back(cur);
    return out;
}

static int parseIntOr(const string& s, int fallback) {
    if (missing(s))
        return fallback;
    try {
        return stoi(s);
    } catch (...) {
        return fallback;
    }
}

static double parseDoubleOr(const string& s, double fallback) {
    if (missing(s))
        return fallback;
    try {
        return stod(s);
    } catch (...) {
        return fallback;
    }
}

// Directed edge (u -> v). Stored inside the source Airport's node adjacency list.
struct Edge {
    int dest_index;            // Destination airport index
    string airline;            // Airline code
    int airline_id;            // Airline numeric ID
    int stops = 0;             // Usually 0 for routes
    string equipment;          // Aircraft codes
    bool codeshare = false;    // True if "Y"
    double est_time_hr;        // Edge weight which is the estimated arrival time. This is calculated by the formula 30 mins + 1 hour per 500 miles (https://openflights.org/faq)
};

// Airport node. Adjacency list = vector<Edge>.
struct Airport {
    string code;                    // IATA/ICAO code (graph key)
    int openflights_id = -1;        // Optional Airport_ID
    vector<Edge> edges;             // Outgoing edges (adjacency list)
};

//stores all nodes (airports) and their edges (flight to destination)
class FlightGraph {
public:
    vector<Airport> airports;       // All airports are stored in this vector

    // read the data from the file to create all the nodes and edges
    // A row is skipped only if a source or destination CODE is missing.
    bool loadFromEstimatedCSV(const string& routes_csv_path) {
        ifstream fin(routes_csv_path);
        if (!fin) {
            cerr << "Error: cannot open " << routes_csv_path << "\n";
            return false;
        }

        string header;
        //return if the file is empty
        if (!getline(fin, header)) {
            cerr << "Error: CSV file is empty.\n";
            return false;
        }

        size_t added = 0, skipped = 0;
        string line;
        while (getline(fin, line)) {
            if (line.empty())
                continue;
            auto cols = parseCsvLine(line);

            // Extract needed fields
            const string& airline      = cols[0];
            const string& airline_id_s = cols[1];
            const string& src_code     = cols[2];
            const string& src_id_s     = cols[3];
            const string& dst_code     = cols[4];
            const string& dst_id_s     = cols[5];
            const string& codeshare_s  = cols[6];
            const string& stops_s      = cols[7];
            const string& equipment    = cols[8];
            const string& est_time_s   = cols[9];

            // Require codes; IDs are optional (\N are still read)
            if (missing(src_code) || missing(dst_code)) {
                skipped++;
                continue;
            }

            // Parse values
            const int airline_id = parseIntOr(airline_id_s, -1);
            const int src_id = parseIntOr(src_id_s, -1);
            const int dst_id = parseIntOr(dst_id_s, -1);
            const bool codeshare = (!missing(codeshare_s) && (codeshare_s == "Y" || codeshare_s == "y"));
            const int stops = parseIntOr(stops_s, 0);
            const double est_time_hr = parseDoubleOr(est_time_s, numeric_limits<double>::quiet_NaN());

            // Create/fetch airports by code
            const int sidx = getOrCreateAirportIndexByCode(src_code);
            const int didx = getOrCreateAirportIndexByCode(dst_code);
            if (sidx < 0 || didx < 0) {
                skipped++;
                continue;
            }

            // Update airport IDs if present and not set yet
            if (src_id >= 0 && airports[sidx].openflights_id < 0)
                airports[sidx].openflights_id = src_id;
            if (dst_id >= 0 && airports[didx].openflights_id < 0)
                airports[didx].openflights_id = dst_id;

            // Append the directed edge to the source's adjacency list (vector<Edge>)
            Edge e;
            e.dest_index   = didx;
            e.airline      = airline;
            e.airline_id   = airline_id;
            e.stops        = stops;
            e.equipment    = equipment;
            e.codeshare    = codeshare;
            e.est_time_hr  = est_time_hr;

            airports[sidx].edges.push_back(std::move(e));
            added++;
        }

        return added > 0;
    }

    // Prints up to `max_edges` outgoing edges for a given airport code for test purposes
    void printSampleEdges(const string& code, size_t max_edges) const {
        const int idx = findAirportIndexByCode(code);
        if (idx < 0) {
            cout << "Airport not found: " << code << "\n";
            return;
        }
        const Airport& A = airports[idx];
        cout << "Airport " << A.code
             << " Airline_ID=" << A.openflights_id
             << " — # of outgoing edges: " << A.edges.size() << "\n";

        for (int i = 0; i < A.edges.size() && i < max_edges; ++i) {
            const Edge& e = A.edges[i];
            const Airport& B = airports[e.dest_index];
            cout << "  -> " << B.code
                 << "  airline=" << e.airline
                 << "  ID=" << e.airline_id
                 << "  stops=" << e.stops
                 << "  equip=" << e.equipment
                 << "  codeshare=" << (e.codeshare ? "Y" : "N")
                 << fixed << setprecision(2)
                 << "  est_time_hr=" << (std::isnan(e.est_time_hr) ? -1.0 : e.est_time_hr)
                 << "\n";
        }
    }

    // returns the fastest direct time from src->dst among parallel edges
    double getFastestDirectTime(const string& src_code, const string& dst_code) const {
        const int sidx = findAirportIndexByCode(src_code);
        const int didx = findAirportIndexByCode(dst_code);
        if (sidx < 0) {
            cout << "Source not found: " << src_code << "\n";
            exit(0);
        }
        if (didx < 0) {
            cout << "Destination not found: " << dst_code << "\n";
            exit(0);
        }

        double best = numeric_limits<double>::infinity(); //assign to positive infinity, the highest

        for (int i = 0; i < airports[sidx].edges.size(); ++i) {
            const Edge& e = airports[sidx].edges[i];
            if (e.dest_index == didx && !std::isnan(e.est_time_hr))
                best = min(best, e.est_time_hr);
        }

        return best;
    }

    // dijkstra's algorithm
    pair<double, vector<string>> dijkstra(const string& source_code, const string& destination_code) const {
        int source_idx = findAirportIndexByCode(source_code);
        int dest_idx = findAirportIndexByCode(destination_code);

        vector<string> empty_path;
        if (source_idx < 0 || dest_idx < 0) {
            return {numeric_limits<double>::infinity(), empty_path};
        }

        int num_airports = airports.size();
        vector<double> distance_array(num_airports, numeric_limits<double>::infinity());
        vector<int> parent_array(num_airports, -1);
        vector<bool> visited_array(num_airports, false);

        priority_queue<pair<double, int>, vector<pair<double, int>>, greater<pair<double, int>>> pq;

        distance_array[source_idx] = 0.0;
        pq.push(make_pair(0.0, source_idx));

        while (!pq.empty()) {
            pair<double, int> top = pq.top();
            pq.pop();
            double current_dist = top.first;
            int current_node = top.second;

            if (visited_array[current_node]) {
                continue;
            }
            visited_array[current_node] = true;

            if (current_node == dest_idx) {
                break;
            }

            for (int i = 0; i < airports[current_node].edges.size(); i++) {
                Edge current_edge = airports[current_node].edges[i];
                int neighbor = current_edge.dest_index;
                double edge_weight = current_edge.est_time_hr;

                if (std::isnan(edge_weight) || edge_weight < 0) {
                    continue;
                }

                // relaxation
                if (!visited_array[neighbor] && distance_array[current_node] + edge_weight < distance_array[neighbor]) {
                    distance_array[neighbor] = distance_array[current_node] + edge_weight;
                    parent_array[neighbor] = current_node;
                    pq.push(make_pair(distance_array[neighbor], neighbor));
                }
            }
        }

        if (distance_array[dest_idx] == numeric_limits<double>::infinity()) {
            return {numeric_limits<double>::infinity(), empty_path};
        }

        // build path
        vector<string> path_result;
        int current = dest_idx;
        while (current != -1) {
            path_result.push_back(airports[current].code);
            current = parent_array[current];
        }
        reverse(path_result.begin(), path_result.end());

        return {distance_array[dest_idx], path_result};
    }

    // bellman-ford algorithm
    pair<double, vector<string>> bellmanFord(const string& source_code, const string& destination_code) const {
        int source_idx = findAirportIndexByCode(source_code);
        int dest_idx = findAirportIndexByCode(destination_code);

        vector<string> empty_path;
        if (source_idx < 0 || dest_idx < 0) {
            return {numeric_limits<double>::infinity(), empty_path};
        }

        int num_airports = airports.size();
        vector<double> distance_array(num_airports, numeric_limits<double>::infinity());
        vector<int> parent_array(num_airports, -1);

        distance_array[source_idx] = 0.0;

        // relax edges v-1 times
        for (int iteration = 0; iteration < num_airports - 1; iteration++) {
            bool any_update = false;

            for (int current_node = 0; current_node < num_airports; current_node++) {
                if (distance_array[current_node] == numeric_limits<double>::infinity()) {
                    continue;
                }

                for (int j = 0; j < airports[current_node].edges.size(); j++) {
                    Edge current_edge = airports[current_node].edges[j];
                    int neighbor_node = current_edge.dest_index;
                    double edge_weight = current_edge.est_time_hr;

                    if (std::isnan(edge_weight) || edge_weight < 0) {
                        continue;
                    }

                    if (distance_array[current_node] + edge_weight < distance_array[neighbor_node]) {
                        distance_array[neighbor_node] = distance_array[current_node] + edge_weight;
                        parent_array[neighbor_node] = current_node;
                        any_update = true;
                    }
                }
            }
            if (!any_update) {
                break;
            }
        }

        if (distance_array[dest_idx] == numeric_limits<double>::infinity()) {
            return {numeric_limits<double>::infinity(), empty_path};
        }

        // build path
        vector<string> path_result;
        int current = dest_idx;
        while (current != -1) {
            path_result.push_back(airports[current].code);
            current = parent_array[current];
        }
        reverse(path_result.begin(), path_result.end());

        return {distance_array[dest_idx], path_result};
    }

private:
    // Maps airport_CODE -> index in Airports vector
    unordered_map<string,int> code_to_index;

    // Return existing airport index by CODE, or create a new node.
    int getOrCreateAirportIndexByCode(const string& code) {
        auto it = code_to_index.find(code);
        if (it != code_to_index.end()) //if found
            return it->second;

        //if not in the vector then add it
        Airport ap;
        ap.code = code;

        const int idx = (int)airports.size();
        airports.push_back(std::move(ap));
        code_to_index[code] = idx;
        return idx;
    }

    // Find airport index by CODE; -1 if not found.
    int findAirportIndexByCode(const string& code) const {
        auto it = code_to_index.find(code);
        if (it != code_to_index.end())
            return it->second;
        return -1;
    }
};


int main() {

    string csv_path = "data/routes_with_estimated_times_plus_33k.csv";

    cout << "Loading flight data from " << csv_path << "...\n";

    FlightGraph G;
    //read everything from the file and load to our graph G
    if (!G.loadFromEstimatedCSV(csv_path)) {
        cerr << "No edges loaded — check the CSV path/format.\n";
        return 0;
    }

    // just to check that all edges were captured
    int total_edge_count = 0;
    for (int i = 0; i < G.airports.size(); ++i) {
        total_edge_count += G.airports[i].edges.size();
    }
    cout << "Graph ready. Airports: " << G.airports.size()
         << " | Edges: " << total_edge_count << "\n";

    string source_airport;
    string dest_airport;
    cout << "\n--- Flight Planner ---\n";
    cout << "Enter source airport code: ";
    cin >> source_airport;
    cout << "Enter destination airport code: ";
    cin >> dest_airport;

    for (int i = 0; i < source_airport.length(); i++) {
        source_airport[i] = toupper(source_airport[i]);
    }
    for (int i = 0; i < dest_airport.length(); i++) {
        dest_airport[i] = toupper(dest_airport[i]);
    }

    cout << "\nFinding fastest route from " << source_airport << " to " << dest_airport << "...\n\n";

    // dijkstra
    cout << "Running Dijkstra's Algorithm...\n";
    chrono::high_resolution_clock::time_point dijkstra_start = chrono::high_resolution_clock::now();
    pair<double, vector<string>> dijkstra_result = G.dijkstra(source_airport, dest_airport);
    chrono::high_resolution_clock::time_point dijkstra_end = chrono::high_resolution_clock::now();

    double dijkstra_time = dijkstra_result.first;
    vector<string> dijkstra_path = dijkstra_result.second;
    long long dijkstra_duration = chrono::duration_cast<chrono::milliseconds>(dijkstra_end - dijkstra_start).count();

    if (dijkstra_path.empty() || !isfinite(dijkstra_time)) {
        cout << "No route found!\n";
    } else {
        cout << "Path found: ";
        for (int i = 0; i < dijkstra_path.size(); i++) {
            cout << dijkstra_path[i];
            if (i < dijkstra_path.size() - 1) {
                cout << " -> ";
            }
        }
        cout << "\n";
        cout << fixed << setprecision(2);
        cout << "Total time: " << dijkstra_time << " hours\n";
        cout << "Stops: " << (dijkstra_path.size() - 1) << "\n";
    }
    cout << "Time taken: " << dijkstra_duration << " ms\n\n";

    // bellman-ford
    cout << "Running Bellman-Ford Algorithm...\n";
    chrono::high_resolution_clock::time_point bellman_start = chrono::high_resolution_clock::now();
    pair<double, vector<string>> bellman_result = G.bellmanFord(source_airport, dest_airport);
    chrono::high_resolution_clock::time_point bellman_end = chrono::high_resolution_clock::now();

    double bellman_time = bellman_result.first;
    vector<string> bellman_path = bellman_result.second;
    long long bellman_duration = chrono::duration_cast<chrono::milliseconds>(bellman_end - bellman_start).count();

    if (bellman_path.empty() || !isfinite(bellman_time)) {
        cout << "No route found!\n";
    } else {
        cout << "Path found: ";
        for (int i = 0; i < bellman_path.size(); i++) {
            cout << bellman_path[i];
            if (i < bellman_path.size() - 1) {
                cout << " -> ";
            }
        }
        cout << "\n";
        cout << fixed << setprecision(2);
        cout << "Total time: " << bellman_time << " hours\n";
        cout << "Stops: " << (bellman_path.size() - 1) << "\n";
    }
    cout << "Time taken: " << bellman_duration << " ms\n\n";

    if (!dijkstra_path.empty() && !bellman_path.empty()) {
        cout << "Both algorithms found routes with " << fixed << setprecision(2)
             << dijkstra_time << " hours total time.\n";
        cout << "Dijkstra took " << dijkstra_duration << " ms\n";
        cout << "Bellman-Ford took " << bellman_duration << " ms\n";
    }

    return 0;
}
