# Airgorithm - Flight Route Optimizer

A C++ flight route planner that finds the fastest path between airports using graph algorithms. Compares Dijkstra's and Bellman-Ford algorithms on real-world flight data.

## Project Overview

- **Data**: 100K+ flight routes across 7,698 airports from OpenFlights database
- **Algorithms**: Dijkstra's and Bellman-Ford for shortest path finding
- **Data Structures**: Graph with adjacency list, hash map for O(1) airport lookups
- **Frontend**: SFML-based visualizer with interactive world map

## Features

- Find fastest flight routes between any two airports
- Visual comparison of algorithm performance (execution time)
- Interactive GUI showing airports on a world map
- Displays total flight time, number of stops, and path

## Running the Code

### Prerequisites

- C++ compiler (MinGW or Visual Studio)
- CMake
- SFML 2.5.1

### Build & Run

```bash
# Navigate to project directory
cd Airgorithm

# Create build directory
mkdir build
cd build

# Configure and build
cmake -G "MinGW Makefiles" ..
cmake --build .

# Run the application
./Airgorithm.exe
```

### Using the Application

1. Enter source airport code (e.g., "JFK")
2. Enter destination airport code (e.g., "LAX")
3. Click "Run Algorithms"
4. View results comparing both algorithms

## Big O Complexity

- **Dijkstra's Algorithm**: O((V + E) log V)
- **Bellman-Ford Algorithm**: O(V × E)
- **Airport Lookup**: O(1) average case (hash map)

## Data Sources

- Routes: `data/routes_with_estimated_times_plus_33k.csv`
- Airports: `data/airports.dat`
