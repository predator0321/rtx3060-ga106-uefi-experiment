/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef GA106_TARGET_H
#define GA106_TARGET_H
#define GA106_PCI_ID 0x2504
/* Board IDs are explicitly pinned by main.c parameters before execution.
 * This internal guard rejects unknown chips and invalid subsystem metadata. */
static inline int ga106_board_matches(unsigned int vendor, unsigned int device,
                                     unsigned int subvendor, unsigned int subdevice)
{
    return vendor == 0x10de && device == GA106_PCI_ID &&
           subvendor > 0 && subvendor < 0xffff &&
           subdevice > 0 && subdevice < 0xffff;
}
#endif
