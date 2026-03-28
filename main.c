#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

#include "totp.h"
#include "storage.h"
#include "base32.h"

#ifdef _WIN32
#include <windows.h>
#include <wininet.h>
#include <stdlib.h>
#pragma comment(lib, "wininet.lib")

#define sleep(x) Sleep(1000 * (x))

int download_key(const char* url, char* key_out, size_t max_len) {
    HINTERNET hInternet = InternetOpenA("Authenticator/1.0", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!hInternet) return 0;
    
    HINTERNET hConnect = InternetOpenUrlA(hInternet, url, NULL, 0, INTERNET_FLAG_RELOAD | INTERNET_FLAG_SECURE, 0);
    if (!hConnect) {
        InternetCloseHandle(hInternet);
        return 0;
    }
    
    DWORD bytesRead;
    size_t totalRead = 0;
    while (InternetReadFile(hConnect, key_out + totalRead, (DWORD)(max_len - totalRead - 1), &bytesRead) && bytesRead > 0) {
        totalRead += bytesRead;
        if (totalRead >= max_len - 1) break;
    }
    key_out[totalRead] = '\0';
    
    while(totalRead > 0 && (key_out[totalRead-1] == '\n' || key_out[totalRead-1] == '\r' || key_out[totalRead-1] == ' ')) {
        key_out[--totalRead] = '\0';
    }
    
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);
    return totalRead > 0;
}

int upload_json_to_api(const char* url, const char* json_data) {
    URL_COMPONENTSA urlComp = {0};
    urlComp.dwStructSize = sizeof(urlComp);
    char host[256] = {0};
    char path[1024] = {0};
    urlComp.lpszHostName = host;
    urlComp.dwHostNameLength = sizeof(host);
    urlComp.lpszUrlPath = path;
    urlComp.dwUrlPathLength = sizeof(path);
    urlComp.dwSchemeLength = 1; 
    
    if (!InternetCrackUrlA(url, 0, 0, &urlComp)) return 0;

    HINTERNET hInternet = InternetOpenA("Authenticator/1.0", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!hInternet) return 0;
    
    HINTERNET hConnect = InternetConnectA(hInternet, host, urlComp.nPort, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
    if (!hConnect) {
        InternetCloseHandle(hInternet);
        return 0;
    }
    
    DWORD flags = INTERNET_FLAG_RELOAD;
    if (urlComp.nScheme == INTERNET_SCHEME_HTTPS) flags |= INTERNET_FLAG_SECURE;
    
    HINTERNET hRequest = HttpOpenRequestA(hConnect, "POST", path, NULL, NULL, NULL, flags, 0);
    if (!hRequest) {
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        return 0;
    }
    
    const char* headers = "Content-Type: application/json\r\n";
    BOOL sent = HttpSendRequestA(hRequest, headers, (DWORD)strlen(headers), (LPVOID)json_data, (DWORD)strlen(json_data));
    
    if (sent) {
        DWORD statusCode = 0;
        DWORD length = sizeof(statusCode);
        HttpQueryInfoA(hRequest, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &statusCode, &length, NULL);
        if (statusCode >= 200 && statusCode < 300) sent = TRUE;
        else sent = FALSE;
    }
    
    InternetCloseHandle(hRequest);
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);
    return sent ? 1 : 0;
}

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

int download_key(const char* url, char* key_out, size_t max_len) {
    char command[2048];
    snprintf(command, sizeof(command), "curl -s \"%s\"", url);
    FILE *fp = popen(command, "r");
    if (!fp) return 0;
    
    size_t totalRead = fread(key_out, 1, max_len - 1, fp);
    key_out[totalRead] = '\0';
    
    while(totalRead > 0 && (key_out[totalRead-1] == '\n' || key_out[totalRead-1] == '\r' || key_out[totalRead-1] == ' ')) {
        key_out[--totalRead] = '\0';
    }
    
    int status = pclose(fp);
    return (status == 0 && totalRead > 0) ? 1 : 0;
}

int upload_json_to_api(const char* url, const char* json_data) {
    char template[] = "/tmp/auth_upload_XXXXXX";
    int fd = mkstemp(template);
    if (fd == -1) return 0;
    
    FILE *tmp = fdopen(fd, "w");
    if (!tmp) {
        close(fd);
        remove(template);
        return 0;
    }
    fputs(json_data, tmp);
    fclose(tmp);
    
    char command[4096];
    snprintf(command, sizeof(command), "curl -s -o /dev/null -w \"%%{http_code}\" -X POST \"%s\" -H \"Content-Type: application/json\" -d @%s", url, template);
    
    FILE *fp = popen(command, "r");
    if (!fp) {
        remove(template);
        return 0;
    }
    
    char result[32] = {0};
    int success = 0;
    if (fgets(result, sizeof(result), fp) != NULL) {
        int code = atoi(result);
        if (code >= 200 && code < 300) success = 1;
    }
    
    pclose(fp);
    remove(template);
    return success;
}

void move_up(int lines) {
    printf("\033[%dA\r", lines);
}
void clear_screen() {
    system("clear");
}
#endif

void parse_and_save_json_keys(const char* json_str) {
    const char *ptr = json_str;
    int count = 0;
    while (*ptr) {
        const char *quote1 = strchr(ptr, '"');
        if (!quote1) break;
        const char *quote2 = strchr(quote1 + 1, '"');
        if (!quote2) break;
        const char *colon = strchr(quote2 + 1, ':');
        if (!colon) { ptr = quote2 + 1; continue; }
        
        const char *qcheck = strchr(quote2 + 1, '"');
        if (qcheck && qcheck < colon) { ptr = quote2 + 1; continue; }

        const char *quote3 = strchr(colon + 1, '"');
        if (!quote3) break;
        const char *quote4 = strchr(quote3 + 1, '"');
        if (!quote4) break;
        
        int nick_len = (int)(quote2 - quote1 - 1);
        int key_len = (int)(quote4 - quote3 - 1);
        
        char nickname[64];
        char key[128];
        
        if (nick_len >= 64) nick_len = 63;
        if (key_len >= 128) key_len = 127;
        
        strncpy(nickname, quote1 + 1, nick_len);
        nickname[nick_len] = '\0';
        
        strncpy(key, quote3 + 1, key_len);
        key[key_len] = '\0';
        
        if (save_entry(nickname, key)) {
            printf("Merged key for '%s'\n", nickname);
            count++;
        }
        
        ptr = quote4 + 1;
    }
    if (count == 0) {
        printf("No keys found in the downloaded JSON.\n");
    } else {
        printf("Successfully merged %d key(s).\n", count);
    }
}

int main(int argc, char *argv[]) {
    setvbuf(stdout, NULL, _IONBF, 0);

    if (argc > 1) {
        if (argc >= 2 && strcmp(argv[1], "--url") == 0) {
            char api_url[512];
            if (argc == 3 && (strncmp(argv[2], "http://", 7) == 0 || strncmp(argv[2], "https://", 8) == 0)) {
                strncpy(api_url, argv[2], 511);
                api_url[511] = '\0';
                save_cloud_api(api_url);
            } else if (!load_cloud_api(api_url, sizeof(api_url))) {
                printf("No Cloud API configured.\n");
                printf("Please provide your API URL: ");
                if (fgets(api_url, sizeof(api_url), stdin)) {
                    size_t len = strlen(api_url);
                    if (len > 0 && api_url[len-1] == '\n') api_url[len-1] = '\0';
                    if (len > 1 && api_url[len-2] == '\r') api_url[len-2] = '\0';
                    save_cloud_api(api_url);
                } else {
                    return 1;
                }
            }

            char *downloaded_json = (char*)malloc(8192);
            if (!downloaded_json) {
                printf("Memory allocation failed.\n");
                return 1;
            }
            printf("Downloading JSON from %s...\n", api_url);
            if (download_key(api_url, downloaded_json, 8192)) {
                parse_and_save_json_keys(downloaded_json);
            } else {
                printf("Failed to download from URL.\n");
            }
            free(downloaded_json);
            return 0;
        } else if (argc == 2 && strcmp(argv[1], "--refresh") == 0) {
            char api_url[512];
            if (!load_cloud_api(api_url, sizeof(api_url))) {
                printf("No API configured. Fetch a key first using: auth.exe --url <link>\n");
                return 1;
            }
            printf("Refreshing keys from %s...\n", api_url);
            char *downloaded_json = (char*)malloc(8192);
            if (downloaded_json && download_key(api_url, downloaded_json, 8192)) {
                parse_and_save_json_keys(downloaded_json);
            } else {
                printf("Failed to fetch from %s\n", api_url);
            }
            if (downloaded_json) free(downloaded_json);
            return 0;
        } else if (argc == 4 && strcmp(argv[1], "--add") == 0) {
            if (save_entry(argv[2], argv[3])) {
                printf("Saved target key!\n");
            }
            return 0;
        } else if (argc == 3 && strcmp(argv[1], "--delete") == 0) {
            if (delete_entry(argv[2])) {
                printf("Successfully deleted key for '%s'\n", argv[2]);
            } else {
                printf("Key for '%s' not found.\n", argv[2]);
            }
            return 0;
        } else if (argc >= 2 && strcmp(argv[1], "--upload") == 0) {
            char api_url[512];
            if (argc == 3) {
                strncpy(api_url, argv[2], 511);
                api_url[511] = '\0';
                save_cloud_api(api_url);
            } else if (!load_cloud_api(api_url, sizeof(api_url))) {
                printf("No Cloud API configured.\n");
                printf("Please provide your API URL: ");
                if (fgets(api_url, sizeof(api_url), stdin)) {
                    size_t len = strlen(api_url);
                    if (len > 0 && api_url[len-1] == '\n') api_url[len-1] = '\0';
                    if (len > 1 && api_url[len-2] == '\r') api_url[len-2] = '\0';
                    save_cloud_api(api_url);
                } else {
                    return 1;
                }
            }
            
            char *json_payload = read_all_db();
            if (!json_payload) {
                printf("Database is empty or cannot be read.\n");
                return 1;
            }
            
            printf("Uploading keys to %s...\n", api_url);
            if (upload_json_to_api(api_url, json_payload)) {
                printf("Successfully uploaded database to cloud!\n");
            } else {
                printf("Failed to upload to the Cloud API.\n");
            }
            free(json_payload);
            return 0;
        } else if (argc == 2 && strcmp(argv[1], "--help") == 0) {
            // help printed below
        } else {
            printf("Invalid parameter passed.\n\n");
        }

        printf("Usage:\n");
        printf("  auth.exe --url [link]       : Download keys natively from universal API.\n");
        printf("  auth.exe --refresh          : Refresh and download keys from universal API.\n");
        printf("  auth.exe --upload [url]     : Upload all local keys to your cloud API.\n");
        printf("  auth.exe --add <nick> <key> : Add a specific TOTP key manually.\n");
        printf("  auth.exe --delete <nick>    : Delete a TOTP key by its nickname.\n");
        printf("  auth.exe --help             : Show this help message.\n");
        printf("  auth.exe                    : Launch the TOTP authenticator.\n");
        return 1;
    }

    AuthEntry entries[100];
    int count = load_entries(entries, 100);

    if (count == 0) {
        printf("No keys found. Add keys from a URL with: auth.exe --url <link>\n");
        printf("Or add manually: auth.exe --add <nickname> <key>\n");
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

        printf("===== Refreshing in %02ld sec (Press Ctrl+C to close) =====\n", remaining);
        
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