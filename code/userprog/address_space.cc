/// Routines to manage address spaces (memory for executing user programs).
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "address_space.hh"
#include "threads/system.hh"

#ifdef SWAP
#include "filesys/directory_entry.hh"
#endif

#include <string.h>
#include <stdio.h>


/// First, set up the translation from program memory to physical memory.
/// For now, this is really simple (1:1), since we are only uniprogramming,
/// and we have a single unsegmented page table.
AddressSpace::AddressSpace(OpenFile *executable_file, int pid)
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

#ifdef SWAP
    swapName = new char[FILE_NAME_MAX_LEN];
    snprintf(swapName, FILE_NAME_MAX_LEN, "SWAP.%u", pid);
    ASSERT(fileSystem->Create(swapName, size));
    ASSERT(swapFile = fileSystem->Open(swapName));
    DEBUG('p', "Archivo swap creado nombre %s\n", swapName);
    inSwap = new Bitmap(numPages);
#else
    // Check we are not trying to run anything too big -- at least until we
    // have virtual memory.
    ASSERT(numPages <= usedPages->CountClear());
#endif
    

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
            #ifndef SWAP
                pageTable[i].physicalPage = usedPages->Find();
            #else
                pageTable[i].physicalPage = coreMap->Find(i, this);
            #endif
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
            #ifdef SWAP
                coreMap->Clear(pageTable[i].physicalPage);
            #else
                usedPages->Clear(pageTable[i].physicalPage);
            #endif
        }
    }
#ifdef DEMAND_LOADING
    delete executable;
#endif
#ifdef SWAP
    delete swapFile;
    fileSystem->Remove(swapName);
    delete inSwap;
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
    //DEBUG('p', "Saving TLB pages on context switch\n");
    for(unsigned page = 0; page < TLB_SIZE; page++)
    {
        SavePageFromTLB(page);
    }
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
    //DEBUG('p', "TLB has been invalidated\n");
#else
    machine->GetMMU()->pageTable     = pageTable;
    machine->GetMMU()->pageTableSize = numPages;
#endif
}

void
AddressSpace::InvalidateTLB()
{
    TranslationEntry *tlb = machine->GetMMU()->tlb;
    for(unsigned i = 0; i < TLB_SIZE; i++)
    {
        tlb[i].valid = false;
    }
}

void
AddressSpace::SavePageFromTLB(unsigned page)
{
    DEBUG('p', "Saving tlb page in index %u\n", page);
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

TranslationEntry* 
AddressSpace::GetTranslationEntry(unsigned vpn)
{
    TranslationEntry *page = &pageTable[vpn];
    if(!page->valid)
    {
#ifdef SWAP
        if(inSwap->Test(vpn))
        {
            ReadFromSwap(vpn);
        }
        else
        {
            LoadPage(vpn);
        }
#else
        LoadPage(vpn);
#endif
    }
    return page;
}



void
AddressSpace::LoadPage(unsigned vpn)
{
    DEBUG('p', "Loading page %u\n", vpn);

#ifdef SWAP
    int frame = coreMap->Find(vpn, this);
#else
    int frame = usedPages->Find();
    ASSERT(frame != -1);
#endif

    DEBUG('p', "Physical page found\n");
    unsigned virtualAddr = vpn * PAGE_SIZE;
    pageTable[vpn].physicalPage = frame;
    pageTable[vpn].virtualPage  = vpn;
    pageTable[vpn].valid        = true;

    Executable exe (executable);
    ASSERT(exe.CheckMagic());

    uint32_t physicalAddr = frame * PAGE_SIZE;


    unsigned int pageEnd = virtualAddr + PAGE_SIZE;
    unsigned int codeEnd = codeAddr + codeSize;
    unsigned int dataEnd = initDataAddr + initDataSize;
    unsigned int uninitStart = codeSize + initDataSize;
    char *mainMemory = &(machine->GetMMU()->mainMemory[physicalAddr]);
    unsigned bytesRead = 0;
    if (pageEnd >= codeAddr && virtualAddr <= codeEnd)
    {
        unsigned int overlapStart = virtualAddr >= codeAddr ? virtualAddr : codeAddr;
        unsigned int overlapEnd = pageEnd < codeEnd ? pageEnd : codeEnd;
        if (overlapEnd - overlapStart)
        {
            exe.ReadCodeBlock(mainMemory, overlapEnd - overlapStart, overlapStart - codeAddr);
            mainMemory += overlapEnd - overlapStart;
            bytesRead += overlapEnd - overlapStart;
            pageTable[vpn].readOnly = true;
        }
    }
    if (pageEnd >= initDataAddr && virtualAddr <= dataEnd)
    {
        unsigned int overlapStart = virtualAddr >= initDataAddr ? virtualAddr : initDataAddr;
        unsigned int overlapEnd = pageEnd < dataEnd ? pageEnd : dataEnd;
        if (overlapEnd - overlapStart)
        {
            exe.ReadDataBlock(mainMemory, overlapEnd - overlapStart, overlapStart - initDataAddr);
            mainMemory += overlapEnd - overlapStart;
            bytesRead += overlapEnd - overlapStart;
            pageTable[vpn].readOnly = false;
        }
    }
    if (pageEnd > uninitStart)
    {
        unsigned int overlapStart = virtualAddr >= uninitStart ? virtualAddr : uninitStart;
        unsigned int size = pageEnd - overlapStart;
        size = size > PAGE_SIZE ? PAGE_SIZE : size;
        memset(mainMemory, 0, size);
        bytesRead += size;
        pageTable[vpn].readOnly = false;
    }
}

#ifdef SWAP
void
AddressSpace::ReadFromSwap(unsigned vpn) 
{
    char *mainMemory = machine->GetMMU()->mainMemory;
    unsigned physicalPage = coreMap->Find(vpn, this);
    DEBUG('p', "Cargando vpn %lu en la ppn %lu desde la swap\n", vpn, physicalPage);

    pageTable[vpn].valid = true;
    pageTable[vpn].dirty = false;
    pageTable[vpn].use = false;
    pageTable[vpn].physicalPage = physicalPage;

    unsigned physicalAddr = physicalPage * PAGE_SIZE;
    swapFile->ReadAt(mainMemory + physicalAddr, PAGE_SIZE, vpn * PAGE_SIZE);
    inSwap->Clear(vpn);
}

void
AddressSpace::WriteToSwap(unsigned vpn)
{
    char *mainMemory = machine->GetMMU()->mainMemory;
    unsigned physicalAddr = pageTable[vpn].physicalPage * PAGE_SIZE;
    pageTable[vpn].valid = false;

    if (pageTable[vpn].dirty) 
    {
        DEBUG('p', "Desalojando physical page number %lu con vpn %lu DIRTY\n", pageTable[vpn].physicalPage, vpn);
        ASSERT(swapFile->WriteAt(mainMemory + physicalAddr, PAGE_SIZE, vpn * PAGE_SIZE) == PAGE_SIZE);
        inSwap->Mark(vpn);
    } else {
        DEBUG('p', "Desalojando physical page number %lu con vpn %lu CLEAN\n", pageTable[vpn].physicalPage, vpn);
    }

    TranslationEntry *tlb = machine->GetMMU()->tlb;
    for (unsigned int i = 0; i < TLB_SIZE; i++)
    {
        if (tlb[i].valid && tlb[i].virtualPage == vpn)
        {
            tlb[i].valid = false;
            break;
        }
    }

}
#endif