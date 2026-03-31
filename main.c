
#include<stdio.h>
#include<stdlib.h>
#include<ctype.h>

int n;
struct Bin bin[100]; // Assuming max 100 bins

struct Bin{
    int bin_id, zone, x, y, capacity, fill_level;
    char type;
};

void createBin()
{
    printf("Enter the No of Bins in the zone: ");
    scanf("%d", &n);
    for(int i = 0; i < n; i++)
    {
        printf("Enter the Bin Id : ");
        scanf("%d", &bin[i].bin_id);
        printf("Enter the Zone Number : ");
        scanf("%d", &bin[i].zone);
        printf("Enter the Bin location Co - ordinates(x, y): ");
        scanf("%d%d", &bin[i].x, &bin[i].y);
        printf("Enter the Capacity : ");
        scanf("%d", &bin[i].capacity);
        printf("Enter the Fill Level : ");
        scanf("%d", &bin[i].fill_level);
        printf("Enter the Bin Type (Dry - D, Wet - W, Mixed - M, Hazardous - H) : ");
        scanf(" %c", &bin[i].type);
    }
    printf("Bins Created Successfully!\n");
}

int main()
{
    int user;
    printf("Choose the user type:\n"
           "1. Municipal Administrator\n"
           "2. User\n"
           "3. Exit\n");
    scanf("%d", &user);
    if(user == 1){
        createBin();
    }
    else if (user == 2) {
        int zone, type, waste_amount;
        printf("Enter the Zone : ");
        scanf("%d", &zone);
        printf("Choose the type of Waste :\n"
               "1. Dry\n"
               "2. Wet\n"
               "3. Mixed\n"
               "4. Hazardous\n");
        scanf("%d", &type);
        if (type == 1) {
            printf("Enter the Waste Amount : ");
            scanf("%d", &waste_amount);
            // Assuming some logic here, e.g., find bins in zone with type 'D'
            for (int i = 0; i < n; i++) {
                if (bin[i].zone == zone && bin[i].type == 'D') {
                    // Add logic to handle waste
                    printf("Bin %d can accept waste.\n", bin[i].bin_id);
                }
            }
        }
        // Add cases for other types
    }
    return 0;
}
    

