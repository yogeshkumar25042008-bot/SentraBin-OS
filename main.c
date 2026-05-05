#include <stdio.h>
#include <stdbool.h>
#include <string.h>

// ── Constants ──────────────────────────────────────────────────────────────────
#define MAX_BINS 100
#define MAX_ZONE 20
#define MAX_VEHICLES 20
#define MAX_DRIVERS 20
#define ADMIN_PASS "admin@123"
#define BINS_CSV "bins.csv"
#define VEHICLES_CSV "vehicle.csv"
#define DRIVERS_CSV "drivers.csv"
#define NETIZEN_PASS "guest@123"

// ── Waste Types ────────────────────────────────────────────────────────────────
#define DRY       1
#define WET       2
#define MIXED     3
#define HAZARDOUS 4

// ── Priority Levels ────────────────────────────────────────────────────────────
#define PRI_HIGH 3
#define PRI_MED  2
#define PRI_LOW  1

// ── WPI / Fill Thresholds ──────────────────────────────────────────────────────
#define FILL_THRESH_HIGH 70.0f   // fill% → HIGH priority
#define FILL_THRESH_MED  40.0f   // fill% → MED  priority
#define WPI_THRESH_HIGH  60.0f   // WPI   → HIGH priority
#define WPI_THRESH_MED   35.0f   // WPI   → MED  priority

// ── Data Structure ──────────────────────────────────────────────────────────────
typedef struct {
    long long int bin_id;
    int   zone;
    int   waste_type;
    float fill_level;
    float capacity;
    float wpi;
    int   priority;
    int   x, y;
    char  last_collection[11]; // DD-MM-YYYY + '\0'
    bool  collected_today;
} Bin;

typedef struct {
    int   vehicle_id;
    char  type[30];                  // "Compactor", "Tipper", "Small"
    float max_capacity;              // kg
    float fuel_tank_capacity;        // liters
    float current_fuel;              // liters
    float fuel_consumption_rate;     // liters per 10km
    int   assigned_zone;             // which zone does it service
    bool  is_available;              // not currently on a route
    bool  under_maintenance;         // blocked from assignment
    int   assigned_driver_id;        // current driver (0 = none)
    float current_load;              // kg (during active collection)
    char  registration[15];          // vehicle plate number
} Vehicle;

typedef struct{
    int   driver_id; // [Hazard Flag][Serial] 1___ - normal driver, 8___ - hazardous Certified Driver
    char  name[50];
    char  phone[10];
    int   assigned_vehicle_id; // 0 if none
    bool  is_available; // 1 if available 0 if unavailable
    bool  is_suspended; // 0 if false 1 if suspended
    bool  has_hazmat_license; // 1 if true 0 if none
    float hours_worked_today;
    float max_daily_hours;
} Driver;


// ── Global Objects ──────────────────────────────────────────────────────────────
Bin bins[MAX_BINS];
Vehicle vehicles[MAX_VEHICLES];
Driver drivers[MAX_DRIVERS];

// ══════════════════════════════════════════════════════════════════════════════
//  HELPERS
// ══════════════════════════════════════════════════════════════════════════════

const char* getWasteTypeString(int type) {
    switch (type) {
        case DRY:       return "DRY";
        case WET:       return "WET";
        case MIXED:     return "MIXED";
        case HAZARDOUS: return "HAZARDOUS";
        default:        return "UNKNOWN";
    }
}

const char* getPriorityString(int priority) {
    switch (priority) {
        case PRI_HIGH: return "HIGH";
        case PRI_MED:  return "MEDIUM";
        case PRI_LOW:  return "LOW";
        default:       return "UNKNOWN";
    }
}

int parseWasteType(const char* str) {
    if (strcmp(str, "DRY")       == 0) return DRY;
    if (strcmp(str, "WET")       == 0) return WET;
    if (strcmp(str, "MIXED")     == 0) return MIXED;
    if (strcmp(str, "HAZARDOUS") == 0) return HAZARDOUS;
    return DRY; // safe default
}

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

bool isCsvEmpty(const char* filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) return true; // File doesn't exist, so it's "empty"

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fclose(fp);

    return (size == 0); // Returns true if file size is 0 bytes
}
// ══════════════════════════════════════════════════════════════════════════════
//  VEHICLE MANAGEMENT MODULE
// ══════════════════════════════════════════════════════════════════════════════
// ── Generate unique Vehicle ID: serial ─────────────────
int generateVehicleID() {
    FILE *fp = fopen(VEHICLES_CSV, "r");
    int maxID = 0;

    if (fp == NULL) {
        return 1001; // Start from 1001
    }

    int vehicle_id;
    char line[512];
    fgets(line, sizeof(line), fp); // skip header

    while (fgets(line, sizeof(line), fp) != NULL) {
        if (sscanf(line, "%d,", &vehicle_id) == 1) {
            if (vehicle_id > maxID) maxID = vehicle_id;
        }
    }
    fclose(fp);

    return maxID + 1;
}

void createVehicle() {
    int n;
    printf("\n========================================");
    printf("\n     CREATE VEHICLES MODULE            ");
    printf("\n========================================\n");
    printf("Enter the Number of Vehicles to Register: ");
    scanf("%d", &n);

    FILE *fp = fopen(VEHICLES_CSV, "a");
    if (!fp) {
        printf("Error Opening Vehicle Database!\n");
        return;
    }

    // Write header if file is new
    if (isCsvEmpty(VEHICLES_CSV)) {
        fprintf(fp, "vehicle_id,type,max_capacity,fuel_tank_capacity,fuel_consumption_rate,");
        fprintf(fp, "assigned_zone,is_available,under_maintenance,assigned_driver_id,");
        fprintf(fp, "current_load,registration\n");
    }
    int vehicle_id = generateVehicleID();
    for (int i = 0; i < n; i++) {
        printf("\n--- Enter Details for Vehicle %d ---", i + 1);

        // Generate unique vehicle ID


        // Vehicle Type
        printf("\nSelect Vehicle Type:\n");
        printf("  1. Compactor   (Large capacity, high fuel consumption)\n");
        printf("  2. Tipper      (Medium capacity, moderate consumption)\n");
        printf("  3. Small       (Small capacity, low consumption)\n");
        printf("Option: ");
        int type_choice;
        scanf("%d", &type_choice);

        char type[30];
        float max_cap, fuel_tank, fuel_consumption;

        switch (type_choice) {
            case 1:
                strcpy(type, "Compactor");
                max_cap           = 5000.0f;   // kg
                fuel_tank         = 120.0f;    // liters
                fuel_consumption  = 8.5f;      // liters per 10km
                break;
            case 2:
                strcpy(type, "Tipper");
                max_cap           = 3500.0f;   // kg
                fuel_tank         = 100.0f;    // liters
                fuel_consumption  = 6.5f;      // liters per 10km
                break;
            case 3:
                strcpy(type, "Small");
                max_cap           = 1500.0f;   // kg
                fuel_tank         = 60.0f;     // liters
                fuel_consumption  = 4.0f;      // liters per 10km
                break;
            default:
                printf("Invalid vehicle type!\n");
                i--;
                continue;
        }

        // Assigned Zone
        printf("\nEnter Assigned Zone (1-%d): ", MAX_ZONE);
        int assigned_zone;
        scanf("%d", &assigned_zone);
        if (assigned_zone < 1 || assigned_zone > MAX_ZONE) {
            printf("Invalid Zone!\n");
            i--;
            continue;
        }

        // Registration plate
        printf("Enter Vehicle Registration/Plate (e.g., TN-01-AB-1234): ");
        char registration[15];
        scanf("%s", registration);

        // All new vehicles start as available with full fuel tank
        bool is_available = true;
        bool under_maintenance = false;
        int assigned_driver_id = 0;
        float current_load = 0.0f;
        float current_fuel = fuel_tank; // Full tank initially

        // ── Write to CSV ────────────────────────────────────────────────────────
        fprintf(fp, "%d,%s,%.2f,%.2f,%.2f,%d,%d,%d,%d,%.2f,%s\n",
                vehicle_id,
                type,
                max_cap,
                fuel_tank,
                fuel_consumption,
                assigned_zone,
                (int)is_available,
                (int)under_maintenance,
                assigned_driver_id,
                current_load,
                registration);

        printf("\n Vehicle %d (%s) registered successfully!\n", vehicle_id, type);
        printf("  Registration: %s\n", registration);
        printf("  Zone: %d\n", assigned_zone);
        printf("  Capacity: %.0f kg\n", max_cap);
        printf("  Fuel Tank: %.0f liters\n", fuel_tank);
        vehicle_id++;
    }

    fclose(fp);
    printf("\n========================================\n");
}
int loadVehiclesFromCSV() {
    FILE *fp = fopen(VEHICLES_CSV, "r");
    if (!fp) {
        return 0;
    }

    char line[512];
    fgets(line, sizeof(line), fp); // skip header

    int count = 0;

    while (count < MAX_VEHICLES &&
           fscanf(fp, "%d,%[^,],%f,%f,%f,%d,%d,%d,%d,%f,%s\n",
                  &vehicles[count].vehicle_id,
                  vehicles[count].type,
                  &vehicles[count].max_capacity,
                  &vehicles[count].fuel_tank_capacity,
                  &vehicles[count].fuel_consumption_rate,
                  &vehicles[count].assigned_zone,
                  (int *)&vehicles[count].is_available,
                  (int *)&vehicles[count].under_maintenance,
                  &vehicles[count].assigned_driver_id,
                  &vehicles[count].current_load,
                  vehicles[count].registration) == 11) {
        count++;
                  }

    fclose(fp);
    return count;
}
void viewVehicles() {
    printf("\n========================================");
    printf("\n      VIEW ALL VEHICLES MODULE         ");
    printf("\n========================================\n");

    int totalVehicles = loadVehiclesFromCSV();
    if (totalVehicles == 0) {
        printf("No vehicles registered yet.\n");
        return;
    }

    printf("\n%-12s %-12s %-10s %-10s %-8s %-9s  %s\n",
           "Vehicle ID", "Type", "Capacity", "Fuel Tank", "Zone", "Status", "Registration");
    printf("--------------------------------------------------------------------------------------\n");

    for (int i = 0; i < totalVehicles; i++) {
        printf("%-12d %-12s %-10.0f %-10.0f %-8d %-9s  %s\n",
               vehicles[i].vehicle_id,
               vehicles[i].type,
               vehicles[i].max_capacity,
               vehicles[i].fuel_tank_capacity,
               vehicles[i].assigned_zone,
               vehicles[i].under_maintenance ? "MAINTENANCE" :
               (vehicles[i].is_available ? "AVAILABLE" : "IN USE"),
               vehicles[i].registration);
    }

    printf("--------------------------------------------------------------------------------------\n");
    printf("Total Vehicles: %d\n", totalVehicles);

    // Count status summary
    int available_count = 0, in_use_count = 0, maintenance_count = 0;
    for (int i = 0; i < totalVehicles; i++) {
        if (vehicles[i].under_maintenance)
            maintenance_count++;
        else if (vehicles[i].is_available)
            available_count++;
        else
            in_use_count++;
    }

    printf("\nStatus Summary:\n");
    printf("  [*] Available  : %d\n", available_count);
    printf("  [=] In Use     : %d\n", in_use_count);
    printf("  [!] Maintenance: %d\n", maintenance_count);
    printf("========================================\n");
}
// ══════════════════════════════════════════════════════════════════════════════
//  BIN MANAGEMENT MODULE
// ══════════════════════════════════════════════════════════════════════════════
// ── Generate unique Bin ID: [waste_type][zone_padded][serial] ─────────────────
long long int generateBinID(int waste_type, int zone) {
    FILE *fp = fopen(BINS_CSV, "r");

    long long int id;
    int z, collected, x, y;
    char wasteTypeStr[20];
    float capacity, fill, wpi;

    int maxSerial = 0;

    if (fp == NULL) {
        return (long long)waste_type * 100000 + zone * 1000 + 1;
    }

    char line[256];
    fgets(line, sizeof(line), fp); // skip header

    while (fscanf(fp, "%lld,%d,%[^,],%f,%f,%d,%d,%f,%d\n",
                  &id, &z, wasteTypeStr,
                  &capacity, &fill,
                  &x, &y,
                  &wpi, &collected) == 9) {

        int currentWasteType = id / 100000;
        int currentZone      = (id / 1000) % 100;

        if (currentZone == zone && currentWasteType == waste_type) {
            int serial = id % 1000;
            if (serial > maxSerial) {
                maxSerial = serial;
            }
        }
    }

    fclose(fp);

    int newSerial = maxSerial + 1;

    if (newSerial > 999) {
        printf("Max bins reached for this zone!\n");
        return -1;
    }

    return (long long)waste_type * 100000 + (long long)zone * 1000 + newSerial;
}
void createBin() {
    int n;
    printf("\nEnter the Number of Bins to be Created: ");
    scanf("%d", &n);

    // ── Open file; write header only if file is new/empty ─────────────────────
    FILE *fp = fopen(BINS_CSV, "a");
    if (!fp) {
        printf("Error Loading Database!\n");
        return;
    }
    if (isCsvEmpty(BINS_CSV)) {
        fprintf(fp, "bin_id,zone,waste_type,capacity,fill_level,x,y,wpi,collected_today\n");
    }

    for (int i = 0; i < n; i++) {
        printf("\n--- Enter Details for Bin %d ---", i + 1);

        // Zone
        printf("\nEnter the Bin Zone (1-%d): ", MAX_ZONE);
        scanf("%d", &bins[i].zone);
        if (bins[i].zone < 1 || bins[i].zone > MAX_ZONE) {
            printf("Invalid Bin Zone!\n");
            i--; continue;
        }

        // Waste Type
        printf("\nSelect Waste Type:\n");
        printf("  1. DRY\n  2. WET\n  3. MIXED\n  4. HAZARDOUS\n");
        printf("Option: ");
        scanf("%d", &bins[i].waste_type);
        if (bins[i].waste_type < 1 || bins[i].waste_type > 4) {
            printf("Invalid Waste Type!\n");
            i--; continue;
        }

        // Capacity
        printf("Enter Capacity (kg): ");
        scanf("%f", &bins[i].capacity);
        if (bins[i].capacity <= 0) {
            printf("Capacity must be > 0!\n");
            i--; continue;
        }

        // Fill Level
        printf("Enter Fill Level (0-100%%): ");
        scanf("%f", &bins[i].fill_level);
        if (bins[i].fill_level < 0 || bins[i].fill_level > 100) {
            printf("Invalid fill level!\n");
            i--; continue;
        }

        // Coordinates
        printf("Enter Location (x y): ");
        scanf("%d %d", &bins[i].x, &bins[i].y);

        // Generate ID
        long long int id = generateBinID(bins[i].waste_type, bins[i].zone);
        if (id < 0) { i--; continue; }

        bins[i].bin_id          = id;
        bins[i].collected_today = false;
        bins[i].wpi             = computeWPI(bins[i].fill_level, bins[i].waste_type);
        strcpy(bins[i].last_collection, "N/A");

        // ── FIX: waste_type written as string; bin_id uses %lld ───────────────
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

        printf("Bin %lld created successfully!\n", bins[i].bin_id);
    }

    fclose(fp);
}
int loadBinsFromCSV() {
    FILE *fp = fopen(BINS_CSV, "r");
    if (!fp) {
        printf("No bin database found. Please create bins first.\n");
        return 0;
    }

    char line[256];
    fgets(line, sizeof(line), fp); // skip header

    int count = 0;
    char wasteTypeStr[20];

    while (count < MAX_BINS &&
           fscanf(fp, "%lld,%d,%[^,],%f,%f,%d,%d,%f,%d\n",
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
        // Recompute WPI fresh on every load for consistency
        bins[count].wpi = computeWPI(bins[count].fill_level, bins[count].waste_type);
        count++;
    }

    fclose(fp);
    return count;
}
void sortBinsByPriority(int totalBins) {
    for (int i = 0; i < totalBins - 1; i++) {
        for (int j = 0; j < totalBins - i - 1; j++) {
            if (bins[j].priority < bins[j + 1].priority) {
                Bin tmp      = bins[j];
                bins[j]      = bins[j + 1];
                bins[j + 1]  = tmp;
            }
        }
    }
}
int identifyCriticalBins() {
    printf("\n========================================");
    printf("\n    IDENTIFY CRITICAL BINS MODULE       ");
    printf("\n========================================\n");

    // Step 1 – load from CSV
    int totalBins = loadBinsFromCSV();
    if (totalBins == 0) {
        printf("No bins available.\n");
        return 0;
    }

    int criticalCount = 0;
    int medCount      = 0;
    int lowCount      = 0;

    // Step 2 – compute WPI + assign priority (follows your flowchart exactly)
    for (int i = 0; i < totalBins; i++) {

        // Already collected today → LOW, skip further checks
        if (bins[i].collected_today) {
            bins[i].priority = PRI_LOW;
            lowCount++;
            continue;
        }

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

    // Step 3 – sort HIGH → MED → LOW
    sortBinsByPriority(totalBins);

    // Step 4 – display report table
    printf("\n%-14s %-6s %-10s %-10s %-7s %-8s  %s\n",
           "Bin ID", "Zone", "WasteType", "Fill(%)", "WPI", "Priority", "Status");
    printf("------------------------------------------------------------------------\n");

    for (int i = 0; i < totalBins; i++) {
        printf("%-14lld %-6d %-10s %-10.1f %-7.2f %-8s  ",
               bins[i].bin_id,
               bins[i].zone,
               getWasteTypeString(bins[i].waste_type),
               bins[i].fill_level,
               bins[i].wpi,
               getPriorityString(bins[i].priority));

        if (bins[i].collected_today)
            printf("[Already Collected]");
        else if (bins[i].priority == PRI_HIGH)
            printf("*** NEEDS COLLECTION ***");
        else if (bins[i].priority == PRI_MED)
            printf("Monitor");
        else
            printf("OK");

        printf("\n");
    }

    printf("------------------------------------------------------------------------\n");
    printf("Total Bins     : %d\n", totalBins);
    printf("HIGH  (Critical): %d  [Fill >= %.0f%% OR WPI >= %.0f]\n",
           criticalCount, FILL_THRESH_HIGH, WPI_THRESH_HIGH);
    printf("MEDIUM          : %d  [Fill >= %.0f%% OR WPI >= %.0f]\n",
           medCount, FILL_THRESH_MED, WPI_THRESH_MED);
    printf("LOW   (OK)      : %d\n", lowCount);
    printf("========================================\n");

    return criticalCount;
}
// ══════════════════════════════════════════════════════════════════════════════
//  DRIVER MANAGEMENT MODULE
// ══════════════════════════════════════════════════════════════════════════════
int generateDriverID(bool hasHazmat) {
    FILE *fp = fopen(DRIVERS_CSV, "r");

    // Set the prefix based on certification
    // 1000 for normal, 8000 for hazardous
    int prefix = hasHazmat ? 8000 : 1000;
    int maxSerial = 0;

    // If the file doesn't exist yet, start with the first ID (e.g., 1001 or 8001)
    if (fp == NULL) {
        return prefix + 1;
    }

    char line[512];
    // Read and skip the header line
    if (fgets(line, sizeof(line), fp) == NULL) {
        fclose(fp);
        return prefix + 1;
    }

    int existingID;
    // Scan the first column (ID) of every row
    while (fgets(line, sizeof(line), fp) != NULL) {
        if (sscanf(line, "%d,", &existingID) == 1) {

            // If we are looking for a Hazmat ID and found an 8xxx ID
            if (hasHazmat && existingID >= 8000) {
                int serial = existingID - 8000;
                if (serial > maxSerial) maxSerial = serial;
            }
            // If we are looking for a Standard ID and found a 1xxx ID
            else if (!hasHazmat && existingID >= 1000 && existingID < 8000) {
                int serial = existingID - 1000;
                if (serial > maxSerial) maxSerial = serial;
            }
        }
    }

    fclose(fp);

    // Return the prefix + the next available serial number
    return prefix + (maxSerial + 1);
}
void createDriver() {
    int n;
    printf("\n========================================");
    printf("\n     CREATE DRIVER RECORDS MODULE       ");
    printf("\n========================================\n");
    printf("\nEnter the No of Driver Records to Be Created: ");
    scanf("%d", &n);

    FILE *fp = fopen(DRIVERS_CSV, "a");
    if (!fp) {
        printf("Error: Could not open driver database.\n");
        return;
    }

    // Use our universal helper to check for header
    if (isCsvEmpty(DRIVERS_CSV)) {
        fprintf(fp, "driver_id,name,phone,vehicle_id,available,suspended,hazmat,hours_today,max_hours\n");
    }

    for (int i = 0; i < n; i++) {
        Driver d;
        int hazChoice;

        printf("\n--- Driver %d Details ---", i + 1);

        printf("\nEnter Name: ");
        scanf(" %[^\n]s", d.name); // Space before % clears input buffer

        printf("Enter 10-Digit Phone Number: ");
        scanf("%s", d.phone);

        printf("Hazardous Handling Certified? (1=Yes, 0=No): ");
        scanf("%d", &hazChoice);
        d.has_hazmat_license = (hazChoice == 1);

        // Generate ID based on the specialized sequence
        d.driver_id = generateDriverID(d.has_hazmat_license);

        printf("Enter Max Daily Working Hours: ");
        scanf("%f", &d.max_daily_hours);
        if (d.max_daily_hours > 12 || d.max_daily_hours < 1) {
            printf("\nMax Daily Working Hours Cannot be greater than 12 hours; Enter Max Daily Working Hours again: ");
            scanf("%f", &d.max_daily_hours);
        }
        // System Defaults for new drivers
        d.assigned_vehicle_id = 0;
        d.is_available = true;
        d.is_suspended = false;
        d.hours_worked_today = 0.0f;

        // Write to CSV
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
        printf("Success! Driver ID %d generated for %s.\n", d.driver_id, d.name);
    }
}
int loadDriversFromCSV() {
    FILE *fp = fopen(DRIVERS_CSV, "r");
    if (!fp) return 0;

    char line[512];
    if (fgets(line, sizeof(line), fp) == NULL) { // Skip header safely
        fclose(fp);
        return 0;
    }

    int count = 0;
    // Added commas after %[^,] to clear them from the buffer
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

void viewDrivers() {
    printf("\n========================================");
    printf("\n      VIEW ALL DRIVERS MODULE          ");
    printf("\n========================================\n");

    int total = loadDriversFromCSV();
    if (total == 0) {
        printf("No driver records found.\n");
        return;
    }

    printf("%-10s %-15s %-12s %-10s %-8s %-10s\n",
           "ID", "Name", "Phone", "Vehicle", "Hazmat", "Status");
    printf("----------------------------------------------------------------------\n");

    for (int i = 0; i < total; i++) {
        printf("%-10d %-15s %-12s %-10d %-8s %-10s\n",
               drivers[i].driver_id,
               drivers[i].name,
               drivers[i].phone,
               drivers[i].assigned_vehicle_id,
               drivers[i].has_hazmat_license ? "YES" : "NO",
               drivers[i].is_suspended ? "SUSPENDED" : (drivers[i].is_available ? "READY" : "ON-ROUTE"));
    }
    printf("----------------------------------------------------------------------\n");
}
// ══════════════════════════════════════════════════════════════════════════════
//  WASTE CLASSIFICATION MODULE - USER
// ══════════════════════════════════════════════════════════════════════════════
// SANJAY'S PART
//Hellojgjg

// ══════════════════════════════════════════════════════════════════════════════
//  VEHICLE AND DRIVER ASSIGNMENT MODULE - ADMIN
// ══════════════════════════════════════════════════════════════════════════════
// WHOEVER COMFORTABLE DOING THIS

// ══════════════════════════════════════════════════════════════════════════════
//  ROUTE ASSIGNMENT MODULE - ADMIN
// ══════════════════════════════════════════════════════════════════════════════
// YOGESH'S PART


// ══════════════════════════════════════════════════════════════════════════════
//  ROUTE OPTIMIZATION MODULE - ADMIN
// ══════════════════════════════════════════════════════════════════════════════
// SHYAM'S PART


// ══════════════════════════════════════════════════════════════════════════════
//  USER MODULE
// ══════════════════════════════════════════════════════════════════════════════

int isValidZone(int zone) {
    return (zone >= 1 && zone <= MAX_ZONE);
}

void saveBinsToCSV(int total) {
    FILE *fp = fopen(BINS_CSV, "w");
    if (!fp) {
        printf("Error saving bins!\n");
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

void viewBinsByZone() {
    int zone;
    printf("Enter your zone: ");
    scanf("%d", &zone);

    if (!isValidZone(zone)) {
        printf("Invalid zone!\n");
        return;
    }

    int total = loadBinsFromCSV();
    if (total == 0) return;

    printf("\n--- Bins in Zone %d ---\n", zone);

    int found = 0;

    for (int i = 0; i < total; i++) {
        if (bins[i].zone == zone) {
            printf("ID: %lld | Type: %-10s | Fill: %5.1f%% | WPI: %6.2f\n",
                   bins[i].bin_id,
                   getWasteTypeString(bins[i].waste_type),
                   bins[i].fill_level,
                   bins[i].wpi);
            found = 1;
        }
    }

    if (!found) printf("No bins found in your zone.\n");
}

void throwWaste() {
    int zone, type;
    float waste;

    printf("Enter your zone: ");
    scanf("%d", &zone);

    if (!isValidZone(zone)) {
        printf("Invalid zone!\n");
        return;
    }

    printf("Select Waste Type (1-DRY, 2-WET, 3-MIXED, 4-HAZARDOUS): ");
    scanf("%d", &type);

    if (type < 1 || type > 4) {
        printf("Invalid waste type!\n");
        return;
    }

    int total = loadBinsFromCSV();
    if (total == 0) return;

    int found = 0;

    printf("\nAvailable bins:\n");
    for (int i = 0; i < total; i++) {
        if (bins[i].zone == zone && bins[i].waste_type == type) {
            printf("ID: %lld | Fill: %.1f%%\n",
                   bins[i].bin_id,
                   bins[i].fill_level);
            found = 1;
        }
    }

    if (!found) {
        printf("No matching bins found!\n");
        return;
    }

    long long int id;
    printf("Enter Bin ID: ");
    scanf("%lld", &id);

    printf("Enter waste amount (%%): ");
    scanf("%f", &waste);

    if (waste <= 0) {
        printf("Invalid waste amount!\n");
        return;
    }

    for (int i = 0; i < total; i++) {
        if (bins[i].bin_id == id) {

            if (bins[i].zone != zone || bins[i].waste_type != type) {
                printf("Wrong bin selected!\n");
                return;
            }

            if (bins[i].fill_level + waste <= 100.0f) {
                bins[i].fill_level += waste;
                bins[i].wpi = computeWPI(bins[i].fill_level, bins[i].waste_type);

                printf("Waste added successfully!\n");

                saveBinsToCSV(total);

                printf("\n[System Update]\n");
                identifyCriticalBins();

            } else {
                printf("Bin overflow! Choose another bin.\n");
            }
            return;
        }
    }

    printf("Invalid Bin ID!\n");
}

void raiseComplaint() {
    long long int id;

    int total = loadBinsFromCSV();
    if (total == 0) return;

    printf("Enter Bin ID to raise complaint: ");
    scanf("%lld", &id);

    for (int i = 0; i < total; i++) {
        if (bins[i].bin_id == id) {

            if (bins[i].fill_level >= 85) {

                bins[i].wpi += 25;
                if (bins[i].wpi > 100) bins[i].wpi = 100;

                printf("Complaint registered!\n");

                saveBinsToCSV(total);

                printf("\n[System Alert]\n");
                identifyCriticalBins();

            } else {
                printf("Bin not critical enough.\n");
            }
            return;
        }
    }

    printf("Invalid Bin ID!\n");
}

void suggestBestBin() {
    int zone, type;

    printf("Enter your zone: ");
    scanf("%d", &zone);

    printf("Enter waste type (1-4): ");
    scanf("%d", &type);

    int total = loadBinsFromCSV();
    if (total == 0) return;

    int best = -1;
    float minFill = 101;

    for (int i = 0; i < total; i++) {
        if (bins[i].zone == zone && bins[i].waste_type == type) {
            if (bins[i].fill_level < minFill) {
                minFill = bins[i].fill_level;
                best = i;
            }
        }
    }

    if (best == -1) {
        printf("No suitable bin found.\n");
        return;
    }

    printf("\nSuggested Bin:\n");
    printf("ID: %lld | Fill: %.1f%% | WPI: %.2f\n",
           bins[best].bin_id,
           bins[best].fill_level,
           bins[best].wpi);
}


// ══════════════════════════════════════════════════════════════════════════════
//  MENU MODULE
// ══════════════════════════════════════════════════════════════════════════════

void adminMenu(const char* adminName) {
    int choice, select;
    while (1) {
        printf("\n===========================================================");
        printf("\n        SENTRABIN OS - Centralized Bin Operations       ");
        printf("\n===========================================================");
        printf("\n  OPERATOR: %s | DATABASE: %d Bins Loaded", adminName, total);
        printf("\n------------------------------------------------");
        printf("\n1. BIN MANAGEMENT");
        printf("\n2. VEHICLE MANAGEMENT");
        printf("\n3. DRIVER MANAGEMENT");
        printf("\n4. EXIT");
        printf("\nEnter your choice: ");
        scanf("%d", &select);

        if (select == 1) {
            printf("\n1. Create Bin Record\n2. Identify Critical Bins\n0. Back\nChoice: ");
            scanf("%d", &choice);
            if (choice == 1) createBin();
            else if (choice == 2) identifyCriticalBins();
        }
        else if (select == 2) {
            printf("\n1. Create Vehicle Record\n2. View Vehicles\n0. Back\nChoice: ");
            scanf("%d", &choice);
            if (choice == 1) createVehicle();
            else if (choice == 2) viewVehicles();
        }
        else if (select == 3) { // FIXED: checking 'select' not 'choice'
            printf("\n1. Create Driver Record\n2. View Driver Records\n0. Back\nChoice: ");
            scanf("%d", &choice);
            if (choice == 1) createDriver();
            else if (choice == 2) viewDrivers();
        }
        else if (select == 4) {
            printf("\nExiting Program. Goodbye!\n");
            return;
        }
    }
}

    void netizenMenu() {
        int choice;
        while (1) {
            printf("===========================================================\n");
            printf("        SENTRABIN OS - Centralized Bin Operations       \n");
            printf("===========================================================\n");
            printf("\n1. View Local Bins (Critical Only)");
            printf("\n0. Logout");
            printf("\nEnter Choice: ");

            if (scanf("%d", &choice) != 1) {
                while(getchar() != '\n');
                continue;
            }

            switch (choice) {
                case 1: identifyCriticalBins(); break; // Netizens can see, but maybe not edit
                case 0: return;
                default: printf("Invalid choice.\n");
            }
        }


    }
}

void netizenMenu(char *username) {
    int choice;

    while (1) {
        printf("\n========================================\n");
        printf("        SENTRABIN OS - USER MODE       \n");
        printf("========================================\n");

        printf("1. View Bins\n");
        printf("2. Throw Waste\n");
        printf("3. Raise Complaint\n");
        printf("4. Suggest Best Bin\n");
        printf("0. Logout\n");

        printf("Enter Choice: ");

        if (scanf("%d", &choice) != 1) {
            while(getchar() != '\n');
            continue;
        }

        switch (choice) {
            case 1: viewBinsByZone(); break;
            case 2: throwWaste(); break;
            case 3: raiseComplaint(); break;
            case 4: suggestBestBin(); break;
            case 0: return;
            default: printf("Invalid choice!\n");
        }
    }
}

    }

// ══════════════════════════════════════════════════════════════════════════════
//      USER AUTHENTICATION MODULE
// ══════════════════════════════════════════════════════════════════════════════

    const char* userAUTH(char* outUsername) {
        char pass[32];


        printf("==========================================\n");
        printf("        SENTRABIN OS - SECURE LOGIN       \n");
        printf("==========================================\n");

        printf("\n  ENTER USERNAME: ");
        scanf("%39s", outUsername); // Store it in the buffer passed from main
        printf("  ENTER PASSCODE: ");
        scanf("%31s", pass);

        if (strcmp(pass, ADMIN_PASS) == 0) {
            printf("\n[AUTH] Admin privileges granted to: %s\n", outUsername);
            return "admin";
        }
        else if (strcmp(pass, NETIZEN_PASS) == 0) {
            printf("\n[AUTH] Netizen access granted to: %s\n", outUsername);
            return "netizen";
        }

        return "unauthorized";
    }

// ══════════════════════════════════════════════════════════════════════════════
//  MAIN FUNCTION
// ══════════════════════════════════════════════════════════════════════════════

    int main(){
        char username[32];
        const char* role = userAUTH(username);

        if (strcmp(role, "admin") == 0) {
            adminMenu(username);
        }
        else if (strcmp(role, "netizen") == 0) {
            netizenMenu(username);
        }
        else {
            printf("\n[ERROR] Access Denied. Closing SENTRABIN OS...\n");
            return 1;
        }

        printf("\nSession Ended. Goodbye!\n");
        return 0;

}
