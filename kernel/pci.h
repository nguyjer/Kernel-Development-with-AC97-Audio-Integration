// pci.h
#ifndef PCI_H
#define PCI_H

#include <stdint.h>

// Function declarations
namespace PCI
{
    uint16_t pciConfigReadWord(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
    void findAC97();
}

namespace AC97 {
    struct BufferDescriptor {
        uint32_t pointer;  // Physical address of the buffer
        uint32_t length;   // Length of the buffer in samples
        uint32_t control;  // Control flags
    };
}

#endif // PCI_H
