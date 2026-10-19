/* SPDX-License-Identifier: GPL-2.0-only */
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include "env.h"

EFI_BOOT_SERVICES *BS;
static int allocs,maps,unmaps,frees,fail_map,short_map,fail_unmap;
static u64 device_address=0x12345000;
static UINTN last_pages,last_bytes;
static EFI_STATUS EFIAPI mock_alloc(EFI_PCI_IO_PROTOCOL *p,EFI_ALLOCATE_TYPE t,
    EFI_MEMORY_TYPE type,UINTN pages,void **host,UINT64 attrs)
{
    assert(t==AllocateAnyPages && type==EfiBootServicesData && !attrs);
    allocs++;last_pages=pages;*host=calloc(pages,4096);return *host?EFI_SUCCESS:EFI_OUT_OF_RESOURCES;
}
static EFI_STATUS EFIAPI mock_map(EFI_PCI_IO_PROTOCOL *p,EFI_PCI_IO_PROTOCOL_OPERATION op,
    void *host,UINTN *bytes,EFI_PHYSICAL_ADDRESS *addr,void **mapping)
{
    assert(op==EfiPciIoOperationBusMasterCommonBuffer);
    maps++;last_bytes=*bytes;if(fail_map)return EFI_DEVICE_ERROR;
    if(short_map)*bytes=1;*addr=device_address;*mapping=host;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI mock_unmap(EFI_PCI_IO_PROTOCOL *p,void *mapping)
{ assert(mapping);unmaps++;return fail_unmap?EFI_DEVICE_ERROR:EFI_SUCCESS; }
static EFI_STATUS EFIAPI mock_free(EFI_PCI_IO_PROTOCOL *p,UINTN pages,void *host)
{ assert(pages==last_pages);frees++;free(host);return EFI_SUCCESS; }
static int mmio_reads,mmio_writes,fail_mmio;
static EFI_STATUS EFIAPI mock_read(EFI_PCI_IO_PROTOCOL *p,EFI_PCI_IO_PROTOCOL_WIDTH width,
    UINT8 bar,UINT64 offset,UINTN count,void *buffer)
{
    assert(bar==0 && offset==0x1234 && count==1 && width==EfiPciIoWidthUint32);
    mmio_reads++;*(u32 *)buffer=0x172000a1;return fail_mmio?EFI_DEVICE_ERROR:EFI_SUCCESS;
}
static EFI_STATUS EFIAPI mock_write(EFI_PCI_IO_PROTOCOL *p,EFI_PCI_IO_PROTOCOL_WIDTH width,
    UINT8 bar,UINT64 offset,UINTN count,void *buffer)
{ assert(bar==0 && offset==0x1234 && count==1 && *(u32 *)buffer==7);mmio_writes++;return EFI_SUCCESS; }

int main(void)
{
    EFI_PCI_IO_PROTOCOL pci={.AllocateBuffer=mock_alloc,.Map=mock_map,.Unmap=mock_unmap,
        .FreeBuffer=mock_free,.Mem={.Read=mock_read,.Write=mock_write}};
    gu.pci=&pci;gu.card.dev.coherent_dma_mask=0xffffffffull;
    dma_addr_t address;void *p=dma_alloc_coherent(&gu.card.dev,5,&address,0);
    assert(p && address==device_address && last_pages==1 && last_bytes==4096);
    /* Device addresses come from Map, never from a cast of the host pointer. */
    assert((u64)p!=address);dma_free_coherent(&gu.card.dev,5,p,address);
    assert(allocs==1 && maps==1 && unmaps==1 && frees==1);
    assert(!gu_live_dma_count());
    fail_map=1;assert(!dma_alloc_coherent(&gu.card.dev,4096,&address,0));fail_map=0;
    assert(unmaps==1 && frees==2);
    short_map=1;assert(!dma_alloc_coherent(&gu.card.dev,4096,&address,0));short_map=0;
    assert(unmaps==2 && frees==3);
    device_address=0xfffff000;
    assert(!dma_alloc_coherent(&gu.card.dev,8192,&address,0));
    assert(unmaps==3 && frees==4);
    assert(!dma_alloc_coherent(&gu.card.dev,0,&address,0));
    assert(!dma_alloc_coherent(&gu.card.dev,~(size_t)0,&address,0));
    assert(allocs==4);
    device_address=0x12345000;
    p=dma_alloc_coherent(&gu.card.dev,4096,&address,0);assert(p);
    fail_unmap=1;dma_free_coherent(&gu.card.dev,4096,p,address);
    assert(gu.retained && frees==4); /* Still mapped: do not free or boot an OS. */
    assert(gu_live_dma_count()==1);
    fail_unmap=0;dma_free_coherent(&gu.card.dev,4096,p,address);assert(frees==5);
    gu.retained=false;gu.bar_length=0x1000000;
    assert(!gu_live_dma_count());
    gu_log("id=%08x count=%zu bytes=%#llx text=%s error=%d",0x250410de,
           (size_t)123,0x100000000ull,"ok",-22);
    assert(!gu_strcmp(gu.log,"id=250410de count=123 bytes=0x100000000 text=ok error=-22"));
    assert(readl((u8 *)GU_MMIO_COOKIE+0x1234)==0x172000a1 && mmio_reads==1);
    writel(7,(u8 *)GU_MMIO_COOKIE+0x1234);assert(mmio_writes==1);
    fail_mmio=1;readl((u8 *)GU_MMIO_COOKIE+0x1234);assert(gu.io_fault);
    writel(7,(u8 *)GU_MMIO_COOKIE+0x1234);assert(mmio_writes==1);
    gu.io_fault=false;
    assert(readl((u8 *)GU_MMIO_COOKIE+gu.bar_length)==~0u && mmio_reads==2);
    writel(7,(u8 *)GU_MMIO_COOKIE+gu.bar_length);assert(gu.io_fault && mmio_writes==1);
    puts("UEFI DMA Map/unmap, 32-bit limits, retained buffers and MMIO fail-closed tests passed");
    return 0;
}
