#include "base32.h"
#include <string.h>
#include <ctype.h>

int val(char c){
    if(c>='A' && c<='Z') return c-'A';
    if(c>='2' && c<='7') return c-'2'+26;
    return -1;
}

int base32_decode(const char *in, unsigned char *out){
    int buffer = 0, bitsLeft = 0, count = 0;

    for(int i=0; in[i]; i++){
        char c = toupper(in[i]);
        if(c=='=' || c==' ') continue;

        int v = val(c);
        if(v < 0) return -1;

        buffer <<= 5;
        buffer |= v & 31;
        bitsLeft += 5;

        if(bitsLeft >= 8){
            out[count++] = (buffer >> (bitsLeft - 8)) & 0xFF;
            bitsLeft -= 8;
        }
    }

    return count;
}