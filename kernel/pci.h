#ifndef _pci_h_
#define _pci_h_

#include <stdint.h>
#include "debug.h"
#include "machine.h"

#define CONFIG_ADDRESS 0xCF8
#define CONFIG_DATA    0xCFC

#define AC97_VENDOR_ID 0x8086  // Example: Intel's vendor ID
#define AC97_DEVICE_ID 0x2415  // Example: AC97's device ID

// // Function to write to a specified port
// void outl(uint16_t port, uint32_t value) {
//     asm volatile ("outl %0, %1" : : "a"(value), "Nd"(port));
// }

// // Function to read from a specified port
// uint32_t inl(uint16_t port) {
//     uint32_t value;
//     asm volatile ("inl %1, %0" : "=a"(value) : "Nd"(port));
//     return value;
// }

// Function to construct the address for PCI config space access
uint32_t pciConfigAddress(uint8_t bus, uint8_t device, uint8_t func, uint8_t offset) {
    return (uint32_t)((bus << 16) | (device << 11) | (func << 8) | (offset & 0xFC) | 0x80000000);
}

// Function to read a word from the PCI config space
uint16_t pciConfigReadWord(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address = pciConfigAddress(bus, slot, func, offset);
    outl(CONFIG_ADDRESS, address);
    uint32_t data = inl(CONFIG_DATA);
    return (uint16_t)((data >> ((offset & 2) * 8)) & 0xFFFF);
}

void findAC97() {
    uint8_t bus, device;
    for (bus = 0; bus < 256; bus++) {
        for (device = 0; device < 32; device++) {
            uint16_t vendor_id = pciConfigReadWord(bus, device, 0, 0);
            if (vendor_id == 0xFFFF) {
                // No device present at this slot
                continue;
            }
            uint16_t device_id = pciConfigReadWord(bus, device, 0, 2);
            if (vendor_id == AC97_VENDOR_ID && device_id == AC97_DEVICE_ID) {
                Debug::printf("AC97 sound card found at bus %d, device %d\n", bus, device);
                return;
            }
        }
    }
    Debug::printf("AC97 sound card not found\n");
}



#endif