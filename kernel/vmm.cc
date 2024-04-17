#include "vmm.h"
#include "machine.h"
#include "idt.h"
#include "libk.h"
#include "blocking_lock.h"
#include "config.h"
#include "threads.h"
#include "debug.h"
#include "ext2.h"
#include "physmem.h"
#include "process.h"

namespace gheith
{

    using namespace PhysMem;

    uint32_t *shared = nullptr;

        bool is_special(uint32_t va)
    {
        return (va < 0x80000000) || (va == kConfig.ioAPIC) || (va == kConfig.localAPIC);
    }

    void map(uint32_t *pd, uint32_t va, uint32_t pa)
    {
        bool shared = (va < 0x80000000 || va == kConfig.ioAPIC || va == kConfig.localAPIC);
        int num = shared ? 3 : 7; //user bit set or not, dont let user touch kernel stuff
        auto pdi = va >> 22;
        auto pti = (va >> 12) & 0x3FF;
        auto pde = pd[pdi];
        if ((pde & 1) == 0)
        {
            pde = PhysMem::alloc_frame() | num;
            pd[pdi] = pde;
        }
        auto pt = (uint32_t *)(pde & 0xFFFFF000);
        pt[pti] = pa | num;
    }

    void unmap(uint32_t *pd, uint32_t va)
    {
        if (is_special(va)) {
            return;
        }
        auto pdi = va >> 22;
        auto pti = (va >> 12) & 0x3FF;
        auto pde = pd[pdi];
        if ((pde & 1) == 0)
            return;
        auto pt = (uint32_t *)(pd[pdi] & 0xFFFFF000);
        auto pte = pt[pti];
        if ((pte & 1) == 0)
            return;
        auto pa = pte & 0xFFFFF000;
        pt[pti] = 0;
        dealloc_frame(pa);
        invlpg(va);
    }


    uint32_t *make_pd()
    {
        auto pd = (uint32_t *)PhysMem::alloc_frame();

        auto m4 = 4 * 1024 * 1024;
        auto shared_size = 4 * (((kConfig.memSize + m4 - 1) / m4));

        memcpy(pd, shared, shared_size);

        map(pd, kConfig.ioAPIC, kConfig.ioAPIC);
        map(pd, kConfig.localAPIC, kConfig.localAPIC);
        // map(pd, kCOnfig.ACpic)

        return pd;
    }

    void delete_private(uint32_t *pd)
    {
        ASSERT(uint32_t(pd) == getCR3());
        for (unsigned pdi = 512; pdi < 1024; pdi++)
        {
            auto pde = pd[pdi];
            if ((pde & 1) == 0)
                continue;
            auto pt = (uint32_t *)(pde & 0xFFFFF000);

            bool contains_special = false;

            for (unsigned pti = 0; pti < 1024; pti++)
            {
                auto va = (pdi << 22) | (pti << 12);
                if (is_special(va))
                {
                    contains_special = true;
                    continue;
                }
                auto pte = pt[pti];
                if ((pte & 1) == 0)
                    continue;
                pt[pti] = 0;
                auto frame = pte & 0xFFFFF000;
                dealloc_frame(frame);
                invlpg(va);
            }
            if (!contains_special)
            {
                pd[pdi] = 0;
                dealloc_frame((uint32_t)pt);
            }
        }
    }

    void delete_pd(uint32_t *pd)
    {
        ASSERT(uint32_t(pd) != getCR3());

        for (unsigned pdi = 512; pdi < 1024; pdi++)
        {
            auto pde = pd[pdi];
            if ((pde & 1) == 0)
                continue;
            auto pt = (uint32_t *)(pde & 0xFFFFF000);

            for (unsigned pti = 0; pti < 1024; pti++)
            {
                auto va = (pdi << 22) | (pti << 12);

                auto pte = pt[pti];
                if ((pte & 1) == 0)
                    continue;
                auto frame = pte & 0xFFFFF000;
                if (!is_special(va))
                {
                    dealloc_frame(frame);
                }
            }
            dealloc_frame((uint32_t)pt);
        }

        PhysMem::dealloc_frame((uint32_t)pd);
    }
}

namespace VMM
{

    void global_init()
    {
        using namespace gheith;
        shared = (uint32_t *)PhysMem::alloc_frame();

        for (uint32_t va = FRAME_SIZE; va < kConfig.memSize; va += FRAME_SIZE)
        {
            map(shared, va, va);
        }
    }

    void per_core_init()
    {
        using namespace gheith;

        Interrupts::protect([]
                            {
        ASSERT(Interrupts::isDisabled());
        auto me = activeThreads[SMP::me()];
        vmm_on((uint32_t)me->process->pd); });
    }

} /* namespace vmm */

extern "C" void vmm_pageFault(uintptr_t va_, uintptr_t *saveState)
{
    using namespace gheith;
    auto me = current();
    ASSERT((uint32_t)me->process->pd == getCR3());
    ASSERT(me->saveArea.cr3 == getCR3());

    uint32_t va = PhysMem::framedown(va_);

    if (va >= 0x80000000)
    {
        auto pa = PhysMem::alloc_frame();
        map(me->process->pd, va, pa);
        return;
    }

    current()->process->exit(current()->id);
    stop();
}