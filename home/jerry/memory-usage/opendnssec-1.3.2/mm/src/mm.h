/* $Id$ */

/*
 * Copyright (c) 2011 .SE (The Internet Infrastructure Foundation).
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#undef MM_MEMORY_TRACKING
#define MM_MEMORY_ALLOC

#ifndef OPENDNSSEC_MM_H
#define OPENDNSSEC_MM_H 1

#include <stdlib.h>
#include <stdio.h>

#ifdef MM_MEMORY_TRACKING

struct mm_taginfo
{
	unsigned long num_allocations;
	unsigned long num_allocations_count;
	unsigned long num_allocations_max;

	unsigned long num_frees_count;

	unsigned long num_allocated_bytes;
	unsigned long num_allocated_bytes_max;
};
#define MM_TAGINFO_STATIC_NEW { 0L, 0L, 0L, 0L, 0L, 0L }

#endif

#ifdef MM_MEMORY_ALLOC

typedef struct mm_alloc_struct mm_alloc_t;
struct mm_alloc_struct
{
	void *next;
	size_t size;
#ifdef MM_MEMORY_TRACKING
	struct mm_taginfo info;
#endif
};

#ifdef MM_MEMORY_TRACKING
#define MM_ALLOC_T_STATIC_NEW(x) { 0L, x, MM_TAGINFO_STATIC_NEW }
#else
#define MM_ALLOC_T_STATIC_NEW(x) { 0L, x }
#endif

void *__mm_alloc_new(mm_alloc_t *alloc, volatile const char *file, volatile int line);
void __mm_alloc_delete(mm_alloc_t *alloc, void *, volatile const char *file, volatile int line);

#define mm_alloc_new(x) __mm_alloc_new(x, __FILE__, __LINE__)
#define mm_alloc_delete(x, y) __mm_alloc_delete(x, y, __FILE__, __LINE__)

#endif

#ifdef MM_MEMORY_TRACKING

struct mm_meminfo
{
	unsigned long num_mm_allocations;
	unsigned long num_mm_allocations_count;
	unsigned long num_mm_allocations_max;

	unsigned long num_mm_frees_count;

	unsigned long num_mm_allocated_bytes;
	unsigned long num_mm_allocated_bytes_max;

	unsigned long num_allocations;
	unsigned long num_allocations_count;
	unsigned long num_allocations_max;

	unsigned long num_frees_count;

	unsigned long num_allocated_bytes;
	unsigned long num_allocated_bytes_max;
};

void *__mm_malloc(size_t size, volatile const char *file, volatile int line);
void *__mm_calloc(size_t nmemb, size_t size, volatile const char *file, volatile int line);
void *__mm_realloc(void *ptr, size_t size, volatile const char *file, volatile int line);
void __mm_free(void *ptr, volatile const char *file, volatile int line);
char *__mm_strdup(const char *ptr, volatile const char *file, volatile int line);

void *mm_malloc2(size_t size);
void *mm_calloc2(size_t nmemb, size_t size);
void *mm_realloc2(void *ptr, size_t size);
void mm_free2(void *ptr);
char *mm_strdup2(const char *ptr);

void *mm_libxml_malloc(size_t size);
void *mm_libxml_calloc(size_t nmemb, size_t size);
void *mm_libxml_realloc(void *ptr, size_t size);
void mm_libxml_free(void *ptr);
char *mm_libxml_strdup(const char *ptr);

void mm_init(void);
const struct mm_meminfo *mm_get_info(void);
typedef void (*mm_detailed_info_callback_t)(const char *, const struct mm_taginfo *, void *);
void mm_get_detailed_info(mm_detailed_info_callback_t func, void *data);

#define mm_malloc(x) __mm_malloc(x, __FILE__, __LINE__)
#define mm_calloc(x, y) __mm_calloc(x, y, __FILE__, __LINE__)
#define mm_realloc(x, y) __mm_realloc(x, y, __FILE__, __LINE__)
#define mm_free(x) __mm_free(x, __FILE__, __LINE__)
#define mm_strdup(x) __mm_strdup(x, __FILE__, __LINE__)

#else

#define mm_malloc(x) malloc(x)
#define mm_calloc(x, y) calloc(x, y)
#define mm_realloc(x, y) realloc(x, y)
#define mm_free(x) free(x)
#define mm_strdup(x) strdup(x)

#define mm_malloc2 malloc
#define mm_calloc2 calloc
#define mm_realloc2 realloc
#define mm_free2 free
#define mm_strdup2 strdup

#endif

#endif /* OPENDNSSEC_MM_H */
