/// Entry points into the Nachos kernel from user programs.
///
/// There are two kinds of things that can cause control to transfer back to
/// here from user code:
///
/// * System calls: the user code explicitly requests to call a procedure in
///   the Nachos kernel.  Right now, the only function we support is `Halt`.
///
/// * Exceptions: the user code does something that the CPU cannot handle.
///   For instance, accessing memory that does not exist, arithmetic errors,
///   etc.
///
/// Interrupts (which can also cause control to transfer from user code into
/// the Nachos kernel) are handled elsewhere.
///
/// For now, this only handles the `Halt` system call.  Everything else core-
/// dumps.
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "transfer.hh"
#include "syscall.h"
#include "filesys/directory_entry.hh"
#include "threads/system.hh"
#include "args.hh"

#include <stdio.h>


void
InitProcess(void* args)
{
    currentThread->space->InitRegisters();  // Set the initial register values.
    currentThread->space->RestoreState();   // Load page table register.    

    if (args != nullptr) {
        unsigned argc = WriteArgs((char**)args);
        machine->WriteRegister(4, argc);

        int space = machine->ReadRegister(STACK_REG);
        machine->WriteRegister(5, space);
        machine->WriteRegister(STACK_REG, space - 24); // MIPS call convention
    }

    machine->Run();  // Jump to the user progam.
}

static void
IncrementPC()
{
    unsigned pc;

    pc = machine->ReadRegister(PC_REG);
    machine->WriteRegister(PREV_PC_REG, pc);
    pc = machine->ReadRegister(NEXT_PC_REG);
    machine->WriteRegister(PC_REG, pc);
    pc += 4;
    machine->WriteRegister(NEXT_PC_REG, pc);
}

/// Do some default behavior for an unexpected exception.
///
/// NOTE: this function is meant specifically for unexpected exceptions.  If
/// you implement a new behavior for some exception, do not extend this
/// function: assign a new handler instead.
///
/// * `et` is the kind of exception.  The list of possible exceptions is in
///   `machine/exception_type.hh`.
static void
DefaultHandler(ExceptionType et)
{
    int exceptionArg = machine->ReadRegister(2);

    fprintf(stderr, "Unexpected user mode exception: %s, arg %d.\n",
            ExceptionTypeToString(et), exceptionArg);
    ASSERT(false);
}

/// Handle a system call exception.
///
/// * `et` is the kind of exception.  The list of possible exceptions is in
///   `machine/exception_type.hh`.
///
/// The calling convention is the following:
///
/// * system call identifier in `r2`;
/// * 1st argument in `r4`;
/// * 2nd argument in `r5`;
/// * 3rd argument in `r6`;
/// * 4th argument in `r7`;
/// * the result of the system call, if any, must be put back into `r2`.
///
/// And do not forget to increment the program counter before returning. (Or
/// else you will loop making the same system call forever!)
static void
SyscallHandler(ExceptionType _et)
{
    int scid = machine->ReadRegister(2);

    switch (scid) {

        case SC_HALT:
            DEBUG('e', "Shutdown, initiated by user program.\n");
            interrupt->Halt();
            break;

        case SC_CREATE: {
            int filenameAddr = machine->ReadRegister(4);
            if (filenameAddr == 0) {
                DEBUG('e', "Error: address to filename string is null.\n");
                machine->WriteRegister(2, -1);
            }

            char filename[FILE_NAME_MAX_LEN + 1];
            if (!ReadStringFromUser(filenameAddr,
                                    filename, sizeof filename)) {
                DEBUG('e', "Error: filename string too long (maximum is %u bytes).\n",
                      FILE_NAME_MAX_LEN);
                machine->WriteRegister(2, -1);
            }

            DEBUG('e', "`Create` requested for file `%s`.\n", filename);
            if (!fileSystem->Create(filename, 0)) {
                DEBUG('e', "Error: Failed to create file %s\n", filename);
                machine->WriteRegister(2, -1);
                break;
            }

            DEBUG('e', "File '%s' created.\n", filename);
            machine->WriteRegister(2,0);
            break;
        }

        case SC_REMOVE: {
            int filenameAddr = machine->ReadRegister(4);
            if (filenameAddr == 0) {
                DEBUG('e', "Error: address to filename string is null.\n");
                machine->WriteRegister(2, -1);
            }

            char filename[FILE_NAME_MAX_LEN + 1];
            if (!ReadStringFromUser(filenameAddr,
                                    filename, sizeof filename)) {
                DEBUG('e', "Error: filename string too long (maximum is %u bytes).\n",
                      FILE_NAME_MAX_LEN);
                machine->WriteRegister(2, -1);
            }

            DEBUG('e', "`Remove` requested for file `%s`.\n", filename);
            if (!fileSystem->Remove(filename)) {
                DEBUG('e', "Error: Failed to remove file %s\n", filename);
                machine->WriteRegister(2, -1);
                break;
            }

            DEBUG('e', "File '%s' removed.\n", filename);
            machine->WriteRegister(2,0);
            break;
        }

        case SC_EXIT: {
            int status = machine->ReadRegister(4);
            DEBUG('e', "Thread '%s' exiting with status %d\n", currentThread->GetName(), status);
            currentThread->Finish(status);
            break;
        }

        case SC_OPEN: {
          int fileNameAddr = machine->ReadRegister(4);

          if (fileNameAddr == 0) {
             machine->WriteRegister(2, -1);
             DEBUG('e', "Error: address to filename string is null.\n");
             break;
          }

          char filename[FILE_NAME_MAX_LEN + 1];

          if (!ReadStringFromUser(fileNameAddr,filename,sizeof filename)) {
             machine->WriteRegister(2, -1);
             DEBUG('e', "Error: filename string too long (maximum is %u bytes).\n", FILE_NAME_MAX_LEN);
             break;
          }

          OpenFile *file = fileSystem->Open(filename);

          if (!file) {
            machine->WriteRegister(2, -1);
            DEBUG('e', "File not found\n");
            break;
          }

          OpenFileId fid = currentThread->filesTable->Add(file);

          if(fid == -1) {
            machine->WriteRegister(2, -1);
            DEBUG('e', "Error: no space left to open file.\n");
            break;
          }

          machine->WriteRegister(2, fid);
          break;
        }

        case SC_CLOSE: {
            int fid = machine->ReadRegister(4);
            if (fid<0){
              DEBUG('e', "File id not valid\n");
              break;
            }
            DEBUG('e', "`Close` requested for id %u.\n", fid);

            if (currentThread->filesTable->HasKey(fid)) {
              delete currentThread->filesTable->Remove(fid);
              DEBUG('e', "File id %u closed.\n", fid);
              machine->WriteRegister(2, 1);
            } else {
                DEBUG('e', "`Close` requested for id failed %u.\n", fid);
                machine->WriteRegister(2, -1);
                break;
            }
            break;
        }

        case SC_READ: {
          int buffer = machine->ReadRegister(4);
          int size = machine->ReadRegister(5);
          OpenFileId fid = machine->ReadRegister(6);
          int bytesRead = 0;

          if (size <= 0) {
            DEBUG('e',"Error: Invalid size.\n");
            machine->WriteRegister(2, -1);
            break;
          } 
          char bufferSys[size];
          if (fid == CONSOLE_OUTPUT || fid < 0) {
            DEBUG('e', "Error: Invalid file for reading.\n");
            machine->WriteRegister(2,-1);
            break;
          } else if(fid == CONSOLE_INPUT) {
                DEBUG('e', "Reading from console.\n");
                synchConsole->ReadBuffer(bufferSys, size);
                bytesRead = size;
                WriteBufferToUser(bufferSys, buffer, bytesRead);
            } else {
                if (currentThread->filesTable->HasKey(fid)) {
                    DEBUG('e', "Reading from file with id %u.\n", fid);
                    OpenFile *file = currentThread->filesTable->Get(fid);
                    bytesRead = file->Read(bufferSys, size);
                    if (bytesRead > 0) {
                        WriteBufferToUser(bufferSys, buffer, bytesRead);
                    }
            } else {
                DEBUG('e', "File with id %u does not exists.\n", fid);
                machine->WriteRegister(2, -1);
                break;
            }
          }
          machine->WriteRegister(2, bytesRead);

          break;
        }
        
        case SC_WRITE: {
          int buffer = machine->ReadRegister(4);
          int size = machine->ReadRegister(5);
          OpenFileId fid = machine->ReadRegister(6);
          int bytesWrite = 0;

          if (size <= 0) {
            DEBUG('e',"Error: Invalid size.\n");
            machine->WriteRegister(2, -1);
            break;
          } 
          char bufferSys[size];
          if (fid == CONSOLE_INPUT || fid < 0) {
            DEBUG('e', "Error: Invalid file for writing.\n");
            machine->WriteRegister(2,-1);
            break;
          } else if(fid == CONSOLE_OUTPUT) {
                DEBUG('e', "writing to console.\n");
                ReadBufferFromUser(buffer, bufferSys, size);
                synchConsole->WriteBuffer(bufferSys, size);
                bytesWrite = size;
          } else {
              if (currentThread->filesTable->HasKey(fid)){
                DEBUG('e', "Writing to file with id %u.\n", fid);
                OpenFile *file = currentThread->filesTable->Get(fid);
                ReadBufferFromUser(buffer, bufferSys, size);
                bytesWrite = file->Write(bufferSys, size);
              } else {
                DEBUG('e', "File with id %u does not exists.\n", fid);
                machine->WriteRegister(2, -1);
                break;
                }
            }
            machine->WriteRegister(2, bytesWrite);

            break;
        }

        case SC_JOIN: {
            SpaceId id = machine->ReadRegister(4);
            if(!runningThreads->HasKey(id)){
            //osea si no existe y entra aca es pq no se encontro un hilo creo
                DEBUG('e',"'Join' Error: No Thread.\n");
                machine->WriteRegister(2,-1);
            }
            DEBUG('e',"'Join' requested by thread %s. \n", currentThread->GetName());
            Thread *child = runningThreads->Get(id);
            int retVal = child->Join();
            machine->WriteRegister(2, retVal);

            break;
        }
        
        case SC_EXEC: {
            int fileNameAddr = machine->ReadRegister(4);
            bool joinable = machine->ReadRegister(5);
            int argvAddr = machine->ReadRegister(6)

            if (fileNameAddr == 0) {
                DEBUG('e', "'Exec' Error: fileNameAdrr is null\n");
                machine->WriteRegister(2, -1);
            }
            char *filename = new char[FILE_NAME_MAX_LEN + 1];
            if (!ReadStringFromUser(fileNameAddr, filename, FILE_NAME_MAX_LEN)) {
                DEBUG('e', "'Exec' Error: filename string too long (maximum is %u bytes).\n",
                      FILE_NAME_MAX_LEN);
                machine->WriteRegister(2, -1);
                break;
            }

            OpenFile *execFile = fileSystem->Open(filename);
            if (!execFile) {
                DEBUG('e', "'ExecÂ' Error: File %s not found.\n", filename);
                machine->WriteRegister(2, -1);
                break;
            }

            Thread* child = new Thread(filename, false, currentThread->GetPriority());
            child->space = new AddressSpace(execFile);

            char **argv = nullptr;
            if (!argvAddr) {
                argv = SaveArgs(argvAddr);
            }
            child->Fork(InitProcess, argv);

            machine->WriteRegister(2, child->spaceId);
            break;
        }

        case SC_STATE: {
            DEBUG('e',"Scheduler state.\n");
            scheduler->Print();
            break;
        }

        default:
            fprintf(stderr, "Unexpected system call: id %d.\n", scid);
            ASSERT(false);
    }

    IncrementPC();

}


/// By default, only system calls have their own handler.  All other
/// exception types are assigned the default handler.
void
SetExceptionHandlers()
{
    machine->SetHandler(NO_EXCEPTION,            &DefaultHandler);
    machine->SetHandler(SYSCALL_EXCEPTION,       &SyscallHandler);
    machine->SetHandler(PAGE_FAULT_EXCEPTION,    &DefaultHandler);
    machine->SetHandler(READ_ONLY_EXCEPTION,     &DefaultHandler);
    machine->SetHandler(BUS_ERROR_EXCEPTION,     &DefaultHandler);
    machine->SetHandler(ADDRESS_ERROR_EXCEPTION, &DefaultHandler);
    machine->SetHandler(OVERFLOW_EXCEPTION,      &DefaultHandler);
    machine->SetHandler(ILLEGAL_INSTR_EXCEPTION, &DefaultHandler);
}
