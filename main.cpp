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

    string csv_path;
    //this is the name of the file: routes_with_estimated_times.csv
    cout << "Enter the name of the routes file: " << endl;
    cin >> csv_path;

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

    // for testing purposes: listing the number of edges for an airport and printing 20 of them
    string source_airport;
    string dest_airport;
    cout << "Enter the code of source airport: " << endl;
    cin >> source_airport;
    cout << "Enter the code of destination airport: " << endl;
    cin >> dest_airport;
    G.printSampleEdges(source_airport, 20);

    // test minimum edge between two airports
    double best = G.getFastestDirectTime(source_airport, dest_airport);
    if (!isfinite(best)) {
        cout << "No direct route found from " << source_airport << " to " << dest_airport << ".\n";
    } else {
        cout << fixed << setprecision(2)
             << "Fastest direct " << source_airport << " -> " << dest_airport
             << " estimated time: " << best << " hr\n";
    }

    return 0;
}
