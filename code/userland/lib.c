#include "syscall.h"

unsigned strlen (const char *string) {
  unsigned len = 0;
  
  for(; string[len] != '\0'; ++len);
  
  return len;
}

void puts (const char *string ) {
  Write(string, strlen(string), CONSOLE_OUTPUT);
  Write("\n", 1, CONSOLE_OUTPUT);
}

void itoa (int n, char *string) {

}