#include "pci.h"
#include "debug.h"
#include "machine.h"

#define CONFIG_ADDRESS 0xCF8
#define CONFIG_DATA    0xCFC
#define AC97_VENDOR_ID 0x8086  // Example: Intel's vendor ID
#define AC97_DEVICE_ID 0x2415  // Example: AC97's device ID

// Function to construct the address for PCI config space access
uint32_t pciConfigAddress(uint8_t bus, uint8_t device, uint8_t func, uint8_t offset) {
    return (uint32_t)((bus << 16) | (device << 11) | (func << 8) | (offset & 0xFC) | 0x80000000);
}

uint32_t pciConfigReadDWord(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address = pciConfigAddress(bus, slot, func, offset);
    outl(CONFIG_ADDRESS, address);
    return inl(CONFIG_DATA); // Read the full 32-bit data
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
                continue; // No device present at this slot
            }
            uint16_t device_id = pciConfigReadWord(bus, device, 0, 2);
            if (vendor_id == AC97_VENDOR_ID && device_id == AC97_DEVICE_ID) {
                Debug::printf("AC97 sound card found at bus %d, device %d\n", bus, device);

                // Read BARs
                for (int bar_num = 0; bar_num < 6; bar_num++) {
                    uint32_t bar = pciConfigReadDWord(bus, device, 0, 0x10 + bar_num * 4);
                    bool is_io = bar & 1; // Check if it's I/O space (bit 0 is 1)
                    uint32_t address = bar & ~0x3; // Mask out the type bits

                    if (is_io) {
                        Debug::printf("BAR %d is an I/O space BAR at address 0x%X\n", bar_num, address);
                    } else {
                        Debug::printf("BAR %d is a memory space BAR at address 0x%X\n", bar_num, address);
                    }
                }
                return;
            }
        }
    }
    Debug::printf("AC97 sound card not found\n");
}