#ifndef COMPILER_H
#define COMPILER_H

#include "stdint.h"

int buildc_compile(const char *src_path, const char *out_path, const char *flags);
int run_unr_file(const char *unr_path, void (*print_fn)(const char *str, uint32_t color));

#endif
