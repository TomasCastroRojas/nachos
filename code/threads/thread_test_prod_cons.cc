/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2007-2009 Universidad de Las Palmas de Gran Canaria.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "thread_test_prod_cons.hh"
#include "condition.hh"
#include "system.hh"

#include <stdio.h>

static const int CICLOS = 5;
static const int CAPACIDAD = 10;
int buffer = 0;
static const int N_PROD = 3;
static const int N_CONS = 2;
static const int N_PROD_CONS = N_PROD + N_CONS;
bool done[N_PROD_CONS];


Lock *lock = new Lock("lockProdCons");
Condition *condFull = new Condition("condition full", lock);
Condition *condEmpty = new Condition("condition empty", lock);

static void producer (void* arg)
{
    printf("[PRODUCER]: %s arrancando\n", currentThread->GetName());
    for (int i=0; i < CICLOS; i++) 
    {
        lock->Acquire();
        while (buffer == CAPACIDAD) 
        {
            printf ("[PRODUCER]: %s espera\n", currentThread->GetName());
            condFull->Wait();
        }
        printf("[PRODUCER]: %s produciendo\n", currentThread->GetName());
        buffer++;
        condEmpty->Signal();
        printf("[PRODUCER]: %s -- buffer = %d\n",currentThread->GetName(), buffer);
        lock->Release();
        currentThread->Yield();
    }
    int id = *(int*)arg;
    printf("[PRODUCER DONE]: productor %u\n", id);
    done[id] = true;
}

static void consumer (void* arg)
{
    printf("[CONSUMER]: %s arrancando\n", currentThread->GetName());
    for (int i=0; i < CICLOS; i++)
    {
        lock->Acquire();
        while (buffer == 0) 
        {
            printf ("[CONSUMER]: %s espera\n", currentThread->GetName());
            condEmpty->Wait();
        }
        printf("[CONSUMER]: %s consumiendo\n", currentThread->GetName());
        buffer--;
        condFull->Signal();
        printf("[CONSUMER]: %s -- buffer = %d\n",currentThread->GetName(), buffer);
        lock->Release();
        currentThread->Yield();
    }
    int id = *(int*)arg;
    printf("[CONSUMER DONE]: consumidor %u\n", id);
    done[id] = true;
}

void
ThreadTestProdCons()
{
    
    for (int i = 0; i < N_CONS; ++i) 
    {
        char *name = new char[64];
        sprintf(name, "Consumidor %u", i);
        int* n = new int;
        *n = i;
        Thread *tCons = new Thread(name);
        tCons->Fork(consumer, (void*)n);
    }
    for (int i = 0; i < N_PROD; ++i) 
    {
        char *name = new char[64];
        sprintf(name, "Productor %u", i);
        int* n = new int;
        *n = i + N_CONS;
        Thread *tProd = new Thread(name);
        tProd->Fork(producer, (void*)n);
    }


    for (unsigned i = 0; i < N_PROD_CONS; i++) {
        printf("[HARDCORE JOIN]: %u\n", i);
        while (!done[i]) {
            printf("[NOT DONE]: hilo %u\n", i);
            currentThread->Yield();
        }
    }
}
