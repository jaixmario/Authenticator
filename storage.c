#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

#include "storage.h"

void get_db_path(char *buffer, size_t size) {
    const char *home = getenv("USERPROFILE");
    if (!home) home = getenv("HOME");
    if (!home) home = ".";

    snprintf(buffer, size, "%s/.authenticator", home);
    
#ifdef _WIN32
    CreateDirectory(buffer, NULL);
#else
    struct stat st = {0};
    if (stat(buffer, &st) == -1) {
        mkdir(buffer, 0700);
    }
#endif

    snprintf(buffer, size, "%s/.authenticator/db.json", home);
}

int load_entries(AuthEntry *entries, int max_entries) {
    char path[512];
    get_db_path(path, sizeof(path));

    FILE *f = fopen(path, "r");
    if (!f) return 0;

    int count = 0;
    char line[512];
    while (fgets(line, sizeof(line), f) && count < max_entries) {
        char *quote1 = strchr(line, '"');
        if (!quote1) continue;
        char *quote2 = strchr(quote1 + 1, '"');
        if (!quote2) continue;
        
        char *colon = strchr(quote2 + 1, ':');
        if (!colon) continue;

        char *quote3 = strchr(colon + 1, '"');
        if (!quote3) continue;
        char *quote4 = strchr(quote3 + 1, '"');
        if (!quote4) continue;

        int nick_len = quote2 - quote1 - 1;
        int key_len = quote4 - quote3 - 1;

        if (nick_len >= 64) nick_len = 63;
        if (key_len >= 128) key_len = 127;

        strncpy(entries[count].nickname, quote1 + 1, nick_len);
        entries[count].nickname[nick_len] = '\0';

        strncpy(entries[count].key, quote3 + 1, key_len);
        entries[count].key[key_len] = '\0';

        count++;
    }

    fclose(f);
    return count;
}

int save_entry(const char *nickname, const char *key) {
    char path[512];
    get_db_path(path, sizeof(path));

    AuthEntry entries[100];
    int count = load_entries(entries, 100);

    int found_index = -1;
    for (int i=0; i<count; i++) {
        if (strcmp(entries[i].nickname, nickname) == 0) {
            found_index = i;
            break;
        }
    }

    if (found_index >= 0) {
        if (strcmp(entries[found_index].key, key) == 0) {
            printf("  -> Key for '%s' matches exactly. Skipping.\n", nickname);
            return 1;
        } else {
            printf("  -> Conflict! '%s' has a different key.\n", nickname);
            printf("     (r)eplace existing or (k)eep both as '%s 2'? [r/k]: ", nickname);
            char choice[16];
            if (fgets(choice, sizeof(choice), stdin)) {
                if (choice[0] == 'r' || choice[0] == 'R') {
                    strncpy(entries[found_index].key, key, 127);
                    entries[found_index].key[127] = '\0';
                } else if (choice[0] == 'k' || choice[0] == 'K') {
                    if (count < 100) {
                        snprintf(entries[count].nickname, 64, "%s 2", nickname);
                        strncpy(entries[count].key, key, 127);
                        entries[count].key[127] = '\0';
                        count++;
                    } else {
                        printf("  -> Database is full!\n");
                        return 0;
                    }
                } else {
                    printf("  -> Aborted.\n");
                    return 0;
                }
            } else {
                return 0;
            }
        }
    } else {
        if (count < 100) {
            strncpy(entries[count].nickname, nickname, 63);
            entries[count].nickname[63] = '\0';
            strncpy(entries[count].key, key, 127);
            entries[count].key[127] = '\0';
            count++;
        } else {
            printf("  -> Database is full!\n");
            return 0;
        }
    }

    FILE *f = fopen(path, "w");
    if (!f) return 0;

    fprintf(f, "{\n");
    for (int i=0; i<count; i++) {
        fprintf(f, "  \"%s\": \"%s\"%s\n", entries[i].nickname, entries[i].key, i == count - 1 ? "" : ",");
    }
    fprintf(f, "}\n");

    fclose(f);
    return 1;
}

int delete_entry(const char *nickname) {
    char path[512];
    get_db_path(path, sizeof(path));

    AuthEntry entries[100];
    int count = load_entries(entries, 100);

    int updated = 0;
    for (int i=0; i<count; i++) {
        if (strcmp(entries[i].nickname, nickname) == 0) {
            for (int j=i; j<count - 1; j++) {
                entries[j] = entries[j+1];
            }
            count--;
            updated = 1;
            break;
        }
    }

    if (!updated) return 0; // Not found

    FILE *f = fopen(path, "w");
    if (!f) return 0;

    fprintf(f, "{\n");
    for (int i=0; i<count; i++) {
        fprintf(f, "  \"%s\": \"%s\"%s\n", entries[i].nickname, entries[i].key, i == count - 1 ? "" : ",");
    }
    fprintf(f, "}\n");

    fclose(f);
    return 1;
}

void get_urls_path(char *buffer, size_t size) {
    const char *home = getenv("USERPROFILE");
    if (!home) home = getenv("HOME");
    if (!home) home = ".";
    snprintf(buffer, size, "%s/.authenticator/urls.json", home);
}

int load_urls(char urls[][512], int max_urls) {
    char path[512];
    get_urls_path(path, sizeof(path));
    FILE *f = fopen(path, "r");
    if (!f) return 0;

    int count = 0;
    char line[1024];
    while (fgets(line, sizeof(line), f) && count < max_urls) {
        char *quote1 = strchr(line, '"');
        if (!quote1) continue;
        char *quote2 = strchr(quote1 + 1, '"');
        if (!quote2) continue;
        
        int len = quote2 - quote1 - 1;
        if (len >= 512) len = 511;
        
        strncpy(urls[count], quote1 + 1, len);
        urls[count][len] = '\0';
        count++;
    }
    fclose(f);
    return count;
}

int save_url(const char *url) {
    char urls[20][512];
    int count = load_urls(urls, 20);
    
    for (int i=0; i<count; i++) {
        if (strcmp(urls[i], url) == 0) return 1;
    }
    
    if (count < 20) {
        strncpy(urls[count], url, 511);
        urls[count][511] = '\0';
        count++;
    }
    
    char path[512];
    get_urls_path(path, sizeof(path));
    FILE *f = fopen(path, "w");
    if (!f) return 0;
    
    fprintf(f, "[\n");
    for (int i=0; i<count; i++) {
        fprintf(f, "  \"%s\"%s\n", urls[i], i == count - 1 ? "" : ",");
    }
    fprintf(f, "]\n");
    fclose(f);
    return 1;
}

void get_cloud_path(char *buffer, size_t size) {
    const char *home = getenv("USERPROFILE");
    if (!home) home = getenv("HOME");
    if (!home) home = ".";
    snprintf(buffer, size, "%s/.authenticator/cloud.json", home);
}

int load_cloud_api(char *url_out, size_t max_out) {
    char path[512];
    get_cloud_path(path, sizeof(path));
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    
    char line[1024];
    int loaded = 0;
    while (fgets(line, sizeof(line), f)) {
        char *quote1 = strchr(line, '"');
        if (!quote1) continue;
        char *quote2 = strchr(quote1 + 1, '"');
        if (!quote2) continue;
        char *colon = strchr(quote2 + 1, ':');
        if (!colon) continue;
        char *quote3 = strchr(colon + 1, '"');
        if (!quote3) continue;
        char *quote4 = strchr(quote3 + 1, '"');
        if (!quote4) continue;
        
        int len = (int)(quote4 - quote3 - 1);
        if (len >= max_out) len = (int)(max_out - 1);
        
        strncpy(url_out, quote3 + 1, len);
        url_out[len] = '\0';
        loaded = 1;
        break;
    }
    fclose(f);
    return loaded;
}

int save_cloud_api(const char *url) {
    char path[512];
    get_cloud_path(path, sizeof(path));
    FILE *f = fopen(path, "w");
    if (!f) return 0;
    fprintf(f, "{\n  \"api\": \"%s\"\n}\n", url);
    fclose(f);
    return 1;
}

char* read_all_db() {
    char path[512];
    get_db_path(path, sizeof(path));
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char *string = malloc(fsize + 1);
    if (!string) {
        fclose(f);
        return NULL;
    }
    size_t readBytes = fread(string, 1, fsize, f);
    string[readBytes] = '\0';
    fclose(f);
    return string;
}