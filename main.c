#include <stdio.h>
#include <stdbool.h>
//Bin Constants
#define MAX_BINS 100
#define MAX_ZONE 20
#define MAX_VEHICLES 20
#define MAX_DRIVERS 20
#define ADMIN_PASS "admin123"
#define BINS_CSV "bins.csv"
//Waste Types
#define DRY 1
#define WET 2
#define MIXED 3
#define HAZARDOUS 4
//Priority levels
#define PRI_HIGH 3
#define PRI_MED  2
#define PRI_LOW  1

// Bin Structure
typedef struct {
    int bin_id; // bin id
    int zone;   // zone
    int waste_type;     // waste type
    float fill_level;   // fill_level (in percentage out of 100)
    float capacity; // capacity(in kgs)
    float wpi; // waste pressure index
    int priority; // priority level : 1, 2, 3
    int x, y; // cartesian coordinates(x,y)
    char last_collection[10]; // Last Collection Date (in DD-MM-YYYY)
    bool collected_today; // today's collection status
} Bin;

Bin bins[MAX_BINS];
//Return Waste Type Module
const char* getWasteTypeString(int type) {
    switch(type) {
        case DRY: return "DRY";
        case WET: return "WET";
        case MIXED: return "MIXED";
        case HAZARDOUS: return "HAZARDOUS";
        default: return "UNKNOWN";
    }
}

int generateBinID(int waste_type, int zone) {
    FILE *fp = fopen(BINS_CSV, "r");

    int id, z, priority;
    char wasteTypeStr[20];
    float fill, cap, wpi;

    int maxSerial = 0;

    if (fp != NULL) {
        char line[200];
        fgets(line, sizeof(line), fp); // skip header

        while (fscanf(fp, "%d,%d,%[^,],%f,%f,%f,%d\n",
                      &id, &z, wasteTypeStr,
                      &fill, &cap, &wpi, &priority) == 7) {

            // Check same zone
            if (z == zone) {
                int serial = id % 1000; // last 3 digits
                if (serial > maxSerial)
                    maxSerial = serial;
            }
                      }

        fclose(fp);
    }

    int newSerial = maxSerial + 1;

    if (newSerial > 999) {
        printf("Max bins reached for this zone!\n");
        return -1;
    }

    // Construct ID
    int binID = (waste_type * 100000) + (zone * 1000) + newSerial;

    return binID;
}

//int computeWPI() TO BE IMPLEMENTED
/* Create Bin Module */
void createBin() {
    int n;
    printf("\nEnter the Number of Bins to be Created :");
    scanf("%d",&n);

    FILE *fp = fopen(BINS_CSV, "a");
    if (!fp) {
        printf("Error Loading Database! \n");
        return ;
    }
    for (int i = 0; i < n; i++) {
        // Zone
        printf("\nEnter the Details for Bin %d", i+1);
        printf("\nEnter the Bin Zone: ");
        scanf("%d",&bins[i].zone);
        if (bins[i].zone < 1 || bins[i].zone > MAX_ZONE) {
            printf("Invalid Bin Zone! \n");
            i--;
            continue;
        }
        //Waste Type
        printf("\nSelect the Waste Type: ");
        printf("\n1. DRY WASTE");
        printf("\n2. WET WASTE");
        printf("\n3. MIXED");
        printf("\n4. HAZARDOUS WASTE");
        printf("\n option Selected :");
        scanf("%d",&bins[i].waste_type);
        if (bins[i].waste_type < 1 || bins[i].waste_type > 4) {
            printf("Invalid Waste Type! \n");
            i--;
            continue;
        }
        //Capacity
        printf("Enter the Capacity (in kgs): ");
        scanf("%f",&bins[i].capacity);
        if (bins[i].capacity <= 0) {
            printf("Capacity cannot be less than or equal to 0");
            i--;
            continue;
        }
        //Fill Level
        printf("\nEnter the Fill Level (in %%): ");
        scanf("%f",&bins[i].fill_level);
        if (bins[i].fill_level < 0 || bins[i].fill_level > 100) {
            printf("\nInvalid Input");
            i--;
            continue;
        }
        // Bin Location Co ordinates(x,y)
        printf("\nEnter the Location Co-ordinates of Bin (x, y): ")
        scanf("%d %d",&bins[i].x, &bins[i].y);

        //Generate Bin ID
        int id = generateBinID(bins[i].waste_type,bins[i].zone);
        if (bins[i].bin_id < 0) {
            i--;
            continue;
        }
        // Initial Flags
        bins[i].bin_id  = id;
        bins[i].collected_today = false;
        //To Compute the WPI
        bins[i].wpi = computeWpi();//to be implemented




    }


}