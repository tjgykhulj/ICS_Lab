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
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "5130309006",
    /* First member's full name */
    "Tang JingGe",
    /* First member's email address */
    "tjg-6666@163.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8
#define OVERHEAD 24

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1<<8)

#define MAX(x,y) ((x)>(y)?(x):(y))

#define GET(p) (*(size_t *)(p))
#define PUT(p,val) (GET(p) = (size_t) val)
/*-------------------------------------------------------------*/

#define SIZE(p) (GET(p) & ~7)
#define ALLOC(p) (GET(p) & 1)
#define PACK(size,alloc) ((size)|(alloc))
/*-------------------------------------------------------------*/
#define HDRP(p) ((void*)p - WSIZE)
#define PREV(p) ((void*)p)
#define NEXT(p) ((void*)p + WSIZE)
#define BUDD(p) ((void*)p + DSIZE)
#define FTRP(p) (NEXT_BLKP(p) - DSIZE)
/*-------------------------------------------------------------*/
#define GET_SIZE(p) (SIZE(HDRP(p)))
#define GET_ALLOC(p) (ALLOC(HDRP(p)))
#define GET_PREV(p) ((void*)GET(PREV(p)))
#define GET_NEXT(p) ((void*)GET(NEXT(p)))
#define GET_BUDD(p) ((void*)GET(BUDD(p)))
/*-------------------------------------------------------------*/
#define PREV_BLKP(bp) ((void*)(bp) - SIZE((void*)(bp)-DSIZE))
#define NEXT_BLKP(bp) ((void*)(bp) + SIZE((void*)(bp)-WSIZE))

static size_t flag = 0; 
static void *heap_listp;
static void Insert(void *);
static void Delete(void *);
static void coaNext(void *);
static void *coalesce(void *);
static void *extend_heap(size_t);
static void *find_fit(size_t );
static void *place(void *, size_t );

static void Insert(void *bp)
{
	PUT(BUDD(bp),NULL);	//兄弟表示大小一样的块，可将大小相同的块用buddy链连起来，查找时能更高效
	void *i, *t;
	for (t=heap_listp; t; t=GET_NEXT(t))
		if (GET_SIZE(bp)==GET_SIZE(t))	//若找到兄弟块，则插入此块
		{
			void *b = GET_BUDD(t);
			PUT(BUDD(t),bp);
			PUT(PREV(bp),t);
			PUT(NEXT(bp),b);
			if (b) PUT(PREV(b),bp); return;
		}
	i = heap_listp;	//若遍历全链都没找到相同大小的块，则新建一个在链表开头
	PUT(PREV(bp),NULL);
	PUT(NEXT(bp),i);
	if (i!=NULL) PUT(PREV(i),bp);
	heap_listp = bp;
}
static void Delete(void *bp)
{
	void *b = GET_BUDD(bp);
	void *p = GET_PREV(bp);
	void *n = GET_NEXT(bp);
	if (b) {	//若有兄弟则它在主链上，若无则在兄弟链上。
		PUT(PREV(b),p);
		PUT(BUDD(b),GET_NEXT(b));
		PUT(NEXT(b),n);
		if (p) PUT(NEXT(p),b); else heap_listp = b;
		if (n) PUT(PREV(n),b);
	} else {
		if (n) PUT(PREV(n),p);
		if (p) 
			if (GET_BUDD(p)==bp) PUT(BUDD(p),n); else PUT(NEXT(p),n);
		else 
			heap_listp = n;
	}
}

static void coaNext(void *bp)		/*将bp与后继合并*/
{
	void *NT = NEXT_BLKP(bp);
	size_t size = GET_SIZE(bp) + GET_SIZE(NT);
	PUT(HDRP(bp), PACK(size,0));
	PUT(FTRP(bp), PACK(size,0));
	Delete(NT);
}
static void *coalesce(void *bp)
{
	if (!GET_ALLOC(NEXT_BLKP(bp))) coaNext(bp);
	if (!GET_ALLOC(PREV_BLKP(bp))) bp = PREV_BLKP(bp), coaNext(bp);
	Delete(bp); Insert(bp);
	return bp;
}
static void *extend_heap(size_t words)
{
	size_t size;
	size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
	void *bp;
	if ((long)(bp = mem_sbrk(size)) == -1) return NULL;	//扩展一个大小为size的块并设为空，然后将此块插入链表并合并
	PUT(HDRP(bp), PACK(size, 0));
	PUT(FTRP(bp), PACK(size, 0));
	PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));
	Insert(bp);
	return coalesce(bp);
}
static void *find_fit(size_t asize)			//find_best_fit
{
	void *bp, *fit = NULL;
	int value = 1<<28;
	for (bp=heap_listp; bp; bp = GET_NEXT(bp))
		if (asize <= GET_SIZE(bp) &&
			value > GET_SIZE(bp))
		{
			value = GET_SIZE(bp);
			fit = bp;
			if (value == asize) break;
		}
	return fit;
}
static void *place(void *bp, size_t asize)
{
	size_t csize = GET_SIZE(bp);
	if ((csize - asize) >= (DSIZE + OVERHEAD))	//若放置后，后面的空白格足以形成一个空块，则切割，否则分配。 
	{
		Delete(bp);
		PUT(HDRP(bp), PACK(csize-asize,0));
		PUT(FTRP(bp), PACK(csize-asize,0));
		Insert(bp);
		bp = NEXT_BLKP(bp);
		PUT(HDRP(bp), PACK(asize,1));
		PUT(FTRP(bp), PACK(asize,1));
	} else {
		PUT(HDRP(bp), PACK(csize,1));
		PUT(FTRP(bp), PACK(csize,1));
		Delete(bp);
	}
	return bp;
}

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
	void *bp;
	if ((bp = mem_sbrk(8*WSIZE)) == (void*) -1)
		return -1;
	PUT(bp, 0);
	PUT(bp+(1*WSIZE), PACK(6*WSIZE,1));
	PUT(bp+(2*WSIZE), 0);
	PUT(bp+(3*WSIZE), 0);
	PUT(bp+(4*WSIZE), 0);
	PUT(bp+(5*WSIZE), 0);
	PUT(bp+(6*WSIZE), PACK(6*WSIZE,1));
	PUT(bp+(7*WSIZE), PACK(0,1));		//放置一个空块
	heap_listp = NULL;
	if (extend_heap(CHUNKSIZE/WSIZE)==NULL) return -1;
	return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)	//PPT原码
{
	size_t asize;
	size_t extendsize;
	void *bp;

	if (size<=0) return NULL;
	
	asize = OVERHEAD + ALIGN(size);
	if ((bp = find_fit(asize)) != NULL)
		return place(bp,asize);

	extendsize = MAX(asize,CHUNKSIZE);
	if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
		return NULL;
	return place(bp,asize);
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)	//将块置空，然后将空块插入链表，合并块
{
	PUT(HDRP(bp),GET_SIZE(bp));
	PUT(FTRP(bp),GET_SIZE(bp));
	Insert(bp);
	coalesce(bp);
}

/*;
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
	if (size == 0) { mm_free(ptr); return ptr; }
	if (ptr == NULL) return mm_malloc(size);

	void *oldptr = ptr;
	int osize = GET_SIZE(ptr);
	int nsize = OVERHEAD + ALIGN(size);

	int w = osize;
	void *NT = NEXT_BLKP(ptr);
	if (!GET_ALLOC(NT)) w+=GET_SIZE(NT);	//先计算出与后面的空块（如果有的话）合并后得到的长度

	if (w < nsize) {			//若长度不够分配，则malloc一个新块
		ptr = mm_malloc(size);
		if (size < osize) osize = size;
		if (ptr != NULL) memcpy(ptr, oldptr, osize);
		mm_free(oldptr);
		return ptr;
	}
	if (!GET_ALLOC(NT)) Delete(NT);
	if ((w - nsize) >= (DSIZE + OVERHEAD)) 	//与place相同，但分配块的方式有略微差别
	{
		PUT(HDRP(ptr), PACK(nsize,1));
		PUT(FTRP(ptr), PACK(nsize,1));
		ptr = NEXT_BLKP(ptr);
		PUT(HDRP(ptr), PACK(w-nsize,0));
		PUT(FTRP(ptr), PACK(w-nsize,0));
		Insert(ptr);
	} else {
		PUT(HDRP(ptr), PACK(w,1));
		PUT(FTRP(ptr), PACK(w,1));
	}
	return oldptr;
}
