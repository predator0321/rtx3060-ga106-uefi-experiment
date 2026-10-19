/* SPDX-License-Identifier: GPL-2.0-only */
#include "env.h"
#include <stdarg.h>

struct gu_env gu;
struct dma_record { void *cpu, *mapping; UINTN pages; u64 address; };
static struct dma_record dma_records[32768];
unsigned int gu_live_dma_count(void)
{ unsigned int count=0;for(unsigned int i=0;i<ARRAY_SIZE(dma_records);i++)if(dma_records[i].cpu)count++;return count; }

void *gu_memcpy(void *d, const void *s, size_t n)
{ u8 *a=d; const u8 *b=s; while(n--) *a++=*b++; return d; }
void *gu_memset(void *d, int c, size_t n)
{ u8 *a=d; while(n--) *a++=(u8)c; return d; }
int gu_memcmp(const void *a, const void *b, size_t n)
{ const u8 *x=a,*y=b; while(n--) { if(*x!=*y) return *x-*y; x++; y++; } return 0; }
size_t gu_strlen(const char *s) { size_t n=0; while(s[n])n++;return n; }
int gu_strcmp(const char *a,const char *b)
{ while(*a && *a==*b){a++;b++;}return (u8)*a-(u8)*b; }
const char *gu_strrchr(const char *s,int c)
{ const char *p=NULL; do{if(*s==c)p=s;}while(*s++);return p; }
void *gu_memchr(const void *s,int c,size_t n)
{ const u8 *p=s;while(n--){if(*p==(u8)c)return (void *)p;p++;}return NULL; }
void *gu_alloc(size_t n) { return n ? AllocatePool(n) : NULL; }
void *gu_calloc(size_t n,size_t s)
{ if(s && n>~(size_t)0/s)return NULL; size_t bytes=n*s;void *p=gu_alloc(bytes);if(p)gu_memset(p,0,bytes);return p; }
void gu_free(void *p) { if(p)FreePool(p); }
void *kzalloc(size_t n,int flags) { (void)flags;return gu_calloc(1,n); }

static void log_char(char c)
{ if(gu.log_length+1<GU_LOG_CAP){gu.log[gu.log_length++]=c;gu.log[gu.log_length]=0;} }
static void log_string(const char *s) { if(!s)s="(null)";while(*s)log_char(*s++); }
void gu_log(const char *fmt,...)
{
    va_list ap; va_start(ap,fmt);
    while(*fmt) {
        if(*fmt!='%'){log_char(*fmt++);continue;}
        fmt++;
        if(*fmt=='%'){log_char(*fmt++);continue;}
        bool prefix=false,zero=false;unsigned int width=0,length=0;
        if(*fmt=='#'){prefix=true;fmt++;}
        if(*fmt=='0'){zero=true;fmt++;}
        while(*fmt>='0' && *fmt<='9'){width=width*10+*fmt++-'0';}
        while(*fmt=='l'){length++;fmt++;}
        if(*fmt=='z'){length=2;fmt++;}
        char kind=*fmt ? *fmt++ : 0;
        if(kind=='s'){log_string(va_arg(ap,const char *));continue;}
        if(kind!='u' && kind!='x' && kind!='d'){log_string("[format]");continue;}
        u64 value;
        if(kind=='d') {
            long long v=length>=2?va_arg(ap,long long):(length?va_arg(ap,long):va_arg(ap,int));
            if(v<0){log_char('-');value=(u64)(-(v+1))+1;}else value=(u64)v;
        } else value=length>=2?va_arg(ap,unsigned long long):(length?va_arg(ap,unsigned long):va_arg(ap,unsigned int));
        unsigned int base=kind=='x'?16:10,n=0;char digits[32];
        do{digits[n++]="0123456789abcdef"[value%base];value/=base;}while(value);
        if(prefix && kind=='x'){log_string("0x");}
        while(width>n){log_char(zero?'0':' ');width--;}
        while(n)log_char(digits[--n]);
    }
    va_end(ap);
}

static u64 tsc(void)
{ u32 lo,hi;__asm__ __volatile__("lfence; rdtsc":"=a"(lo),"=d"(hi)::"memory");return ((u64)hi<<32)|lo; }
int gu_init_clock(void)
{
    u32 a,b,c,d;
    __asm__ __volatile__("cpuid":"=a"(a),"=b"(b),"=c"(c),"=d"(d):"a"(0x80000000u),"c"(0));
    if(a<0x80000007u)return -EOPNOTSUPP;
    __asm__ __volatile__("cpuid":"=a"(a),"=b"(b),"=c"(c),"=d"(d):"a"(0x80000007u),"c"(0));
    if(!(d&(1u<<8)))return -EOPNOTSUPP;
    u64 before=tsc();uefi_call_wrapper(BS->Stall,1,10000);
    gu.ticks_per_us=(tsc()-before)/10000;
    return gu.ticks_per_us?0:-EIO;
}
ktime_t ktime_get(void) { return tsc()/gu.ticks_per_us; }
void usleep_range(unsigned long a,unsigned long b)
{ (void)b;uefi_call_wrapper(BS->Stall,1,a); }

int pci_read_config_word(struct pci_dev *p,int off,u16 *v)
{ (void)p;return EFI_ERROR(uefi_call_wrapper(gu.pci->Pci.Read,5,gu.pci,EfiPciIoWidthUint16,off,1,v))?-EIO:0; }
int pci_read_config_dword(struct pci_dev *p,int off,u32 *v)
{ (void)p;return EFI_ERROR(uefi_call_wrapper(gu.pci->Pci.Read,5,gu.pci,EfiPciIoWidthUint32,off,1,v))?-EIO:0; }
int pci_write_config_word(struct pci_dev *p,int off,u16 v)
{ (void)p;return EFI_ERROR(uefi_call_wrapper(gu.pci->Pci.Write,5,gu.pci,EfiPciIoWidthUint16,off,1,&v))?-EIO:0; }
int pcie_capability_read_word(struct pci_dev *p,int off,u16 *v)
{ return gu.pcie_cap ? pci_read_config_word(p,gu.pcie_cap+off,v):-ENODEV; }
int pcie_capability_read_dword(struct pci_dev *p,int off,u32 *v)
{ return gu.pcie_cap ? pci_read_config_dword(p,gu.pcie_cap+off,v):-ENODEV; }
int pci_is_pcie(struct pci_dev *p) { (void)p;return gu.pcie_cap!=0; }
int pci_select_bars(struct pci_dev *p,int flags) { (void)p;(void)flags;return 1; }
int pci_request_region(struct pci_dev *p,int bar,const char *name)
{ (void)p;(void)name;return bar?-EINVAL:0; }
void pci_release_region(struct pci_dev *p,int bar) { (void)p;(void)bar; }
int pci_enable_device_mem(struct pci_dev *p)
{ u16 v;return pci_read_config_word(p,PCI_COMMAND,&v)?-EIO:pci_write_config_word(p,PCI_COMMAND,v|2); }
void pci_disable_device(struct pci_dev *p) { (void)p; }
resource_size_t pci_resource_len(struct pci_dev *p,int bar)
{ (void)p;return bar?0:gu.bar_length; }
void *pci_iomap(struct pci_dev *p,int bar,unsigned long n)
{ (void)p;(void)n;return bar?NULL:GU_MMIO_COOKIE; }
void pci_iounmap(struct pci_dev *p,void *ptr) { (void)p;(void)ptr; }
u32 readl(const void *ptr)
{
    UINT64 off=(UINT64)ptr-(UINT64)GU_MMIO_COOKIE;u32 value=~0u;
    if(gu.io_fault || off>gu.bar_length || gu.bar_length-off<4)return value;
    if(EFI_ERROR(uefi_call_wrapper(gu.pci->Mem.Read,6,gu.pci,EfiPciIoWidthUint32,0,off,1,&value)))gu.io_fault=true;
    return value;
}
void writel(u32 value,void *ptr)
{
    UINT64 off=(UINT64)ptr-(UINT64)GU_MMIO_COOKIE;
    if(gu.io_fault || off>gu.bar_length || gu.bar_length-off<4){gu.io_fault=true;return;}
    if(EFI_ERROR(uefi_call_wrapper(gu.pci->Mem.Write,6,gu.pci,EfiPciIoWidthUint32,0,off,1,&value)))gu.io_fault=true;
}
void gu_memcpy_fromio(void *dst,const void *src,size_t n)
{
    UINT64 off=(UINT64)src-(UINT64)GU_MMIO_COOKIE;
    if(gu.io_fault || off>gu.bar_length || n>gu.bar_length-off ||
       EFI_ERROR(uefi_call_wrapper(gu.pci->Mem.Read,6,gu.pci,EfiPciIoWidthUint8,0,off,n,dst))) {
        gu.io_fault=true;gu_memset(dst,0xff,n);
    }
}
int pci_save_state(struct pci_dev *p)
{
    if(pci_read_config_word(p,4,&gu.saved_command))return -EIO;
    for(int i=0;i<6;i++)if(pci_read_config_dword(p,16+4*i,&gu.saved_bars[i]))return -EIO;
    return 0;
}
void pci_restore_state(struct pci_dev *p)
{
    /* FLR must not relocate BARs. Verify actual values in the common core. */
    for(int i=0;i<6;i++)
        if(EFI_ERROR(uefi_call_wrapper(gu.pci->Pci.Write,5,gu.pci,EfiPciIoWidthUint32,16+4*i,1,&gu.saved_bars[i])))gu.io_fault=true;
    if(pci_write_config_word(p,4,gu.saved_command&~4u))gu.io_fault=true;
}
void pci_set_master(struct pci_dev *p)
{ u16 v;if(!pci_read_config_word(p,4,&v))pci_write_config_word(p,4,v|4); }
void pci_clear_master(struct pci_dev *p)
{ u16 v;if(!pci_read_config_word(p,4,&v))pci_write_config_word(p,4,v&~4u); }
int pcie_flr(struct pci_dev *p)
{
    u16 ctrl,pending;u32 identity;
    if(!gu.pcie_cap || pcie_capability_read_word(p,8,&ctrl))return -EIO;
    for(int i=0;i<1000;i++) {
        if(pcie_capability_read_word(p,10,&pending))return -EIO;
        if(!(pending&PCI_EXP_DEVSTA_TRPND))break;
        if(i==999)return -ETIMEDOUT;
        usleep_range(1000,1000);
    }
    if(pci_write_config_word(p,gu.pcie_cap+8,ctrl|(1u<<15)))return -EIO;
    usleep_range(100000,100000);
    for(int i=0;i<1000;i++) {
        if(!pci_read_config_dword(p,0,&identity) && identity==0x250410de)return 0;
        usleep_range(1000,1000);
    }
    return -ETIMEDOUT;
}
int aperture_remove_conflicting_pci_devices(struct pci_dev *p,const char *s)
{ (void)p;(void)s;return 0; } /* No Linux framebuffer exists in EFI. Do not call GOP after FLR. */
struct pci_dev *pci_dev_get(struct pci_dev *p) { p->refs++;return p; }
void pci_dev_put(struct pci_dev *p) { p->refs--; }

void *dma_alloc_coherent(struct device *d,size_t n,dma_addr_t *addr,int flags)
{
    (void)flags;unsigned int slot;
    for(slot=0;slot<ARRAY_SIZE(dma_records);slot++)if(!dma_records[slot].cpu)break;
    if(slot==ARRAY_SIZE(dma_records) || !n || n>~(UINTN)0-4095)return NULL;
    UINTN pages=(n+4095)/4096,bytes=pages*4096;void *host=NULL,*mapping=NULL;
    EFI_PHYSICAL_ADDRESS device_address=0;
    if(EFI_ERROR(uefi_call_wrapper(gu.pci->AllocateBuffer,6,gu.pci,AllocateAnyPages,EfiBootServicesData,pages,&host,0)))return NULL;
    EFI_STATUS st=uefi_call_wrapper(gu.pci->Map,6,gu.pci,EfiPciIoOperationBusMasterCommonBuffer,host,&bytes,&device_address,&mapping);
    if(EFI_ERROR(st) || bytes<pages*4096 || device_address>d->coherent_dma_mask ||
       pages*4096-1>d->coherent_dma_mask-device_address) {
        if(!EFI_ERROR(st))uefi_call_wrapper(gu.pci->Unmap,2,gu.pci,mapping);
        uefi_call_wrapper(gu.pci->FreeBuffer,3,gu.pci,pages,host);return NULL;
    }
    gu_memset(host,0,pages*4096);
    dma_records[slot]=(struct dma_record){host,mapping,pages,device_address};
    *addr=device_address;return host;
}
void dma_free_coherent(struct device *d,size_t n,void *cpu,dma_addr_t addr)
{
    (void)d;(void)n;
    for(unsigned int i=0;i<ARRAY_SIZE(dma_records);i++)if(dma_records[i].cpu==cpu) {
        struct dma_record *r=&dma_records[i];
        if(r->address!=addr){gu.retained=true;return;}
        if(EFI_ERROR(uefi_call_wrapper(gu.pci->Unmap,2,gu.pci,r->mapping))) {gu.retained=true;return;}
        if(EFI_ERROR(uefi_call_wrapper(gu.pci->FreeBuffer,3,gu.pci,r->pages,cpu))) {gu.retained=true;return;}
        gu_memset(r,0,sizeof(*r));return;
    }
    gu.retained=true;
}
int dma_set_mask(struct device *d,u64 m) { *d->dma_mask=m;return 0; }
int dma_set_coherent_mask(struct device *d,u64 m) { d->coherent_dma_mask=m;return 0; }
void dma_wmb(void)
{ __asm__ __volatile__("mfence":::"memory");if(EFI_ERROR(uefi_call_wrapper(gu.pci->Flush,1,gu.pci)))gu.io_fault=true; }
void *devres_open_group(struct device *d,void *id,int flags)
{ (void)id;(void)flags;return d; }
int devres_release_group(struct device *d,void *id) { (void)d;(void)id;return 0; }
void __module_get(void *unused) { (void)unused;gu.retained=true; }

EFI_STATUS gu_read_file(const CHAR16 *name,void **data,UINTN *length,UINTN limit)
{
    EFI_FILE_HANDLE file=NULL;EFI_STATUS st;EFI_GUID info_id=EFI_FILE_INFO_ID;
    *data=NULL;*length=0;
    st=uefi_call_wrapper(gu.root->Open,5,gu.root,&file,(CHAR16 *)name,EFI_FILE_MODE_READ,0);
    if(EFI_ERROR(st))return st;
    UINTN info_size=sizeof(EFI_FILE_INFO)+512;EFI_FILE_INFO *info=AllocatePool(info_size);
    if(!info){st=EFI_OUT_OF_RESOURCES;goto done;}
    st=uefi_call_wrapper(file->GetInfo,4,file,&info_id,&info_size,info);
    if(EFI_ERROR(st)||!info->FileSize||info->FileSize>limit){if(!EFI_ERROR(st))st=EFI_BAD_BUFFER_SIZE;FreePool(info);goto done;}
    *length=(UINTN)info->FileSize;FreePool(info);*data=AllocatePool(*length);
    if(!*data){st=EFI_OUT_OF_RESOURCES;goto done;}
    UINTN count=*length;
    st=uefi_call_wrapper(file->Read,3,file,&count,*data);
    if(!EFI_ERROR(st) && count!=*length)st=EFI_DEVICE_ERROR;
    if(EFI_ERROR(st)){FreePool(*data);*data=NULL;*length=0;}
done:
    uefi_call_wrapper(file->Close,1,file);return st;
}
int request_firmware(const struct firmware **out,const char *name,struct device *d)
{
    (void)d;const char *base=gu_strrchr(name,'/');base=base?base+1:name;
    CHAR16 path[160];const CHAR16 *prefix=L"\\ga106\\firmware\\";UINTN i=0;
    while(prefix[i]){path[i]=prefix[i];i++;}
    for(UINTN j=0;base[j] && i+1<ARRAY_SIZE(path);j++)path[i++]=(CHAR16)(u8)base[j];path[i]=0;
    void *data;UINTN n;EFI_STATUS st=gu_read_file(path,&data,&n,128u*1024u*1024u);
    if(EFI_ERROR(st))return -ENOENT;
    if(!gu_verify_asset(base,data,n)){FreePool(data);return -EBADMSG;}
    struct firmware *f=AllocatePool(sizeof(*f));if(!f){FreePool(data);return -ENOMEM;}
    f->size=n;f->data=data;*out=f;return 0;
}
void release_firmware(const struct firmware *f)
{ if(f){gu_free((void *)f->data);gu_free((void *)f);} }
EFI_STATUS gu_write_log(void)
{
    EFI_FILE_HANDLE f=NULL;
    EFI_STATUS st=uefi_call_wrapper(gu.root->Open,5,gu.root,&f,L"\\ga106-log.txt",EFI_FILE_MODE_CREATE|EFI_FILE_MODE_WRITE|EFI_FILE_MODE_READ,0);
    if(EFI_ERROR(st))return st;
    /* Truncate by deleting any prior file, then recreate. */
    st=uefi_call_wrapper(f->Delete,1,f);f=NULL;
    if(EFI_ERROR(st))return st;
    st=uefi_call_wrapper(gu.root->Open,5,gu.root,&f,L"\\ga106-log.txt",EFI_FILE_MODE_CREATE|EFI_FILE_MODE_WRITE|EFI_FILE_MODE_READ,0);
    if(EFI_ERROR(st))return st;
    UINTN n=gu.log_length;st=uefi_call_wrapper(f->Write,3,f,&n,gu.log);
    if(!EFI_ERROR(st) && n!=gu.log_length)st=EFI_DEVICE_ERROR;
    EFI_STATUS flush_status=uefi_call_wrapper(f->Flush,1,f);
    EFI_STATUS close_status=uefi_call_wrapper(f->Close,1,f);
    if(!EFI_ERROR(st) && EFI_ERROR(flush_status))st=flush_status;
    if(!EFI_ERROR(st) && EFI_ERROR(close_status))st=close_status;
    return st;
}
