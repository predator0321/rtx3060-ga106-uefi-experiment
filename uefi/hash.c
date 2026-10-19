/* SPDX-License-Identifier: GPL-2.0-only */
/* SHA-256 (FIPS 180-4) for pinned asset integrity, not image signing. */
#include <stdint.h>
#include <stddef.h>
static const uint32_t k[64]={
0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
static uint32_t ror(uint32_t v,unsigned int n){return (v>>n)|(v<<(32-n));}
static void block(uint32_t h[8],const uint8_t p[64])
{
    uint32_t w[64];for(unsigned int i=0;i<16;i++)w[i]=((uint32_t)p[4*i]<<24)|((uint32_t)p[4*i+1]<<16)|((uint32_t)p[4*i+2]<<8)|p[4*i+3];
    for(unsigned int i=16;i<64;i++){uint32_t a=w[i-15],b=w[i-2];w[i]=w[i-16]+(ror(a,7)^ror(a,18)^(a>>3))+w[i-7]+(ror(b,17)^ror(b,19)^(b>>10));}
    uint32_t a=h[0],b=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],z=h[7];
    for(unsigned int i=0;i<64;i++){
        uint32_t t=z+(ror(e,6)^ror(e,11)^ror(e,25))+((e&f)^((~e)&g))+k[i]+w[i];
        uint32_t u=(ror(a,2)^ror(a,13)^ror(a,22))+((a&b)^(a&c)^(b&c));
        z=g;g=f;f=e;e=d+t;d=c;c=b;b=a;a=t+u;
    }
    h[0]+=a;h[1]+=b;h[2]+=c;h[3]+=d;h[4]+=e;h[5]+=f;h[6]+=g;h[7]+=z;
}
void gu_sha256(const void *data,size_t n,uint8_t out[32])
{
    const uint8_t *p=data;uint64_t bits=(uint64_t)n*8;
    uint32_t h[8]={0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    while(n>=64){block(h,p);p+=64;n-=64;}
    uint8_t tail[128]={0};for(size_t i=0;i<n;i++)tail[i]=p[i];tail[n]=0x80;
    size_t total=n<56?64:128;
    for(unsigned int i=0;i<8;i++)tail[total-1-i]=(uint8_t)(bits>>(i*8));
    block(h,tail);if(total==128)block(h,tail+64);
    for(unsigned int i=0;i<32;i++)out[i]=(uint8_t)(h[i/4]>>(24-(i%4)*8));
}
static int same(const char *a,const char *b)
{ while(*a && *a==*b){a++;b++;}return *a==*b; }
int gu_verify_asset(const char *name,const void *data,size_t n)
{
    static const struct {const char *name,*hash;} pins[]={
        {"booter_load_ga102.bin","056cbc064a888f80717726c54d7f64fcd68044d123870c95a74f37f4975dcaa4"},
        {"gsp_boot_ga102.bin","3df651acb174fd5f5561a1996965b192f50c5d763168446ed0d4ead19d895768"},
        {"gsp_boot_desc_ga102.bin","cc5a3fc1f4c47f2fe107b1b68f0176d8a448e6dec84b575e5560ccef5262bf15"},
        {"gsp_ga10x.bin","572373620a37418f24dc16b5031c39338778c3257e48e8408de9a57291b24f3a"},
        {"v67-ga102-610.43.03.bin","2a0aa1f86d0b6afff5214d641d87d46dc98ef06ed908ff05d0ddbad5ffb7e197"}};
    for(unsigned int i=0;i<sizeof(pins)/sizeof(pins[0]);i++)if(same(name,pins[i].name)){
        uint8_t sum[32];gu_sha256(data,n,sum);
        for(unsigned int j=0;j<32;j++)if("0123456789abcdef"[sum[j]>>4]!=pins[i].hash[2*j] || "0123456789abcdef"[sum[j]&15]!=pins[i].hash[2*j+1])return 0;
        return 1;
    }
    return 0;
}
