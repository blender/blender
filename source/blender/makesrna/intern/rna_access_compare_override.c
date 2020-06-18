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
 * \ingroup RNA
 */

#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_ID.h"
#include "DNA_constraint_types.h"
#include "DNA_modifier_types.h"

#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

//#define DEBUG_OVERRIDE_TIMEIT

#ifdef DEBUG_OVERRIDE_TIMEIT
#  include "PIL_time_utildefines.h"
#endif

#include "BKE_idprop.h"
#include "BKE_lib_override.h"
#include "BKE_main.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "rna_access_internal.h"
#include "rna_internal.h"

int RNA_property_override_flag(PropertyRNA *prop)
{
  return rna_ensure_property(prop)->flag_override;
}

/** \note Does not take into account editable status, this has to be checked separately
 * (using #RNA_property_editable_flag() usually). */
bool RNA_property_overridable_get(PointerRNA *ptr, PropertyRNA *prop)
{
  if (prop->magic == RNA_MAGIC) {
    /* Special handling for insertions of constraints or modifiers... */
    /* TODO Note We may want to add a more generic system to RNA
     * (like a special property in struct of items)
     * if we get more override-able collections,
     * for now we can live with those special-cases handling I think. */
    if (RNA_struct_is_a(ptr->type, &RNA_Constraint)) {
      bConstraint *con = ptr->data;
      if (con->flag & CONSTRAINT_OVERRIDE_LIBRARY_LOCAL) {
        return true;
      }
    }
    else if (RNA_struct_is_a(ptr->type, &RNA_Modifier)) {
      ModifierData *mod = ptr->data;
      if (mod->flag & eModifierFlag_OverrideLibrary_Local) {
        return true;
      }
    }
    /* If this is a RNA-defined property (real or 'virtual' IDProp),
     * we want to use RNA prop flag. */
    return !(prop->flag_override & PROPOVERRIDE_NO_COMPARISON) &&
           (prop->flag_override & PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  }
  else {
    /* If this is a real 'pure' IDProp (aka custom property), we want to use the IDProp flag. */
    IDProperty *idprop = (IDProperty *)prop;
    return (idprop->flag & IDP_FLAG_OVERRIDABLE_LIBRARY) != 0;
  }
}

/* Should only be used for custom properties */
bool RNA_property_overridable_library_set(PointerRNA *UNUSED(ptr),
                                          PropertyRNA *prop,
                                          const bool is_overridable)
{
  /* Only works for pure custom properties IDProps. */
  if (prop->magic != RNA_MAGIC) {
    IDProperty *idprop = (IDProperty *)prop;

    idprop->flag = is_overridable ? (idprop->flag | IDP_FLAG_OVERRIDABLE_LIBRARY) :
                                    (idprop->flag & ~IDP_FLAG_OVERRIDABLE_LIBRARY);
    return true;
  }

  return false;
}

bool RNA_property_overridden(PointerRNA *ptr, PropertyRNA *prop)
{
  char *rna_path = RNA_path_from_ID_to_property(ptr, prop);
  ID *id = ptr->owner_id;

  if (rna_path == NULL || id == NULL || id->override_library == NULL) {
    return false;
  }

  return (BKE_lib_override_library_property_find(id->override_library, rna_path) != NULL);
}

bool RNA_property_comparable(PointerRNA *UNUSED(ptr), PropertyRNA *prop)
{
  prop = rna_ensure_property(prop);

  return !(prop->flag_override & PROPOVERRIDE_NO_COMPARISON);
}

static bool rna_property_override_operation_apply(Main *bmain,
                                                  PointerRNA *ptr_local,
                                                  PointerRNA *ptr_override,
                                                  PointerRNA *ptr_storage,
                                                  PropertyRNA *prop_local,
                                                  PropertyRNA *prop_override,
                                                  PropertyRNA *prop_storage,
                                                  PointerRNA *ptr_item_local,
                                                  PointerRNA *ptr_item_override,
                                                  PointerRNA *ptr_item_storage,
                                                  IDOverrideLibraryPropertyOperation *opop);

bool RNA_property_copy(
    Main *bmain, PointerRNA *ptr, PointerRNA *fromptr, PropertyRNA *prop, int index)
{
  if (!RNA_property_editable(ptr, prop)) {
    return false;
  }

  PropertyRNA *prop_dst = prop;
  PropertyRNA *prop_src = prop;

  /* Ensure we get real property data,
   * be it an actual RNA property, or an IDProperty in disguise. */
  prop_dst = rna_ensure_property_realdata(&prop_dst, ptr);
  prop_src = rna_ensure_property_realdata(&prop_src, fromptr);

  /* IDprops: destination may not exist, if source does and is set, try to create it. */
  /* Note: this is sort of quick hack/bandage to fix the issue,
   * we need to rethink how IDProps are handled in 'diff' RNA code completely, imho... */
  if (prop_src != NULL && prop_dst == NULL && RNA_property_is_set(fromptr, prop)) {
    BLI_assert(prop_src->magic != RNA_MAGIC);
    IDProperty *idp_dst = RNA_struct_idprops(ptr, true);
    IDProperty *prop_idp_dst = IDP_CopyProperty((IDProperty *)prop_src);
    IDP_AddToGroup(idp_dst, prop_idp_dst);
    rna_idproperty_touch(prop_idp_dst);
    /* Nothing else to do here... */
    return true;
  }

  if (ELEM(NULL, prop_dst, prop_src)) {
    return false;
  }

  IDOverrideLibraryPropertyOperation opop = {
      .operation = IDOVERRIDE_LIBRARY_OP_REPLACE,
      .subitem_reference_index = index,
      .subitem_local_index = index,
  };
  return rna_property_override_operation_apply(
      bmain, ptr, fromptr, NULL, prop_dst, prop_src, NULL, NULL, NULL, NULL, &opop);
}

static int rna_property_override_diff(Main *bmain,
                                      PointerRNA *ptr_a,
                                      PointerRNA *ptr_b,
                                      PropertyRNA *prop,
                                      PropertyRNA *prop_a,
                                      PropertyRNA *prop_b,
                                      const char *rna_path,
                                      const size_t rna_path_len,
                                      eRNACompareMode mode,
                                      IDOverrideLibrary *override,
                                      const int flags,
                                      eRNAOverrideMatchResult *r_report_flags);

bool RNA_property_equals(
    Main *bmain, PointerRNA *ptr_a, PointerRNA *ptr_b, PropertyRNA *prop, eRNACompareMode mode)
{
  BLI_assert(ELEM(mode, RNA_EQ_STRICT, RNA_EQ_UNSET_MATCH_ANY, RNA_EQ_UNSET_MATCH_NONE));

  return (rna_property_override_diff(
              bmain, ptr_a, ptr_b, prop, NULL, NULL, NULL, 0, mode, NULL, 0, NULL) == 0);
}

bool RNA_struct_equals(Main *bmain, PointerRNA *ptr_a, PointerRNA *ptr_b, eRNACompareMode mode)
{
  CollectionPropertyIterator iter;
  PropertyRNA *iterprop;
  bool equals = true;

  if (ptr_a == NULL && ptr_b == NULL) {
    return true;
  }
  else if (ptr_a == NULL || ptr_b == NULL) {
    return false;
  }
  else if (ptr_a->type != ptr_b->type) {
    return false;
  }

  iterprop = RNA_struct_iterator_property(ptr_a->type);

  RNA_property_collection_begin(ptr_a, iterprop, &iter);
  for (; iter.valid; RNA_property_collection_next(&iter)) {
    PropertyRNA *prop = iter.ptr.data;

    if (!RNA_property_equals(bmain, ptr_a, ptr_b, prop, mode)) {
      equals = false;
      break;
    }
  }
  RNA_property_collection_end(&iter);

  return equals;
}

/* Low-level functions, also used by non-override RNA API like copy or equality check. */

/**
 * Generic RNA property diff function.
 *
 * \note about \a prop and \a prop_a/prop_b parameters:
 * the former is expected to be an 'un-resolved' one,
 * while the two later are expected to be fully resolved ones
 * (i.e. to be the IDProps when they should be, etc.).
 * When \a prop is given, \a prop_a and \a prop_b should always be NULL, and vice-versa.
 * This is necessary, because we cannot perform 'set/unset' checks on resolved properties
 * (unset IDProps would merely be NULL then).
 *
 * \note When there is no equality,
 * but we cannot determine an order (greater than/lesser than), we return 1.
 */
static int rna_property_override_diff(Main *bmain,
                                      PointerRNA *ptr_a,
                                      PointerRNA *ptr_b,
                                      PropertyRNA *prop,
                                      PropertyRNA *prop_a,
                                      PropertyRNA *prop_b,
                                      const char *rna_path,
                                      const size_t rna_path_len,
                                      eRNACompareMode mode,
                                      IDOverrideLibrary *override,
                                      const int flags,
                                      eRNAOverrideMatchResult *r_report_flags)
{
  if (prop != NULL) {
    BLI_assert(prop_a == NULL && prop_b == NULL);
    prop_a = prop;
    prop_b = prop;
  }

  if (ELEM(NULL, prop_a, prop_b)) {
    return (prop_a == prop_b) ? 0 : 1;
  }

  if (!RNA_property_comparable(ptr_a, prop_a) || !RNA_property_comparable(ptr_b, prop_b)) {
    return 0;
  }

  if (mode == RNA_EQ_UNSET_MATCH_ANY) {
    /* uninitialized properties are assumed to match anything */
    if (!RNA_property_is_set(ptr_a, prop_a) || !RNA_property_is_set(ptr_b, prop_b)) {
      return 0;
    }
  }
  else if (mode == RNA_EQ_UNSET_MATCH_NONE) {
    /* unset properties never match set properties */
    if (RNA_property_is_set(ptr_a, prop_a) != RNA_property_is_set(ptr_b, prop_b)) {
      return 1;
    }
  }

  if (prop != NULL) {
    /* Ensure we get real property data, be it an actual RNA property,
     * or an IDProperty in disguise. */
    prop_a = rna_ensure_property_realdata(&prop_a, ptr_a);
    prop_b = rna_ensure_property_realdata(&prop_b, ptr_b);

    if (ELEM(NULL, prop_a, prop_b)) {
      return (prop_a == prop_b) ? 0 : 1;
    }
  }

  /* Check if we are working with arrays. */
  const bool is_array_a = RNA_property_array_check(prop_a);
  const bool is_array_b = RNA_property_array_check(prop_b);

  if (is_array_a != is_array_b) {
    /* Should probably never happen actually... */
    BLI_assert(0);
    return is_array_a ? 1 : -1;
  }

  /* Get the length of the array to work with. */
  const int len_a = RNA_property_array_length(ptr_a, prop_a);
  const int len_b = RNA_property_array_length(ptr_b, prop_b);

  if (len_a != len_b) {
    /* Do not handle override in that case,
     * we do not support insertion/deletion from arrays for now. */
    return len_a > len_b ? 1 : -1;
  }

  if (is_array_a && len_a == 0) {
    /* Empty arrays, will happen in some case with dynamic ones. */
    return 0;
  }

  RNAPropOverrideDiff override_diff = NULL;
  /* Special case for IDProps, we use default callback then. */
  if (prop_a->magic != RNA_MAGIC) {
    override_diff = rna_property_override_diff_default;
    if (prop_b->magic == RNA_MAGIC && prop_b->override_diff != override_diff) {
      override_diff = NULL;
    }
  }
  else if (prop_b->magic != RNA_MAGIC) {
    override_diff = rna_property_override_diff_default;
    if (prop_a->override_diff != override_diff) {
      override_diff = NULL;
    }
  }
  else if (prop_a->override_diff == prop_b->override_diff) {
    override_diff = prop_a->override_diff;
  }

  if (override_diff == NULL) {
#ifndef NDEBUG
    printf("'%s' gives unmatching or NULL RNA diff callbacks, should not happen (%d vs. %d).\n",
           rna_path ?
               rna_path :
               (prop_a->magic != RNA_MAGIC ? ((IDProperty *)prop_a)->name : prop_a->identifier),
           prop_a->magic == RNA_MAGIC,
           prop_b->magic == RNA_MAGIC);
#endif
    BLI_assert(0);
    return 1;
  }

  bool override_changed = false;
  int diff_flags = flags;
  if (!RNA_property_overridable_get(ptr_a, prop_a)) {
    diff_flags &= ~RNA_OVERRIDE_COMPARE_CREATE;
  }
  const int diff = override_diff(bmain,
                                 ptr_a,
                                 ptr_b,
                                 prop_a,
                                 prop_b,
                                 len_a,
                                 len_b,
                                 mode,
                                 override,
                                 rna_path,
                                 rna_path_len,
                                 diff_flags,
                                 &override_changed);
  if (override_changed && r_report_flags) {
    *r_report_flags |= RNA_OVERRIDE_MATCH_RESULT_CREATED;
  }

  return diff;
}

/* Modify local data-block to make it ready for override application
 * (only needed for diff operations, where we use
 * the local data-block's data as second operand). */
static bool rna_property_override_operation_store(Main *bmain,
                                                  PointerRNA *ptr_local,
                                                  PointerRNA *ptr_reference,
                                                  PointerRNA *ptr_storage,
                                                  PropertyRNA *prop_local,
                                                  PropertyRNA *prop_reference,
                                                  PropertyRNA *prop_storage,
                                                  IDOverrideLibraryProperty *op)
{
  int len_local, len_reference, len_storage = 0;
  bool changed = false;

  if (ptr_storage == NULL) {
    return changed;
  }

  /* get the length of the array to work with */
  len_local = RNA_property_array_length(ptr_local, prop_local);
  len_reference = RNA_property_array_length(ptr_reference, prop_reference);
  if (prop_storage) {
    len_storage = RNA_property_array_length(ptr_storage, prop_storage);
  }

  if (len_local != len_reference || len_local != len_storage) {
    /* Do not handle override in that case,
     * we do not support insertion/deletion from arrays for now. */
    return changed;
  }

  RNAPropOverrideStore override_store = NULL;
  /* Special case for IDProps, we use default callback then. */
  if (prop_local->magic != RNA_MAGIC) {
    override_store = rna_property_override_store_default;
    if (prop_reference->magic == RNA_MAGIC && prop_reference->override_store != override_store) {
      override_store = NULL;
    }
  }
  else if (prop_reference->magic != RNA_MAGIC) {
    override_store = rna_property_override_store_default;
    if (prop_local->override_store != override_store) {
      override_store = NULL;
    }
  }
  else if (prop_local->override_store == prop_reference->override_store) {
    override_store = prop_local->override_store;
  }

  if (ptr_storage != NULL && prop_storage->magic == RNA_MAGIC &&
      prop_storage->override_store != override_store) {
    override_store = NULL;
  }

  if (override_store == NULL) {
#ifndef NDEBUG
    printf("'%s' gives unmatching or NULL RNA store callbacks, should not happen (%d vs. %d).\n",
           op->rna_path,
           prop_local->magic == RNA_MAGIC,
           prop_reference->magic == RNA_MAGIC);
#endif
    BLI_assert(0);
    return changed;
  }

  LISTBASE_FOREACH (IDOverrideLibraryPropertyOperation *, opop, &op->operations) {
    /* Only needed for diff operations. */
    if (!ELEM(opop->operation,
              IDOVERRIDE_LIBRARY_OP_ADD,
              IDOVERRIDE_LIBRARY_OP_SUBTRACT,
              IDOVERRIDE_LIBRARY_OP_MULTIPLY)) {
      continue;
    }

    if (override_store(bmain,
                       ptr_local,
                       ptr_reference,
                       ptr_storage,
                       prop_local,
                       prop_reference,
                       prop_storage,
                       len_local,
                       len_reference,
                       len_storage,
                       opop)) {
      changed = true;
    }
  }

  return changed;
}

static bool rna_property_override_operation_apply(Main *bmain,
                                                  PointerRNA *ptr_dst,
                                                  PointerRNA *ptr_src,
                                                  PointerRNA *ptr_storage,
                                                  PropertyRNA *prop_dst,
                                                  PropertyRNA *prop_src,
                                                  PropertyRNA *prop_storage,
                                                  PointerRNA *ptr_item_dst,
                                                  PointerRNA *ptr_item_src,
                                                  PointerRNA *ptr_item_storage,
                                                  IDOverrideLibraryPropertyOperation *opop)
{
  int len_dst, len_src, len_storage = 0;

  const short override_op = opop->operation;

  if (!BKE_lib_override_library_property_operation_operands_validate(
          opop, ptr_dst, ptr_src, ptr_storage, prop_dst, prop_src, prop_storage)) {
    return false;
  }

  if (override_op == IDOVERRIDE_LIBRARY_OP_NOOP) {
    return true;
  }

  RNAPropOverrideApply override_apply = NULL;
  /* Special case for IDProps, we use default callback then. */
  if (prop_dst->magic != RNA_MAGIC) {
    override_apply = rna_property_override_apply_default;
    if (prop_src->magic == RNA_MAGIC && prop_src->override_apply != override_apply) {
      override_apply = NULL;
    }
  }
  else if (prop_src->magic != RNA_MAGIC) {
    override_apply = rna_property_override_apply_default;
    if (prop_dst->override_apply != override_apply) {
      override_apply = NULL;
    }
  }
  else if (prop_dst->override_apply == prop_src->override_apply) {
    override_apply = prop_dst->override_apply;
  }

  if (ptr_storage && prop_storage->magic == RNA_MAGIC &&
      prop_storage->override_apply != override_apply) {
    override_apply = NULL;
  }

  if (override_apply == NULL) {
#ifndef NDEBUG
    printf("'%s' gives unmatching or NULL RNA copy callbacks, should not happen (%d vs. %d).\n",
           prop_dst->magic != RNA_MAGIC ? ((IDProperty *)prop_dst)->name : prop_dst->identifier,
           prop_dst->magic == RNA_MAGIC,
           prop_src->magic == RNA_MAGIC);
#endif
    BLI_assert(0);
    return false;
  }

  /* get the length of the array to work with */
  len_dst = RNA_property_array_length(ptr_dst, prop_dst);
  len_src = RNA_property_array_length(ptr_src, prop_src);
  if (ptr_storage) {
    len_storage = RNA_property_array_length(ptr_storage, prop_storage);
  }

  if (len_dst != len_src || (ptr_storage && len_dst != len_storage)) {
    /* Do not handle override in that case,
     * we do not support insertion/deletion from arrays for now. */
    return false;
  }

  /* get and set the default values as appropriate for the various types */
  return override_apply(bmain,
                        ptr_dst,
                        ptr_src,
                        ptr_storage,
                        prop_dst,
                        prop_src,
                        prop_storage,
                        len_dst,
                        len_src,
                        len_storage,
                        ptr_item_dst,
                        ptr_item_src,
                        ptr_item_storage,
                        opop);
}

/**
 * Check whether reference and local overridden data match (are the same),
 * with respect to given restrictive sets of properties.
 * If requested, will generate needed new property overrides, and/or restore values from reference.
 *
 * \param r_report_flags: If given,
 * will be set with flags matching actions taken by the function on \a ptr_local.
 *
 * \return True if _resulting_ \a ptr_local does match \a ptr_reference.
 */
bool RNA_struct_override_matches(Main *bmain,
                                 PointerRNA *ptr_local,
                                 PointerRNA *ptr_reference,
                                 const char *root_path,
                                 const size_t root_path_len,
                                 IDOverrideLibrary *override,
                                 const eRNAOverrideMatch flags,
                                 eRNAOverrideMatchResult *r_report_flags)
{
  CollectionPropertyIterator iter;
  PropertyRNA *iterprop;
  bool matching = true;

  BLI_assert(ptr_local->type == ptr_reference->type);
  BLI_assert(ptr_local->owner_id && ptr_reference->owner_id);

  const bool ignore_non_overridable = (flags & RNA_OVERRIDE_COMPARE_IGNORE_NON_OVERRIDABLE) != 0;
  const bool ignore_overridden = (flags & RNA_OVERRIDE_COMPARE_IGNORE_OVERRIDDEN) != 0;
  const bool do_create = (flags & RNA_OVERRIDE_COMPARE_CREATE) != 0;
  const bool do_restore = (flags & RNA_OVERRIDE_COMPARE_RESTORE) != 0;

#ifdef DEBUG_OVERRIDE_TIMEIT
  static float _sum_time_global = 0.0f;
  static float _num_time_global = 0.0f;
  double _timeit_time_global;
  static float _sum_time_diffing = 0.0f;
  static float _delta_time_diffing = 0.0f;
  static int _num_delta_time_diffing = 0.0f;
  static float _num_time_diffing = 0.0f;
  double _timeit_time_diffing;

  if (!root_path) {
    _delta_time_diffing = 0.0f;
    _num_delta_time_diffing = 0;
    _timeit_time_global = PIL_check_seconds_timer();
  }
#endif

  iterprop = RNA_struct_iterator_property(ptr_local->type);

  for (RNA_property_collection_begin(ptr_local, iterprop, &iter); iter.valid;
       RNA_property_collection_next(&iter)) {
    PropertyRNA *prop_local = iter.ptr.data;
    PropertyRNA *prop_reference = iter.ptr.data;

    /* Ensure we get real property data, be it an actual RNA property,
     * or an IDProperty in disguise. */
    prop_local = rna_ensure_property_realdata(&prop_local, ptr_local);
    prop_reference = rna_ensure_property_realdata(&prop_reference, ptr_reference);

    /* IDProps (custom properties) are even more of a PITA here, we cannot use
     * `rna_ensure_property_realdata()` to deal with them, we have to use the path generated from
     * `prop_local` (which is valid) to access to the actual reference counterpart... */
    if (prop_local != NULL && prop_local->magic != RNA_MAGIC && prop_local == prop_reference) {
      /* We could also use (lower in this code, after rna_path has been computed):
       *    RNA_path_resolve_property(ptr_reference, rna_path, &some_rna_ptr, &prop_reference);
       * But that would be much more costly, and would also fail when ptr_reference
       * is not an ID pointer itself, so we'd need to rebuild it from its owner_id, then check that
       * generated some_rna_ptr and ptr_reference do point to the same data, etc.
       * For now, let's try that simple access, it won't cover all cases but should handle fine
       * most basic custom properties situations. */
      prop_reference = (PropertyRNA *)rna_idproperty_find(ptr_reference,
                                                          ((IDProperty *)prop_local)->name);
    }

    if (ELEM(NULL, prop_local, prop_reference)) {
      continue;
    }

    if (ignore_non_overridable && !RNA_property_overridable_get(ptr_local, prop_local)) {
      continue;
    }

    if (RNA_property_override_flag(prop_local) & PROPOVERRIDE_IGNORE) {
      continue;
    }

#if 0 /* This actually makes things slower, since it has to check for animation paths etc! */
    if (RNA_property_animated(ptr_local, prop_local)) {
      /* We cannot do anything here really, animation is some kind of dynamic overrides that has
       * precedence over static one... */
      continue;
    }
#endif

#define RNA_PATH_BUFFSIZE 8192

    char rna_path_buffer[RNA_PATH_BUFFSIZE];
    char *rna_path = rna_path_buffer;
    size_t rna_path_len = 0;

    /* XXX TODO this will have to be refined to handle collections insertions, and array items */
    if (root_path) {
      BLI_assert(strlen(root_path) == root_path_len);

      const char *prop_name = RNA_property_identifier(prop_local);
      const size_t prop_name_len = strlen(prop_name);

      /* Inlined building, much much more efficient. */
      if (prop_local->magic == RNA_MAGIC) {
        rna_path_len = root_path_len + 1 + prop_name_len;
        if (rna_path_len >= RNA_PATH_BUFFSIZE) {
          rna_path = MEM_mallocN(rna_path_len + 1, __func__);
        }

        memcpy(rna_path, root_path, root_path_len);
        rna_path[root_path_len] = '.';
        memcpy(rna_path + root_path_len + 1, prop_name, prop_name_len);
        rna_path[rna_path_len] = '\0';
      }
      else {
        rna_path_len = root_path_len + 2 + prop_name_len + 2;
        if (rna_path_len >= RNA_PATH_BUFFSIZE) {
          rna_path = MEM_mallocN(rna_path_len + 1, __func__);
        }

        memcpy(rna_path, root_path, root_path_len);
        rna_path[root_path_len] = '[';
        rna_path[root_path_len + 1] = '"';
        memcpy(rna_path + root_path_len + 2, prop_name, prop_name_len);
        rna_path[root_path_len + 2 + prop_name_len] = '"';
        rna_path[root_path_len + 2 + prop_name_len + 1] = ']';
        rna_path[rna_path_len] = '\0';
      }
    }
    else {
      /* This is rather slow, but is not much called, so not really worth optimizing. */
      rna_path = RNA_path_from_ID_to_property(ptr_local, prop_local);
      if (rna_path != NULL) {
        rna_path_len = strlen(rna_path);
      }
    }
    if (rna_path == NULL) {
      continue;
    }

    //    printf("Override Checking %s\n", rna_path);

    IDOverrideLibraryProperty *op = BKE_lib_override_library_property_find(override, rna_path);
    if (ignore_overridden && op != NULL) {
      BKE_lib_override_library_operations_tag(op, IDOVERRIDE_LIBRARY_TAG_UNUSED, false);

      if (rna_path != rna_path_buffer) {
        MEM_freeN(rna_path);
      }
      continue;
    }

#ifdef DEBUG_OVERRIDE_TIMEIT
    if (!root_path) {
      _timeit_time_diffing = PIL_check_seconds_timer();
    }
#endif

    eRNAOverrideMatchResult report_flags = 0;
    const int diff = rna_property_override_diff(bmain,
                                                ptr_local,
                                                ptr_reference,
                                                NULL,
                                                prop_local,
                                                prop_reference,
                                                rna_path,
                                                rna_path_len,
                                                RNA_EQ_STRICT,
                                                override,
                                                flags,
                                                &report_flags);

#ifdef DEBUG_OVERRIDE_TIMEIT
    if (!root_path) {
      const float _delta_time = (float)(PIL_check_seconds_timer() - _timeit_time_diffing);
      _delta_time_diffing += _delta_time;
      _num_delta_time_diffing++;
    }
#endif

    matching = matching && diff == 0;
    if (r_report_flags) {
      *r_report_flags |= report_flags;
    }

    if (diff != 0) {
      /* XXX TODO: refine this for per-item overriding of arrays... */
      op = BKE_lib_override_library_property_find(override, rna_path);
      IDOverrideLibraryPropertyOperation *opop = op ? op->operations.first : NULL;

      if (op != NULL) {
        BKE_lib_override_library_operations_tag(op, IDOVERRIDE_LIBRARY_TAG_UNUSED, false);
      }

      if (do_restore && (report_flags & RNA_OVERRIDE_MATCH_RESULT_CREATED) == 0) {
        /* We are allowed to restore to reference's values. */
        if (ELEM(NULL, op, opop) || opop->operation == IDOVERRIDE_LIBRARY_OP_NOOP) {
          /* We should restore that property to its reference value */
          if (RNA_property_editable(ptr_local, prop_local)) {
            IDOverrideLibraryPropertyOperation opop_tmp = {
                .operation = IDOVERRIDE_LIBRARY_OP_REPLACE,
                .subitem_reference_index = -1,
                .subitem_local_index = -1,
            };
            rna_property_override_operation_apply(bmain,
                                                  ptr_local,
                                                  ptr_reference,
                                                  NULL,
                                                  prop_local,
                                                  prop_reference,
                                                  NULL,
                                                  NULL,
                                                  NULL,
                                                  NULL,
                                                  &opop_tmp);
            if (r_report_flags) {
              *r_report_flags |= RNA_OVERRIDE_MATCH_RESULT_RESTORED;
            }
          }
          else {
            /* Too noisy for now, this triggers on runtime props like transform matrices etc. */
#if 0
            BLI_assert(!"We have differences between reference and "
                       "overriding data on non-editable property.");
#endif
            matching = false;
          }
        }
      }
      else if ((report_flags & RNA_OVERRIDE_MATCH_RESULT_CREATED) == 0 && ELEM(NULL, op, opop)) {
        /* This property is not overridden, and differs from reference, so we have no match. */
        matching = false;
        if (!(do_create || do_restore)) {
          /* Since we have no 'changing' action allowed, we can break here. */
          if (rna_path != rna_path_buffer) {
            MEM_freeN(rna_path);
          }
          break;
        }
      }
    }

    if (rna_path != rna_path_buffer) {
      MEM_freeN(rna_path);
    }
#undef RNA_PATH_BUFFSIZE
  }
  RNA_property_collection_end(&iter);

#ifdef DEBUG_OVERRIDE_TIMEIT
  if (!root_path) {
    const float _delta_time = (float)(PIL_check_seconds_timer() - _timeit_time_global);
    _sum_time_global += _delta_time;
    _num_time_global++;
    _sum_time_diffing += _delta_time_diffing;
    _num_time_diffing++;
    printf("ID: %s\n", ((ID *)ptr_local->owner_id)->name);
    printf("time end      (%s): %.6f\n", __func__, _delta_time);
    printf("time averaged (%s): %.6f (total: %.6f, in %d runs)\n",
           __func__,
           (_sum_time_global / _num_time_global),
           _sum_time_global,
           (int)_num_time_global);
    printf("diffing time end      (%s): %.6f (in %d runs)\n",
           __func__,
           _delta_time_diffing,
           _num_delta_time_diffing);
    printf("diffing time averaged (%s): %.6f (total: %.6f, in %d runs)\n",
           __func__,
           (_sum_time_diffing / _num_time_diffing),
           _sum_time_diffing,
           (int)_num_time_diffing);
  }
#endif

  return matching;
}

/**
 * Store needed second operands into \a storage data-block
 * for differential override operations.
 */
bool RNA_struct_override_store(Main *bmain,
                               PointerRNA *ptr_local,
                               PointerRNA *ptr_reference,
                               PointerRNA *ptr_storage,
                               IDOverrideLibrary *override)
{
  bool changed = false;

#ifdef DEBUG_OVERRIDE_TIMEIT
  TIMEIT_START_AVERAGED(RNA_struct_override_store);
#endif
  LISTBASE_FOREACH (IDOverrideLibraryProperty *, op, &override->properties) {
    /* Simplified for now! */
    PointerRNA data_reference, data_local;
    PropertyRNA *prop_reference, *prop_local;

    if (RNA_path_resolve_property(ptr_local, op->rna_path, &data_local, &prop_local) &&
        RNA_path_resolve_property(ptr_reference, op->rna_path, &data_reference, &prop_reference)) {
      PointerRNA data_storage;
      PropertyRNA *prop_storage = NULL;

      /* It is totally OK if this does not success,
       * only a subset of override operations actually need storage. */
      if (ptr_storage && (ptr_storage->owner_id != NULL)) {
        RNA_path_resolve_property(ptr_storage, op->rna_path, &data_storage, &prop_storage);
      }

      if (rna_property_override_operation_store(bmain,
                                                &data_local,
                                                &data_reference,
                                                &data_storage,
                                                prop_reference,
                                                prop_local,
                                                prop_storage,
                                                op)) {
        changed = true;
      }
    }
  }
#ifdef DEBUG_OVERRIDE_TIMEIT
  TIMEIT_END_AVERAGED(RNA_struct_override_store);
#endif

  return changed;
}

static void rna_property_override_apply_ex(Main *bmain,
                                           PointerRNA *ptr_dst,
                                           PointerRNA *ptr_src,
                                           PointerRNA *ptr_storage,
                                           PropertyRNA *prop_dst,
                                           PropertyRNA *prop_src,
                                           PropertyRNA *prop_storage,
                                           PointerRNA *ptr_item_dst,
                                           PointerRNA *ptr_item_src,
                                           PointerRNA *ptr_item_storage,
                                           IDOverrideLibraryProperty *op,
                                           const bool do_insert)
{
  LISTBASE_FOREACH (IDOverrideLibraryPropertyOperation *, opop, &op->operations) {
    if (!do_insert != !ELEM(opop->operation,
                            IDOVERRIDE_LIBRARY_OP_INSERT_AFTER,
                            IDOVERRIDE_LIBRARY_OP_INSERT_BEFORE)) {
      if (!do_insert) {
        printf("Skipping insert override operations in first pass (%s)!\n", op->rna_path);
      }
      continue;
    }

    /* Note: will have to think about putting that logic into its own function maybe?
     * Would be nice to have it in a single place...
     * Note that here, src is the local saved ID, and dst is a copy of the linked ID (since we use
     * local ID as storage to apply local changes on top of a clean copy of the linked data). */
    PointerRNA private_ptr_item_dst, private_ptr_item_src, private_ptr_item_storage;
    if ((RNA_property_type(prop_dst) == PROP_COLLECTION &&
         RNA_property_type(prop_src) == PROP_COLLECTION &&
         (prop_storage == NULL || RNA_property_type(prop_storage) == PROP_COLLECTION)) &&
        (opop->subitem_local_name != NULL || opop->subitem_reference_name != NULL ||
         opop->subitem_local_index != -1 || opop->subitem_reference_index != -1)) {
      RNA_POINTER_INVALIDATE(&private_ptr_item_dst);
      RNA_POINTER_INVALIDATE(&private_ptr_item_src);
      RNA_POINTER_INVALIDATE(&private_ptr_item_storage);
      if (opop->subitem_local_name != NULL) {
        RNA_property_collection_lookup_string(
            ptr_src, prop_src, opop->subitem_local_name, &private_ptr_item_src);
        if (opop->subitem_reference_name != NULL) {
          RNA_property_collection_lookup_string(
              ptr_dst, prop_dst, opop->subitem_reference_name, &private_ptr_item_dst);
        }
        else {
          RNA_property_collection_lookup_string(
              ptr_dst, prop_dst, opop->subitem_local_name, &private_ptr_item_dst);
        }
      }
      else if (opop->subitem_reference_name != NULL) {
        RNA_property_collection_lookup_string(
            ptr_src, prop_src, opop->subitem_reference_name, &private_ptr_item_src);
        RNA_property_collection_lookup_string(
            ptr_dst, prop_dst, opop->subitem_reference_name, &private_ptr_item_dst);
      }
      else if (opop->subitem_local_index != -1) {
        RNA_property_collection_lookup_int(
            ptr_src, prop_src, opop->subitem_local_index, &private_ptr_item_src);
        if (opop->subitem_reference_index != -1) {
          RNA_property_collection_lookup_int(
              ptr_dst, prop_dst, opop->subitem_reference_index, &private_ptr_item_dst);
        }
        else {
          RNA_property_collection_lookup_int(
              ptr_dst, prop_dst, opop->subitem_local_index, &private_ptr_item_dst);
        }
      }
      else if (opop->subitem_reference_index != -1) {
        RNA_property_collection_lookup_int(
            ptr_src, prop_src, opop->subitem_reference_index, &private_ptr_item_src);
        RNA_property_collection_lookup_int(
            ptr_dst, prop_dst, opop->subitem_reference_index, &private_ptr_item_dst);
      }
      if (prop_storage != NULL) {
        if (opop->subitem_local_name != NULL) {
          RNA_property_collection_lookup_string(
              ptr_storage, prop_storage, opop->subitem_local_name, &private_ptr_item_storage);
        }
        else if (opop->subitem_reference_name != NULL) {
          RNA_property_collection_lookup_string(
              ptr_storage, prop_storage, opop->subitem_reference_name, &private_ptr_item_storage);
        }
        else if (opop->subitem_local_index != -1) {
          RNA_property_collection_lookup_int(
              ptr_storage, prop_storage, opop->subitem_local_index, &private_ptr_item_storage);
        }
        else if (opop->subitem_reference_index != -1) {
          RNA_property_collection_lookup_int(
              ptr_storage, prop_storage, opop->subitem_reference_index, &private_ptr_item_storage);
        }
      }
      ptr_item_dst = &private_ptr_item_dst;
      ptr_item_src = &private_ptr_item_src;
      ptr_item_storage = &private_ptr_item_storage;
    }

    if (!rna_property_override_operation_apply(bmain,
                                               ptr_dst,
                                               ptr_src,
                                               ptr_storage,
                                               prop_dst,
                                               prop_src,
                                               prop_storage,
                                               ptr_item_dst,
                                               ptr_item_src,
                                               ptr_item_storage,
                                               opop)) {
      /* TODO No assert here, would be much much better to just report as warning,
       * failing override applications will probably be fairly common! */
      BLI_assert(0);
    }
  }
}

/**
 * Apply given \a override operations on \a ptr_dst, using \a ptr_src
 * (and \a ptr_storage for differential ops) as source.
 */
void RNA_struct_override_apply(Main *bmain,
                               PointerRNA *ptr_dst,
                               PointerRNA *ptr_src,
                               PointerRNA *ptr_storage,
                               IDOverrideLibrary *override)
{
#ifdef DEBUG_OVERRIDE_TIMEIT
  TIMEIT_START_AVERAGED(RNA_struct_override_apply);
#endif
  /* Note: Applying insert operations in a separate pass is mandatory.
   * We could optimize this later, but for now, as inefficient as it is,
   * don't think this is a critical point.
   */
  bool do_insert = false;
  for (int i = 0; i < 2; i++, do_insert = true) {
    LISTBASE_FOREACH (IDOverrideLibraryProperty *, op, &override->properties) {
      /* Simplified for now! */
      PointerRNA data_src, data_dst;
      PointerRNA data_item_src, data_item_dst;
      PropertyRNA *prop_src, *prop_dst;

      if (RNA_path_resolve_property_and_item_pointer(
              ptr_dst, op->rna_path, &data_dst, &prop_dst, &data_item_dst) &&
          RNA_path_resolve_property_and_item_pointer(
              ptr_src, op->rna_path, &data_src, &prop_src, &data_item_src)) {
        PointerRNA data_storage, data_item_storage;
        PropertyRNA *prop_storage = NULL;

        /* It is totally OK if this does not success,
         * only a subset of override operations actually need storage. */
        if (ptr_storage && (ptr_storage->owner_id != NULL)) {
          RNA_path_resolve_property_and_item_pointer(
              ptr_storage, op->rna_path, &data_storage, &prop_storage, &data_item_storage);
        }

        rna_property_override_apply_ex(bmain,
                                       &data_dst,
                                       &data_src,
                                       prop_storage ? &data_storage : NULL,
                                       prop_dst,
                                       prop_src,
                                       prop_storage,
                                       &data_item_dst,
                                       &data_item_src,
                                       prop_storage ? &data_item_storage : NULL,
                                       op,
                                       do_insert);
      }
#ifndef NDEBUG
      else {
        printf(
            "Failed to apply library override operation to '%s.%s' "
            "(could not resolve some properties, local:  %d, override: %d)\n",
            ((ID *)ptr_src->owner_id)->name,
            op->rna_path,
            RNA_path_resolve_property(ptr_dst, op->rna_path, &data_dst, &prop_dst),
            RNA_path_resolve_property(ptr_src, op->rna_path, &data_src, &prop_src));
      }
#endif
    }
  }
#ifdef DEBUG_OVERRIDE_TIMEIT
  TIMEIT_END_AVERAGED(RNA_struct_override_apply);
#endif
}

IDOverrideLibraryProperty *RNA_property_override_property_find(PointerRNA *ptr, PropertyRNA *prop)
{
  ID *id = ptr->owner_id;

  if (!id || !id->override_library) {
    return NULL;
  }

  char *rna_path = RNA_path_from_ID_to_property(ptr, prop);
  if (rna_path) {
    IDOverrideLibraryProperty *op = BKE_lib_override_library_property_find(id->override_library,
                                                                           rna_path);
    MEM_freeN(rna_path);
    return op;
  }
  return NULL;
}

IDOverrideLibraryProperty *RNA_property_override_property_get(PointerRNA *ptr,
                                                              PropertyRNA *prop,
                                                              bool *r_created)
{
  ID *id = ptr->owner_id;

  if (!id || !id->override_library) {
    return NULL;
  }

  char *rna_path = RNA_path_from_ID_to_property(ptr, prop);
  if (rna_path) {
    IDOverrideLibraryProperty *op = BKE_lib_override_library_property_get(
        id->override_library, rna_path, r_created);
    MEM_freeN(rna_path);
    return op;
  }
  return NULL;
}

IDOverrideLibraryPropertyOperation *RNA_property_override_property_operation_find(
    PointerRNA *ptr, PropertyRNA *prop, const int index, const bool strict, bool *r_strict)
{
  IDOverrideLibraryProperty *op = RNA_property_override_property_find(ptr, prop);

  if (!op) {
    return NULL;
  }

  return BKE_lib_override_library_property_operation_find(
      op, NULL, NULL, index, index, strict, r_strict);
}

IDOverrideLibraryPropertyOperation *RNA_property_override_property_operation_get(
    PointerRNA *ptr,
    PropertyRNA *prop,
    const short operation,
    const int index,
    const bool strict,
    bool *r_strict,
    bool *r_created)
{
  IDOverrideLibraryProperty *op = RNA_property_override_property_get(ptr, prop, NULL);

  if (!op) {
    return NULL;
  }

  return BKE_lib_override_library_property_operation_get(
      op, operation, NULL, NULL, index, index, strict, r_strict, r_created);
}

eRNAOverrideStatus RNA_property_override_library_status(PointerRNA *ptr,
                                                        PropertyRNA *prop,
                                                        const int index)
{
  int override_status = 0;

  if (!BKE_lib_override_library_is_enabled()) {
    return override_status;
  }

  if (!ptr || !prop || !ptr->owner_id || !(ptr->owner_id)->override_library) {
    return override_status;
  }

  if (RNA_property_overridable_get(ptr, prop) && RNA_property_editable_flag(ptr, prop)) {
    override_status |= RNA_OVERRIDE_STATUS_OVERRIDABLE;
  }

  IDOverrideLibraryPropertyOperation *opop = RNA_property_override_property_operation_find(
      ptr, prop, index, false, NULL);
  if (opop != NULL) {
    override_status |= RNA_OVERRIDE_STATUS_OVERRIDDEN;
    if (opop->flag & IDOVERRIDE_LIBRARY_FLAG_MANDATORY) {
      override_status |= RNA_OVERRIDE_STATUS_MANDATORY;
    }
    if (opop->flag & IDOVERRIDE_LIBRARY_FLAG_LOCKED) {
      override_status |= RNA_OVERRIDE_STATUS_LOCKED;
    }
  }

  return override_status;
}
