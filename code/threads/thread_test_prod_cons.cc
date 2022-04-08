/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2007-2009 Universidad de Las Palmas de Gran Canaria.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "thread_test_prod_cons.hh"
#include "condition.hh"
#include "system.hh"

#include <stdio.h>
#include <string.h>

static const int items = 10;
int buffer = 0;

Lock *lock = new Lock("lockProdCons");
Condition *condFull = new Condition("condition full", lock);
Condition *condEmpty = new Condition("condition empty", lock);

void producer (void* arg)
{
    printf("%s producer arrancando\n", currentThread->GetName());
    while (1) {
        lock->Acquire();
        while (buffer == items) {
            condFull->Wait();
        }
        printf("%s produciendo\n", currentThread->GetName());
        buffer++;
        condEmpty->Signal();
        lock->Release();
        currentThread->Yield();
    }
}

void consumer (void* arg)
{
    printf("%s consumer arrancando\n", currentThread->GetName());
    while (1) {
        lock->Acquire();
        while (buffer == 0) {
            condEmpty->Wait();
        }
        printf("%s consumiendo\n", currentThread->GetName());
        buffer--;
        condFull->Signal();
        lock->Release();
        currentThread->Yield();
    }
}

void
ThreadTestProdCons()
{
    Thread *tProd = new Thread("hilo prod");
    tProd->Fork(producer, nullptr);

    Thread *tCons = new Thread("hilo cons");
    tCons->Fork(consumer, nullptr);
}
