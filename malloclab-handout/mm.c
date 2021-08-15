/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 *
 *  using implicit free list and next-fit strategy
 * 
 */
#include "mm.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "ateam",
    /* First member's full name */
    "Harry Bovik",
    /* First member's email address */
    "bovik@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1 << 12)

#define MAX(x, y) ((x) > (y) ? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc))

/* Read and write a word at address p*/
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

/* Read and size and allocated fields from address P */
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* Given block ptr bp, compute address of ites header and footer */
#define HDRP(bp) ((char *)(bp)-WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/*Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE((char *)(bp)-WSIZE))
#define PREV_BLKP(bp) ((char *)(bp)-GET_SIZE((char *)(bp)-DSIZE))

/*
 * mm_init - initialize the malloc package.
 */
static void *heap_listp;
static void *last_ptr;

static void *coalesce(void *bp) {
  size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
  size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
  size_t size = GET_SIZE(HDRP(bp));
  if (prev_alloc && next_alloc) {
    last_ptr = bp;
    return bp;
  } else if (prev_alloc && !next_alloc) {
    size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
  } else if (!prev_alloc && next_alloc) {
    size += GET_SIZE(HDRP(PREV_BLKP(bp)));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
    bp = PREV_BLKP(bp);
  } else {
    size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
    PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
    PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
    bp = PREV_BLKP(bp);
  }
  last_ptr = bp; 
  return bp;
}

static void *extend_heap(size_t words) {
  char *bp;
  size_t size;
  size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
  if ((long)(bp = mem_sbrk(size)) == -1) {
    return NULL;
  }
  PUT(HDRP(bp), PACK(size, 0));
  PUT(FTRP(bp), PACK(size, 0));
  PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));
  return coalesce(bp);
}

/* next-fit */
static void *find_fit(size_t asize) {
  for (char *bp = last_ptr; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
    if (!GET_ALLOC(HDRP(bp)) && GET_SIZE(HDRP(bp)) >= asize) {
      last_ptr = bp;
      return bp;
    }
  }
  for (char *bp = heap_listp; bp != last_ptr; bp = NEXT_BLKP(bp)) {
    if (!GET_ALLOC(HDRP(bp)) && GET_SIZE(HDRP(bp)) >= asize) {
      last_ptr = bp;
      return bp;
    }
  }
  return NULL;
}

static void place(void *bp, size_t asize) {
  size_t left = GET_SIZE(HDRP(bp)) - asize;
  PUT(HDRP(bp), PACK(GET_SIZE(HDRP(bp)), 1));
  PUT(FTRP(bp), PACK(GET_SIZE(HDRP(bp)), 1));
  if (left > DSIZE * 2) {
    PUT(HDRP(bp), PACK(asize, 1));
    PUT(FTRP(bp), PACK(asize, 1));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(left, 0));
    PUT(FTRP(NEXT_BLKP(bp)), PACK(left, 0));
  }
  return;
}

int mm_init(void) {
  if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1) {
    return -1;
  }
  PUT(heap_listp, 0);
  PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1));
  PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1));
  PUT(heap_listp + (3 * WSIZE), PACK(0, 1));
  heap_listp += (2 * WSIZE);
  last_ptr = heap_listp;
  if (extend_heap(CHUNKSIZE / WSIZE) == NULL) {
    return -1;
  }
  return 0;
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size) {
  size_t asize;
  size_t extendsize;
  char *bp;
  if (size == 0) {
    return NULL;
  }
  if (size <= DSIZE) {
    asize = 2 * DSIZE;
  } else {
    asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);
  }
  if ((bp = find_fit(asize)) != NULL) {
    place(bp, asize);
    return bp;
  }
  extendsize = MAX(asize, CHUNKSIZE);
  if ((bp = extend_heap(extendsize / WSIZE)) == NULL) {
    return NULL;
  }
  place(bp, asize);
  return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr) {
  size_t size = GET_SIZE(HDRP(ptr));
  PUT(HDRP(ptr), PACK(size, 0));
  PUT(FTRP(ptr), PACK(size, 0));
  coalesce(ptr);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size) {
  size_t asize;
  if (ptr == NULL) {
    return mm_malloc(size);
  }
  if (size == 0) {
    mm_free(ptr);
    return NULL;
  }
  if (size <= DSIZE) {
    asize = 2 * DSIZE;
  } else {
    asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);
  }
  if (GET_SIZE(HDRP(ptr)) == asize) {
    return ptr;
  }
  if (GET_SIZE(HDRP(ptr)) > asize) {
    size_t left = GET_SIZE(HDRP(ptr)) - asize;
    PUT(HDRP(ptr), PACK(GET_SIZE(HDRP(ptr)), 1));
    PUT(FTRP(ptr), PACK(GET_SIZE(HDRP(ptr)), 1));
    size_t nextleft =
        GET_ALLOC(HDRP(NEXT_BLKP(ptr))) ? 0 : GET_SIZE(HDRP(NEXT_BLKP(ptr)));
    if (left + nextleft > DSIZE * 2) {
      if(last_ptr == NEXT_BLKP(ptr)){
        last_ptr = ptr + asize;
      }
      PUT(HDRP(ptr), PACK(asize, 1));
      PUT(FTRP(ptr), PACK(asize, 1));
      PUT(HDRP(NEXT_BLKP(ptr)), PACK(left + nextleft, 0));
      PUT(FTRP(NEXT_BLKP(ptr)), PACK(left + nextleft, 0));
    }
    return ptr;
  }
  if (GET_SIZE(HDRP(ptr)) < asize) {
    char *nextHDRP = HDRP(NEXT_BLKP(ptr));
    if (GET_ALLOC(nextHDRP) == 0 &&
        GET_SIZE(nextHDRP) + GET_SIZE(HDRP(ptr)) >= asize) {
      size_t left = GET_SIZE(nextHDRP) + GET_SIZE(HDRP(ptr)) - asize;
      PUT(HDRP(ptr), PACK(left + asize, 1));
      PUT(FTRP(ptr), PACK(left + asize, 1));
      if (left > DSIZE * 2) {
        if(last_ptr == NEXT_BLKP(ptr)){
          last_ptr = ptr + asize;
        }
        PUT(HDRP(ptr), PACK(asize, 1));
        PUT(FTRP(ptr), PACK(asize, 1));
        PUT(HDRP(NEXT_BLKP(ptr)), PACK(left, 0));
        PUT(FTRP(NEXT_BLKP(ptr)), PACK(left, 0));
      }
      return ptr;
    }
    char *oldptr = ptr;
    char *newptr;
    size_t copySize;
    newptr = mm_malloc(asize);
    if (newptr == NULL) {
      return NULL;
    }
    copySize = GET_SIZE(HDRP(oldptr)) - WSIZE;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
  }
  return NULL;
}
