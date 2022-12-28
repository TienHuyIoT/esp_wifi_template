#include <stdlib.h>
#include "THIoT_PFcList.h"

/**
 * A list is an ordered set of entries.  We have the following primitives:
 * * list_create() - Create an empty list.  The return is the list pointer.
 * * c_list_remove() - Delete the list and optionally free all its entries.
 * * c_list_prev() - Return the prev item in the list.
 * * c_list_insert() - Add an item to the end of the list.
 * * c_list_insert_after() - Add an item to the list after a given entry.
 * * c_list_insert_before() - Add an item to the list before a given entry.
 * * c_list_next() - Get the next item in the list.
 * * c_list_remove() - Remove a specific item from the list.
 * * c_list_removeByValue() - Find the first element in the list with a matching value and remove it.
 * * c_list_findByValue() - Find the first element in the list with a matching value
 */

#if (defined LIST_MONITOR_HEAP_MEMORY) && (LIST_MONITOR_HEAP_MEMORY == 1)
static size_t list_heap = 0;
#endif

/**
 * Delete a list.
 */
void c_list_deleteList(c_list_t **pRootList, int withFree) {
    c_list_t *pList = *pRootList;
    c_list_t *pNext;
    while(pList != NULL) {
        pNext = pList->next;
        if (withFree) {
            free(pList->value);
        }
        free(pList);
#if (defined LIST_MONITOR_HEAP_MEMORY) && (LIST_MONITOR_HEAP_MEMORY == 1)
        list_heap -= sizeof(c_list_t);
#endif
        pList = pNext;
    }
    *pRootList = NULL;
} // c_list_deleteList

/**
 * Insert a new item at the end of the list.
 *[A] -> [endOLD]    ------>   [A] -> [endOLD] -> [X]
 *
 */
c_list_t *c_list_insert(c_list_t **pRootList, void *value) {
    c_list_t *pList = *pRootList;
    if (NULL == pList) {
        pList = malloc(sizeof(c_list_t));
        if(NULL == pList)
        {
            return NULL;
        }
#if (defined LIST_MONITOR_HEAP_MEMORY) && (LIST_MONITOR_HEAP_MEMORY == 1)
        list_heap += sizeof(c_list_t);
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
    return c_list_insert_after(pList, value);
} // c_list_insert

/**
 * [pEntry] -> [B]    ------>   [pEntry] -> [X] -> [B]
 *
 */
c_list_t *c_list_insert_after(c_list_t *pEntry, void *value) {
    if (NULL == pEntry) {
        return NULL;
    }
    c_list_t *pNew = malloc(sizeof(c_list_t));
    if(NULL == pNew)
    {
        return NULL;
    }
#if (defined LIST_MONITOR_HEAP_MEMORY) && (LIST_MONITOR_HEAP_MEMORY == 1)
    list_heap += sizeof(c_list_t);
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
} // c_list_insert_after

/**
 * [A] -> [pEntry]   ------>   [A] -> [X] -> [pEntry]
 *
 */
c_list_t *c_list_insert_before(c_list_t *pEntry, void *value) {
    // Can't insert before the list itself.
    if (NULL == pEntry->prev) {
        return NULL;
    }
    c_list_t *pNew = malloc(sizeof(c_list_t));
    if(NULL == pNew)
    {
        return NULL;
    }
#if (defined LIST_MONITOR_HEAP_MEMORY) && (LIST_MONITOR_HEAP_MEMORY == 1)
    list_heap += sizeof(c_list_t);
#endif
    pNew->next = pEntry;
    pNew->prev = pEntry->prev;
    pNew->value = value;

    // Order IS important here.
    pEntry->prev->next = pNew;
    pEntry->prev = pNew;
    return pNew;
} // c_list_insert_before

/**
 * Remove an item from the list.
 */
uint8_t c_list_remove(c_list_t **pRootList, c_list_t *pEntry, int withFree) {
    c_list_t *pList = *pRootList;

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
    list_heap -= sizeof(c_list_t);
#endif

    return LIST_OK;
} // list_delete

/**
 * Delete a list entry by value.
 */
uint8_t c_list_removeByValue(c_list_t **pRootList, void *value, int withFree) {
    c_list_t *pList;
    uint8_t status = LIST_ERROR;

    pList = c_list_findByValue(pRootList, value);
    if(pList != NULL)
    {
        status = c_list_remove(pRootList, pList, withFree);
    }

    return status;
} // list_deleteByValue

/**
 * find a list entry by value.
 */
c_list_t *c_list_findByValue(c_list_t **pRootList, void *value) {
    c_list_t *pList = *pRootList;
    c_list_t *pNext = pList;
    while(pNext != NULL) {
        if (pNext->value == value) {
            return pNext;
        }
        pNext = pNext->next;
    } // End while

    return NULL;
} // c_list_findByValue

/**
 * Get the next item in a list.
 */
c_list_t *c_list_next(c_list_t *pList) {
    if (NULL == pList) {
        return NULL;
    }
    return (pList->next);
} // c_list_next


c_list_t *c_list_prev(c_list_t *pList) {
    if (NULL == pList) {
        return NULL;
    }
    return pList->prev;
} // list_first

void *c_list_get_value(c_list_t *pList) {
    if (NULL == pList) {
        return NULL;
    }
    return pList->value;
}

#if (defined LIST_MONITOR_HEAP_MEMORY) && (LIST_MONITOR_HEAP_MEMORY == 1)
size_t c_list_heap_size(void) {
    return list_heap;
}
#endif
