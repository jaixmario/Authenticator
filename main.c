#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

#include "totp.h"
#include "storage.h"
#include "base32.h"

#ifdef _WIN32
#include <windows.h>
#define sleep(x) Sleep(1000 * (x))
#else
#include <unistd.h>
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

    printf("Loaded %d keys.\n\n", count);

    while (1) {
        long remaining = 30 - (time(NULL) % 30);
        printf("--- %ld sec ---\n", remaining);
        
        for (int i=0; i<count; i++) {
            unsigned char decoded[64];
            int key_len = base32_decode(entries[i].key, decoded);
            if (key_len <= 0) {
                printf("%-15s: Invalid Key\n", entries[i].nickname);
            } else {
                int code = totp(decoded, key_len);
                printf("%-15s: %06d\n", entries[i].nickname, code);
            }
        }
        printf("\n");
        sleep(1);
    }

    return 0;
}