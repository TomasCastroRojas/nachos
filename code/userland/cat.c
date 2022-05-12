#include "lib.c"
#include "syscall.h"

int main (int argc, char** argv) {
    if (argc != 2) {
        putS("Error: wrong amount of arguments.\n");
        Exit(-1);
    }

    OpenFileId fid = Open(argv[1]);

    if (fid < 2) {
        putS("Error: could not open the file.\n");
        Exit(-1);
    }

    char buffer[1];
    while (Read(buffer, 1, fid)) {
        Write(buffer, 1, CONSOLE_OUTPUT);
    }
    Write("\n", 1, CONSOLE_OUTPUT);

    Close(fid);
    return 0;
}