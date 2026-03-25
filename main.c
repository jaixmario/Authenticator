#include <stdio.h>
#include <string.h>
#include <time.h>
#include "totp.h"
#include "storage.h"

#ifdef _WIN32
#include <windows.h>
#define sleep(x) Sleep(1000 * (x))
#else
#include <unistd.h>
#endif

int main(){
    char key[128];

    if(!load_key(key)){
        printf("Enter secret key: ");
        fgets(key,sizeof(key),stdin);
        key[strcspn(key,"\n")] = 0;
        save_key(key);
    } else {
        printf("Key loaded\n");
    }

    while(1){
        int code = totp((uint8_t*)key,strlen(key));
        printf("%06d (%ld sec)\n",code,30-(time(NULL)%30));
        sleep(1);
    }

    return 0;
}