#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX 500

// COLORS
#define RESET "\033[0m"
#define RED "\033[1;31m"
#define GREEN "\033[1;32m"
#define CYAN "\033[1;36m"
#define YELLOW "\033[1;33m"

// ---------------- STRUCTURES ----------------
struct Bin {
    int id;
    int type;     // 1-4
    int zone;     // 1-10
    int number;   // 1-999
    int fill;     // %
    int capacity; // kg
    int x, y;     // location
    int priority;
    int collected;
};

struct Vehicle {
    int id;
    int capacity; // kg
    int fuel;     // litres
    int zone;
    int type;     // 1-normal, 2-hazard
    int x, y;
};

// ---------------- GLOBAL ----------------
struct Bin bins[MAX];
struct Vehicle vehicles[MAX];

int n_bins = 0, n_vehicles = 0;
char admin_pass[50], user_pass[50];

// ---------------- SAFE INPUT ----------------
int safeInt() {
    int x;
    while (scanf("%d", &x) != 1) {
        printf(RED "Invalid input! Try again: " RESET);
        while (getchar() != '\n');
    }
    return x;
}

// ---------------- MANHATTAN DISTANCE ----------------
int distance(int x1, int y1, int x2, int y2) {
    return abs(x1 - x2) + abs(y1 - y2);
}

// ---------------- BIN ID ----------------
int generateID(int type, int zone, int num) {
    return type * 100000 + zone * 1000 + num;
}

// ---------------- LOGIN ----------------
void loadUsers() {
    FILE *fp = fopen("users.txt", "r");

    if (fp == NULL) {
        printf("Set Admin Password: ");
        scanf("%s", admin_pass);

        printf("Set User Password: ");
        scanf("%s", user_pass);

        fp = fopen("users.txt", "w");
        fprintf(fp, "admin %s\nuser %s", admin_pass, user_pass);
        fclose(fp);
    } else {
        fscanf(fp, "admin %s", admin_pass);
        fscanf(fp, "user %s", user_pass);
        fclose(fp);
    }
}

int login() {
    char role[20], pass[50];

    printf(CYAN "\nLogin\n" RESET);
    printf("Role (admin/user): ");
    scanf("%s", role);

    printf("Password: ");
    scanf("%s", pass);

    if (strcmp(role, "admin") == 0 && strcmp(pass, admin_pass) == 0)
        return 1;

    if (strcmp(role, "user") == 0 && strcmp(pass, user_pass) == 0)
        return 2;

    return 0;
}

// ---------------- PRIORITY ----------------
void identifyCriticalBins() {
    for (int i = 0; i < n_bins; i++) {
        if (bins[i].fill >= 100)
            bins[i].priority = 2;  // Emergency
        else if (bins[i].fill >= 80)
            bins[i].priority = 1;  // Critical
        else
            bins[i].priority = 0;  // Normal
    }
}

// ---------------- INPUT BINS ----------------
void inputBins() {
    printf("Number of bins: ");
    n_bins = safeInt();

    for (int i = 0; i < n_bins; i++) {
        printf("\nType (1-Dry 2-Wet 3-Mixed 4-Hazard): ");
        bins[i].type = safeInt();

        printf("Zone (1-10): ");
        bins[i].zone = safeInt();

        printf("Bin number (1-999): ");
        bins[i].number = safeInt();

        bins[i].id = generateID(bins[i].type, bins[i].zone, bins[i].number);

        printf("Capacity (kg): ");
        bins[i].capacity = safeInt();

        printf("Fill level (%%): ");
        bins[i].fill = safeInt();

        if (bins[i].fill > 100) bins[i].fill = 100;

        printf("Location (x y): ");
        bins[i].x = safeInt();
        bins[i].y = safeInt();

        bins[i].collected = 0;

        printf(GREEN "Generated Bin ID: %d\n" RESET, bins[i].id);
    }

    identifyCriticalBins();
}

// ---------------- INPUT VEHICLES ----------------
void inputVehicles() {
    printf("Number of vehicles: ");
    n_vehicles = safeInt();

    for (int i = 0; i < n_vehicles; i++) {
        vehicles[i].id = i;

        printf("Capacity (kg): ");
        vehicles[i].capacity = safeInt();

        printf("Fuel (litres): ");
        vehicles[i].fuel = safeInt();

        printf("Zone (1-10): ");
        vehicles[i].zone = safeInt();

        printf("Type (1-Normal 2-Hazard): ");
        vehicles[i].type = safeInt();

        printf("Start location (x y): ");
        vehicles[i].x = safeInt();
        vehicles[i].y = safeInt();
    }
}

// ---------------- ASSIGN VEHICLES ----------------
void assignVehicle() {

    identifyCriticalBins();

    printf(YELLOW "\n--- Scheduling Collection ---\n" RESET);

    for (int j = 0; j < n_bins; j++) {

        if (bins[j].collected) continue;

        int best = -1;
        int minDist = 1000000;

        for (int i = 0; i < n_vehicles; i++) {

            if (vehicles[i].fuel <= 0) continue;

            if (bins[j].type == 4 && vehicles[i].type != 2)
                continue;

            int d = distance(vehicles[i].x, vehicles[i].y,
                             bins[j].x, bins[j].y);

            if (d < minDist) {
                minDist = d;
                best = i;
            }
        }

        if (best != -1) {
            printf(CYAN "\nVehicle %d -> Bin %d (Dist: %d)\n" RESET,
                   best, bins[j].id, minDist);

            vehicles[best].x = bins[j].x;
            vehicles[best].y = bins[j].y;

            vehicles[best].fuel -= (minDist / 2);

            bins[j].collected = 1;
        }
        else {
            printf(RED "No vehicle available for Bin %d\n" RESET,
                   bins[j].id);
        }
    }
}

// ---------------- REPORT ----------------
void generateReport() {

    printf(YELLOW "\n===== REPORT =====\n" RESET);

    for (int i = 0; i < n_bins; i++) {
        printf("Bin %d | %d%% | %s\n",
               bins[i].id,
               bins[i].fill,
               bins[i].collected ? "Collected" : "Pending");
    }

    for (int i = 0; i < n_vehicles; i++) {
        printf("Vehicle %d | Fuel:%d litres\n",
               vehicles[i].id,
               vehicles[i].fuel);
    }
}

// ---------------- MAIN ----------------
int main() {

    printf(GREEN "\n🚮 Welcome to SentraBin OS 🚮\n" RESET);

    loadUsers();

    while (1) {
        int role = login();

        if (role == 1) {
            int ch;
            while (1) {
                printf("\n1.Bins 2.Vehicles 3.Schedule 4.Report 5.Logout\n");
                ch = safeInt();

                if (ch == 1) inputBins();
                else if (ch == 2) inputVehicles();
                else if (ch == 3) assignVehicle();
                else if (ch == 4) generateReport();
                else break;
            }
        }
        else if (role == 2) {
            generateReport();
        }
        else {
            printf(RED "Invalid Login\n" RESET);
        }
    }
}