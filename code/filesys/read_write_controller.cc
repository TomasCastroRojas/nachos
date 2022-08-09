#include "read_write_controller.hh"

ReadWriteController::ReadWriteController(){
    ReadCounterLock = new Lock("ReadCounterLock");
    NoReaders = new Condition("ReadWriteController CondVar", ReadCounterLock);
    readCounter = 0;
}

ReadWriteController::~ReadWriteController(){
    delete NoReaders;
    delete ReadCounterLock;
}

void
ReadWriteController::AcquireRead(){
    if(not ReadCounterLock -> IsHeldByCurrentThread()){
        ReadCounterLock -> Acquire();

        readCounter++;

        ReadCounterLock -> Release();
    }
}

void
ReadWriteController::ReleaseRead(){
    if(not ReadCounterLock -> IsHeldByCurrentThread()){
        ReadCounterLock -> Acquire();

        readCounter--;

        if(readCounter == 0)
            NoReaders -> Broadcast();

        ReadCounterLock -> Release();
    }
}

void
ReadWriteController::AcquireWrite(){
    ReadCounterLock -> Acquire();

    while(readCounter > 0)
        NoReaders -> Wait();

}

void
ReadWriteController::ReleaseWrite(){
    NoReaders -> Signal();
    ReadCounterLock -> Release();
}