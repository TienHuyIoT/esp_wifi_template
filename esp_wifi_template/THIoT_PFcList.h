/*
 * c_utils.h
 *
 *  Created on: Nov 15, 2016
 *      Author: kolban
 */

#ifndef __C_LIST_H
#define __C_LIST_H

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

typedef struct _c_list_t {
    void *value;
    struct _c_list_t *next;
    struct _c_list_t *prev;
} c_list_t;

void c_list_deleteList(c_list_t **pRootList, int withFree);
c_list_t *c_list_insert(c_list_t **pRootList, void *value);
uint8_t c_list_remove(c_list_t **pRootList, c_list_t *pEntry, int withFree);
uint8_t c_list_validate(c_list_t **pRootList, c_list_t *pEntry);
c_list_t *c_list_next(c_list_t *pList);
uint8_t c_list_removeByValue(c_list_t **pRootList, void *value, int withFree);
c_list_t *c_list_findByValue(c_list_t **pRootList, void *value);
c_list_t *c_list_prev(c_list_t *pList);
c_list_t *c_list_insert_after(c_list_t *pEntry, void *value);
c_list_t *c_list_insert_before(c_list_t *pEntry, void *value);
void *c_list_get_value(c_list_t *pList);
#if (defined LIST_MONITOR_HEAP_MEMORY) && (LIST_MONITOR_HEAP_MEMORY == 1)
size_t c_list_heap_size(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* __C_LIST_H_ */
