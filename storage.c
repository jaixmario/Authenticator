#include <stdio.h>
#include <string.h>

#define FILE_NAME ".authkey"

void save_key(char *key){
    FILE *f=fopen(FILE_NAME,"w");
    if(f){
        fprintf(f,"%s",key);
        fclose(f);
    }
}

int load_key(char *key){
    FILE *f=fopen(FILE_NAME,"r");
    if(!f) return 0;

    fgets(key,128,f);
    fclose(f);
    key[strcspn(key,"\n")] = 0;
    return 1;
}