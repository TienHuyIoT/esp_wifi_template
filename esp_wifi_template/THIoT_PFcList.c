#include <stdlib.h>
#include "THIoT_PFcList.h"

/**
 * A list is an ordered set of entries.  We have the following primitives:
 * * list_create() - Create an empty list.  The return is the list pointer.
 * * list_remove() - Delete the list and optionally free all its entries.
 * * list_prev() - Return the prev item in the list.
 * * list_insert() - Add an item to the end of the list.
 * * list_insert_after() - Add an item to the list after a given entry.
 * * list_insert_before() - Add an item to the list before a given entry.
 * * list_next() - Get the next item in the list.
 * * list_remove() - Remove a specific item from the list.
 * * list_removeByValue() - Find the first element in the list with a matching value and remove it.
 * * list_findByValue() - Find the first element in the list with a matching value
 */

#if (defined LIST_MONITOR_HEAP_MEMORY) && (LIST_MONITOR_HEAP_MEMORY == 1)
static size_t list_heap = 0;
#endif

/**
 * Delete a list.
 */
void list_deleteList(list_t **pRootList, int withFree) {
    list_t *pList = *pRootList;
    list_t *pNext;
    while(pList != NULL) {
        pNext = pList->next;
        if (withFree) {
            free(pList->value);
        }
        free(pList);
#if (defined LIST_MONITOR_HEAP_MEMORY) && (LIST_MONITOR_HEAP_MEMORY == 1)
        list_heap -= sizeof(list_t);
#endif
        pList = pNext;
    }
    *pRootList = NULL;
} // list_deleteList

/**
 * Insert a new item at the end of the list.
 *[A] -> [endOLD]    ------>   [A] -> [endOLD] -> [X]
 *
 */
list_t *list_insert(list_t **pRootList, void *value) {
    list_t *pList = *pRootList;
    if (NULL == pList) {
        pList = malloc(sizeof(list_t));
        if(NULL == pList)
        {
            return NULL;
        }
#if (defined LIST_MONITOR_HEAP_MEMORY) && (LIST_MONITOR_HEAP_MEMORY == 1)
        list_heap += sizeof(list_t);
#endif
        pList->next = NULL;
        pList->prev = NULL;
        pList->value = value;
        *pRootList = pList;
        return pList;
    }

    // get last item
    while(pList->next != NULL) {
        pList = pList->next;
    }
    return list_insert_after(pList, value);
} // list_insert

/**
 * [pEntry] -> [B]    ------>   [pEntry] -> [X] -> [B]
 *
 */
list_t *list_insert_after(list_t *pEntry, void *value) {
    if (NULL == pEntry) {
        return NULL;
    }
    list_t *pNew = malloc(sizeof(list_t));
    if(NULL == pNew)
    {
        return NULL;
    }
#if (defined LIST_MONITOR_HEAP_MEMORY) && (LIST_MONITOR_HEAP_MEMORY == 1)
    list_heap += sizeof(list_t);
#endif
    pNew->next = pEntry->next;
    pNew->prev = pEntry;
    pNew->value = value;

    // Order IS important here.
    if (pEntry->next != NULL) {
        pEntry->next->prev = pNew;
    }
    pEntry->next = pNew;
    return pNew;
} // list_insert_after

/**
 * [A] -> [pEntry]   ------>   [A] -> [X] -> [pEntry]
 *
 */
list_t *list_insert_before(list_t *pEntry, void *value) {
    // Can't insert before the list itself.
    if (NULL == pEntry->prev) {
        return NULL;
    }
    list_t *pNew = malloc(sizeof(list_t));
    if(NULL == pNew)
    {
        return NULL;
    }
#if (defined LIST_MONITOR_HEAP_MEMORY) && (LIST_MONITOR_HEAP_MEMORY == 1)
    list_heap += sizeof(list_t);
#endif
    pNew->next = pEntry;
    pNew->prev = pEntry->prev;
    pNew->value = value;

    // Order IS important here.
    pEntry->prev->next = pNew;
    pEntry->prev = pNew;
    return pNew;
} // list_insert_before

/**
 * Remove an item from the list.
 */
uint8_t list_remove(list_t **pRootList, list_t *pEntry, int withFree) {
    list_t *pList = *pRootList;

    if (NULL == pList) {
        return LIST_ERROR;
    }

    if(pList == pEntry) {
        if (pList->next != NULL) {
            pList = pList->next;
            pList->prev = NULL;
        }
        else {
            pList = NULL;
        }
        *pRootList = pList;
    }
    else
    {
        while(pList != NULL && pList->next != pEntry) {
            pList = pList->next;
        }
        if (NULL == pList) {
            return LIST_ERROR;
        }
        pList->next = pEntry->next;
        if (pEntry->next != NULL) {
            pEntry->next->prev = pList;
        }
    }

    if (withFree) {
        free(pEntry->value);
    }
    free(pEntry);
#if (defined LIST_MONITOR_HEAP_MEMORY) && (LIST_MONITOR_HEAP_MEMORY == 1)
    list_heap -= sizeof(list_t);
#endif

    return LIST_OK;
} // list_delete

/**
 * Delete a list entry by value.
 */
uint8_t list_removeByValue(list_t **pRootList, void *value, int withFree) {
    list_t *pList;
    uint8_t status = LIST_ERROR;

    pList = list_findByValue(pRootList, value);
    if(pList != NULL)
    {
        status = list_remove(pRootList, pList, withFree);
    }

    return status;
} // list_deleteByValue

/**
 * find a list entry by value.
 */
list_t *list_findByValue(list_t **pRootList, void *value) {
    list_t *pList = *pRootList;
    list_t *pNext = pList;
    while(pNext != NULL) {
        if (pNext->value == value) {
            return pNext;
        }
        pNext = pNext->next;
    } // End while

    return NULL;
} // list_findByValue

/**
 * Get the next item in a list.
 */
list_t *list_next(list_t *pList) {
    if (NULL == pList) {
        return NULL;
    }
    return (pList->next);
} // list_next


list_t *list_prev(list_t *pList) {
    if (NULL == pList) {
        return NULL;
    }
    return pList->prev;
} // list_first

void *list_get_value(list_t *pList) {
    if (NULL == pList) {
        return NULL;
    }
    return pList->value;
}

#if (defined LIST_MONITOR_HEAP_MEMORY) && (LIST_MONITOR_HEAP_MEMORY == 1)
size_t list_heap_size(void) {
    return list_heap;
}
#endif
