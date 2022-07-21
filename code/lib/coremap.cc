#include "coremap.hh"
#include "stdlib.h"
#include "time.h"

Coremap::Coremap(unsigned size)
{
    srand(time(NULL));
    framesMap = new Bitmap(size);
    virtualPages = new unsigned[size];
    spaces = new AddressSpace*[size];
    mapSize = size;
    #if defined(POLICY_FIFO) || defined(POLICY_LRU)
    pagesQueue = new List<unsigned>;
    #endif
}

Coremap::~Coremap()
{
    delete framesMap;
    delete [] spaces;
    delete [] virtualPages;
    #if defined(FIFO) || defined(LRU)
    delete pagesQueue;
    #endif
}

int
Coremap::Find(unsigned vpn, AddressSpace *space)
{
    DEBUG('p', "%u finding physical frame\n", vpn);
    int page = framesMap->Find();
    #ifdef SWAP
    if (page == -1)
    {
        page = PickVictim();
        DEBUG('p', "Frames full. Page %u picked victim\n", page);
        spaces[page]->WriteToSwap(virtualPages[page]);
    }
    virtualPages[page] = vpn;
    spaces[page] = space;
    #if defined(FIFO) || defined(LRU)
    pagesQueue->Append(page);
    #endif
    #endif
    return page;
}

void
Coremap::Clear(unsigned which)
{
    framesMap->Clear(which);
    spaces[which] = nullptr;
}

int
Coremap::CountClear()
{
    return framesMap->CountClear();
}

unsigned
Coremap::PickVictim()
{
    #if defined(FIFO) || defined(LRU)
    return pagesQueue->Pop();
    #else
    return rand() % mapSize;
    #endif
}

void
Coremap::PageUsed(unsigned which)
{
    pagesQueue->Remove(which);
    pagesQueue->Append(which);
}