#ifndef FILESYS_READWRITECONTROLLER__HH
#define FILESYS_READWRITECONTROLLER__HH

#include "threads/lock.hh"
#include "threads/condition.hh"

//class Lock;
//class Condition;

class ReadWriteController {
public:
    ReadWriteController();
    ~ReadWriteController();
    void AcquireRead();
    void ReleaseRead();
    void AcquireWrite();
    void ReleaseWrite();
private:
    Lock *ReadCounterLock;
    Condition *NoReaders;
    int readCounter;
};

#endif