// Kernel/Source/string.c
#include "../Include/string.h"
#include "../Include/stdint.h"
int strlen(const char *str) {
    int len = 0;
    while (str[len]) len++;
    return len;
}

int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

char *strcpy(char *dest, const char *src) {
    char *d = dest;
    while (*src) {
        *d++ = *src++;
    }
    *d = 0;
    return dest;
}
void *memset(void *dest, int val, int n) {
    void *ret = dest;
    __asm__ volatile(
        "cld\n\t"
        "rep stosb"
        : "+D"(dest), "+c"(n)
        : "a"((unsigned char)val)
        : "memory"
    );
    return ret;
}

void *memcpy(void *dest, const void *src, int n) {
    void *ret = dest;
    uint64_t qwords = n / 8; // Số khối 8-byte
    uint64_t bytes = n % 8;  // Số byte lẻ còn lại nếu có

    __asm__ volatile(
        "cld\n\t"
        "rep movsq\n\t"      // Đẩy một phát 8 bytes (64-bit) cực nhanh
        "movq %3, %%rcx\n\t" // Nạp số byte lẻ vào rcx
        "rep movsb"          // Copy nốt vài byte lẻ cuối cùng nếu có
        : "+D"(dest), "+S"(src), "+c"(qwords)
        : "r"(bytes)
        : "memory"
    );
    return ret;
}


int strncmp(const char *s1, const char *s2, int n) {
    for (int i = 0; i < n; i++) {
        if (s1[i] != s2[i] || s1[i] == 0) {
            return (unsigned char)s1[i] - (unsigned char)s2[i];
        }
    }
    return 0;
}

int memcmp(const void *a, const void *b, int n) {
    const unsigned char *pa = (const unsigned char*)a;
    const unsigned char *pb = (const unsigned char*)b;
    for (int i = 0; i < n; i++) {
        if (pa[i] != pb[i]) return pa[i] - pb[i];
    }
    return 0;
}

char to_upper(char c) {
    if (c >= 'a' && c <= 'z') return c - ('a' - 'A');
    return c;
}

// ============================================
// 🔥 THÊM 3 HÀM MỚI CHO SHELL
// ============================================

char *strcat(char *dest, const char *src) {
    char *d = dest;
    while (*d) d++;          // Tìm đến cuối dest
    while (*src) {
        *d++ = *src++;
    }
    *d = 0;
    return dest;
}

char *strchr(const char *s, int c) {
    while (*s) {
        if (*s == c) return (char*)s;
        s++;
    }
    return NULL;
}

char *strtok(char *str, const char *delim) {
    static char *next = NULL;
    char *token;

    if (str != NULL) {
        next = str;
    }

    if (next == NULL) {
        return NULL;
    }

    // Bỏ qua các ký tự phân cách ở đầu
    while (*next && strchr(delim, *next)) {
        next++;
    }

    if (*next == 0) {
        next = NULL;
        return NULL;
    }

    token = next;

    // Tìm vị trí kết thúc token
    while (*next && !strchr(delim, *next)) {
        next++;
    }

    if (*next) {
        *next = 0;
        next++;
    } else {
        next = NULL;
    }

    return token;
}