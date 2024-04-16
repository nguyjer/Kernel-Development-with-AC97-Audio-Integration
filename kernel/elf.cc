#include "elf.h"
#include "machine.h"
#include "debug.h"
#include "config.h"

uint32_t ELF::load(Shared<Node> file) {
#if 0
    MISSING();
    return 0;
#else
    //Debug::printf("Loading a file\n");
    if (file == nullptr) {
        return -1;
    }
    ElfHeader hdr;

    file->read(0,hdr);

    if (hdr.magic0 != 0x7F || hdr.magic1 != 'E' || hdr.magic2 != 'L' || hdr.magic3 != 'F') {
        return -1; //not a valid ELF file
    }
    if (hdr.encoding != 1 || hdr.cls != 1 || hdr.abi != 0 || hdr.header_version != 1) { //Little-endian 32-bit Unix
        return -1;
    }
    if (hdr.version != 1 || hdr.type != 2) { //valid ELF version, executable
        return -1;
    }
    if (hdr.phoff == 0) {
        return -1;
    }
    //Debug::printf("%d, %c, %c, %c\n", hdr.magic0, hdr.magic1, hdr.magic2, hdr.magic3);
    uint32_t hoff = hdr.phoff;
    if (hoff == 0) {
        return -1;
    }
    for (uint32_t i=0; i<hdr.phnum; i++) {
        ProgramHeader phdr;
        file->read(hoff,phdr);
        hoff += hdr.phentsize;

        if (phdr.type == 1) {
            char *p = (char*) phdr.vaddr;
            if ((uint32_t) p < 0x80000000 || ((uint32_t) p) + phdr.filesz > 0xFFFFFFFF
            || (uint32_t) p == kConfig.ioAPIC || (uint32_t) p == kConfig.localAPIC) {
                Debug::panic("*** Invalid segment.\n");
                continue; //invalid segment
            }
            uint32_t memsz = phdr.memsz;
            uint32_t filesz = phdr.filesz;

            //Debug::printf("vaddr:%x memsz:0x%x filesz:0x%x fileoff:%x\n",
                //p,memsz,filesz,phdr.offset);
            file->read_all(phdr.offset,filesz,p);
            bzero(p + filesz, memsz - filesz);
        }
    }

    return hdr.entry;
#endif
}
