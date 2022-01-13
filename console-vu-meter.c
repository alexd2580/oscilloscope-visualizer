#include<stdio.h>
#include<stdlib.h>
#include<math.h>

int main(int argc, char* argv[]) {
    const char *padding = "#####################################################";
    const char *empty = "                                                     ";
    int max = 50;

    unsigned char buffer[9] = { 0 };

    float left = 0;
    float right = 0;

    while (fgets(buffer, 9, stdin)) {
        for(int i = 0; i < 4; i++) {
            left = 0.9 * left + 0.1 * ((float)buffer[2 * i] - 127.0);
            right = 0.9 * right + 0.1 * ((float)buffer[2 * i + 1] - 127.0);
        }

        int num_left = (int)fabs(left);
        int num_right = (int)fabs(right);
        /* printf("%d %d\n", num_left, num_right); */
        /*  */
        // Left channel
        printf("%.*s%.*s\t", num_left, padding, max - num_left, empty);
        // Right channel
        printf("%.*s%.*s\r", num_right, padding, max - num_right, empty);;
        fflush(stdout);
    }
    fclose(stdin);

    return 0;
}
