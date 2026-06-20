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
    for (;; s++) {
        if (*s == (char)c) return (char *)s;
        if (!*s) return 0;
    }
}
char *itoa(int value, char *str, int base) {
    char *p = str;
    if (base < 2 || base > 36) { *p = 0; return str; }
    if (value == 0) { *p++ = '0'; *p = 0; return str; }
    unsigned int num;
    if (base == 10 && value < 0) { *p++ = '-'; num = -(unsigned int)value; }
    else num = (unsigned int)value;
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
int atoi(const char *str, int *ok) {
    int res = 0, sign = 1, i = 0, cnt = 0;
    while (str[i] == ' ') i++;
    if (str[i] == '-') { sign = -1; i++; }
    else if (str[i] == '+') i++;
    while (str[i] >= '0' && str[i] <= '9') {
        int d = str[i] - '0'; cnt++;
        if (res > 2147483647 / 10 || (res == 2147483647 / 10 && d > 2147483647 % 10))
            { if (ok) *ok = 1; return sign == 1 ? 2147483647 : -2147483648; }
        res = res * 10 + d;
        i++;
    }
    if (ok) *ok = cnt > 0;
    return cnt > 0 ? sign * res : 0;
}
