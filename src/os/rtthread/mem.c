/*
 * RT-Thread Small Memory Heap Allocator (src/os/rtthread/mem.c)
 * SPDX-License-Identifier: Apache-2.0
 */

#include <rthw.h>
#include <rtthread.h>

#ifdef RT_USING_SMALL_MEM

#define HEAP_MAGIC                      0x1ea0
#define MIN_SIZE                        12
#define MIN_SIZE_ALIGNED                RT_ALIGN_UP(MIN_SIZE, RT_ALIGN_SIZE)
#define SIZEOF_STRUCT_MEM               RT_ALIGN_UP(sizeof(struct heap_mem), RT_ALIGN_SIZE)

struct heap_mem
{
    rt_uint16_t magic;
    rt_uint16_t used;
    rt_size_t   next;
    rt_size_t   prev;
};

static rt_uint8_t *heap_ptr = RT_NULL;
static struct heap_mem *heap_end = RT_NULL;
static struct heap_mem *lfree = RT_NULL;
static struct rt_semaphore heap_sem;
static rt_size_t mem_size_aligned = 0;
static rt_size_t used_mem = 0;
static rt_size_t max_mem = 0;

static void plug_holes(struct heap_mem *mem)
{
    struct heap_mem *nmem;
    struct heap_mem *pmem;

    nmem = (struct heap_mem *)&heap_ptr[mem->next];
    if (mem != nmem && nmem->used == 0 && (rt_uint8_t *)nmem != (rt_uint8_t *)heap_end) {
        if (lfree == nmem) {
            lfree = mem;
        }
        mem->next = nmem->next;
        ((struct heap_mem *)&heap_ptr[nmem->next])->prev = (rt_uint8_t *)mem - heap_ptr;
    }

    pmem = (struct heap_mem *)&heap_ptr[mem->prev];
    if (pmem != mem && pmem->used == 0) {
        if (lfree == mem) {
            lfree = pmem;
        }
        pmem->next = mem->next;
        ((struct heap_mem *)&heap_ptr[mem->next])->prev = (rt_uint8_t *)pmem - heap_ptr;
    }
}

void rt_system_heap_init(void *begin_addr, void *end_addr)
{
    struct heap_mem *mem;
    rt_ubase_t begin_align = RT_ALIGN_UP((rt_ubase_t)begin_addr, RT_ALIGN_SIZE);
    rt_ubase_t end_align   = RT_ALIGN_DOWN((rt_ubase_t)end_addr, RT_ALIGN_SIZE);

    if ((end_align > (2 * SIZEOF_STRUCT_MEM)) &&
        ((end_align - 2 * SIZEOF_STRUCT_MEM) >= begin_align)) {
        mem_size_aligned = end_align - begin_align - 2 * SIZEOF_STRUCT_MEM;
    } else {
        return;
    }

    heap_ptr = (rt_uint8_t *)begin_align;

    mem        = (struct heap_mem *)heap_ptr;
    mem->magic = HEAP_MAGIC;
    mem->next  = mem_size_aligned + SIZEOF_STRUCT_MEM;
    mem->prev  = 0;
    mem->used  = 0;

    heap_end        = (struct heap_mem *)&heap_ptr[mem->next];
    heap_end->magic = HEAP_MAGIC;
    heap_end->used  = 1;
    heap_end->next  = mem_size_aligned + SIZEOF_STRUCT_MEM;
    heap_end->prev  = mem_size_aligned + SIZEOF_STRUCT_MEM;

    rt_sem_init(&heap_sem, "heap", 1, RT_IPC_FLAG_FIFO);

    lfree = (struct heap_mem *)heap_ptr;
}

void *rt_malloc(rt_size_t size)
{
    rt_size_t ptr, ptr2;
    struct heap_mem *mem, *mem2;

    if (size == 0 || heap_ptr == RT_NULL) {
        return RT_NULL;
    }

    size = RT_ALIGN_UP(size, RT_ALIGN_SIZE);
    if (size < MIN_SIZE_ALIGNED) {
        size = MIN_SIZE_ALIGNED;
    }
    if (size > mem_size_aligned) {
        return RT_NULL;
    }

    if (rt_thread_self() != RT_NULL && rt_interrupt_get_nest() == 0) {
        rt_sem_take(&heap_sem, RT_WAITING_FOREVER);
    }

    for (ptr = (rt_uint8_t *)lfree - heap_ptr;
         ptr < mem_size_aligned - size;
         ptr = ((struct heap_mem *)&heap_ptr[ptr])->next) {
        mem = (struct heap_mem *)&heap_ptr[ptr];

        if ((!mem->used) && (mem->next - (ptr + SIZEOF_STRUCT_MEM)) >= size) {
            if (mem->next - (ptr + SIZEOF_STRUCT_MEM) >=
                (size + SIZEOF_STRUCT_MEM + MIN_SIZE_ALIGNED)) {
                ptr2 = ptr + SIZEOF_STRUCT_MEM + size;
                mem2       = (struct heap_mem *)&heap_ptr[ptr2];
                mem2->magic = HEAP_MAGIC;
                mem2->used = 0;
                mem2->next = mem->next;
                mem2->prev = ptr;

                mem->next = ptr2;
                mem->used = 1;

                if (mem2->next != mem_size_aligned + SIZEOF_STRUCT_MEM) {
                    ((struct heap_mem *)&heap_ptr[mem2->next])->prev = ptr2;
                }
                used_mem += (size + SIZEOF_STRUCT_MEM);
                if (max_mem < used_mem) {
                    max_mem = used_mem;
                }
            } else {
                mem->used = 1;
                used_mem += mem->next - ((rt_uint8_t *)mem - heap_ptr);
                if (max_mem < used_mem) {
                    max_mem = used_mem;
                }
            }

            mem->magic = HEAP_MAGIC;
            if (mem == lfree) {
                while (lfree->used && lfree != heap_end) {
                    lfree = (struct heap_mem *)&heap_ptr[lfree->next];
                }
            }

            if (rt_thread_self() != RT_NULL && rt_interrupt_get_nest() == 0) {
                rt_sem_release(&heap_sem);
            }

            return (rt_uint8_t *)mem + SIZEOF_STRUCT_MEM;
        }
    }

    if (rt_thread_self() != RT_NULL && rt_interrupt_get_nest() == 0) {
        rt_sem_release(&heap_sem);
    }

    return RT_NULL;
}

void rt_free(void *rmem)
{
    struct heap_mem *mem;

    if (rmem == RT_NULL || heap_ptr == RT_NULL) {
        return;
    }

    if ((rt_uint8_t *)rmem < (rt_uint8_t *)heap_ptr ||
        (rt_uint8_t *)rmem >= (rt_uint8_t *)heap_end) {
        return;
    }

    mem = (struct heap_mem *)((rt_uint8_t *)rmem - SIZEOF_STRUCT_MEM);

    if (rt_thread_self() != RT_NULL && rt_interrupt_get_nest() == 0) {
        rt_sem_take(&heap_sem, RT_WAITING_FOREVER);
    }

    mem->used  = 0;
    mem->magic = HEAP_MAGIC;

    if (mem < lfree) {
        lfree = mem;
    }

    used_mem -= (mem->next - ((rt_uint8_t *)mem - heap_ptr));
    plug_holes(mem);

    if (rt_thread_self() != RT_NULL && rt_interrupt_get_nest() == 0) {
        rt_sem_release(&heap_sem);
    }
}

void *rt_realloc(void *rmem, rt_size_t newsize)
{
    void *nmem;
    struct heap_mem *mem;
    rt_size_t oldsize;

    if (rmem == RT_NULL) {
        return rt_malloc(newsize);
    }
    if (newsize == 0) {
        rt_free(rmem);
        return RT_NULL;
    }

    mem = (struct heap_mem *)((rt_uint8_t *)rmem - SIZEOF_STRUCT_MEM);
    oldsize = mem->next - ((rt_uint8_t *)mem - heap_ptr) - SIZEOF_STRUCT_MEM;

    nmem = rt_malloc(newsize);
    if (nmem != RT_NULL) {
        rt_memcpy(nmem, rmem, oldsize < newsize ? oldsize : newsize);
        rt_free(rmem);
    }
    return nmem;
}

void *rt_calloc(rt_size_t count, rt_size_t size)
{
    void *p;
    p = rt_malloc(count * size);
    if (p != RT_NULL) {
        rt_memset(p, 0, count * size);
    }
    return p;
}

void rt_memory_info(rt_uint32_t *total,
                    rt_uint32_t *used,
                    rt_uint32_t *max_used)
{
    if (total != RT_NULL) {
        *total = (rt_uint32_t)mem_size_aligned;
    }
    if (used != RT_NULL) {
        *used = (rt_uint32_t)used_mem;
    }
    if (max_used != RT_NULL) {
        *max_used = (rt_uint32_t)max_mem;
    }
}

#endif /* RT_USING_SMALL_MEM */

