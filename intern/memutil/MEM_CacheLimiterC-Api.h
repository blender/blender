/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup intern_memutil
 */

#ifndef __MEM_CACHELIMITERC_API_H__
#define __MEM_CACHELIMITERC_API_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "BLI_utildefines.h"

struct MEM_CacheLimiter_s;
struct MEM_CacheLimiterHandle_s;

typedef struct MEM_CacheLimiter_s MEM_CacheLimiterC;
typedef struct MEM_CacheLimiterHandle_s MEM_CacheLimiterHandleC;

/* function used to remove data from memory */
typedef void (*MEM_CacheLimiter_Destruct_Func)(void *);

/* function used to measure stored data element size */
typedef size_t (*MEM_CacheLimiter_DataSize_Func)(void *);

/* function used to measure priority of item when freeing memory */
typedef int (*MEM_CacheLimiter_ItemPriority_Func)(void *, int);

/* function to check whether item could be destroyed */
typedef bool (*MEM_CacheLimiter_ItemDestroyable_Func)(void *);

#ifndef __MEM_CACHELIMITER_H__
void MEM_CacheLimiter_set_maximum(size_t m);
size_t MEM_CacheLimiter_get_maximum(void);
void MEM_CacheLimiter_set_disabled(bool disabled);
bool MEM_CacheLimiter_is_disabled(void);
#endif /* __MEM_CACHELIMITER_H__ */

/**
 * Create new MEM_CacheLimiter object
 * managed objects are destructed with the data_destructor
 *
 * \param data_destructor: TODO.
 * \return A new #MEM_CacheLimter object.
 */

MEM_CacheLimiterC *new_MEM_CacheLimiter(MEM_CacheLimiter_Destruct_Func data_destructor,
                                        MEM_CacheLimiter_DataSize_Func data_size);

/**
 * Delete MEM_CacheLimiter
 *
 * Frees the memory of the CacheLimiter but does not touch managed objects!
 *
 * \param This: "This" pointer.
 */

void delete_MEM_CacheLimiter(MEM_CacheLimiterC *This);

/**
 * Manage object
 *
 * \param This: "This" pointer, data object to manage.
 * \return The handle to reference/unreference & touch the managed object.
 */

MEM_CacheLimiterHandleC *MEM_CacheLimiter_insert(MEM_CacheLimiterC *This, void *data);

/**
 * Free objects until memory constraints are satisfied
 *
 * \param This: "This" pointer.
 */

void MEM_CacheLimiter_enforce_limits(MEM_CacheLimiterC *This);

/**
 * Unmanage object previously inserted object.
 * Does _not_ delete managed object!
 *
 * \param handle: of object.
 */

void MEM_CacheLimiter_unmanage(MEM_CacheLimiterHandleC *handle);

/**
 * Raise priority of object (put it at the tail of the deletion chain)
 *
 * \param handle: of object.
 */

void MEM_CacheLimiter_touch(MEM_CacheLimiterHandleC *handle);

/**
 * Increment reference counter. Objects with reference counter != 0 are _not_
 * deleted.
 *
 * \param handle: of object.
 */

void MEM_CacheLimiter_ref(MEM_CacheLimiterHandleC *handle);

/**
 * Decrement reference counter. Objects with reference counter != 0 are _not_
 * deleted.
 *
 * \param handle: of object.
 */

void MEM_CacheLimiter_unref(MEM_CacheLimiterHandleC *handle);

/**
 * Get reference counter.
 *
 * \param handle: of object.
 */

int MEM_CacheLimiter_get_refcount(MEM_CacheLimiterHandleC *handle);

/**
 * Get pointer to managed object
 *
 * \param handle: of object.
 */

void *MEM_CacheLimiter_get(MEM_CacheLimiterHandleC *handle);

void MEM_CacheLimiter_ItemPriority_Func_set(MEM_CacheLimiterC *This,
                                            MEM_CacheLimiter_ItemPriority_Func item_priority_func);

void MEM_CacheLimiter_ItemDestroyable_Func_set(
    MEM_CacheLimiterC *This, MEM_CacheLimiter_ItemDestroyable_Func item_destroyable_func);

size_t MEM_CacheLimiter_get_memory_in_use(MEM_CacheLimiterC *This);

#ifdef __cplusplus
}
#endif

#endif  // __MEM_CACHELIMITERC_API_H__
