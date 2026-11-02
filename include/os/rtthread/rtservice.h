/*
 * RT-Thread Doubly-Linked Intrusive List Service (include/rtthread/rtservice.h)
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __RT_SERVICE_H__
#define __RT_SERVICE_H__

#ifdef __cplusplus
extern "C" {
#endif

#define rt_container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - (unsigned long)(&((type *)0)->member)))

#define RT_LIST_OBJECT_INIT(object) { &(object), &(object) }

rt_inline void rt_list_init(rt_list_t *l)
{
    l->next = l->prev = l;
}

rt_inline void rt_list_insert_after(rt_list_t *l, rt_list_t *n)
{
    l->next->prev = n;
    n->next = l->next;
    l->next = n;
    n->prev = l;
}

rt_inline void rt_list_insert_before(rt_list_t *l, rt_list_t *n)
{
    l->prev->next = n;
    n->prev = l->prev;
    l->prev = n;
    n->next = l;
}

rt_inline void rt_list_remove(rt_list_t *n)
{
    n->next->prev = n->prev;
    n->prev->next = n->next;
    n->next = n->prev = n;
}

rt_inline int rt_list_isempty(const rt_list_t *l)
{
    return l->next == l;
}

rt_inline unsigned int rt_list_len(const rt_list_t *l)
{
    unsigned int len = 0;
    const rt_list_t *p = l;
    while (p->next != l) {
        p = p->next;
        len++;
    }
    return len;
}

#define rt_list_entry(node, type, member) \
    rt_container_of(node, type, member)

#define rt_list_for_each(pos, head) \
    for (pos = (head)->next; pos != (head); pos = pos->next)

#define rt_list_for_each_safe(pos, n, head) \
    for (pos = (head)->next, n = pos->next; pos != (head); \
        pos = n, n = pos->next)

#define rt_list_first_entry(ptr, type, member) \
    rt_list_entry((ptr)->next, type, member)

#ifdef __cplusplus
}
#endif

#endif /* __RT_SERVICE_H__ */

