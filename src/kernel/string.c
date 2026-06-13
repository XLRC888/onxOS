#include "string.h"
void memcpy(void *dest, const void *src, uint32_t n) {
    uint8_t *d = dest; const uint8_t *s = src;
    for (uint32_t i = 0; i < n; i++) d[i] = s[i];
}
void memset(void *dest, uint8_t val, uint32_t n) {
    uint8_t *d = dest;
    for (uint32_t i = 0; i < n; i++) d[i] = val;
}
int strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return (uint8_t)*a - (uint8_t)*b;
}
int strncmp(const char *a, const char *b, uint32_t n) {
    for (uint32_t i = 0; i < n; i++) {
        if (a[i] != b[i]) return (uint8_t)a[i] - (uint8_t)b[i];
        if (!a[i]) return 0;
    }
    return 0;
}
char *strcpy(char *dest, const char *src) {
    char *d = dest;
    while (*src) *d++ = *src++;
    *d = 0;
    return dest;
}
char *strncpy(char *dest, const char *src, uint32_t n) {
    uint32_t i;
    for (i = 0; i < n && src[i]; i++) dest[i] = src[i];
    for (; i < n; i++) dest[i] = 0;
    return dest;
}
uint32_t strlen(const char *s) {
    uint32_t len = 0;
    while (s[len]) len++;
    return len;
}
char *strcat(char *dest, const char *src) {
    char *d = dest + strlen(dest);
    while (*src) *d++ = *src++;
    *d = 0;
    return dest;
}
char *strstr(const char *haystack, const char *needle) {
    if (!*needle) return (char *)haystack;
    for (; *haystack; haystack++) {
        const char *h = haystack, *n = needle;
        while (*h && *n && *h == *n) { h++; n++; }
        if (!*n) return (char *)haystack;
    }
    return 0;
}
char *strchr(const char *s, int c) {
    while (*s) {
        if (*s == (char)c) return (char *)s;
        s++;
    }
    return 0;
}
char *itoa(int value, char *str, int base) {
    char *p = str;
    int num = value;
    if (value == 0) { *p++ = '0'; *p = 0; return str; }
    if (base == 10 && value < 0) { *p++ = '-'; num = -value; }
    char temp[32]; int i = 0;
    while (num > 0) {
        int d = num % base;
        temp[i++] = d < 10 ? '0' + d : 'a' + d - 10;
        num /= base;
    }
    while (i > 0) *p++ = temp[--i];
    *p = 0;
    return str;
}
int atoi(const char *str) {
    int res = 0, sign = 1, i = 0;
    while (str[i] == ' ') i++;
    if (str[i] == '-') { sign = -1; i++; }
    else if (str[i] == '+') i++;
    while (str[i] >= '0' && str[i] <= '9') {
        res = res * 10 + (str[i] - '0');
        i++;
    }
    return sign * res;
}
