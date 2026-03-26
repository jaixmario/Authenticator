#ifndef STORAGE_H
#define STORAGE_H

typedef struct {
    char nickname[64];
    char key[128];
} AuthEntry;

int load_entries(AuthEntry *entries, int max_entries);
int save_entry(const char *nickname, const char *key);

int load_urls(char urls[][512], int max_urls);
int save_url(const char *url);

#endif