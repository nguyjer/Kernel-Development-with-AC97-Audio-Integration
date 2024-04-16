#ifndef _pci_h_
#define _pci_h_

#include <stdint.h>
#include "debug.h"

#define AC97_VENDOR_ID 0x8086  // Example vendor ID for Intel
#define AC97_DEVICE_ID 0x2415  // Example device ID for AC97
#define MAX_PCI_BUSES 256
#define MAX_PCI_SLOTS 32
#define MAX_PCI_FUNCS 8

uint16_t pciConfigReadWord(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
void outl(uint16_t port, uint32_t value);
uint32_t inl(uint16_t port);

uint16_t pciCheckVendor(uint8_t bus, uint8_t slot) {
    uint16_t vendor, device;
    /* Try and read the first configuration register. */
    vendor = pciConfigReadWord(bus, slot, 0, 0);
    if (vendor != 0xFFFF) {
        device = pciConfigReadWord(bus, slot, 0, 2);
        if (vendor == AC97_VENDOR_ID && device == AC97_DEVICE_ID) {
            // Found the AC97 device
            return 1;
        }
    }
    return 0;
}

void scanForAC97() {
    uint8_t bus, slot, func;

    for (bus = 0; bus < MAX_PCI_BUSES; bus++) {
        for (slot = 0; slot < MAX_PCI_SLOTS; slot++) {
            for (func = 0; func < MAX_PCI_FUNCS; func++) {
                if (pciCheckVendor(bus, slot) == 1) {
                    Debug::printf("AC97 found at bus %d, slot %d, func %d\n", bus, slot, func);
                    return; // Stop scanning after finding the first AC97 device
                }
            }
        }
    }
    Debug::printf("AC97 not found\n");
}


#endif