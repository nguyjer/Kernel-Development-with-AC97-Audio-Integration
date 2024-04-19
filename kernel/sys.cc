#include "sys.h"
#include "stdint.h"
#include "idt.h"
#include "debug.h"
#include "threads.h"
#include "process.h"
#include "machine.h"
#include "ext2.h"
#include "elf.h"
#include "libk.h"
#include "file.h"
#include "heap.h"
#include "shared.h"
#include "kernel.h"
#include "openfilestruct.h"


int strlen(const char *string)
{
    int strlen = 0;
    int index = 0;
    while (string[index] != '\0')
    {
        strlen++;
        index++;
    }
    return strlen;
}

int SYS::exec(const char *path,
              int argc,
              const char *argv[])
{
    using namespace gheith;
    //Debug::printf("In exec. path = %s\n", path);
    auto file = root_fs->find(root_fs->root, path);
    //Debug::printf("In exec. File = %x\n", file);
    if (file == nullptr)
    {
        return -1;
    }
    if (!file->is_file())
    {
        return -1;
    }
    ElfHeader hdr;

    file->read(0,hdr);
    //Debug::printf("Can we read this file?\n");
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
    //Debug::printf("EXEC path = %s\n", path);
    //Debug::printf("EXEC argc = %d", argc);
    for (int i = 0; i < argc; i++)
    {
        // //Debug::printf("argv[%d] = %s\n", i, argv[i]);
    }
    // //Debug::printf("NULL ? = %x\n", argv[argc]);
    uint32_t sp = 0xefffe000;

    // Copy arguments
    int path_len = strlen(path);
    char *path_temp = new char[path_len + 1];
    memcpy((void *)path_temp, (void *)path, path_len + 1);
    path = path_temp;
    char **buffer = new char *[argc];
    int totalLen = 0;
    for (int i = 0; i < argc; i++)
    {
        int len = strlen(argv[i]) + 1;
        totalLen += len;
        buffer[i] = new char[len];
        memcpy(buffer[i], argv[i], len); // copy null-terminator also
    }

    // Clear the address space
    current()->process->clear_private();

    // copy all the actual chars from the buffer into the very top of the stack
    uint32_t *addresses = new uint32_t[argc]; // contains start addresses of each argv

    int alignedLen = ((totalLen + 4) / 4) * 4;
    // //Debug::printf("exec1\n");
    int offset = alignedLen - totalLen;
    sp -= offset; // padding to align SP to 4 byte boundary
    for (int i = argc - 1; i >= 0; i--)
    {
        int len = strlen(buffer[i]) + 1;
        sp -= len;
        addresses[i] = sp;
        memcpy((void *)sp, buffer[i], len);
    }

    sp -= 4;
    *((uint32_t *)sp) = 0; // nullptr to indicate end of the argv array
    // //Debug::printf("argc = %d\n", argc);
    // //Debug::printf("sp = %x\n", sp);

    for (int i = argc - 1; i >= 0; i--)
    {
        sp -= 4;
        *((uint32_t *)sp) = addresses[i];
        // ////Debug::printf("this arg = %s at %x\n", *((char **)sp), addresses[i]);
    }
    sp -= 4;
    *((uint32_t *)sp) = sp + 4;
    sp -= 4;
    *((uint32_t *)sp) = argc;

    for (int i = 0; i < argc; i++)
    {
        delete[] buffer[i];
    }
    delete[] buffer;
    delete path_temp;
    uint32_t e = ELF::load(file);

    file = nullptr;
    // uint32_t* temp_sp = (uint32_t*)sp;
    //  ////Debug::printf("ARGC = %d\n", *temp_sp);
    //  temp_sp++;
    //  while (*temp_sp != 0)
    //  {
    //      ////Debug::printf("EXEC: %s\n", *temp_sp);
    //      temp_sp++;
    //  }

    // while (temp_sp <= 0xefffe000)
    // {
    //     ////Debug::printf("SP = %x\n", temp_sp);
    //     ////Debug::printf("EXEC: %x\n", *((uint32_t *)temp_sp));
    //     temp_sp+=4;
    // }
    // ////Debug::printf("Calling switch\n");
    switchToUser(e, sp, 0);
    Debug::panic("*** implement switchToUser");
    return -1;
}

extern "C" int sysHandler(uint32_t eax, uint32_t *frame)
{
    using namespace gheith;

    uint32_t *userEsp = (uint32_t *)frame[3];
    uint32_t userPC = frame[0];

    // ////Debug::printf("*** syscall #%d\n", eax);

    switch (eax)
    {
    case 0:
    {
        auto status = userEsp[1];
        // MISSING();
        current()->process->output->set(status);
        stop();
    }
        return 0;
    case 1: /* write */
    {
        int fd = (int)userEsp[1];
        char *buf = (char *)userEsp[2];
        
        size_t nbyte = (size_t)userEsp[3];
        if ((uint32_t) buf < 0x80000000 || (uint32_t) buf == kConfig.ioAPIC || (uint32_t) buf == kConfig.localAPIC)
        {
            //Debug::printf("Passing in kernel pointer in wait.\n");
            return -1;
        }
        auto file = current()->process->getFile(fd);
        if (file == nullptr)
        {
            return -1;
        }
        return file->write(buf, nbyte);
    }
    case 2: /* fork */
    {
        int id = -1;
        Shared<Process> child = current()->process->fork(id);
        thread(child, [userPC, userEsp]
               {
                   switchToUser(userPC, (uint32_t)userEsp, 0); // return 0 in eax if child
               });
        return id;
    }
    case 3: /* sem */
    {
        uint32_t init = userEsp[1];
        int id = -1;
        Interrupts::protect([init, &id]
                            { id = activeThreads[SMP::me()]->process->newSemaphore(init); });
        return id;
    }

    case 4: /* up */
    {
        int id = (int)userEsp[1];
        Shared<Semaphore> sem;
        Interrupts::protect([id, &sem]
                            { sem = activeThreads[SMP::me()]->process->getSemaphore(id); });
        if (sem == nullptr)
        {
            return -1;
        }
        sem->up();
        return 0;
    }
    case 5: /* down */
    {
        int id = (int)userEsp[1];
        Shared<Semaphore> sem;
        Interrupts::protect([id, &sem]
                            { sem = activeThreads[SMP::me()]->process->getSemaphore(id); });
        if (sem == nullptr)
        {
            return -1;
        }
        sem->down();
        return 0;
    }
    case 6: /* close */
    {
        int id = (int)userEsp[1];
        return activeThreads[SMP::me()]->process->close(id);
    };

    case 7: /* shutdown */
        Debug::shutdown();
        return -1;

    case 8: /* wait */
    {
        /* wait for a child, status filled with exit value from child */
        /* return 0 on success, -ve value on failure */

        int id = (int)userEsp[1];
        uint32_t *status = (uint32_t *)userEsp[2];
        if (status < (uint32_t *)0x80000000 || status == (uint32_t *)kConfig.ioAPIC || status == (uint32_t *)kConfig.localAPIC)
        {
            //Debug::printf("Passing in kernel pointer in wait.\n");
            return -1;
        }
        Interrupts::protect([id, status]
                            { activeThreads[SMP::me()]->process->wait(id, status); });
        return *status;
    }
    case 9:
    {
        /* execl */
        // get path, argc, and argv from the useresp
        ////Debug::printf("*** EXECL STUFF\n");
        const char *path = (const char *)userEsp[1];
        if (path < (const char *)0x80000000 || path == (const char *)kConfig.ioAPIC || path == (const char *)kConfig.localAPIC)
        {
            //Debug::printf("Passing in kernel pointer as path in execl.\n");
            return -1;
        }
        // check is path is a valid address and if argv is valid address
        //Debug::printf("my path is %s\n", path);
        //Debug::printf("userEsp[2] = %x\n", userEsp[1]);
        //Debug::printf("userEsp[2] = %x\n", userEsp[2]);
        //Debug::printf("userEsp[2] = %x\n", userEsp[3]);
        //Debug::printf("userEsp[2] = %x\n", userEsp[4]);
        const char *argv[100]; // arbitrarily assume no more than 100 args
        int argc = 0;
        while (userEsp[argc + 2] != 0 && argc < 99)
        {
            if (userEsp[argc + 2] < 0x80000000 || userEsp[argc + 2] == kConfig.ioAPIC || userEsp[argc + 2] == kConfig.localAPIC)
            {
                //Debug::printf("Passing in kernel pointer as argv[%d] in execl.\n", argc);
                return -1;
            }
            argv[argc] = (char *)userEsp[argc + 2];
            // //Debug::printf("arg %d = %s\n", argc, argv[argc]);
            argc++;
        }
        if (argc == 99)
        {
            Debug::panic("Too many args in execl\n");
        }
        // //Debug::printf("L - argc is %d\n", argc);
        argv[argc] = nullptr; // Terminate the argv array
        //Debug::printf("Calling exec from -L\n");
        SYS::exec(path, argc, argv);
        return -1;
    }
    case 10: /* open */
    {
        const char *filename = (const char *)userEsp[1];

        if (filename < (const char *)0x80000000 || filename == (const char *)kConfig.ioAPIC || filename == (const char *)kConfig.localAPIC)
        {
            return -1;
        }
        // //Debug::printf("filename = %s\n", filename);
        //  checks for valid string here
        Shared<Node> node = root_fs->find(root_fs->root, filename);
        if (node == nullptr)
        {
            return -1;
        }
        //Debug::printf("JACOB's print: %d %d %d\n", node->is_symlink(), node->is_file(), node->is_dir());
        while (node->is_symlink()) {
            char* filename_buffer = new char[node->size_in_bytes() + 1];
            node->get_symbol(filename_buffer);
            filename_buffer[node->size_in_bytes()] = '\0';
            //Debug::printf("Symbolic link = %s\n", filename_buffer);
            node = root_fs->find(root_fs->root, filename_buffer);
            delete filename_buffer;
        }
        if (node == nullptr) //check one more time for dangling symbolic link
        {
            return -1;
        }
        File *temp = new OpenFileStruct(node);
        Shared<File> ofs(temp);
        int fd = -1;
        Interrupts::protect([ofs, &fd]()
                            { fd = activeThreads[SMP::me()]->process->setFile(ofs); });
        return fd;
    }
    case 11: /* len */
    {
        int fd = (int)userEsp[1];
        auto file = activeThreads[SMP::me()]->process->getFile(fd);
        if (file == nullptr)
        {
            return -1;
        }
        return file->size();
    }

    case 12: /* read */
             /* read */
        /* reads up to nbytes from file, returns number of bytes read */
        {
            int fd = (int)userEsp[1];
            void *buf = (void *)userEsp[2];
            if (buf < (void *)0x80000000 || (uint32_t)buf == kConfig.ioAPIC || (uint32_t)buf == kConfig.localAPIC)
            {
                //Debug::printf("Invalid read at %x\n", buf);
                return -1;
            }
            //Debug::printf("bruh %x, %x\n", kConfig.ioAPIC, kConfig.localAPIC);
            // need to check if this buffer is in correct range
            size_t nbytes = (size_t)userEsp[3];
            auto file = current()->process->getFile(fd);
            if (file == nullptr)
            {
                return -1;
            } else if (file->isU8250()) {
                return -1;
            }
            return file->read(buf, nbytes);
        }

    case 13: /* seek */
    {
        int fd = (int)userEsp[1];
        int offset = (int)userEsp[2];
        auto file = activeThreads[SMP::me()]->process->getFile(fd);
        if (file == nullptr)
        {
            return -1;
        } else if (file->isU8250()) {
            return -1;
        }
        return file->seek(offset);
    }

    default:
        //Debug::printf("*** 1000000000 unknown system call %d\n", eax);
        return -1;
    }
    return 0;
}

void SYS::init(void)
{
    IDT::trap(48, (uint32_t)sysHandler_, 3);
}
