/// Routines to manage address spaces (memory for executing user programs).
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "address_space.hh"
#include "executable.hh"
#include "threads/system.hh"

#include <string.h>
#include <stdio.h>


/// First, set up the translation from program memory to physical memory.
/// For now, this is really simple (1:1), since we are only uniprogramming,
/// and we have a single unsegmented page table.
AddressSpace::AddressSpace(OpenFile *executable_file)
{
    ASSERT(executable_file != nullptr);

    Executable exe (executable_file);
    ASSERT(exe.CheckMagic());
#ifdef DEMAND_LOADING
    executable = executable_file;
    codeSize = exe.GetCodeSize();
    initDataSize = exe.GetInitDataSize();
    codeAddr = exe.GetCodeAddr();
    initDataAddr = exe.GetInitDataAddr();
#endif

    // How big is address space?

    unsigned size = exe.GetSize() + USER_STACK_SIZE;
    // We need to increase the size to leave room for the stack.
    numPages = DivRoundUp(size, PAGE_SIZE);
    size = numPages * PAGE_SIZE;
    tlbIndex = 0;

    ASSERT(numPages <= usedPages->CountClear());
    // Check we are not trying to run anything too big -- at least until we
    // have virtual memory.

    DEBUG('a', "Initializing address space, num pages %u, size %u\n",
          numPages, size);

    // First, set up the translation.

#ifndef DEMAND_LOADING
    char *mainMemory = machine->GetMMU()->mainMemory;
#endif
    pageTable = new TranslationEntry[numPages];
    for (unsigned i = 0; i < numPages; i++) {
        pageTable[i].virtualPage  = i;
        #ifndef DEMAND_LOADING
        pageTable[i].physicalPage = usedPages->Find();
        pageTable[i].valid        = true;
        #else
        pageTable[i].physicalPage = -1;
        pageTable[i].valid        = false;
        #endif
        pageTable[i].use          = false;
        pageTable[i].dirty        = false;
        pageTable[i].readOnly     = false;
        // If the code segment was entirely on a separate page, we could
        // set its pages to be read-only.
#ifndef DEMAND_LOADING
        memset(mainMemory + (pageTable[i].physicalPage * PAGE_SIZE), 0, PAGE_SIZE);
#endif
    }

#ifndef DEMAND_LOADING
    // Then, copy in the code and data segments into memory.
    uint32_t codeSize = exe.GetCodeSize();
    uint32_t initDataSize = exe.GetInitDataSize();
    if (codeSize > 0) {
        uint32_t virtualAddr = exe.GetCodeAddr();
        DEBUG('a', "Initializing code segment, at 0x%X, size %u\n",
              virtualAddr, codeSize);
        for (uint32_t i = 0; i < codeSize; i++) {
          uint32_t physicalPage = pageTable[DivRoundDown(virtualAddr + i, PAGE_SIZE)].physicalPage;

          uint32_t offset = (virtualAddr + i) % PAGE_SIZE;

          uint32_t physicalAddress = physicalPage * PAGE_SIZE + offset;

          exe.ReadCodeBlock(&mainMemory[physicalAddress], 1, i);
        }
    }
    if (initDataSize > 0) {
        uint32_t virtualAddr = exe.GetInitDataAddr();
        DEBUG('a', "Initializing data segment, at 0x%X, size %u\n",
              virtualAddr, initDataSize);
        for (uint32_t i = 0; i < initDataSize; i++) {
          uint32_t physicalPage = pageTable[DivRoundDown(virtualAddr + i, PAGE_SIZE)].physicalPage;

          uint32_t offset = (virtualAddr + i) % PAGE_SIZE;

          uint32_t physicalAddress = physicalPage * PAGE_SIZE + offset;

          exe.ReadDataBlock(&mainMemory[physicalAddress], 1, i);
        }
    }
#endif
}

/// Deallocate an address space.
///
/// Nothing for now!
AddressSpace::~AddressSpace()
{
    for(unsigned i = 0 ; i < numPages ; i++) 
    {
        if(pageTable[i].valid) 
        {
            usedPages->Clear(pageTable[i].physicalPage);
        }
    }
#ifdef DEMAND_LOADING
    delete executable;
#endif
    delete [] pageTable;

}

/// Set the initial values for the user-level register set.
///
/// We write these directly into the “machine” registers, so that we can
/// immediately jump to user code.  Note that these will be saved/restored
/// into the `currentThread->userRegisters` when this thread is context
/// switched out.
void
AddressSpace::InitRegisters()
{
    for (unsigned i = 0; i < NUM_TOTAL_REGS; i++) {
        machine->WriteRegister(i, 0);
    }

    // Initial program counter -- must be location of `Start`.
    machine->WriteRegister(PC_REG, 0);

    // Need to also tell MIPS where next instruction is, because of branch
    // delay possibility.
    machine->WriteRegister(NEXT_PC_REG, 4);

    // Set the stack register to the end of the address space, where we
    // allocated the stack; but subtract off a bit, to make sure we do not
    // accidentally reference off the end!
    machine->WriteRegister(STACK_REG, numPages * PAGE_SIZE - 16);
    DEBUG('a', "Initializing stack register to %u\n",
          numPages * PAGE_SIZE - 16);
}

/// On a context switch, save any machine state, specific to this address
/// space, that needs saving.
///
/// For now, nothing!
void
AddressSpace::SaveState()
{
    for(unsigned page = 0; page < TLB_SIZE; page++)
    {
        SavePageFromTLB(page);
    }
}

void
AddressSpace::InvalidateTLB()
{
    MMU *mmu = machine->GetMMU();
    for(unsigned i = 0; i < TLB_SIZE; i++)
    {
        mmu->tlb[i].valid = false;
    }
}

void
AddressSpace::SavePageFromTLB(unsigned page)
{
    TranslationEntry *tlb = machine->GetMMU()->tlb;
    if (tlb[page].valid)
    {
        pageTable[tlb[page].virtualPage].dirty = tlb[page].dirty;
        pageTable[tlb[page].virtualPage].use   = tlb[page].use;
        tlb[page].valid = false;
    }
}

bool 
AddressSpace::SetTlbPage(TranslationEntry *pageTranslation)
{
#ifdef USE_TLB
    if(pageTranslation == nullptr) {
        DEBUG('p', "Page translation invalid\n");
        return false;
    }
    TranslationEntry *tlb = machine->GetMMU()->tlb;
    DEBUG('p', "Set Tlb in page: %d \n", tlbIndex);

    if(tlb[tlbIndex].valid)
    {
        SavePageFromTLB(tlbIndex);
    }
    tlb[tlbIndex] = *pageTranslation;
    tlbIndex = (tlbIndex + 1) % TLB_SIZE;
    return true;
#else
    DEBUG('p', "TLB not present in the machine.\n");
    return false;
#endif
}

TranslationEntry * 
AddressSpace::GetTranslationEntry(unsigned vpn)
{
    return &pageTable[vpn];
}

/// On a context switch, restore the machine state so that this address space
/// can run.
///
/// For now, tell the machine where to find the page table.
void
AddressSpace::RestoreState()
{
#ifdef USE_TLB
    InvalidateTLB();
    DEBUG('a', "TLB has been invalidated\n");
#else
    machine->GetMMU()->pageTable     = pageTable;
    machine->GetMMU()->pageTableSize = numPages;
#endif
}

void
AddressSpace::LoadPage(unsigned vpn)
{
    DEBUG('p', "Loading page %u\n", vpn);

    int frame = usedPages->Find();
    ASSERT(frame != -1);

    DEBUG('p', "Physical page found\n");
    unsigned virtualAddr = vpn * PAGE_SIZE;
    pageTable[vpn].physicalPage = frame;
    pageTable[vpn].virtualPage  = vpn;
    pageTable[vpn].valid        = true;

    Executable exe (executable);
    ASSERT(exe.CheckMagic());

    uint32_t physicalAddr = frame * PAGE_SIZE;

    char *mainMemory = machine->GetMMU()->mainMemory;

    unsigned leftToRead = PAGE_SIZE;
    unsigned sizeToRead;

    if (virtualAddr >= codeSize + initDataSize) {
        sizeToRead = PAGE_SIZE;
        memset(mainMemory + physicalAddr, 0, sizeToRead);
        pageTable[vpn].readOnly = false;
    } else if (virtualAddr >= codeSize) {
        sizeToRead = PAGE_SIZE < codeSize + initDataSize - virtualAddr ? 
                     PAGE_SIZE : codeSize + initDataSize - virtualAddr;
        exe.ReadDataBlock(mainMemory + physicalAddr, sizeToRead, virtualAddr - codeSize);
        pageTable[vpn].readOnly = false;
    } else {
        sizeToRead = PAGE_SIZE < codeAddr + codeSize - virtualAddr ?
                     PAGE_SIZE : codeAddr + codeSize - virtualAddr;
        exe.ReadCodeBlock(mainMemory + physicalAddr, sizeToRead, virtualAddr - codeAddr);
        pageTable[vpn].readOnly = true;
    }

    leftToRead -= sizeToRead;

    if (leftToRead > 0) {
        virtualAddr += sizeToRead;
        physicalAddr += sizeToRead;
        pageTable[vpn].readOnly = false;
        if (virtualAddr >= codeSize + initDataSize) {
            sizeToRead = PAGE_SIZE;
            memset(mainMemory + physicalAddr, 0, sizeToRead);
        } else if (virtualAddr >= codeSize) {
            exe.ReadDataBlock(mainMemory + physicalAddr, leftToRead, virtualAddr - codeSize);
        } else {
            ASSERT(false);
        }
    }
}