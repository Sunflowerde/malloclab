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
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE((char *)(bp) - WSIZE))

static char *heap_listp; /* 指向堆的起始位置 */

static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);

/* 向系统申请堆内存，当堆内存不够时使用 */
/* 这里的参数表示申请多少个字的内存，而不是字节 */
static void *extend_heap(size_t words) {
    char *bp;
    size_t size;

    /* 保证申请的字节是与 8 对齐的 */
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;

    /* 调用 mem_sbrk 申请空间 */
    /* 这里 bp 指向原来的堆顶，也就是尾块的后面，此时回退 WSIZE 就到了尾块的开头 */
    if ((long)(bp = mem_sbrk(size)) == -1) {
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
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

    /* 如果前一个块是空闲的，就把他俩合并 */
    return coalesce(bp);
}

/* 合并空闲块，分 4 种情况处理 */
static void *coalesce(void *bp) {
    /* 获取前后块的分配状态 */
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    
    /* 当前块的大小 */
    size_t size = GET_SIZE(HDRP(bp));

    /* 只有当前面块发生合并时才需要移动 bp */
    /* 前后都被占用 */
    if (prev_alloc && next_alloc) {
        return bp;
    }
    /* 前面被占用后面空闲 */
    else if (prev_alloc && !next_alloc) {
        /* 更新块大小 */
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        /* 更新头部和尾部 */
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0)); /* 脚部的更新依赖于头部 */
    }
    /* 前面空闲后面被占用 */
    else if (!prev_alloc && next_alloc) {
        size += GET_SIZE(FTRP(PREV_BLKP(bp)));
        /* 更新当前尾部 */
        PUT(FTRP(bp), PACK(size, 0));
        /* 更新前一个头部 */
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        /* 更新 bp */
        bp = PREV_BLKP(bp);
    }

    /* 前后均空闲 */
    else if (!prev_alloc && !next_alloc) {
        size += GET_SIZE(FTRP(PREV_BLKP(bp))) + GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    return bp;
}

/* 找到第一个满足条件的内存块 */
static void *find_fit(size_t asize) {
    void *bp;

    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        if (!GET_ALLOC(HDRP(bp)) && asize >= GET_SIZE(HDRP(bp))) {
            return bp;
        }
    }

    return NULL;
}

static void place(void *bp, size_t asize) {
    size_t csize = GET_SIZE(HDRP(bp));

    /* 如果能分配出一个小块，就把他拆分 */
    if ((csize - asize) >= (2 * DSIZE)) {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        /* 更新被分割出的小块 */
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));
    }
    /* 如果剩余内存不够大 */
    else {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
    }
}

/*
 * Initialize: return -1 on error, 0 on success.
 */
int mm_init(void) {
    /* 先申请 4 个字的空间给 padding，序言块，尾块 */
    /* 此时 heap_listp 指向堆的最开头 */
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1) {
        return -1;
    }

    /* 第一个 padding 全为 0 */
    PUT(heap_listp, 0);
    /* 序言块的 header 和 footer */
    PUT(heap_listp + 1 * WSIZE, PACK(8, 0));
    PUT(heap_listp + 2 * WSIZE, PACK(8, 0));
    /* 尾块 */
    PUT(heap_listp + 3 * WSIZE, PACK(0, 1));

    /* 让 heap_listp 指向序言块的 header 和 footer 的中间 */
    heap_listp += 2 * WSIZE;

    /* 然后扩展一大块内存 */
    if (extend_heap(CHUNKSIZE / WSIZE) == NULL) {
        return -1;
    }

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
