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
#define MIN_BLOCK_SIZE 24

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
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE((char *)(bp) - DSIZE))

#define NUM_CLASSES 10 /* 大小类数量 */
static char *heap_listp; /* 指向堆的起始位置 */
static char *free_lists[NUM_CLASSES]; /* 分离空闲链表数组 */


/* 获取空闲块中的前驱或后继指针 */
#define PREV_FREE_BLKP(bp) (*(char **)(bp))
#define NEXT_FREE_BLKP(bp) (*(char **)((char *)(bp) + DSIZE)) /* 后继指针位于前驱指针一个字后 */

/* 设置空闲块的前驱和后继 */
#define SET_PREV_FREE(bp, ptr) (*(char **)(bp) = (ptr))
#define SET_NEXT_FREE(bp, ptr) (*(char **)((char *)(bp) + DSIZE) = (ptr))

static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);
static int get_class_index(size_t size);
static void insert_free_block(void *bp);
static void remove_free_block(void *bp);

/* 根据块大小确定对应的大小类索引 */
static int get_class_index(size_t size) {
    if (size <= 16) return 0;
    if (size <= 32) return 1;
    if (size <= 64) return 2;
    if (size <= 128) return 3;
    if (size <= 256) return 4;
    if (size <= 512) return 5;
    if (size <= 1024) return 6;
    if (size <= 2048) return 7;
    if (size <= 4096) return 8;
    return 9;
}

/* 将空闲块插入到对应的空闲链表（LIFO），插入到链表头部 */
static void insert_free_block(void *bp) {
    size_t size = GET_SIZE(HDRP(bp));
    int index = get_class_index(size);
    char *head = free_lists[index];

    /* 插入到链表头部，前驱为 NULL，后继为原链表的表头 */
    SET_PREV_FREE(bp, NULL);
    SET_NEXT_FREE(bp, head);

    /* 如果链表非空，则更新原头节点的前驱 */
    if (head != NULL) {
        SET_PREV_FREE(head, bp);
    }

    /* 更新链表头 */
    free_lists[index] = bp;
}

/* 从空闲链表中移除指定块 */
static void remove_free_block(void *bp) {
    size_t size = GET_SIZE(HDRP(bp));
    int index = get_class_index(size);

    char *prev = PREV_FREE_BLKP(bp);
    char *next = NEXT_FREE_BLKP(bp);

    /* 更新前驱节点的后继 */
    /* 如果 bp 是链表头 */
    if (prev == NULL) {
        free_lists[index] = next;
    } else {
        SET_NEXT_FREE(prev, next);
    }

    /* 更新后继结点的前驱 */
    if (next != NULL) {
        SET_PREV_FREE(next, prev);
    }
}

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
    char *prev_bp = PREV_BLKP(bp);
    char *next_bp = NEXT_BLKP(bp);

    /* 获取前后块的分配状态 */
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    /* 前后都被占用 */
    if (prev_alloc && next_alloc) {
        /* 直接插入空闲链表 */
        insert_free_block(bp);
        return bp;
    }
    /* 前面被占用后面空闲 */
    else if (prev_alloc && !next_alloc) {
        /* 先获取大小再移除 */
        size_t next_size = GET_SIZE(HDRP(next_bp));
        /* 移除后面的空闲块 */
        remove_free_block(next_bp);
        size += next_size;       

        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    /* 前面空闲后面被占用 */
    else if (!prev_alloc && next_alloc) {
        size_t prev_size = GET_SIZE(HDRP(PREV_BLKP(bp)));
        /* 移除前面的空闲节点 */
        remove_free_block(prev_bp);

        size += prev_size;
        /* 移动 bp */
        bp = prev_bp;
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    /* 前后均空闲 */
    else if (!prev_alloc && !next_alloc) {
        size_t next_size = GET_SIZE(HDRP(next_bp));
        size_t prev_size = GET_SIZE(HDRP(prev_bp));

        /* 移除两个空闲块 */
        remove_free_block(prev_bp);
        remove_free_block(next_bp);

        size += next_size + prev_size;
        /* 移动 bp */
        bp = prev_bp;
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }

    /* 将合并后的块插入链表 */
    insert_free_block(bp);
    return bp;
}

/* 从空闲链表中查找适合的块 */
static void *find_fit(size_t asize) {
    int index = get_class_index(asize);
    char *bp;

    /* 从当前大小类开始 */
    while (index < NUM_CLASSES) {
        bp = free_lists[index];

        /* 在当前大小类中遍历 */
        while (bp != NULL) {
            if (GET_SIZE(HDRP(bp)) >= asize) {
                return bp;
            }
            bp = NEXT_FREE_BLKP(bp);
        }

        index++;
    }

    return NULL;
}

static void place(void *bp, size_t asize) {
    size_t csize = GET_SIZE(HDRP(bp));

    /* 先从空闲链表中移除该块 */
    remove_free_block(bp);

    /* 最小块大小为 header+footer+prev+next=16*/
    /* 如果能分配出一个小块，就把他拆分 */
    if ((csize - asize) >= MIN_BLOCK_SIZE) {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        /* 更新被分割出的小块 */
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));

        /* 剩余块插入空闲链表 */
        insert_free_block(bp);
    }
    /* 如果剩余内存不够大 */
    else {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}

/*
 * Initialize: return -1 on error, 0 on success.
 */
int mm_init(void) {
    int i;

    /* 初始化所有空闲链表为空 */
    for (i = 0; i < NUM_CLASSES; i++) {
        free_lists[i] = NULL;
    }

    /* 先申请 4 个字的空间给 padding，序言块，尾块 */
    /* 此时 heap_listp 指向堆的最开头 */
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1) {
        return -1;
    }

    /* 第一个 padding 全为 0 */
    PUT(heap_listp, 0);
    /* 序言块的 header 和 footer */
    PUT(heap_listp + 1 * WSIZE, PACK(DSIZE, 1));
    PUT(heap_listp + 2 * WSIZE, PACK(DSIZE, 1));
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
    size_t asize; /* 需要实际分配的内存大小 */
    size_t extendsize;
    char *bp;

    /* 忽略无效请求 */
    if (size == 0) {
        return NULL;
    }

    /* 调整块大小 */
    /* 如果申请分配的内存比 2 个字小，就向上对齐到 4 个字，因为最少要 4 个字，头尾占 2 个字 */
    if (size <= DSIZE) {
        asize = MIN_BLOCK_SIZE;
    } else {
        asize = DSIZE * ((size + DSIZE + (DSIZE - 1)) / DSIZE);
        if (asize < MIN_BLOCK_SIZE) {
            asize = MIN_BLOCK_SIZE;
        }
    }
    /* 先搜索空闲链表 */
    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    /* 如果没找到合适的块，需要向系统申请更多的内存 */
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL) {
        return NULL;
    }

    /* 分配好后再重新放入分配好的块 */
    place(bp, asize);

    return bp;
}

/*
 * free
 */
void free (void *ptr) {
    if (ptr == NULL) {
        return;
    }
    
    /* 修改分配位 */
    size_t size = GET_SIZE(HDRP(ptr));
    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));

    /* 与相邻块进行合并 */
    coalesce(ptr);
}

/*
 * realloc - you may want to look at mm-naive.c
 */

/* 重新分配内存块大小 */
void *realloc(void *oldptr, size_t size) {
    void *newptr;
    size_t oldsize;

    /* 如果 oldptr 为 NULL，相当于 malloc */
    if (oldptr == NULL) {
        return malloc(size);
    }

    /* 如果 size 为 0，相当于 free */
    if (size == 0) {
        free(oldptr);
        return NULL;
    }

    /* 分配新块 */
    newptr = malloc(size);
    if (newptr == NULL) {
        return NULL;
    }

    /* 复制旧数据到新块 */
    oldsize = GET_SIZE(HDRP(oldptr)) - DSIZE;
    /* 如果要分配的内存小于旧块共有的内存，则只分配少的那部分 */
    if (size < oldsize) {
        oldsize = size;
    }
    memcpy(newptr, oldptr, oldsize);
    /* 释放旧块 */
    free(oldptr);

    return newptr;
}

/*
 * calloc - you may want to look at mm-naive.c
 * This function is not tested by mdriver, but it is
 * needed to run the traces.
 */
void *calloc (size_t nmemb, size_t size) {
    size_t bytes;
    void *ptr;

    /* 检查总大小，防止溢出 */
    bytes = nmemb * size;

    /* 检查溢出 */
    if (nmemb != 0 && bytes / nmemb != size) {
        return NULL;
    }

    ptr = malloc(bytes);
    if (ptr == NULL) {
        return NULL;
    }

    /* 清零 */
    memset(ptr, 0, bytes);

    return ptr;
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
