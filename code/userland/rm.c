#include "lib.c"
#include "syscall.h"

int main(int argc, char** argv) {
    if (argc != 2) {
        putS("Error: wrong amount of arguments.\n");
        Exit(-1);
    }

    if (Remove(argv[1]) == -1) {
        putS("Error: could not remove file.\n");
        Exit(-1);
    }

    return 0;
}