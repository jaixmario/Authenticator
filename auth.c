#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#define sleep(x) Sleep(1000 * (x))
#else
#include <unistd.h>
#endif

#define ROL(x,n) ((x << n) | (x >> (32 - n)))

void sha1(const uint8_t *msg, size_t len, uint8_t *out) {
    uint32_t h0=0x67452301,h1=0xEFCDAB89,h2=0x98BADCFE;
    uint32_t h3=0x10325476,h4=0xC3D2E1F0;

    uint64_t bits=len*8;
    size_t new_len=len+1;
    while(new_len%64!=56) new_len++;

    uint8_t *data=calloc(new_len+8,1);
    memcpy(data,msg,len);
    data[len]=0x80;

    for(int i=0;i<8;i++)
        data[new_len+i]=(bits>>(56-8*i))&0xFF;

    for(size_t off=0;off<new_len+8;off+=64){
        uint32_t w[80];

        for(int i=0;i<16;i++)
            w[i]=(data[off+i*4]<<24)|(data[off+i*4+1]<<16)|
                 (data[off+i*4+2]<<8)|data[off+i*4+3];

        for(int i=16;i<80;i++)
            w[i]=ROL(w[i-3]^w[i-8]^w[i-14]^w[i-16],1);

        uint32_t a=h0,b=h1,c=h2,d=h3,e=h4;

        for(int i=0;i<80;i++){
            uint32_t f,k;

            if(i<20){f=(b&c)|((~b)&d);k=0x5A827999;}
            else if(i<40){f=b^c^d;k=0x6ED9EBA1;}
            else if(i<60){f=(b&c)|(b&d)|(c&d);k=0x8F1BBCDC;}
            else{f=b^c^d;k=0xCA62C1D6;}

            uint32_t t=ROL(a,5)+f+e+k+w[i];
            e=d; d=c; c=ROL(b,30); b=a; a=t;
        }

        h0+=a; h1+=b; h2+=c; h3+=d; h4+=e;
    }

    free(data);

    out[0]=h0>>24; out[1]=h0>>16; out[2]=h0>>8; out[3]=h0;
    out[4]=h1>>24; out[5]=h1>>16; out[6]=h1>>8; out[7]=h1;
    out[8]=h2>>24; out[9]=h2>>16; out[10]=h2>>8; out[11]=h2;
    out[12]=h3>>24; out[13]=h3>>16; out[14]=h3>>8; out[15]=h3;
    out[16]=h4>>24; out[17]=h4>>16; out[18]=h4>>8; out[19]=h4;
}

void hmac_sha1(const uint8_t *key,int klen,
               const uint8_t *data,int dlen,
               uint8_t *out){

    uint8_t ipad[64]={0},opad[64]={0},tmp[20];

    memcpy(ipad,key,klen);
    memcpy(opad,key,klen);

    for(int i=0;i<64;i++){
        ipad[i]^=0x36;
        opad[i]^=0x5c;
    }

    uint8_t inner[64+dlen];
    memcpy(inner,ipad,64);
    memcpy(inner+64,data,dlen);
    sha1(inner,64+dlen,tmp);

    uint8_t outer[64+20];
    memcpy(outer,opad,64);
    memcpy(outer+64,tmp,20);
    sha1(outer,64+20,out);
}

void to_bytes(uint64_t v,uint8_t *b){
    for(int i=7;i>=0;i--){ b[i]=v&0xFF; v>>=8; }
}

int totp(uint8_t *key,int len){
    uint64_t t=time(NULL)/30;

    uint8_t c[8],h[20];
    to_bytes(t,c);

    hmac_sha1(key,len,c,8,h);

    int o=h[19]&0x0F;
    int bin=((h[o]&0x7F)<<24)|((h[o+1]&0xFF)<<16)|
            ((h[o+2]&0xFF)<<8)|(h[o+3]&0xFF);

    return bin%1000000;
}

int main(){
    char input[128];

    printf("Enter secret key: ");
    fgets(input,sizeof(input),stdin);

    input[strcspn(input,"\n")] = 0;

    uint8_t *key = (uint8_t*)input;

    while(1){
        int code = totp(key,strlen((char*)key));
        printf("%06d  (%ld sec)\n",code,30-(time(NULL)%30));
        sleep(1);
    }

    return 0;
}