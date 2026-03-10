
#include<stdio.h>
#include<stdlib.h>
#include<ctype.h>
    struct Bin{
        int bin_id, zone, x, y, capacity, fill_level;
        char type;
    };

    int main()
    {
        int user;
        int n;
        printf("Choose the user type:  "
               "1.Municipal Administrator"
               "2.User"
               "3.Exit");
        scanf("%d", &user);
        if(user == 1){
            printf("Enter the No of Bins in the zone: ");
            scanf("%d", &n);
            struct Bin bin[n];
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
                scanf("%c", &bin[i].type);
            }
            else if (user == 2) {
                int zone, type, waste_amount;
                printf("Enter the Zone : ");
                scanf("%d", &zone);
                printf("Choose the type of Waste : "
                       "1. Dry"
                       "2. Wet"
                       "3. Mixed"
                       "4. Hazardous");
                scanf("%d", &type);
                if (type == 1) {
                    printf("Enter the Waste Amount : ");
                    scanf("%d", &waste_amount);
                    for (int i = 0; i < n; i++) {
                        bin[i].
                    }

                }


            }

        }
    }

