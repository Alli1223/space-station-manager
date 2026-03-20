#include "shared/station_generator.h"
#include <iostream>
#include <cmath>
#include <algorithm>
#include <climits>
#include <queue>

namespace ssm {

// =========================================================================
// Grid helpers
// =========================================================================

void StationGenerator::initGrid(int w, int h) {
    gridWidth = w;
    gridHeight = h;
    grid.assign(w * h, CellType::EMPTY);
    rooms.clear();
}

CellType StationGenerator::getGrid(int x, int y) const {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight) return CellType::EMPTY;
    return grid[y * gridWidth + x];
}

void StationGenerator::setGrid(int x, int y, CellType type) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight) return;
    grid[y * gridWidth + x] = type;
}

bool StationGenerator::isAreaClear(int x, int y, int w, int h) const {
    for (int cy = y; cy < y + h; cy++) {
        for (int cx = x; cx < x + w; cx++) {
            if (cx < 0 || cx >= gridWidth || cy < 0 || cy >= gridHeight) return false;
            if (getGrid(cx, cy) != CellType::EMPTY) return false;
        }
    }
    return true;
}

int StationGenerator::randInt(int minVal, int maxVal) {
    std::uniform_int_distribution<int> dist(minVal, maxVal);
    return dist(rng);
}

// =========================================================================
// Room placement
// =========================================================================

void StationGenerator::placeRoomWalls(const PlacedRoom& room) {
    // Top and bottom walls
    for (int x = room.x; x < room.x + room.width; x++) {
        setGrid(x, room.y, CellType::WALL);
        setGrid(x, room.y + room.height - 1, CellType::WALL);
    }
    // Left and right walls
    for (int y = room.y; y < room.y + room.height; y++) {
        setGrid(room.x, y, CellType::WALL);
        setGrid(room.x + room.width - 1, y, CellType::WALL);
    }
    // Fill interior with floor
    for (int y = room.interiorY(); y < room.interiorY() + room.interiorH(); y++) {
        for (int x = room.interiorX(); x < room.interiorX() + room.interiorW(); x++) {
            setGrid(x, y, CellType::FLOOR);
        }
    }
}

bool StationGenerator::tryPlaceRoom(const RoomDef& def, int belowY) {
    // Try multiple random positions, biased toward center-X
    for (int attempt = 0; attempt < 150; attempt++) {
        int interiorW = randInt(def.minWidth, def.maxWidth);
        int interiorH = randInt(def.minHeight, def.maxHeight);
        int totalW = interiorW + 2; // +2 for walls
        int totalH = interiorH + 2;

        // Bias X toward center, widening range on later attempts
        int centerBias = gridWidth / 2;
        int spread = (attempt < 80) ? 20 : 30;
        int rx = centerBias + randInt(-spread, spread);
        rx = rx - totalW / 2;

        // Y: below the landing bay
        int ySpread = (attempt < 80) ? 18 : 25;
        int ry = belowY + randInt(1, ySpread);

        // Clamp to grid
        if (rx < 1) rx = 1;
        if (ry < belowY) ry = belowY;
        if (rx + totalW >= gridWidth - 1) rx = gridWidth - totalW - 1;
        if (ry + totalH >= gridHeight - 1) ry = gridHeight - totalH - 1;

        // Check clearance (1-cell buffer for first 50 attempts, then 0)
        int buffer = (attempt < 50) ? 1 : 0;
        if (isAreaClear(rx - buffer, ry - buffer, totalW + 2 * buffer, totalH + 2 * buffer)) {
            PlacedRoom room;
            room.type = def.type;
            room.x = rx;
            room.y = ry;
            room.width = totalW;
            room.height = totalH;
            placeRoomWalls(room);
            rooms.push_back(room);
            return true;
        }
    }
    return false;
}

// =========================================================================
// Landing bay — large hangar with 4 landing pads + hangar door
// =========================================================================

void StationGenerator::placeLandingBay() {
    // The landing bay is a large room anchored to the top of the station.
    // Ships fly in from above through HANGAR_DOOR cells on the north wall,
    // and land on LANDING_PAD areas inside the bay.
    // Row 0-1 are EMPTY (ship approach zone above station).
    // The bay starts at row 2.

    int bayInteriorW = 32;
    int bayInteriorH = 12;
    int bayW = bayInteriorW + 2; // +2 for walls
    int bayH = bayInteriorH + 2;
    int bayX = (gridWidth - bayW) / 2;
    int bayY = 2; // rows 0-1 empty for ships

    PlacedRoom bay;
    bay.type = RoomType::LANDING_BAY;
    bay.x = bayX;
    bay.y = bayY;
    bay.width = bayW;
    bay.height = bayH;
    placeRoomWalls(bay);

    // Replace north wall with HANGAR_DOOR cells (keep 2 wall cells on each corner)
    for (int x = bayX + 2; x < bayX + bayW - 2; x++) {
        setGrid(x, bayY, CellType::HANGAR_DOOR);
    }

    // Place 4 landing pads inside the bay.
    // Each pad is 5 cells wide × 6 cells tall (fits LARGE ship: 5×6 cells).
    // Layout (within interior, y=0 is first interior row):
    //   y=0..1: Floor (approach clearance from hangar door to pads)
    //   y=2..7: Landing pads (6 rows)
    //   y=8..11: Floor (walkway for cargo access below pads)
    //
    // Pad X positions (within interior):
    //   Pad 0: x=3..7    Pad 1: x=10..14
    //   Pad 2: x=17..21  Pad 3: x=24..28
    // (3-cell edge margins, 2-cell walkways between pads)

    int padWidth = 5;
    int padHeight = 6;
    int padStartY = bay.interiorY() + 2; // 2 rows of floor below hangar door
    int padXPositions[] = { 3, 10, 17, 24 };

    for (int p = 0; p < 4; p++) {
        int padX = bay.interiorX() + padXPositions[p];
        for (int py = padStartY; py < padStartY + padHeight; py++) {
            for (int px = padX; px < padX + padWidth; px++) {
                setGrid(px, py, CellType::LANDING_PAD);
            }
        }
    }

    rooms.push_back(bay);
}

// =========================================================================
// Room furnishing
// =========================================================================

void StationGenerator::furnishRoom(const PlacedRoom& room) {
    switch (room.type) {
        case RoomType::STORAGE:        furnishStorage(room); break;
        case RoomType::COMMAND:        furnishCommand(room); break;
        case RoomType::MESS_HALL:      furnishMessHall(room); break;
        case RoomType::ENGINEERING:    furnishEngineering(room); break;
        case RoomType::MEDBAY:         furnishMedbay(room); break;
        case RoomType::CREW_QUARTERS:  furnishCrewQuarters(room); break;
        case RoomType::REFINERY:       furnishRefinery(room); break;
        default: break; // landing bay already furnished
    }
}

void StationGenerator::furnishStorage(const PlacedRoom& room) {
    // Fill interior with R cells, leaving 1-cell walkway around edges
    for (int y = room.interiorY() + 1; y < room.interiorY() + room.interiorH() - 1; y++) {
        for (int x = room.interiorX() + 1; x < room.interiorX() + room.interiorW() - 1; x++) {
            setGrid(x, y, CellType::STORAGE);
        }
    }
}

void StationGenerator::furnishCommand(const PlacedRoom& room) {
    // Terminals along the north interior wall
    int termY = room.interiorY();
    for (int x = room.interiorX() + 1; x < room.interiorX() + room.interiorW() - 1; x += 2) {
        setGrid(x, termY, CellType::TERMINAL);
    }

    // Spawn points in center
    int cx = room.centerX();
    int cy = room.centerY();
    setGrid(cx, cy, CellType::SPAWN_POINT);
    setGrid(cx - 1, cy, CellType::SPAWN_POINT);
    if (room.interiorW() > 5) {
        setGrid(cx + 1, cy, CellType::SPAWN_POINT);
    }
}

void StationGenerator::furnishMessHall(const PlacedRoom& room) {
    // "Tables" — alternating floor pattern (visually, these are just floor tiles,
    // but leaving open walkways suggests table rows)
    // Place some storage tiles to represent food storage area
    int bottomY = room.interiorY() + room.interiorH() - 1;
    for (int x = room.interiorX() + 1; x < room.interiorX() + room.interiorW() - 1; x += 2) {
        setGrid(x, bottomY, CellType::STORAGE);
    }
}

void StationGenerator::furnishEngineering(const PlacedRoom& room) {
    // Terminals along two walls (engineering consoles)
    int termY = room.interiorY();
    for (int x = room.interiorX(); x < room.interiorX() + room.interiorW(); x += 2) {
        setGrid(x, termY, CellType::TERMINAL);
    }
    // Terminals along the south interior wall too
    int southY = room.interiorY() + room.interiorH() - 1;
    for (int x = room.interiorX() + 1; x < room.interiorX() + room.interiorW(); x += 2) {
        setGrid(x, southY, CellType::TERMINAL);
    }
}

void StationGenerator::furnishMedbay(const PlacedRoom& room) {
    // A few terminals as medical equipment
    setGrid(room.interiorX(), room.interiorY(), CellType::TERMINAL);
    if (room.interiorW() > 3) {
        setGrid(room.interiorX() + room.interiorW() - 1, room.interiorY(), CellType::TERMINAL);
    }
}

void StationGenerator::furnishCrewQuarters(const PlacedRoom& room) {
    // Scatter spawn points
    int ix = room.interiorX();
    int iy = room.interiorY();
    setGrid(ix + 1, iy + 1, CellType::SPAWN_POINT);
    setGrid(ix + 2, iy + 1, CellType::SPAWN_POINT);
    if (room.interiorH() > 3) {
        setGrid(ix + 1, iy + 2, CellType::SPAWN_POINT);
        setGrid(ix + 2, iy + 2, CellType::SPAWN_POINT);
    }
}

void StationGenerator::furnishRefinery(const PlacedRoom& room) {
    // Left half of interior: REFINERY cells (input — drop raw materials here)
    // Right half of interior: FLOOR cells (output — processed cargo appears here)
    int halfW = room.interiorW() / 2;
    for (int y = room.interiorY(); y < room.interiorY() + room.interiorH(); y++) {
        for (int x = room.interiorX(); x < room.interiorX() + halfW; x++) {
            setGrid(x, y, CellType::REFINERY);
        }
        // Right half stays as FLOOR (already placed by placeRoomWalls)
    }
}

// =========================================================================
// Corridor connection (MST + extra edges)
// =========================================================================

void StationGenerator::connectAllRooms() {
    int n = static_cast<int>(rooms.size());
    if (n <= 1) return;

    // Prim's MST using Manhattan distance
    std::vector<bool> inMST(n, false);
    std::vector<int> minDist(n, INT_MAX);
    std::vector<int> parent(n, -1);
    minDist[0] = 0;

    std::vector<std::pair<int, int>> edges;

    for (int i = 0; i < n; i++) {
        // Find vertex with minimum distance not in MST
        int u = -1;
        for (int v = 0; v < n; v++) {
            if (!inMST[v] && (u == -1 || minDist[v] < minDist[u])) {
                u = v;
            }
        }
        if (u == -1) break;
        inMST[u] = true;
        if (parent[u] != -1) {
            edges.push_back({parent[u], u});
        }

        // Update distances
        for (int v = 0; v < n; v++) {
            if (inMST[v]) continue;
            int dist = std::abs(rooms[u].centerX() - rooms[v].centerX())
                     + std::abs(rooms[u].centerY() - rooms[v].centerY());
            if (dist < minDist[v]) {
                minDist[v] = dist;
                parent[v] = u;
            }
        }
    }

    // Carve corridors for MST edges
    for (auto& [a, b] : edges) {
        connectRooms(rooms[a], rooms[b]);
    }

    // Add 1-2 random extra connections for loops
    if (n > 3) {
        for (int extra = 0; extra < 2; extra++) {
            int a = randInt(0, n - 1);
            int b = randInt(0, n - 1);
            if (a != b) connectRooms(rooms[a], rooms[b]);
        }
    }
}

void StationGenerator::connectRooms(const PlacedRoom& a, const PlacedRoom& b) {
    // Use corridor exit point: for landing bay, use the south edge center
    // so corridors don't carve through the middle of the bay
    int ax = a.centerX();
    int ay = (a.type == RoomType::LANDING_BAY) ? a.y + a.height - 1 : a.centerY();
    int bx = b.centerX();
    int by = (b.type == RoomType::LANDING_BAY) ? b.y + b.height - 1 : b.centerY();

    // L-shaped corridor: randomly choose horizontal-first or vertical-first
    bool hFirst = (rng() % 2 == 0);

    if (hFirst) {
        carveCorridorH(ax, bx, ay);
        carveCorridorV(ay, by, bx);
    } else {
        carveCorridorV(ay, by, ax);
        carveCorridorH(ax, bx, by);
    }
}

void StationGenerator::carveCorridorH(int x1, int x2, int y) {
    if (x1 > x2) std::swap(x1, x2);
    for (int x = x1; x <= x2; x++) {
        CellType current = getGrid(x, y);
        // Carve through empty AND walls (to punch through room walls)
        if (current == CellType::EMPTY || current == CellType::WALL) {
            setGrid(x, y, CellType::FLOOR);
        }
        // Add walls above and below if those cells are EMPTY
        if (getGrid(x, y - 1) == CellType::EMPTY) {
            setGrid(x, y - 1, CellType::WALL);
        }
        if (getGrid(x, y + 1) == CellType::EMPTY) {
            setGrid(x, y + 1, CellType::WALL);
        }
    }
}

void StationGenerator::carveCorridorV(int y1, int y2, int x) {
    if (y1 > y2) std::swap(y1, y2);
    for (int y = y1; y <= y2; y++) {
        CellType current = getGrid(x, y);
        // Carve through empty AND walls (to punch through room walls)
        if (current == CellType::EMPTY || current == CellType::WALL) {
            setGrid(x, y, CellType::FLOOR);
        }
        // Add walls left and right if those cells are EMPTY
        if (getGrid(x - 1, y) == CellType::EMPTY) {
            setGrid(x - 1, y, CellType::WALL);
        }
        if (getGrid(x + 1, y) == CellType::EMPTY) {
            setGrid(x + 1, y, CellType::WALL);
        }
    }
}

// =========================================================================
// Door placement pass
// =========================================================================

void StationGenerator::placeDoors() {
    // Place doors where single-width corridors pass through walls.
    // A door candidate is a FLOOR cell with walls on exactly two opposite sides
    // AND no adjacent door already placed (to prevent door chains).
    // Doors now open automatically — no terminals needed near corridor doors.
    for (int y = 1; y < gridHeight - 1; y++) {
        for (int x = 1; x < gridWidth - 1; x++) {
            if (getGrid(x, y) != CellType::FLOOR) continue;

            bool wallLeft  = (getGrid(x - 1, y) == CellType::WALL);
            bool wallRight = (getGrid(x + 1, y) == CellType::WALL);
            bool wallUp    = (getGrid(x, y - 1) == CellType::WALL);
            bool wallDown  = (getGrid(x, y + 1) == CellType::WALL);

            // Check that no nearby cell is already a door (prevents chains)
            auto isDoor = [this](int cx, int cy) {
                return getGrid(cx, cy) == CellType::DOOR;
            };

            bool nearbyDoor = isDoor(x - 1, y) || isDoor(x + 1, y) ||
                              isDoor(x, y - 1) || isDoor(x, y + 1) ||
                              isDoor(x - 2, y) || isDoor(x + 2, y) ||
                              isDoor(x, y - 2) || isDoor(x, y + 2);
            if (nearbyDoor) continue;

            // Horizontal corridor door: walls left + right
            if (wallLeft && wallRight && !wallUp && !wallDown) {
                setGrid(x, y, CellType::DOOR);
            }
            // Vertical corridor door: walls up + down
            else if (wallUp && wallDown && !wallLeft && !wallRight) {
                setGrid(x, y, CellType::DOOR);
            }
        }
    }
}

// =========================================================================
// Connectivity validation
// =========================================================================

bool StationGenerator::validateConnectivity() {
    // Find any spawn point to start flood fill
    int startX = -1, startY = -1;
    for (int y = 0; y < gridHeight && startX < 0; y++) {
        for (int x = 0; x < gridWidth; x++) {
            if (getGrid(x, y) == CellType::SPAWN_POINT) {
                startX = x;
                startY = y;
                break;
            }
        }
    }
    if (startX < 0) return false; // no spawn points at all

    // BFS flood fill on walkable cells
    std::vector<bool> visited(gridWidth * gridHeight, false);
    std::queue<std::pair<int, int>> bfsQueue;
    bfsQueue.push({startX, startY});
    visited[startY * gridWidth + startX] = true;

    auto isWalkable = [this](int x, int y) -> bool {
        CellType c = getGrid(x, y);
        return c == CellType::FLOOR || c == CellType::DOOR || c == CellType::TERMINAL ||
               c == CellType::SPAWN_POINT || c == CellType::AIRLOCK || c == CellType::STORAGE ||
               c == CellType::DOCKING_COLLAR || c == CellType::LANDING_PAD ||
               c == CellType::HANGAR_DOOR || c == CellType::REFINERY ||
               c == CellType::TURRET_BASE;
    };

    while (!bfsQueue.empty()) {
        auto [cx, cy] = bfsQueue.front();
        bfsQueue.pop();
        int dx[] = {-1, 1, 0, 0};
        int dy[] = {0, 0, -1, 1};
        for (int d = 0; d < 4; d++) {
            int nx = cx + dx[d];
            int ny = cy + dy[d];
            if (nx < 0 || nx >= gridWidth || ny < 0 || ny >= gridHeight) continue;
            int idx = ny * gridWidth + nx;
            if (visited[idx]) continue;
            if (!isWalkable(nx, ny)) continue;
            visited[idx] = true;
            bfsQueue.push({nx, ny});
        }
    }

    // Check if all rooms have at least one visited interior cell
    bool allConnected = true;
    for (size_t i = 0; i < rooms.size(); i++) {
        const auto& room = rooms[i];
        bool roomReachable = false;
        for (int y = room.interiorY(); y < room.interiorY() + room.interiorH() && !roomReachable; y++) {
            for (int x = room.interiorX(); x < room.interiorX() + room.interiorW(); x++) {
                int idx = y * gridWidth + x;
                if (idx >= 0 && idx < static_cast<int>(visited.size()) && visited[idx]) {
                    roomReachable = true;
                    break;
                }
            }
        }
        if (!roomReachable) {
            // Emergency: connect this room to room 0 (docking bay)
            std::cout << "[GENERATOR] Room " << i << " unreachable, emergency corridor" << std::endl;
            emergencyConnect(rooms[0], room);
            allConnected = false;
        }
    }
    return allConnected;
}

void StationGenerator::emergencyConnect(const PlacedRoom& from, const PlacedRoom& to) {
    // Direct L-corridor
    connectRooms(from, to);
    // Re-place doors along the new corridor
    placeDoors();
}

// =========================================================================
// Trim and commit to StationMap
// =========================================================================

void StationGenerator::trimAndCommit(StationMap& map) {
    // Find bounding box of all non-EMPTY cells
    int minX = gridWidth, maxX = 0, minY = gridHeight, maxY = 0;
    for (int y = 0; y < gridHeight; y++) {
        for (int x = 0; x < gridWidth; x++) {
            if (getGrid(x, y) != CellType::EMPTY) {
                if (x < minX) minX = x;
                if (x > maxX) maxX = x;
                if (y < minY) minY = y;
                if (y > maxY) maxY = y;
            }
        }
    }

    if (minX > maxX) return; // empty map somehow

    // Add 14 rows of padding above for ship approach (ships spawn 400px = ~13 cells above pads)
    int padTop = 14;
    int startY = (std::max)(0, minY - padTop);
    int startX = (std::max)(0, minX - 1); // 1 col padding on sides
    int endX = (std::min)(gridWidth - 1, maxX + 1);
    int endY = (std::min)(gridHeight - 1, maxY + 1);

    int trimmedW = endX - startX + 1;
    int trimmedH = endY - startY + 1;

    map.width = trimmedW;
    map.height = trimmedH;
    map.originX = 0;
    map.originY = 0;
    map.cells.resize(trimmedW * trimmedH, CellType::EMPTY);

    for (int y = 0; y < trimmedH; y++) {
        for (int x = 0; x < trimmedW; x++) {
            map.cells[y * trimmedW + x] = getGrid(startX + x, startY + y);
        }
    }
}

// =========================================================================
// Turret placement on exterior walls
// =========================================================================

void StationGenerator::placeTurrets() {
    // Find all WALL cells that face EMPTY (exterior-facing)
    struct TurretCandidate {
        int x, y;
    };
    std::vector<TurretCandidate> candidates;

    for (int y = 1; y < gridHeight - 1; y++) {
        for (int x = 1; x < gridWidth - 1; x++) {
            if (getGrid(x, y) != CellType::WALL) continue;

            // Must have at least one EMPTY neighbor (exterior-facing)
            bool hasEmpty = (getGrid(x - 1, y) == CellType::EMPTY) ||
                            (getGrid(x + 1, y) == CellType::EMPTY) ||
                            (getGrid(x, y - 1) == CellType::EMPTY) ||
                            (getGrid(x, y + 1) == CellType::EMPTY);
            if (!hasEmpty) continue;

            // Skip corners (walls with EMPTY on two adjacent sides)
            int emptyCount = 0;
            if (getGrid(x - 1, y) == CellType::EMPTY) emptyCount++;
            if (getGrid(x + 1, y) == CellType::EMPTY) emptyCount++;
            if (getGrid(x, y - 1) == CellType::EMPTY) emptyCount++;
            if (getGrid(x, y + 1) == CellType::EMPTY) emptyCount++;
            if (emptyCount > 2) continue;

            // Skip hangar door area (top of landing bay)
            if (!rooms.empty() && rooms[0].type == RoomType::LANDING_BAY) {
                const auto& bay = rooms[0];
                if (y == bay.y && x >= bay.x && x < bay.x + bay.width) continue;
            }

            candidates.push_back({x, y});
        }
    }

    if (candidates.empty()) return;

    // Place turrets at intervals along the perimeter
    // Shuffle candidates and pick every ~10th one
    std::shuffle(candidates.begin(), candidates.end(), rng);

    int targetCount = (std::max)(6, (std::min)(12, static_cast<int>(candidates.size()) / 10));
    int placed = 0;

    for (auto& c : candidates) {
        if (placed >= targetCount) break;

        // Check minimum distance from already-placed turrets
        bool tooClose = false;
        for (int ty = c.y - 6; ty <= c.y + 6 && !tooClose; ty++) {
            for (int tx = c.x - 6; tx <= c.x + 6; tx++) {
                if (getGrid(tx, ty) == CellType::TURRET_BASE) {
                    tooClose = true;
                    break;
                }
            }
        }
        if (tooClose) continue;

        setGrid(c.x, c.y, CellType::TURRET_BASE);
        placed++;
    }

    std::cout << "[GENERATOR] Placed " << placed << " turret bases" << std::endl;
}

// =========================================================================
// Main generate() orchestrator
// =========================================================================

bool StationGenerator::generate(StationMap& map, uint32_t seed) {
    if (seed == 0) {
        seed = std::random_device{}();
    }
    rng.seed(seed);
    std::cout << "[GENERATOR] Generating station with seed " << seed << std::endl;

    // Phase 1: Initialize grid (larger to fit landing bay + rooms)
    initGrid(90, 70);

    // Phase 2: Place landing bay at top
    placeLandingBay();
    int belowLandingBay = rooms[0].y + rooms[0].height;

    // Phase 3: Place remaining rooms
    std::vector<RoomDef> roomDefs = {
        {RoomType::STORAGE,        6, 6, 10, 8},
        {RoomType::COMMAND,        6, 6,  8, 8},
        {RoomType::MESS_HALL,      6, 5, 10, 7},
        {RoomType::ENGINEERING,    5, 5,  8, 6},
        {RoomType::MEDBAY,         4, 4,  6, 5},
        {RoomType::CREW_QUARTERS,  5, 4,  7, 5},
        {RoomType::REFINERY,       6, 5,  8, 6},
    };

    for (auto& def : roomDefs) {
        if (!tryPlaceRoom(def, belowLandingBay)) {
            // Try again with smaller size
            RoomDef smaller = def;
            smaller.maxWidth = smaller.minWidth;
            smaller.maxHeight = smaller.minHeight;
            if (!tryPlaceRoom(smaller, belowLandingBay)) {
                std::cout << "[GENERATOR] Warning: Could not place room type "
                          << static_cast<int>(def.type) << std::endl;
            }
        }
    }

    // Phase 4: Connect all rooms with corridors (MST)
    connectAllRooms();

    // Phase 5: Place doors where corridors meet walls
    placeDoors();

    // Phase 6: Furnish rooms
    for (auto& room : rooms) {
        furnishRoom(room);
    }

    // Phase 7: Place turrets on exterior walls
    placeTurrets();

    // Phase 8: Validate connectivity
    if (!validateConnectivity()) {
        std::cout << "[GENERATOR] Fixed connectivity issues" << std::endl;
    }

    // Phase 9: Ensure at least some spawn points exist
    {
        bool hasSpawn = false;
        for (int y = 0; y < gridHeight && !hasSpawn; y++) {
            for (int x = 0; x < gridWidth; x++) {
                if (getGrid(x, y) == CellType::SPAWN_POINT) {
                    hasSpawn = true;
                    break;
                }
            }
        }
        if (!hasSpawn && rooms.size() > 1) {
            // Place spawn points in the center of the first non-docking room
            auto& fallback = rooms[1];
            int cx = fallback.centerX();
            int cy = fallback.centerY();
            setGrid(cx, cy, CellType::SPAWN_POINT);
            setGrid(cx - 1, cy, CellType::SPAWN_POINT);
            std::cout << "[GENERATOR] No spawn points found, placed in fallback room" << std::endl;
        }
    }

    // Phase 8: Trim and commit to StationMap
    trimAndCommit(map);

    // Debug: print ASCII layout
    std::cout << "[GENERATOR] Station layout (" << map.width << "x" << map.height << "):" << std::endl;
    for (int y = 0; y < map.height; y++) {
        std::string line;
        for (int x = 0; x < map.width; x++) {
            line += StationMap::cellToChar(map.getCell(x, y));
        }
        std::cout << "  " << line << std::endl;
    }

    return true;
}

} // namespace ssm
