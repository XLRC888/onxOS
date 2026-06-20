#ifndef STRING_H
#define STRING_H
#include "kernel.h"
void memcpy(void *dest, const void *src, uint32_t n);
void memset(void *dest, uint8_t val, uint32_t n);
int strcmp(const char *a, const char *b);
int strncmp(const char *a, const char *b, uint32_t n);
char *strcpy(char *dest, const char *src);
char *strncpy(char *dest, const char *src, uint32_t n);
uint32_t strlen(const char *s);
char *strcat(char *dest, const char *src);
char *strchr(const char *s, int c);
char *strstr(const char *haystack, const char *needle);
char *itoa(int value, char *str, int base);
int atoi(const char *str, int *ok);
#endif
