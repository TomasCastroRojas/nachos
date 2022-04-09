#include "channel.hh"

Channel::Channel(const char *debugName)
{
    name = debugName;
    buffer = nullptr;
    lockSend = new Lock("send lock");
    lockRecv = new Lock("receive lock");
    semSend = new Semaphore("semaphore send", 0);
    semRecv = new Semaphore("semaphore recv", 0);
}

Channel::~Channel()
{
    delete lockSend;
    delete lockRecv;
    delete semSend;
    delete semRecv;
}

void
Channel::Send(int message)
{
    lockSend->Acquire();
    semSend->P();
    ASSERT(buffer != nullptr);
    *buffer = message;
    semRecv->V();
    lockSend->Release();
}

void
Channel::Receive(int *message) 
{
    lockRecv->Acquire();
    ASSERT (buffer == nullptr);
    buffer = message;
    semSend->V();
    semRecv->P();
    buffer = nullptr;
    lockRecv->Release();
}