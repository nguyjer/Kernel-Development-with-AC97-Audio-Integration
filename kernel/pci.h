// pci.h
#ifndef PCI_H
#define PCI_H

#include <stdint.h>
#include "machine.h"

// Function declarations
namespace PCI
{
    extern uint16_t pciConfigReadWord(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
    extern void findAC97();
}

namespace AC97
{
    struct BufferDescriptor
    {
        uint32_t pointer; // Physical address of the buffer
        uint16_t length;  // Length of the buffer in samples
        uint16_t reserved: 14;
        uint8_t last_buffer_entry;
        uint8_t control;
    };
    extern uint32_t BAR0;
    extern uint32_t BAR1;
    extern uint32_t GCR;

    extern bool audioPlaying;
    extern void play(uint32_t duration);
    extern bool isPlaying();
}

#endif // PCI_H
