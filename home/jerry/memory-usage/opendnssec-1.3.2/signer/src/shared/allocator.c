/*
 * $Id: allocator.c 3817 2010-08-27 08:43:00Z matthijs $
 *
 * Copyright (c) 2010-2011 NLNet Labs. All rights reserved.
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
 *
 */

/**
 * Memory management.
 *
 */

#include "config.h"
#include "mm.h"
#include "shared/allocator.h"
#include "shared/log.h"

#include <stdlib.h>
#include <string.h>

static const char* allocator_str = "allocator";

#undef __STATIC_ALLOCATORS

#ifdef __STATIC_ALLOCATORS
#ifdef MM_MEMORY_TRACKING
allocator_type __allocator = {
		__mm_malloc,
		__mm_free
};
#else
allocator_type __allocator = {
		mm_malloc2,
		mm_free2
};
#endif
#endif

/**
 * Create allocator.
 *
 */
allocator_type*
#ifdef MM_MEMORY_TRACKING
__allocator_create(void *(*allocator)(size_t size), void (*deallocator)(void *), const char *file, int line)
#else
allocator_create(void *(*allocator)(size_t size), void (*deallocator)(void *))
#endif
{
#ifdef __STATIC_ALLOCATORS
	return &__allocator;
#else
    allocator_type* result =
        (allocator_type*) allocator(sizeof(allocator_type));
    if (!result) {
        ods_log_error("[%s] failed to create allocator", allocator_str);
        return NULL;
    }
    result->allocator = allocator;
    result->deallocator = deallocator;
    return result;
#endif
}


/**
 * Allocate memory.
 *
 */
void*
#ifdef MM_MEMORY_TRACKING
__allocator_alloc(allocator_type* allocator, size_t size, const char *file, int line)
#else
allocator_alloc(allocator_type* allocator, size_t size)
#endif
{
    void* result;

    ods_log_assert(allocator);
    /* align size */
    if (size == 0) {
        size = 1;
    }
#ifdef MM_MEMORY_TRACKING
    result = allocator->allocator(size, file, line);
#else
    result = allocator->allocator(size);
#endif
    if (!result) {
        ods_fatal_exit("[%s] allocator failed: out of memory", allocator_str);
        return NULL;
    }
    return result;
}


/**
 * Allocate memory and initialize to zero.
 *
 */
void*
#ifdef MM_MEMORY_TRACKING
__allocator_alloc_zero(allocator_type *allocator, size_t size, const char *file, int line)
#else
allocator_alloc_zero(allocator_type *allocator, size_t size)
#endif
{
#ifdef MM_MEMORY_TRACKING
    void *result = __allocator_alloc(allocator, size, file, line);
#else
    void *result = allocator_alloc(allocator, size);
#endif
    if (!result) {
        return NULL;
    }
    memset(result, 0, size);
    return result;
}


/**
 * Allocate memory and initialize with data.
 *
 */
void*
#ifdef MM_MEMORY_TRACKING
__allocator_alloc_init(allocator_type *allocator, size_t size, const void *init, const char *file, int line)
#else
allocator_alloc_init(allocator_type *allocator, size_t size, const void *init)
#endif
{
#ifdef MM_MEMORY_TRACKING
    void *result = __allocator_alloc(allocator, size, file, line);
#else
    void *result = allocator_alloc(allocator, size);
#endif
    if (!result) {
        return NULL;
    }
    memcpy(result, init, size);
    return result;
}


/**
 * Duplicate string.
 *
 */
char*
#ifdef MM_MEMORY_TRACKING
__allocator_strdup(allocator_type *allocator, const char *string, const char *file, int line)
#else
allocator_strdup(allocator_type *allocator, const char *string)
#endif
{
    if (!string) {
        return NULL;
    }
#ifdef MM_MEMORY_TRACKING
    return (char*) __allocator_alloc_init(allocator, strlen(string) + 1, string, file, line);
#else
    return (char*) allocator_alloc_init(allocator, strlen(string) + 1, string);
#endif
}


/**
 * Deallocate memory.
 *
 */
void
#ifdef MM_MEMORY_TRACKING
__allocator_deallocate(allocator_type *allocator, void* data, const char *file, int line)
#else
allocator_deallocate(allocator_type *allocator, void* data)
#endif
{
    ods_log_assert(allocator);

    if (!data) {
        return;
    }
#ifdef MM_MEMORY_TRACKING
    allocator->deallocator(data, file, line);
#else
    allocator->deallocator(data);
#endif
    return;
}


/**
 * Cleanup allocator.
 *
 */
void
#ifdef MM_MEMORY_TRACKING
__allocator_cleanup(allocator_type *allocator)
#else
allocator_cleanup(allocator_type *allocator)
#endif
{
#ifndef __STATIC_ALLOCATORS
    void (*deallocator)(void *);
    if (!allocator) {
        return;
    }
    deallocator = allocator->deallocator;
    deallocator(allocator);
#endif
    return;
}

