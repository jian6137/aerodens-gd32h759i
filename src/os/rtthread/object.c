/*
 * RT-Thread Kernel Object Container (src/os/rtthread/object.c)
 * SPDX-License-Identifier: Apache-2.0
 */

#include <rtthread.h>
#include <rthw.h>

enum rt_object_info_type
{
    RT_Object_Info_Thread = 0,
#ifdef RT_USING_SEMAPHORE
    RT_Object_Info_Semaphore,
#endif
#ifdef RT_USING_MUTEX
    RT_Object_Info_Mutex,
#endif
#ifdef RT_USING_MAILBOX
    RT_Object_Info_MailBox,
#endif
    RT_Object_Info_Timer,
    RT_Object_Info_Unknown,
};

#define _OBJ_CONTAINER_LIST_INIT(c)     \
    { &(rt_object_container[c].object_list), &(rt_object_container[c].object_list) }

static struct rt_object_information rt_object_container[RT_Object_Info_Unknown] =
{
    /* Thread */
    {
        RT_Object_Class_Thread,
        _OBJ_CONTAINER_LIST_INIT(RT_Object_Info_Thread),
        sizeof(struct rt_thread)
    },
#ifdef RT_USING_SEMAPHORE
    /* Semaphore */
    {
        RT_Object_Class_Semaphore,
        _OBJ_CONTAINER_LIST_INIT(RT_Object_Info_Semaphore),
        sizeof(struct rt_semaphore)
    },
#endif
#ifdef RT_USING_MUTEX
    /* Mutex */
    {
        RT_Object_Class_Mutex,
        _OBJ_CONTAINER_LIST_INIT(RT_Object_Info_Mutex),
        sizeof(struct rt_mutex)
    },
#endif
#ifdef RT_USING_MAILBOX
    /* MailBox */
    {
        RT_Object_Class_MailBox,
        _OBJ_CONTAINER_LIST_INIT(RT_Object_Info_MailBox),
        sizeof(struct rt_mailbox)
    },
#endif
    /* Timer */
    {
        RT_Object_Class_Timer,
        _OBJ_CONTAINER_LIST_INIT(RT_Object_Info_Timer),
        sizeof(struct rt_timer)
    },
};

struct rt_object_information *
rt_object_get_information(enum rt_object_class_type type)
{
    int index;

    for (index = 0; index < RT_Object_Info_Unknown; index++) {
        if (rt_object_container[index].type == type) {
            return &rt_object_container[index];
        }
    }

    return RT_NULL;
}

void rt_object_init(struct rt_object         *object,
                    enum rt_object_class_type type,
                    const char               *name)
{
    register rt_base_t temp;
    struct rt_object_information *information;

    information = rt_object_get_information(type);
    RT_ASSERT(information != RT_NULL);

    object->type = type | RT_Object_Class_Static;
    object->flag = 0;
    rt_strncpy(object->name, name, RT_NAME_MAX);

    temp = rt_hw_interrupt_disable();
    rt_list_insert_after(&(information->object_list), &(object->list));
    rt_hw_interrupt_enable(temp);
}

void rt_object_detach(rt_object_t object)
{
    register rt_base_t temp;

    RT_ASSERT(object != RT_NULL);

    object->type = 0;
    temp = rt_hw_interrupt_disable();
    rt_list_remove(&(object->list));
    rt_hw_interrupt_enable(temp);
}

#ifdef RT_USING_HEAP
rt_object_t rt_object_allocate(enum rt_object_class_type type, const char *name)
{
    struct rt_object *object;
    register rt_base_t temp;
    struct rt_object_information *information;

    information = rt_object_get_information(type);
    RT_ASSERT(information != RT_NULL);

    object = (struct rt_object *)rt_malloc(information->object_size);
    if (object == RT_NULL) {
        return RT_NULL;
    }

    rt_memset(object, 0x0, information->object_size);
    object->type = type;
    object->flag = 0;
    rt_strncpy(object->name, name, RT_NAME_MAX);

    temp = rt_hw_interrupt_disable();
    rt_list_insert_after(&(information->object_list), &(object->list));
    rt_hw_interrupt_enable(temp);

    return object;
}

void rt_object_delete(rt_object_t object)
{
    register rt_base_t temp;

    RT_ASSERT(object != RT_NULL);
    RT_ASSERT(!(object->type & RT_Object_Class_Static));

    object->type = RT_Object_Class_Null;

    temp = rt_hw_interrupt_disable();
    rt_list_remove(&(object->list));
    rt_hw_interrupt_enable(temp);

    rt_free(object);
}
#endif

rt_bool_t rt_object_is_systemobject(rt_object_t object)
{
    RT_ASSERT(object != RT_NULL);
    if (object->type & RT_Object_Class_Static) {
        return RT_TRUE;
    }
    return RT_FALSE;
}

rt_uint8_t rt_object_get_type(rt_object_t object)
{
    RT_ASSERT(object != RT_NULL);
    return object->type & ~RT_Object_Class_Static;
}

