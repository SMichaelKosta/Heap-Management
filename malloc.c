#include <assert.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#define ALIGN4(s)         (((((s) - 1) >> 2) << 2) + 4)
#define BLOCK_DATA(b)      ((b) + 1)
#define BLOCK_HEADER(ptr)   ((struct _block *)(ptr) - 1)


static int atexit_registered = 0;
static int num_mallocs       = 0;
static int num_frees         = 0;
static int num_reuses        = 0;
static int num_grows         = 0;
static int num_splits        = 0;
static int num_coalesces     = 0;
static int num_blocks        = 0;
static int num_requested     = 0;
static int max_heap          = 0;

/*
 *  \brief printStatistics
 *
 *  \param none
 *
 *  Prints the heap statistics upon process exit.  Registered
 *  via atexit()
 *
 *  \return none
 */
void printStatistics( void )
{
  printf("\nheap management statistics\n");
  printf("mallocs:\t%d\n", num_mallocs );
  printf("frees:\t\t%d\n", num_frees );
  printf("reuses:\t\t%d\n", num_reuses );
  printf("grows:\t\t%d\n", num_grows );
  printf("splits:\t\t%d\n", num_splits );
  printf("coalesces:\t%d\n", num_coalesces );
  printf("blocks:\t\t%d\n", num_blocks );
  printf("requested:\t%d\n", num_requested );
  printf("max heap:\t%d\n", max_heap );
}

struct _block 
{
    size_t  size;         /* Size of the allocated _block of memory in bytes */
    struct _block *prev;  /* Pointer to the previous _block of allcated memory   */
    struct _block *next;  /* Pointer to the next _block of allcated memory   */
    bool   free;          /* Is this _block free?                     */
    char   padding[3];
};


struct _block *freeList = NULL; /* Free list to track the _blocks available */

/*
 * \brief findFreeBlock
 *
 * \param last pointer to the linked list of free _blocks
 * \param size size of the _block needed in bytes 
 *
 * \return a _block that fits the request or NULL if no free _block matches
 *
 * \TODO Implement Next Fit
 * \TODO Implement Best Fit
 * \TODO Implement Worst Fit
 */
struct _block *findFreeBlock(struct _block **last, size_t size) 
{
    struct _block *curr = freeList;
#if defined FIT && FIT == 0
    /* First fit */
    while (curr && !(curr->free && curr->size >= size))
    {
        *last = curr;
        curr  = curr->next;
    }
#endif

#if defined BEST && BEST == 0
    /* Best fit */
    struct _block *best = curr;
    int count = 0;
    while (curr)
    {
        if (curr->free == 1)
        {
            count = 1;
        }
        if (curr->size < best->size && !(curr->size < size))
        {
            best = curr;
        }
        *last = curr;
        curr  = curr->next;
    }
    if (count == 1)
    {
        curr = best;
    }
#endif

#if defined WORST && WORST == 0
    /* Worst fit */
    struct _block *worst = curr;
    int count = 0;
    while (curr)
    {
        if (curr->free == 1)
        {
            count = 1;
        }
        if (curr->size > worst->size && !(curr->size < size))
        {
            worst = curr;
        }
        *last = curr;
        curr  = curr->next;
    }
    if (count == 1)
    {
        curr = worst;
    }
#endif

#if defined NEXT && NEXT == 0
    /* Next fit */
    int Truth = 0;
    int NOTT = 0;
    struct _block *test = curr;
    if(test)
    {
        if(test->free == 0)
        {
            while (test)
            {
                if(test->free == 1)
                {
                    Truth = 1;
                    break;
                }
                test = test->next;
            }
        }
        else if(test->free == 1)
        {
            while (test)
            {
                if(test->free == 0)
                {
                    NOTT = 1;
                    break;
                }
                test = test->next;
            }
        }
    }
    if(Truth == 0 && NOTT == 0)
    {
        while (curr && !(curr->free && curr->size >= size))
        {
            *last = curr;
            curr  = curr->next;
        }
    }
    else if (Truth == 1)
    {
        while (curr)
        {
            if(curr->free != 0 && curr->size >= size)
            {
                return curr;
            }
            curr = curr->next;
        }
    }
    else if (NOTT == 1)
    {
        while (NOTT != 2 && curr)
        {
            if(curr->free == 0)
            {
                NOTT = 2;
            }
            curr = curr->next;
        }
        while (curr)
        {
            if(curr->free != 0 && curr->size >= size)
            {
                return curr;
            }
            curr = curr->next;
        }
    }
#endif
    if (curr == NULL)
    {
        num_blocks++;
    }
    return curr;
}

/*
 * \brief growheap
 *
 * Given a requested size of memory, use sbrk() to dynamically 
 * increase the data segment of the calling process.  Updates
 * the free list with the newly allocated memory.
 *
 * \param last tail of the free _block list
 * \param size size in bytes to request from the OS
 *
 * \return returns the newly allocated _block of NULL if failed
 */
struct _block *growHeap(struct _block *last, size_t size) 
{
    /* Request more space from OS */
    struct _block *curr = (struct _block *)sbrk(0);
    struct _block *prev = (struct _block *)sbrk(sizeof(struct _block) + size);

    assert(curr == prev);

    /* OS allocation failed */
    if (curr == (struct _block *)-1)
    {
        return NULL;
    }

    /* Update freeList if not set */
    if (freeList == NULL)
    {
        freeList = curr;
    }

    /* Attach new _block to prev _block */
    if (last)
    {
        last->next = curr;
    }

    /* Update _block metadata */
    curr->size = size;
    curr->next = NULL;
    curr->free = false;
    num_grows++;
    return curr;
}

/*
 * \brief malloc
 *
 * finds a free _block of heap memory for the calling process.
 * if there is no free _block that satisfies the request then grows the 
 * heap and returns a new _block
 *
 * \param size size of the requested memory in bytes
 *
 * \return returns the requested memory allocation to the calling process 
 * or NULL if failed
 */
void *malloc(size_t size) 
{
    num_requested = num_requested + size;
    if(size > max_heap)
    {
        max_heap = size;
    }
    if( atexit_registered == 0 )
    {
        atexit_registered = 1;
        atexit( printStatistics );
    }

    /* Align to multiple of 4 */
    size = ALIGN4(size);

    /* Handle 0 size */
    if (size == 0)
    {
        return NULL;
    }

    /* Look for free _block */
    struct _block *last = freeList;
    struct _block *next = findFreeBlock(&last, size);

    /* TODO: Split free _block if possible */
    if (next != NULL)
    {
        num_reuses++;
    }
    if ((next != NULL) && (next->free == true) && (next->size > size))
    {
        int count = 1;
        size_t Nsize = next->size - size - sizeof(struct _block);
        struct _block *temp;
        if ((freeList->free == true) && (freeList->size == next->size))
        {
            temp = growHeap(next, size);
            next->size = Nsize;
            next->free = false;
            num_splits++;
            num_blocks++;
        }
        struct _block *now = freeList;
        while (now != NULL)
        {
            //printf("%d: %zu\n", count, now->size);
            now = now->next;
            count++;
        }
    }

    /* Could not find free _block, so grow heap */
    if (next == NULL)
    {
        next = growHeap(last, size);
    }

    /* Could not find free _block or grow heap, so just return NULL */
    if (next == NULL)
    {
        return NULL;
    }

    /* Mark _block as in use */
    next->free = false;

    /* Return data address associated with _block */
    num_mallocs++;
    return BLOCK_DATA(next);
}

/*
 * \brief free
 *
 * frees the memory _block pointed to by pointer. if the _block is adjacent
 * to another _block then coalesces (combines) them
 *
 * \param ptr the heap memory to free
 *
 * \return none
 */
void free(void *ptr) 
{
    if (ptr == NULL)
    {
        return;
    }

    /* Make _block as free */
    num_frees++;
    struct _block *curr = BLOCK_HEADER(ptr);
    assert(curr->free == 0);
    curr->free = true;

    /* TODO: Coalesce free _blocks if needed */
    int Test = 0;
    int Tester = 1;
    struct _block *after = freeList->next;
    struct _block *temp;
    struct _block *rep;
    struct _block *prev;
    while (freeList && after)
    {
        Tester = 0;
        if (freeList->free == 1 && after->free == 1)
        {
            freeList->size = freeList->size + after->size + sizeof(struct _block);
            Test = 1;
            num_coalesces++;
            num_blocks--;
        }
        if (Test == 1)
        {
            if (after->next != NULL)
            {
                temp = after->next;
                temp->prev = freeList;
                freeList->next = temp;
            }
            else
            {
                rep = freeList->next;
                rep->size = 0;
                rep->prev = freeList;
                rep->free = 0;
                freeList->next = rep;
            }
            Test = 0;
        }
        prev = freeList;
        freeList = freeList->next;
        freeList->prev = prev;
        after = freeList->next;
    }
    while (Tester != 1)
    {
        if (freeList->prev != NULL)
        {
            freeList = freeList->prev;
        }
        else
        {
            Tester = 1;
        }
    }
}

void *calloc(size_t nmeb, size_t size)
{
    char * ptr;
    size_t i = 0;
    while(i < nmeb)
    {
        ptr = malloc(size);
        memset(ptr, 0, size);
        i++;
    }
    return ptr;
}

void *realloc(void *ptr, size_t size)
{
    char * newPtr;
    if (ptr == 0)
    {
        return malloc(size);
    }
    struct _block *curr = BLOCK_HEADER(ptr);
    if (size <= curr->size)
    {
        return ptr;
    }
    newPtr = malloc(size);
    bcopy(ptr, newPtr, (size_t) curr->size);
    free(ptr);
    num_blocks++;
    return (newPtr);
}

/* vim: set expandtab sts=3 sw=3 ts=6 ft=cpp: --------------------------------*/
