#include "directory_list.hh"
#include "threads/lock.hh"

DirectoryList::DirectoryList()
{
    lock = new Lock("Read Write Lock");
    last = nullptr;
    first = nullptr;
}

DirectoryList::~DirectoryList()
{
    delete lock;
    DirListEntry* aux;
    for (aux = first; aux != nullptr; aux = first)
    {
        first = first->next;
        delete aux->dirLock;
        delete aux;
    }
}

void
DirectoryList::LockAcquire() {
    lock->Acquire();
}

void
DirectoryList::LockRelease() {
    lock->Release();
}

Lock* 
DirectoryList::OpenDirectory(int fSector){
    DirListEntry* aux;
    for (aux = first; aux != nullptr; aux = aux->next)
    {
        if (aux->sector == fSector)
            break;
    }

    if (!aux) {
        aux = new DirListEntry();
        aux->sector = fSector;
        aux->opened = 1;
        aux->next = nullptr;
        aux->dirLock = new Lock("dir lock");
        if (!first) {
            first = last = aux;
        } else {
            last->next = aux;
            last = aux;
        }
    } else {
        aux->opened++;
    }
    return aux->dirLock;
}

void 
DirectoryList::CloseDirectory(int fSector) {
    DirListEntry *aux, *father = nullptr;
    for (aux = first; aux != nullptr; father = aux, aux = aux->next)
    {
        if (aux->sector == fSector)
            break;
    }

    ASSERT(aux != nullptr);
    if (aux->opened > 1) {
        aux->opened--;
    } else {
        if (aux == first) {
            first = aux->next;
            if (aux == last)
                last = nullptr;
        } else {
            if (aux == last) {
                father->next = nullptr;
                last = father;
            } else
                father->next = aux->next;
        }

        delete aux->dirLock;
        delete aux;
    }
}

bool 
DirectoryList::CanRemove(int fSector) {
    DirListEntry* aux;
    for (aux = first; aux != nullptr; aux = aux->next) {
        if (aux->sector == fSector)
            break;
    }

    if (!aux) {
        return true;
    } else {
        return false;
    }
}