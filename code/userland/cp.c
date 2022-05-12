#include "lib.c"
#include "syscall.h"

int main (int argc, char** argv) {
    if (argc != 3) {
        putS("Error: wrong amount of arguments.\n");
        Exit(-1);
    }

    OpenFileId origen = Open(argv[1]);
    
    if (origen < 2) {
        putS("Error: could not open source file.\n");
        Exit(-1);
    }

    if (Create(argv[2]) == -1) {
        putS("Error: could not create destination file.\n");
        Exit(-1);
    }

    OpenFileId destino = Open(argv[2]);
    if (destino < 2) {
        putS("Error: could not open destination file.\n");
        Exit(-1);
    }
    char buffer[1];
    while(Read(buffer, 1, origen) > 0)
        Write(buffer, 1, destino);

    Close(origen);
    Close(destino);

    return 0;
}