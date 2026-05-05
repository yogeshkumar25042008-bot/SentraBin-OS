// ============================================================
//  SENTRABIN OS — Centralized Bin Operations System
//  Final Full Build | Pure C (C99) | No complex headers
//  Modules: Auth, Bin, Vehicle, Driver, Assignment,
//           Route Optimization, User, Request/Address
// ============================================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>      // sqrtf — compile with -lm

// ── File Names ────────────────────────────────────────────────────────────────
#define BINS_CSV       "bins.csv"
#define VEHICLES_CSV   "vehicle.csv"
#define DRIVERS_CSV    "drivers.csv"
#define REQUESTS_CSV   "requests.csv"
#define ADDRESSED_CSV  "addressed.csv"
#define ROUTES_CSV     "routes.csv"

// ── System Limits ─────────────────────────────────────────────────────────────
#define MAX_BINS       100
#define MAX_ZONE       20
#define MAX_VEHICLES   20
#define MAX_DRIVERS    20
#define MAX_REQUESTS   100
#define MAX_ROUTE_BINS 50    // max bins one vehicle can visit per route run

// ── Authentication ────────────────────────────────────────────────────────────
#define ADMIN_PASS    "admin@123"
#define NETIZEN_PASS  "guest@123"

// ── Waste Types ───────────────────────────────────────────────────────────────
#define DRY       1
#define WET       2
#define MIXED     3
#define HAZARDOUS 4

// ── Priority Levels ───────────────────────────────────────────────────────────
#define PRI_HIGH 3
#define PRI_MED  2
#define PRI_LOW  1

// ── WPI / Fill Thresholds ─────────────────────────────────────────────────────
#define FILL_THRESH_HIGH 70.0f
#define FILL_THRESH_MED  40.0f
#define WPI_THRESH_HIGH  60.0f
#define WPI_THRESH_MED   35.0f

// ── Central Hub Coordinates (per zone, assumed 0,0) ──────────────────────────
// Each zone's collection vehicles start and return here.
// Admin can extend this to a per-zone table later.
#define HUB_X 0
#define HUB_Y 0

// ── Driver daily hour cap (fixed per spec) ────────────────────────────────────
#define DRIVER_MAX_HOURS 5.0f

// ── Request Reason Codes ──────────────────────────────────────────────────────
#define REQ_FUEL        "FUEL_OUT"
#define REQ_CAPACITY    "CAPACITY_FULL"
#define REQ_NO_BACKUP   "NO_BACKUP"
#define REQ_HOURS_OVER  "HOURS_EXCEEDED"
#define REQ_SICK        "DRIVER_SICK"
#define REQ_SUSPENDED   "DRIVER_SUSPENDED"

// ── Average speed assumption for hour estimation ──────────────────────────────
// Vehicles travel at ~30 coordinate-units per hour (tunable)
#define AVG_SPEED 30.0f

// ─────────────────────────────────────────────────────────────────────────────
//  DATA STRUCTURES
// ─────────────────────────────────────────────────────────────────────────────

// Bin: represents one physical waste bin in a zone
typedef struct {
    long long int bin_id;
    int           zone;
    int           waste_type;
    float         fill_level;    // percentage 0–100
    float         capacity;      // kg
    float         wpi;           // Waste Priority Index
    int           priority;      // PRI_HIGH / PRI_MED / PRI_LOW
    int           x, y;          // grid coordinates
    char          last_collection[11]; // DD-MM-YYYY
    bool          collected_today;
} Bin;

// Vehicle: a collection truck
typedef struct {
    int   vehicle_id;
    char  type[30];              // "Compactor" / "Tipper" / "Small"
    float max_capacity;          // kg
    float fuel_tank_capacity;    // liters
    float current_fuel;          // liters remaining
    float fuel_consumption_rate; // liters per 10 coordinate-units
    int   assigned_zone;
    bool  is_available;
    bool  under_maintenance;
    int   assigned_driver_id;    // 0 = unassigned
    float current_load;          // kg currently loaded
    char  registration[15];
} Vehicle;

// Driver: a person who operates a vehicle
typedef struct {
    int   driver_id;             // 1xxx = normal, 8xxx = hazmat certified
    char  name[50];
    char  phone[15];
    int   assigned_vehicle_id;   // 0 = none
    bool  is_available;
    bool  is_suspended;
    bool  has_hazmat_license;
    float hours_worked_today;
    float max_daily_hours;
} Driver;

// Request: raised by vehicle or driver, viewed by admin
typedef struct {
    int   request_id;
    char  reason[30];            // REQ_* codes
    int   vehicle_id;
    char  vehicle_reg[15];
    char  vehicle_type[30];
    int   zone;
    long long int last_bin_id;   // bin being served when request raised
    int   driver_id;
    char  driver_name[50];
    float hours_worked;          // for HOURS_EXCEEDED / SICK
    char  status[15];            // "PENDING" / "ADDRESSED"
} Request;

// ─────────────────────────────────────────────────────────────────────────────
//  GLOBAL ARRAYS
// ─────────────────────────────────────────────────────────────────────────────
Bin      bins[MAX_BINS];
Vehicle  vehicles[MAX_VEHICLES];
Driver   drivers[MAX_DRIVERS];
Request  requests[MAX_REQUESTS];

// ─────────────────────────────────────────────────────────────────────────────
//  HELPER FUNCTIONS
// ─────────────────────────────────────────────────────────────────────────────
//error fix
int loadDriversFromCSV();

// Convert waste type int → readable string
const char* getWasteTypeString(int type) {
    switch (type) {
        case DRY:       return "DRY";
        case WET:       return "WET";
        case MIXED:     return "MIXED";
        case HAZARDOUS: return "HAZARDOUS";
        default:        return "UNKNOWN";
    }
}

// Convert priority int → readable string
const char* getPriorityString(int priority) {
    switch (priority) {
        case PRI_HIGH: return "HIGH";
        case PRI_MED:  return "MEDIUM";
        case PRI_LOW:  return "LOW";
        default:       return "UNKNOWN";
    }
}

// Parse waste type string → int
int parseWasteType(const char* str) {
    if (strcmp(str, "DRY")       == 0) return DRY;
    if (strcmp(str, "WET")       == 0) return WET;
    if (strcmp(str, "MIXED")     == 0) return MIXED;
    if (strcmp(str, "HAZARDOUS") == 0) return HAZARDOUS;
    return DRY;
}

// Compute Waste Priority Index from fill% and waste type
float computeWPI(float fill, int wasteType) {
    int typefactor;
    switch (wasteType) {
        case DRY:       typefactor = 20; break;
        case WET:       typefactor = 40; break;
        case MIXED:     typefactor = 50; break;
        case HAZARDOUS: typefactor = 80; break;
        default:        typefactor = 20;
    }
    return (0.4f * typefactor) + (0.6f * fill);
}

// Check if a CSV file is empty or missing (used before writing headers)
bool isCsvEmpty(const char* filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) return true;
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fclose(fp);
    return (size == 0);
}

// Manhattan distance between two grid points
float manhattanDist(int x1, int y1, int x2, int y2) {
    return (float)(abs(x2 - x1) + abs(y2 - y1));
}

// Diagonal (Chebyshev) distance between two grid points
float diagonalDist(int x1, int y1, int x2, int y2) {
    int dx = abs(x2 - x1);
    int dy = abs(y2 - y1);
    // Chebyshev: move diagonally where possible
    return (float)((dx > dy) ? dx : dy);
}

// Best distance = minimum of Manhattan and Diagonal
// This gives the route optimizer two options and picks cheapest fuel-wise
float bestDist(int x1, int y1, int x2, int y2) {
    float m = manhattanDist(x1, y1, x2, y2);
    float d = diagonalDist(x1, y1, x2, y2);
    return (m < d) ? m : d;
}

// Fuel needed to travel a given distance with a given vehicle
float fuelNeeded(float distance, float consumptionRate) {
    // consumptionRate is liters per 10 units of distance
    return (distance / 10.0f) * consumptionRate;
}

// Estimate hours to travel a distance at AVG_SPEED
float hoursToTravel(float distance) {
    return distance / AVG_SPEED;
}

// Generate next request ID by scanning requests.csv
int generateRequestID() {
    FILE *fp = fopen(REQUESTS_CSV, "r");
    if (!fp) return 1;
    char line[512];
    fgets(line, sizeof(line), fp); // skip header
    int maxID = 0, id;
    while (fgets(line, sizeof(line), fp)) {
        if (sscanf(line, "%d,", &id) == 1 && id > maxID)
            maxID = id;
    }
    fclose(fp);
    return maxID + 1;
}
// ══════════════════════════════════════════════════════════════════════════════
//  VEHICLE MANAGEMENT MODULE
//  Handles: ID generation, vehicle creation, CSV load/save, view
//  Vehicles start with full fuel tank and are available by default.
//  current_fuel is now persisted in CSV (added column vs original).
// ══════════════════════════════════════════════════════════════════════════════

// ── Generate next unique Vehicle ID by scanning existing CSV ──────────────────
int generateVehicleID() {
    FILE *fp = fopen(VEHICLES_CSV, "r");
    int maxID = 0;

    if (fp == NULL) return 1001; // First vehicle starts at 1001

    char line[512];
    fgets(line, sizeof(line), fp); // skip header

    int vehicle_id;
    while (fgets(line, sizeof(line), fp) != NULL) {
        if (sscanf(line, "%d,", &vehicle_id) == 1)
            if (vehicle_id > maxID) maxID = vehicle_id;
    }
    fclose(fp);
    return maxID + 1;
}

// ── Register new vehicles into vehicle.csv ────────────────────────────────────
// Admin enters count, type, zone, and registration plate.
// Capacity, fuel tank, and consumption are auto-set by vehicle type.
void createVehicle() {
    int n;
    printf("\n========================================");
    printf("\n     CREATE VEHICLES MODULE            ");
    printf("\n========================================\n");
    printf("Enter the Number of Vehicles to Register: ");
    scanf("%d", &n);
    if (n <= 0 || n > MAX_VEHICLES) {
    printf("Invalid number of vehicles\n");
    return;
}

    // Check if drivers exist — vehicles need drivers 1:1
    int totalDrivers = loadDriversFromCSV();
    if (totalDrivers == 0) {
        printf("[WARN] No drivers registered yet. Register drivers before or after vehicles.\n");
    }

bool isNew = isCsvEmpty(VEHICLES_CSV);
FILE *fp = fopen(VEHICLES_CSV, "a");

if (isNew) {
    fprintf(fp,
        "vehicle_id,type,max_capacity,fuel_tank_capacity,current_fuel,"
        "fuel_consumption_rate,assigned_zone,is_available,"
        "under_maintenance,assigned_driver_id,current_load,registration\n");
    }

    int vehicle_id = generateVehicleID();

    for (int i = 0; i < n; i++) {
        printf("\n--- Enter Details for Vehicle %d ---", i + 1);

        // ── Vehicle Type Selection ────────────────────────────────────────────
        printf("\nSelect Vehicle Type:\n");
        printf("  1. Compactor  (5000kg | 120L tank | 8.5L/10units)\n");
        printf("  2. Tipper     (3500kg | 100L tank | 6.5L/10units)\n");
        printf("  3. Small      (1500kg |  60L tank | 4.0L/10units)\n");
        printf("Option: ");
        int type_choice;
        scanf("%d", &type_choice);

        char  type[30];
        float max_cap, fuel_tank, fuel_consumption;

        switch (type_choice) {
            case 1:
                strcpy(type, "Compactor");
                max_cap          = 5000.0f;
                fuel_tank        = 120.0f;
                fuel_consumption = 8.5f;
                break;
            case 2:
                strcpy(type, "Tipper");
                max_cap          = 3500.0f;
                fuel_tank        = 100.0f;
                fuel_consumption = 6.5f;
                break;
            case 3:
                strcpy(type, "Small");
                max_cap          = 1500.0f;
                fuel_tank        = 60.0f;
                fuel_consumption = 4.0f;
                break;
            default:
                printf("Invalid vehicle type! Try again.\n");
                i--; continue;
        }

        // ── Zone Assignment ───────────────────────────────────────────────────
        printf("\nEnter Assigned Zone (1-%d): ", MAX_ZONE);
        int assigned_zone;
        scanf("%d", &assigned_zone);
        if (assigned_zone < 1 || assigned_zone > MAX_ZONE) {
            printf("Invalid Zone!\n");
            i--; continue;
        }

        // ── Registration Plate ────────────────────────────────────────────────
        printf("Enter Vehicle Registration (e.g., TN-01-AB-1234): ");
        char registration[15];
        scanf("%14s", registration);

        // ── Defaults for new vehicle ──────────────────────────────────────────
        bool  is_available      = true;
        bool  under_maintenance = false;
        int   assigned_driver   = 0;      // unassigned until assignment module runs
        float current_load      = 0.0f;
        float current_fuel      = fuel_tank; // starts with full tank

        // ── Write to CSV (12 columns including current_fuel) ──────────────────
        fprintf(fp, "%d,%s,%.2f,%.2f,%.2f,%.2f,%d,%d,%d,%d,%.2f,%s\n",
                vehicle_id,
                type,
                max_cap,
                fuel_tank,
                current_fuel,
                fuel_consumption,
                assigned_zone,
                (int)is_available,
                (int)under_maintenance,
                assigned_driver,
                current_load,
                registration);

        printf("\n[OK] Vehicle %d (%s) registered!\n", vehicle_id, type);
        printf("     Reg: %-15s | Zone: %d | Cap: %.0fkg | Fuel: %.0fL\n",
               registration, assigned_zone, max_cap, fuel_tank);

        vehicle_id++;
    }

    fclose(fp);
    printf("\n========================================\n");
}

// ── Load all vehicles from CSV into global vehicles[] array ──────────────────
// Returns count of vehicles loaded.
// NOTE: CSV now has 12 columns (current_fuel added between fuel_tank and rate).

//error fix
int loadVehiclesFromCSV() {
    FILE *fp = fopen(VEHICLES_CSV, "r");
    if (!fp) return 0;

    char line[512];
    fgets(line, sizeof(line), fp); // skip header

    int count = 0;

    while (count < MAX_VEHICLES) {

        int avail, maint;

        int result = fscanf(fp,
            "%d,%29[^,],%f,%f,%f,%f,%d,%d,%d,%d,%f,%14[^\n]",
            &vehicles[count].vehicle_id,
            vehicles[count].type,
            &vehicles[count].max_capacity,
            &vehicles[count].fuel_tank_capacity,
            &vehicles[count].current_fuel,
            &vehicles[count].fuel_consumption_rate,
            &vehicles[count].assigned_zone,
            &avail,
            &maint,
            &vehicles[count].assigned_driver_id,
            &vehicles[count].current_load,
            vehicles[count].registration
        );

        if (result != 12) break;

        // assign bool safely
        vehicles[count].is_available = avail;
        vehicles[count].under_maintenance = maint;

        count++;
    }

    fclose(fp);
    return count;
}

// ── Save all vehicles in global array back to CSV ─────────────────────────────
// Called after assignment or route updates change vehicle state.
void saveVehiclesToCSV(int total) {
    FILE *fp = fopen(VEHICLES_CSV, "w");
    if (!fp) {
        printf("Error saving vehicle data!\n");
        return;
    }

    // Write header
    fprintf(fp,
        "vehicle_id,type,max_capacity,fuel_tank_capacity,current_fuel,"
        "fuel_consumption_rate,assigned_zone,is_available,"
        "under_maintenance,assigned_driver_id,current_load,registration\n");

    for (int i = 0; i < total; i++) {
        fprintf(fp, "%d,%s,%.2f,%.2f,%.2f,%.2f,%d,%d,%d,%d,%.2f,%s\n",
                vehicles[i].vehicle_id,
                vehicles[i].type,
                vehicles[i].max_capacity,
                vehicles[i].fuel_tank_capacity,
                vehicles[i].current_fuel,
                vehicles[i].fuel_consumption_rate,
                vehicles[i].assigned_zone,
                (int)vehicles[i].is_available,
                (int)vehicles[i].under_maintenance,
                vehicles[i].assigned_driver_id,
                vehicles[i].current_load,
                vehicles[i].registration);
    }

    fclose(fp);
}

// ── Display all registered vehicles in a formatted table ─────────────────────
void viewVehicles() {
    printf("\n========================================");
    printf("\n      VIEW ALL VEHICLES MODULE         ");
    printf("\n========================================\n");

    int total = loadVehiclesFromCSV();
    if (total == 0) {
        printf("No vehicles registered yet.\n");
        return;
    }

    printf("\n%-10s %-11s %-9s %-9s %-8s %-7s %-11s %-6s %s\n",
           "VehicleID", "Type", "Cap(kg)", "Fuel(L)", "CurFuel",
           "Zone", "Status", "Driver", "Reg");
    printf("-------------------------------------------------------------------------------------\n");

    int avail = 0, inuse = 0, maint = 0;

    for (int i = 0; i < total; i++) {
        const char* status;
        if (vehicles[i].under_maintenance) { status = "MAINTENANCE"; maint++; }
        else if (!vehicles[i].is_available){ status = "IN-USE";      inuse++; }
        else                               { status = "AVAILABLE";   avail++; }

        printf("%-10d %-11s %-9.0f %-9.0f %-8.1f %-7d %-11s %-6d %s\n",
               vehicles[i].vehicle_id,
               vehicles[i].type,
               vehicles[i].max_capacity,
               vehicles[i].fuel_tank_capacity,
               vehicles[i].current_fuel,
               vehicles[i].assigned_zone,
               status,
               vehicles[i].assigned_driver_id,
               vehicles[i].registration);
    }

    printf("-------------------------------------------------------------------------------------\n");
    printf("Total: %d  |  Available: %d  |  In-Use: %d  |  Maintenance: %d\n",
           total, avail, inuse, maint);
    printf("========================================\n");
}
// ══════════════════════════════════════════════════════════════════════════════
//  BIN MANAGEMENT MODULE
//  Handles: Bin ID generation, bin creation, CSV load/save,
//           WPI computation, priority assignment, critical bin display.
//  Bin ID format: [waste_type][zone_2digit][serial_3digit]
//  Example: HAZARDOUS zone 3 bin 1 → 4003001
// ══════════════════════════════════════════════════════════════════════════════

// ── Generate unique Bin ID based on waste type, zone, and serial ──────────────
// Scans existing CSV to find the highest serial for that type+zone combo.
long long int generateBinID(int waste_type, int zone) {
    FILE *fp = fopen(BINS_CSV, "r");

    int maxSerial = 0;

    if (fp == NULL) {
        // No file yet — first bin for this type+zone is serial 001
        return (long long)waste_type * 100000 + zone * 1000 + 1;
    }

    char line[256];
    fgets(line, sizeof(line), fp); // skip header

    long long int id;
    int z, collected, x, y;
    char wasteTypeStr[20];
    float capacity, fill, wpi;

    while (fscanf(fp, "%lld,%d,%[^,],%f,%f,%d,%d,%f,%d\n",
                  &id, &z, wasteTypeStr,
                  &capacity, &fill,
                  &x, &y,
                  &wpi, &collected) == 9) {

        int currentWasteType = (int)(id / 100000);
        int currentZone      = (int)((id / 1000) % 100);

        if (currentZone == zone && currentWasteType == waste_type) {
            int serial = (int)(id % 1000);
            if (serial > maxSerial) maxSerial = serial;
        }
    }

    fclose(fp);

    int newSerial = maxSerial + 1;
    if (newSerial > 999) {
        printf("[ERROR] Max bins (999) reached for WasteType %d Zone %d!\n",
               waste_type, zone);
        return -1;
    }

    return (long long)waste_type * 100000 + (long long)zone * 1000 + newSerial;
}

// ── Create bins and append to bins.csv ───────────────────────────────────────
// Admin enters zone, waste type, capacity, fill level, and coordinates.
// WPI and priority are computed automatically.
void createBin() {
    int n;
    printf("\n========================================");
    printf("\n       CREATE BIN RECORDS MODULE       ");
    printf("\n========================================\n");
    printf("Enter the Number of Bins to be Created: ");
    scanf("%d", &n);

    bool isNew = isCsvEmpty(BINS_CSV);
    FILE *fp = fopen(BINS_CSV, "a");

    // Write header only if file is new or empty
    if (isNew) {
        fprintf(fp, "bin_id,zone,waste_type,capacity,fill_level,x,y,wpi,collected_today\n");
    }

    for (int i = 0; i < n; i++) {
        Bin b;
        printf("\n--- Enter Details for Bin %d ---", i + 1);

        // ── Zone ──────────────────────────────────────────────────────────────
        printf("\nEnter Bin Zone (1-%d): ", MAX_ZONE);
        scanf("%d", &b.zone);
        if (b.zone < 1 || b.zone > MAX_ZONE) {
            printf("Invalid zone! Try again.\n");
            i--; continue;
        }

        // ── Waste Type ────────────────────────────────────────────────────────
        printf("Select Waste Type:\n");
        printf("  1. DRY\n  2. WET\n  3. MIXED\n  4. HAZARDOUS\n");
        printf("Option: ");
        scanf("%d", &b.waste_type);
        if (b.waste_type < 1 || b.waste_type > 4) {
            printf("Invalid waste type!\n");
            i--; continue;
        }

        // ── Capacity ──────────────────────────────────────────────────────────
        printf("Enter Capacity (kg): ");
        scanf("%f", &b.capacity);
        if (b.capacity <= 0) {
            printf("Capacity must be > 0!\n");
            i--; continue;
        }

        // ── Fill Level ────────────────────────────────────────────────────────
        printf("Enter Current Fill Level (0-100%%): ");
        scanf("%f", &b.fill_level);
        if (b.fill_level < 0 || b.fill_level > 100) {
            printf("Invalid fill level!\n");
            i--; continue;
        }

        // ── Grid Coordinates ──────────────────────────────────────────────────
        printf("Enter Location Coordinates (x y): ");
        scanf("%d %d", &b.x, &b.y);

        // ── Auto-compute fields ───────────────────────────────────────────────
        long long int id = generateBinID(b.waste_type, b.zone);
        if (id < 0) { i--; continue; }

        b.bin_id          = id;
        b.collected_today = false;
        b.wpi             = computeWPI(b.fill_level, b.waste_type);
        strcpy(b.last_collection, "N/A");

        // Assign priority immediately on creation for reference
        if (b.fill_level >= FILL_THRESH_HIGH || b.wpi >= WPI_THRESH_HIGH)
            b.priority = PRI_HIGH;
        else if (b.fill_level >= FILL_THRESH_MED || b.wpi >= WPI_THRESH_MED)
            b.priority = PRI_MED;
        else
            b.priority = PRI_LOW;

        // ── Write to CSV ──────────────────────────────────────────────────────
        fprintf(fp, "%lld,%d,%s,%.2f,%.2f,%d,%d,%.2f,%d\n",
                b.bin_id,
                b.zone,
                getWasteTypeString(b.waste_type),
                b.capacity,
                b.fill_level,
                b.x,
                b.y,
                b.wpi,
                (int)b.collected_today);

        printf("[OK] Bin %lld created | Zone %d | %s | Fill: %.1f%% | WPI: %.2f | Priority: %s\n",
               b.bin_id, b.zone,
               getWasteTypeString(b.waste_type),
               b.fill_level, b.wpi,
               getPriorityString(b.priority));
    }

    fclose(fp);
    printf("\n========================================\n");
}

// ── Load all bins from CSV into global bins[] array ───────────────────────────
// WPI is recomputed on every load to stay consistent with fill level.
// Returns count of bins loaded.
int loadBinsFromCSV() {
    FILE *fp = fopen(BINS_CSV, "r");
    if (!fp) {
        printf("[INFO] No bin database found. Create bins first.\n");
        return 0;
    }

    char line[256];
    fgets(line, sizeof(line), fp); // skip header

    int   count = 0;
    char  wasteTypeStr[20];

    while (count < MAX_BINS &&
           fscanf(fp, "%lld,%d,%19[^,],%f,%f,%d,%d,%f,%d\n",
                  &bins[count].bin_id,
                  &bins[count].zone,
                  wasteTypeStr,
                  &bins[count].capacity,
                  &bins[count].fill_level,
                  &bins[count].x,
                  &bins[count].y,
                  &bins[count].wpi,
                  (int *)&bins[count].collected_today) == 9) {

        bins[count].waste_type = parseWasteType(wasteTypeStr);

        // Always recompute WPI fresh to reflect any fill_level changes
        bins[count].wpi = computeWPI(bins[count].fill_level,
                                     bins[count].waste_type);
        count++;
    }

    fclose(fp);
    return count;
}

// ── Save global bins[] array back to bins.csv ─────────────────────────────────
// Called after any operation that modifies bin state (throwWaste, collection).
void saveBinsToCSV(int total) {
    FILE *fp = fopen(BINS_CSV, "w");
    if (!fp) {
        printf("[ERROR] Cannot save bin data!\n");
        return;
    }

    fprintf(fp, "bin_id,zone,waste_type,capacity,fill_level,x,y,wpi,collected_today\n");

    for (int i = 0; i < total; i++) {
        fprintf(fp, "%lld,%d,%s,%.2f,%.2f,%d,%d,%.2f,%d\n",
                bins[i].bin_id,
                bins[i].zone,
                getWasteTypeString(bins[i].waste_type),
                bins[i].capacity,
                bins[i].fill_level,
                bins[i].x,
                bins[i].y,
                bins[i].wpi,
                (int)bins[i].collected_today);
    }

    fclose(fp);
}

// ── Sort bins[] HIGH → MED → LOW by priority (bubble sort) ───────────────────
void sortBinsByPriority(int totalBins) {
    for (int i = 0; i < totalBins - 1; i++) {
        for (int j = 0; j < totalBins - i - 1; j++) {
            if (bins[j].priority < bins[j + 1].priority) {
                Bin tmp     = bins[j];
                bins[j]     = bins[j + 1];
                bins[j + 1] = tmp;
            }
        }
    }
}

// ── Compute and display all bin priorities, sorted HIGH to LOW ────────────────
// Also used internally by route optimization to get the priority-sorted list.
// Returns count of HIGH priority (critical) bins.
int identifyCriticalBins() {
    printf("\n========================================");
    printf("\n    IDENTIFY CRITICAL BINS MODULE       ");
    printf("\n========================================\n");

    int totalBins = loadBinsFromCSV();
    if (totalBins == 0) {
        printf("No bins available.\n");
        return 0;
    }

    int criticalCount = 0, medCount = 0, lowCount = 0;

    // Assign priority to every bin based on WPI and fill thresholds
    for (int i = 0; i < totalBins; i++) {

        // Already collected today → force LOW, skip threshold checks
        if (bins[i].collected_today) {
            bins[i].priority = PRI_LOW;
            lowCount++;
            continue;
        }

        // Recompute WPI to ensure it reflects latest fill level
        bins[i].wpi = computeWPI(bins[i].fill_level, bins[i].waste_type);

        bool fillHigh = (bins[i].fill_level >= FILL_THRESH_HIGH);
        bool wpiHigh  = (bins[i].wpi        >= WPI_THRESH_HIGH);
        bool fillMed  = (bins[i].fill_level >= FILL_THRESH_MED);
        bool wpiMed   = (bins[i].wpi        >= WPI_THRESH_MED);

        if (fillHigh || wpiHigh) {
            bins[i].priority = PRI_HIGH;
            criticalCount++;
        } else if (fillMed || wpiMed) {
            bins[i].priority = PRI_MED;
            medCount++;
        } else {
            bins[i].priority = PRI_LOW;
            lowCount++;
        }
    }

    // Sort so HIGH bins appear first in the display and route planning
    sortBinsByPriority(totalBins);

    // ── Display Report Table ──────────────────────────────────────────────────
    printf("\n%-14s %-6s %-10s %-9s %-7s %-8s  %s\n",
           "Bin ID", "Zone", "WasteType", "Fill(%)", "WPI",
           "Priority", "Status");
    printf("--------------------------------------------------------------------------\n");

    for (int i = 0; i < totalBins; i++) {
        printf("%-14lld %-6d %-10s %-9.1f %-7.2f %-8s  ",
               bins[i].bin_id,
               bins[i].zone,
               getWasteTypeString(bins[i].waste_type),
               bins[i].fill_level,
               bins[i].wpi,
               getPriorityString(bins[i].priority));

        if      (bins[i].collected_today)        printf("[Collected Today]");
        else if (bins[i].priority == PRI_HIGH)   printf("*** CRITICAL - NEEDS COLLECTION ***");
        else if (bins[i].priority == PRI_MED)    printf("MONITORING - May Need Collection Soon");
        else                                     printf("OK - No Immediate Action Needed");

        printf("\n");
    }

    printf("--------------------------------------------------------------------------\n");
    printf("Total Bins      : %d\n", totalBins);
    printf("HIGH (Critical) : %d  [Fill >= %.0f%% OR WPI >= %.0f]\n",
           criticalCount, FILL_THRESH_HIGH, WPI_THRESH_HIGH);
    printf("MEDIUM (MONITORING)         : %d  [Fill >= %.0f%% OR WPI >= %.0f]\n",
           medCount,      FILL_THRESH_MED,  WPI_THRESH_MED);
    printf("LOW  (OK)       : %d\n", lowCount);
    printf("========================================\n");

    return criticalCount;
}
// ══════════════════════════════════════════════════════════════════════════════
//  DRIVER MANAGEMENT MODULE
//  Handles: Driver ID generation, driver creation, CSV load/save/view,
//           sick leave request raising, suspension alerts to admin.
//  Driver ID format:
//    1xxx = standard driver
//    8xxx = hazardous material certified driver
//  One driver per vehicle (1:1 ratio enforced by design).
//  Max daily hours = 5hrs (DRIVER_MAX_HOURS constant).
// ══════════════════════════════════════════════════════════════════════════════

// ── Generate next unique Driver ID based on hazmat flag ───────────────────────
// Standard drivers: 1001, 1002, ...
// Hazmat drivers  : 8001, 8002, ...
// Scans existing CSV to find highest serial in the relevant prefix range.
int generateDriverID(bool hasHazmat) {
    FILE *fp = fopen(DRIVERS_CSV, "r");

    int prefix    = hasHazmat ? 8000 : 1000;
    int maxSerial = 0;

    if (fp == NULL) return prefix + 1; // First driver of this type

    char line[512];
    if (fgets(line, sizeof(line), fp) == NULL) { // skip header
        fclose(fp);
        return prefix + 1;
    }

    int existingID;
    while (fgets(line, sizeof(line), fp) != NULL) {
        if (sscanf(line, "%d,", &existingID) == 1) {
            if (hasHazmat && existingID >= 8000) {
                int serial = existingID - 8000;
                if (serial > maxSerial) maxSerial = serial;
            } else if (!hasHazmat && existingID >= 1000 && existingID < 8000) {
                int serial = existingID - 1000;
                if (serial > maxSerial) maxSerial = serial;
            }
        }
    }

    fclose(fp);
    return prefix + (maxSerial + 1);
}

// ── Register new drivers into drivers.csv ────────────────────────────────────
// Admin enters name, phone, hazmat flag.
// Max daily hours is fixed at DRIVER_MAX_HOURS (5hrs) per system spec.
// Every driver starts unassigned, available, not suspended.
void createDriver() {
    int n;
    printf("\n========================================");
    printf("\n     CREATE DRIVER RECORDS MODULE       ");
    printf("\n========================================\n");
    printf("Enter the Number of Driver Records to Create: ");
    scanf("%d", &n);

    FILE *fp = fopen(DRIVERS_CSV, "a");
    if (!fp) {
        printf("[ERROR] Cannot open driver database.\n");
        return;
    }

    // Write header only once when file is new or empty
    if (isCsvEmpty(DRIVERS_CSV)) {
        fprintf(fp,
            "driver_id,name,phone,vehicle_id,available,"
            "suspended,hazmat,hours_today,max_hours\n");
    }

    for (int i = 0; i < n; i++) {
        Driver d;
        int hazChoice;

        printf("\n--- Driver %d Details ---", i + 1);

        // ── Name ──────────────────────────────────────────────────────────────
        printf("\nEnter Full Name: ");
        scanf(" %[^\n]", d.name); // leading space clears newline from buffer

        // ── Phone ─────────────────────────────────────────────────────────────
        printf("Enter 10-Digit Phone Number: ");
        scanf("%s", d.phone);

        // ── Hazmat Certification ──────────────────────────────────────────────
        printf("Hazardous Material Certified? (1=Yes, 0=No): ");
        scanf("%d", &hazChoice);
        d.has_hazmat_license = (hazChoice == 1);

        // ── Auto-generate Driver ID ───────────────────────────────────────────
        d.driver_id = generateDriverID(d.has_hazmat_license);

        // ── System Defaults ───────────────────────────────────────────────────
        // Max hours fixed at system constant (5hrs per spec)
        d.max_daily_hours    = DRIVER_MAX_HOURS;
        d.assigned_vehicle_id = 0;
        d.is_available       = true;
        d.is_suspended       = false;
        d.hours_worked_today = 0.0f;

        // ── Write to CSV ──────────────────────────────────────────────────────
        fprintf(fp, "%d,%s,%s,%d,%d,%d,%d,%.2f,%.2f\n",
                d.driver_id,
                d.name,
                d.phone,
                d.assigned_vehicle_id,
                (int)d.is_available,
                (int)d.is_suspended,
                (int)d.has_hazmat_license,
                d.hours_worked_today,
                d.max_daily_hours);
        fflush(fp);

        printf("[OK] Driver ID %d assigned to %s | Hazmat: %s | Max Hours: %.1f\n",
               d.driver_id, d.name,
               d.has_hazmat_license ? "YES" : "NO",
               d.max_daily_hours);
    }

    fclose(fp);
    printf("\n========================================\n");
}

// ── Load all drivers from CSV into global drivers[] array ─────────────────────
// Returns count of drivers loaded.
int loadDriversFromCSV() {
    FILE *fp = fopen(DRIVERS_CSV, "r");
    if (!fp) return 0;

    char line[512];
    if (fgets(line, sizeof(line), fp) == NULL) {
        fclose(fp);
        return 0;
    }

    int count = 0;
    while (count < MAX_DRIVERS &&
           fscanf(fp, "%d,%[^,],%[^,],%d,%d,%d,%d,%f,%f\n",
                  &drivers[count].driver_id,
                  drivers[count].name,
                  drivers[count].phone,
                  &drivers[count].assigned_vehicle_id,
                  (int *)&drivers[count].is_available,
                  (int *)&drivers[count].is_suspended,
                  (int *)&drivers[count].has_hazmat_license,
                  &drivers[count].hours_worked_today,
                  &drivers[count].max_daily_hours) == 9) {
        count++;
    }

    fclose(fp);
    return count;
}

// ── Save global drivers[] array back to drivers.csv ───────────────────────────
// Called after assignment, hour updates, suspension changes.
void saveDriversToCSV(int total) {
    FILE *fp = fopen(DRIVERS_CSV, "w");
    if (!fp) {
        printf("[ERROR] Cannot save driver data!\n");
        return;
    }

    fprintf(fp,
        "driver_id,name,phone,vehicle_id,available,"
        "suspended,hazmat,hours_today,max_hours\n");

    for (int i = 0; i < total; i++) {
        fprintf(fp, "%d,%s,%s,%d,%d,%d,%d,%.2f,%.2f\n",
                drivers[i].driver_id,
                drivers[i].name,
                drivers[i].phone,
                drivers[i].assigned_vehicle_id,
                (int)drivers[i].is_available,
                (int)drivers[i].is_suspended,
                (int)drivers[i].has_hazmat_license,
                drivers[i].hours_worked_today,
                drivers[i].max_daily_hours);
    }

    fclose(fp);
}

// ── Display all driver records in a formatted table ───────────────────────────
void viewDrivers() {
    printf("\n========================================");
    printf("\n      VIEW ALL DRIVERS MODULE          ");
    printf("\n========================================\n");

    int total = loadDriversFromCSV();
    if (total == 0) {
        printf("No driver records found.\n");
        return;
    }

    printf("%-10s %-20s %-12s %-10s %-8s %-6s %-10s\n",
           "ID", "Name", "Phone", "Vehicle",
           "Hazmat", "Hrs", "Status");
    printf("------------------------------------------------------------------------\n");

    for (int i = 0; i < total; i++) {
        const char* status;
        if      (drivers[i].is_suspended)   status = "SUSPENDED";
        else if (!drivers[i].is_available)  status = "ON-ROUTE";
        else                                status = "READY";

        printf("%-10d %-20s %-12s %-10d %-8s %-6.1f %-10s\n",
               drivers[i].driver_id,
               drivers[i].name,
               drivers[i].phone,
               drivers[i].assigned_vehicle_id,
               drivers[i].has_hazmat_license ? "YES" : "NO",
               drivers[i].hours_worked_today,
               status);
    }

    printf("------------------------------------------------------------------------\n");
    printf("Total Drivers: %d\n", total);
    printf("========================================\n");
}

// ── Driver raises sick leave request → saved to requests.csv ─────────────────
// Driver is identified by ID. System auto-flags as PENDING.
// Admin views this from the Request Management menu and approves/rejects.
void driverRaiseSickLeave() {
    printf("\n========================================");
    printf("\n     DRIVER SICK LEAVE REQUEST          ");
    printf("\n========================================\n");

    int total = loadDriversFromCSV();
    if (total == 0) {
        printf("No drivers found.\n");
        return;
    }

    int driverID;
    printf("Enter Your Driver ID: ");
    scanf("%d", &driverID);

    // Find the driver
    int idx = -1;
    for (int i = 0; i < total; i++) {
        if (drivers[i].driver_id == driverID) { idx = i; break; }
    }

    if (idx == -1) {
        printf("[ERROR] Driver ID %d not found.\n", driverID);
        return;
    }

    if (drivers[idx].is_suspended) {
        printf("[WARN] Driver %s is already suspended.\n", drivers[idx].name);
    }

    // Find the vehicle assigned to this driver to get zone info
    int totalVehicles = loadVehiclesFromCSV();
    int zone = 0;
    int vehicleID = drivers[idx].assigned_vehicle_id;
    char vehicleReg[15]  = "UNASSIGNED";
    char vehicleType[30] = "UNASSIGNED";

    for (int i = 0; i < totalVehicles; i++) {
        if (vehicles[i].vehicle_id == vehicleID) {
            zone = vehicles[i].assigned_zone;
            strcpy(vehicleReg,  vehicles[i].registration);
            strcpy(vehicleType, vehicles[i].type);
            break;
        }
    }

    // ── Write request to requests.csv ─────────────────────────────────────────
    int reqID = generateRequestID();

    bool isNew = isCsvEmpty(REQUESTS_CSV);
    FILE *fp = fopen(REQUESTS_CSV, "a");
    if (!fp) {
        printf("[ERROR] Cannot open requests file.\n");
        return;
    }

    if (isNew) {
        fprintf(fp,
            "request_id,reason,vehicle_id,vehicle_reg,vehicle_type,"
            "zone,last_bin_id,driver_id,driver_name,hours_worked,status\n");
    }

    fprintf(fp, "%d,%s,%d,%s,%s,%d,%lld,%d,%s,%.2f,PENDING\n",
            reqID,
            REQ_SICK,
            vehicleID,
            vehicleReg,
            vehicleType,
            zone,
            0LL,           // no specific bin — driver is at base
            drivers[idx].driver_id,
            drivers[idx].name,
            drivers[idx].hours_worked_today);

    fclose(fp);

    printf("[OK] Sick leave request #%d submitted for Driver %s.\n",
           reqID, drivers[idx].name);
    printf("     Admin will review and approve/reject your leave.\n");
    printf("========================================\n");
}

// ── Auto-raise suspension alert when a suspended driver causes zone gap ────────
// Called internally by assignment module when a zone has no available driver
// due to suspension. Saves alert to requests.csv for admin attention.
void raiseSuspensionAlert(int driverIdx, int zone) {
    int totalVehicles = loadVehiclesFromCSV();
    int vehicleID = drivers[driverIdx].assigned_vehicle_id;
    char vehicleReg[15]  = "UNASSIGNED";
    char vehicleType[30] = "UNASSIGNED";

    for (int i = 0; i < totalVehicles; i++) {
        if (vehicles[i].vehicle_id == vehicleID) {
            strcpy(vehicleReg,  vehicles[i].registration);
            strcpy(vehicleType, vehicles[i].type);
            break;
        }
    }

    int reqID  = generateRequestID();
    bool isNew = isCsvEmpty(REQUESTS_CSV);

    FILE *fp = fopen(REQUESTS_CSV, "a");
    if (!fp) return;

    if (isNew) {
        fprintf(fp,
            "request_id,reason,vehicle_id,vehicle_reg,vehicle_type,"
            "zone,last_bin_id,driver_id,driver_name,hours_worked,status\n");
    }

    fprintf(fp, "%d,%s,%d,%s,%s,%d,%lld,%d,%s,%.2f,PENDING\n",
            reqID,
            REQ_SUSPENDED,
            vehicleID,
            vehicleReg,
            vehicleType,
            zone,
            0LL,
            drivers[driverIdx].driver_id,
            drivers[driverIdx].name,
            drivers[driverIdx].hours_worked_today);

    fclose(fp);

    printf("[ALERT] Suspension alert #%d raised for Driver %s in Zone %d.\n",
           reqID, drivers[driverIdx].name, zone);
}
// ══════════════════════════════════════════════════════════════════════════════
//  VEHICLE & DRIVER ASSIGNMENT MODULE
//  Handles: Assigning one driver to one vehicle (1:1 per spec),
//           checking hazmat license for hazardous zones,
//           checking hours worked before assignment,
//           finding replacement driver in same zone or nearest zone,
//           saving updated assignments back to both CSVs,
//           raising alerts to requests.csv when no driver is found.
//
//  Flow:
//  1. Load all vehicles and drivers from CSV
//  2. For each vehicle, find best available driver in same zone
//     - Must not be suspended
//     - Must not have exceeded max daily hours
//     - If bin in zone is HAZARDOUS, must have hazmat license
//  3. If no driver in same zone → search nearest zone by zone number proximity
//  4. If still none → raise NO_BACKUP request to admin
//  5. Save updated vehicles.csv and drivers.csv
// ══════════════════════════════════════════════════════════════════════════════

// ── Find zone of a driver by their assigned vehicle's zone ────────────────────
// Returns zone number or 0 if unassigned
int getDriverZone(int driverIdx, int totalVehicles) {
    int vid = drivers[driverIdx].assigned_vehicle_id;
    if (vid == 0) return 0;
    for (int i = 0; i < totalVehicles; i++) {
        if (vehicles[i].vehicle_id == vid)
            return vehicles[i].assigned_zone;
    }
    return 0;
}

// ── Check if a zone has any HAZARDOUS bins ────────────────────────────────────
// Used to decide whether hazmat license is required for that zone's vehicle
bool zoneHasHazardousBins(int zone, int totalBins) {
    for (int i = 0; i < totalBins; i++) {
        if (bins[i].zone == zone &&
            bins[i].waste_type == HAZARDOUS &&
            !bins[i].collected_today)
            return true;
    }
    return false;
}

// ── Find best available driver for a given zone ───────────────────────────────
// Priority: same zone first, then nearest zone by number difference
// Rules: not suspended, hours not exceeded, hazmat if zone needs it
// Returns index into drivers[] or -1 if none found
int findBestDriver(int targetZone, bool needsHazmat,
                   int totalDrivers, int totalVehicles) {

    int bestIdx      = -1;
    int bestZoneDiff = 9999;

    for (int i = 0; i < totalDrivers; i++) {

        // Skip suspended drivers
        if (drivers[i].is_suspended) continue;

        // Skip drivers who have used up their daily hours
        if (drivers[i].hours_worked_today >= drivers[i].max_daily_hours) continue;

        // Skip drivers already assigned and unavailable
        if (!drivers[i].is_available) continue;

        // Skip if hazmat needed but driver not certified
        if (needsHazmat && !drivers[i].has_hazmat_license) continue;

        // Determine which zone this driver currently belongs to
        int driverZone = getDriverZone(i, totalVehicles);

        // Zone difference — 0 means same zone (best)
        int zoneDiff = abs(driverZone - targetZone);

        if (zoneDiff < bestZoneDiff) {
            bestZoneDiff = zoneDiff;
            bestIdx      = i;
        }
    }

    return bestIdx;
}

// ── Raise a request when no driver can be found for a vehicle ─────────────────
// Saves to requests.csv with reason NO_BACKUP for admin to address
void raiseNoDriverRequest(int vehicleIdx, int zone) {
    int reqID  = generateRequestID();
    bool isNew = isCsvEmpty(REQUESTS_CSV);

    FILE *fp = fopen(REQUESTS_CSV, "a");
    if (!fp) {
        printf("[ERROR] Cannot open requests file.\n");
        return;
    }

    if (isNew) {
        fprintf(fp,
            "request_id,reason,vehicle_id,vehicle_reg,vehicle_type,"
            "zone,last_bin_id,driver_id,driver_name,hours_worked,status\n");
    }

    // No driver available — driver fields left as 0 / NONE
    fprintf(fp, "%d,%s,%d,%s,%s,%d,%lld,%d,%s,%.2f,PENDING\n",
            reqID,
            REQ_NO_BACKUP,
            vehicles[vehicleIdx].vehicle_id,
            vehicles[vehicleIdx].registration,
            vehicles[vehicleIdx].type,
            zone,
            0LL,   // no specific bin
            0,     // no driver
            "NONE",
            0.0f);

    fclose(fp);

    printf("[ALERT] No driver available for Vehicle %d (Zone %d)."
           " Request #%d raised to admin.\n",
           vehicles[vehicleIdx].vehicle_id, zone, reqID);
}

// ── Raise hours-exceeded request when a driver finishes route over limit ───────
// Driver completes current route to hub (per spec) then request is logged
void raiseHoursExceededRequest(int driverIdx, int vehicleIdx, int zone) {
    int reqID  = generateRequestID();
    bool isNew = isCsvEmpty(REQUESTS_CSV);

    FILE *fp = fopen(REQUESTS_CSV, "a");
    if (!fp) return;

    if (isNew) {
        fprintf(fp,
            "request_id,reason,vehicle_id,vehicle_reg,vehicle_type,"
            "zone,last_bin_id,driver_id,driver_name,hours_worked,status\n");
    }

    fprintf(fp, "%d,%s,%d,%s,%s,%d,%lld,%d,%s,%.2f,PENDING\n",
            reqID,
            REQ_HOURS_OVER,
            vehicles[vehicleIdx].vehicle_id,
            vehicles[vehicleIdx].registration,
            vehicles[vehicleIdx].type,
            zone,
            0LL,
            drivers[driverIdx].driver_id,
            drivers[driverIdx].name,
            drivers[driverIdx].hours_worked_today);

    fclose(fp);

    printf("[ALERT] Driver %s exceeded %.1f hrs on Vehicle %d."
           " Request #%d raised. Driver heads to hub.\n",
           drivers[driverIdx].name,
           drivers[driverIdx].hours_worked_today,
           vehicles[vehicleIdx].vehicle_id,
           reqID);
}

// ══════════════════════════════════════════════════════════════════════════════
//  MAIN ASSIGNMENT FUNCTION — called from admin menu
//  Assigns one driver to each vehicle, saves CSVs, prints summary table.
// ══════════════════════════════════════════════════════════════════════════════
void assignDriversToVehicles() {
    printf("\n========================================");
    printf("\n   VEHICLE & DRIVER ASSIGNMENT MODULE   ");
    printf("\n========================================\n");

    int totalVehicles = loadVehiclesFromCSV();
    int totalDrivers  = loadDriversFromCSV();
    int totalBins     = loadBinsFromCSV();

    if (totalVehicles == 0) {
        printf("[ERROR] No vehicles registered. Create vehicles first.\n");
        return;
    }
    if (totalDrivers == 0) {
        printf("[ERROR] No drivers registered. Create drivers first.\n");
        return;
    }

    // Track which drivers have already been assigned this run
    // to avoid giving the same driver to two vehicles
    bool driverAssignedThisRun[MAX_DRIVERS] = { false };

    int assigned   = 0;
    int unassigned = 0;

    printf("\nAssigning drivers to vehicles...\n");
    printf("------------------------------------------------------------------------\n");

    for (int v = 0; v < totalVehicles; v++) {

        // Skip vehicles under maintenance
        if (vehicles[v].under_maintenance) {
            printf("[SKIP] Vehicle %d (%s) is under maintenance.\n",
                   vehicles[v].vehicle_id, vehicles[v].registration);
            continue;
        }

        int zone        = vehicles[v].assigned_zone;
        bool needHazmat = zoneHasHazardousBins(zone, totalBins);

        // ── Check if vehicle already has a valid assigned driver ───────────────
        // If driver is available and hours not exceeded, keep the assignment
        if (vehicles[v].assigned_driver_id != 0) {
            bool found = false;
            for (int d = 0; d < totalDrivers; d++) {
                if (drivers[d].driver_id == vehicles[v].assigned_driver_id) {

                    // Driver is still valid — not suspended, hours OK
                    if (!drivers[d].is_suspended &&
                        drivers[d].hours_worked_today < drivers[d].max_daily_hours &&
                        drivers[d].is_available) {

                        // If zone needs hazmat but driver lacks license
                        if (needHazmat && !drivers[d].has_hazmat_license) {
                            printf("[WARN] Vehicle %d zone needs HAZMAT but "
                                   "driver %d lacks license. Reassigning.\n",
                                   vehicles[v].vehicle_id,
                                   drivers[d].driver_id);
                            break; // fall through to reassign
                        }

                        printf("[KEEP] Vehicle %d → Driver %d (%s) [already assigned, valid]\n",
                               vehicles[v].vehicle_id,
                               drivers[d].driver_id,
                               drivers[d].name);
                        driverAssignedThisRun[d] = true;
                        found = true;
                        assigned++;
                    } else {
                        // Driver's hours exceeded — raise request, find replacement
                        if (drivers[d].hours_worked_today >= drivers[d].max_daily_hours) {
                            raiseHoursExceededRequest(d, v, zone);
                        }
                        // Suspended driver — raise alert
                        if (drivers[d].is_suspended) {
                            raiseSuspensionAlert(d, zone);
                        }
                    }
                    break;
                }
            }
            if (found) continue; // move to next vehicle
        }

        // ── Find best available driver for this vehicle ────────────────────────
        // Temporarily mark already-assigned drivers as unavailable for search
        bool savedAvail[MAX_DRIVERS];
        for (int d = 0; d < totalDrivers; d++) {
            savedAvail[d] = drivers[d].is_available;
            if (driverAssignedThisRun[d])
                drivers[d].is_available = false; // block double-assignment
        }

        int bestD = findBestDriver(zone, needHazmat, totalDrivers, totalVehicles);

        // Restore availability flags after search
        for (int d = 0; d < totalDrivers; d++)
            drivers[d].is_available = savedAvail[d];

        if (bestD == -1) {
            // No driver found anywhere — raise admin request
            raiseNoDriverRequest(v, zone);
            vehicles[v].assigned_driver_id = 0;
            unassigned++;
            continue;
        }

        // ── Perform the assignment ─────────────────────────────────────────────
        int driverZone = getDriverZone(bestD, totalVehicles);
        if (driverZone != zone) {
            printf("[CROSS-ZONE] No driver in Zone %d. Using Driver %d (%s)"
                   " from Zone %d.\n",
                   zone,
                   drivers[bestD].driver_id,
                   drivers[bestD].name,
                   driverZone);
        }

        // Link vehicle → driver and driver → vehicle
        vehicles[v].assigned_driver_id       = drivers[bestD].driver_id;
        drivers[bestD].assigned_vehicle_id   = vehicles[v].vehicle_id;
        drivers[bestD].is_available          = false; // now on-route
        driverAssignedThisRun[bestD]         = true;

        printf("[ASSIGNED] Vehicle %d (%s, Zone %d) → Driver %d (%s)"
               " | Hazmat: %s | Hrs Left: %.1f\n",
               vehicles[v].vehicle_id,
               vehicles[v].type,
               zone,
               drivers[bestD].driver_id,
               drivers[bestD].name,
               drivers[bestD].has_hazmat_license ? "YES" : "NO",
               drivers[bestD].max_daily_hours - drivers[bestD].hours_worked_today);

        assigned++;
    }

    // ── Save updated assignments back to CSVs ──────────────────────────────────
    saveVehiclesToCSV(totalVehicles);
    saveDriversToCSV(totalDrivers);

    printf("------------------------------------------------------------------------\n");
    printf("Assignment Complete: %d assigned | %d unassigned (check requests)\n",
           assigned, unassigned);
    printf("========================================\n");
}

// ── View current vehicle-driver assignments ───────────────────────────────────
// Displays a summary table of which driver is on which vehicle
void viewAssignments() {
    printf("\n========================================");
    printf("\n    CURRENT VEHICLE-DRIVER ASSIGNMENTS  ");
    printf("\n========================================\n");

    int totalVehicles = loadVehiclesFromCSV();
    int totalDrivers  = loadDriversFromCSV();

    if (totalVehicles == 0) {
        printf("No vehicles found.\n");
        return;
    }

    printf("%-10s %-12s %-6s %-10s %-20s %-8s %-10s\n",
           "VehicleID", "Reg", "Zone", "DriverID",
           "DriverName", "Hazmat", "DrvrStatus");
    printf("------------------------------------------------------------------------\n");

    for (int v = 0; v < totalVehicles; v++) {
        int driverID = vehicles[v].assigned_driver_id;
        char driverName[50]  = "UNASSIGNED";
        char hazmat[5]       = "N/A";
        char driverStatus[15]= "N/A";

        // Find matching driver
        for (int d = 0; d < totalDrivers; d++) {
            if (drivers[d].driver_id == driverID) {
                strcpy(driverName, drivers[d].name);
                strcpy(hazmat, drivers[d].has_hazmat_license ? "YES" : "NO");

                if      (drivers[d].is_suspended)  strcpy(driverStatus, "SUSPENDED");
                else if (!drivers[d].is_available) strcpy(driverStatus, "ON-ROUTE");
                else                               strcpy(driverStatus, "READY");
                break;
            }
        }

        printf("%-10d %-12s %-6d %-10d %-20s %-8s %-10s\n",
               vehicles[v].vehicle_id,
               vehicles[v].registration,
               vehicles[v].assigned_zone,
               driverID,
               driverName,
               hazmat,
               driverStatus);
    }

    printf("------------------------------------------------------------------------\n");
    printf("========================================\n");
}
// ══════════════════════════════════════════════════════════════════════════════
//  ROUTE OPTIMIZATION & ROUTE ASSIGNMENT MODULE
//  Handles: Per-zone parallel route planning, bin visit order by priority
//           and minimum fuel distance, mid-route hub return on capacity/fuel,
//           cross-zone vehicle borrowing, SOS requests to requests.csv,
//           saving final routes to routes.csv.
//
//  Hub is at (HUB_X, HUB_Y) = (0,0) for all zones (per spec).
//
//  Distance metric: min(Manhattan, Diagonal/Chebyshev) for fuel efficiency.
//  Fuel math : fuelNeeded(dist) = (dist/10) * vehicle.fuel_consumption_rate
//  Weight collected per bin: (fill_level/100.0) * capacity  kg
//
//  Flow per zone:
//  1. Collect all HIGH priority bins in zone (sorted by WPI desc)
//     then MED, then LOW
//  2. All vehicles in zone run simultaneously (parallel)
//  3. Bins split across vehicles round-robin by priority order
//  4. Each vehicle: hub → bin1 → bin2 → ... → hub
//     If mid-route capacity full OR fuel low → return hub, reload/refuel,
//     continue remaining bins
//  5. If vehicle cannot reach next bin + return hub on current fuel →
//     raise FUEL_OUT SOS request
//  6. If zone has no available vehicle → borrow nearest available
//     vehicle from another zone
//  7. All routes saved to routes.csv (appended with run sequence)
// ══════════════════════════════════════════════════════════════════════════════

// ── Raise SOS fuel-out or capacity request from a vehicle mid-route ───────────
// Saves to requests.csv for admin to view under Request Management
void raiseVehicleSOS(int vehicleIdx, int zone,
                     long long int lastBinID, const char* reason) {
    int reqID  = generateRequestID();
    bool isNew = isCsvEmpty(REQUESTS_CSV);

    FILE *fp = fopen(REQUESTS_CSV, "a");
    if (!fp) {
        printf("[ERROR] Cannot open requests file for SOS.\n");
        return;
    }

    if (isNew) {
        fprintf(fp,
            "request_id,reason,vehicle_id,vehicle_reg,vehicle_type,"
            "zone,last_bin_id,driver_id,driver_name,hours_worked,status\n");
    }

    // Find driver name for this vehicle
    int totalDrivers = loadDriversFromCSV();
    char driverName[50]  = "UNKNOWN";
    float hoursWorked    = 0.0f;
    int driverID         = vehicles[vehicleIdx].assigned_driver_id;

    for (int d = 0; d < totalDrivers; d++) {
        if (drivers[d].driver_id == driverID) {
            strcpy(driverName, drivers[d].name);
            hoursWorked = drivers[d].hours_worked_today;
            break;
        }
    }

    fprintf(fp, "%d,%s,%d,%s,%s,%d,%lld,%d,%s,%.2f,PENDING\n",
            reqID,
            reason,
            vehicles[vehicleIdx].vehicle_id,
            vehicles[vehicleIdx].registration,
            vehicles[vehicleIdx].type,
            zone,
            lastBinID,
            driverID,
            driverName,
            hoursWorked);

    fclose(fp);

    printf("[SOS] Request #%d → Vehicle %d (%s) | Reason: %s | Zone: %d\n",
           reqID,
           vehicles[vehicleIdx].vehicle_id,
           vehicles[vehicleIdx].registration,
           reason, zone);
}

// ── Write one route entry to routes.csv ───────────────────────────────────────
// Called once per vehicle per optimization run.
// routeStr is a human-readable string like "HUB→BinA→BinB→HUB"
void saveRouteToCSV(int runID, int vehicleID, const char* vehicleReg,
                    int zone, int driverID, const char* driverName,
                    float totalDist, float fuelUsed, float loadKg,
                    int binsCollected, const char* routeStr) {

    bool isNew = isCsvEmpty(ROUTES_CSV);
    FILE *fp   = fopen(ROUTES_CSV, "a");
    if (!fp) {
        printf("[ERROR] Cannot open routes.csv.\n");
        return;
    }

    if (isNew) {
        fprintf(fp,
            "run_id,vehicle_id,vehicle_reg,zone,driver_id,driver_name,"
            "total_distance,fuel_used,load_kg,bins_collected,route\n");
    }

    fprintf(fp, "%d,%d,%s,%d,%d,%s,%.2f,%.2f,%.2f,%d,\"%s\"\n",
            runID,
            vehicleID,
            vehicleReg,
            zone,
            driverID,
            driverName,
            totalDist,
            fuelUsed,
            loadKg,
            binsCollected,
            routeStr);

    fclose(fp);
}

// ── Get next run ID by scanning routes.csv ────────────────────────────────────
int generateRunID() {
    FILE *fp = fopen(ROUTES_CSV, "r");
    if (!fp) return 1;
    char line[1024];
    fgets(line, sizeof(line), fp); // skip header
    int maxID = 0, id;
    while (fgets(line, sizeof(line), fp)) {
        if (sscanf(line, "%d,", &id) == 1 && id > maxID)
            maxID = id;
    }
    fclose(fp);
    return maxID + 1;
}

// ── Find nearest available vehicle from another zone ──────────────────────────
// Used when a zone has no available vehicle of its own.
// Returns index into vehicles[] or -1 if none found.
int findNearestAvailableVehicle(int targetZone, int totalVehicles,
                                bool needHazmat) {
    int bestIdx      = -1;
    int bestZoneDiff = 9999;

    for (int v = 0; v < totalVehicles; v++) {
        if (!vehicles[v].is_available)    continue;
        if (vehicles[v].under_maintenance) continue;
        if (vehicles[v].assigned_zone == targetZone) continue; // same zone already tried

        // If zone needs hazmat-capable vehicle, check driver license
        if (needHazmat) {
            int totalDrivers = loadDriversFromCSV();
            bool driverHazmat = false;
            for (int d = 0; d < totalDrivers; d++) {
                if (drivers[d].driver_id == vehicles[v].assigned_driver_id) {
                    driverHazmat = drivers[d].has_hazmat_license;
                    break;
                }
            }
            if (!driverHazmat) continue;
        }

        int zoneDiff = abs(vehicles[v].assigned_zone - targetZone);
        if (zoneDiff < bestZoneDiff) {
            bestZoneDiff = zoneDiff;
            bestIdx      = v;
        }
    }

    return bestIdx;
}

// ── Plan and execute route for ONE vehicle over its assigned bin list ──────────
// binList[]    : indices into bins[] this vehicle should visit
// binCount     : how many bins in the list
// vehicleIdx   : index into vehicles[]
// zone         : zone number
// runID        : current optimization run ID (for routes.csv)
// Updates bins[].collected_today, vehicles[].current_fuel/load in memory.
void planVehicleRoute(int vehicleIdx, int* binList, int binCount,
                      int zone, int runID) {

    Vehicle *veh = &vehicles[vehicleIdx];

    // Driver info for logging
    int totalDrivers = loadDriversFromCSV();
    char driverName[50] = "UNKNOWN";
    int  driverID       = veh->assigned_driver_id;
    int  driverIdx      = -1;

    for (int d = 0; d < totalDrivers; d++) {
        if (drivers[d].driver_id == driverID) {
            strcpy(driverName, drivers[d].name);
            driverIdx = d;
            break;
        }
    }

    // Route state
    int   curX        = HUB_X;
    int   curY        = HUB_Y;
    float totalDist   = 0.0f;
    float fuelUsed    = 0.0f;
    float loadKg      = 0.0f;
    int   binsCollected = 0;

    // Route string: "HUB→BinID→BinID→HUB"
    char routeStr[2048];
    strcpy(routeStr, "HUB");

    printf("\n  [VEHICLE %d | %s | Zone %d | Driver: %s]\n",
           veh->vehicle_id, veh->registration, zone, driverName);
    printf("  Route: HUB(0,0)");

    for (int b = 0; b < binCount; b++) {
        int bi = binList[b]; // index into bins[]

        // ── Distance from current position to this bin ─────────────────────
        float distToBin = bestDist(curX, curY,
                                   bins[bi].x, bins[bi].y);

        // ── Distance from bin back to hub (for fuel safety check) ──────────
        float distBinToHub = bestDist(bins[bi].x, bins[bi].y,
                                      HUB_X, HUB_Y);

        // ── Fuel needed: current→bin + bin→hub ────────────────────────────
        float fuelToBin    = fuelNeeded(distToBin,    veh->fuel_consumption_rate);
        float fuelToHub    = fuelNeeded(distBinToHub, veh->fuel_consumption_rate);
        float fuelRequired = fuelToBin + fuelToHub;

        // ── Waste weight this bin will add ─────────────────────────────────
        float wasteWeight = (bins[bi].fill_level / 100.0f) * bins[bi].capacity;

        // ── Check 1: Not enough fuel to reach bin AND return to hub ────────
        if (veh->current_fuel < fuelRequired) {
            printf("\n  [FUEL LOW] Cannot reach Bin %lld and return."
                   " Returning to hub.\n", bins[bi].bin_id);

            // Return to hub: deduct fuel for hub return from current position
            float fuelBack = fuelNeeded(
                bestDist(curX, curY, HUB_X, HUB_Y),
                veh->fuel_consumption_rate);
            veh->current_fuel -= fuelBack;
            if (veh->current_fuel < 0) veh->current_fuel = 0;

            totalDist += bestDist(curX, curY, HUB_X, HUB_Y);
            strcat(routeStr, "→HUB(FUEL)");

            // Check if truly out of fuel (couldn't even reach hub safely)
            if (veh->current_fuel <= 0) {
                raiseVehicleSOS(vehicleIdx, zone,
                                bins[bi].bin_id, REQ_FUEL);
            }

            // Refuel at hub to full tank
            printf("  [HUB] Vehicle %d refuelled. Resuming route.\n",
                   veh->vehicle_id);
            veh->current_fuel = veh->fuel_tank_capacity;
            veh->current_load = 0.0f; // also dump load at hub
            loadKg = 0.0f;
            curX = HUB_X;
            curY = HUB_Y;
            strcat(routeStr, "→HUB(REFUEL)");

            // Retry this bin after refuel
            b--;
            continue;
        }

        // ── Check 2: Adding this bin's waste would exceed vehicle capacity ──
        if (loadKg + wasteWeight > veh->max_capacity) {
            printf("\n  [CAPACITY FULL] Load %.1fkg + %.1fkg > %.1fkg max."
                   " Returning to hub.\n",
                   loadKg, wasteWeight, veh->max_capacity);

            // Return to hub to dump load
            float fuelBack = fuelNeeded(
                bestDist(curX, curY, HUB_X, HUB_Y),
                veh->fuel_consumption_rate);

            veh->current_fuel -= fuelBack;
            if (veh->current_fuel < 0) veh->current_fuel = 0;

            totalDist += bestDist(curX, curY, HUB_X, HUB_Y);
            strcat(routeStr, "→HUB(DUMP)");

            // Dump load, refuel
            veh->current_load = 0.0f;
            loadKg = 0.0f;
            curX = HUB_X;
            curY = HUB_Y;

            // Raise SOS only if no backup available after capacity full
            // Check if any other vehicle in zone is free
            bool backupExists = false;
            int totalVehicles = loadVehiclesFromCSV();
            for (int ov = 0; ov < totalVehicles; ov++) {
                if (ov == vehicleIdx) continue;
                if (vehicles[ov].assigned_zone == zone &&
                    vehicles[ov].is_available  &&
                    !vehicles[ov].under_maintenance) {
                    backupExists = true;
                    break;
                }
            }
            if (!backupExists) {
                raiseVehicleSOS(vehicleIdx, zone,
                                bins[bi].bin_id, REQ_CAPACITY);
            }

            // Retry this bin after dumping
            b--;
            continue;
        }

        // ── All checks passed: travel to bin and collect ───────────────────
        veh->current_fuel -= fuelToBin;
        totalDist         += distToBin;
        fuelUsed          += fuelToBin;
        loadKg            += wasteWeight;
        veh->current_load  = loadKg;

        // Mark bin as collected
        bins[bi].collected_today = true;
        bins[bi].fill_level      = 0.0f;  // emptied after collection
        bins[bi].wpi             = computeWPI(0.0f, bins[bi].waste_type);

        binsCollected++;
        curX = bins[bi].x;
        curY = bins[bi].y;

        // Append to route string
        char binStr[40];
        snprintf(binStr, sizeof(binStr), "→Bin%lld(%d,%d)",
                 bins[bi].bin_id, bins[bi].x, bins[bi].y);
        strcat(routeStr, binStr);

        printf(" → Bin%lld(%d,%d)", bins[bi].bin_id,
               bins[bi].x, bins[bi].y);

        // ── Update driver hours for this leg ──────────────────────────────
        if (driverIdx >= 0) {
            drivers[driverIdx].hours_worked_today +=
                hoursToTravel(distToBin);

            // Check if hours exceeded after this leg
            if (drivers[driverIdx].hours_worked_today >=
                drivers[driverIdx].max_daily_hours) {

                // Driver completes travel to hub then logs off (per spec)
                printf("\n  [HOURS] Driver %s reached max hours."
                       " Will return to hub after this bin.\n",
                       driverName);

                raiseHoursExceededRequest(driverIdx, vehicleIdx, zone);

                // Complete return to hub then stop
                float fuelFinal = fuelNeeded(
                    bestDist(curX, curY, HUB_X, HUB_Y),
                    veh->fuel_consumption_rate);
                veh->current_fuel -= fuelFinal;
                if (veh->current_fuel < 0) veh->current_fuel = 0;
                totalDist += bestDist(curX, curY, HUB_X, HUB_Y);
                strcat(routeStr, "→HUB(DRIVER-END)");
                printf(" → HUB(DRIVER-END)\n");
                break; // stop assigning more bins to this vehicle
            }
        }
    }

    // ── Final return to hub ────────────────────────────────────────────────────
    float finalDist    = bestDist(curX, curY, HUB_X, HUB_Y);
    float finalFuel    = fuelNeeded(finalDist, veh->fuel_consumption_rate);
    veh->current_fuel -= finalFuel;
    if (veh->current_fuel < 0) veh->current_fuel = 0;
    totalDist += finalDist;
    fuelUsed  += finalFuel;
    strcat(routeStr, "→HUB");
    printf(" → HUB\n");

    // Mark vehicle as available again after route
    veh->is_available  = true;
    veh->current_load  = 0.0f;

    // ── Print per-vehicle summary ──────────────────────────────────────────────
    printf("  Summary: %d bins | %.1f dist units | %.2fL fuel | %.1fkg load\n",
           binsCollected, totalDist, fuelUsed, loadKg);

    // ── Save route to routes.csv ───────────────────────────────────────────────
    saveRouteToCSV(runID,
                   veh->vehicle_id,
                   veh->registration,
                   zone,
                   driverID,
                   driverName,
                   totalDist,
                   fuelUsed,
                   loadKg,
                   binsCollected,
                   routeStr);
}

// ══════════════════════════════════════════════════════════════════════════════
//  MAIN ROUTE OPTIMIZATION FUNCTION — called from admin menu
//  Runs all zones in parallel (simulated sequentially per zone).
//  For each zone: collects priority-sorted bins, splits across vehicles,
//  plans each vehicle's route, saves to routes.csv, updates all CSVs.
// ══════════════════════════════════════════════════════════════════════════════
void runRouteOptimization() {
    printf("\n========================================");
    printf("\n    ROUTE OPTIMIZATION MODULE           ");
    printf("\n========================================\n");

    // ── Step 1: Load all data ──────────────────────────────────────────────────
    int totalBins     = loadBinsFromCSV();
    int totalVehicles = loadVehiclesFromCSV();
    int totalDrivers  = loadDriversFromCSV();

    if (totalBins == 0) {
        printf("[ERROR] No bins found. Create bins first.\n");
        return;
    }
    if (totalVehicles == 0) {
        printf("[ERROR] No vehicles found. Create vehicles first.\n");
        return;
    }

    // ── Step 2: Compute WPI and assign priority to all bins ───────────────────
    int critCount = 0;
    for (int i = 0; i < totalBins; i++) {
        if (bins[i].collected_today) {
            bins[i].priority = PRI_LOW;
            continue;
        }
        bins[i].wpi = computeWPI(bins[i].fill_level, bins[i].waste_type);

        if (bins[i].fill_level >= FILL_THRESH_HIGH ||
            bins[i].wpi        >= WPI_THRESH_HIGH) {
            bins[i].priority = PRI_HIGH;
            critCount++;
        } else if (bins[i].fill_level >= FILL_THRESH_MED ||
                   bins[i].wpi        >= WPI_THRESH_MED) {
            bins[i].priority = PRI_MED;
        } else {
            bins[i].priority = PRI_LOW;
        }
    }

    // Sort bins HIGH → MED → LOW globally first
    sortBinsByPriority(totalBins);

    printf("Bins loaded: %d | Critical: %d | Vehicles: %d\n",
           totalBins, critCount, totalVehicles);
    printf("Hub location: (%d,%d) for all zones\n", HUB_X, HUB_Y);
    printf("========================================\n");

    int runID = generateRunID();

    // ── Step 3: Process each zone independently (parallel simulation) ──────────
    for (int zone = 1; zone <= MAX_ZONE; zone++) {

        // Collect bins in this zone that are not yet collected
        // Already sorted by priority from global sort above
        int zoneBinList[MAX_BINS];
        int zoneBinCount = 0;

        for (int i = 0; i < totalBins; i++) {
            if (bins[i].zone == zone && !bins[i].collected_today)
                zoneBinList[zoneBinCount++] = i;
        }

        if (zoneBinCount == 0) continue; // no work in this zone

        // Sort zone bins: HIGH first, within same priority sort by WPI desc
        // (Simple insertion sort for zone-local list)
        for (int a = 1; a < zoneBinCount; a++) {
            int key = zoneBinList[a];
            int b   = a - 1;
            while (b >= 0 &&
                   (bins[zoneBinList[b]].priority < bins[key].priority ||
                   (bins[zoneBinList[b]].priority == bins[key].priority &&
                    bins[zoneBinList[b]].wpi < bins[key].wpi))) {
                zoneBinList[b + 1] = zoneBinList[b];
                b--;
            }
            zoneBinList[b + 1] = key;
        }

        // ── Collect available vehicles for this zone ───────────────────────────
        int zoneVehicleList[MAX_VEHICLES];
        int zoneVehicleCount = 0;

        for (int v = 0; v < totalVehicles; v++) {
            if (vehicles[v].assigned_zone == zone &&
                vehicles[v].is_available  &&
                !vehicles[v].under_maintenance) {
                zoneVehicleList[zoneVehicleCount++] = v;
            }
        }

        // ── If no vehicle in zone → borrow from nearest zone ──────────────────
        bool needHazmat = zoneHasHazardousBins(zone, totalBins);

        if (zoneVehicleCount == 0) {
            printf("\n[ZONE %d] No available vehicle. Searching nearest zone...\n",
                   zone);

            int borrowed = findNearestAvailableVehicle(zone,
                                                       totalVehicles,
                                                       needHazmat);
            if (borrowed == -1) {
                printf("[ZONE %d] No backup vehicle found anywhere."
                       " Raising SOS.\n", zone);

                // Raise NO_BACKUP request using first bin as reference
                // Create a dummy vehicle index request
                // We log it with vehicle_id=0 to indicate no vehicle
                int reqID  = generateRequestID();
                bool isNew = isCsvEmpty(REQUESTS_CSV);
                FILE *rfp  = fopen(REQUESTS_CSV, "a");
                if (rfp) {
                    if (isNew)
                        fprintf(rfp,
                            "request_id,reason,vehicle_id,vehicle_reg,"
                            "vehicle_type,zone,last_bin_id,driver_id,"
                            "driver_name,hours_worked,status\n");
                    fprintf(rfp,
                            "%d,%s,%d,%s,%s,%d,%lld,%d,%s,%.2f,PENDING\n",
                            reqID, REQ_NO_BACKUP,
                            0, "NONE", "NONE",
                            zone,
                            bins[zoneBinList[0]].bin_id,
                            0, "NONE", 0.0f);
                    fclose(rfp);
                    printf("[SOS] Request #%d logged for Zone %d.\n",
                           reqID, zone);
                }
                continue; // skip this zone
            }

            // Use borrowed vehicle
            zoneVehicleList[0]  = borrowed;
            zoneVehicleCount    = 1;
            printf("[ZONE %d] Borrowed Vehicle %d (%s) from Zone %d.\n",
                   zone,
                   vehicles[borrowed].vehicle_id,
                   vehicles[borrowed].registration,
                   vehicles[borrowed].assigned_zone);
        }

        printf("\n========================================\n");
        printf("ZONE %d | %d bins | %d vehicle(s)\n",
               zone, zoneBinCount, zoneVehicleCount);
        printf("========================================\n");

        // ── Step 4: Split bins across zone vehicles round-robin ───────────────
        // Each vehicle gets a sub-list of bins
        // Round-robin respects priority order (bins already sorted HIGH first)
        int vehicleBinLists[MAX_VEHICLES][MAX_BINS];
        int vehicleBinCounts[MAX_VEHICLES];
        for (int v = 0; v < zoneVehicleCount; v++)
            vehicleBinCounts[v] = 0;

        for (int b = 0; b < zoneBinCount; b++) {
            int vSlot = b % zoneVehicleCount; // round-robin assignment
            int vIdx  = zoneVehicleList[vSlot];

            // Check hazmat: if bin is hazardous and vehicle driver has no license,
            // try another vehicle in zone that has hazmat
            if (bins[zoneBinList[b]].waste_type == HAZARDOUS) {
                bool assigned = false;
                for (int vv = 0; vv < zoneVehicleCount; vv++) {
                    int checkV = zoneVehicleList[vv];
                    int dID    = vehicles[checkV].assigned_driver_id;
                    for (int d = 0; d < totalDrivers; d++) {
                        if (drivers[d].driver_id == dID &&
                            drivers[d].has_hazmat_license) {
                            vehicleBinLists[vv]
                                [vehicleBinCounts[vv]++] = zoneBinList[b];
                            assigned = true;
                            break;
                        }
                    }
                    if (assigned) break;
                }
                if (!assigned) {
                    // No hazmat vehicle — assign to round-robin slot anyway
                    // and let SOS handle it
                    vehicleBinLists[vSlot]
                        [vehicleBinCounts[vSlot]++] = zoneBinList[b];
                }
            } else {
                // Non-hazardous: plain round-robin
                vehicleBinLists[vSlot]
                    [vehicleBinCounts[vSlot]++] = zoneBinList[b];
            }
        }

        // ── Step 5: Plan route for each vehicle in this zone ──────────────────
        // All vehicles in a zone run simultaneously (parallel per spec).
        // We simulate each one sequentially but they are conceptually parallel.
        for (int v = 0; v < zoneVehicleCount; v++) {
            int vIdx = zoneVehicleList[v];

            if (vehicleBinCounts[v] == 0) {
                printf("  [Vehicle %d] No bins assigned.\n",
                       vehicles[vIdx].vehicle_id);
                continue;
            }

            // Mark vehicle as in-use for this route
            vehicles[vIdx].is_available = false;

            planVehicleRoute(vIdx,
                             vehicleBinLists[v],
                             vehicleBinCounts[v],
                             zone,
                             runID);
        }

        runID++; // increment run ID per zone for routes.csv
    }

    // ── Step 6: Save all updated state back to CSVs ───────────────────────────
    saveBinsToCSV(totalBins);
    saveVehiclesToCSV(totalVehicles);
    saveDriversToCSV(totalDrivers);

    printf("\n========================================\n");
    printf("Route Optimization Complete.\n");
    printf("All routes saved to: %s\n", ROUTES_CSV);
    printf("Pending SOS requests saved to: %s\n", REQUESTS_CSV);
    printf("========================================\n");
}

// ── Display routes from routes.csv to admin ───────────────────────────────────
void viewRoutes() {
    printf("\n========================================");
    printf("\n        VIEW OPTIMIZED ROUTES           ");
    printf("\n========================================\n");

    FILE *fp = fopen(ROUTES_CSV, "r");
    if (!fp) {
        printf("No routes found. Run Route Optimization first.\n");
        return;
    }

    char line[2048];
    fgets(line, sizeof(line), fp); // skip header

    printf("%-6s %-10s %-12s %-5s %-10s %-20s %-8s %-8s %-8s %-5s\n",
           "RunID", "VehicleID", "Reg", "Zone",
           "DriverID", "DriverName", "Dist",
           "Fuel(L)", "Load(kg)", "Bins");
    printf("Route\n");
    printf("------------------------------------------------------------------------\n");

    int runID, vehicleID, zone, driverID, bins_collected;
    char vehicleReg[15], driverName[50], routeStr[2048];
    float totalDist, fuelUsed, loadKg;

    while (fscanf(fp,
                  "%d,%d,%[^,],%d,%d,%[^,],%f,%f,%f,%d,\"%[^\"]\"\n",
                  &runID, &vehicleID, vehicleReg,
                  &zone, &driverID, driverName,
                  &totalDist, &fuelUsed, &loadKg,
                  &bins_collected, routeStr) == 11) {

        printf("%-6d %-10d %-12s %-5d %-10d %-20s %-8.1f %-8.2f %-8.1f %-5d\n",
               runID, vehicleID, vehicleReg, zone, driverID,
               driverName, totalDist, fuelUsed, loadKg, bins_collected);
        printf("  Route: %s\n\n", routeStr);
    }

    fclose(fp);
    printf("========================================\n");
}
// ══════════════════════════════════════════════════════════════════════════════
//  USER MODULE
//  Handles: Netizen-facing features — view bins by zone, throw waste,
//           raise bin complaint, suggest best bin for waste disposal.
//  All changes to bins are saved back to bins.csv immediately.
//  Critical bin status is refreshed after every bin state change.
// ══════════════════════════════════════════════════════════════════════════════

// ── Validate zone number is within system range ───────────────────────────────
int isValidZone(int zone) {
    return (zone >= 1 && zone <= MAX_ZONE);
}

// ── Display all bins in a specific zone with fill and WPI info ────────────────
void viewBinsByZone() {
    printf("\n========================================");
    printf("\n        VIEW BINS BY ZONE MODULE        ");
    printf("\n========================================\n");

    int zone;
    printf("Enter Your Zone Number (1-%d): ", MAX_ZONE);
    scanf("%d", &zone);

    if (!isValidZone(zone)) {
        printf("[ERROR] Invalid zone number.\n");
        return;
    }

    int total = loadBinsFromCSV();
    if (total == 0) return;

    printf("\n--- Bins in Zone %d ---\n", zone);
    printf("%-14s %-10s %-9s %-7s %-10s\n",
           "Bin ID", "WasteType", "Fill(%)", "WPI", "Status");
    printf("------------------------------------------------------------\n");

    int found = 0;
    for (int i = 0; i < total; i++) {
        if (bins[i].zone == zone) {
            const char* status;
            if      (bins[i].collected_today)        status = "Collected";
            else if (bins[i].fill_level >= FILL_THRESH_HIGH) status = "CRITICAL";
            else if (bins[i].fill_level >= FILL_THRESH_MED)  status = "Monitor";
            else                                              status = "OK";

            printf("%-14lld %-10s %-9.1f %-7.2f %-10s\n",
                   bins[i].bin_id,
                   getWasteTypeString(bins[i].waste_type),
                   bins[i].fill_level,
                   bins[i].wpi,
                   status);
            found = 1;
        }
    }

    if (!found)
        printf("No bins found in Zone %d.\n", zone);

    printf("------------------------------------------------------------\n");
    printf("========================================\n");
}

// ── Netizen throws waste into a selected bin ──────────────────────────────────
// User selects zone, waste type, then picks from available bins.
// Fill level is updated and bins.csv is saved immediately.
// Critical bin check is triggered after update.
void throwWaste() {
    printf("\n========================================");
    printf("\n         THROW WASTE MODULE             ");
    printf("\n========================================\n");

    int zone, type;
    float waste;

    printf("Enter Your Zone (1-%d): ", MAX_ZONE);
    scanf("%d", &zone);

    if (!isValidZone(zone)) {
        printf("[ERROR] Invalid zone.\n");
        return;
    }

    printf("Select Waste Type:\n");
    printf("  1. DRY\n  2. WET\n  3. MIXED\n  4. HAZARDOUS\n");
    printf("Option: ");
    scanf("%d", &type);

    if (type < 1 || type > 4) {
        printf("[ERROR] Invalid waste type.\n");
        return;
    }

    int total = loadBinsFromCSV();
    if (total == 0) return;

    // Show available bins of matching zone and type
    printf("\nAvailable Bins in Zone %d for %s waste:\n",
           zone, getWasteTypeString(type));
    printf("%-14s %-9s\n", "Bin ID", "Fill(%)");
    printf("---------------------------\n");

    int found = 0;
    for (int i = 0; i < total; i++) {
        if (bins[i].zone == zone &&
            bins[i].waste_type == type &&
            !bins[i].collected_today) {
            printf("%-14lld %-9.1f\n",
                   bins[i].bin_id,
                   bins[i].fill_level);
            found = 1;
        }
    }

    if (!found) {
        printf("No available bins for this waste type in Zone %d.\n", zone);
        return;
    }

    // User selects a bin and enters waste amount
    long long int id;
    printf("\nEnter Bin ID: ");
    scanf("%lld", &id);

    printf("Enter Waste Amount to Add (as fill %%, e.g. 10 = 10%%): ");
    scanf("%f", &waste);

    if (waste <= 0) {
        printf("[ERROR] Waste amount must be greater than 0.\n");
        return;
    }

    for (int i = 0; i < total; i++) {
        if (bins[i].bin_id == id) {

            // Verify bin matches selected zone and type
            if (bins[i].zone != zone || bins[i].waste_type != type) {
                printf("[ERROR] Bin does not match selected zone/type.\n");
                return;
            }

            if (bins[i].fill_level + waste <= 100.0f) {
                bins[i].fill_level += waste;
                bins[i].wpi = computeWPI(bins[i].fill_level,
                                         bins[i].waste_type);

                printf("[OK] Waste added to Bin %lld."
                       " New Fill: %.1f%% | WPI: %.2f\n",
                       bins[i].bin_id,
                       bins[i].fill_level,
                       bins[i].wpi);

                // Save updated bin data immediately
                saveBinsToCSV(total);

                // Refresh critical bin status after change
                printf("\n[System Update — Bin Priority Refresh]\n");
                identifyCriticalBins();

            } else {
                printf("[WARN] Bin overflow! Only %.1f%% space left."
                       " Choose another bin.\n",
                       100.0f - bins[i].fill_level);
            }
            return;
        }
    }

    printf("[ERROR] Bin ID %lld not found.\n", id);
}

// ── Netizen raises a complaint about a critically full bin ────────────────────
// Bin must be >= 85% full to qualify for a complaint.
// Complaint boosts WPI by 25 (capped at 100) to escalate priority.
void raiseComplaint() {
    printf("\n========================================");
    printf("\n       RAISE BIN COMPLAINT MODULE       ");
    printf("\n========================================\n");

    int total = loadBinsFromCSV();
    if (total == 0) return;

    long long int id;
    printf("Enter Bin ID to Raise Complaint: ");
    scanf("%lld", &id);

    for (int i = 0; i < total; i++) {
        if (bins[i].bin_id == id) {

            if (bins[i].fill_level >= 85.0f) {
                bins[i].wpi += 25.0f;
                if (bins[i].wpi > 100.0f) bins[i].wpi = 100.0f;

                printf("[OK] Complaint registered for Bin %lld."
                       " WPI boosted to %.2f.\n",
                       bins[i].bin_id, bins[i].wpi);

                // Save and refresh priorities
                saveBinsToCSV(total);

                printf("\n[System Alert — Priority Escalation]\n");
                identifyCriticalBins();

            } else {
                printf("[INFO] Bin %lld is only %.1f%% full."
                       " Complaints require >= 85%% fill.\n",
                       bins[i].bin_id, bins[i].fill_level);
            }
            return;
        }
    }

    printf("[ERROR] Bin ID %lld not found.\n", id);
}

// ── Suggest the best (least full) bin for a user's waste type and zone ─────────
// Helps users avoid overfilling bins by directing to emptiest available bin.
void suggestBestBin() {
    printf("\n========================================");
    printf("\n       SUGGEST BEST BIN MODULE          ");
    printf("\n========================================\n");

    int zone, type;

    printf("Enter Your Zone (1-%d): ", MAX_ZONE);
    scanf("%d", &zone);

    if (!isValidZone(zone)) {
        printf("[ERROR] Invalid zone.\n");
        return;
    }

    printf("Select Waste Type (1-DRY, 2-WET, 3-MIXED, 4-HAZARDOUS): ");
    scanf("%d", &type);

    if (type < 1 || type > 4) {
        printf("[ERROR] Invalid waste type.\n");
        return;
    }

    int total = loadBinsFromCSV();
    if (total == 0) return;

    // Find bin with lowest fill level in this zone and type
    int   best    = -1;
    float minFill = 101.0f;

    for (int i = 0; i < total; i++) {
        if (bins[i].zone      == zone &&
            bins[i].waste_type == type &&
            !bins[i].collected_today   &&
            bins[i].fill_level < minFill) {
            minFill = bins[i].fill_level;
            best    = i;
        }
    }

    if (best == -1) {
        printf("No suitable bin found in Zone %d for %s waste.\n",
               zone, getWasteTypeString(type));
        return;
    }

    printf("\nSuggested Bin:\n");
    printf("  Bin ID   : %lld\n",  bins[best].bin_id);
    printf("  Location : (%d, %d)\n", bins[best].x, bins[best].y);
    printf("  Fill     : %.1f%%\n",   bins[best].fill_level);
    printf("  WPI      : %.2f\n",     bins[best].wpi);
    printf("  Priority : %s\n",
           getPriorityString(bins[best].priority));
    printf("========================================\n");
}

// ══════════════════════════════════════════════════════════════════════════════
//  REQUEST MANAGEMENT MODULE — ADMIN
//  Handles: Viewing all pending requests, admin writing a solution,
//           moving addressed requests from requests.csv to addressed.csv,
//           approving/rejecting driver sick leave,
//           updating driver suspension status after admin decision.
// ══════════════════════════════════════════════════════════════════════════════

// ── Load all requests from requests.csv into requests[] array ─────────────────
// Returns count of requests loaded.
int loadRequestsFromCSV() {
    FILE *fp = fopen(REQUESTS_CSV, "r");
    if (!fp) return 0;

    char line[512];
    if (fgets(line, sizeof(line), fp) == NULL) {
        fclose(fp);
        return 0;
    }

    int count = 0;
    while (count < MAX_REQUESTS &&
           fscanf(fp, "%d,%[^,],%d,%[^,],%[^,],%d,%lld,%d,%[^,],%f,%s\n",
                  &requests[count].request_id,
                  requests[count].reason,
                  &requests[count].vehicle_id,
                  requests[count].vehicle_reg,
                  requests[count].vehicle_type,
                  &requests[count].zone,
                  &requests[count].last_bin_id,
                  &requests[count].driver_id,
                  requests[count].driver_name,
                  &requests[count].hours_worked,
                  requests[count].status) == 11) {
        count++;
    }

    fclose(fp);
    return count;
}

// ── Save only PENDING requests back to requests.csv ───────────────────────────
// Called after admin addresses a request — removes it from pending list.
void savePendingRequestsToCSV(int total) {
    FILE *fp = fopen(REQUESTS_CSV, "w");
    if (!fp) {
        printf("[ERROR] Cannot rewrite requests.csv.\n");
        return;
    }

    fprintf(fp,
        "request_id,reason,vehicle_id,vehicle_reg,vehicle_type,"
        "zone,last_bin_id,driver_id,driver_name,hours_worked,status\n");

    for (int i = 0; i < total; i++) {
        if (strcmp(requests[i].status, "PENDING") == 0) {
            fprintf(fp, "%d,%s,%d,%s,%s,%d,%lld,%d,%s,%.2f,%s\n",
                    requests[i].request_id,
                    requests[i].reason,
                    requests[i].vehicle_id,
                    requests[i].vehicle_reg,
                    requests[i].vehicle_type,
                    requests[i].zone,
                    requests[i].last_bin_id,
                    requests[i].driver_id,
                    requests[i].driver_name,
                    requests[i].hours_worked,
                    requests[i].status);
        }
    }

    fclose(fp);
}

// ── Append one addressed request to addressed.csv ─────────────────────────────
// Includes all original request fields plus admin's solution text.
void appendToAddressedCSV(Request *req, const char* solution) {
    bool isNew = isCsvEmpty(ADDRESSED_CSV);
    FILE *fp   = fopen(ADDRESSED_CSV, "a");
    if (!fp) {
        printf("[ERROR] Cannot open addressed.csv.\n");
        return;
    }

    if (isNew) {
        fprintf(fp,
            "request_id,reason,vehicle_id,vehicle_reg,vehicle_type,"
            "zone,last_bin_id,driver_id,driver_name,hours_worked,"
            "status,admin_solution\n");
    }

    fprintf(fp, "%d,%s,%d,%s,%s,%d,%lld,%d,%s,%.2f,ADDRESSED,\"%s\"\n",
            req->request_id,
            req->reason,
            req->vehicle_id,
            req->vehicle_reg,
            req->vehicle_type,
            req->zone,
            req->last_bin_id,
            req->driver_id,
            req->driver_name,
            req->hours_worked,
            solution);

    fclose(fp);
}

// ── Admin views all pending requests and writes solution for each ─────────────
// For DRIVER_SICK requests: admin approves or rejects leave.
//   Approved → driver marked suspended (on leave) in drivers.csv
//   Rejected → driver stays active, note saved
// For all other requests: admin types a free-text solution, saved to addressed.csv
// Addressed requests are moved out of requests.csv.
void adminViewAndAddressRequests() {
    printf("\n========================================");
    printf("\n      REQUEST MANAGEMENT MODULE         ");
    printf("\n========================================\n");

    int total = loadRequestsFromCSV();
    if (total == 0) {
        printf("No pending requests found.\n");
        return;
    }

    // Count only PENDING
    int pendingCount = 0;
    for (int i = 0; i < total; i++) {
        if (strcmp(requests[i].status, "PENDING") == 0)
            pendingCount++;
    }

    if (pendingCount == 0) {
        printf("All requests have been addressed.\n");
        return;
    }

    printf("Total Pending Requests: %d\n\n", pendingCount);

    int totalDrivers  = loadDriversFromCSV();
    int totalVehicles = loadVehiclesFromCSV();

    for (int i = 0; i < total; i++) {
        if (strcmp(requests[i].status, "PENDING") != 0) continue;

        // ── Display request details ────────────────────────────────────────────
        printf("========================================\n");
        printf("Request ID   : %d\n",   requests[i].request_id);
        printf("Reason       : %s\n",   requests[i].reason);
        printf("Vehicle ID   : %d\n",   requests[i].vehicle_id);
        printf("Vehicle Reg  : %s\n",   requests[i].vehicle_reg);
        printf("Vehicle Type : %s\n",   requests[i].vehicle_type);
        printf("Zone         : %d\n",   requests[i].zone);
        printf("Last Bin ID  : %lld\n", requests[i].last_bin_id);
        printf("Driver ID    : %d\n",   requests[i].driver_id);
        printf("Driver Name  : %s\n",   requests[i].driver_name);
        printf("Hours Worked : %.2f\n", requests[i].hours_worked);
        printf("----------------------------------------\n");

        char solution[256] = "";

        // ── Special handling for DRIVER_SICK leave requests ───────────────────
        if (strcmp(requests[i].reason, REQ_SICK) == 0) {
            printf("Driver %s has requested sick leave.\n",
                   requests[i].driver_name);
            printf("Approve leave? (1=Yes, 0=No): ");
            int approve;
            scanf("%d", &approve);
            getchar(); // clear newline

            if (approve == 1) {
                // Find driver and mark as suspended (on approved leave)
                for (int d = 0; d < totalDrivers; d++) {
                    if (drivers[d].driver_id == requests[i].driver_id) {
                        drivers[d].is_suspended  = true;
                        drivers[d].is_available  = false;
                        break;
                    }
                }
                strcpy(solution, "Sick leave APPROVED. Driver marked on leave.");
                printf("[OK] Leave approved. Driver suspended from duty.\n");

                // Check if zone now has no driver — raise alert
                int dZone = requests[i].zone;
                bool zoneHasDriver = false;
                for (int d = 0; d < totalDrivers; d++) {
                    if (drivers[d].assigned_vehicle_id != 0 &&
                        !drivers[d].is_suspended &&
                        drivers[d].is_available) {
                        // Check if this driver's vehicle is in the affected zone
                        for (int v = 0; v < totalVehicles; v++) {
                            if (vehicles[v].vehicle_id ==
                                drivers[d].assigned_vehicle_id &&
                                vehicles[v].assigned_zone == dZone) {
                                zoneHasDriver = true;
                                break;
                            }
                        }
                    }
                    if (zoneHasDriver) break;
                }

                if (!zoneHasDriver) {
                    printf("[ALERT] Zone %d now has no available driver!"
                           " Run Driver Assignment to reassign.\n", dZone);
                }

            } else {
                // Rejected — driver stays active
                strcpy(solution,
                       "Sick leave REJECTED. Driver must report for duty.");
                printf("[OK] Leave rejected. Driver remains on duty.\n");
            }

        } else if (strcmp(requests[i].reason, REQ_SUSPENDED) == 0) {
            // ── Admin addresses a suspension alert ────────────────────────────
            printf("Driver %s is suspended causing zone %d gap.\n",
                   requests[i].driver_name, requests[i].zone);
            printf("Enter your action/solution: ");
            getchar();
            fgets(solution, sizeof(solution), stdin);
            solution[strcspn(solution, "\n")] = '\0'; // trim newline

        } else if (strcmp(requests[i].reason, REQ_HOURS_OVER) == 0) {
            // ── Admin acknowledges hours-exceeded report ───────────────────────
            printf("Driver %s worked %.2f hours (over limit).\n",
                   requests[i].driver_name, requests[i].hours_worked);
            printf("Enter compensation note or action: ");
            getchar();
            fgets(solution, sizeof(solution), stdin);
            solution[strcspn(solution, "\n")] = '\0';

        } else {
            // ── General SOS request (FUEL_OUT, CAPACITY_FULL, NO_BACKUP) ──────
            printf("Enter your solution/action for this request: ");
            getchar();
            fgets(solution, sizeof(solution), stdin);
            solution[strcspn(solution, "\n")] = '\0';
        }

        // ── Move request: mark ADDRESSED, append to addressed.csv ─────────────
        strcpy(requests[i].status, "ADDRESSED");
        appendToAddressedCSV(&requests[i], solution);
        printf("[OK] Request #%d addressed and moved to addressed.csv.\n",
               requests[i].request_id);
    }

    // ── Save only remaining PENDING requests back to requests.csv ─────────────
    savePendingRequestsToCSV(total);

    // ── Save any driver status changes (leave approvals) ──────────────────────
    saveDriversToCSV(totalDrivers);

    printf("\n========================================\n");
    printf("All requests processed.\n");
    printf("Solutions saved to: %s\n", ADDRESSED_CSV);
    printf("========================================\n");
}

// ── View already-addressed requests from addressed.csv ────────────────────────
void viewAddressedRequests() {
    printf("\n========================================");
    printf("\n      ADDRESSED REQUESTS LOG            ");
    printf("\n========================================\n");

    FILE *fp = fopen(ADDRESSED_CSV, "r");
    if (!fp) {
        printf("No addressed requests found yet.\n");
        return;
    }

    char line[512];
    fgets(line, sizeof(line), fp); // skip header

    int    reqID, vehicleID, zone, driverID;
    char   reason[30], vehicleReg[15], vehicleType[30];
    char   driverName[50], status[15], solution[256];
    long long int lastBinID;
    float  hoursWorked;

    printf("%-6s %-16s %-6s %-12s %-20s  Solution\n",
           "ReqID", "Reason", "Zone", "DriverName",
           "VehicleReg");
    printf("------------------------------------------------------------------------\n");

    while (fscanf(fp,
                  "%d,%[^,],%d,%[^,],%[^,],%d,%lld,%d,%[^,],%f,%[^,],\"%[^\"]\"\n",
                  &reqID, reason,
                  &vehicleID, vehicleReg, vehicleType,
                  &zone, &lastBinID,
                  &driverID, driverName,
                  &hoursWorked, status, solution) == 12) {

        printf("%-6d %-16s %-6d %-12s %-20s  %s\n",
               reqID, reason, zone, driverName, vehicleReg, solution);
    }

    fclose(fp);
    printf("========================================\n");
}
// ══════════════════════════════════════════════════════════════════════════════
//  AUTHENTICATION MODULE
//  Handles: Username + password login, role detection (admin / netizen),
//           returns role string used by main() to route to correct menu.
//  Passwords are defined as constants (ADMIN_PASS, NETIZEN_PASS).
// ══════════════════════════════════════════════════════════════════════════════

// ── Authenticate user and return role string ──────────────────────────────────
// outUsername buffer must be at least 40 chars (filled by this function).
// Returns: "admin" | "netizen" | "unauthorized"
const char* userAUTH(char* outUsername) {
    char pass[32];

    printf("\n==========================================\n");
    printf("      SENTRABIN OS - SECURE LOGIN         \n");
    printf("==========================================\n");
    printf("  Centralized Bin Operations System\n");
    printf("------------------------------------------\n");

    printf("\n  ENTER USERNAME : ");
    scanf("%39s", outUsername);
    printf("  ENTER PASSCODE : ");
    scanf("%31s", pass);

    if (strcmp(pass, ADMIN_PASS) == 0) {
        printf("\n[AUTH] Admin privileges granted to: %s\n", outUsername);
        return "admin";
    } else if (strcmp(pass, NETIZEN_PASS) == 0) {
        printf("\n[AUTH] Netizen access granted to: %s\n", outUsername);
        return "netizen";
    }

    printf("\n[AUTH] Invalid credentials.\n");
    return "unauthorized";
}

// ══════════════════════════════════════════════════════════════════════════════
//  ADMIN MENU MODULE
//  Handles: All admin-facing options organized into submenus.
//  Menu Structure:
//    1. Bin Management
//       1. Create Bin Records
//       2. Identify Critical Bins
//    2. Vehicle Management
//       1. Create Vehicle Records
//       2. View Vehicles
//    3. Driver Management
//       1. Create Driver Records
//       2. View Driver Records
//       3. Driver Sick Leave Request (raised by driver via admin terminal)
//    4. Assignment Management
//       1. Assign Drivers to Vehicles
//       2. View Current Assignments
//    5. Route Management
//       1. Run Route Optimization (auto — all zones parallel)
//       2. View Optimized Routes
//    6. Request Management
//       1. View & Address Pending Requests
//       2. View Addressed Requests Log
//    7. Exit
// ══════════════════════════════════════════════════════════════════════════════

void adminMenu(const char* adminName) {
    int select, choice;

    while (1) {
        // Load bin count fresh each loop for display
        int totalBins = loadBinsFromCSV();

        printf("\n===========================================================");
        printf("\n        SENTRABIN OS - Centralized Bin Operations          ");
        printf("\n===========================================================");
        printf("\n  OPERATOR : %-20s | BINS LOADED: %d", adminName, totalBins);
        printf("\n-----------------------------------------------------------");
        printf("\n  1. BIN MANAGEMENT");
        printf("\n  2. VEHICLE MANAGEMENT");
        printf("\n  3. DRIVER MANAGEMENT");
        printf("\n  4. ASSIGNMENT MANAGEMENT");
        printf("\n  5. ROUTE MANAGEMENT");
        printf("\n  6. REQUEST MANAGEMENT");
        printf("\n  7. EXIT");
        printf("\n===========================================================");
        printf("\nEnter Choice: ");
        scanf("%d", &select);

        // ── 1. BIN MANAGEMENT ─────────────────────────────────────────────────
        if (select == 1) {
            printf("\n--- BIN MANAGEMENT ---\n");
            printf("  1. Create Bin Records\n");
            printf("  2. Identify Critical Bins\n");
            printf("  0. Back\n");
            printf("Choice: ");
            scanf("%d", &choice);

            if      (choice == 1) createBin();
            else if (choice == 2) identifyCriticalBins();
            else if (choice == 0) continue;
            else printf("[ERROR] Invalid choice.\n");
        }

        // ── 2. VEHICLE MANAGEMENT ─────────────────────────────────────────────
        else if (select == 2) {
            printf("\n--- VEHICLE MANAGEMENT ---\n");
            printf("  1. Create Vehicle Records\n");
            printf("  2. View All Vehicles\n");
            printf("  0. Back\n");
            printf("Choice: ");
            scanf("%d", &choice);

            if      (choice == 1) createVehicle();
            else if (choice == 2) viewVehicles();
            else if (choice == 0) continue;
            else printf("[ERROR] Invalid choice.\n");
        }

        // ── 3. DRIVER MANAGEMENT ──────────────────────────────────────────────
        else if (select == 3) {
            printf("\n--- DRIVER MANAGEMENT ---\n");
            printf("  1. Create Driver Records\n");
            printf("  2. View All Driver Records\n");
            printf("  3. Submit Driver Sick Leave Request\n");
            printf("  0. Back\n");
            printf("Choice: ");
            scanf("%d", &choice);

            if      (choice == 1) createDriver();
            else if (choice == 2) viewDrivers();
            else if (choice == 3) driverRaiseSickLeave();
            else if (choice == 0) continue;
            else printf("[ERROR] Invalid choice.\n");
        }

        // ── 4. ASSIGNMENT MANAGEMENT ──────────────────────────────────────────
        else if (select == 4) {
            printf("\n--- ASSIGNMENT MANAGEMENT ---\n");
            printf("  1. Assign Drivers to Vehicles\n");
            printf("  2. View Current Assignments\n");
            printf("  0. Back\n");
            printf("Choice: ");
            scanf("%d", &choice);

            if      (choice == 1) assignDriversToVehicles();
            else if (choice == 2) viewAssignments();
            else if (choice == 0) continue;
            else printf("[ERROR] Invalid choice.\n");
        }

        // ── 5. ROUTE MANAGEMENT ───────────────────────────────────────────────
        else if (select == 5) {
            printf("\n--- ROUTE MANAGEMENT ---\n");
            printf("  1. Run Route Optimization (All Zones)\n");
            printf("  2. View Optimized Routes\n");
            printf("  0. Back\n");
            printf("Choice: ");
            scanf("%d", &choice);

            if      (choice == 1) runRouteOptimization();
            else if (choice == 2) viewRoutes();
            else if (choice == 0) continue;
            else printf("[ERROR] Invalid choice.\n");
        }

        // ── 6. REQUEST MANAGEMENT ─────────────────────────────────────────────
        else if (select == 6) {
            printf("\n--- REQUEST MANAGEMENT ---\n");
            printf("  1. View & Address Pending Requests\n");
            printf("  2. View Addressed Requests Log\n");
            printf("  0. Back\n");
            printf("Choice: ");
            scanf("%d", &choice);

            if      (choice == 1) adminViewAndAddressRequests();
            else if (choice == 2) viewAddressedRequests();
            else if (choice == 0) continue;
            else printf("[ERROR] Invalid choice.\n");
        }

        // ── 7. EXIT ───────────────────────────────────────────────────────────
        else if (select == 7) {
            printf("\n[SENTRABIN OS] Session closed by %s. Goodbye!\n",
                   adminName);
            return;
        }

        else {
            printf("[ERROR] Invalid menu option. Try again.\n");
        }
    }
}

// ══════════════════════════════════════════════════════════════════════════════
//  NETIZEN MENU MODULE
//  Handles: Public-facing options for residents/citizens.
//  Menu Structure:
//    1. View Bins in My Zone
//    2. Throw Waste
//    3. Raise Bin Complaint
//    4. Suggest Best Bin
//    0. Logout
// ══════════════════════════════════════════════════════════════════════════════

void netizenMenu(char* username) {
    int choice;

    while (1) {
        printf("\n========================================");
        printf("\n     SENTRABIN OS - USER PORTAL         ");
        printf("\n========================================");
        printf("\n  Welcome, %s", username);
        printf("\n----------------------------------------");
        printf("\n  1. View Bins in My Zone");
        printf("\n  2. Throw Waste");
        printf("\n  3. Raise Bin Complaint");
        printf("\n  4. Suggest Best Bin for My Waste");
        printf("\n  0. Logout");
        printf("\n========================================");
        printf("\nEnter Choice: ");

        if (scanf("%d", &choice) != 1) {
            // Clear invalid input from buffer
            while (getchar() != '\n');
            continue;
        }

        switch (choice) {
            case 1: viewBinsByZone();  break;
            case 2: throwWaste();      break;
            case 3: raiseComplaint();  break;
            case 4: suggestBestBin();  break;
            case 0:
                printf("\n[SENTRABIN OS] Logged out. Goodbye, %s!\n",
                       username);
                return;
            default:
                printf("[ERROR] Invalid choice. Try again.\n");
        }
    }
}

// ══════════════════════════════════════════════════════════════════════════════
//  MAIN FUNCTION
//  Entry point. Runs authentication then routes to correct menu.
//  Returns 0 on clean exit, 1 on auth failure.
// ══════════════════════════════════════════════════════════════════════════════

int main() {
    char username[40];

    // ── Authenticate user ──────────────────────────────────────────────────────
    const char* role = userAUTH(username);

    // ── Route to correct menu based on role ───────────────────────────────────
    if (strcmp(role, "admin") == 0) {
        adminMenu(username);
    } else if (strcmp(role, "netizen") == 0) {
        netizenMenu(username);
    } else {
        printf("\n[SENTRABIN OS] Access Denied. Invalid credentials.\n");
        printf("              Closing system...\n");
        return 1;
    }

    printf("\n[SENTRABIN OS] System session ended. Goodbye!\n");
    return 0;
}