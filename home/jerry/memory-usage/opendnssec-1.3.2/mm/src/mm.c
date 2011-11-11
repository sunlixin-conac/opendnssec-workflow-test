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

#include "config.h"

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/types.h>

#include "mm.h"

#undef MM_DEBUG

#define hashsize(n) ((uint32_t)1<<(n))
#define hashmask(n) (hashsize(n)-1)

#if defined MM_MEMORY_TRACKING || defined MM_MEMORY_ALLOC

pthread_mutex_t __mm_memory_lock = PTHREAD_MUTEX_INITIALIZER;

#endif

#ifdef MM_MEMORY_TRACKING

struct mm_meminfo __mm_info = {
		0L, 0L, 0L,
		0L, 0L, 0L,
		0L, 0L, 0L,
		0L, 0L, 0L
};

#if SIZEOF_VOIDP == 4
typedef uint32_t __hash_t;
__hash_t __hash_voidp(void *ptr)
{
    register __hash_t hash = (__hash_t)ptr;
    hash = ~hash + (hash << 15);
    hash = hash ^ (hash >> 12);
    hash = hash + (hash << 2);
    hash = hash ^ (hash >> 4);
    hash = hash * 2057;
    hash = hash ^ (hash >> 16);
    return hash;
}
#elif SIZEOF_VOIDP == 8
typedef uint64_t __hash_t;
__hash_t __hash_voidp(void *ptr)
{
	register __hash_t hash = (__hash_t)ptr;
	hash = (~hash) + (hash << 21);
	hash = hash ^ (hash >> 24);
	hash = (hash + (hash << 3)) + (hash << 8);
	hash = hash ^ (hash >> 14);
	hash = (hash + (hash << 2)) + (hash << 4);
	hash = hash ^ (hash >> 28);
	hash = hash + (hash << 31);
	return hash;
}
#endif

uint32_t __hash_fnv1(register const char *key)
{
	register uint32_t hash = 2166136261;
	for (; *key; key++)
		hash = (16777619 * hash) ^ (*key);
	return hash;
}

FILE *__alloc_stdout;
#define __alloc_stdout_init \
	if (!__alloc_stdout) { \
		int fd = open("log", O_RDWR|O_APPEND|O_CREAT, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH); \
		__alloc_stdout = fdopen(dup2(fd, 666), "w+"); \
		close(fd); \
	}

struct __alloc_tag
{
	struct __alloc_tag *next;
	char tag[128];
	size_t size;
	struct mm_taginfo info;
};

#define __alloc_tag_hash_size hashsize(8)
#define __alloc_tag_hash_mask hashmask(8)

struct __alloc_tag *__alloc_tag_free = 0L;
struct __alloc_tag *__alloc_tag_hash[__alloc_tag_hash_size];

void __alloc_tag_delete(struct __alloc_tag *ptr)
{
	ptr->next = __alloc_tag_free;
	__alloc_tag_free = ptr;
	__mm_info.num_mm_frees_count++;
	__mm_info.num_mm_allocations--;
}

#define __alloc_tag_size 65536

void *__alloc_tag_new(void)
{
	struct __alloc_tag *ptr;

	if (!__alloc_tag_free) {
		unsigned int i;
		struct __alloc_tag *batch;

		if (!(batch = malloc(__alloc_tag_size))) {
			exit(-1);
		}

		__mm_info.num_mm_allocated_bytes += __alloc_tag_size;
		/*
		if (__mm_info.num_mm_allocated_bytes > __mm_info.num_mm_allocated_bytes_max) {
			__mm_info.num_mm_allocated_bytes_max = __mm_info.num_mm_allocated_bytes;
		}
		*/

		for (i=0; i<(__alloc_tag_size / sizeof(struct __alloc_tag)); i++) {
			batch->next = __alloc_tag_free;
			__alloc_tag_free = batch++;
		}
	}

	ptr = __alloc_tag_free;
	__alloc_tag_free = ptr->next;
	ptr->next = 0L;
	__mm_info.num_mm_allocations++;
	__mm_info.num_mm_allocations_count++;

	ptr->info.num_allocations = 0;
	ptr->info.num_allocations_count = 0;
	ptr->info.num_allocations_max = 0;
	ptr->info.num_frees_count = 0;
	ptr->info.num_allocated_bytes = 0;
	ptr->info.num_allocated_bytes_max = 0;

	return ptr;
}

void __alloc_tag_hash_add(struct __alloc_tag *tag)
{
	struct __alloc_tag **tag_ptr = &__alloc_tag_hash[__hash_fnv1(tag->tag) & __alloc_tag_hash_mask];

	if (*tag_ptr) {
		tag->next = *tag_ptr;
		*tag_ptr = tag;
	}
	else {
		*tag_ptr = tag;
	}
}

void __alloc_tag_hash_remove(struct __alloc_tag *tag)
{
	struct __alloc_tag **tag_ptr = &__alloc_tag_hash[__hash_fnv1(tag->tag) & __alloc_tag_hash_mask];

	if (*tag_ptr) {
		struct __alloc_tag *prev = *tag_ptr;

		if ((*tag_ptr)->size == tag->size && strncmp((*tag_ptr)->tag, tag->tag, tag->size) == 0) {
			*tag_ptr = tag->next;
			tag->next = 0;
			return;
		}

		while (prev) {
			if (prev->next->size == tag->size && strncmp(prev->next->tag, tag->tag, tag->size) == 0) {
				prev->next = prev->next->next;
				tag->next = 0;
				return;
			}
			prev = prev->next;
		}
	}

	tag = tag->next;
}

struct __alloc_tag *__alloc_tag_hash_find(const char *tag, size_t size)
{
	struct __alloc_tag *tag_ptr = __alloc_tag_hash[__hash_fnv1(tag) & __alloc_tag_hash_mask];

	while (tag_ptr) {
		if (tag_ptr->size == size && strncmp(tag_ptr->tag, tag, size) == 0) {
			return tag_ptr;
		}
		tag_ptr = tag_ptr->next;
	}

	return 0L;
}

struct __alloc_desc
{
	struct __alloc_desc *next;
	void *ptr;
	size_t size;
	struct __alloc_tag *tag;
};

#define __alloc_desc_hash_size hashsize(14)
#define __alloc_desc_hash_mask hashmask(14)

struct __alloc_desc *__alloc_desc_free = 0L;
struct __alloc_desc *__alloc_desc_hash[__alloc_desc_hash_size];

void __alloc_desc_delete(struct __alloc_desc *ptr)
{
	ptr->next = __alloc_desc_free;
	__alloc_desc_free = ptr;
	__mm_info.num_mm_frees_count++;
	__mm_info.num_mm_allocations--;
}

#define __alloc_desc_size 65536

void *__alloc_desc_new(void)
{
	struct __alloc_desc *ptr;

	if (!__alloc_desc_free) {
		unsigned int i;
		struct __alloc_desc *batch;

		if (!(batch = malloc(__alloc_desc_size))) {
			exit(-1);
		}

		__mm_info.num_mm_allocated_bytes += __alloc_desc_size;
		/*
		if (__mm_info.num_mm_allocated_bytes > __mm_info.num_mm_allocated_bytes_max) {
			__mm_info.num_mm_allocated_bytes_max = __mm_info.num_mm_allocated_bytes;
		}
		*/

		for (i=0; i<(__alloc_desc_size / sizeof(struct __alloc_desc)); i++) {
			batch->next = __alloc_desc_free;
			__alloc_desc_free = batch++;
		}
	}

	ptr = __alloc_desc_free;
	__alloc_desc_free = ptr->next;
	ptr->next = 0L;
	__mm_info.num_mm_allocations++;
	__mm_info.num_mm_allocations_count++;
	return ptr;
}

void __alloc_hash_add(struct __alloc_desc *desc)
{
	struct __alloc_desc **desc_ptr = &__alloc_desc_hash[__hash_voidp(desc->ptr) & __alloc_desc_hash_mask];

	if (*desc_ptr) {
		desc->next = *desc_ptr;
		*desc_ptr = desc;
	}
	else {
		*desc_ptr = desc;
	}
}

void __alloc_hash_remove(struct __alloc_desc *desc)
{
	struct __alloc_desc **desc_ptr = &__alloc_desc_hash[__hash_voidp(desc->ptr) & __alloc_desc_hash_mask];

	if (*desc_ptr) {
		struct __alloc_desc *prev = *desc_ptr;

		if (*desc_ptr == desc) {
			*desc_ptr = desc->next;
			desc->next = 0L;
			return;
		}

		while (prev) {
			if (prev->next == desc) {
				prev->next = prev->next->next;
				desc->next = 0L;
				return;
			}
			prev = prev->next;
		}
	}

	desc = desc->next;
}

struct __alloc_desc *__alloc_hash_find(void *ptr)
{
	struct __alloc_desc *desc = __alloc_desc_hash[__hash_voidp(ptr) & __alloc_desc_hash_mask];

	while (desc) {
		if (desc->ptr == ptr) {
			return desc;
		}
		desc = desc->next;
	}

	return 0L;
}

void *__mm_malloc(size_t size, volatile const char *file, volatile int line)
{
	struct __alloc_desc *desc;
	void *ptr;
	char tag[128];
	size_t tag_size;

	if ((tag_size = snprintf(tag, sizeof(tag), "%s:%06d", file, line)) < 1) {
		return 0L;
	}

	if (pthread_mutex_lock(&__mm_memory_lock)) {
		return 0L;
	}

	if (!(desc = __alloc_desc_new())) {
		pthread_mutex_unlock(&__mm_memory_lock);
		return 0L;
	}

	if (!(ptr = malloc(size))) {
		__alloc_desc_delete(desc);
		pthread_mutex_unlock(&__mm_memory_lock);
		return 0L;
	}

	if (!(desc->tag = __alloc_tag_hash_find(tag, tag_size))) {
		if (!(desc->tag = __alloc_tag_new())) {
			free(ptr);
			__alloc_desc_delete(desc);
			pthread_mutex_unlock(&__mm_memory_lock);
			return 0L;
		}

		memcpy(desc->tag->tag, tag, tag_size);
		desc->tag->tag[tag_size] = 0;
		desc->tag->size = tag_size;

		__alloc_tag_hash_add(desc->tag);
	}

	desc->ptr = ptr;
	desc->size = size;

	__alloc_hash_add(desc);

	__mm_info.num_allocations++;
	__mm_info.num_allocations_count++;
	if (__mm_info.num_allocations > __mm_info.num_allocations_max) {
		__mm_info.num_allocations_max = __mm_info.num_allocations;
	}
	__mm_info.num_allocated_bytes += size;
	if (__mm_info.num_allocated_bytes > __mm_info.num_allocated_bytes_max) {
		__mm_info.num_allocated_bytes_max = __mm_info.num_allocated_bytes;
	}

	desc->tag->info.num_allocations++;
	desc->tag->info.num_allocations_count++;
	if (desc->tag->info.num_allocations > desc->tag->info.num_allocations_max) {
		desc->tag->info.num_allocations_max = desc->tag->info.num_allocations;
	}
	desc->tag->info.num_allocated_bytes += size;
	if (desc->tag->info.num_allocated_bytes > desc->tag->info.num_allocated_bytes_max) {
		desc->tag->info.num_allocated_bytes_max = desc->tag->info.num_allocated_bytes;
	}

	pthread_mutex_unlock(&__mm_memory_lock);
	return ptr;
}

void *__mm_calloc(size_t nmemb, size_t size, volatile const char *file, volatile int line)
{
	struct __alloc_desc *desc;
	void *ptr;
	char tag[128];
	size_t tag_size;

	if ((tag_size = snprintf(tag, sizeof(tag), "%s:%06d", file, line)) < 1) {
		return 0L;
	}

	if (pthread_mutex_lock(&__mm_memory_lock)) {
		return 0L;
	}

	if (!(desc = __alloc_desc_new())) {
		pthread_mutex_unlock(&__mm_memory_lock);
		return 0L;
	}

	if (!(ptr = malloc(nmemb * size))) {
		__alloc_desc_delete(desc);
		pthread_mutex_unlock(&__mm_memory_lock);
		return 0L;
	}

	if (!(desc->tag = __alloc_tag_hash_find(tag, tag_size))) {
		if (!(desc->tag = __alloc_tag_new())) {
			free(ptr);
			__alloc_desc_delete(desc);
			pthread_mutex_unlock(&__mm_memory_lock);
			return 0L;
		}

		memcpy(desc->tag->tag, tag, tag_size);
		desc->tag->tag[tag_size] = 0;
		desc->tag->size = tag_size;

		__alloc_tag_hash_add(desc->tag);
	}

	memset(ptr, 0, nmemb * size);

	desc->ptr = ptr;
	desc->size = nmemb * size;

	__alloc_hash_add(desc);

	__mm_info.num_allocations++;
	__mm_info.num_allocations_count++;
	if (__mm_info.num_allocations > __mm_info.num_allocations_max) {
		__mm_info.num_allocations_max = __mm_info.num_allocations;
	}
	__mm_info.num_allocated_bytes += nmemb * size;
	if (__mm_info.num_allocated_bytes > __mm_info.num_allocated_bytes_max) {
		__mm_info.num_allocated_bytes_max = __mm_info.num_allocated_bytes;
	}

	desc->tag->info.num_allocations++;
	desc->tag->info.num_allocations_count++;
	if (desc->tag->info.num_allocations > desc->tag->info.num_allocations_max) {
		desc->tag->info.num_allocations_max = desc->tag->info.num_allocations;
	}
	desc->tag->info.num_allocated_bytes += nmemb * size;
	if (desc->tag->info.num_allocated_bytes > desc->tag->info.num_allocated_bytes_max) {
		desc->tag->info.num_allocated_bytes_max = desc->tag->info.num_allocated_bytes;
	}

	pthread_mutex_unlock(&__mm_memory_lock);
	return ptr;
}

void *__mm_realloc(void *ptr, size_t size, volatile const char *file, volatile int line)
{
	struct __alloc_desc *desc;
	char tag[128];
	size_t tag_size;

	if ((tag_size = snprintf(tag, sizeof(tag), "%s:%06d", file, line)) < 1) {
		return 0L;
	}

	if (pthread_mutex_lock(&__mm_memory_lock)) {
		return 0L;
	}

	if (!ptr) {
		if (!(desc = __alloc_desc_new())) {
			pthread_mutex_unlock(&__mm_memory_lock);
			return 0L;
		}

		if (!(ptr = malloc(size))) {
			__alloc_desc_delete(desc);
			pthread_mutex_unlock(&__mm_memory_lock);
			return 0L;
		}

		if (!(desc->tag = __alloc_tag_hash_find(tag, tag_size))) {
			if (!(desc->tag = __alloc_tag_new())) {
				free(ptr);
				__alloc_desc_delete(desc);
				pthread_mutex_unlock(&__mm_memory_lock);
				return 0L;
			}

			memcpy(desc->tag->tag, tag, tag_size);
			desc->tag->tag[tag_size] = 0;
			desc->tag->size = tag_size;

			__alloc_tag_hash_add(desc->tag);
		}

		desc->ptr = ptr;
		desc->size = size;

		__alloc_hash_add(desc);

		__mm_info.num_allocations++;
		__mm_info.num_allocations_count++;
		if (__mm_info.num_allocations > __mm_info.num_allocations_max) {
			__mm_info.num_allocations_max = __mm_info.num_allocations;
		}
		__mm_info.num_allocated_bytes += size;
		if (__mm_info.num_allocated_bytes > __mm_info.num_allocated_bytes_max) {
			__mm_info.num_allocated_bytes_max = __mm_info.num_allocated_bytes;
		}

		desc->tag->info.num_allocations++;
		desc->tag->info.num_allocations_count++;
		if (desc->tag->info.num_allocations > desc->tag->info.num_allocations_max) {
			desc->tag->info.num_allocations_max = desc->tag->info.num_allocations;
		}
		desc->tag->info.num_allocated_bytes += size;
		if (desc->tag->info.num_allocated_bytes > desc->tag->info.num_allocated_bytes_max) {
			desc->tag->info.num_allocated_bytes_max = desc->tag->info.num_allocated_bytes;
		}
	}
	else {
		desc = __alloc_hash_find(ptr);

		if (desc) {
			__alloc_hash_remove(desc);

			if (!(ptr = realloc(ptr, size))) {
				__alloc_desc_delete(desc);
				pthread_mutex_unlock(&__mm_memory_lock);
				return 0L;
			}

			desc->tag->info.num_allocated_bytes -= desc->size;
			__mm_info.num_allocated_bytes -= desc->size;

			desc->ptr = ptr;
			desc->size = size;

			__alloc_hash_add(desc);

			__mm_info.num_allocated_bytes += size;
			if (__mm_info.num_allocated_bytes > __mm_info.num_allocated_bytes_max) {
				__mm_info.num_allocated_bytes_max = __mm_info.num_allocated_bytes;
			}

			desc->tag->info.num_allocated_bytes += size;
			if (desc->tag->info.num_allocated_bytes > desc->tag->info.num_allocated_bytes_max) {
				desc->tag->info.num_allocated_bytes_max = desc->tag->info.num_allocated_bytes;
			}
		}
		else {
			ptr = realloc(ptr, size);
		}
	}

	pthread_mutex_unlock(&__mm_memory_lock);
	return ptr;
}

void __mm_free(void *ptr, volatile const char *file, volatile int line)
{
	if (ptr) {
		struct __alloc_desc *desc = __alloc_hash_find(ptr);

		if (pthread_mutex_lock(&__mm_memory_lock)) {
			return;
		}

		if (desc) {
			__alloc_hash_remove(desc);

			free(ptr);

			__mm_info.num_frees_count++;
			__mm_info.num_allocations--;
			__mm_info.num_allocated_bytes -= desc->size;

			desc->tag->info.num_frees_count++;
			desc->tag->info.num_allocations--;
			desc->tag->info.num_allocated_bytes -= desc->size;

			__alloc_desc_delete(desc);
		}
		else {
			free(ptr);
		}
		pthread_mutex_unlock(&__mm_memory_lock);
	}
}

char *__mm_strdup(const char *src, volatile const char *file, volatile int line)
{
	void *ptr = 0;
	char tag[128];
	size_t tag_size;

	if (src) {
		size_t size = strlen(src) + 1;
		struct __alloc_desc *desc;

		if ((tag_size = snprintf(tag, sizeof(tag), "%s:%06d", file, line)) < 1) {
			return 0L;
		}

		if (pthread_mutex_lock(&__mm_memory_lock)) {
			return 0L;
		}

		if (!(desc = __alloc_desc_new())) {
			pthread_mutex_unlock(&__mm_memory_lock);
			return 0L;
		}

		if (!(ptr = malloc(size))) {
			__alloc_desc_delete(desc);
			pthread_mutex_unlock(&__mm_memory_lock);
			return 0L;
		}

		if (!(desc->tag = __alloc_tag_hash_find(tag, tag_size))) {
			if (!(desc->tag = __alloc_tag_new())) {
				free(ptr);
				__alloc_desc_delete(desc);
				pthread_mutex_unlock(&__mm_memory_lock);
				return 0L;
			}

			memcpy(desc->tag->tag, tag, tag_size);
			desc->tag->tag[tag_size] = 0;
			desc->tag->size = tag_size;

			__alloc_tag_hash_add(desc->tag);
		}

		desc->ptr = ptr;
		desc->size = size;

		__alloc_hash_add(desc);

		memcpy(ptr, src, size);

		__mm_info.num_allocations++;
		__mm_info.num_allocations_count++;
		if (__mm_info.num_allocations > __mm_info.num_allocations_max) {
			__mm_info.num_allocations_max = __mm_info.num_allocations;
		}
		__mm_info.num_allocated_bytes += size;
		if (__mm_info.num_allocated_bytes > __mm_info.num_allocated_bytes_max) {
			__mm_info.num_allocated_bytes_max = __mm_info.num_allocated_bytes;
		}

		desc->tag->info.num_allocations++;
		desc->tag->info.num_allocations_count++;
		if (desc->tag->info.num_allocations > desc->tag->info.num_allocations_max) {
			desc->tag->info.num_allocations_max = desc->tag->info.num_allocations;
		}
		desc->tag->info.num_allocated_bytes += size;
		if (desc->tag->info.num_allocated_bytes > desc->tag->info.num_allocated_bytes_max) {
			desc->tag->info.num_allocated_bytes_max = desc->tag->info.num_allocated_bytes;
		}

		pthread_mutex_unlock(&__mm_memory_lock);
	}

	return (char *)ptr;
}

void *mm_malloc2(size_t size)
{
	return mm_malloc(size);
}

void *mm_calloc2(size_t nmemb, size_t size)
{
	return mm_calloc(nmemb, size);
}

void *mm_realloc2(void *ptr, size_t size)
{
	return mm_realloc(ptr, size);
}

void mm_free2(void *ptr)
{
	mm_free(ptr);
}

char *mm_strdup2(const char *ptr)
{
	return mm_strdup(ptr);
}

void *mm_libxml_malloc(size_t size)
{
	return mm_malloc(size);
}

void *mm_libxml_calloc(size_t nmemb, size_t size)
{
	return mm_calloc(nmemb, size);
}

void *mm_libxml_realloc(void *ptr, size_t size)
{
	return mm_realloc(ptr, size);
}

void mm_libxml_free(void *ptr)
{
	mm_free(ptr);
}

char *mm_libxml_strdup(const char *ptr)
{
	return mm_strdup(ptr);
}

void mm_init(void) {
	memset(&__alloc_tag_hash[0], 0, sizeof(__alloc_tag_hash));
	memset(&__alloc_desc_hash[0], 0, sizeof(__alloc_desc_hash));
}

const struct mm_meminfo *mm_get_info(void)
{
	return &__mm_info;
}

void mm_get_detailed_info(mm_detailed_info_callback_t func, void *data) {
	static const char *summary = "summary";
	struct mm_taginfo info = { 0L, 0L, 0L, 0L, 0L, 0L };
	struct __alloc_tag **tag = __alloc_tag_hash;
	struct __alloc_tag *next = 0L;
	unsigned int i, found = 1;

	for (i=0; i<__alloc_tag_hash_size; i++, tag++) {
		if (*tag) {
			next = *tag;
			while (next) {
				info.num_allocated_bytes += next->info.num_allocated_bytes;
				info.num_allocated_bytes_max += next->info.num_allocated_bytes_max;
				info.num_allocations += next->info.num_allocations;
				info.num_allocations_count += next->info.num_allocations_count;
				info.num_allocations_max += next->info.num_allocations_max;
				info.num_frees_count += next->info.num_frees_count;
				func(next->tag, &next->info, data);
				next = next->next;
			}
		}
	}
	func(summary, &info, data);
}

#endif

#ifdef MM_MEMORY_ALLOC

#define __mm_alloc_size 65536

void *__mm_alloc_new(mm_alloc_t *alloc, volatile const char *file, volatile int line)
{
	void *ptr = 0L;
#ifdef MM_MEMORY_TRACKING
	struct __alloc_tag *tag_ptr;
	char tag[128];
	size_t tag_size;
#endif

	if (alloc == 0L) {
		return 0L;
	}

	if (pthread_mutex_lock(&__mm_memory_lock)) {
		return 0L;
	}

	if (!alloc->next) {
		unsigned int i;
		void *batch;

#ifdef MM_MEMORY_TRACKING
		if ((tag_size = snprintf(tag, sizeof(tag), "%s:%06d", file, line)) < 1) {
			pthread_mutex_unlock(&__mm_memory_lock);
			return 0L;
		}

		if (!(tag_ptr = __alloc_tag_hash_find(tag, tag_size))) {
			if (!(tag_ptr = __alloc_tag_new())) {
				pthread_mutex_unlock(&__mm_memory_lock);
				return 0L;
			}

			memcpy(tag_ptr->tag, tag, tag_size);
			tag_ptr->tag[tag_size] = 0;
			tag_ptr->size = tag_size;

			__alloc_tag_hash_add(tag_ptr);
		}
#endif

		if (!(batch = malloc(__mm_alloc_size))) {
			exit(-1);
		}

#ifdef MM_MEMORY_TRACKING
		__mm_info.num_allocations++;
		__mm_info.num_allocations_count++;
		if (__mm_info.num_allocations > __mm_info.num_allocations_max) {
			__mm_info.num_allocations_max = __mm_info.num_allocations;
		}
		__mm_info.num_allocated_bytes += __alloc_desc_size;
		if (__mm_info.num_allocated_bytes > __mm_info.num_allocated_bytes_max) {
			__mm_info.num_allocated_bytes_max = __mm_info.num_allocated_bytes;
		}

		tag_ptr->info.num_allocations++;
		tag_ptr->info.num_allocations_count++;
		if (tag_ptr->info.num_allocations > tag_ptr->info.num_allocations_max) {
			tag_ptr->info.num_allocations_max = tag_ptr->info.num_allocations;
		}
		tag_ptr->info.num_allocated_bytes += __alloc_desc_size;
		if (tag_ptr->info.num_allocated_bytes > tag_ptr->info.num_allocated_bytes_max) {
			tag_ptr->info.num_allocated_bytes_max = tag_ptr->info.num_allocated_bytes;
		}
#endif

		for (i=0; i<(__mm_alloc_size / alloc->size); i++) {
			*(void **)batch = alloc->next;
			alloc->next = batch;
			batch = ((char *)batch + alloc->size);
		}
	}

	ptr = alloc->next;
	alloc->next = *(void **)ptr;
	*(void **)ptr = 0L;

#ifdef MM_MEMORY_TRACKING
	alloc->info.num_allocations++;
	alloc->info.num_allocations_count++;
	if (alloc->info.num_allocations > alloc->info.num_allocations_max) {
		alloc->info.num_allocations_max = alloc->info.num_allocations;
	}
	alloc->info.num_allocated_bytes += alloc->size;
	if (alloc->info.num_allocated_bytes > alloc->info.num_allocated_bytes_max) {
		alloc->info.num_allocated_bytes_max = alloc->info.num_allocated_bytes;
	}
#endif

	pthread_mutex_unlock(&__mm_memory_lock);
	return ptr;
}

void __mm_alloc_delete(mm_alloc_t *alloc, void *ptr, volatile const char *file, volatile int line)
{
	if (alloc == 0L || ptr == 0L) {
		return;
	}

	if (pthread_mutex_lock(&__mm_memory_lock)) {
		return;
	}

	*(void **)ptr = alloc->next;
	alloc->next = ptr;

#ifdef MM_MEMORY_TRACKING
	alloc->info.num_frees_count++;
	alloc->info.num_allocations--;
	alloc->info.num_allocated_bytes -= alloc->size;
#endif

	pthread_mutex_unlock(&__mm_memory_lock);
}

#endif
