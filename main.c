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

int main(){
    char key[128];
    unsigned char decoded[64];

    if(!load_key(key)){
        printf("Enter Base32 key: ");
        fgets(key,sizeof(key),stdin);
        key[strcspn(key,"\n")] = 0;
        save_key(key);
    } else {
        printf("Key loaded\n");
    }

    int key_len = base32_decode(key, decoded);
    if(key_len <= 0){
        printf("Invalid key\n");
        return 1;
    }

    while(1){
        int code = totp(decoded,key_len);
        printf("%06d (%ld sec)\n",code,30-(time(NULL)%30));
        sleep(1);
    }

    return 0;
}