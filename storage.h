#ifndef STORAGE_H
#define STORAGE_H

typedef struct {
    char nickname[64];
    char key[128];
} AuthEntry;

int load_entries(AuthEntry *entries, int max_entries);
int save_entry(const char *nickname, const char *key);
int delete_entry(const char *nickname);

int load_urls(char urls[][512], int max_urls);
int save_url(const char *url);

int load_cloud_api(char *url_out, size_t max_out);
int save_cloud_api(const char *url);
char* read_all_db();

#endif