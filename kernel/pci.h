// pci.h
#ifndef PCI_H
#define PCI_H

#include <stdint.h>

// Function declarations
uint16_t pciConfigReadWord(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
void findAC97();

#endif // PCI_H
