/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef GA106_EFI_ENV_H
#define GA106_EFI_ENV_H
#include <efi.h>
#include <efilib.h>
#include <efipciio.h>
#include "include/compat.h"

#define GU_LOG_CAP (2u * 1024u * 1024u)
#define GU_MMIO_COOKIE ((void *)0x1000000000ull)
struct gu_env {
    EFI_PCI_IO_PROTOCOL *pci;
    EFI_FILE_HANDLE root;
    struct pci_dev card;
    u64 dma_mask, bar_length, ticks_per_us;
    u32 saved_bars[6];
    u16 saved_command;
    unsigned int pcie_cap;
    bool io_fault, retained;
    char log[GU_LOG_CAP];
    size_t log_length;
};
extern struct gu_env gu;
EFI_STATUS gu_read_file(const CHAR16 *, void **, UINTN *, UINTN);
EFI_STATUS gu_write_log(void);
int gu_init_clock(void);
unsigned int gu_live_dma_count(void);
void gu_sha256(const void *, size_t, u8[32]);
int gu_verify_asset(const char *, const void *, size_t);
#endif
