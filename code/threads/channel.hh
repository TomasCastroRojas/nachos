/// Channels
///
/// A data structure for synchronizing threads.
///
/// All synchronization objects have a `name` parameter in the constructor;
/// its only aim is to ease debugging the program.
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2000      José Miguel Santos Espino - ULPGC.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.

#ifndef NACHOS_THREADS_CHANNEL__HH
#define NACHOS_THREADS_CHANNEL__HH


#include "condition.hh"

class Channel {
public:

    Channel(const char *debugName);

    ~Channel();

    const char* GetName() const;

    void Send(int message);
    void Receive(int *message);

private:
    const char *name;

    int *buffer;
    Lock *lock;
    Condition *condRecv, *condSend;;
};

#endif