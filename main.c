#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

#include "totp.h"
#include "storage.h"
#include "base32.h"

#ifdef _WIN32
#include <windows.h>
#include <stdlib.h>
#define sleep(x) Sleep(1000 * (x))
void move_up(int lines) {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(hOut, &csbi)) {
        csbi.dwCursorPosition.Y -= lines;
        if (csbi.dwCursorPosition.Y < 0) csbi.dwCursorPosition.Y = 0;
        csbi.dwCursorPosition.X = 0;
        SetConsoleCursorPosition(hOut, csbi.dwCursorPosition);
    }
}
void clear_screen() {
    system("cls");
}
#else
#include <unistd.h>
#include <stdlib.h>
void move_up(int lines) {
    printf("\033[%dA\r", lines);
}
void clear_screen() {
    system("clear");
}
#endif

int main(int argc, char *argv[]) {
    setvbuf(stdout, NULL, _IONBF, 0);
    if (argc >= 3) {
        save_entry(argv[1], argv[2]);
        printf("Saved key '%s' under nickname '%s'\n", argv[2], argv[1]);
        return 0;
    }

    AuthEntry entries[100];
    int count = load_entries(entries, 100);

    if (count == 0) {
        printf("No keys found. Add one with: auth.exe <nickname> <key>\n");
        return 1;
    }

    clear_screen();
    int first_iteration = 1;

    while (1) {
        long remaining = 30 - (time(NULL) % 30);
        
        if (!first_iteration) {
            move_up(count + 2);
        }
        first_iteration = 0;

        printf("===== Refreshing in %02ld sec =====\n", remaining);
        
        for (int i=0; i<count; i++) {
            unsigned char decoded[64];
            int key_len = base32_decode(entries[i].key, decoded);
            if (key_len <= 0) {
                printf("%-15s: Invalid Key      \n", entries[i].nickname);
            } else {
                int code = totp(decoded, key_len);
                printf("%-15s: %06d           \n", entries[i].nickname, code);
            }
        }
        printf("\n");
        sleep(1);
    }

    return 0;
}