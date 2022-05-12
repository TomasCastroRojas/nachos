#include "syscall.h"

unsigned strlargo (const char *string) {
  unsigned len = 0;
  
  for(; string[len] != '\0'; ++len);
  
  return len;
}

void putS (const char *string ) {
  Write(string, strlargo(string), CONSOLE_OUTPUT);
}

 
// Function to reverse a string
void reverse_string(char *str, unsigned len) {
    char temp;
    for(unsigned i = 0; i < (len / 2); i++) {
        temp = str[i];
        str[i] = str[len - i - 1];
        str[len - i - 1] = temp;
    }
}

void itoa (int n, char *str) {
    int negative = 0;
    if (n < 0) {
        n *= -1;
        negative = 1;
    }    
    int i = 0, r;
    while (n)
    {
        r = n % 10;
        
        str[i++] = 48 + r;
 
        n = n / 10;
    }
    // if the number is 0
    if (i == 0) {
        str[i++] = '0';
    }

    if (negative)
        str[i++] = '-';
    
    str[i] = '\0';

    reverse_string(str, i);
}