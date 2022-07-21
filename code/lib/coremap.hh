/// A data structure to handle page swapping

#ifndef NACHOS_LIB_COREMAP_HH
#define NACHOS_LIB_COREMAP_HH

#include "bitmap.hh"
#include "list.hh"
#include "userprog/address_space.hh"

class Coremap {

public:
    Coremap(unsigned size);

    ~Coremap();

    int Find(unsigned vpn, AddressSpace* space);

    void Clear(unsigned which);

    int CountClear();

    void PageUsed(unsigned which);


private:
    Bitmap* framesMap;

    unsigned *virtualPages;

    AddressSpace **spaces;

    unsigned mapSize;

    List <unsigned> *pagesQueue;

    unsigned PickVictim();

};
#endif