/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2007-2009 Universidad de Las Palmas de Gran Canaria.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "thread_test_garden.hh"
#include "system.hh"
#include "semaphore.hh"

#include <stdio.h>


static const unsigned NUM_TURNSTILES = 2;
static const unsigned ITERATIONS_PER_TURNSTILE = 5;
static int count;
Semaphore *s = new Semaphore("Sem jardin", 1);

static void
Turnstile(void *n_)
{
    unsigned *n = (unsigned *) n_;

    for (unsigned i = 0; i < ITERATIONS_PER_TURNSTILE; i++) {
        int temp = count;
        count = temp + 1;
        currentThread->Yield();
    }
    printf("Turnstile %u finished. Count is now %u.\n", *n, count);
    delete n;
}

void
ThreadTestGarden()
{
    List<Thread*> *turnstiles = new List<Thread*>;
    // Launch a new thread for each turnstile.
    for (unsigned i = 0; i < NUM_TURNSTILES; i++) {
        printf("Launching turnstile %u.\n", i);
        char *name = new char [16];
        sprintf(name, "Turnstile %u", i);
        unsigned *n = new unsigned;
        *n = i;
        Thread *t = new Thread(name, true, 0);
        t->Fork(Turnstile, (void *) n);
        turnstiles->Append(t);
    }

    Thread *t;
    while(!turnstiles->IsEmpty())
    {
        t = turnstiles->Pop();
        t->Join();
    }

    printf("All turnstiles finished. Final count is %u (should be %u).\n",
           count, ITERATIONS_PER_TURNSTILE * NUM_TURNSTILES);
}

static void
TurnstileSem(void *n_)
{
    unsigned *n = (unsigned *) n_;

    for (unsigned i = 0; i < ITERATIONS_PER_TURNSTILE; i++) {
        s->P();
        int temp = count;
        currentThread->Yield();
        count = temp + 1;
        s->V(); 
    }
    printf("Turnstile %u finished. Count is now %u.\n", *n, count);
    delete n;
}

void
ThreadTestGardenSem()
{
    List<Thread*> *turnstiles = new List<Thread*>;
    // Launch a new thread for each turnstile.
    for (unsigned i = 0; i < NUM_TURNSTILES; i++) {
        printf("Launching turnstile %u.\n", i);
        char *name = new char [16];
        sprintf(name, "Turnstile %u", i);
        unsigned *n = new unsigned;
        *n = i;
        Thread *t = new Thread(name, true, 0);
        t->Fork(TurnstileSem, (void *) n);
        turnstiles->Append(t);
    }

    Thread *t;
    while(!turnstiles->IsEmpty())
    {
        t = turnstiles->Pop();
        t->Join();
    }
    
    printf("All turnstiles finished. Final count is %u (should be %u).\n",
           count, ITERATIONS_PER_TURNSTILE * NUM_TURNSTILES);
}