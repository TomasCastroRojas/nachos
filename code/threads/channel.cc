#include "channel.hh"
#include "system.hh"

Channel::Channel(const char *debugName)
{
    name = debugName;
    buffer = nullptr;
    lock = new Lock("channel lock");
    condRecv = new Condition("receive condition", lock);
    condSend = new Condition("send condition", lock);
}

Channel::~Channel()
{
    delete lock;
    delete condRecv;
    delete condSend;
}

const char *
Channel::GetName() const
{
    return name;
}

void
Channel::Send(int message)
{
    lock->Acquire();

    while(buffer == nullptr)
        condRecv->Wait();
    *buffer = message;
    condSend->Signal();

    lock->Release();
}

void
Channel::Receive(int *message)
{
    lock->Acquire();

    while(buffer != nullptr)
        condSend->Wait();
    buffer = message;
    condRecv->Signal();
    condSend->Wait();
    buffer = nullptr;

    lock->Release();
}

