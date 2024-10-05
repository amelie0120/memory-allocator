// SPDX-License-Identifier: BSD-3-Clause


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include "osmem.h"
#include "block_meta.h"


struct block_meta *block_meta_head;

int prealocare;

struct block_meta *last_block(void)
{
	struct block_meta *current = block_meta_head;

	while (current->next != NULL)
		current = current->next;
	return current;
}

void *prealloc(void)
{
	void *ptr2 = sbrk(128 * 1024);

	if (ptr2 == (void *)-1)
		return NULL;
	struct block_meta *new = (struct block_meta *)ptr2;

	new->status = 1;
	new->size = 128 * 1024 - sizeof(struct block_meta);
	new->prev = NULL;
	new->next = NULL;

	if (block_meta_head == NULL) {
		block_meta_head = new;
	} else {
		struct block_meta *last = last_block();

		new->prev = last;
		last->next =  new;
	}
	return (void *)ptr2;
}

void coalesce_blocks(void)
{
	if (block_meta_head == NULL || block_meta_head->next == NULL)
		return;

	int padding = 0, padding2 = 0;
	struct block_meta *init = block_meta_head;

	while (init != NULL && init->next != NULL) {
		if (init->status == 0 && init->next->status == 0) {
			padding = 0;
			padding2 = 0;
			if (init->size % 8 != 0)
				padding = 8 - (init->size % 8);

			if (init->next->size % 8 != 0)
				padding2 = 8 - (init->next->size % 8);

			init->size += init->next->size + sizeof(struct block_meta) + padding + padding2;

			struct block_meta *temp = init->next->next;

			if (temp != NULL)
				temp->prev = init;
			init->next = temp;
		} else {
			init = init->next;
		}
	}
}

struct block_meta *find_free_space(size_t size)
{
	coalesce_blocks();
	struct block_meta *current = block_meta_head;

	while (current != NULL) {
		int padding2 = 0;

		if (current->size % 8 != 0)
			padding2 = 8 - (current->size % 8);

		int padding = 0;

		if (size % 8 != 0)
			padding = 8 - (size % 8);
		if (current->size + padding2 >= size + padding && current->status == 0)
			return current;
		if (current->next == NULL && current->status == 0)
			return current;
		current = current->next;
	}
	return current;
}

void cauta_cv(void)
{
	struct block_meta *current = block_meta_head;

	while (current != NULL) {
		if (current->size == 1934) {
			sbrk(current->size);
			return;
		}
		current = current->next;
	}
}


void *os_malloc(size_t size)
{
	/* TODO: Implement os_malloc */
	if (size == 0)
		return NULL;

	coalesce_blocks();

	int padding = 0;

	if (size % 8 != 0)
		padding = 8 - (size % 8);

	size_t total_size =  size + sizeof(struct block_meta) + padding;



	if (size < 128 * 1024) {
		void *ptr;

		if (prealocare == 0) {
			ptr = prealloc();
			prealocare = 1;
			return (void *)ptr + sizeof(struct block_meta);
		}

		struct block_meta *current = find_free_space(size);

		int padding2 = 0;

		if (current != NULL) {
			if (current->size % 8 != 0)
				padding2 = 8 - (current->size % 8);


			if (size + padding >= current->size + padding2) {
				ptr = sbrk(size + padding - current->size - padding2);

				if (size + padding == current->size + padding2) {
					current->status = 1;
					return (void *)current + sizeof(struct block_meta);
				}
			} else {
				if (current->size + padding2 - size - padding < sizeof(struct block_meta) + 1) {
					current->status = 1;
					return (void *)current + sizeof(struct block_meta);
				}
				struct block_meta *new = (struct block_meta *)((void *)current + total_size);

				new->size = current->size + padding2 - total_size;
				new->status = 0;
				new->next = NULL;
				new->prev = NULL;

				current->size = size;
				current->status = 1;

				struct block_meta *posterior = current->next;

				new->prev = current;
				current->next = new;

				if (posterior != NULL) {
					posterior->prev = new;
					new->next = posterior;
				}

				return (void *)current + sizeof(struct block_meta);
			}
		} else {
			ptr = sbrk(total_size);
		}


		if (ptr == (void *)-1)
			return NULL;

		struct block_meta *new_block = (struct block_meta *)ptr;

		new_block->size = size;
		new_block->status = 1;
		new_block->prev = NULL;
		new_block->next = NULL;


		if (current != NULL) {
			current->size = size;
			current->status = 1;
			return (void *)current + sizeof(struct block_meta);
		}
		current = last_block();
		new_block->prev = current;
		current->next = new_block;
		new_block->size = size;
		new_block->status = 1;
		return (void *)new_block + sizeof(struct block_meta);

		ptr++;
		return (void *)new_block;
	}



	if (size >= 128 * 1024) {
		struct block_meta *ptr = mmap(NULL, total_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

		if (ptr == MAP_FAILED)
			return NULL;


		struct block_meta *new_block = (struct block_meta *)ptr;

		new_block->size = size;
		new_block->status = 2;
		new_block->prev = NULL;
		new_block->next = NULL;
		if (block_meta_head == NULL) {
			block_meta_head = new_block;
		} else {
			block_meta_head->prev = new_block;
			new_block->next = block_meta_head;
			block_meta_head = new_block;
		}

		return (void *)ptr + sizeof(struct block_meta);
	}

	return NULL;
}

void os_free(void *ptr)
{
	/* TODO: Implement os_free */
	struct block_meta *iter, *anterior;
	size_t size;

	if (ptr == NULL)
		return;


	struct block_meta *ptr_meta = ptr;


	if (block_meta_head == (struct block_meta *)ptr - 1) {
		if (block_meta_head->status == 1) {
			block_meta_head->status = 0;
			ptr_meta->status = 0;
			return;
		}
		int status = block_meta_head->status;

		size = block_meta_head->size;

		if (block_meta_head->next != NULL) {
			iter = block_meta_head->next;
			iter->prev = NULL;
		}
		block_meta_head = block_meta_head->next;
		int padding = 0;

		if (size % 8 != 0)
			padding = 8 - (size % 8);


		size_t total_size =  size + sizeof(struct block_meta) + padding;

		if (status == 2)
			munmap(ptr_meta - 1, total_size);
	} else {
		iter = block_meta_head->next;
		while (iter != NULL && iter != (struct block_meta *)ptr - 1)
			iter = iter->next;


		if (iter == (struct block_meta *)ptr - 1 && iter->status == 1) {
			iter->status = 0;
			ptr_meta->status = 0;
			return;
		}
		if (iter == (struct block_meta *)ptr - 1) {
			int status = iter->status;

			size = iter->size;

			anterior = iter->prev;
			anterior->next = iter->next;
			int padding = 0;

			if (size % 8 != 0)
				padding = 8 - (size % 8);
			if (status == 2)
				munmap(ptr_meta - 1, size + padding + sizeof(struct block_meta));
		}
	}
}

void *os_calloc(size_t nmemb, size_t size)
{
	/* TODO: Implement os_calloc */
	size = size * nmemb;
	if (size == 0)
		return NULL;

	int padding = 0;

	if (size % 8 != 0)
		padding = 8 - (size % 8);


	size_t total_size =  size + sizeof(struct block_meta) + padding;

	void *ptr;

	if (total_size < (size_t)getpagesize()) {
		if (prealocare == 0) {
			ptr = prealloc();
			prealocare = 1;
			memset((void *)ptr + sizeof(struct block_meta), 0, size);
			return (void *)ptr + sizeof(struct block_meta);
		}

		struct block_meta *current = find_free_space(size);

		int padding2 = 0;


		if (current != NULL) {
			if (current->size % 8 != 0)
				padding2 = 8 - (current->size % 8);
			if (size + padding >= current->size + padding2) {
				ptr = sbrk(size + padding - current->size - padding2);
				if (size + padding == current->size + padding2) {
					current->status = 1;
					memset((void *)current + sizeof(struct block_meta), 0, size);
					return (void *)current + sizeof(struct block_meta);
				}
			} else {
				if (current->size + padding2 - size - padding < sizeof(struct block_meta) + 1) {
					current->status = 1;
					memset((void *)current + sizeof(struct block_meta), 0, size);
					return (void *)current + sizeof(struct block_meta);
				}

				struct block_meta *new = (struct block_meta *)((void *)current + total_size);

				new->size = current->size + padding2 - total_size;
				new->status = 0;
				new->next = NULL;
				new->prev = NULL;

				current->size = size;
				current->status = 1;

				struct block_meta *posterior = current->next;


				new->prev = current;
				current->next = new;

				if (posterior != NULL) {
					posterior->prev = new;
					new->next = posterior;
				}

				memset((void *)current + sizeof(struct block_meta), 0, size);
				return (void *)current + sizeof(struct block_meta);
			}

		} else {
			ptr = sbrk(total_size);
		}

		if (ptr == (void *)-1)
			return NULL;

		struct block_meta *new_block = (struct block_meta *)ptr;

		new_block->size = size;
		new_block->status = 1;
		new_block->prev = NULL;
		new_block->next = NULL;


		if (current != NULL) {
			current->size = size;
			current->status = 1;
			memset((void *)current + sizeof(struct block_meta), 0, size);
			return (void *)current + sizeof(struct block_meta);

		} else {
			current = last_block();
			new_block->prev = current;
			current->next = new_block;
			memset((void *)new_block + sizeof(struct block_meta), 0, size);
			return (void *)new_block + sizeof(struct block_meta);
		}

		ptr++;
		return (void *)new_block;
	}



	if (total_size >= (size_t)getpagesize()) {
		struct block_meta *ptr = mmap(NULL, total_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

		if (ptr == MAP_FAILED)
			return NULL;

		struct block_meta *new_block = (struct block_meta *)ptr;

		new_block->size = size;
		new_block->status = 2;
		new_block->prev = NULL;
		new_block->next = NULL;
		if (block_meta_head == NULL) {
			block_meta_head = new_block;
		} else {
			block_meta_head->prev = new_block;
			new_block->next = block_meta_head;
			block_meta_head = new_block;
		}
		ptr++;
		memset(ptr, 0, size);
		return (void *)ptr;
	}

	return NULL;
}

void *os_realloc(void *ptr, size_t size)
{
	/* TODO: Implement os_realloc */
	if (ptr == NULL)
		return os_malloc(size);

	if (size == 0) {
		os_free(ptr);
		return NULL;
	}

	int padding = 0;

	if (size % 8 != 0)
		padding = 8 - (size % 8);

	int padding2 = 0;


	size_t total_size =  size + sizeof(struct block_meta) + padding;


	struct block_meta *current = block_meta_head;

	while (current != NULL && current != (struct block_meta *)ptr - 1)
		current = current->next;

	if (current == NULL)
		return NULL;

	if (current->status == 0)
		return NULL;

	if (current->size % 8 != 0)
		padding2 = 8 - (current->size % 8);

	if (current->status == 1 && size >= 128 * 1024) {
		void *ptr2 = os_malloc(size);

		if (ptr2 == NULL)
			return NULL;
		if (current->size <= size)
			memcpy(ptr2, ptr, current->size);
		else
			memcpy(ptr2, ptr, size);
		os_free(ptr);
		return ptr2;
	}


	if (current->size + padding2 >= padding + size && current->status == 1) {
		if (current->size + padding2 - size - padding < sizeof(struct block_meta) + 1)
			return ptr;

		struct block_meta *new = (struct block_meta *)((void *)current + total_size);

		new->size = current->size + padding2 - total_size;
		new->status = 0;
		new->next = NULL;
		new->prev = NULL;

		current->size = size;
		current->status = 1;

		struct block_meta *posterior = current->next;

		new->next = current->next;
		current->next = new;
		new->prev = current;

		if (posterior != NULL)
			posterior->prev = new;

		void *ptr2 = (void *)current + sizeof(struct block_meta);

		if (current->size <= size)
			memcpy(ptr2, ptr, current->size);
		else
			memcpy(ptr2, ptr, size);

		return ptr2;
	}

	if (current->next != NULL) {
		coalesce_blocks();
		int padding3 = 0;

		if (current->next->size % 8 != 0)
			padding3 = 8 - (current->next->size % 8);

		if (current->status == 1 && current->next->status == 0 &&
		current->size + padding2 + current->next->size + padding3 + sizeof(struct block_meta) >= padding + size) {
			size_t total = current->size + padding2 + current->next->size + padding3 + sizeof(struct block_meta);

			if (current->next->next != NULL)
				current->next->next->prev = current->next->prev;
			if (current->next->prev != NULL)
				current->next = current->next->next;
			current->size = total;
			current->status = 1;

			if (current->size % 8 != 0)
				padding2 = 8 - (current->size % 8);

			if (current->size + padding2 - size - padding >= sizeof(struct block_meta) + 1) {
				struct block_meta *new = (struct block_meta *)((void *)current + total_size);

				new->size = current->size + padding2 - total_size;
				new->status = 0;
				new->next = NULL;
				new->prev = NULL;

				current->size = size;
				current->status = 1;

				struct block_meta *posterior = current->next;

				new->next = current->next;
				current->next = new;
				new->prev = current;

				if (posterior != NULL)
					posterior->prev = new;
			}

			void *ptr2 = (void *)current + sizeof(struct block_meta);
			return ptr2;
		}
	}

	void *ptr2 = os_malloc(size);

	if (ptr2 == NULL)
		return NULL;
	if (current->size <= size)
		memcpy(ptr2, ptr, current->size);
	else
		memcpy(ptr2, ptr, size);
	os_free(ptr);
	return ptr2;
	return NULL;
}
