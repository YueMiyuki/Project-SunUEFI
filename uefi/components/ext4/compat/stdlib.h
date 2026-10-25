// SPDX-License-Identifier: BSD-2-Clause-Patent
#pragma once
#include <stddef.h>
void *malloc(size_t);
void *calloc(size_t,size_t);
void *realloc(void *,size_t);
void free(void *);
void qsort(void *,size_t,size_t,int (*)(const void *,const void *));
