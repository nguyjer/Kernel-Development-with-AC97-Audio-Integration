#include "pci.h"
#include "debug.h"
#include "machine.h"
#include "bb.h"
#include "threads.h"
#include "process.h"
#include "pit.h"

// Some of the code is from ChatGPT, some is adapted from OSDev.

#define CONFIG_ADDRESS 0xCF8
#define CONFIG_DATA 0xCFC
#define AC97_VENDOR_ID 0x8086 // Example: Intel's vendor ID
#define AC97_DEVICE_ID 0x2415 // Example: AC97's device ID
bool setupBuffers = false;

namespace AC97
{
    constexpr uint16_t AC97_RESET_REG = 0x00;
    constexpr uint16_t AC97_MASTER_VOL_REG = 0x02;
    constexpr uint16_t AC97_AUX_VOL_REG = 0x04;
    constexpr uint16_t AC97_PCM_OUT_VOL_REG = 0x18;
    constexpr uint16_t AC97_EXTENDED_AUDIO_REG = 0x28;
    constexpr uint16_t AC97_PCM_DAC_RATE_REG = 0x2C;
    constexpr uint16_t AC97_NABM_IO_GLOBAL_CONTROL = 0x2C;
    constexpr uint32_t BUFFER_SIZE = 131070; // 64 KB per buffer
    constexpr uint32_t NUM_BUFFERS = 32;

    uint32_t BAR0;
    uint32_t BAR1;
    uint32_t GCR;
    BufferDescriptor *audio_buffers;

    bool audioPlaying = false;
    void setupDMABuffers(uint32_t nabm_base)
    {
        if (setupBuffers)
        {
            return;
        }
        audio_buffers = new AC97::BufferDescriptor[NUM_BUFFERS];
        for (uint32_t i = 0; i < NUM_BUFFERS; i++)
        {
            audio_buffers[i].pointer = (uint32_t) new char[BUFFER_SIZE];
            audio_buffers[i].length = 0xFFFE;
        }

        // Assuming the first descriptor is located at nabm_base + 0x00 for PCM Out
        // Debug::printf("| DMA buffers setup completed.\n");
        setupBuffers = true;
    }
    // Initialize AC97 codec and set up basic operation
    void initializeCodec()
    {
        // Properly setting global control register, ensuring correct register (0x6C)

        outl(GCR, (0b00 << 22) | (0b00 << 20) | (0 << 2) | (1 << 1));

        outb(BAR1 + 0xB, 0x2);

        // Reset the codec by writing to the reset register using outl for 32-bit value simulation
        outw(BAR0, 0xFF);

        // Set volume levels to maximum (0x0000 is maximum, 0x8000 is mute in AC97)
        // int temp = nam_base + AC97_MASTER_VOL_REG;

        outw(BAR0 + AC97_PCM_OUT_VOL_REG, 0x0); // PCM volume to max

        setupDMABuffers(BAR1);

        // outl(BAR0 + AC97_MASTER_VOL_REG, 0x0000); // Master volume to max
        // outl(BAR0 + AC97_AUX_VOL_REG, 0x0000);    // AUX volume to max

        // Enable audio output

        Debug::printf("| AC97 codec initialized with NAM base I/O address 0x%X and NABM base I/O address 0x%X\n", BAR0, BAR1);
    }

    void play(uint32_t duration, uint32_t jiffies)
    {
        outw(BAR1 + 0x06, 0x1C);
        outb(BAR1 + 0x0B, 0x1);
        Debug::printf("Started playing audio.\n");
        uint32_t target = Pit::jiffies + Pit::secondsToJiffies(duration) + jiffies; 
        // sti();
        // Debug::printf("jiffies per second = %d\n", Pit::secondsToJiffies(30));
        while (Pit::jiffies < target)
        {
            iAmStuckInALoop(true);
        }
        outb(BAR1 + 0x0B, 0x2);
        Debug::printf("Finished playing audio.\n");
    }

    bool isPlaying()
    {
        return audioPlaying;
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
        Debug::printf("| Enabled PCI register\n");
    }

    void findAC97()
    {
        uint8_t bus = 0;
        uint8_t device = 0;
        for (bus = 0; bus < 255; bus++)
        {
            for (device = 0; device < 32; device++)
            {
                // Debug::printf("Testing bus %d device %d\n", bus, device);
                uint16_t vendor_id = pciConfigReadWord(bus, device, 0, 0);
                uint16_t device_id = pciConfigReadWord(bus, device, 0, 2);
                // if (bus == 0 && device == 4)
                // {
                // Debug::printf("Suppose to be AC97 vendor_id = %d device_id = %d\n", vendor_id, device_id);
                // }
                if (vendor_id == 0xFFFF)
                {
                    continue;
                }

                if (vendor_id == AC97_VENDOR_ID && device_id == AC97_DEVICE_ID)
                {
                    Debug::printf("| Found AC97.\n");
                    enablePCICommandRegister(bus, device, 0);
                    uint32_t nam_base = pciConfigReadDWord(bus, device, 0, 0x10);
                    uint32_t nabm_base = pciConfigReadDWord(bus, device, 0, 0x14);
                    nam_base &= ~0x3;
                    nabm_base &= ~0x3;
                    AC97::BAR0 = nam_base;
                    AC97::BAR1 = nabm_base + 0x10;
                    AC97::GCR = nabm_base + 0x2C;
                    AC97::initializeCodec();
                    // gheith::current()->process->setupDMABuffers(nabm_base);
                    return;
                }
            }
        }
        Debug::panic("XXX AC97 sound card not found\n");
    }
}
