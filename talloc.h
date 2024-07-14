#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

// Union memory block
// It stores the header and the memory allocated
typedef char ALIGN[16];
union header {
    struct {
        size_t size;
        unsigned is_free;
        // pointer to next header
        union header *next;
    } s;
    ALIGN stub;
};
typedef union header header_t;

header_t *get_free_block(size_t size);
void *talloc(size_t size);
void free(void *block);
void *calloc(size_t num, size_t nsize);
void *realloc(void *block, size_t size);

header_t *head, *tail;
pthread_mutex_t global_malloc_lock;

int main(int argc, char *argv[])
{
    return 0;
}

void *talloc(size_t size)
{
    // Initialize variables
    size_t total_size;
    void *block;
    header_t *header;
    if (!size) return NULL;
    
    // lock the threading for others, for safety
    pthread_mutex_lock(&global_malloc_lock);
    header = get_free_block(size);
    if (header) {
        header->s.is_free = 0;
        pthread_mutex_unlock(&global_malloc_lock);
        // change the the pointer from header to the memory block
        return (void*)(header + 1);
    }

    total_size = sizeof(header_t) + size;
    block = sbrk(total_size);
    // fallback if sbrk fails
    if (block == (void*) -1) {
        pthread_mutex_unlock(&global_malloc_lock);
        return NULL;
    }
    header = block;
    header->s.size = size;
    header->s.is_free = 0;
    header->s.next = NULL;

    // if head is empty then assign it to the first header/node
    if (!head) {
        head = header;
    } 
    // if tail has value, assign the tail to the new header/node 
    if (tail) {
        tail->s.next = header;
    }
    pthread_mutex_unlock(&global_malloc_lock); 
    return (void*)(header + 1);
}

header_t *get_free_block(size_t size) 
{
    header_t *curr = head;
    while (curr) {
        if (curr->s.is_free && curr->s.size >= size)
            return curr;
        curr = curr->s.next; 
    }
    return NULL; 
}

void free (void *block)
{
    header_t *header, *tmp;
    void *programbreak;

    // lock the thread
    pthread_mutex_lock(&global_malloc_lock);
    // move backward to select the header instead of block/data
    header = (header_t*)(block -1);

    // sbrk(0) returns the current address of brk
    programbreak = sbrk(0);
    if ((char*)block + header->s.size == programbreak) {
        if (head == tail) {
            head = tail = NULL; 
        } else {
            tmp = header;
            // transverse through the linked list
            while (tmp) {
                if (tmp->s.next == tail) {
                    // change the tail to tmp, since tail is going to be released 
                    tmp->s.next = NULL;
                    tail = tmp;
                }
                tmp = tmp->s.next;
            }
            sbrk(0 - sizeof(header_t) - header->s.size);
            pthread_mutex_unlock(&global_malloc_lock);
            return;
        }
    }
    // mark the header as free, the current address is not at the brk
    header->s.is_free = 1;
    pthread_mutex_unlock(&global_malloc_lock);
}

// allocates n elements, with each size of elements is nsize
void *calloc(size_t num, size_t nsize)
{
    size_t size;
    void *block; 
    if (!num || !nsize) {
        return NULL;
    }
    size = num * nsize;

    if (nsize != size / num) {
        return NULL;
    }
    block = talloc(size);
    if (!block) {
        return NULL;
    }
    memset(block, 0, size);
    return block;
}

void *realloc(void* block, size_t size)
{
    header_t *header;
    void* resized;
    if (!block || !size) {
        return NULL; 
    }
    // change from block to header
    header = (header_t*)(block -1);
    if (header->s.size >= size) {
        return block;
    }
    resized = talloc(size);
    if (resized) {
       memcpy(resized, block, header->s.size); 
    }
    return resized;
}


