/*
 * c_utils.h
 *
 *  Created on: Nov 15, 2016
 *      Author: kolban
 */

#ifndef __C_LIST_H_
#define __C_LIST_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define LIST_MONITOR_HEAP_MEMORY 0

typedef enum {
    LIST_OK = 0,
    LIST_ERROR
} list_status_t;

typedef enum {
    LIST_STATIC_VALUE = 0,
    LIST_DYNAMIC_VALUE
} list_value_t;

typedef struct _list_t {
    void *value;
    struct _list_t *next;
    struct _list_t *prev;
} list_t;

void list_deleteList(list_t **pRootList, int withFree);
list_t *list_insert(list_t **pRootList, void *value);
uint8_t list_remove(list_t **pRootList, list_t *pEntry, int withFree);
list_t *list_next(list_t *pList);
uint8_t list_removeByValue(list_t **pRootList, void *value, int withFree);
list_t *list_findByValue(list_t **pRootList, void *value);
list_t *list_prev(list_t *pList);
list_t *list_insert_after(list_t *pEntry, void *value);
list_t *list_insert_before(list_t *pEntry, void *value);
void *list_get_value(list_t *pList);
#if (defined LIST_MONITOR_HEAP_MEMORY) && (LIST_MONITOR_HEAP_MEMORY == 1)
size_t list_heap_size(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* __C_LIST_H_ */
