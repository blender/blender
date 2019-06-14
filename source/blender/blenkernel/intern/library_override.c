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
 *
 * The Original Code is Copyright (C) 2016 by Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup bke
 */

#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_ID.h"
#include "DNA_object_types.h"

#include "DEG_depsgraph.h"
#include "BKE_library.h"
#include "BKE_library_override.h"
#include "BKE_library_remap.h"
#include "BKE_main.h"

#include "BLI_utildefines.h"
#include "BLI_listbase.h"
#include "BLI_string.h"

#include "RNA_access.h"
#include "RNA_types.h"

#include "PIL_time.h"
#include "PIL_time_utildefines.h"

#define OVERRIDE_AUTO_CHECK_DELAY 0.2 /* 200ms between auto-override checks. */

static void bke_override_property_copy(IDOverrideLibraryProperty *op_dst,
                                       IDOverrideLibraryProperty *op_src);
static void bke_override_property_operation_copy(IDOverrideLibraryPropertyOperation *opop_dst,
                                                 IDOverrideLibraryPropertyOperation *opop_src);

static void bke_override_property_clear(IDOverrideLibraryProperty *op);
static void bke_override_property_operation_clear(IDOverrideLibraryPropertyOperation *opop);

/* Temp, for until library override is ready and tested enough to go 'public',
 * we hide it by default in UI and such. */
static bool _override_library_enabled = false;

void BKE_override_library_enable(const bool do_enable)
{
  _override_library_enabled = do_enable;
}

bool BKE_override_library_is_enabled()
{
  return _override_library_enabled;
}

/** Initialize empty overriding of \a reference_id by \a local_id. */
IDOverrideLibrary *BKE_override_library_init(ID *local_id, ID *reference_id)
{
  /* If reference_id is NULL, we are creating an override template for purely local data.
   * Else, reference *must* be linked data. */
  BLI_assert(reference_id == NULL || reference_id->lib != NULL);
  BLI_assert(local_id->override_library == NULL);

  ID *ancestor_id;
  for (ancestor_id = reference_id; ancestor_id != NULL && ancestor_id->override_library != NULL &&
                                   ancestor_id->override_library->reference != NULL;
       ancestor_id = ancestor_id->override_library->reference) {
    /* pass */
  }

  if (ancestor_id != NULL && ancestor_id->override_library != NULL) {
    /* Original ID has a template, use it! */
    BKE_override_library_copy(local_id, ancestor_id);
    if (local_id->override_library->reference != reference_id) {
      id_us_min(local_id->override_library->reference);
      local_id->override_library->reference = reference_id;
      id_us_plus(local_id->override_library->reference);
    }
    return local_id->override_library;
  }

  /* Else, generate new empty override. */
  local_id->override_library = MEM_callocN(sizeof(*local_id->override_library), __func__);
  local_id->override_library->reference = reference_id;
  id_us_plus(local_id->override_library->reference);
  local_id->tag &= ~LIB_TAG_OVERRIDE_LIBRARY_REFOK;
  /* TODO do we want to add tag or flag to referee to mark it as such? */
  return local_id->override_library;
}

/** Deep copy of a whole override from \a src_id to \a dst_id. */
void BKE_override_library_copy(ID *dst_id, const ID *src_id)
{
  BLI_assert(src_id->override_library != NULL);

  if (dst_id->override_library != NULL) {
    if (src_id->override_library == NULL) {
      BKE_override_library_free(&dst_id->override_library);
      return;
    }
    else {
      BKE_override_library_clear(dst_id->override_library);
    }
  }
  else if (src_id->override_library == NULL) {
    return;
  }
  else {
    BKE_override_library_init(dst_id, NULL);
  }

  /* Source is already overriding data, we copy it but reuse its reference for dest ID.
   * otherwise, source is only an override template, it then becomes reference of dest ID. */
  dst_id->override_library->reference = src_id->override_library->reference ?
                                            src_id->override_library->reference :
                                            (ID *)src_id;
  id_us_plus(dst_id->override_library->reference);

  BLI_duplicatelist(&dst_id->override_library->properties, &src_id->override_library->properties);
  for (IDOverrideLibraryProperty *op_dst = dst_id->override_library->properties.first,
                                 *op_src = src_id->override_library->properties.first;
       op_dst;
       op_dst = op_dst->next, op_src = op_src->next) {
    bke_override_property_copy(op_dst, op_src);
  }

  dst_id->tag &= ~LIB_TAG_OVERRIDE_LIBRARY_REFOK;
}

/** Clear any overriding data from given \a override. */
void BKE_override_library_clear(IDOverrideLibrary *override)
{
  BLI_assert(override != NULL);

  for (IDOverrideLibraryProperty *op = override->properties.first; op; op = op->next) {
    bke_override_property_clear(op);
  }
  BLI_freelistN(&override->properties);

  id_us_min(override->reference);
  /* override->storage should never be refcounted... */
}

/** Free given \a override. */
void BKE_override_library_free(struct IDOverrideLibrary **override)
{
  BLI_assert(*override != NULL);

  BKE_override_library_clear(*override);
  MEM_freeN(*override);
  *override = NULL;
}

static ID *override_library_create_from(Main *bmain, ID *reference_id)
{
  ID *local_id;

  if (!BKE_id_copy(bmain, reference_id, (ID **)&local_id)) {
    return NULL;
  }
  id_us_min(local_id);

  BKE_override_library_init(local_id, reference_id);
  local_id->override_library->flag |= OVERRIDE_LIBRARY_AUTO;

  return local_id;
}

/** Create an overridden local copy of linked reference. */
ID *BKE_override_library_create_from_id(Main *bmain, ID *reference_id)
{
  BLI_assert(reference_id != NULL);
  BLI_assert(reference_id->lib != NULL);

  ID *local_id = override_library_create_from(bmain, reference_id);

  /* Remapping, we obviously only want to affect local data
   * (and not our own reference pointer to overridden ID). */
  BKE_libblock_remap(bmain,
                     reference_id,
                     local_id,
                     ID_REMAP_SKIP_INDIRECT_USAGE | ID_REMAP_SKIP_OVERRIDE_LIBRARY);

  return local_id;
}

/** Create overridden local copies of all tagged data-blocks in given Main.
 *
 * \note Set id->newid of overridden libs with newly created overrides,
 * caller is responsible to clean those pointers before/after usage as needed.
 *
 * \return \a true on success, \a false otherwise.
 */
bool BKE_override_library_create_from_tag(Main *bmain)
{
  ID *reference_id;
  bool ret = true;

  FOREACH_MAIN_ID_BEGIN (bmain, reference_id) {
    if ((reference_id->tag & LIB_TAG_DOIT) != 0 && reference_id->lib != NULL) {
      if ((reference_id->newid = override_library_create_from(bmain, reference_id)) == NULL) {
        ret = false;
      }
    }
  }
  FOREACH_MAIN_ID_END;

  FOREACH_MAIN_ID_BEGIN (bmain, reference_id) {
    if ((reference_id->tag & LIB_TAG_DOIT) != 0 && reference_id->lib != NULL &&
        reference_id->newid != NULL) {
      ID *local_id = reference_id->newid;
      BKE_libblock_remap(bmain,
                         reference_id,
                         local_id,
                         ID_REMAP_SKIP_INDIRECT_USAGE | ID_REMAP_SKIP_OVERRIDE_LIBRARY);
    }
  }
  FOREACH_MAIN_ID_END;

  return ret;
}

/**
 * Find override property from given RNA path, if it exists.
 */
IDOverrideLibraryProperty *BKE_override_library_property_find(IDOverrideLibrary *override,
                                                              const char *rna_path)
{
  /* XXX TODO we'll most likely want a runtime ghash to store that mapping at some point. */
  return BLI_findstring_ptr(
      &override->properties, rna_path, offsetof(IDOverrideLibraryProperty, rna_path));
}

/**
 * Find override property from given RNA path, or create it if it does not exist.
 */
IDOverrideLibraryProperty *BKE_override_library_property_get(IDOverrideLibrary *override,
                                                             const char *rna_path,
                                                             bool *r_created)
{
  /* XXX TODO we'll most likely want a runtime ghash to store that mapping at some point. */
  IDOverrideLibraryProperty *op = BKE_override_library_property_find(override, rna_path);

  if (op == NULL) {
    op = MEM_callocN(sizeof(IDOverrideLibraryProperty), __func__);
    op->rna_path = BLI_strdup(rna_path);
    BLI_addtail(&override->properties, op);

    if (r_created) {
      *r_created = true;
    }
  }
  else if (r_created) {
    *r_created = false;
  }

  return op;
}

void bke_override_property_copy(IDOverrideLibraryProperty *op_dst,
                                IDOverrideLibraryProperty *op_src)
{
  op_dst->rna_path = BLI_strdup(op_src->rna_path);
  BLI_duplicatelist(&op_dst->operations, &op_src->operations);

  for (IDOverrideLibraryPropertyOperation *opop_dst = op_dst->operations.first,
                                          *opop_src = op_src->operations.first;
       opop_dst;
       opop_dst = opop_dst->next, opop_src = opop_src->next) {
    bke_override_property_operation_copy(opop_dst, opop_src);
  }
}

void bke_override_property_clear(IDOverrideLibraryProperty *op)
{
  BLI_assert(op->rna_path != NULL);

  MEM_freeN(op->rna_path);

  for (IDOverrideLibraryPropertyOperation *opop = op->operations.first; opop; opop = opop->next) {
    bke_override_property_operation_clear(opop);
  }
  BLI_freelistN(&op->operations);
}

/**
 * Remove and free given \a override_property from given ID \a override.
 */
void BKE_override_library_property_delete(IDOverrideLibrary *override,
                                          IDOverrideLibraryProperty *override_property)
{
  bke_override_property_clear(override_property);
  BLI_freelinkN(&override->properties, override_property);
}

/**
 * Find override property operation from given sub-item(s), if it exists.
 */
IDOverrideLibraryPropertyOperation *BKE_override_library_property_operation_find(
    IDOverrideLibraryProperty *override_property,
    const char *subitem_refname,
    const char *subitem_locname,
    const int subitem_refindex,
    const int subitem_locindex,
    const bool strict,
    bool *r_strict)
{
  IDOverrideLibraryPropertyOperation *opop;
  const int subitem_defindex = -1;

  if (r_strict) {
    *r_strict = true;
  }

  if (subitem_locname != NULL) {
    opop = BLI_findstring_ptr(&override_property->operations,
                              subitem_locname,
                              offsetof(IDOverrideLibraryPropertyOperation, subitem_local_name));

    if (opop == NULL) {
      return NULL;
    }

    if (subitem_refname == NULL || opop->subitem_reference_name == NULL) {
      return subitem_refname == opop->subitem_reference_name ? opop : NULL;
    }
    return (subitem_refname != NULL && opop->subitem_reference_name != NULL &&
            STREQ(subitem_refname, opop->subitem_reference_name)) ?
               opop :
               NULL;
  }

  if (subitem_refname != NULL) {
    opop = BLI_findstring_ptr(
        &override_property->operations,
        subitem_refname,
        offsetof(IDOverrideLibraryPropertyOperation, subitem_reference_name));

    if (opop == NULL) {
      return NULL;
    }

    if (subitem_locname == NULL || opop->subitem_local_name == NULL) {
      return subitem_locname == opop->subitem_local_name ? opop : NULL;
    }
    return (subitem_locname != NULL && opop->subitem_local_name != NULL &&
            STREQ(subitem_locname, opop->subitem_local_name)) ?
               opop :
               NULL;
  }

  if ((opop = BLI_listbase_bytes_find(
           &override_property->operations,
           &subitem_locindex,
           sizeof(subitem_locindex),
           offsetof(IDOverrideLibraryPropertyOperation, subitem_local_index)))) {
    return ELEM(subitem_refindex, -1, opop->subitem_reference_index) ? opop : NULL;
  }

  if ((opop = BLI_listbase_bytes_find(
           &override_property->operations,
           &subitem_refindex,
           sizeof(subitem_refindex),
           offsetof(IDOverrideLibraryPropertyOperation, subitem_reference_index)))) {
    return ELEM(subitem_locindex, -1, opop->subitem_local_index) ? opop : NULL;
  }

  /* index == -1 means all indices, that is valid fallback in case we requested specific index. */
  if (!strict && (subitem_locindex != subitem_defindex) &&
      (opop = BLI_listbase_bytes_find(
           &override_property->operations,
           &subitem_defindex,
           sizeof(subitem_defindex),
           offsetof(IDOverrideLibraryPropertyOperation, subitem_local_index)))) {
    if (r_strict) {
      *r_strict = false;
    }
    return opop;
  }

  return NULL;
}

/**
 * Find override property operation from given sub-item(s), or create it if it does not exist.
 */
IDOverrideLibraryPropertyOperation *BKE_override_library_property_operation_get(
    IDOverrideLibraryProperty *override_property,
    const short operation,
    const char *subitem_refname,
    const char *subitem_locname,
    const int subitem_refindex,
    const int subitem_locindex,
    const bool strict,
    bool *r_strict,
    bool *r_created)
{
  IDOverrideLibraryPropertyOperation *opop = BKE_override_library_property_operation_find(
      override_property,
      subitem_refname,
      subitem_locname,
      subitem_refindex,
      subitem_locindex,
      strict,
      r_strict);

  if (opop == NULL) {
    opop = MEM_callocN(sizeof(IDOverrideLibraryPropertyOperation), __func__);
    opop->operation = operation;
    if (subitem_locname) {
      opop->subitem_local_name = BLI_strdup(subitem_locname);
    }
    if (subitem_refname) {
      opop->subitem_reference_name = BLI_strdup(subitem_refname);
    }
    opop->subitem_local_index = subitem_locindex;
    opop->subitem_reference_index = subitem_refindex;

    BLI_addtail(&override_property->operations, opop);

    if (r_created) {
      *r_created = true;
    }
  }
  else if (r_created) {
    *r_created = false;
  }

  return opop;
}

void bke_override_property_operation_copy(IDOverrideLibraryPropertyOperation *opop_dst,
                                          IDOverrideLibraryPropertyOperation *opop_src)
{
  if (opop_src->subitem_reference_name) {
    opop_dst->subitem_reference_name = BLI_strdup(opop_src->subitem_reference_name);
  }
  if (opop_src->subitem_local_name) {
    opop_dst->subitem_local_name = BLI_strdup(opop_src->subitem_local_name);
  }
}

void bke_override_property_operation_clear(IDOverrideLibraryPropertyOperation *opop)
{
  if (opop->subitem_reference_name) {
    MEM_freeN(opop->subitem_reference_name);
  }
  if (opop->subitem_local_name) {
    MEM_freeN(opop->subitem_local_name);
  }
}

/**
 * Remove and free given \a override_property_operation from given ID \a override_property.
 */
void BKE_override_library_property_operation_delete(
    IDOverrideLibraryProperty *override_property,
    IDOverrideLibraryPropertyOperation *override_property_operation)
{
  bke_override_property_operation_clear(override_property_operation);
  BLI_freelinkN(&override_property->operations, override_property_operation);
}

/**
 * Check that status of local data-block is still valid against current reference one.
 *
 * It means that all overridable, but not overridden, properties' local values must be equal to
 * reference ones. Clears #LIB_TAG_OVERRIDE_OK if they do not.
 *
 * This is typically used to detect whether some property has been changed in local and a new
 * #IDOverrideProperty (of #IDOverridePropertyOperation) has to be added.
 *
 * \return true if status is OK, false otherwise. */
bool BKE_override_library_status_check_local(Main *bmain, ID *local)
{
  BLI_assert(local->override_library != NULL);

  ID *reference = local->override_library->reference;

  if (reference == NULL) {
    /* This is an override template, local status is always OK! */
    return true;
  }

  BLI_assert(GS(local->name) == GS(reference->name));

  /* Note that reference is assumed always valid, caller has to ensure that itself. */

  PointerRNA rnaptr_local, rnaptr_reference;
  RNA_id_pointer_create(local, &rnaptr_local);
  RNA_id_pointer_create(reference, &rnaptr_reference);

  if (!RNA_struct_override_matches(bmain,
                                   &rnaptr_local,
                                   &rnaptr_reference,
                                   NULL,
                                   local->override_library,
                                   RNA_OVERRIDE_COMPARE_IGNORE_NON_OVERRIDABLE |
                                       RNA_OVERRIDE_COMPARE_IGNORE_OVERRIDDEN,
                                   NULL)) {
    local->tag &= ~LIB_TAG_OVERRIDE_LIBRARY_REFOK;
    return false;
  }

  return true;
}

/**
 * Check that status of reference data-block is still valid against current local one.
 *
 * It means that all non-overridden properties' local values must be equal to reference ones.
 * Clears LIB_TAG_OVERRIDE_OK if they do not.
 *
 * This is typically used to detect whether some reference has changed and local
 * needs to be updated against it.
 *
 * \return true if status is OK, false otherwise. */
bool BKE_override_library_status_check_reference(Main *bmain, ID *local)
{
  BLI_assert(local->override_library != NULL);

  ID *reference = local->override_library->reference;

  if (reference == NULL) {
    /* This is an override template, reference is virtual, so its status is always OK! */
    return true;
  }

  BLI_assert(GS(local->name) == GS(reference->name));

  if (reference->override_library && (reference->tag & LIB_TAG_OVERRIDE_LIBRARY_REFOK) == 0) {
    if (!BKE_override_library_status_check_reference(bmain, reference)) {
      /* If reference is also override of another data-block, and its status is not OK,
       * then this override is not OK either.
       * Note that this should only happen when reloading libraries... */
      local->tag &= ~LIB_TAG_OVERRIDE_LIBRARY_REFOK;
      return false;
    }
  }

  PointerRNA rnaptr_local, rnaptr_reference;
  RNA_id_pointer_create(local, &rnaptr_local);
  RNA_id_pointer_create(reference, &rnaptr_reference);

  if (!RNA_struct_override_matches(bmain,
                                   &rnaptr_local,
                                   &rnaptr_reference,
                                   NULL,
                                   local->override_library,
                                   RNA_OVERRIDE_COMPARE_IGNORE_OVERRIDDEN,
                                   NULL)) {
    local->tag &= ~LIB_TAG_OVERRIDE_LIBRARY_REFOK;
    return false;
  }

  return true;
}

/**
 * Compares local and reference data-blocks and create new override operations as needed,
 * or reset to reference values if overriding is not allowed.
 *
 * \note Defining override operations is only mandatory before saving a `.blend` file on disk
 * (not for undo!).
 * Knowing that info at runtime is only useful for UI/UX feedback.
 *
 * \note This is by far the biggest operation (the more time-consuming) of the three so far,
 * since it has to go over all properties in depth (all overridable ones at least).
 * Generating diff values and applying overrides are much cheaper.
 *
 * \return true if new overriding op was created, or some local data was reset. */
bool BKE_override_library_operations_create(Main *bmain, ID *local, const bool force_auto)
{
  BLI_assert(local->override_library != NULL);
  const bool is_template = (local->override_library->reference == NULL);
  bool ret = false;

  if (!is_template && (force_auto || local->override_library->flag & OVERRIDE_LIBRARY_AUTO)) {
    PointerRNA rnaptr_local, rnaptr_reference;
    RNA_id_pointer_create(local, &rnaptr_local);
    RNA_id_pointer_create(local->override_library->reference, &rnaptr_reference);

    eRNAOverrideMatchResult report_flags = 0;
    RNA_struct_override_matches(bmain,
                                &rnaptr_local,
                                &rnaptr_reference,
                                NULL,
                                local->override_library,
                                RNA_OVERRIDE_COMPARE_CREATE | RNA_OVERRIDE_COMPARE_RESTORE,
                                &report_flags);
    if (report_flags & RNA_OVERRIDE_MATCH_RESULT_CREATED) {
      ret = true;
    }
#ifndef NDEBUG
    if (report_flags & RNA_OVERRIDE_MATCH_RESULT_RESTORED) {
      printf("We did restore some properties of %s from its reference.\n", local->name);
    }
    if (ret) {
      printf("We did generate library override rules for %s\n", local->name);
    }
    else {
      printf("No new library override rules for %s\n", local->name);
    }
#endif
  }
  return ret;
}

/** Check all overrides from given \a bmain and create/update overriding operations as needed. */
void BKE_main_override_library_operations_create(Main *bmain, const bool force_auto)
{
  ID *id;

  FOREACH_MAIN_ID_BEGIN (bmain, id) {
    if (force_auto ||
        (ID_IS_OVERRIDE_LIBRARY_AUTO(id) && (id->tag & LIB_TAG_OVERRIDE_LIBRARY_AUTOREFRESH))) {
      BKE_override_library_operations_create(bmain, id, force_auto);
      id->tag &= ~LIB_TAG_OVERRIDE_LIBRARY_AUTOREFRESH;
    }
  }
  FOREACH_MAIN_ID_END;
}

/** Update given override from its reference (re-applying overridden properties). */
void BKE_override_library_update(Main *bmain, ID *local)
{
  if (local->override_library == NULL || local->override_library->reference == NULL) {
    return;
  }

  /* Recursively do 'ancestors' overrides first, if any. */
  if (local->override_library->reference->override_library &&
      (local->override_library->reference->tag & LIB_TAG_OVERRIDE_LIBRARY_REFOK) == 0) {
    BKE_override_library_update(bmain, local->override_library->reference);
  }

  /* We want to avoid having to remap here, however creating up-to-date override is much simpler
   * if based on reference than on current override.
   * So we work on temp copy of reference, and 'swap' its content with local. */

  /* XXX We need a way to get off-Main copies of IDs (similar to localized mats/texts/ etc.)!
   *     However, this is whole bunch of code work in itself, so for now plain stupid ID copy will
   *     do, as inn-efficient as it is. :/
   *     Actually, maybe not! Since we are swapping with original ID's local content, we want to
   *     keep user-count in correct state when freeing tmp_id
   *     (and that user-counts of IDs used by 'new' local data also remain correct). */
  /* This would imply change in handling of usercout all over RNA
   * (and possibly all over Blender code).
   * Not impossible to do, but would rather see first if extra useless usual user handling
   * is actually a (performances) issue here. */

  ID *tmp_id;
  BKE_id_copy(bmain, local->override_library->reference, &tmp_id);

  if (tmp_id == NULL) {
    return;
  }

  PointerRNA rnaptr_src, rnaptr_dst, rnaptr_storage_stack, *rnaptr_storage = NULL;
  RNA_id_pointer_create(local, &rnaptr_src);
  RNA_id_pointer_create(tmp_id, &rnaptr_dst);
  if (local->override_library->storage) {
    rnaptr_storage = &rnaptr_storage_stack;
    RNA_id_pointer_create(local->override_library->storage, rnaptr_storage);
  }

  RNA_struct_override_apply(
      bmain, &rnaptr_dst, &rnaptr_src, rnaptr_storage, local->override_library);

  /* This also transfers all pointers (memory) owned by local to tmp_id, and vice-versa.
   * So when we'll free tmp_id, we'll actually free old, outdated data from local. */
  BKE_id_swap(bmain, local, tmp_id);

  /* Again, horribly inn-efficient in our case, we need something off-Main
   * (aka more generic nolib copy/free stuff)! */
  /* XXX And crashing in complex cases (e.g. because depsgraph uses same data...). */
  BKE_id_free_ex(bmain, tmp_id, LIB_ID_FREE_NO_UI_USER, true);

  if (local->override_library->storage) {
    /* We know this datablock is not used anywhere besides local->override->storage. */
    /* XXX For until we get fully shadow copies, we still need to ensure storage releases
     *     its usage of any ID pointers it may have. */
    BKE_id_free_ex(bmain, local->override_library->storage, LIB_ID_FREE_NO_UI_USER, true);
    local->override_library->storage = NULL;
  }

  local->tag |= LIB_TAG_OVERRIDE_LIBRARY_REFOK;

  /* Full rebuild of Depsgraph! */
  DEG_on_visible_update(
      bmain,
      true); /* XXX Is this actual valid replacement for old DAG_relations_tag_update(bmain) ? */
}

/** Update all overrides from given \a bmain. */
void BKE_main_override_library_update(Main *bmain)
{
  ID *id;

  FOREACH_MAIN_ID_BEGIN (bmain, id) {
    if (id->override_library != NULL && id->lib == NULL) {
      BKE_override_library_update(bmain, id);
    }
  }
  FOREACH_MAIN_ID_END;
}

/**
 * Storage (how to store overriding data into `.blend` files).
 *
 * Basically:
 * 1) Only 'differential' storage needs special handling here. All others (replacing values or
 *    inserting/removing items from a collection) can be handled with simply storing current
 *    content of local data-block.
 * 2) We store the differential value into a second 'ghost' data-block,
 *    which is an empty ID of same type as local one,
 *    where we only define values that need differential data.
 *
 * This avoids us having to modify 'real' data-block at write time (and restoring it afterwards),
 * which is inneficient, and potentially dangerous (in case of concurrent access...), while not
 * using much extra memory in typical cases.  It also ensures stored data-block always contains
 * exact same data as "desired" ones (kind of "baked" data-blocks).
 */

/** Initialize an override storage. */
OverrideLibraryStorage *BKE_override_library_operations_store_initialize(void)
{
  return BKE_main_new();
}

/**
 * Generate suitable 'write' data (this only affects differential override operations).
 *
 * Note that \a local ID is no more modified by this call,
 * all extra data are stored in its temp \a storage_id copy. */
ID *BKE_override_library_operations_store_start(Main *bmain,
                                                OverrideLibraryStorage *override_storage,
                                                ID *local)
{
  BLI_assert(local->override_library != NULL);
  BLI_assert(override_storage != NULL);
  const bool is_template = (local->override_library->reference == NULL);

  if (is_template) {
    /* This is actually purely local data with an override template, nothing to do here! */
    return NULL;
  }

  /* Forcefully ensure we know about all needed override operations. */
  BKE_override_library_operations_create(bmain, local, false);

  ID *storage_id;
#ifdef DEBUG_OVERRIDE_TIMEIT
  TIMEIT_START_AVERAGED(BKE_override_operations_store_start);
#endif

  /* XXX TODO We may also want a specialized handling of things here too, to avoid copying heavy
   * never-overridable data (like Mesh geometry etc.)? And also maybe avoid lib reference-counting
   * completely (shallow copy...). */
  /* This would imply change in handling of user-count all over RNA
   * (and possibly all over Blender code).
   * Not impossible to do, but would rather see first is extra useless usual user handling is
   * actually a (performances) issue here, before doing it. */
  BKE_id_copy((Main *)override_storage, local, &storage_id);

  if (storage_id != NULL) {
    PointerRNA rnaptr_reference, rnaptr_final, rnaptr_storage;
    RNA_id_pointer_create(local->override_library->reference, &rnaptr_reference);
    RNA_id_pointer_create(local, &rnaptr_final);
    RNA_id_pointer_create(storage_id, &rnaptr_storage);

    if (!RNA_struct_override_store(
            bmain, &rnaptr_final, &rnaptr_reference, &rnaptr_storage, local->override_library)) {
      BKE_id_free_ex(override_storage, storage_id, LIB_ID_FREE_NO_UI_USER, true);
      storage_id = NULL;
    }
  }

  local->override_library->storage = storage_id;

#ifdef DEBUG_OVERRIDE_TIMEIT
  TIMEIT_END_AVERAGED(BKE_override_operations_store_start);
#endif
  return storage_id;
}

/** Restore given ID modified by \a BKE_override_operations_store_start, to its original state. */
void BKE_override_library_operations_store_end(OverrideLibraryStorage *UNUSED(override_storage),
                                               ID *local)
{
  BLI_assert(local->override_library != NULL);

  /* Nothing else to do here really, we need to keep all temp override storage data-blocks in
   * memory until whole file is written anyway (otherwise we'd get mem pointers overlap...). */
  local->override_library->storage = NULL;
}

void BKE_override_library_operations_store_finalize(OverrideLibraryStorage *override_storage)
{
  /* We cannot just call BKE_main_free(override_storage), not until we have option to make 'ghost'
   * copies of IDs without increasing usercount of used data-blocks. */
  ID *id;

  FOREACH_MAIN_ID_BEGIN (override_storage, id) {
    BKE_id_free_ex(override_storage, id, LIB_ID_FREE_NO_UI_USER, true);
  }
  FOREACH_MAIN_ID_END;

  BKE_main_free(override_storage);
}
