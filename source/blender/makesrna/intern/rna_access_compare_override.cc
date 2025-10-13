/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include <cstring>
#include <fmt/format.h>
#include <optional>

#include <CLG_log.h>

#include "MEM_guardedalloc.h"

#include "DNA_ID.h"
#include "DNA_anim_types.h"
#include "DNA_camera_types.h"
#include "DNA_constraint_types.h"
#include "DNA_key_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"

#include "BLI_listbase.h"
#include "BLI_utildefines.h"

// #define DEBUG_OVERRIDE_TIMEIT

#ifdef DEBUG_OVERRIDE_TIMEIT
#  include "BLI_time_utildefines.h"
#  include <stdio.h>
#endif

#include "BKE_armature.hh"
#include "BKE_idprop.hh"
#include "BKE_idtype.hh"
#include "BKE_lib_override.hh"
#include "BKE_library.hh"
#include "BKE_main.hh"

#include "RNA_access.hh"
#include "RNA_enum_types.hh"
#include "RNA_path.hh"
#include "RNA_prototypes.hh"

#include "rna_access_internal.hh"
#include "rna_internal.hh"

static CLG_LogRef LOG = {"rna.access_compare_override"};

/**
 * Find the actual ID owner of the given \a ptr #PointerRNA, in override sense, and generate the
 * full rna path from it to given \a prop #PropertyRNA if \a rna_path is given.
 *
 * \note This is slightly different than 'generic' RNA 'id owner' as returned by
 * #RNA_find_real_ID_and_path, since in overrides we also consider shape keys as embedded data, not
 * only root node trees and master collections.
 */
static ID *rna_property_override_property_real_id_owner(Main * /*bmain*/,
                                                        PointerRNA *ptr,
                                                        PropertyRNA *prop,
                                                        std::optional<std::string> *r_rna_path)
{
  ID *id = ptr->owner_id;
  ID *owner_id = id;
  const char *rna_path_prefix = nullptr;

  if (r_rna_path != nullptr) {
    *r_rna_path = std::nullopt;
  }

  if (id == nullptr) {
    return nullptr;
  }

  if (id->flag & (ID_FLAG_EMBEDDED_DATA | ID_FLAG_EMBEDDED_DATA_LIB_OVERRIDE)) {
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
        owner_id = RNA_find_real_ID_and_path(id, &rna_path_prefix);
        break;
      default:
        BLI_assert_unreachable();
    }
  }

  if (r_rna_path == nullptr) {
    return owner_id;
  }

  if (std::optional<std::string> rna_path = RNA_path_from_ID_to_property(ptr, prop)) {
    if (rna_path_prefix) {
      r_rna_path->emplace(fmt::format("{}{}", rna_path_prefix, *rna_path));
    }
    else {
      r_rna_path->emplace(std::move(*rna_path));
    }

    return owner_id;
  }
  return nullptr;
}

int RNA_property_override_flag(PropertyRNA *prop)
{
  return rna_ensure_property(prop)->flag_override;
}

bool RNA_property_overridable_get(const PointerRNA *ptr, PropertyRNA *prop)
{
  if (prop->magic == RNA_MAGIC) {
    /* Special handling for insertions of constraints or modifiers... */
    /* TODO: Note We may want to add a more generic system to RNA
     * (like a special property in struct of items)
     * if we get more overridable collections,
     * for now we can live with those special-cases handling I think. */
    if (RNA_struct_is_a(ptr->type, &RNA_Constraint)) {
      bConstraint *con = static_cast<bConstraint *>(ptr->data);
      if (con->flag & CONSTRAINT_OVERRIDE_LIBRARY_LOCAL) {
        return true;
      }
    }
    else if (RNA_struct_is_a(ptr->type, &RNA_Modifier)) {
      ModifierData *mod = static_cast<ModifierData *>(ptr->data);
      if (mod->flag & eModifierFlag_OverrideLibrary_Local) {
        return true;
      }
    }
    else if (RNA_struct_is_a(ptr->type, &RNA_NlaTrack)) {
      NlaTrack *nla_track = static_cast<NlaTrack *>(ptr->data);
      if (nla_track->flag & NLATRACK_OVERRIDELIBRARY_LOCAL) {
        return true;
      }
    }
    else if (RNA_struct_is_a(ptr->type, &RNA_CameraBackgroundImage)) {
      CameraBGImage *bgpic = static_cast<CameraBGImage *>(ptr->data);
      if (bgpic->flag & CAM_BGIMG_FLAG_OVERRIDE_LIBRARY_LOCAL) {
        return true;
      }
    }
    else if (RNA_struct_is_a(ptr->type, &RNA_BoneCollection)) {
      BoneCollection *bcoll = static_cast<BoneCollection *>(ptr->data);
      if (bcoll->flags & BONE_COLLECTION_OVERRIDE_LIBRARY_LOCAL) {
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

bool RNA_property_overridable_library_set(PointerRNA * /*ptr*/,
                                          PropertyRNA *prop,
                                          const bool is_overridable)
{
  /* Only works for pure custom properties IDProps. */
  if (prop->magic != RNA_MAGIC) {
    IDProperty *idprop = (IDProperty *)prop;
    constexpr short flags = (IDP_FLAG_OVERRIDABLE_LIBRARY | IDP_FLAG_STATIC_TYPE);
    idprop->flag = is_overridable ? (idprop->flag | flags) : (idprop->flag & ~flags);
    return true;
  }

  return false;
}

bool RNA_property_overridden(PointerRNA *ptr, PropertyRNA *prop)
{
  const std::optional<std::string> rna_path = RNA_path_from_ID_to_property(ptr, prop);
  ID *id = ptr->owner_id;

  if (!rna_path || id == nullptr || !ID_IS_OVERRIDE_LIBRARY(id)) {
    return false;
  }

  return (BKE_lib_override_library_property_find(id->override_library, rna_path->c_str()) !=
          nullptr);
}

bool RNA_property_comparable(PointerRNA * /*ptr*/, PropertyRNA *prop)
{
  prop = rna_ensure_property(prop);

  return !(prop->flag_override & PROPOVERRIDE_NO_COMPARISON);
}

static bool rna_property_override_operation_apply(Main *bmain,
                                                  RNAPropertyOverrideApplyContext &rnaapply_ctx);

bool RNA_property_copy(
    Main *bmain, PointerRNA *ptr, PointerRNA *fromptr, PropertyRNA *prop, int index)
{
  if (!RNA_property_editable(ptr, prop)) {
    return false;
  }

  IDOverrideLibraryPropertyOperation opop{};
  opop.operation = LIBOVERRIDE_OP_REPLACE;
  opop.subitem_reference_index = index;
  opop.subitem_local_index = index;

  RNAPropertyOverrideApplyContext rnaapply_ctx;
  rnaapply_ctx.ptr_dst = *ptr;
  rnaapply_ctx.ptr_src = *fromptr;
  rnaapply_ctx.prop_dst = prop;
  rnaapply_ctx.prop_src = prop;
  rnaapply_ctx.liboverride_operation = &opop;

  return rna_property_override_operation_apply(bmain, rnaapply_ctx);
}

static int rna_property_override_diff(Main *bmain,
                                      PropertyRNAOrID *prop_a,
                                      PropertyRNAOrID *prop_b,
                                      const char *rna_path,
                                      const size_t rna_path_len,
                                      eRNACompareMode mode,
                                      IDOverrideLibrary *liboverride,
                                      const eRNAOverrideMatch flags,
                                      eRNAOverrideMatchResult *r_report_flags);

bool RNA_property_equals(
    Main *bmain, PointerRNA *ptr_a, PointerRNA *ptr_b, PropertyRNA *prop, eRNACompareMode mode)
{
  BLI_assert(ELEM(mode, RNA_EQ_STRICT, RNA_EQ_UNSET_MATCH_ANY, RNA_EQ_UNSET_MATCH_NONE));

  PropertyRNAOrID prop_a, prop_b;

  rna_property_rna_or_id_get(prop, ptr_a, &prop_a);
  rna_property_rna_or_id_get(prop, ptr_b, &prop_b);

  return (rna_property_override_diff(
              bmain, &prop_a, &prop_b, nullptr, 0, mode, nullptr, eRNAOverrideMatch(0), nullptr) ==
          0);
}

bool RNA_struct_equals(Main *bmain, PointerRNA *ptr_a, PointerRNA *ptr_b, eRNACompareMode mode)
{
  CollectionPropertyIterator iter;
  PropertyRNA *iterprop;
  bool equals = true;

  if (ptr_a == nullptr && ptr_b == nullptr) {
    return true;
  }
  if (ptr_a == nullptr || ptr_b == nullptr) {
    return false;
  }
  if (ptr_a->type != ptr_b->type) {
    return false;
  }

  if (RNA_pointer_is_null(ptr_a)) {
    if (RNA_pointer_is_null(ptr_b)) {
      return true;
    }
    return false;
  }

  iterprop = RNA_struct_iterator_property(ptr_a->type);

  RNA_property_collection_begin(ptr_a, iterprop, &iter);
  for (; iter.valid; RNA_property_collection_next(&iter)) {
    PropertyRNA *prop = static_cast<PropertyRNA *>(iter.ptr.data);

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
 * Return value follows comparison functions convention (`0` is equal, `-1` if `prop_a` value is
 * lesser than `prop_b` one, and `1` otherwise.
 *
 * \note When there is no equality, but no order can be determined (greater than/lesser than),
 *       1 is returned.
 */
static int rna_property_override_diff(Main *bmain,
                                      PropertyRNAOrID *prop_a,
                                      PropertyRNAOrID *prop_b,
                                      const char *rna_path,
                                      const size_t rna_path_len,
                                      eRNACompareMode mode,
                                      IDOverrideLibrary *liboverride,
                                      const eRNAOverrideMatch flags,
                                      eRNAOverrideMatchResult *r_report_flags)
{
  BLI_assert(!ELEM(nullptr, prop_a, prop_b));

  if (prop_a->rnaprop->flag_override & PROPOVERRIDE_NO_COMPARISON ||
      prop_b->rnaprop->flag_override & PROPOVERRIDE_NO_COMPARISON)
  {
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

  if (prop_a->is_idprop && ELEM(nullptr, prop_a->idprop, prop_b->idprop)) {
    return (prop_a->idprop == prop_b->idprop) ? 0 : 1;
  }

  /* Check if we are working with arrays. */
  const bool is_array_a = prop_a->is_array;
  const bool is_array_b = prop_b->is_array;

  if (is_array_a != is_array_b) {
    /* Should probably never happen actually... */
    BLI_assert_unreachable();
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

  RNAPropOverrideDiff override_diff = nullptr;
  /* Special case for IDProps, we use default callback then. */
  if (prop_a->is_idprop) {
    override_diff = rna_property_override_diff_default;
    if (!prop_b->is_idprop && prop_b->rnaprop->override_diff != override_diff) {
      override_diff = nullptr;
    }
  }
  else if (prop_b->is_idprop) {
    override_diff = rna_property_override_diff_default;
    if (prop_a->rnaprop->override_diff != override_diff) {
      override_diff = nullptr;
    }
  }
  else if (prop_a->rnaprop->override_diff == prop_b->rnaprop->override_diff) {
    override_diff = prop_a->rnaprop->override_diff;
    if (override_diff == nullptr) {
      override_diff = rna_property_override_diff_default;
    }
  }

  if (override_diff == nullptr) {
    CLOG_ERROR(
        &LOG,
        "'%s' gives unmatching or nullptr RNA diff callbacks, should not happen (%d vs. %d)",
        rna_path ? rna_path : prop_a->identifier,
        !prop_a->is_idprop,
        !prop_b->is_idprop);
    BLI_assert_unreachable();
    return 1;
  }

  eRNAOverrideMatch diff_flags = flags;
  if (!RNA_property_overridable_get(prop_a->ptr, prop_a->rawprop) ||
      (!ELEM(RNA_property_type(prop_a->rawprop), PROP_POINTER, PROP_COLLECTION) &&
       !RNA_property_editable_flag(prop_a->ptr, prop_a->rawprop)))
  {
    diff_flags &= ~RNA_OVERRIDE_COMPARE_CREATE;
  }

  RNAPropertyOverrideDiffContext rnadiff_ctx;
  rnadiff_ctx.prop_a = prop_a;
  rnadiff_ctx.prop_b = prop_b;
  rnadiff_ctx.mode = mode;

  rnadiff_ctx.liboverride = liboverride;
  rnadiff_ctx.rna_path = rna_path;
  rnadiff_ctx.rna_path_len = rna_path_len;
  rnadiff_ctx.liboverride_flags = diff_flags;
  override_diff(bmain, rnadiff_ctx);

  if (r_report_flags) {
    *r_report_flags = rnadiff_ctx.report_flag;
  }
  return rnadiff_ctx.comparison;
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

  if (ptr_storage == nullptr) {
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

  RNAPropOverrideStore override_store = nullptr;
  /* Special case for IDProps, we use default callback then. */
  if (prop_local->magic != RNA_MAGIC) {
    override_store = rna_property_override_store_default;
    if (prop_reference->magic == RNA_MAGIC && prop_reference->override_store != override_store) {
      override_store = nullptr;
    }
  }
  else if (prop_reference->magic != RNA_MAGIC) {
    override_store = rna_property_override_store_default;
    if (prop_local->override_store != override_store) {
      override_store = nullptr;
    }
  }
  else if (prop_local->override_store == prop_reference->override_store) {
    override_store = prop_local->override_store;
    if (override_store == nullptr) {
      override_store = rna_property_override_store_default;
    }
  }

  if ((prop_storage->magic == RNA_MAGIC) &&
      !ELEM(prop_storage->override_store, nullptr, override_store))
  {
    override_store = nullptr;
  }

  if (override_store == nullptr) {
    CLOG_ERROR(
        &LOG,
        "'%s' gives unmatching or nullptr RNA store callbacks, should not happen (%d vs. %d)",
        op->rna_path,
        prop_local->magic == RNA_MAGIC,
        prop_reference->magic == RNA_MAGIC);
    BLI_assert_unreachable();
    return changed;
  }

  LISTBASE_FOREACH (IDOverrideLibraryPropertyOperation *, opop, &op->operations) {
    /* Only needed for diff operations. */
    if (!ELEM(
            opop->operation, LIBOVERRIDE_OP_ADD, LIBOVERRIDE_OP_SUBTRACT, LIBOVERRIDE_OP_MULTIPLY))
    {
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
                       opop))
    {
      changed = true;
    }
  }

  return changed;
}

static bool rna_property_override_operation_apply(Main *bmain,
                                                  RNAPropertyOverrideApplyContext &rnaapply_ctx)
{
  PointerRNA *ptr_dst = &rnaapply_ctx.ptr_dst;
  PointerRNA *ptr_src = &rnaapply_ctx.ptr_src;
  PointerRNA *ptr_storage = &rnaapply_ctx.ptr_storage;
  PropertyRNA *prop_dst = rnaapply_ctx.prop_dst;
  PropertyRNA *prop_src = rnaapply_ctx.prop_src;
  PropertyRNA *prop_storage = rnaapply_ctx.prop_storage;
  IDOverrideLibraryPropertyOperation *opop = rnaapply_ctx.liboverride_operation;

  const short override_op = opop->operation;

  if (!BKE_lib_override_library_property_operation_operands_validate(
          opop, ptr_dst, ptr_src, ptr_storage, prop_dst, prop_src, prop_storage))
  {
    return false;
  }

  if (override_op == LIBOVERRIDE_OP_NOOP) {
    return true;
  }

  RNAPropOverrideApply override_apply = nullptr;
  /* Special case for IDProps, we use default callback then. */
  if (prop_dst->magic != RNA_MAGIC) {
    override_apply = rna_property_override_apply_default;
    if (prop_src->magic == RNA_MAGIC && !ELEM(prop_src->override_apply, nullptr, override_apply)) {
      override_apply = nullptr;
    }
  }
  else if (prop_src->magic != RNA_MAGIC) {
    override_apply = rna_property_override_apply_default;
    if (!ELEM(prop_dst->override_apply, nullptr, override_apply)) {
      override_apply = nullptr;
    }
  }
  else if (prop_dst->override_apply == prop_src->override_apply) {
    override_apply = prop_dst->override_apply;
    if (override_apply == nullptr) {
      override_apply = rna_property_override_apply_default;
    }
  }

  if (prop_storage && prop_storage->magic == RNA_MAGIC &&
      !ELEM(prop_storage->override_apply, nullptr, override_apply))
  {
    override_apply = nullptr;
  }

  if (override_apply == nullptr) {
    CLOG_ERROR(
        &LOG,
        "'%s' gives unmatching or nullptr RNA apply callbacks, should not happen (%d vs. %d)",
        prop_dst->magic != RNA_MAGIC ? ((IDProperty *)prop_dst)->name : prop_dst->identifier,
        prop_dst->magic == RNA_MAGIC,
        prop_src->magic == RNA_MAGIC);
    BLI_assert_unreachable();
    return false;
  }

  /* get the length of the array to work with */
  rnaapply_ctx.len_dst = RNA_property_array_length(ptr_dst, prop_dst);
  rnaapply_ctx.len_src = RNA_property_array_length(ptr_src, prop_src);
  if (prop_storage) {
    rnaapply_ctx.len_storage = RNA_property_array_length(ptr_storage, prop_storage);
  }

  if (rnaapply_ctx.len_dst != rnaapply_ctx.len_src ||
      (prop_storage && rnaapply_ctx.len_dst != rnaapply_ctx.len_storage))
  {
    /* Do not handle override in that case,
     * we do not support insertion/deletion from arrays for now. */
    return false;
  }

  /* get and set the default values as appropriate for the various types */
  const bool success = override_apply(bmain, rnaapply_ctx);
  return success;
}

bool RNA_struct_override_matches(Main *bmain,
                                 PointerRNA *ptr_local,
                                 PointerRNA *ptr_reference,
                                 const char *root_path,
                                 const size_t root_path_len,
                                 IDOverrideLibrary *liboverride,
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
  const bool do_tag_for_restore = (flags & RNA_OVERRIDE_COMPARE_TAG_FOR_RESTORE) != 0;

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
    _timeit_time_global = BLI_time_now_seconds();
  }
#endif

  if (ptr_local->owner_id == ptr_local->data && GS(ptr_local->owner_id->name) == ID_OB) {
    /* Our beloved pose's bone cross-data pointers. Usually, depsgraph evaluation would
     * ensure this is valid, but in some situations (like hidden collections etc.) this won't
     * be the case, so we need to take care of this ourselves.
     *
     * NOTE: Typically callers of this function (from BKE_lib_override area) will already have
     * ensured this. However, studio is still reporting sporadic, unreproducible crashes due to
     * invalid pose data, so think there are still some cases where some armatures are somehow
     * missing updates (possibly due to dependencies?). Since calling this function on same ID
     * several time is almost free, and safe even in a threaded context as long as it has been done
     * at least once first outside of threaded processing, we do it another time here. */
    Object *ob_local = (Object *)ptr_local->owner_id;
    if (ob_local->type == OB_ARMATURE) {
      Object *ob_reference = (Object *)ptr_local->owner_id->override_library->reference;
      BLI_assert(ob_local->data != nullptr);
      BLI_assert(ob_reference->data != nullptr);
      BKE_pose_ensure(bmain, ob_local, static_cast<bArmature *>(ob_local->data), true);
      BKE_pose_ensure(bmain, ob_reference, static_cast<bArmature *>(ob_reference->data), true);
    }
  }

  iterprop = RNA_struct_iterator_property(ptr_local->type);

  for (RNA_property_collection_begin(ptr_local, iterprop, &iter); iter.valid;
       RNA_property_collection_next(&iter))
  {
    PropertyRNA *rawprop = static_cast<PropertyRNA *>(iter.ptr.data);

    PropertyRNAOrID prop_local;
    PropertyRNAOrID prop_reference;

    rna_property_rna_or_id_get(rawprop, ptr_local, &prop_local);
    rna_property_rna_or_id_get(rawprop, ptr_reference, &prop_reference);

    BLI_assert(prop_local.rnaprop != nullptr);
    BLI_assert(prop_local.rnaprop == prop_reference.rnaprop);
    BLI_assert(prop_local.is_idprop == prop_reference.is_idprop);

    if ((prop_local.is_idprop && prop_local.idprop == nullptr) ||
        (prop_reference.is_idprop && prop_reference.idprop == nullptr))
    {
      continue;
    }

    if (ignore_non_overridable && !RNA_property_overridable_get(prop_local.ptr, rawprop)) {
      continue;
    }

    if (!prop_local.is_idprop &&
        RNA_property_override_flag(prop_local.rnaprop) & PROPOVERRIDE_IGNORE)
    {
      continue;
    }

#if 0
    /* This actually makes things slower, since it has to check for animation paths etc! */
    if (RNA_property_animated(ptr_local, prop_local)) {
      /* We cannot do anything here really, animation is some kind of dynamic overrides that has
       * precedence over static one... */
      continue;
    }
#endif

#define RNA_PATH_BUFFSIZE 8192

    std::optional<std::string> rna_path;
    size_t rna_path_len = 0;

    /* XXX TODO: this will have to be refined to handle collections insertions, and array items. */
    if (root_path) {
      BLI_assert(strlen(root_path) == root_path_len);

      const char *prop_name = prop_local.identifier;
      const size_t prop_name_len = strlen(prop_name);

      char rna_path_buffer[RNA_PATH_BUFFSIZE];
      char *rna_path_c = rna_path_buffer;

      /* Inlined building (significantly more efficient). */
      if (!prop_local.is_idprop) {
        rna_path_len = root_path_len + 1 + prop_name_len;
        if (rna_path_len >= RNA_PATH_BUFFSIZE) {
          rna_path = MEM_malloc_arrayN<char>(rna_path_len + 1, __func__);
        }

        memcpy(rna_path_c, root_path, root_path_len);
        rna_path_c[root_path_len] = '.';
        memcpy(rna_path_c + root_path_len + 1, prop_name, prop_name_len);
        rna_path_c[rna_path_len] = '\0';
      }
      else {
        rna_path_len = root_path_len + 2 + prop_name_len + 2;
        if (rna_path_len >= RNA_PATH_BUFFSIZE) {
          rna_path_c = MEM_malloc_arrayN<char>(rna_path_len + 1, __func__);
        }

        memcpy(rna_path_c, root_path, root_path_len);
        rna_path_c[root_path_len] = '[';
        rna_path_c[root_path_len + 1] = '"';
        memcpy(rna_path_c + root_path_len + 2, prop_name, prop_name_len);
        rna_path_c[root_path_len + 2 + prop_name_len] = '"';
        rna_path_c[root_path_len + 2 + prop_name_len + 1] = ']';
        rna_path_c[rna_path_len] = '\0';
      }

      rna_path.emplace(rna_path_c);
    }
    else {
      /* This is rather slow, but is not much called, so not really worth optimizing. */
      rna_path = RNA_path_from_ID_to_property(ptr_local, rawprop);
      if (rna_path) {
        rna_path_len = rna_path->size();
      }
    }
    if (!rna_path) {
      continue;
    }

    CLOG_DEBUG(&LOG, "Override Checking %s", rna_path->c_str());

    if (ignore_overridden) {
      IDOverrideLibraryProperty *op = BKE_lib_override_library_property_find(liboverride,
                                                                             rna_path->c_str());
      if (op != nullptr) {
        BKE_lib_override_library_operations_tag(op, LIBOVERRIDE_PROP_OP_TAG_UNUSED, false);
        continue;
      }
    }

#ifdef DEBUG_OVERRIDE_TIMEIT
    if (!root_path) {
      _timeit_time_diffing = BLI_time_now_seconds();
    }
#endif

    eRNAOverrideMatchResult report_flags = eRNAOverrideMatchResult(0);
    const int diff = rna_property_override_diff(bmain,
                                                &prop_local,
                                                &prop_reference,
                                                rna_path->c_str(),
                                                rna_path_len,
                                                RNA_EQ_STRICT,
                                                liboverride,
                                                flags,
                                                &report_flags);

#ifdef DEBUG_OVERRIDE_TIMEIT
    if (!root_path) {
      const float _delta_time = float(BLI_time_now_seconds() - _timeit_time_diffing);
      _delta_time_diffing += _delta_time;
      _num_delta_time_diffing++;
    }
#endif

    matching = matching && diff == 0;
    if (r_report_flags) {
      *r_report_flags = (*r_report_flags | report_flags);
    }

    if (diff != 0) {
      /* XXX TODO: refine this for per-item overriding of arrays... */
      IDOverrideLibraryProperty *op = BKE_lib_override_library_property_find(liboverride,
                                                                             rna_path->c_str());
      IDOverrideLibraryPropertyOperation *opop = static_cast<IDOverrideLibraryPropertyOperation *>(
          op ? op->operations.first : nullptr);

      if (op != nullptr) {
        /* Only set all operations from this property as used (via
         * #BKE_lib_override_library_operations_tag) if the property itself is still tagged as
         * unused.
         *
         * In case the property itself is already tagged as used, in means lower-level diffing code
         * took care of this property (e.g. as is needed collections of items, since then some
         * operations may be valid, while others may need to be purged). */
        if (op->tag & LIBOVERRIDE_PROP_OP_TAG_UNUSED) {
          BKE_lib_override_library_operations_tag(op, LIBOVERRIDE_PROP_OP_TAG_UNUSED, false);
        }
      }

      if ((do_restore || do_tag_for_restore) &&
          (report_flags & RNA_OVERRIDE_MATCH_RESULT_CREATED) == 0)
      {
        /* We are allowed to restore to reference's values. */
        if (ELEM(nullptr, op, opop) || opop->operation == LIBOVERRIDE_OP_NOOP) {
          if (RNA_property_editable(ptr_local, rawprop)) {
            /* This property should be restored to its reference value. This should not be done
             * here, since this code may be called from non-main thread (modifying data through RNA
             * is not thread safe). */
            if (do_restore) {
              IDOverrideLibraryPropertyOperation opop_tmp{};
              opop_tmp.operation = LIBOVERRIDE_OP_REPLACE;
              opop_tmp.subitem_reference_index = -1;
              opop_tmp.subitem_local_index = -1;

              RNAPropertyOverrideApplyContext rnaapply_ctx;
              rnaapply_ctx.ptr_dst = *ptr_local;
              rnaapply_ctx.ptr_src = *ptr_reference;
              rnaapply_ctx.prop_dst = rawprop;
              rnaapply_ctx.prop_src = rawprop;
              rnaapply_ctx.liboverride_operation = &opop_tmp;

              const bool is_restored = rna_property_override_operation_apply(bmain, rnaapply_ctx);

              if (is_restored) {
                CLOG_DEBUG(&LOG,
                           "Restoreed forbidden liboverride `%s` for override data '%s'",
                           rna_path->c_str(),
                           ptr_local->owner_id->name);
                if (r_report_flags) {
                  *r_report_flags |= RNA_OVERRIDE_MATCH_RESULT_RESTORED;
                }
              }
              else {
                CLOG_DEBUG(&LOG,
                           "Failed to restore forbidden liboverride `%s` for override data '%s'",
                           rna_path->c_str(),
                           ptr_local->owner_id->name);
              }
            }
            else {
              if (op == nullptr) {
                /* An override property is needed, create a temp one if necessary. */
                op = BKE_lib_override_library_property_get(
                    liboverride, rna_path->c_str(), nullptr);
                BKE_lib_override_library_operations_tag(op, LIBOVERRIDE_PROP_OP_TAG_UNUSED, true);
              }
              IDOverrideLibraryPropertyOperation *opop_restore =
                  BKE_lib_override_library_property_operation_get(op,
                                                                  LIBOVERRIDE_OP_REPLACE,
                                                                  nullptr,
                                                                  nullptr,
                                                                  {},
                                                                  {},
                                                                  -1,
                                                                  -1,
                                                                  false,
                                                                  nullptr,
                                                                  nullptr);
              /* Do not use `BKE_lib_override_library_operations_tag` here, as the property may be
               * a valid one that has other operations that needs to remain (e.g. from a template,
               * a NOOP operation to enforce no change on that property, etc.). */
              op->tag |= LIBOVERRIDE_PROP_TAG_NEEDS_RETORE;
              opop_restore->tag |= LIBOVERRIDE_PROP_TAG_NEEDS_RETORE;
              liboverride->runtime->tag |= LIBOVERRIDE_TAG_NEEDS_RESTORE;

              CLOG_DEBUG(
                  &LOG,
                  "Tagging for restoration forbidden liboverride `%s` for override data '%s'",
                  rna_path->c_str(),
                  ptr_local->owner_id->name);
              if (r_report_flags) {
                *r_report_flags |= RNA_OVERRIDE_MATCH_RESULT_RESTORE_TAGGED;
              }
            }
          }
          else {
            /* Too noisy for now, this triggers on runtime props like transform matrices etc. */
#if 0
            BLI_assert_msg(0,
                           "We have differences between reference and "
                           "overriding data on non-editable property.");
#endif
            matching = false;
          }
        }
      }
      else if ((report_flags & RNA_OVERRIDE_MATCH_RESULT_CREATED) == 0 && ELEM(nullptr, op, opop))
      {
        /* This property is not overridden, and differs from reference, so we have no match. */
        matching = false;
        if (!(do_create || do_restore || do_tag_for_restore)) {

          break;
        }
      }
    }
#undef RNA_PATH_BUFFSIZE
  }
  RNA_property_collection_end(&iter);

#ifdef DEBUG_OVERRIDE_TIMEIT
  if (!root_path) {
    const float _delta_time = float(BLI_time_now_seconds() - _timeit_time_global);
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
           int(_num_time_global));
    printf("diffing time end      (%s): %.6f (in %d runs)\n",
           __func__,
           _delta_time_diffing,
           _num_delta_time_diffing);
    printf("diffing time averaged (%s): %.6f (total: %.6f, in %d runs)\n",
           __func__,
           (_sum_time_diffing / _num_time_diffing),
           _sum_time_diffing,
           int(_num_time_diffing));
  }
#endif

  return matching;
}

bool RNA_struct_override_store(Main *bmain,
                               PointerRNA *ptr_local,
                               PointerRNA *ptr_reference,
                               PointerRNA *ptr_storage,
                               IDOverrideLibrary *liboverride)
{
  bool changed = false;

#ifdef DEBUG_OVERRIDE_TIMEIT
  TIMEIT_START_AVERAGED(RNA_struct_override_store);
#endif
  LISTBASE_FOREACH (IDOverrideLibraryProperty *, op, &liboverride->properties) {
    /* Simplified for now! */
    PointerRNA data_reference, data_local;
    PropertyRNA *prop_reference, *prop_local;

    if (RNA_path_resolve_property(ptr_local, op->rna_path, &data_local, &prop_local) &&
        RNA_path_resolve_property(ptr_reference, op->rna_path, &data_reference, &prop_reference))
    {
      PointerRNA data_storage;
      PropertyRNA *prop_storage = nullptr;

      /* It is totally OK if this does not success,
       * only a subset of override operations actually need storage. */
      if (ptr_storage && (ptr_storage->owner_id != nullptr)) {
        RNA_path_resolve_property(ptr_storage, op->rna_path, &data_storage, &prop_storage);
      }

      if (rna_property_override_operation_store(bmain,
                                                &data_local,
                                                &data_reference,
                                                &data_storage,
                                                prop_reference,
                                                prop_local,
                                                prop_storage,
                                                op))
      {
        changed = true;
      }
    }
  }
#ifdef DEBUG_OVERRIDE_TIMEIT
  TIMEIT_END_AVERAGED(RNA_struct_override_store);
#endif

  return changed;
}

static bool rna_property_override_collection_subitem_name_id_match(
    const char *item_name,
    const int item_name_len,
    const bool do_id_pointer,
    const std::optional<ID *> &item_id,
    PointerRNA *ptr_item_name)
{
  BLI_assert(!do_id_pointer || RNA_struct_is_ID(ptr_item_name->type));

  bool is_match = false;

  if (do_id_pointer) {
    if (*item_id != static_cast<ID *>(ptr_item_name->data)) {
      /* If the ID pointer does not match, then there is no match, no need to check the
       * name itself. */
      return is_match;
    }
  }

  PropertyRNA *nameprop = ptr_item_name->type->nameproperty;
  char name_buf[256];
  char *name;
  int namelen;

  name = RNA_property_string_get_alloc(
      ptr_item_name, nameprop, name_buf, sizeof(name_buf), &namelen);

  is_match = ((item_name_len == namelen) && STREQ(item_name, name));

  if (UNLIKELY(name != name_buf)) {
    MEM_freeN(name);
  }

  return is_match;
}

static bool rna_property_override_collection_subitem_name_id_lookup(
    PointerRNA *ptr,
    PropertyRNA *prop,
    const char *item_name,
    const int item_name_len,
    const bool do_id_pointer,
    const std::optional<ID *> &item_id,
    PointerRNA *r_ptr_item_name)
{
  /* NOTE: This code is very similar to the one from #RNA_property_collection_lookup_string_index,
   * but it adds an extra early check on matching ID pointer.
   *
   * This custom code is needed because otherwise, it is only possible to check the first
   * name-matched item found by #RNA_property_collection_lookup_string, and not potential other
   * items having the same name. */
  if (do_id_pointer) {
    BLI_assert(RNA_property_type(prop) == PROP_COLLECTION);

    /* We cannot use a potential `CollectionPropertyRNA->lookupstring` here. */
    CollectionPropertyIterator iter;

    RNA_property_collection_begin(ptr, prop, &iter);
    for (; iter.valid; RNA_property_collection_next(&iter)) {
      if (iter.ptr.data && iter.ptr.type->nameproperty) {
        if (rna_property_override_collection_subitem_name_id_match(
                item_name, item_name_len, do_id_pointer, item_id, &iter.ptr))
        {
          *r_ptr_item_name = iter.ptr;
          break;
        }
      }
    }
    RNA_property_collection_end(&iter);

    if (!iter.valid) {
      *r_ptr_item_name = {};
    }

    return iter.valid;
  }

  return RNA_property_collection_lookup_string(ptr, prop, item_name, r_ptr_item_name);
}

static void rna_property_override_collection_subitem_name_index_lookup(
    PointerRNA *ptr,
    PropertyRNA *prop,
    const char *item_name,
    const std::optional<ID *> &item_id,
    const int item_index,
    /* Never use index-only lookup to validate a match (unless no item name (+ id) was given). */
    const bool ignore_index_only_lookup,
    PointerRNA *r_ptr_item_name,
    PointerRNA *r_ptr_item_index)
{
  r_ptr_item_name->invalidate();
  r_ptr_item_index->invalidate();

  const bool do_id_pointer = item_id && RNA_struct_is_ID(RNA_property_pointer_type(ptr, prop));

  const int item_name_len = item_name ? int(strlen(item_name)) : 0;

  /* First, lookup by index, but only validate if name also matches (or if there is no given name).
   *
   * Note that this is also beneficial on performances (when looking up in big collections), since
   * typically index lookup will be faster than name lookup.
   */
  if (item_index != -1) {
    if (RNA_property_collection_lookup_int(ptr, prop, item_index, r_ptr_item_index)) {
      if (item_name && r_ptr_item_index->type) {
        if (rna_property_override_collection_subitem_name_id_match(
                item_name, item_name_len, do_id_pointer, item_id, r_ptr_item_index))
        {
          *r_ptr_item_name = *r_ptr_item_index;
          return;
        }
      }
    }
  }

  if (!item_name) {
    return;
  }

  /* If index + name (+ id) lookup failed, do not keep result of index-only lookup. That means that
   * if the name (+ id) only lookup fails, no matching item was found, even if index-only would
   * have matched. */
  if (ignore_index_only_lookup) {
    r_ptr_item_index->invalidate();
  }

  /* Then, lookup by name (+ id) only. */
  if (rna_property_override_collection_subitem_name_id_lookup(
          ptr, prop, item_name, item_name_len, do_id_pointer, item_id, r_ptr_item_name))
  {
    r_ptr_item_index->invalidate();
    return;
  }

  /* If name (+ id) lookup failed, `r_ptr_item_name` is invalidated, so if index lookup was
   * successful it will be the only valid return value. */
}

static void rna_property_override_collection_subitem_lookup(
    RNAPropertyOverrideApplyContext &rnaapply_ctx)
{
  PointerRNA *ptr_dst = &rnaapply_ctx.ptr_dst;
  PointerRNA *ptr_src = &rnaapply_ctx.ptr_src;
  PointerRNA *ptr_storage = &rnaapply_ctx.ptr_storage;
  PropertyRNA *prop_dst = rnaapply_ctx.prop_dst;
  PropertyRNA *prop_src = rnaapply_ctx.prop_src;
  PropertyRNA *prop_storage = rnaapply_ctx.prop_storage;
  PointerRNA *ptr_item_dst = &rnaapply_ctx.ptr_item_dst;
  PointerRNA *ptr_item_src = &rnaapply_ctx.ptr_item_src;
  PointerRNA *ptr_item_storage = &rnaapply_ctx.ptr_item_storage;
  IDOverrideLibraryProperty *op = rnaapply_ctx.liboverride_property;
  IDOverrideLibraryPropertyOperation *opop = rnaapply_ctx.liboverride_operation;

  if ((RNA_property_type(prop_dst) != PROP_COLLECTION ||
       RNA_property_type(prop_src) != PROP_COLLECTION ||
       (prop_storage != nullptr && RNA_property_type(prop_storage) != PROP_COLLECTION)) ||
      (opop->subitem_local_name == nullptr && opop->subitem_reference_name == nullptr &&
       opop->subitem_local_index == -1 && opop->subitem_reference_index == -1))
  {
    return;
  }

  const bool use_id_pointer = (opop->flag & LIBOVERRIDE_OP_FLAG_IDPOINTER_ITEM_USE_ID) != 0;
  std::optional<ID *> subitem_local_id = use_id_pointer ? std::optional(opop->subitem_local_id) :
                                                          std::nullopt;
  std::optional<ID *> subitem_reference_id = use_id_pointer ?
                                                 std::optional(opop->subitem_reference_id) :
                                                 std::nullopt;

  ptr_item_dst->invalidate();
  ptr_item_src->invalidate();
  if (prop_storage != nullptr) {
    ptr_item_storage->invalidate();
  }

  /* If there is an item ID, there should _always_ be a valid item name too. */
  BLI_assert(opop->subitem_local_name || !subitem_local_id);
  BLI_assert(opop->subitem_reference_name || !subitem_reference_id);
  /* Do not match by index only, if there are valid item names and ID.
   *
   * Otherwise, it can end up 'matching by index' e.g. collection childrens, re-assigning
   * completely wrong collections only based on indices. This is especially bad when some
   * collections are _removed_ from the reference collection's children. */
  const bool ignore_index_only_lookup = (subitem_local_id || subitem_reference_id);

  PointerRNA ptr_item_dst_name, ptr_item_dst_index;
  PointerRNA ptr_item_src_name, ptr_item_src_index;
  PointerRNA ptr_item_storage_name, ptr_item_storage_index;
  rna_property_override_collection_subitem_name_index_lookup(ptr_src,
                                                             prop_src,
                                                             opop->subitem_local_name,
                                                             subitem_local_id,
                                                             opop->subitem_local_index,
                                                             ignore_index_only_lookup,
                                                             &ptr_item_src_name,
                                                             &ptr_item_src_index);
  rna_property_override_collection_subitem_name_index_lookup(ptr_dst,
                                                             prop_dst,
                                                             opop->subitem_reference_name,
                                                             subitem_reference_id,
                                                             opop->subitem_reference_index,
                                                             ignore_index_only_lookup,
                                                             &ptr_item_dst_name,
                                                             &ptr_item_dst_index);
  /* This is rather fragile, but the fact that local override IDs may have a different name
   * than their linked reference makes it necessary.
   * Basically, here we are considering that if we cannot find the original linked ID in
   * the local override we are (re-)applying the operations, then it may be because some of
   * those operations have already been applied, and we may already have the local ID
   * pointer we want to set.
   * This happens e.g. during re-sync of an override, since we have already remapped all ID
   * pointers to their expected values.
   * In that case we simply try to get the property from the local expected name. */
  if (opop->subitem_reference_name != nullptr && opop->subitem_local_name != nullptr &&
      ptr_item_dst_name.type == nullptr)
  {
    rna_property_override_collection_subitem_name_index_lookup(
        ptr_dst,
        prop_dst,
        opop->subitem_local_name,
        {},
        opop->subitem_reference_index != -1 ? opop->subitem_reference_index :
                                              opop->subitem_local_index,
        ignore_index_only_lookup,
        &ptr_item_dst_name,
        &ptr_item_dst_index);
  }

  /* For historical compatibility reasons, we fallback to reference if no local item info is given,
   * and vice-versa. */
  if (opop->subitem_reference_name == nullptr && opop->subitem_local_name != nullptr) {
    rna_property_override_collection_subitem_name_index_lookup(
        ptr_dst,
        prop_dst,
        opop->subitem_local_name,
        {},
        opop->subitem_reference_index != -1 ? opop->subitem_reference_index :
                                              opop->subitem_local_index,
        ignore_index_only_lookup,
        &ptr_item_dst_name,
        &ptr_item_dst_index);
  }
  else if (opop->subitem_reference_name != nullptr && opop->subitem_local_name == nullptr) {
    rna_property_override_collection_subitem_name_index_lookup(ptr_src,
                                                               prop_src,
                                                               opop->subitem_reference_name,
                                                               {},
                                                               opop->subitem_local_index != -1 ?
                                                                   opop->subitem_local_index :
                                                                   opop->subitem_reference_index,
                                                               ignore_index_only_lookup,
                                                               &ptr_item_src_name,
                                                               &ptr_item_src_index);
  }
  if (opop->subitem_reference_index == -1 && opop->subitem_local_index != -1) {
    rna_property_override_collection_subitem_name_index_lookup(ptr_dst,
                                                               prop_dst,
                                                               nullptr,
                                                               {},
                                                               opop->subitem_local_index,
                                                               ignore_index_only_lookup,
                                                               &ptr_item_dst_name,
                                                               &ptr_item_dst_index);
  }
  else if (opop->subitem_reference_index != -1 && opop->subitem_local_index == -1) {
    rna_property_override_collection_subitem_name_index_lookup(ptr_src,
                                                               prop_src,
                                                               nullptr,
                                                               {},
                                                               opop->subitem_reference_index,
                                                               ignore_index_only_lookup,
                                                               &ptr_item_src_name,
                                                               &ptr_item_src_index);
  }

  /* For storage, simply lookup by name first, and fallback to indices. */
  if (prop_storage != nullptr) {
    rna_property_override_collection_subitem_name_index_lookup(ptr_storage,
                                                               prop_storage,
                                                               opop->subitem_local_name,
                                                               subitem_local_id,
                                                               opop->subitem_local_index,
                                                               ignore_index_only_lookup,
                                                               &ptr_item_storage_name,
                                                               &ptr_item_storage_index);
    if (ptr_item_storage_name.data == nullptr) {
      rna_property_override_collection_subitem_name_index_lookup(ptr_storage,
                                                                 prop_storage,
                                                                 opop->subitem_reference_name,
                                                                 subitem_reference_id,
                                                                 opop->subitem_reference_index,
                                                                 ignore_index_only_lookup,
                                                                 &ptr_item_storage_name,
                                                                 &ptr_item_storage_index);
    }
    if (ptr_item_storage_name.data == nullptr && ptr_item_storage_index.data == nullptr) {
      rna_property_override_collection_subitem_name_index_lookup(ptr_storage,
                                                                 prop_storage,
                                                                 nullptr,
                                                                 {},
                                                                 opop->subitem_local_index,
                                                                 ignore_index_only_lookup,
                                                                 &ptr_item_storage_name,
                                                                 &ptr_item_storage_index);
    }
  }

  /* Final selection. Both matches have to be based on names, or indices, but not a mix of both.
   * If we are missing either source or destination data based on names, and based on indices, then
   * use partial data from names (allows to handle 'need resync' detection cases). */
  if ((ptr_item_src_name.type || ptr_item_dst_name.type) &&
      !(ptr_item_src_index.type && ptr_item_dst_index.type))
  {
    *ptr_item_src = ptr_item_src_name;
    *ptr_item_dst = ptr_item_dst_name;
    if (prop_storage != nullptr) {
      *ptr_item_storage = ptr_item_storage_name;
    }
  }
  else if (ptr_item_src_index.type != nullptr || ptr_item_dst_index.type != nullptr) {
    *ptr_item_src = ptr_item_src_index;
    *ptr_item_dst = ptr_item_dst_index;
    if (prop_storage != nullptr) {
      *ptr_item_storage = ptr_item_storage_index;
    }
  }

  /* Note that there is no reason to report in case no item is expected, i.e. in case subitem name
   * and index are invalid. This can often happen when inserting new items (constraint,
   * modifier...) in a collection that supports it. */
  if (ptr_item_dst->type == nullptr &&
      ((opop->subitem_reference_name != nullptr && opop->subitem_reference_name[0] != '\0') ||
       opop->subitem_reference_index != -1))
  {
    CLOG_DEBUG(&LOG,
               "Failed to find destination sub-item '%s' (%d) of '%s' in new override data '%s'",
               opop->subitem_reference_name != nullptr ? opop->subitem_reference_name : "",
               opop->subitem_reference_index,
               op->rna_path,
               ptr_dst->owner_id->name);
  }
  if (ptr_item_src->type == nullptr &&
      ((opop->subitem_local_name != nullptr && opop->subitem_local_name[0] != '\0') ||
       opop->subitem_local_index != -1))
  {
    CLOG_DEBUG(&LOG,
               "Failed to find source sub-item '%s' (%d) of '%s' in old override data '%s'",
               opop->subitem_local_name != nullptr ? opop->subitem_local_name : "",
               opop->subitem_local_index,
               op->rna_path,
               ptr_src->owner_id->name);
  }
}

static void rna_property_override_check_resync(Main *bmain,
                                               PointerRNA *ptr_dst,
                                               PointerRNA *ptr_src,
                                               PointerRNA *ptr_item_dst,
                                               PointerRNA *ptr_item_src)
{
  ID *id_owner_src = rna_property_override_property_real_id_owner(
      bmain, ptr_src, nullptr, nullptr);
  ID *id_owner_dst = rna_property_override_property_real_id_owner(
      bmain, ptr_dst, nullptr, nullptr);
  ID *id_src = rna_property_override_property_real_id_owner(bmain, ptr_item_src, nullptr, nullptr);
  ID *id_dst = rna_property_override_property_real_id_owner(bmain, ptr_item_dst, nullptr, nullptr);

  BLI_assert(ID_IS_OVERRIDE_LIBRARY_REAL(id_owner_src));

  /* If the owner ID is not part of an override hierarchy, there is no possible resync. */
  if (id_owner_src->override_library->flag & LIBOVERRIDE_FLAG_NO_HIERARCHY) {
    return;
  }

  /* If `id_src` is not a liboverride, we cannot perform any further 'need resync' checks from
   * here. */
  if (id_src != nullptr && !ID_IS_OVERRIDE_LIBRARY_REAL(id_src)) {
    return;
  }

  if (/* We might be in a case where id_dst has already been processed and its usages
       * remapped to its new local override. In that case overrides and linked data
       * are always properly matching. */
      id_src != id_dst &&
      /* If one of the pointers is nullptr and not the other, we are in a non-matching case. */
      (ELEM(nullptr, id_src, id_dst) ||
       /* If `id_dst` is not from same lib as id_src, and linked reference ID of `id_src` is not
        * `id_dst`, we are in a non-matching case. */
       (id_dst->lib != id_src->lib && id_src->override_library->reference != id_dst) ||
       /* If `id_dst` is from same lib as id_src, and is not same as `id_owner`, we are in a
        * non-matching case.
        *
        * NOTE: Here we are testing if `id_owner` is referencing itself, in that case the new
        * override copy generated by `BKE_lib_override_library_update` will already have its
        * self-references updated to itself, instead of still pointing to its linked source. */
       (id_dst->lib == id_src->lib && id_dst != id_owner_dst)))
  {
    id_owner_dst->tag |= ID_TAG_LIBOVERRIDE_NEED_RESYNC;
    if (ID_IS_LINKED(id_owner_src)) {
      id_owner_src->lib->runtime->tag |= LIBRARY_TAG_RESYNC_REQUIRED;
    }
    CLOG_DEBUG(&LOG,
               "Local override %s detected as needing resync due to mismatch in its used IDs",
               id_owner_dst->name);
  }
  if ((id_owner_src->override_library->reference->tag & ID_TAG_LIBOVERRIDE_NEED_RESYNC) != 0) {
    id_owner_dst->tag |= ID_TAG_LIBOVERRIDE_NEED_RESYNC;
    if (ID_IS_LINKED(id_owner_src)) {
      id_owner_src->lib->runtime->tag |= LIBRARY_TAG_RESYNC_REQUIRED;
    }
    CLOG_DEBUG(&LOG,
               "Local override %s detected as needing resync as its liboverride reference is "
               "already tagged for resync",
               id_owner_dst->name);
  }
}

static void rna_property_override_apply_ex(Main *bmain,
                                           RNAPropertyOverrideApplyContext &rnaapply_ctx)
{
  IDOverrideLibraryProperty *op = rnaapply_ctx.liboverride_property;
  const bool do_insert = rnaapply_ctx.do_insert;

  LISTBASE_FOREACH (IDOverrideLibraryPropertyOperation *, opop, &op->operations) {
    if (opop->operation == LIBOVERRIDE_OP_NOOP) {
      continue;
    }

    if (!do_insert !=
        !ELEM(opop->operation, LIBOVERRIDE_OP_INSERT_AFTER, LIBOVERRIDE_OP_INSERT_BEFORE))
    {
      if (!do_insert) {
        CLOG_DEBUG(&LOG, "Skipping insert override operations in first pass (%s)", op->rna_path);
      }
      continue;
    }

    rnaapply_ctx.liboverride_operation = opop;

    rna_property_override_collection_subitem_lookup(rnaapply_ctx);

    if (!rna_property_override_operation_apply(bmain, rnaapply_ctx)) {
      CLOG_DEBUG(&LOG,
                 "Failed to apply '%s' override operation on %s\n",
                 op->rna_path,
                 rnaapply_ctx.ptr_src.owner_id->name);
    }
  }

  rnaapply_ctx.liboverride_operation = nullptr;
}

/**
 * Workaround for broken overrides, non-matching ID pointers override operations that replace a
 * non-null value are then assumed as 'mistakes', and ignored (not applied).
 */
static bool override_apply_property_check_skip(Main *bmain,
                                               PointerRNA *id_ptr_dst,
                                               PointerRNA *id_ptr_src,
                                               RNAPropertyOverrideApplyContext &rnaapply_ctx)
{
  UNUSED_VARS_NDEBUG(bmain, id_ptr_src);

  if ((rnaapply_ctx.flag & RNA_OVERRIDE_APPLY_FLAG_IGNORE_ID_POINTERS) == 0) {
    return false;
  }

  if (!RNA_struct_is_ID(RNA_property_pointer_type(&rnaapply_ctx.ptr_dst, rnaapply_ctx.prop_dst))) {
    BLI_assert(!RNA_struct_is_ID(
        RNA_property_pointer_type(&rnaapply_ctx.ptr_src, rnaapply_ctx.prop_src)));
    return false;
  }

  IDOverrideLibraryProperty *op = rnaapply_ctx.liboverride_property;

  /* IDProperties case. */
  if (rnaapply_ctx.prop_dst->magic != RNA_MAGIC) {
    CLOG_DEBUG(&LOG,
               "%s: Ignoring local override on ID pointer custom property '%s', as requested by "
               "RNA_OVERRIDE_APPLY_FLAG_IGNORE_ID_POINTERS flag",
               id_ptr_dst->owner_id->name,
               op->rna_path);
    return true;
  }

  switch (op->rna_prop_type) {
    case PROP_POINTER: {
      if ((static_cast<IDOverrideLibraryPropertyOperation *>(op->operations.first)->flag &
           LIBOVERRIDE_OP_FLAG_IDPOINTER_MATCH_REFERENCE) == 0)
      {
        BLI_assert(id_ptr_src->owner_id == rna_property_override_property_real_id_owner(
                                               bmain, &rnaapply_ctx.ptr_src, nullptr, nullptr));
        BLI_assert(id_ptr_dst->owner_id == rna_property_override_property_real_id_owner(
                                               bmain, &rnaapply_ctx.ptr_dst, nullptr, nullptr));

        CLOG_DEBUG(&LOG,
                   "%s: Ignoring local override on ID pointer property '%s', as requested by "
                   "RNA_OVERRIDE_APPLY_FLAG_IGNORE_ID_POINTERS flag",
                   id_ptr_dst->owner_id->name,
                   op->rna_path);
        return true;
      }
      break;
    }
    case PROP_COLLECTION: {
      /* For collections of ID pointers just completely skip the override ops here... A tad brutal,
       * but this is a backup 'fix the mess' tool, and in practice this should never be an issue.
       * Can always be refined later if needed. */
      CLOG_DEBUG(&LOG,
                 "%s: Ignoring all local override on ID pointer collection property '%s', as "
                 "requested by RNA_OVERRIDE_APPLY_FLAG_IGNORE_ID_POINTERS flag",
                 id_ptr_dst->owner_id->name,
                 op->rna_path);
      return true;
    }
    default:
      break;
  }

  return false;
}

void RNA_struct_override_apply(Main *bmain,
                               PointerRNA *id_ptr_dst,
                               PointerRNA *id_ptr_src,
                               PointerRNA *id_ptr_storage,
                               IDOverrideLibrary *liboverride,
                               const eRNAOverrideApplyFlag flag)
{
#ifdef DEBUG_OVERRIDE_TIMEIT
  TIMEIT_START_AVERAGED(RNA_struct_override_apply);
#endif
  const bool do_restore_only = (flag & RNA_OVERRIDE_APPLY_FLAG_RESTORE_ONLY) != 0;
  /* NOTE: Applying insert operations in a separate pass is mandatory.
   * We could optimize this later, but for now, as inefficient as it is,
   * don't think this is a critical point.
   */
  bool do_insert = false;
  for (int i = 0; i < (do_restore_only ? 1 : 2); i++, do_insert = true) {
    LISTBASE_FOREACH (IDOverrideLibraryProperty *, op, &liboverride->properties) {
      if (do_restore_only && (op->tag % LIBOVERRIDE_PROP_TAG_NEEDS_RETORE) == 0) {
        continue;
      }
      /* That tag should only exist for short lifespan when restoring values from reference linked
       * data. */
      BLI_assert((op->tag & LIBOVERRIDE_PROP_TAG_NEEDS_RETORE) == 0 || do_restore_only);

      RNAPropertyOverrideApplyContext rnaapply_ctx;
      rnaapply_ctx.flag = flag;
      rnaapply_ctx.do_insert = do_insert;

      rnaapply_ctx.liboverride = liboverride;
      rnaapply_ctx.liboverride_property = op;

      if (!(RNA_path_resolve_property_and_item_pointer(id_ptr_dst,
                                                       op->rna_path,
                                                       &rnaapply_ctx.ptr_dst,
                                                       &rnaapply_ctx.prop_dst,
                                                       &rnaapply_ctx.ptr_item_dst) &&
            RNA_path_resolve_property_and_item_pointer(id_ptr_src,
                                                       op->rna_path,
                                                       &rnaapply_ctx.ptr_src,
                                                       &rnaapply_ctx.prop_src,
                                                       &rnaapply_ctx.ptr_item_src)))
      {
        CLOG_DEBUG(&LOG,
                   "Failed to apply library override operation to '%s.%s' "
                   "(could not resolve some properties, local:  %d, override: %d)",
                   static_cast<ID *>(id_ptr_src->owner_id)->name,
                   op->rna_path,
                   RNA_path_resolve_property(
                       id_ptr_dst, op->rna_path, &rnaapply_ctx.ptr_dst, &rnaapply_ctx.prop_dst),
                   RNA_path_resolve_property(
                       id_ptr_src, op->rna_path, &rnaapply_ctx.ptr_src, &rnaapply_ctx.prop_src));
        continue;
      }

      /* It is totally OK if this does not success,
       * only a subset of override operations actually need storage. */
      if (id_ptr_storage && (id_ptr_storage->owner_id != nullptr)) {
        RNA_path_resolve_property_and_item_pointer(id_ptr_storage,
                                                   op->rna_path,
                                                   &rnaapply_ctx.ptr_storage,
                                                   &rnaapply_ctx.prop_storage,
                                                   &rnaapply_ctx.ptr_item_storage);
      }

      /* Check if an overridden ID pointer supposed to be in sync with linked data gets out of
       * sync. */
      if ((flag & RNA_OVERRIDE_APPLY_FLAG_SKIP_RESYNC_CHECK) == 0 &&
          (id_ptr_dst->owner_id->tag & ID_TAG_LIBOVERRIDE_NEED_RESYNC) == 0)
      {
        if (op->rna_prop_type == PROP_POINTER && op->operations.first != nullptr &&
            (static_cast<IDOverrideLibraryPropertyOperation *>(op->operations.first)->flag &
             LIBOVERRIDE_OP_FLAG_IDPOINTER_MATCH_REFERENCE) != 0)
        {
          BLI_assert(RNA_struct_is_ID(
              RNA_property_pointer_type(&rnaapply_ctx.ptr_src, rnaapply_ctx.prop_src)));
          BLI_assert(id_ptr_src->owner_id == rna_property_override_property_real_id_owner(
                                                 bmain, &rnaapply_ctx.ptr_src, nullptr, nullptr));
          BLI_assert(id_ptr_dst->owner_id == rna_property_override_property_real_id_owner(
                                                 bmain, &rnaapply_ctx.ptr_dst, nullptr, nullptr));

          PointerRNA prop_ptr_src = RNA_property_pointer_get(&rnaapply_ctx.ptr_src,
                                                             rnaapply_ctx.prop_src);
          PointerRNA prop_ptr_dst = RNA_property_pointer_get(&rnaapply_ctx.ptr_dst,
                                                             rnaapply_ctx.prop_dst);
          rna_property_override_check_resync(
              bmain, id_ptr_dst, id_ptr_src, &prop_ptr_dst, &prop_ptr_src);
        }
        else if (op->rna_prop_type == PROP_COLLECTION) {
          if (RNA_struct_is_ID(
                  RNA_property_pointer_type(&rnaapply_ctx.ptr_src, rnaapply_ctx.prop_src)))
          {
            BLI_assert(id_ptr_src->owner_id ==
                       rna_property_override_property_real_id_owner(
                           bmain, &rnaapply_ctx.ptr_src, nullptr, nullptr));
            BLI_assert(id_ptr_dst->owner_id ==
                       rna_property_override_property_real_id_owner(
                           bmain, &rnaapply_ctx.ptr_dst, nullptr, nullptr));

            LISTBASE_FOREACH (IDOverrideLibraryPropertyOperation *, opop, &op->operations) {
              if ((opop->flag & LIBOVERRIDE_OP_FLAG_IDPOINTER_MATCH_REFERENCE) == 0) {
                continue;
              }
              rnaapply_ctx.liboverride_operation = opop;

              rna_property_override_collection_subitem_lookup(rnaapply_ctx);

              rna_property_override_check_resync(bmain,
                                                 id_ptr_dst,
                                                 id_ptr_src,
                                                 &rnaapply_ctx.ptr_item_dst,
                                                 &rnaapply_ctx.ptr_item_src);
            }
            rnaapply_ctx.liboverride_operation = nullptr;
          }
        }
      }

      if (override_apply_property_check_skip(bmain, id_ptr_dst, id_ptr_src, rnaapply_ctx)) {
        continue;
      }

      rna_property_override_apply_ex(bmain, rnaapply_ctx);
    }
  }

  /* Some cases (like point caches) may require additional post-processing. */
  if (RNA_struct_is_a(id_ptr_dst->type, &RNA_ID)) {
    ID *id_dst = static_cast<ID *>(id_ptr_dst->data);
    ID *id_src = static_cast<ID *>(id_ptr_src->data);
    const IDTypeInfo *id_type = BKE_idtype_get_info_from_id(id_dst);
    if (id_type->lib_override_apply_post != nullptr) {
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
  std::optional<std::string> rna_path;

  *r_owner_id = rna_property_override_property_real_id_owner(bmain, ptr, prop, &rna_path);
  if (rna_path) {
    IDOverrideLibraryProperty *op = BKE_lib_override_library_property_find(
        (*r_owner_id)->override_library, rna_path->c_str());
    return op;
  }
  return nullptr;
}

IDOverrideLibraryProperty *RNA_property_override_property_get(Main *bmain,
                                                              PointerRNA *ptr,
                                                              PropertyRNA *prop,
                                                              bool *r_created)
{
  std::optional<std::string> rna_path;

  if (r_created != nullptr) {
    *r_created = false;
  }

  ID *id = rna_property_override_property_real_id_owner(bmain, ptr, prop, &rna_path);
  if (rna_path) {
    IDOverrideLibraryProperty *op = BKE_lib_override_library_property_get(
        id->override_library, rna_path->c_str(), r_created);
    return op;
  }
  return nullptr;
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
    return nullptr;
  }

  return BKE_lib_override_library_property_operation_find(
      op, nullptr, nullptr, {}, {}, index, index, strict, r_strict);
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
  if (r_created != nullptr) {
    *r_created = false;
  }

  IDOverrideLibraryProperty *op = RNA_property_override_property_get(bmain, ptr, prop, nullptr);

  if (!op) {
    return nullptr;
  }

  return BKE_lib_override_library_property_operation_get(
      op, operation, nullptr, nullptr, {}, {}, index, index, strict, r_strict, r_created);
}

eRNAOverrideStatus RNA_property_override_library_status(Main *bmain,
                                                        PointerRNA *ptr,
                                                        PropertyRNA *prop,
                                                        const int index)
{
  eRNAOverrideStatus override_status = eRNAOverrideStatus(0);

  if (!ptr || !prop || !ptr->owner_id || !ID_IS_OVERRIDE_LIBRARY(ptr->owner_id)) {
    return override_status;
  }

  if (RNA_property_overridable_get(ptr, prop) && RNA_property_editable_flag(ptr, prop)) {
    override_status |= RNA_OVERRIDE_STATUS_OVERRIDABLE;
  }

  IDOverrideLibraryPropertyOperation *opop = RNA_property_override_property_operation_find(
      bmain, ptr, prop, index, false, nullptr);
  if (opop != nullptr) {
    override_status |= RNA_OVERRIDE_STATUS_OVERRIDDEN;
    if (opop->flag & LIBOVERRIDE_OP_FLAG_MANDATORY) {
      override_status |= RNA_OVERRIDE_STATUS_MANDATORY;
    }
    if (opop->flag & LIBOVERRIDE_OP_FLAG_LOCKED) {
      override_status |= RNA_OVERRIDE_STATUS_LOCKED;
    }
  }

  return override_status;
}
