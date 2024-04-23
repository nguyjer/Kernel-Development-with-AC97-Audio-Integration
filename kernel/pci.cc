#include "pci.h"
#include "debug.h"
#include "machine.h"
#include "bb.h"
#include "threads.h"
#include "process.h"
#include "pit.h"

// Some of the code is from ChatGPT, some is adapted from OSDev

#define CONFIG_ADDRESS 0xCF8
#define CONFIG_DATA 0xCFC
#define AC97_VENDOR_ID 0x8086 // Example: Intel's vendor ID
#define AC97_DEVICE_ID 0x2415 // Example: AC97's device ID

namespace AC97
{
    constexpr uint16_t AC97_RESET_REG = 0x00;
    constexpr uint16_t AC97_MASTER_VOL_REG = 0x02;
    constexpr uint16_t AC97_AUX_VOL_REG = 0x04;
    constexpr uint16_t AC97_PCM_OUT_VOL_REG = 0x18;
    constexpr uint16_t AC97_EXTENDED_AUDIO_REG = 0x28;
    constexpr uint16_t AC97_PCM_DAC_RATE_REG = 0x2C;
    constexpr uint16_t AC97_NABM_IO_GLOBAL_CONTROL = 0x2C;
    constexpr uint32_t BUFFER_SIZE = 65536; // 64 KB per buffer
    constexpr uint32_t NUM_BUFFERS = 10;

    uint32_t nam_register;
    uint32_t nabm_register;

    // Initialize AC97 codec and set up basic operation
    void initializeCodec(uint32_t nam_base, uint32_t nabm_base)
    {
        // Reset the codec by writing to the reset register using outl for 32-bit value simulation
        outl(nam_base + AC97_RESET_REG, 0x00000001);

        // // Set volume levels
        // outb(nam_base + AC97_MASTER_VOL_REG, 0x00);     // Low byte for max volume
        // outb(nam_base + AC97_MASTER_VOL_REG + 1, 0x00); // High byte for max volume
        // outb(nam_base + AC97_AUX_VOL_REG, 0x00);     // Low byte for max volume
        // outb(nam_base + AC97_AUX_VOL_REG + 1, 0x00); // High byte for max volume

        // Convert 22000 to bytes and write them separately
        uint16_t rate = 22000; // Sample rate in Hz for 22kHz
        // Set low byte of sample rate
        outb(nam_base + 0x2C, (uint8_t)(rate & 0xFF));
        // Set high byte of sample rate
        outb(nam_base + 0x2C + 1, (uint8_t)(rate >> 8));

        // Set up global control without using outd
        outl(nabm_base + AC97_NABM_IO_GLOBAL_CONTROL, (1 << 1)); // Assume 32-bit handling via outl

        Debug::printf("AC97 codec initialized with NAM base I/O address 0x%X and NABM base I/O address 0x%X\n", nam_base, nabm_base);
    }

    void play()
    {
        outb(nabm_register + 0x0B, 1);
        Debug::printf("Started playing audio.\n");
        int currJiffies = Pit::jiffies;
        int target = currJiffies + 30000; //target is 30 seconds
        while (currJiffies < target) {
            //busy wait
            Debug::printf("Jiffies = %d\n", currJiffies);
            currJiffies = Pit::jiffies;
            //iAmStuckInALoop(true);
        }
        Debug::printf("Finished playing audio.\n");
    }
}

namespace PCI
{
    // Function to construct the address for PCI config space access
    uint32_t pciConfigAddress(uint8_t bus, uint8_t device, uint8_t func, uint8_t offset)
    {
        return (uint32_t)((bus << 16) | (device << 11) | (func << 8) | (offset & 0xFC) | 0x80000000);
    }

    uint32_t pciConfigReadDWord(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset)
    {
        uint32_t address = pciConfigAddress(bus, slot, func, offset);
        outl(CONFIG_ADDRESS, address);
        return inl(CONFIG_DATA); // Read the full 32-bit data
    }

    uint16_t pciConfigReadWord(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset)
    {
        uint32_t address = pciConfigAddress(bus, slot, func, offset);
        outl(CONFIG_ADDRESS, address);
        uint32_t data = inl(CONFIG_DATA);
        return (uint16_t)((data >> ((offset & 2) * 8)) & 0xFFFF);
    }

    void pciConfigWriteWord(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint16_t value)
    {
        uint32_t address = pciConfigAddress(bus, slot, func, offset);
        outl(CONFIG_ADDRESS, address);
        // Prepare the full 32-bit value to be written in a way that preserves the other bits
        uint32_t current = inl(CONFIG_DATA);
        uint32_t mask = 0xFFFF << ((offset & 2) * 8);
        uint32_t data = (current & ~mask) | ((uint32_t)value << ((offset & 2) * 8));
        outl(CONFIG_DATA, data);
    }

    void enablePCICommandRegister(uint8_t bus, uint8_t device, uint8_t function)
    {
        // Read the current value of the command register
        uint16_t command_register = pciConfigReadWord(bus, device, function, 0x04);

        // Set bit 0 (Enable I/O space) and bit 2 (Enable bus mastering)
        command_register |= 0x0005;

        // Write the modified command register back to the PCI configuration space
        pciConfigWriteWord(bus, device, function, 0x04, command_register);
        Debug::printf("Enabled PCI register\n");
    }

    void findAC97()
    {
        uint8_t bus = 0;
        uint8_t device = 0;
        for (bus = 0; bus < 255; bus++)
        {
            for (device = 0; device < 32; device++)
            {
                Debug::printf("Testing bus %d device %d\n", bus, device);
                uint16_t vendor_id = pciConfigReadWord(bus, device, 0, 0);
                if (vendor_id == 0xFFFF)
                {
                    continue;
                }
                uint16_t device_id = pciConfigReadWord(bus, device, 0, 2);

                if (vendor_id == AC97_VENDOR_ID && device_id == AC97_DEVICE_ID)
                {
                    Debug::printf("Found AC97.\n");
                    enablePCICommandRegister(bus, device, 0);
                    uint32_t nam_base = pciConfigReadDWord(bus, device, 0, 0x10);
                    uint32_t nabm_base = pciConfigReadDWord(bus, device, 0, 0x14);
                    nam_base &= ~0x3;
                    nabm_base &= ~0x3;
                    AC97::nam_register = nam_base;
                    AC97::nabm_register = nabm_base;
                    AC97::initializeCodec(nam_base, nabm_base);
                    //gheith::current()->process->setupDMABuffers(nabm_base);
                    return;
                }
            }
        }
        Debug::printf("AC97 sound card not found\n");
    }
}