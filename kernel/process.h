#ifndef _PROCESS_H_
#define _PROCESS_H_

#include "atomic.h"
#include "stdint.h"
#include "shared.h"
#include "vmm.h"
#include "blocking_lock.h"
#include "future.h"
#include "file.h"
#include "u8250.h"
#include "shared.h"
#include "pci.h"
struct WAVHeader
{
    unsigned char riff[4];
    uint32_t file_size;
    unsigned char magic0;
    unsigned char magic1;
    unsigned char magic2;
    unsigned char magic3;
    unsigned char fmt[4];
    uint32_t fmt_length;
    uint16_t format_type;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t sample_rate_eq;
    uint16_t garb;
    uint16_t bitsPerSample;
    unsigned char start_of_data[4];
    uint32_t data_size;
};

class Process
{
    constexpr static int NSEM = 10;
    constexpr static int NCHILD = 10;
    constexpr static int NFILE = 10;
    constexpr static int NBUFFERS = 32;

    Shared<File> files[NFILE]{};
    Shared<Semaphore> sems[NSEM]{};
    Shared<Future<uint32_t>> children[NCHILD]{};
    BlockingLock mutex{};
    

    int getChildIndex(int id);
    int getSemaphoreIndex(int id);
    int getFileIndex(int id);

    Atomic<uint32_t> ref_count{0};

public:
    Shared<Future<uint32_t>> output = Shared<Future<uint32_t>>::make(); // { new Future<uint32_t>() };
    uint32_t *pd = gheith::make_pd();
    static Shared<Process> kernelProcess;

    Process(bool isInit);
    virtual ~Process();

    Shared<Process> fork(int &id);
    void clear_private();


    uint32_t fillBuffers(Shared<File> file);

    int newSemaphore(uint32_t init);

    Shared<Semaphore> getSemaphore(int id);

    Shared<File> getFile(int fd)
    {
        auto i = getFileIndex(fd);
        if (i < 0)
        {
            return Shared<File>();
        }
        return files[fd];
    }

    int setFile(Shared<File> file)
    {
        for (auto i = 0; i < NFILE; i++)
        {
            auto f = files[i];
            if (f == nullptr)
            {
                files[i] = file;
                return i;
            }
        }
        return -1;
    }

    int close(int id);
    void exit(uint32_t v)
    {

        output->set(v);
    }
    int wait(int id, uint32_t *ptr);

    static void init(void);

    friend class Shared<Process>;
    // friend class gheith::TCB;
};

#endif