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

#include <CLG_log.h>

#include "MEM_guardedalloc.h"

#include "DNA_ID.h"
#include "DNA_anim_types.h"
#include "DNA_constraint_types.h"
#include "DNA_gpencil_modifier_types.h"
#include "DNA_key_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"

#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

//#define DEBUG_OVERRIDE_TIMEIT

#ifdef DEBUG_OVERRIDE_TIMEIT
#  include "PIL_time_utildefines.h"
#endif

#include "BKE_armature.h"
#include "BKE_idprop.h"
#include "BKE_idtype.h"
#include "BKE_lib_override.h"
#include "BKE_main.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "rna_access_internal.h"
#include "rna_internal.h"

static CLG_LogRef LOG = {"rna.access_compare_override"};

/**
 * Find the actual ID owner of the given \a ptr #PointerRNA, in override sense, and generate the
 * full rna path from it to given \a prop #PropertyRNA if \a rna_path is given.
 *
 * \note This is slightly different than 'generic' RNA 'id owner' as returned by
 * #RNA_find_real_ID_and_path, since in overrides we also consider shape keys as embedded data, not
 * only root node trees and master collections.
 */
static ID *rna_property_override_property_real_id_owner(Main *bmain,
                                                        PointerRNA *ptr,
                                                        PropertyRNA *prop,
                                                        char **r_rna_path)
{
  ID *id = ptr->owner_id;
  ID *owner_id = id;
  const char *rna_path_prefix = NULL;

  if (r_rna_path != NULL) {
    *r_rna_path = NULL;
  }

  if (id == NULL) {
    return NULL;
  }

  if (id->flag & (LIB_EMBEDDED_DATA | LIB_EMBEDDED_DATA_LIB_OVERRIDE)) {
    /* XXX this is very bad band-aid code, but for now it will do.
     * We should at least use a #define for those prop names.
     * Ideally RNA as a whole should be aware of those PITA of embedded IDs, and have a way to
     * retrieve their owner IDs and generate paths from those.
     */

    switch (GS(id->name)) {
      case ID_KE:
        owner_id = ((Key *)id)->from;
        rna_path_prefix = "shape_keys.";
        break;
      case ID_GR:
      case ID_NT:
        /* Master collections, Root node trees. */
        owner_id = RNA_find_real_ID_and_path(bmain, id, &rna_path_prefix);
        break;
      default:
        BLI_assert(0);
    }
  }

  if (r_rna_path == NULL) {
    return owner_id;
  }

  char *rna_path = RNA_path_from_ID_to_property(ptr, prop);
  if (rna_path) {
    *r_rna_path = rna_path;
    if (rna_path_prefix != NULL) {
      *r_rna_path = BLI_sprintfN("%s%s", rna_path_prefix, rna_path);
      MEM_freeN(rna_path);
    }

    return owner_id;
  }
  return NULL;
}

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
     * if we get more overridable collections,
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
    else if (RNA_struct_is_a(ptr->type, &RNA_GpencilModifier)) {
      GpencilModifierData *gp_mod = ptr->data;
      if (gp_mod->flag & eGpencilModifierFlag_OverrideLibrary_Local) {
        return true;
      }
    }
    else if (RNA_struct_is_a(ptr->type, &RNA_NlaTrack)) {
      NlaTrack *nla_track = ptr->data;
      if (nla_track->flag & NLATRACK_OVERRIDELIBRARY_LOCAL) {
        return true;
      }
    }
    /* If this is a RNA-defined property (real or 'virtual' IDProp),
     * we want to use RNA prop flag. */
    return !(prop->flag_override & PROPOVERRIDE_NO_COMPARISON) &&
           (prop->flag_override & PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  }
  /* If this is a real 'pure' IDProp (aka custom property), we want to use the IDProp flag. */
  IDProperty *idprop = (IDProperty *)prop;
  return (idprop->flag & IDP_FLAG_OVERRIDABLE_LIBRARY) != 0;
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

  if (rna_path == NULL || id == NULL || !ID_IS_OVERRIDE_LIBRARY(id)) {
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
                                                  PointerRNA *ptr_dst,
                                                  PointerRNA *ptr_src,
                                                  PointerRNA *ptr_storage,
                                                  PropertyRNA *prop_dst,
                                                  PropertyRNA *prop_src,
                                                  PropertyRNA *prop_storage,
                                                  PointerRNA *ptr_item_dst,
                                                  PointerRNA *ptr_item_src,
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
   * be it an actual RNA property, or an #IDProperty in disguise. */
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
                                      PropertyRNAOrID *prop_a,
                                      PropertyRNAOrID *prop_b,
                                      const char *rna_path,
                                      const size_t rna_path_len,
                                      eRNACompareMode mode,
                                      IDOverrideLibrary *override,
                                      const eRNAOverrideMatch flags,
                                      eRNAOverrideMatchResult *r_report_flags);

bool RNA_property_equals(
    Main *bmain, PointerRNA *ptr_a, PointerRNA *ptr_b, PropertyRNA *prop, eRNACompareMode mode)
{
  BLI_assert(ELEM(mode, RNA_EQ_STRICT, RNA_EQ_UNSET_MATCH_ANY, RNA_EQ_UNSET_MATCH_NONE));

  PropertyRNAOrID prop_a, prop_b;

  rna_property_rna_or_id_get(prop, ptr_a, &prop_a);
  rna_property_rna_or_id_get(prop, ptr_b, &prop_b);

  return (rna_property_override_diff(bmain, &prop_a, &prop_b, NULL, 0, mode, NULL, 0, NULL) == 0);
}

bool RNA_struct_equals(Main *bmain, PointerRNA *ptr_a, PointerRNA *ptr_b, eRNACompareMode mode)
{
  CollectionPropertyIterator iter;
  PropertyRNA *iterprop;
  bool equals = true;

  if (ptr_a == NULL && ptr_b == NULL) {
    return true;
  }
  if (ptr_a == NULL || ptr_b == NULL) {
    return false;
  }
  if (ptr_a->type != ptr_b->type) {
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
                                      PropertyRNAOrID *prop_a,
                                      PropertyRNAOrID *prop_b,
                                      const char *rna_path,
                                      const size_t rna_path_len,
                                      eRNACompareMode mode,
                                      IDOverrideLibrary *override,
                                      const eRNAOverrideMatch flags,
                                      eRNAOverrideMatchResult *r_report_flags)
{
  BLI_assert(!ELEM(NULL, prop_a, prop_b));

  if (prop_a->rnaprop->flag_override & PROPOVERRIDE_NO_COMPARISON ||
      prop_b->rnaprop->flag_override & PROPOVERRIDE_NO_COMPARISON) {
    return 0;
  }

  if (mode == RNA_EQ_UNSET_MATCH_ANY) {
    /* Unset properties are assumed to match anything. */
    if (!prop_a->is_set || !prop_b->is_set) {
      return 0;
    }
  }
  else if (mode == RNA_EQ_UNSET_MATCH_NONE) {
    /* Unset properties never match set properties. */
    if (prop_a->is_set != prop_b->is_set) {
      return 1;
    }
  }

  if (prop_a->is_idprop && ELEM(NULL, prop_a->idprop, prop_b->idprop)) {
    return (prop_a->idprop == prop_b->idprop) ? 0 : 1;
  }

  /* Check if we are working with arrays. */
  const bool is_array_a = prop_a->is_array;
  const bool is_array_b = prop_b->is_array;

  if (is_array_a != is_array_b) {
    /* Should probably never happen actually... */
    BLI_assert(0);
    return is_array_a ? 1 : -1;
  }

  /* Get the length of the array to work with. */
  const uint len_a = prop_a->array_len;
  const uint len_b = prop_b->array_len;

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
  if (prop_a->is_idprop) {
    override_diff = rna_property_override_diff_default;
    if (!prop_b->is_idprop && prop_b->rnaprop->override_diff != override_diff) {
      override_diff = NULL;
    }
  }
  else if (prop_b->is_idprop) {
    override_diff = rna_property_override_diff_default;
    if (prop_a->rnaprop->override_diff != override_diff) {
      override_diff = NULL;
    }
  }
  else if (prop_a->rnaprop->override_diff == prop_b->rnaprop->override_diff) {
    override_diff = prop_a->rnaprop->override_diff;
    if (override_diff == NULL) {
      override_diff = rna_property_override_diff_default;
    }
  }

  if (override_diff == NULL) {
    CLOG_ERROR(&LOG,
               "'%s' gives unmatching or NULL RNA diff callbacks, should not happen (%d vs. %d)",
               rna_path ? rna_path : prop_a->identifier,
               !prop_a->is_idprop,
               !prop_b->is_idprop);
    BLI_assert(0);
    return 1;
  }

  bool override_changed = false;
  eRNAOverrideMatch diff_flags = flags;
  if (!RNA_property_overridable_get(&prop_a->ptr, prop_a->rawprop)) {
    diff_flags &= ~RNA_OVERRIDE_COMPARE_CREATE;
  }
  const int diff = override_diff(bmain,
                                 prop_a,
                                 prop_b,
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
    if (override_store == NULL) {
      override_store = rna_property_override_store_default;
    }
  }

  if (ptr_storage != NULL && prop_storage->magic == RNA_MAGIC &&
      !ELEM(prop_storage->override_store, NULL, override_store)) {
    override_store = NULL;
  }

  if (override_store == NULL) {
    CLOG_ERROR(&LOG,
               "'%s' gives unmatching or NULL RNA store callbacks, should not happen (%d vs. %d)",
               op->rna_path,
               prop_local->magic == RNA_MAGIC,
               prop_reference->magic == RNA_MAGIC);
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
    if (prop_src->magic == RNA_MAGIC && !ELEM(prop_src->override_apply, NULL, override_apply)) {
      override_apply = NULL;
    }
  }
  else if (prop_src->magic != RNA_MAGIC) {
    override_apply = rna_property_override_apply_default;
    if (!ELEM(prop_dst->override_apply, NULL, override_apply)) {
      override_apply = NULL;
    }
  }
  else if (prop_dst->override_apply == prop_src->override_apply) {
    override_apply = prop_dst->override_apply;
    if (override_apply == NULL) {
      override_apply = rna_property_override_apply_default;
    }
  }

  if (ptr_storage && prop_storage->magic == RNA_MAGIC &&
      !ELEM(prop_storage->override_apply, NULL, override_apply)) {
    override_apply = NULL;
  }

  if (override_apply == NULL) {
    CLOG_ERROR(&LOG,
               "'%s' gives unmatching or NULL RNA apply callbacks, should not happen (%d vs. %d)",
               prop_dst->magic != RNA_MAGIC ? ((IDProperty *)prop_dst)->name :
                                              prop_dst->identifier,
               prop_dst->magic == RNA_MAGIC,
               prop_src->magic == RNA_MAGIC);
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

  if (ptr_local->owner_id == ptr_local->data && GS(ptr_local->owner_id->name) == ID_OB) {
    /* Our beloved pose's bone cross-data pointers. Usually, depsgraph evaluation would
     * ensure this is valid, but in some situations (like hidden collections etc.) this won't
     * be the case, so we need to take care of this ourselves.
     *
     * Note: Typically callers of this function (from BKE_lib_override area) will already have
     * ensured this. However, studio is still reporting sporadic, unreproducible crashes due to
     * invalid pose data, so think there are still some cases where some armatures are somehow
     * missing updates (possibly due to dependencies?). Since calling this function on same ID
     * several time is almost free, and safe even in a threaded context as long as it has been done
     * at least once first outside of threaded processing, we do it another time here. */
    Object *ob_local = (Object *)ptr_local->owner_id;
    if (ob_local->type == OB_ARMATURE) {
      Object *ob_reference = (Object *)ptr_local->owner_id->override_library->reference;
      BLI_assert(ob_local->data != NULL);
      BLI_assert(ob_reference->data != NULL);
      BKE_pose_ensure(bmain, ob_local, ob_local->data, true);
      BKE_pose_ensure(bmain, ob_reference, ob_reference->data, true);
    }
  }

  iterprop = RNA_struct_iterator_property(ptr_local->type);

  for (RNA_property_collection_begin(ptr_local, iterprop, &iter); iter.valid;
       RNA_property_collection_next(&iter)) {
    PropertyRNA *rawprop = iter.ptr.data;

    PropertyRNAOrID prop_local;
    PropertyRNAOrID prop_reference;

    rna_property_rna_or_id_get(rawprop, ptr_local, &prop_local);
    rna_property_rna_or_id_get(rawprop, ptr_reference, &prop_reference);

    BLI_assert(prop_local.rnaprop != NULL);
    BLI_assert(prop_local.rnaprop == prop_reference.rnaprop);
    BLI_assert(prop_local.is_idprop == prop_reference.is_idprop);

    if ((prop_local.is_idprop && prop_local.idprop == NULL) ||
        (prop_reference.is_idprop && prop_reference.idprop == NULL)) {
      continue;
    }

    if (ignore_non_overridable && !RNA_property_overridable_get(&prop_local.ptr, rawprop)) {
      continue;
    }

    if (!prop_local.is_idprop &&
        RNA_property_override_flag(prop_local.rnaprop) & PROPOVERRIDE_IGNORE) {
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

      const char *prop_name = prop_local.identifier;
      const size_t prop_name_len = strlen(prop_name);

      /* Inlined building, much much more efficient. */
      if (!prop_local.is_idprop) {
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
      rna_path = RNA_path_from_ID_to_property(ptr_local, rawprop);
      if (rna_path != NULL) {
        rna_path_len = strlen(rna_path);
      }
    }
    if (rna_path == NULL) {
      continue;
    }

    CLOG_INFO(&LOG, 5, "Override Checking %s\n", rna_path);

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
                                                &prop_local,
                                                &prop_reference,
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
          if (RNA_property_editable(ptr_local, rawprop)) {
            IDOverrideLibraryPropertyOperation opop_tmp = {
                .operation = IDOVERRIDE_LIBRARY_OP_REPLACE,
                .subitem_reference_index = -1,
                .subitem_local_index = -1,
            };
            rna_property_override_operation_apply(bmain,
                                                  ptr_local,
                                                  ptr_reference,
                                                  NULL,
                                                  rawprop,
                                                  rawprop,
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
        CLOG_INFO(&LOG, 5, "Skipping insert override operations in first pass (%s)", op->rna_path);
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
        if (opop->subitem_reference_name != NULL &&
            RNA_property_collection_lookup_string(
                ptr_dst, prop_dst, opop->subitem_reference_name, &private_ptr_item_dst)) {
          /* This is rather fragile, but the fact that local override IDs may have a different name
           * than their linked reference makes it necessary.
           * Basically, here we are considering that if we cannot find the original linked ID in
           * the local override we are (re-)applying the operations, then it may be because some of
           * those operations have already been applied, and we may already have the local ID
           * pointer we want to set.
           * This happens e.g. during re-sync of an override, since we have already remapped all ID
           * pointers to their expected values.
           * In that case we simply try to get the property from the local expected name. */
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

      if (ptr_item_dst->type == NULL) {
        CLOG_INFO(
            &LOG,
            2,
            "Failed to find destination sub-item '%s' (%d) of '%s' in new override data '%s'",
            opop->subitem_reference_name,
            opop->subitem_reference_index,
            op->rna_path,
            ptr_dst->owner_id->name);
      }
      if (ptr_item_src->type == NULL) {
        CLOG_INFO(&LOG,
                  2,
                  "Failed to find source sub-item '%s' (%d) of '%s' in old override data '%s'",
                  opop->subitem_local_name,
                  opop->subitem_local_index,
                  op->rna_path,
                  ptr_src->owner_id->name);
      }
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
      CLOG_INFO(&LOG,
                2,
                "Failed to apply '%s' override operation on %s\n",
                op->rna_path,
                ptr_src->owner_id->name);
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
                               IDOverrideLibrary *override,
                               const eRNAOverrideApplyFlag flag)
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

        /* Check if an overridden ID pointer supposed to be in sync with linked data gets out of
         * sync. */
        if ((ptr_dst->owner_id->tag & LIB_TAG_LIB_OVERRIDE_NEED_RESYNC) == 0 &&
            op->rna_prop_type == PROP_POINTER &&
            (((IDOverrideLibraryPropertyOperation *)op->operations.first)->flag &
             IDOVERRIDE_LIBRARY_FLAG_IDPOINTER_MATCH_REFERENCE) != 0) {
          BLI_assert(ptr_src->owner_id ==
                     rna_property_override_property_real_id_owner(bmain, &data_src, NULL, NULL));
          BLI_assert(ptr_dst->owner_id ==
                     rna_property_override_property_real_id_owner(bmain, &data_dst, NULL, NULL));

          PointerRNA prop_ptr_src = RNA_property_pointer_get(&data_src, prop_src);
          PointerRNA prop_ptr_dst = RNA_property_pointer_get(&data_dst, prop_dst);
          ID *id_src = rna_property_override_property_real_id_owner(
              bmain, &prop_ptr_src, NULL, NULL);
          ID *id_dst = rna_property_override_property_real_id_owner(
              bmain, &prop_ptr_dst, NULL, NULL);

          BLI_assert(id_src == NULL || ID_IS_OVERRIDE_LIBRARY_REAL(id_src));

          if (/* We might be in a case where id_dst has already been processed and its usages
               * remapped to its new local override. In that case overrides and linked data are
               * always properly matching. */
              id_src != id_dst &&
              /* If one of the pointers is NULL and not the other, or if linked reference ID of
               * `id_src` is not `id_dst`,  we are in a non-matching case. */
              (ELEM(NULL, id_src, id_dst) || id_src->override_library->reference != id_dst)) {
            ptr_dst->owner_id->tag |= LIB_TAG_LIB_OVERRIDE_NEED_RESYNC;
            CLOG_INFO(
                &LOG, 3, "Local override %s detected as needing resync", ptr_dst->owner_id->name);
          }
        }

        /* Workaround for older broken overrides, we then assume that non-matching ID pointers
         * override operations that replace a non-NULL value are 'mistakes', and ignore (do not
         * apply) them. */
        if ((flag & RNA_OVERRIDE_APPLY_FLAG_IGNORE_ID_POINTERS) != 0 &&
            op->rna_prop_type == PROP_POINTER &&
            (((IDOverrideLibraryPropertyOperation *)op->operations.first)->flag &
             IDOVERRIDE_LIBRARY_FLAG_IDPOINTER_MATCH_REFERENCE) == 0) {
          BLI_assert(ptr_src->owner_id ==
                     rna_property_override_property_real_id_owner(bmain, &data_src, NULL, NULL));
          BLI_assert(ptr_dst->owner_id ==
                     rna_property_override_property_real_id_owner(bmain, &data_dst, NULL, NULL));

          PointerRNA prop_ptr_dst = RNA_property_pointer_get(&data_dst, prop_dst);
          if (prop_ptr_dst.type != NULL && RNA_struct_is_ID(prop_ptr_dst.type)) {
#ifndef NDEBUG
            PointerRNA prop_ptr_src = RNA_property_pointer_get(&data_src, prop_src);
            BLI_assert(prop_ptr_src.type == NULL || RNA_struct_is_ID(prop_ptr_src.type));
#endif
            ID *id_dst = rna_property_override_property_real_id_owner(
                bmain, &prop_ptr_dst, NULL, NULL);

            if (id_dst != NULL) {
              CLOG_INFO(&LOG,
                        3,
                        "%s: Ignoring local override on ID pointer property '%s', as requested by "
                        "RNA_OVERRIDE_APPLY_FLAG_IGNORE_ID_POINTERS flag",
                        ptr_dst->owner_id->name,
                        op->rna_path);
              continue;
            }
          }
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
      else {
        CLOG_INFO(&LOG,
                  2,
                  "Failed to apply library override operation to '%s.%s' "
                  "(could not resolve some properties, local:  %d, override: %d)",
                  ((ID *)ptr_src->owner_id)->name,
                  op->rna_path,
                  RNA_path_resolve_property(ptr_dst, op->rna_path, &data_dst, &prop_dst),
                  RNA_path_resolve_property(ptr_src, op->rna_path, &data_src, &prop_src));
      }
    }
  }

  /* Some cases (like point caches) may require additional post-processing. */
  if (RNA_struct_is_a(ptr_dst->type, &RNA_ID)) {
    ID *id_dst = ptr_dst->data;
    ID *id_src = ptr_src->data;
    const IDTypeInfo *id_type = BKE_idtype_get_info_from_id(id_dst);
    if (id_type->lib_override_apply_post != NULL) {
      id_type->lib_override_apply_post(id_dst, id_src);
    }
  }

#ifdef DEBUG_OVERRIDE_TIMEIT
  TIMEIT_END_AVERAGED(RNA_struct_override_apply);
#endif
}

IDOverrideLibraryProperty *RNA_property_override_property_find(Main *bmain,
                                                               PointerRNA *ptr,
                                                               PropertyRNA *prop,
                                                               ID **r_owner_id)
{
  char *rna_path;

  *r_owner_id = rna_property_override_property_real_id_owner(bmain, ptr, prop, &rna_path);
  if (rna_path != NULL) {
    IDOverrideLibraryProperty *op = BKE_lib_override_library_property_find(
        (*r_owner_id)->override_library, rna_path);
    MEM_freeN(rna_path);
    return op;
  }
  return NULL;
}

IDOverrideLibraryProperty *RNA_property_override_property_get(Main *bmain,
                                                              PointerRNA *ptr,
                                                              PropertyRNA *prop,
                                                              bool *r_created)
{
  char *rna_path;

  if (r_created != NULL) {
    *r_created = false;
  }

  ID *id = rna_property_override_property_real_id_owner(bmain, ptr, prop, &rna_path);
  if (rna_path != NULL) {
    IDOverrideLibraryProperty *op = BKE_lib_override_library_property_get(
        id->override_library, rna_path, r_created);
    MEM_freeN(rna_path);
    return op;
  }
  return NULL;
}

IDOverrideLibraryPropertyOperation *RNA_property_override_property_operation_find(
    Main *bmain,
    PointerRNA *ptr,
    PropertyRNA *prop,
    const int index,
    const bool strict,
    bool *r_strict)
{
  ID *owner_id;
  IDOverrideLibraryProperty *op = RNA_property_override_property_find(bmain, ptr, prop, &owner_id);

  if (!op) {
    return NULL;
  }

  return BKE_lib_override_library_property_operation_find(
      op, NULL, NULL, index, index, strict, r_strict);
}

IDOverrideLibraryPropertyOperation *RNA_property_override_property_operation_get(
    Main *bmain,
    PointerRNA *ptr,
    PropertyRNA *prop,
    const short operation,
    const int index,
    const bool strict,
    bool *r_strict,
    bool *r_created)
{
  if (r_created != NULL) {
    *r_created = false;
  }

  IDOverrideLibraryProperty *op = RNA_property_override_property_get(bmain, ptr, prop, NULL);

  if (!op) {
    return NULL;
  }

  return BKE_lib_override_library_property_operation_get(
      op, operation, NULL, NULL, index, index, strict, r_strict, r_created);
}

eRNAOverrideStatus RNA_property_override_library_status(Main *bmain,
                                                        PointerRNA *ptr,
                                                        PropertyRNA *prop,
                                                        const int index)
{
  uint override_status = 0;

  if (!ptr || !prop || !ptr->owner_id || !ID_IS_OVERRIDE_LIBRARY(ptr->owner_id)) {
    return override_status;
  }

  if (RNA_property_overridable_get(ptr, prop) && RNA_property_editable_flag(ptr, prop)) {
    override_status |= RNA_OVERRIDE_STATUS_OVERRIDABLE;
  }

  IDOverrideLibraryPropertyOperation *opop = RNA_property_override_property_operation_find(
      bmain, ptr, prop, index, false, NULL);
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
