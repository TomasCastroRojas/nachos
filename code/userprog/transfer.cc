/// Copyright (c) 2019-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "transfer.hh"
#include "lib/utility.hh"
#include "threads/system.hh"

#ifdef VMEM
static const int MAX_MEM_TRIES = 3;
#else
static const int MAX_MEM_TRIES = 1;
#endif


void ReadBufferFromUser(int userAddress, char *outBuffer,
                        unsigned byteCount)
{
    ASSERT(userAddress != 0);
    ASSERT(outBuffer != nullptr);
    ASSERT(byteCount != 0);

    unsigned count = 0;
    do {
        int temp;
        count++;
        bool valid = false;
        for (int tries = 0; tries < MAX_MEM_TRIES && !valid; tries++)
        {
            valid = machine->ReadMem(userAddress, 1, &temp);
        }
        ASSERT(valid);
        userAddress++;
        *outBuffer++ = (unsigned char) temp;
    } while (count < byteCount);


}

bool ReadStringFromUser(int userAddress, char *outString,
                        unsigned maxByteCount)
{
    ASSERT(userAddress != 0);
    ASSERT(outString != nullptr);
    ASSERT(maxByteCount != 0);

    unsigned count = 0;
    do {
        int temp;
        count++;
        bool valid = false;
        for (int tries = 0; tries < MAX_MEM_TRIES && !valid; tries++)
        {
            valid = machine->ReadMem(userAddress, 1, &temp);
        }
        ASSERT(valid);
        userAddress++;
        *outString = (unsigned char) temp;
    } while (*outString++ != '\0' && count < maxByteCount);
    return *(outString - 1) == '\0';
}

void WriteBufferToUser(const char *buffer, int userAddress,
                       unsigned byteCount)
{
    ASSERT(userAddress != 0);
    ASSERT(buffer != nullptr);
    ASSERT(byteCount != 0);

    unsigned count = 0;
    do {
        int temp;
        temp = (int)(buffer[count++]);
        bool valid = false;
        for (int tries = 0; tries < MAX_MEM_TRIES && !valid; tries++)
        {
            valid = machine->WriteMem(userAddress, 1, temp);
        }
        ASSERT(valid);
        userAddress++;
    } while (count < byteCount);
}

void WriteStringToUser(const char *string, int userAddress)
{
    ASSERT(userAddress != 0);
    ASSERT(string != nullptr);

    do {
        int temp = (int) *string;
        bool valid = false;
        for (int tries = 0; tries < MAX_MEM_TRIES && !valid; tries++)
        {
            valid = machine->WriteMem(userAddress, 1, temp);
        }
        ASSERT(valid);
        userAddress++;
    } while (*string++ != '\0');
}
