# SmartCity Pro — Part 6

SmartCity Pro is a C++17/Qt 6 Widgets university algorithms project. Part 6 preserves the map, routing, and infrastructure tools while adding an educational ambulance-dispatch simulation. It never contacts real emergency services.

## Implemented features

- Add, edit, drag, select, and delete city locations
- Stable integer IDs, names, coordinates, and six location categories
- Add, edit, select, and delete roads with stable IDs
- Road distance, travel time, construction cost, direction, and open/closed status
- One-way adjacency and arrows; two-way adjacency and labels
- Automatic deletion of connected roads when a location is removed
- Mouse-wheel zoom, middle-button drag panning, Fit Map, labels, tooltips, and legend
- Complete JSON save/load with atomic saves and safe validation before replacement
- Monotonic graph revision counter for future route-cache invalidation
- A fictional six-location, seven-road sample city, including one closed road
- Qt-free graph/model code based on standard C++ adjacency-list containers
- Dijkstra routing that minimizes either distance or travel time
- Reconstructed location and road ID paths with route totals
- Correct handling of closed roads, one-way roads, zero-weight edges, unreachable destinations, and start = destination
- BFS and DFS traversal demonstrations with directed reachability results
- A reusable, Qt-free algorithm-event format
- Non-blocking play, pause, single-step, reset, and speed-controlled animation
- Floyd–Warshall distance and travel-time matrices with next-hop/road reconstruction
- Background computation from immutable graph snapshots with stale-result rejection
- Results cached by graph revision and weight mode
- Labeled matrix cells that show `Unreachable` instead of a numeric sentinel
- Pair selection with reconstructed route highlighting on a map
- A separate, explicit undirected construction-candidate network (never inferred from travel roads)
- Candidate add, edit, delete, stable IDs, nonnegative costs, and JSON persistence
- Manual Prim and Kruskal minimum-spanning-forest implementations
- Disjoint Set Union with path compression and union by rank for Kruskal
- Animated candidate consideration, acceptance, rejection, and component starts
- Total construction cost, connected-component count, and side-by-side algorithm comparison
- Clear minimum-spanning-forest reporting for disconnected candidate networks
- Simulated ambulances with stable IDs, current graph locations, availability, and assignments
- Hospital resources with simulated bed counts and supported service categories
- Priority/time-ordered requests with explicit creation times, requirements, statuses, and timing results
- Travel-time Dijkstra selection for reachable ambulances and compatible hospitals
- Deterministic ambulance and hospital tie-breaking by stable ID/location ID
- Reservation of ambulances and beds before a journey starts
- Explicit simulation clock and request state machine
- Animated ambulance-to-patient and patient-to-hospital node-by-node journeys
- Graph-revision detection and rerouting at the next node after road changes
- Cancellation, unavailable-resource, unreachable-route, and failed-transport handling
- Atomic, validated dispatch-state JSON save/load

Delivery optimization, the general Algorithm Lab, and benchmarking remain placeholders for later parts.

## Project structure

```text
SmartCity/
├── CMakeLists.txt
├── README.md
├── samples/sample-city.json
├── samples/sample-dispatch.json
├── src/
│   ├── main.cpp
│   ├── core/models/
│   │   ├── CityLocation.h/.cpp
│   │   ├── ConstructionCandidate.h/.cpp
│   │   ├── EmergencyModels.h/.cpp
│   │   └── Road.h/.cpp
│   ├── algorithms/
│   │   ├── WeightedGraph.h/.cpp
│   │   ├── AlgorithmEvent.h
│   │   ├── RouteAlgorithms.h/.cpp
│   │   ├── FloydWarshall.h/.cpp
│   │   ├── DisjointSetUnion.h/.cpp
│   │   └── InfrastructureAlgorithms.h/.cpp
│   ├── services/
│   │   ├── CityRepository.h/.cpp
│   │   ├── CityJsonSerializer.h/.cpp
│   │   ├── EmergencyDispatchSimulation.h/.cpp
│   │   └── DispatchJsonSerializer.h/.cpp
│   └── gui/
│       ├── MainWindow.h/.cpp
│       ├── animation/AlgorithmAnimationController.h/.cpp
│       └── pages/
│           ├── DashboardPage.h/.cpp
│           ├── CityMapPage.h/.cpp
│           ├── RouteFinderPage.h/.cpp
│           ├── AllPairsRoutesPage.h/.cpp
│           ├── InfrastructurePage.h/.cpp
│           ├── EmergencyDispatchPage.h/.cpp
│           └── PlaceholderPage.h/.cpp
└── tests/core_tests.cpp
```

## Build on Windows with Qt Creator

1. Install Qt 6.5 or newer with Qt Creator, CMake, and one desktop kit. **MinGW 64-bit** is a straightforward choice. If you choose MSVC instead, install the matching Visual Studio C++ tools.
2. In Qt Creator, choose **File > Open File or Project** and open the top-level `CMakeLists.txt`.
3. Select the matching **Desktop Qt 6.x** kit and choose **Configure Project**.
4. Build with **Ctrl+B** and run with **Ctrl+R**.
5. Run the core tests from Qt Creator's Test Results pane, or use the command line below.

Keep the compiler and Qt package matched: use a MinGW compiler with a MinGW Qt kit, or the corresponding MSVC compiler with an MSVC Qt kit.

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

For a multi-configuration generator, append `--config Debug` to the build command and use `ctest --test-dir build -C Debug --output-on-failure`.

## Try the editor

1. Run SmartCity Pro and select **City Map** in the sidebar.
2. Drag a colored location with the left mouse button. Its connected roads follow it and the revision shown below the map increases.
3. Double-click a location or road to edit it. You can also single-click an item and choose **Edit selected**.
4. Use **+ Location** and **+ Road** to add data. IDs are generated by the graph and displayed as read-only values.
5. Select an item and choose **Delete selected**. Deleting a location shows how many connected roads and construction candidates will also be removed.
6. Use the mouse wheel to zoom, hold the middle mouse button to pan, and choose **Fit map** to frame all items.
7. Choose **Save JSON** to save the current city. Choose **Load JSON** and open `samples/sample-city.json` to restore the supplied example.

If a JSON file is malformed, has duplicate IDs, missing endpoints, unknown categories/directions, non-finite values, negative road weights, or self-loop roads, loading is rejected and the current city remains unchanged.

## Try the Route Finder

1. Select **Route Finder** in the sidebar. Its selectors and drawing refresh from the graph whenever the graph revision changes.
2. Choose start and destination locations, choose distance or travel time, and select **Find shortest route**.
3. The result panel shows the ordered location IDs/names, the road IDs used, and the total in kilometres or minutes.
4. Use **Play**, **Pause**, **Step**, and **Reset** to inspect the event sequence. Change the speed slider at any time.
5. Choose **Run BFS** or **Run DFS** for directed reachability demonstrations. Closed roads are ignored in all three algorithms.

BFS can minimize the number of edges in an unweighted graph. It does not minimize weighted distance, travel time, or construction cost. DFS does not guarantee a shortest path.

## Try the All-Pairs Route Planner

1. Select **All-Pairs Routes** and choose distance or travel-time mode.
2. Select **Compute matrix**. Work runs on an immutable graph copy, so the GUI remains responsive.
3. Matrix rows are sources and columns are destinations. The diagonal is zero and disconnected pairs say **Unreachable**.
4. Choose a source and destination to reconstruct and highlight that pair's route.
5. Switching back to an already-computed mode uses the cached result while the graph revision is unchanged.

Full matrix visualization is limited to 40 locations to keep the table readable. Pair routing remains available above that size, while computation itself is capped at 250 locations to prevent accidental cubic workloads. Editing or loading the city changes its graph revision and invalidates cached results. If the graph changes while a worker is running, its stale result is discarded.

## Try the Infrastructure Planner

1. Select **Infrastructure** in the sidebar. The drawing shows only explicit undirected construction candidates, not travel roads.
2. Choose **+ Candidate** to select two city locations and enter a nonnegative construction cost. Select a row to edit or delete it.
3. Choose **Prim** or **Kruskal**, then select **Build minimum forest**. The result shows selected-edge count, total cost, and connected components.
4. Use **Play**, **Pause**, **Step**, **Reset**, and the speed slider to inspect the shared event stream.
5. Choose **Compare both** to verify that both algorithms produce the same total cost and component count on identical input. Equal-cost ties can produce different valid edge sets.
6. Use **City Map → Save JSON** or **Load JSON** to persist or restore both the travel graph and construction candidates.

If the candidate network is disconnected, the planner returns a minimum spanning forest and explicitly reports that a single connected network is impossible with the available candidates.

## Try the Emergency Dispatch Simulator

1. Select **Emergency Dispatch**. The supplied city starts with two available simulated ambulances and three simulated beds at Northside Hospital.
2. Use **+ Ambulance** and **+ Hospital** to edit simulation resources. Hospital resources can only be attached to locations whose category is `hospital`.
3. Use **+ Request** to select a patient location, simulated priority, and required service. Creation time comes from the explicit simulation clock.
4. Choose **Dispatch next**. Requests are ordered critical, medium, then low; equal priorities use earlier creation time, then lower request ID.
5. The simulator runs travel-time Dijkstra from every available ambulance. The fastest reachable ambulance wins; equal times use the lower ambulance ID. A reachable hospital with the requested service and a free simulated bed is selected by travel time, then location ID.
6. Use **Play**, **Pause**, or **Step to next node** to animate the selected active request. The clock advances by each traversed road's simulated travel time.
7. Select a request row and choose **Cancel request** to cancel it. Active reservations are released safely. Terminal history records can be removed with **Delete record** when their city references are no longer needed.
8. Use **Save simulation** and **Load simulation** for atomic dispatch JSON persistence. `samples/sample-dispatch.json` contains a queued fictional example.

State transitions are `queued → to-patient → to-hospital → completed`. Cancellation is allowed while queued or active. If no ambulance, patient route, or suitable hospital is currently available, the request remains queued with an explanation. If road changes make the patient unreachable at a node, the request is requeued and reservations are released. If this occurs after pickup and no hospital route remains, the simulated request becomes `failed`.

Response time is measured from request creation on the simulation clock until the ambulance reaches the patient, so it includes simulated queue waiting. Transport time accumulates the travel time of roads traversed after pickup.

Road changes never interrupt the ambulance partway along an edge. The simulator finishes the current discrete edge movement, detects the changed graph revision at the next node, and reruns Dijkstra before entering another edge.

## Architecture notes

- `WeightedGraph` is the source of truth. It owns locations, travel roads, explicit construction candidates, ID generation, directed travel adjacency lists, validation, connected-record cleanup, and the graph revision.
- `CityMapPage` turns graph records into graphics items. Dragging writes coordinates to the graph first, then redraws every incident road.
- `CityJsonSerializer` uses Qt's JSON classes at the service boundary. It parses into a temporary `WeightedGraph`; only a fully valid graph replaces the live one.
- `CityRepository` constructs the default fictional city. It does not keep a second copy of graph data.
- Location and road models, and the graph implementation, do not include or depend on Qt.
- `RouteAlgorithms` contains Dijkstra, BFS, and DFS and depends only on standard C++ and `WeightedGraph`.
- `AlgorithmEvent` represents visits, explored edges, distance updates, and final-path elements without any GUI types.
- `AlgorithmAnimationController` replays precomputed events with `QTimer`; it never blocks the GUI event loop.
- `FloydWarshall` initializes zero diagonals and infinity for other pairs, consumes only open directed adjacency entries, and maintains next-hop and next-road matrices.
- `AllPairsRoutesPage` uses Qt Concurrent only at the GUI boundary. The worker receives a complete graph copy and never touches widgets or mutable live data.
- `InfrastructureAlgorithms` is Qt-free. Prim builds an undirected adjacency structure only from construction candidates; Kruskal sorts those same candidates and uses `DisjointSetUnion`.
- `InfrastructurePage` edits candidate records and replays the algorithm's precomputed events with the same timer-based animation controller used by Route Finder.
- `EmergencyDispatchSimulation` is a Qt-free state machine. It owns the clock, resources, requests, reservations, planned legs, and node-level transitions while reusing the Qt-free Dijkstra implementation.
- `EmergencyDispatchPage` uses a GUI timer to advance one graph edge without blocking the event loop. It renders current ambulance nodes and the most recent journey edge.
- `DispatchJsonSerializer` writes atomically and parses into temporary model vectors. The live simulation is replaced only after all references and reservation invariants validate against the current city graph.

## Complexity

With `V` locations and `E` directed adjacency entries, Dijkstra uses an adjacency list and binary min-priority queue. Its time complexity is `O((V + E) log V)`, commonly written `O(E log V)` for a connected graph. Distances, parents, and settled state use `O(V)` space. This implementation uses lazy priority-queue updates, so queued entries can use `O(E)` space; including recorded animation events, auxiliary space is `O(V + E)`.

A two-way road contributes two directed adjacency entries, while a one-way road contributes one. Closed roads remain in the model but are skipped by routing and traversal. BFS and DFS each run in `O(V + E)` time and use `O(V)` traversal storage, in addition to the graph and recorded events.

Floyd–Warshall uses `O(V³)` time because it considers every `(source, intermediate, destination)` combination. Its weight, next-hop, and next-road matrices each use `O(V²)` space, so total asymptotic space is `O(V²)`. Before adding two partial weights, the implementation rejects infinity operands and values that would exceed `double`'s finite maximum.

For `V` locations and `C` construction candidates, this Prim implementation uses a binary min-priority queue and runs in `O((V + C) log C)` time with `O(V + C)` auxiliary storage. Kruskal sorts candidates in `O(C log C)` time; its path-compressing, union-by-rank DSU operations take near-constant amortized time (`O(α(V))` each) and use `O(V)` storage. Recorded animation events add `O(V + C)` space.

An MST minimizes the **sum of selected construction costs** needed to connect all locations when connection is possible. It does not minimize the distance or travel time between particular pairs: an inexpensive tree can force a long detour. Shortest-route questions remain the job of Dijkstra or Floyd–Warshall on the travel-road graph.

For `A` available ambulances and `H` eligible hospitals, a dispatch can run up to `A + H` travel-time Dijkstra searches. With `V` locations and `E` directed adjacency entries, the worst-case dispatch time is `O((A + H)(V + E) log V)` and the temporary routing storage is `O(V + E)` per sequential search.

The dispatch rule is deliberately greedy: it chooses the fastest ambulance and hospital for the next highest-priority request using current information. That is a local decision. It does not minimize total response time, missed coverage, or resource usage across several present or future requests; globally optimal multi-request assignment requires a different optimization model.

## JSON format

The root object contains `schemaVersion`, informational `graphRevision`, `locations`, `roads`, and `constructionCandidates`. Older schema-version-1 files without the optional candidate array still load with an empty candidate network. Categories use these exact strings: `intersection`, `hospital`, `university`, `market`, `residential area`, and `ambulance station`. Road direction is `one-way` or `two-way`. A construction candidate stores `id`, `first`, `second`, and `constructionCost` and is always interpreted as undirected.

Dispatch data uses a separate schema-version-1 JSON document containing `simulationClockMinutes`, `ambulances`, `hospitals`, and `requests`. Keeping it separate allows a city layout to be reused with different simulation scenarios. Loading rejects missing locations, duplicate IDs, invalid states, unknown enum values, inconsistent assignments, and malformed reservation records without changing the current simulation.

## Verification note

See the delivery response for the checks performed in the creation environment. A Qt 6 desktop kit is required for a full GUI compile and runtime test.
