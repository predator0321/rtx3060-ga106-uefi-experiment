/* SPDX-License-Identifier: GPL-2.0-only */
#include "policy.h"
static int hex(char c)
{ if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;if(c>='A'&&c<='F')return c-'A'+10;return -1; }
static int equal(const char *a,size_t n,const char *b)
{ size_t i=0;while(i<n && b[i] && a[i]==b[i])i++;return i==n && !b[i]; }
static int number(const char *s,size_t n,unsigned int *out)
{
    if(n>2 && s[0]=='0' && s[1]=='x'){s+=2;n-=2;}
    if(!n || n>4)return -1;
    unsigned int v=0;for(size_t i=0;i<n;i++){int d=hex(s[i]);if(d<0)return -1;v=v*16+(unsigned int)d;}
    *out=v;return 0;
}
static int target(const char *s,size_t n,struct gu_options *o)
{
    if(n!=12 || s[4]!=':' || s[7]!=':' || s[10]!='.')return -1;
    if(number(s,4,&o->segment)||number(s+5,2,&o->bus)||number(s+8,2,&o->device)||number(s+11,1,&o->function))return -1;
    if(o->device>31 || o->function>7)return -1;
    o->has_target=true;return 0;
}
static int partition_guid(const char *s,size_t n,uint8_t out[16])
{
    /* Text GUID -> GPT/EFI_GUID byte order, not network UUID byte order. */
    static const unsigned int offsets[16]={6,4,2,0,11,9,16,14,19,21,24,26,28,30,32,34};
    if(n!=36 || s[8]!='-' || s[13]!='-' || s[18]!='-' || s[23]!='-')return -1;
    unsigned int nonzero=0;
    for(unsigned int i=0;i<16;i++) {
        int a=hex(s[offsets[i]]),b=hex(s[offsets[i]+1]);
        if(a<0 || b<0)return -1;
        out[i]=(uint8_t)((a<<4)|b);nonzero|=out[i];
    }
    return nonzero?0:-1;
}
int gu_parse_options(const char *s,size_t n,struct gu_options *out)
{
    if(!s || !out || n>4096)return -1;
    struct gu_options o={.windows=true};unsigned int seen=0;
    for(size_t pos=0;pos<n;) {
        size_t start=pos;while(pos<n && s[pos]!='\n'){if(!s[pos])return -1;pos++;}
        size_t end=pos;if(pos<n)pos++;
        while(start<end && (s[start]==' '||s[start]=='\t'))start++;
        while(end>start && (s[end-1]=='\r'||s[end-1]==' '||s[end-1]=='\t'))end--;
        if(start==end || s[start]=='#')continue;
        size_t eq=start;while(eq<end && s[eq]!='=')eq++;
        if(eq==end)return -1;
        const char *v=s+eq+1;size_t count=end-eq-1;unsigned int bit=0;
        if(equal(s+start,eq-start,"target")){bit=1;if(target(v,count,&o))return -1;}
        else if(equal(s+start,eq-start,"board_vendor")){bit=2;if(number(v,count,&o.board_vendor))return -1;}
        else if(equal(s+start,eq-start,"board_device")){bit=4;if(number(v,count,&o.board_device))return -1;}
        else if(equal(s+start,eq-start,"windows_partition")){
            bit=64;if(partition_guid(v,count,o.windows_partition))return -1;
            o.has_windows_partition=true;
        }
        else {
            if(count!=1 || (*v!='0' && *v!='1'))return -1;
            if(equal(s+start,eq-start,"execute")){bit=8;o.execute=*v=='1';}
            else if(equal(s+start,eq-start,"ss1_full")){bit=16;o.ss1_full=*v=='1';}
            else if(equal(s+start,eq-start,"windows")){bit=32;o.windows=*v=='1';}
            else return -1;
        }
        if(seen&bit)return -1;seen|=bit;
    }
    if(o.execute && ((seen&7)!=7 || !o.board_vendor || o.board_vendor==0xffff || !o.board_device || o.board_device==0xffff))return -1;
    *out=o;return 0;
}
int gu_capability_offset(const uint8_t config[256],unsigned int id)
{
    if(!(config[6]&0x10))return 0;
    unsigned int at=config[0x34];bool seen[256]={0};
    for(unsigned int i=0;i<48 && at;i++) {
        if(at<0x40 || at>0xfc || (at&3) || seen[at])return -1;
        seen[at]=true;if(config[at]==id)return (int)at;at=config[at+1];
    }
    return at?-1:0;
}
uint64_t gu_bar_resource_length(const uint8_t *p,size_t n)
{
    /* UEFI GetBarAttributes returns ACPI QWORD address-space descriptors.
     * Header(3), resource type(1), flags(2), gran/min/max/translation/length. */
    if(!p || n<46 || p[0]!=0x8a || p[1]!=0x2b || p[2] || p[3]!=0)return 0;
    uint64_t size=0;for(unsigned int i=0;i<8;i++)size|=(uint64_t)p[38+i]<<(i*8);
    return size;
}
int gu_partition_matches(const uint8_t *p,size_t n,const uint8_t guid[16])
{
    if(!p || !guid || n<4 || n>4096)return -1;
    bool found=false,match=false;
    for(size_t at=0;at+4<=n;) {
        size_t len=(size_t)p[at+2]|((size_t)p[at+3]<<8);
        if(len<4 || len>n-at)return -1;
        if(p[at]==0x7f) {
            /* No ambiguous multi-instance paths or trailing data. */
            if(p[at+1]!=0xff || len!=4 || at+4!=n)return -1;
            return match?1:0;
        }
        if(p[at]==4 && p[at+1]==1) {
            if(len!=42 || found)return -1;
            found=true;
            if(p[at+40]==2 && p[at+41]==2) {
                match=true;
                for(unsigned int i=0;i<16;i++)if(p[at+24+i]!=guid[i])match=false;
            }
        }
        at+=len;
    }
    return -1;
}
