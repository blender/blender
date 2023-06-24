/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "BLI_listbase.h"
#include "BLI_utildefines.h"

#include "BKE_callbacks.h"

#include "MEM_guardedalloc.h"

#include "RNA_access.h"
#include "RNA_prototypes.h"
#include "RNA_types.h"

static ListBase callback_slots[BKE_CB_EVT_TOT] = {{NULL}};

static bool callbacks_initialized = false;

#define ASSERT_CALLBACKS_INITIALIZED() \
  BLI_assert_msg(callbacks_initialized, \
                 "Callbacks should be initialized with BKE_callback_global_init() before using " \
                 "the callback system.")

void BKE_callback_exec(struct Main *bmain,
                       PointerRNA **pointers,
                       const int num_pointers,
                       eCbEvent evt)
{
  ASSERT_CALLBACKS_INITIALIZED();

  /* Use mutable iteration so handlers are able to remove themselves. */
  ListBase *lb = &callback_slots[evt];
  LISTBASE_FOREACH_MUTABLE (bCallbackFuncStore *, funcstore, lb) {
    funcstore->func(bmain, pointers, num_pointers, funcstore->arg);
  }
}

void BKE_callback_exec_null(struct Main *bmain, eCbEvent evt)
{
  BKE_callback_exec(bmain, NULL, 0, evt);
}

void BKE_callback_exec_id(struct Main *bmain, struct ID *id, eCbEvent evt)
{
  PointerRNA id_ptr;
  RNA_id_pointer_create(id, &id_ptr);

  PointerRNA *pointers[1] = {&id_ptr};

  BKE_callback_exec(bmain, pointers, 1, evt);
}

void BKE_callback_exec_id_depsgraph(struct Main *bmain,
                                    struct ID *id,
                                    struct Depsgraph *depsgraph,
                                    eCbEvent evt)
{
  PointerRNA id_ptr;
  RNA_id_pointer_create(id, &id_ptr);

  PointerRNA depsgraph_ptr;
  RNA_pointer_create(NULL, &RNA_Depsgraph, depsgraph, &depsgraph_ptr);

  PointerRNA *pointers[2] = {&id_ptr, &depsgraph_ptr};

  BKE_callback_exec(bmain, pointers, 2, evt);
}

void BKE_callback_exec_string(struct Main *bmain, eCbEvent evt, const char *str)
{
  PointerRNA str_ptr;
  PrimitiveStringRNA data = {NULL};
  data.value = str;
  RNA_pointer_create(NULL, &RNA_PrimitiveString, &data, &str_ptr);

  PointerRNA *pointers[1] = {&str_ptr};

  BKE_callback_exec(bmain, pointers, 1, evt);
}

void BKE_callback_add(bCallbackFuncStore *funcstore, eCbEvent evt)
{
  ASSERT_CALLBACKS_INITIALIZED();
  ListBase *lb = &callback_slots[evt];
  BLI_addtail(lb, funcstore);
}

void BKE_callback_remove(bCallbackFuncStore *funcstore, eCbEvent evt)
{
  /* The callback may have already been removed by BKE_callback_global_finalize(), for
   * example when removing callbacks in response to a BKE_blender_atexit_register callback
   * function. `BKE_blender_atexit()` runs after `BKE_callback_global_finalize()`. */
  if (!callbacks_initialized) {
    return;
  }

  ListBase *lb = &callback_slots[evt];

  /* Be noisy about potential programming errors. */
  BLI_assert_msg(BLI_findindex(lb, funcstore) != -1, "To-be-removed callback not found");

  BLI_remlink(lb, funcstore);

  if (funcstore->alloc) {
    MEM_freeN(funcstore);
  }
}

void BKE_callback_global_init(void)
{
  callbacks_initialized = true;
}

void BKE_callback_global_finalize(void)
{
  eCbEvent evt;
  for (evt = 0; evt < BKE_CB_EVT_TOT; evt++) {
    ListBase *lb = &callback_slots[evt];
    bCallbackFuncStore *funcstore;
    bCallbackFuncStore *funcstore_next;
    for (funcstore = lb->first; funcstore; funcstore = funcstore_next) {
      funcstore_next = funcstore->next;
      BKE_callback_remove(funcstore, evt);
    }
  }

  callbacks_initialized = false;
}
