#ifndef NACHOS_DIRECTORY_LIST__HH
#define NACHOS_DIRECTORY_LIST__HH

class Lock;

struct DirListEntry {
    int sector;
    unsigned opened;
    Lock* dirLock;
    DirListEntry* next;
};

class DirectoryList {
public:

    DirectoryList();

    ~DirectoryList();

    void LockAcquire();
    void LockRelease();

    Lock* OpenDirectory(int fSector);

    void CloseDirectory(int fSector);

    bool CanRemove(int fSector);

private:
    Lock* lock;
    DirListEntry *first, *last;
};


#endif