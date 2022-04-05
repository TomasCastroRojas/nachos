/// Routines for synchronizing threads.
///
/// The implementation for this primitive does not come with base Nachos.
/// It is left to the student.
///
/// When implementing this module, keep in mind that any implementation of a
/// synchronization routine needs some primitive atomic operation.  The
/// semaphore implementation, for example, disables interrupts in order to
/// achieve this; another way could be leveraging an already existing
/// primitive.
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "condition.hh"


/// Dummy functions -- so we can compile our later assignments.
///
/// Note -- without a correct implementation of `Condition::Wait`, the test
/// case in the network assignment will not work!

Condition::Condition(const char *debugName, Lock *conditionLock)
{
    name = debugName;
    lock = conditionLock;
    queue = new List<Semaphore*>;
}

Condition::~Condition()
{
    delete queue;
}

const char *
Condition::GetName() const
{
    return name;
}

void
Condition::Wait()
{
    ASSERT(lock->IsHeldByCurrentThread());
    lock->Release();
    Semaphore *semWait = new Semaphore(name, 0);
    queue->Append(semWait);
    semWait->P();
    lock->Acquire();
}

void
Condition::Signal()
{
    ASSERT(lock->IsHeldByCurrentThread());
    if (!queue->IsEmpty()) 
    {
        Semaphore *semWoken = queue->Pop();
        semWoken->V();
        delete semWoken;
    }
}

void
Condition::Broadcast()
{
    ASSERT(lock->IsHeldByCurrentThread());
    while (!queue->IsEmpty()) 
    {
        Semaphore *semWoken = queue->Pop();
        semWoken->V();
        delete semWoken;
    }
}
