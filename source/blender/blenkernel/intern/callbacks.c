/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup bke
 */

#include "BLI_listbase.h"
#include "BLI_utildefines.h"

#include "BKE_callbacks.h"

#include "MEM_guardedalloc.h"

#include "RNA_access.h"
#include "RNA_types.h"

static ListBase callback_slots[BKE_CB_EVT_TOT] = {{NULL}};

void BKE_callback_exec(struct Main *bmain,
                       struct PointerRNA **pointers,
                       const int num_pointers,
                       eCbEvent evt)
{
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

void BKE_callback_add(bCallbackFuncStore *funcstore, eCbEvent evt)
{
  ListBase *lb = &callback_slots[evt];
  BLI_addtail(lb, funcstore);
}

void BKE_callback_remove(bCallbackFuncStore *funcstore, eCbEvent evt)
{
  ListBase *lb = &callback_slots[evt];

  /* Be safe, as the callback may have already been removed by BKE_callback_global_finalize(), for
   * example when removing callbacks in response to a BKE_blender_atexit_register callback
   * function. `BKE_blender_atexit()` runs after `BKE_callback_global_finalize()`. */
  BLI_remlink_safe(lb, funcstore);

  if (funcstore->alloc) {
    MEM_freeN(funcstore);
  }
}

void BKE_callback_global_init(void)
{
  /* do nothing */
}

/* call on application exit */
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
}
