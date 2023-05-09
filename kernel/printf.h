#ifndef _PRINTF_H_
#define _PRINTF_H_

void            printf(char*, ...);
void            panic(char*) __attribute__((noreturn));
void            printfinit(void);

#endif