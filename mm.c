/*
 * mm.c
 * 徐梓文 2410306105
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mm.h"
#include "memlib.h"

/* If you want debugging output, use the following macro.  When you hand
 * in, remove the #define DEBUG line. */
#define DEBUG
#ifdef DEBUG
# define dbg_printf(...) printf(__VA_ARGS__)
#else
# define dbg_printf(...)
#endif

/* do not change the following! */
#ifdef DRIVER
/* create aliases for driver tests */
#define malloc mm_malloc
#define free mm_free
#define realloc mm_realloc
#define calloc mm_calloc
#endif /* def DRIVER */

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(p) (((size_t)(p) + (ALIGNMENT-1)) & ~0x7)

#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1 << 12) /* 每次堆扩展的大小 */

#define MAX(x, y) ((x) > (y) ? (x) : (y))

/* 将大小和分配位打包 */
#define PACK(size, alloc) ((size) | (alloc))

/* 在地址 p 处读写数据 */
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

/* 从头部或脚部读取大小和分配位 */
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* 给定 payload 指针 bp，计算出头部和尾部的位置 */
#define HDRP(bp) ((char *)(bp) - WSIZE) /* 一个 header 占 4 字节 */
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* 给定 bp，计算上一个块或下一个块的 bp */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE((char *)(bp) - WSIZE)) /* bp 加上当前块大小 */
#define PREV_BLOP(bp) ((char *)(bp) - GET_SIZE((char *)(bp) - WSIZE))

static char *heap_listp; /* 指向堆的起始位置 */

/* 向系统申请堆内存，当堆内存不够时使用 */
static void *extend_heap(size_t words) {
    char *bp;
    size_t size;

    /* 保证申请的字节是与 8 对齐的 */
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;

    /* 调用 mem_sbrk 申请空间 */
    /* 这里 bp 指向原来的堆顶，也就是尾块的后面，此时回退 WSIZE 就到了尾块的开头 */
    if ((long)(bp = memsbrk(size)) == -1) {
        return NULL;
    }

    /* 初始化堆，其中有一个序言块，只包含 header 和 footer */
    /* 由于 8 字节对齐，每个 header 的开始都需要是 8n + 4，所以需要有 WSIZE 的 padding */
    /* 最后还有一个 WSIZE 的尾块，内容为 0/1 */
    /* 覆盖先前的尾部 */
    /* 创建一个大的空闲块 */
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    /* 创建尾块 */
    PUT(HDBP(NEXT_BLKP(bp)), PACK(0, 1));

    /* 如果前一个块是空闲的，就把他俩合并 */
    return coalesce(bp);
}

static void *coalesce(void *bp) {
    return bp;
}

/*
 * Initialize: return -1 on error, 0 on success.
 */
int mm_init(void) {
    return 0;
}

/*
 * malloc
 */
void *malloc (size_t size) {
    return NULL;
}

/*
 * free
 */
void free (void *ptr) {
    if(!ptr) return;
}

/*
 * realloc - you may want to look at mm-naive.c
 */
void *realloc(void *oldptr, size_t size) {
    return NULL;
}

/*
 * calloc - you may want to look at mm-naive.c
 * This function is not tested by mdriver, but it is
 * needed to run the traces.
 */
void *calloc (size_t nmemb, size_t size) {
    return NULL;
}


/*
 * Return whether the pointer is in the heap.
 * May be useful for debugging.
 */
static int in_heap(const void *p) {
    return p <= mem_heap_hi() && p >= mem_heap_lo();
}

/*
 * Return whether the pointer is aligned.
 * May be useful for debugging.
 */
static int aligned(const void *p) {
    return (size_t)ALIGN(p) == (size_t)p;
}

/*
 * mm_checkheap
 */
void mm_checkheap(int lineno) {
}
