#include "coremap.hh"

Coremap::Coremap(unsigned size)
{
    framesMap = new Bitmap(size);
    virtualPages = new unsigned[size];
    spaces = new AddressSpace*[size];
    pagesQueue = new List<unsigned>;
}

Coremap::~Coremap()
{
    delete framesMap;
    delete [] spaces;
    delete [] virtualPages;
    delete pagesQueue;
}

int
Coremap::Find(unsigned vpn, AddressSpace *space)
{
    int page = framesMap->Find();
    if (page == -1)
    {
        page = PickVictim();
        spaces[page]->WriteToSwap(virtualPages[page]);
    }
    virtualPages[page] = vpn;
    spaces[page] = space;
    pagesQueue->Append(page);
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
    return pagesQueue->Pop();
}

void
Coremap::PageUsed(unsigned which)
{
    pagesQueue->Remove(which);
    pagesQueue->Append(which);
}