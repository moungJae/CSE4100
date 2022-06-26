/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 *
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

 /*********************************************************
  * NOTE TO STUDENTS: Before you do anything else, please
  * provide your information in the following struct.
  ********************************************************/
team_t team = {
	/* Your student ID */
	"20192138",
	/* Your full name*/
	"Moungjae Joe",
	/* Your email address */
	"cmj092222@sogang.ac.kr",
};

/* $begin mallocmacros */
/* Basic constants and macros */
#define WSIZE       	4       /* Word and header/footer size (bytes) */ //line:vm:mm:beginconst
#define DSIZE       	8       /* Doubleword size (bytes) */
#define CHUNKSIZE  		4096  /* Extend heap by this amount (bytes) */  //line:vm:mm:endconst 
#define REALLOC_BUFFER	256
#define TOTAL_LIST		16

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc)  ((size) | (alloc)) //line:vm:mm:pack

#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define MIN(x, y) ((x) < (y) ? (x) : (y))

/* Read and write a word at address p */
#define GET(p)				(*(unsigned int *)(p))            //line:vm:mm:get
#define PUT(p, val)			(*(unsigned int *)(p) = (val))    //line:vm:mm:put
#define SET_PTR(p, ptr)		(*(unsigned int *)(p) = (unsigned int)(ptr))
#define PUT_TAG(p, val)		(*(unsigned int *)(p) = (val) | GET_TAG(p))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)			(GET(p) & ~0x7)               //line:vm:mm:getsize
#define GET_ALLOC(p)		(GET(p) & 0x1)                //line:vm:mm:getalloc
#define GET_TAG(p)			(GET(p) & 0x2)
#define SET_TAG(p)			(GET(p) | 0x2)
#define REMOVE_TAG(p)		(GET(p) &= ~0x2)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)       ((char *)(bp) - WSIZE)                      //line:vm:mm:hdrp
#define FTRP(bp)       ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) //line:vm:mm:ftrp

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp)  ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE))) //line:vm:mm:nextblkp
#define PREV_BLKP(bp)  ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE))) //line:vm:mm:prevblkp
/* $end mallocmacros */

#define PRED_PTR(bp)	((char *)(bp))
#define SUCC_PTR(bp)	((char *)(bp) + WSIZE)

#define PRED(bp)		(*(char **)(bp))
#define SUCC(bp)		(*(char **)(bp + WSIZE))

static void* seg_list_1, *seg_list_2, *seg_list_3, *seg_list_4;
static void* seg_list_5, * seg_list_6, * seg_list_7, * seg_list_8;
static void* seg_list_9, * seg_list_10, * seg_list_11, * seg_list_12;
static void* seg_list_13, * seg_list_14, * seg_list_15, * seg_list_16;

static void* extend_heap(size_t words);
static void add_seglist(char* bp, size_t size);
static void remove_seglist(char* bp);
static void* coalesce(void* bp);
static void* place(void* bp, size_t asize);
int mm_check();

/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
	void** seg_lists[TOTAL_LIST] = {
		&seg_list_1, &seg_list_2, &seg_list_3, &seg_list_4,
		&seg_list_5, &seg_list_6, &seg_list_7, &seg_list_8,
		&seg_list_9, &seg_list_10, &seg_list_11, &seg_list_12,
		&seg_list_13, &seg_list_14, &seg_list_15, &seg_list_16,
	};

	void* start;

	if ((start = mem_sbrk(4 * WSIZE)) == (void*)-1)
		return -1; // Error

	PUT(start, 0);
	PUT(start + WSIZE, PACK(DSIZE, 1));
	PUT(start + (2 * WSIZE), PACK(DSIZE, 1));
	PUT(start + (3 * WSIZE), PACK(0, 1));

	for (int i = 0; i < TOTAL_LIST; i++) {
		*seg_lists[i] = NULL;
	}

	if (extend_heap(CHUNKSIZE) == NULL)
		return -1; // Error

	return 0; // No Error
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void* mm_malloc(size_t size)
{
	void** seg_lists[TOTAL_LIST] = {
		&seg_list_1, &seg_list_2, &seg_list_3, &seg_list_4,
		&seg_list_5, &seg_list_6, &seg_list_7, &seg_list_8,
		&seg_list_9, &seg_list_10, &seg_list_11, &seg_list_12,
		&seg_list_13, &seg_list_14, &seg_list_15, &seg_list_16,
	};

	size_t asize;      /* Adjusted block size */
	size_t extendsize; /* Amount to extend heap if no fit */
	void* bp = NULL;
	int i = 0;

	/* Ignore spurious requests */
	if (size == 0)
		return NULL;

	/* Adjust block size to include overhead and alignment reqs. */
	if (size <= DSIZE)                                          //line:vm:mm:sizeadjust1
		asize = 2 * DSIZE;                                        //line:vm:mm:sizeadjust2
	else
		asize = DSIZE * ((size + (DSIZE)+(DSIZE - 1)) / DSIZE); //line:vm:mm:sizeadjust3

	for (i = 0; i < TOTAL_LIST; i++) {
		if (*seg_lists[i] != NULL && ((asize <= (1 << (i + 4))) || i == (TOTAL_LIST - 1))) {
			bp = *seg_lists[i];
			while (bp != NULL && ((asize > GET_SIZE(HDRP(bp))) || GET_TAG(HDRP(bp)))) {
				bp = SUCC(bp);
			}
			if (bp != NULL) {
				return place(bp, asize);
			}
		}
	}

	/* No fit found. Get more memory and place the block */
	extendsize = MAX(asize, CHUNKSIZE);                 //line:vm:mm:growheap1
	if ((bp = extend_heap(extendsize)) == NULL)
		return NULL;                                  //line:vm:mm:growheap2

	return place(bp, asize);                                 //line:vm:mm:growheap3
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void* ptr)
{
	/* $end mmfree */
	if (ptr == 0)
		return;

	/* $begin mmfree */
	size_t size = GET_SIZE(HDRP(ptr));
	/* $end mmfree */

	PUT_TAG(HDRP(ptr), PACK(size, 0));
	PUT_TAG(FTRP(ptr), PACK(size, 0));
	REMOVE_TAG(HDRP(NEXT_BLKP(ptr)));

	add_seglist(ptr, size);
	coalesce(ptr);

	//if (mm_check() == -1)
	//	exit(0);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void* mm_realloc(void* ptr, size_t size)
{
	size_t new_size;
	void* new_ptr;
	int remainder;
	int extend_size;
	int block_size;

	/* If size == 0 then this is just free, and we return NULL. */
	if (size == 0) {
		mm_free(ptr);
		return 0;
	}

	/* If oldptr is NULL, then this is just malloc. */
	if (ptr == NULL) {
		return mm_malloc(size);
	}

	new_ptr = ptr;
	new_size = size;

	if (new_size <= DSIZE) {
		new_size = 2 * DSIZE;
	}
	else {
		new_size = DSIZE * ((size + (DSIZE)+(DSIZE - 1)) / DSIZE);
	}

	new_size += REALLOC_BUFFER;
	block_size = GET_SIZE(HDRP(ptr)) - new_size;

	if (block_size < 0) {
		if (GET_ALLOC(HDRP(NEXT_BLKP(ptr))) && GET_SIZE(HDRP(NEXT_BLKP(ptr)))) {
			new_ptr = mm_malloc(new_size - DSIZE);
			memcpy(new_ptr, ptr, MIN(size, new_size));
			mm_free(ptr);
		}
		else {
			remainder = GET_SIZE(HDRP(ptr)) + GET_SIZE(HDRP(NEXT_BLKP(ptr))) - new_size;
			if (remainder < 0) {
				extend_size = MAX(CHUNKSIZE, (-1) * remainder);
				if ((extend_heap(extend_size)) == NULL)
					return 0;
				remainder += extend_size;
			}
			remove_seglist(NEXT_BLKP(ptr));
			PUT(HDRP(ptr), PACK(new_size + remainder, 1));
			PUT(FTRP(ptr), PACK(new_size + remainder, 1));
		}
		block_size = GET_SIZE(HDRP(new_ptr)) - new_size;
	}

	if (block_size < 2 * REALLOC_BUFFER) {
		SET_TAG(HDRP(NEXT_BLKP(new_ptr)));
	}

	return new_ptr;
}

int mm_check()
{
	void** seg_lists[TOTAL_LIST] = {
		&seg_list_1, &seg_list_2, &seg_list_3, &seg_list_4,
		&seg_list_5, &seg_list_6, &seg_list_7, &seg_list_8,
		&seg_list_9, &seg_list_10, &seg_list_11, &seg_list_12,
		&seg_list_13, &seg_list_14, &seg_list_15, &seg_list_16,
	};

	void* head;
	int result = 1;

	for (int i = 0; i < TOTAL_LIST; i++) {
		if (*seg_lists[i] == NULL)
			continue;
		head = *seg_lists[i];
		while (head != NULL) {
			if (GET_SIZE(HDRP(head)) != GET_SIZE(FTRP(head))) {
				printf("Header's size and Footer's size are not equal.\n");
				printf("Result(header, footer) : [%d], [%d]\n", GET_SIZE(HDRP(head)), GET_SIZE(FTRP(head)));
				result = -1;
			}
			else {
				if ((i == TOTAL_LIST - 1) && (GET_SIZE(HDRP(head)) <= (1 << (i + 3)))) {
					printf("Last seg_list case : Header's size is small.\n");
					printf("Expected size : [%d, INF], Header's size : [%d]\n", (1 << (i + 3)), GET_SIZE(HDRP(head)));
					result = -1;
				}
				else if ((i == 0) && (GET_SIZE(HDRP(head)) != 16)) {
					printf("First seg_list case : Header's size is not 16.\n");
					printf("Expected size : [16], Header's size : [%d]\n", GET_SIZE(HDRP(head)));
					result = -1;
				}
				else if ((i != TOTAL_LIST - 1) && (GET_SIZE(HDRP(head)) <= (1 << (i + 3)) || GET_SIZE(HDRP(head)) > (1 << (i + 4)))) {
					printf("Case except of first and last seg_list : Header's size is small or large.\n");
					printf("Expected size : [%d, %d], Header's size : [%d]\n", (1 << (i + 3)) + 1, (1 << (i + 4)), GET_SIZE(HDRP(head)));
					result = -1;
				}
			}
			head = SUCC(head);
		}
	}

	return result;
}

// seg_list_1 : [2^4, 2^4]
// seg_list_2 : [2^4 + 1, 2^5]
// ...
// seg_list_16 : [2^18 + 1, INF]
static void add_seglist(char* bp, size_t size)
{
	void** seg_lists[TOTAL_LIST] = {
		&seg_list_1, &seg_list_2, &seg_list_3, &seg_list_4,
		&seg_list_5, &seg_list_6, &seg_list_7, &seg_list_8,
		&seg_list_9, &seg_list_10, &seg_list_11, &seg_list_12,
		&seg_list_13, &seg_list_14, &seg_list_15, &seg_list_16,
	};

	void* head = NULL;
	void* before = NULL;
	int i = 0;

	for (i = 0; i < TOTAL_LIST; i++) {
		if (size <= (1 << (i + 4)) || i == (TOTAL_LIST - 1)) {
			break;
		}
	}

	head = *seg_lists[i];
	while ((head) && (size > GET_SIZE(HDRP(head)))) {
		before = head;
		head = SUCC(head);
	}

	if (head) {
		if (before) { /* Mid block */
			SET_PTR(PRED_PTR(bp), before);
			SET_PTR(SUCC_PTR(before), bp);
			SET_PTR(SUCC_PTR(bp), head);
			SET_PTR(PRED_PTR(head), bp);
		}
		else { /* First block */
			SET_PTR(PRED_PTR(bp), NULL);
			SET_PTR(SUCC_PTR(bp), head);
			SET_PTR(PRED_PTR(head), bp);
			*seg_lists[i] = bp;
		}
	}
	else {
		if (before) { /* Last block */
			SET_PTR(PRED_PTR(bp), before);
			SET_PTR(SUCC_PTR(bp), NULL);
			SET_PTR(SUCC_PTR(before), bp);
		}
		else { /* All empty */
			SET_PTR(PRED_PTR(bp), NULL);
			SET_PTR(SUCC_PTR(bp), NULL);
			*seg_lists[i] = bp;
		}
	}
}

static void remove_seglist(char* bp)
{
	void** seg_lists[TOTAL_LIST] = {
		&seg_list_1, &seg_list_2, &seg_list_3, &seg_list_4,
		&seg_list_5, &seg_list_6, &seg_list_7, &seg_list_8,
		&seg_list_9, &seg_list_10, &seg_list_11, &seg_list_12,
		&seg_list_13, &seg_list_14, &seg_list_15, &seg_list_16,
	};

	int i = 0;
	size_t size = GET_SIZE(HDRP(bp));

	for (i = 0; i < TOTAL_LIST; i++) {
		if (size <= (1 << (i + 4)) || i == (TOTAL_LIST - 1)) {
			break;
		}
	}

	if (SUCC(bp)) {
		if (PRED(bp)) { /* Mid block */
			SET_PTR(SUCC_PTR(PRED(bp)), SUCC(bp));
			SET_PTR(PRED_PTR(SUCC(bp)), PRED(bp));
		}
		else { /* First block */
			SET_PTR(PRED_PTR(SUCC(bp)), NULL);
			*seg_lists[i] = SUCC(bp);
		}
	}
	else {
		if (PRED(bp)) { /* Last block */
			SET_PTR(SUCC_PTR(PRED(bp)), NULL);
		}
		else { /* Empty */
			*seg_lists[i] = NULL;
		}
	}
}

static void* coalesce(void* bp)
{
	size_t prev_alloc = GET_ALLOC(HDRP(PREV_BLKP(bp))) || GET_TAG(HDRP(PREV_BLKP(bp)));
	size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
	size_t size = GET_SIZE(HDRP(bp));

	if (prev_alloc && next_alloc) {            /* Case 1 */
		return bp;
	}
	else if (prev_alloc && !next_alloc) {      /* Case 2 */
		remove_seglist(bp);
		remove_seglist(NEXT_BLKP(bp));

		size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
		PUT_TAG(HDRP(bp), PACK(size, 0));
		PUT_TAG(FTRP(bp), PACK(size, 0));

		add_seglist(bp, size);
	}
	else if (!prev_alloc && next_alloc) {      /* Case 3 */
		remove_seglist(PREV_BLKP(bp));
		remove_seglist(bp);

		size += GET_SIZE(HDRP(PREV_BLKP(bp)));
		PUT_TAG(FTRP(bp), PACK(size, 0));
		PUT_TAG(HDRP(PREV_BLKP(bp)), PACK(size, 0));
		bp = PREV_BLKP(bp);

		add_seglist(bp, size);
	}
	else {                                     /* Case 4 */
		remove_seglist(PREV_BLKP(bp));
		remove_seglist(bp);
		remove_seglist(NEXT_BLKP(bp));

		size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(HDRP(NEXT_BLKP(bp)));
		PUT_TAG(HDRP(PREV_BLKP(bp)), PACK(size, 0));
		PUT_TAG(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
		bp = PREV_BLKP(bp);

		add_seglist(bp, size);
	}

	return bp;
}

static void* extend_heap(size_t words)
{
	void* bp;
	size_t asize;

	asize = (size_t)(words + DSIZE - 1) & ~0x7;
	if ((bp = mem_sbrk(asize)) == (void*)-1)
		return NULL;

	PUT(HDRP(bp), PACK(asize, 0));
	PUT(FTRP(bp), PACK(asize, 0));
	PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));
	add_seglist(bp, asize);

	return coalesce(bp);
}

static void* place(void* bp, size_t asize)
{
	size_t csize = GET_SIZE(HDRP(bp));

	remove_seglist(bp);

	if ((csize - asize) >= (2 * DSIZE)) { /* block division */
		if (asize < 100)
		{
			PUT_TAG(HDRP(bp), PACK(asize, 1));
			PUT_TAG(FTRP(bp), PACK(asize, 1));

			PUT(HDRP(NEXT_BLKP(bp)), PACK(csize - asize, 0));
			PUT(FTRP(NEXT_BLKP(bp)), PACK(csize - asize, 0));
			add_seglist(NEXT_BLKP(bp), csize - asize);

			//if (mm_check() == -1)
			//	exit(0);
			return bp;
		}
		else
		{
			PUT_TAG(HDRP(bp), PACK(csize - asize, 0));
			PUT_TAG(FTRP(bp), PACK(csize - asize, 0));

			PUT(HDRP(NEXT_BLKP(bp)), PACK(asize, 1));
			PUT(FTRP(NEXT_BLKP(bp)), PACK(asize, 1));
			add_seglist(bp, csize - asize);

			//if (mm_check() == -1)
			//	exit(0);
			return NEXT_BLKP(bp);
		}
	}
	else { /* block division x */
		PUT_TAG(HDRP(bp), PACK(csize, 1));
		PUT_TAG(FTRP(bp), PACK(csize, 1));

		//if (mm_check() == -1)
		//	exit(0);
		return bp;
	}
}