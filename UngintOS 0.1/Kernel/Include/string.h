// Kernel/Include/string.h
#ifndef STRING_H
#define STRING_H

// Hàm có sẵn
int strlen(const char *str);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, int n);
char *strcpy(char *dest, const char *src);
void *memset(void *dest, int val, int n);
void *memcpy(void *dest, const void *src, int n);
int memcmp(const void *a, const void *b, int n);
char to_upper(char c);

// 🔥 THÊM 3 HÀM MỚI
char *strcat(char *dest, const char *src);
char *strchr(const char *s, int c);
char *strtok(char *str, const char *delim);

// 🔥 THÊM NULL
#define NULL ((void*)0)

#endif