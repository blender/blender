/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include <cstdlib>

#include "BLI_math_constants.h"
#include "BLI_string_utf8_symbols.h"

#include "BLT_translation.hh"

#include "RNA_define.hh"

#include "rna_internal.hh"

#include "DNA_armature_types.h"

#include "ED_anim_api.hh"

#include "WM_api.hh"
#include "WM_types.hh"

/* Bone Collection Color Sets */
const EnumPropertyItem rna_enum_color_palettes_items[] = {
    {0, "DEFAULT", 0, "Default Colors", ""},
    {1, "THEME01", ICON_COLORSET_01_VEC, "01 - Theme Color Set", ""},
    {2, "THEME02", ICON_COLORSET_02_VEC, "02 - Theme Color Set", ""},
    {3, "THEME03", ICON_COLORSET_03_VEC, "03 - Theme Color Set", ""},
    {4, "THEME04", ICON_COLORSET_04_VEC, "04 - Theme Color Set", ""},
    {5, "THEME05", ICON_COLORSET_05_VEC, "05 - Theme Color Set", ""},
    {6, "THEME06", ICON_COLORSET_06_VEC, "06 - Theme Color Set", ""},
    {7, "THEME07", ICON_COLORSET_07_VEC, "07 - Theme Color Set", ""},
    {8, "THEME08", ICON_COLORSET_08_VEC, "08 - Theme Color Set", ""},
    {9, "THEME09", ICON_COLORSET_09_VEC, "09 - Theme Color Set", ""},
    {10, "THEME10", ICON_COLORSET_10_VEC, "10 - Theme Color Set", ""},
    {11, "THEME11", ICON_COLORSET_11_VEC, "11 - Theme Color Set", ""},
    {12, "THEME12", ICON_COLORSET_12_VEC, "12 - Theme Color Set", ""},
    {13, "THEME13", ICON_COLORSET_13_VEC, "13 - Theme Color Set", ""},
    {14, "THEME14", ICON_COLORSET_14_VEC, "14 - Theme Color Set", ""},
    {15, "THEME15", ICON_COLORSET_15_VEC, "15 - Theme Color Set", ""},
    {16, "THEME16", ICON_COLORSET_16_VEC, "16 - Theme Color Set", ""},
    {17, "THEME17", ICON_COLORSET_17_VEC, "17 - Theme Color Set", ""},
    {18, "THEME18", ICON_COLORSET_18_VEC, "18 - Theme Color Set", ""},
    {19, "THEME19", ICON_COLORSET_19_VEC, "19 - Theme Color Set", ""},
    {20, "THEME20", ICON_COLORSET_20_VEC, "20 - Theme Color Set", ""},
    {-1, "CUSTOM", 0, "Custom Color Set", ""},
    {0, nullptr, 0, nullptr, nullptr},
};
#ifdef RNA_RUNTIME
constexpr int COLOR_SETS_MAX_THEMED_INDEX = 20;
#endif

#ifdef RNA_RUNTIME

#  include <fmt/format.h>

#  include "BLI_math_vector.h"
#  include "BLI_string.h"
#  include "BLI_string_utf8.h"

#  include "BKE_action.hh"
#  include "BKE_context.hh"
#  include "BKE_global.hh"
#  include "BKE_idprop.hh"
#  include "BKE_lib_id.hh"
#  include "BKE_main.hh"
#  include "BKE_report.hh"

#  include "BKE_armature.hh"
#  include "ED_armature.hh"

#  include "ANIM_bone_collections.hh"

#  include "DEG_depsgraph.hh"
#  include "DEG_depsgraph_build.hh"

#  ifndef NDEBUG
#    include "ANIM_armature_iter.hh"
#  endif

static void rna_Armature_update(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  ID *id = ptr->owner_id;

  DEG_id_tag_update(id, ID_RECALC_SYNC_TO_EVAL);
}

static void rna_Armature_update_data(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  ID *id = ptr->owner_id;

  DEG_id_tag_update(id, 0);
  WM_main_add_notifier(NC_GEOM | ND_DATA, id);
  // WM_main_add_notifier(NC_OBJECT|ND_POSE, nullptr);
}

static void rna_Armature_dependency_update(Main *bmain, Scene * /*scene*/, PointerRNA *ptr)
{
  ID *id = ptr->owner_id;

  DEG_relations_tag_update(bmain);

  DEG_id_tag_update(id, 0);
  WM_main_add_notifier(NC_GEOM | ND_DATA, id);
}

static void rna_Armature_act_bone_set(PointerRNA *ptr, PointerRNA value, ReportList * /*reports*/)
{
  bArmature *arm = (bArmature *)ptr->data;

  if (value.owner_id == nullptr && value.data == nullptr) {
    arm->act_bone = nullptr;
  }
  else {
    if (value.owner_id != &arm->id) {
      Object *ob = (Object *)value.owner_id;

      if (GS(ob->id.name) != ID_OB || (ob->data != arm)) {
        printf("ERROR: armature set active bone - new active does not come from this armature\n");
        return;
      }
    }

    arm->act_bone = static_cast<Bone *>(value.data);
    arm->act_bone->flag |= BONE_SELECTED;
  }
}

static void rna_Armature_act_edit_bone_set(PointerRNA *ptr,
                                           PointerRNA value,
                                           ReportList * /*reports*/)
{
  bArmature *arm = (bArmature *)ptr->data;

  if (value.owner_id == nullptr && value.data == nullptr) {
    arm->act_edbone = nullptr;
  }
  else {
    if (value.owner_id != &arm->id) {
      /* raise an error! */
    }
    else {
      arm->act_edbone = static_cast<EditBone *>(value.data);
      ((EditBone *)arm->act_edbone)->flag |= BONE_SELECTED;
    }
  }
}

static EditBone *rna_Armature_edit_bone_new(bArmature *arm, ReportList *reports, const char *name)
{
  if (arm->edbo == nullptr) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Armature '%s' not in edit mode, cannot add an editbone",
                arm->id.name + 2);
    return nullptr;
  }
  return ED_armature_ebone_add(arm, name);
}

static void rna_Armature_edit_bone_remove(bArmature *arm,
                                          ReportList *reports,
                                          PointerRNA *ebone_ptr)
{
  EditBone *ebone = static_cast<EditBone *>(ebone_ptr->data);
  if (arm->edbo == nullptr) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Armature '%s' not in edit mode, cannot remove an editbone",
                arm->id.name + 2);
    return;
  }

  if (BLI_findindex(arm->edbo, ebone) == -1) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Armature '%s' does not contain bone '%s'",
                arm->id.name + 2,
                ebone->name);
    return;
  }

  ED_armature_ebone_remove(arm, ebone);
  ebone_ptr->invalidate();
}

static void rna_iterator_bone_collections_all_begin(CollectionPropertyIterator *iter,
                                                    PointerRNA *ptr)
{
  bArmature *arm = (bArmature *)ptr->data;
  rna_iterator_array_begin(iter,
                           ptr,
                           arm->collection_array,
                           sizeof(BoneCollection *),
                           arm->collection_array_num,
                           false,
                           nullptr);
}
static int rna_iterator_bone_collections_all_length(PointerRNA *ptr)
{
  bArmature *arm = (bArmature *)ptr->data;
  return arm->collection_array_num;
}

static void rna_iterator_bone_collections_roots_begin(CollectionPropertyIterator *iter,
                                                      PointerRNA *ptr)
{
  bArmature *arm = (bArmature *)ptr->data;
  rna_iterator_array_begin(iter,
                           ptr,
                           arm->collection_array,
                           sizeof(BoneCollection *),
                           arm->collection_root_count,
                           false,
                           nullptr);
}
static int rna_iterator_bone_collections_roots_length(PointerRNA *ptr)
{
  bArmature *arm = (bArmature *)ptr->data;
  return arm->collection_root_count;
}

static void rna_BoneCollections_active_set(PointerRNA *ptr,
                                           PointerRNA value,
                                           struct ReportList * /*reports*/)
{
  bArmature *arm = (bArmature *)ptr->data;
  BoneCollection *bcoll = (BoneCollection *)value.data;
  ANIM_armature_bonecoll_active_set(arm, bcoll);
}

static void rna_iterator_bone_collection_children_begin(CollectionPropertyIterator *iter,
                                                        PointerRNA *ptr)
{
  bArmature *arm = (bArmature *)ptr->owner_id;
  const BoneCollection *bcoll = (BoneCollection *)ptr->data;
  rna_iterator_array_begin(iter,
                           ptr,
                           arm->collection_array + bcoll->child_index,
                           sizeof(BoneCollection *),
                           bcoll->child_count,
                           false,
                           nullptr);
}
static int rna_iterator_bone_collection_children_length(PointerRNA *ptr)
{
  const BoneCollection *bcoll = (BoneCollection *)ptr->data;
  return bcoll->child_count;
}

static PointerRNA rna_BoneCollection_parent_get(PointerRNA *ptr)
{
  bArmature *arm = (bArmature *)ptr->owner_id;
  const BoneCollection *bcoll = (BoneCollection *)ptr->data;

  /* Note that this performs two scans of the array. This might look bad, but as
   * long as `Object.children` still loops in Python over all of
   * `bpy.data.objects`, this should also be acceptable. */
  using namespace blender::animrig;
  const int bcoll_index = armature_bonecoll_find_index(arm, bcoll);
  const int parent_index = armature_bonecoll_find_parent_index(arm, bcoll_index);

  if (parent_index < 0) {
    return PointerRNA_NULL;
  }

  BoneCollection *parent = arm->collection_array[parent_index];
  return RNA_pointer_create_discrete(&arm->id, &RNA_BoneCollection, parent);
}

static void rna_BoneCollection_parent_set(PointerRNA *ptr,
                                          PointerRNA value,
                                          struct ReportList *reports)
{
  using namespace blender::animrig;

  BoneCollection *self = (BoneCollection *)ptr->data;
  BoneCollection *to_parent = (BoneCollection *)value.data;

  bArmature *armature = (bArmature *)ptr->owner_id;

  const int from_bcoll_index = armature_bonecoll_find_index(armature, self);
  const int from_parent_index = armature_bonecoll_find_parent_index(armature, from_bcoll_index);
  const int to_parent_index = armature_bonecoll_find_index(armature, to_parent);

  if (to_parent_index >= 0) {
    /* No need to check for parenthood cycles when the bone collection is turned into a root. */
    if (to_parent_index == from_bcoll_index ||
        armature_bonecoll_is_descendant_of(armature, from_bcoll_index, to_parent_index))
    {
      BKE_report(reports, RPT_ERROR, "Cannot make a bone collection a descendant of itself");
      return;
    }
  }

  armature_bonecoll_move_to_parent(
      armature, from_bcoll_index, -1, from_parent_index, to_parent_index);

  WM_main_add_notifier(NC_OBJECT | ND_BONE_COLLECTION, nullptr);
}

static int rna_BoneCollections_active_index_get(PointerRNA *ptr)
{
  bArmature *arm = (bArmature *)ptr->data;
  return arm->runtime.active_collection_index;
}

static void rna_BoneCollections_active_index_set(PointerRNA *ptr, const int bone_collection_index)
{
  bArmature *arm = (bArmature *)ptr->data;
  ANIM_armature_bonecoll_active_index_set(arm, bone_collection_index);

  WM_main_add_notifier(NC_OBJECT | ND_BONE_COLLECTION, ptr->data);
}

static void rna_BoneCollections_active_index_range(
    PointerRNA *ptr, int *min, int *max, int * /*softmin*/, int * /*softmax*/)
{
  bArmature *arm = (bArmature *)ptr->data;

  /* TODO: Figure out what this function actually is used for, as we may want to protect the first
   * collection (i.e. the default collection that should remain first). */
  *min = 0;
  *max = max_ii(0, arm->collection_array_num - 1);
}

static BoneCollection *rna_BoneCollections_new(bArmature *armature,
                                               ReportList *reports,
                                               const char *name,
                                               BoneCollection *parent)
{
  if (parent == nullptr) {
    BoneCollection *bcoll = ANIM_armature_bonecoll_new(armature, name);
    WM_main_add_notifier(NC_OBJECT | ND_BONE_COLLECTION, armature);
    return bcoll;
  }

  const int32_t parent_index = blender::animrig::armature_bonecoll_find_index(armature, parent);
  if (parent_index < 0) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Bone collection '%s' not found in Armature '%s'",
                parent->name,
                armature->id.name + 2);
    return nullptr;
  }

  BoneCollection *bcoll = ANIM_armature_bonecoll_new(armature, name, parent_index);
  WM_main_add_notifier(NC_OBJECT | ND_BONE_COLLECTION, armature);
  return bcoll;
}

static void rna_BoneCollections_active_name_set(PointerRNA *ptr, const char *name)
{
  bArmature *arm = (bArmature *)ptr->data;
  ANIM_armature_bonecoll_active_name_set(arm, name);
}

static void rna_BoneCollections_move(bArmature *arm, ReportList *reports, int from, int to)
{
  const int count = arm->collection_array_num;
  if (from < 0 || from >= count || to < 0 || to >= count ||
      (from != to && !ANIM_armature_bonecoll_move_to_index(arm, from, to)))
  {
    BKE_reportf(reports, RPT_ERROR, "Cannot move collection from index '%d' to '%d'", from, to);
  }

  WM_main_add_notifier(NC_OBJECT | ND_BONE_COLLECTION, &arm->id);
}

static void rna_BoneCollection_name_set(PointerRNA *ptr, const char *name)
{
  bArmature *arm = (bArmature *)ptr->owner_id;
  BoneCollection *bcoll = (BoneCollection *)ptr->data;

  ANIM_armature_bonecoll_name_set(arm, bcoll, name);
}

static void rna_BoneCollection_is_visible_set(PointerRNA *ptr, const bool is_visible)
{
  bArmature *arm = (bArmature *)ptr->owner_id;
  BoneCollection *bcoll = (BoneCollection *)ptr->data;

  ANIM_armature_bonecoll_is_visible_set(arm, bcoll, is_visible);
}

static bool rna_BoneCollection_is_visible_effectively_get(PointerRNA *ptr)
{
  const bArmature *arm = (bArmature *)ptr->owner_id;
  const BoneCollection *bcoll = (BoneCollection *)ptr->data;
  return ANIM_armature_bonecoll_is_visible_effectively(arm, bcoll);
}

static void rna_BoneCollection_is_solo_set(PointerRNA *ptr, const bool is_solo)
{
  bArmature *arm = (bArmature *)ptr->owner_id;
  BoneCollection *bcoll = (BoneCollection *)ptr->data;

  ANIM_armature_bonecoll_solo_set(arm, bcoll, is_solo);
}

static void rna_BoneCollection_is_expanded_set(PointerRNA *ptr, const bool is_expanded)
{
  BoneCollection *bcoll = (BoneCollection *)ptr->data;
  ANIM_armature_bonecoll_is_expanded_set(bcoll, is_expanded);
}

static std::optional<std::string> rna_BoneCollection_path(const PointerRNA *ptr)
{
  const BoneCollection *bcoll = (const BoneCollection *)ptr->data;
  char name_esc[sizeof(bcoll->name) * 2];
  BLI_str_escape(name_esc, bcoll->name, sizeof(name_esc));
  return fmt::format("collections_all[\"{}\"]", name_esc);
}

static IDProperty **rna_BoneCollection_idprops(PointerRNA *ptr)
{
  BoneCollection *bcoll = static_cast<BoneCollection *>(ptr->data);
  return &bcoll->prop;
}

static IDProperty **rna_BoneCollection_system_idprops(PointerRNA *ptr)
{
  BoneCollection *bcoll = static_cast<BoneCollection *>(ptr->data);
  return &bcoll->system_properties;
}

static void rna_BoneCollectionMemberships_clear(Bone *bone)
{
  ANIM_armature_bonecoll_unassign_all(bone);
  WM_main_add_notifier(NC_OBJECT | ND_BONE_COLLECTION, nullptr);
}

static bool rna_BoneCollection_is_editable_get(PointerRNA *ptr)
{
  bArmature *arm = reinterpret_cast<bArmature *>(ptr->owner_id);
  BoneCollection *bcoll = static_cast<BoneCollection *>(ptr->data);
  return ANIM_armature_bonecoll_is_editable(arm, bcoll);
}

static int rna_BoneCollection_index_get(PointerRNA *ptr)
{
  bArmature *arm = reinterpret_cast<bArmature *>(ptr->owner_id);
  BoneCollection *bcoll = static_cast<BoneCollection *>(ptr->data);
  return blender::animrig::armature_bonecoll_find_index(arm, bcoll);
}

static int rna_BoneCollection_child_number_get(PointerRNA *ptr)
{
  bArmature *arm = reinterpret_cast<bArmature *>(ptr->owner_id);
  BoneCollection *bcoll = static_cast<BoneCollection *>(ptr->data);
  return blender::animrig::armature_bonecoll_child_number_find(arm, bcoll);
}
static void rna_BoneCollection_child_number_set(PointerRNA *ptr, const int new_child_number)
{
  bArmature *arm = reinterpret_cast<bArmature *>(ptr->owner_id);
  BoneCollection *bcoll = static_cast<BoneCollection *>(ptr->data);
  blender::animrig::armature_bonecoll_child_number_set(arm, bcoll, new_child_number);
  WM_main_add_notifier(NC_OBJECT | ND_BONE_COLLECTION, nullptr);
}

/* BoneCollection.bones iterator functions. */

static void rna_BoneCollection_bones_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  bArmature *arm = (bArmature *)ptr->owner_id;
  if (arm->edbo) {
    iter->valid = false;
    BKE_reportf(nullptr, RPT_WARNING, "`Collection.bones` is not available in armature edit mode");
    return;
  }

  BoneCollection *bcoll = (BoneCollection *)ptr->data;
  rna_iterator_listbase_begin(iter, ptr, &bcoll->bones, nullptr);
}

static PointerRNA rna_BoneCollection_bones_get(CollectionPropertyIterator *iter)
{
  ListBaseIterator *lb_iter = &iter->internal.listbase;
  BoneCollectionMember *member = (BoneCollectionMember *)lb_iter->link;
  return RNA_pointer_create_with_parent(iter->parent, &RNA_Bone, member->bone);
}

/* Bone.collections iterator functions. */

static void rna_Bone_collections_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  Bone *bone = (Bone *)ptr->data;
  ListBase /*BoneCollectionReference*/ bone_collection_refs = bone->runtime.collections;
  rna_iterator_listbase_begin(iter, ptr, &bone_collection_refs, nullptr);
}

static PointerRNA rna_Bone_collections_get(CollectionPropertyIterator *iter)
{
  ListBaseIterator *lb_iter = &iter->internal.listbase;
  BoneCollectionReference *bcoll_ref = (BoneCollectionReference *)lb_iter->link;
  return RNA_pointer_create_with_parent(iter->parent, &RNA_BoneCollection, bcoll_ref->bcoll);
}

/* EditBone.collections iterator functions. */

static void rna_EditBone_collections_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  EditBone *ebone = (EditBone *)ptr->data;
  ListBase /*BoneCollectionReference*/ bone_collection_refs = ebone->bone_collections;
  rna_iterator_listbase_begin(iter, ptr, &bone_collection_refs, nullptr);
}

/* Armature.collections library override support. */
static bool rna_Armature_collections_override_apply(Main *bmain,
                                                    RNAPropertyOverrideApplyContext &rnaapply_ctx)
{
  PointerRNA *ptr_src = &rnaapply_ctx.ptr_src;
  PointerRNA *ptr_dst = &rnaapply_ctx.ptr_dst;
  PropertyRNA *prop_dst = rnaapply_ctx.prop_dst;
  PointerRNA *ptr_item_dst = &rnaapply_ctx.ptr_item_dst;
  PointerRNA *ptr_item_src = &rnaapply_ctx.ptr_item_src;
  IDOverrideLibraryPropertyOperation *opop = rnaapply_ctx.liboverride_operation;

  switch (opop->operation) {
    case LIBOVERRIDE_OP_INSERT_AFTER:
      /* This is the case this function was written for: adding new bone collections. It will be
       * handled below this switch. */
      break;
    case LIBOVERRIDE_OP_REPLACE:
      /* NOTE(@sybren): These are stored by Blender when overridable properties are changed on the
       * root collections, However, these are *also* created on the `armature.collections_all`
       * property, which is actually where these per-collection overrides are handled.
       * This doesn't seem to be proper behavior, but I also don't want to spam the console about
       * this as this is not something a user could fix. */
      return false;
    default:
      /* Any other operation is simply not supported, and also not expected to exist. */
      printf("Unsupported RNA override operation on armature collections, ignoring\n");
      return false;
  }

  const bArmature *arm_src = (bArmature *)ptr_src->owner_id;
  bArmature *arm_dst = (bArmature *)ptr_dst->owner_id;
  BoneCollection *bcoll_anchor = static_cast<BoneCollection *>(ptr_item_dst->data);
  BoneCollection *bcoll_src = static_cast<BoneCollection *>(ptr_item_src->data);
  BoneCollection *bcoll = ANIM_armature_bonecoll_insert_copy_after(
      arm_dst, arm_src, bcoll_anchor, bcoll_src);

  if (!ID_IS_LINKED(&arm_dst->id)) {
    /* Mark this bone collection as local override, so that certain operations can be allowed. */
    bcoll->flags |= BONE_COLLECTION_OVERRIDE_LIBRARY_LOCAL;
  }

  RNA_property_update_main(bmain, nullptr, ptr_dst, prop_dst);
  return true;
}

static std::optional<std::string> rna_BoneColor_path_posebone(const PointerRNA *ptr)
{
  /* Find the bPoseChan that owns this BoneColor. */
  const uint8_t *bcolor_ptr = static_cast<const uint8_t *>(ptr->data);
  const uint8_t *bone_ptr = bcolor_ptr - offsetof(bPoseChannel, color);
  const bPoseChannel *bone = reinterpret_cast<const bPoseChannel *>(bone_ptr);

#  ifndef NDEBUG
  /* Sanity check that the above pointer magic actually worked. */
  BLI_assert(GS(ptr->owner_id->name) == ID_OB);
  const Object *ob = reinterpret_cast<const Object *>(ptr->owner_id);
  bool found = false;
  LISTBASE_FOREACH (bPoseChannel *, checkBone, &ob->pose->chanbase) {
    if (&checkBone->color == ptr->data) {
      BLI_assert_msg(checkBone == bone,
                     "pointer magic to find the pose bone failed (found the wrong bone)");
      found = true;
      break;
    }
  }
  BLI_assert_msg(found, "pointer magic to find the pose bone failed (did not find the bone)");
#  endif

  char name_esc[sizeof(bone->name) * 2];
  BLI_str_escape(name_esc, bone->name, sizeof(name_esc));
  return fmt::format("pose.bones[\"{}\"].color", name_esc);
}

static std::optional<std::string> rna_BoneColor_path_bone(const PointerRNA *ptr)
{
  /* Find the Bone that owns this BoneColor. */
  const uint8_t *bcolor_ptr = static_cast<const uint8_t *>(ptr->data);
  const uint8_t *bone_ptr = bcolor_ptr - offsetof(Bone, color);
  const Bone *bone = reinterpret_cast<const Bone *>(bone_ptr);

#  ifndef NDEBUG
  /* Sanity check that the above pointer magic actually worked. */
  BLI_assert(GS(ptr->owner_id->name) == ID_AR);
  const bArmature *arm = reinterpret_cast<const bArmature *>(ptr->owner_id);

  bool found = false;
  blender::animrig::ANIM_armature_foreach_bone(&arm->bonebase, [&](const Bone *checkBone) {
    if (&checkBone->color == ptr->data) {
      BLI_assert_msg(checkBone == bone,
                     "pointer magic to find the pose bone failed (found the wrong bone)");
      found = true;
    }
  });
  BLI_assert_msg(found, "pointer magic to find the pose bone failed (did not find the bone)");
#  endif

  char name_esc[sizeof(bone->name) * 2];
  BLI_str_escape(name_esc, bone->name, sizeof(name_esc));
  return fmt::format("bones[\"{}\"].color", name_esc);
}

static std::optional<std::string> rna_BoneColor_path_editbone(const PointerRNA *ptr)
{
  /* Find the Bone that owns this BoneColor. */
  const uint8_t *bcolor_ptr = static_cast<const uint8_t *>(ptr->data);
  const uint8_t *bone_ptr = bcolor_ptr - offsetof(EditBone, color);
  const EditBone *bone = reinterpret_cast<const EditBone *>(bone_ptr);

#  ifndef NDEBUG
  /* Sanity check that the above pointer magic actually worked. */
  BLI_assert(GS(ptr->owner_id->name) == ID_AR);
  const bArmature *arm = reinterpret_cast<const bArmature *>(ptr->owner_id);

  bool found = false;
  LISTBASE_FOREACH (const EditBone *, checkBone, arm->edbo) {
    if (&checkBone->color == ptr->data) {
      BLI_assert_msg(checkBone == bone,
                     "pointer magic to find the pose bone failed (found the wrong bone)");
      found = true;
      break;
    }
  }
  BLI_assert_msg(found, "pointer magic to find the pose bone failed (did not find the bone)");
#  endif

  char name_esc[sizeof(bone->name) * 2];
  BLI_str_escape(name_esc, bone->name, sizeof(name_esc));
  return fmt::format("bones[\"{}\"].color", name_esc);
}

static std::optional<std::string> rna_BoneColor_path(const PointerRNA *ptr)
{
  const ID *owner = ptr->owner_id;
  BLI_assert_msg(owner, "expecting all bone colors to have an owner");

  switch (GS(owner->name)) {
    case ID_OB:
      return rna_BoneColor_path_posebone(ptr);
    case ID_AR: {
      const bArmature *arm = reinterpret_cast<const bArmature *>(owner);
      if (arm->edbo == nullptr) {
        return rna_BoneColor_path_bone(ptr);
      }
      return rna_BoneColor_path_editbone(ptr);
    }
    default:
      BLI_assert_msg(false, "expected object or armature");
      return std::nullopt;
  }
}

void rna_BoneColor_palette_index_set(PointerRNA *ptr, const int new_palette_index)
{
  if (new_palette_index < -1 || new_palette_index > COLOR_SETS_MAX_THEMED_INDEX) {
    BKE_reportf(nullptr, RPT_ERROR, "Invalid color palette index: %d", new_palette_index);
    return;
  }

  BoneColor *bcolor = static_cast<BoneColor *>(ptr->data);
  bcolor->palette_index = new_palette_index;

  ID *id = ptr->owner_id;
  DEG_id_tag_update(id, ID_RECALC_SYNC_TO_EVAL);
  WM_main_add_notifier(NC_GEOM | ND_DATA, id);
}

bool rna_BoneColor_is_custom_get(PointerRNA *ptr)
{
  BoneColor *bcolor = static_cast<BoneColor *>(ptr->data);
  return bcolor->palette_index < 0;
}

static void rna_BoneColor_update(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  /* Ugly hack to trigger the setting of the SACTION_RUNTIME_FLAG_NEED_CHAN_SYNC flag on the
   * animation editors, which in turn calls ANIM_sync_animchannels_to_data(C) with the right
   * context.
   *
   * Without this, changes to the bone colors are not reflected on the bActionGroup colors.
   */
  WM_main_add_notifier(NC_OBJECT | ND_BONE_SELECT, ptr->data);
}

static void rna_Armature_redraw_data(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  ID *id = ptr->owner_id;

  DEG_id_tag_update(id, ID_RECALC_SYNC_TO_EVAL);
  WM_main_add_notifier(NC_GEOM | ND_DATA, id);
  WM_main_add_notifier(NC_SPACE | ND_SPACE_OUTLINER, nullptr);
}

/* Unselect bones when hidden or not selectable. */
static void rna_EditBone_hide_update(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  bArmature *arm = (bArmature *)ptr->owner_id;
  EditBone *ebone = static_cast<EditBone *>(ptr->data);

  if (ebone->flag & (BONE_HIDDEN_A | BONE_UNSELECTABLE)) {
    ebone->flag &= ~(BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL);
  }

  WM_main_add_notifier(NC_OBJECT | ND_POSE, arm);
  DEG_id_tag_update(&arm->id, ID_RECALC_SYNC_TO_EVAL);
}

/* Unselect bones when hidden or not selectable. */
static void rna_Bone_hide_update(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  bArmature *arm = (bArmature *)ptr->owner_id;
  Bone *bone = (Bone *)ptr->data;
  if (bone->flag & (BONE_HIDDEN_A | BONE_UNSELECTABLE)) {
    bone->flag &= ~(BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL);
  }
  WM_main_add_notifier(NC_OBJECT | ND_POSE, arm);
  DEG_id_tag_update(&arm->id, ID_RECALC_SYNC_TO_EVAL);
}

/* called whenever a bone is renamed */
static void rna_Bone_update_renamed(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  ID *id = ptr->owner_id;

  /* Redraw Outliner / Dope-sheet. */
  WM_main_add_notifier(NC_GEOM | ND_DATA | NA_RENAME, id);

  /* update animation channels */
  WM_main_add_notifier(NC_ANIMATION | ND_ANIMCHAN, id);
}

static std::optional<std::string> rna_Bone_path(const PointerRNA *ptr)
{
  const ID *id = ptr->owner_id;
  const Bone *bone = (const Bone *)ptr->data;
  char name_esc[sizeof(bone->name) * 2];

  BLI_str_escape(name_esc, bone->name, sizeof(name_esc));

  /* special exception for trying to get the path where ID-block is Object
   * - this will be assumed to be from a Pose Bone...
   */
  if (id) {
    if (GS(id->name) == ID_OB) {
      return fmt::format("pose.bones[\"{}\"].bone", name_esc);
    }
  }

  /* from armature... */
  return fmt::format("bones[\"{}\"]", name_esc);
}

static IDProperty **rna_Bone_idprops(PointerRNA *ptr)
{
  Bone *bone = static_cast<Bone *>(ptr->data);
  return &bone->prop;
}

static IDProperty **rna_Bone_system_idprops(PointerRNA *ptr)
{
  Bone *bone = static_cast<Bone *>(ptr->data);
  return &bone->system_properties;
}

static std::optional<std::string> rna_EditBone_path(const PointerRNA *ptr)
{
  EditBone *ebone = static_cast<EditBone *>(ptr->data);
  char name_esc[sizeof(ebone->name) * 2];

  BLI_str_escape(name_esc, ebone->name, sizeof(name_esc));
  return fmt::format("edit_bones[\"{}\"]", name_esc);
}

static IDProperty **rna_EditBone_idprops(PointerRNA *ptr)
{
  EditBone *ebone = static_cast<EditBone *>(ptr->data);
  return &ebone->prop;
}

static IDProperty **rna_EditBone_system_idprops(PointerRNA *ptr)
{
  EditBone *ebone = static_cast<EditBone *>(ptr->data);
  return &ebone->system_properties;
}

static void rna_EditBone_name_set(PointerRNA *ptr, const char *value)
{
  bArmature *arm = (bArmature *)ptr->owner_id;
  EditBone *ebone = (EditBone *)ptr->data;
  char oldname[sizeof(ebone->name)], newname[sizeof(ebone->name)];

  /* need to be on the stack */
  STRNCPY_UTF8(newname, value);
  STRNCPY(oldname, ebone->name);

  BLI_assert(BKE_id_is_in_global_main(&arm->id));
  ED_armature_bone_rename(G_MAIN, arm, oldname, newname);
}

static void rna_Bone_name_set(PointerRNA *ptr, const char *value)
{
  bArmature *arm = (bArmature *)ptr->owner_id;
  Bone *bone = (Bone *)ptr->data;
  char oldname[sizeof(bone->name)], newname[sizeof(bone->name)];

  /* need to be on the stack */
  STRNCPY_UTF8(newname, value);
  STRNCPY(oldname, bone->name);

  BLI_assert(BKE_id_is_in_global_main(&arm->id));
  ED_armature_bone_rename(G_MAIN, arm, oldname, newname);
}

static void rna_EditBone_connected_check(EditBone *ebone)
{
  if (ebone->parent) {
    if (ebone->flag & BONE_CONNECTED) {
      /* Attach this bone to its parent */
      copy_v3_v3(ebone->head, ebone->parent->tail);

      if (ebone->flag & BONE_ROOTSEL) {
        ebone->parent->flag |= BONE_TIPSEL;
      }
    }
    else if (!(ebone->parent->flag & BONE_ROOTSEL)) {
      ebone->parent->flag &= ~BONE_TIPSEL;
    }
  }
}

static void rna_EditBone_connected_set(PointerRNA *ptr, bool value)
{
  EditBone *ebone = (EditBone *)(ptr->data);

  if (value) {
    ebone->flag |= BONE_CONNECTED;
  }
  else {
    ebone->flag &= ~BONE_CONNECTED;
  }

  rna_EditBone_connected_check(ebone);
}

static PointerRNA rna_EditBone_parent_get(PointerRNA *ptr)
{
  EditBone *data = (EditBone *)(ptr->data);
  return RNA_pointer_create_with_parent(*ptr, &RNA_EditBone, data->parent);
}

static void rna_EditBone_parent_set(PointerRNA *ptr, PointerRNA value, ReportList * /*reports*/)
{
  EditBone *ebone = (EditBone *)(ptr->data);
  EditBone *pbone, *parbone = (EditBone *)value.data;

  if (parbone == nullptr) {
    if (ebone->parent && !(ebone->parent->flag & BONE_ROOTSEL)) {
      ebone->parent->flag &= ~BONE_TIPSEL;
    }

    ebone->parent = nullptr;
    ebone->flag &= ~BONE_CONNECTED;
  }
  else {
    /* within same armature */
    if (value.owner_id != ptr->owner_id) {
      return;
    }

    /* make sure this is a valid child */
    if (parbone == ebone) {
      return;
    }

    for (pbone = parbone->parent; pbone; pbone = pbone->parent) {
      if (pbone == ebone) {
        return;
      }
    }

    ebone->parent = parbone;
    rna_EditBone_connected_check(ebone);
  }
}

static void rna_EditBone_matrix_get(PointerRNA *ptr, float *values)
{
  EditBone *ebone = (EditBone *)(ptr->data);
  ED_armature_ebone_to_mat4(ebone, (float (*)[4])values);
}

static void rna_EditBone_matrix_set(PointerRNA *ptr, const float *values)
{
  EditBone *ebone = (EditBone *)(ptr->data);
  ED_armature_ebone_from_mat4(ebone, (float (*)[4])values);
}

static float rna_EditBone_length_get(PointerRNA *ptr)
{
  EditBone *ebone = (EditBone *)(ptr->data);
  return len_v3v3(ebone->head, ebone->tail);
}

static void rna_EditBone_length_set(PointerRNA *ptr, float length)
{
  EditBone *ebone = (EditBone *)(ptr->data);
  float delta[3];

  sub_v3_v3v3(delta, ebone->tail, ebone->head);
  if (normalize_v3(delta) == 0.0f) {
    /* Zero length means directional information is lost. Choose arbitrary direction to avoid
     * getting stuck. */
    delta[2] = 1.0f;
  }

  madd_v3_v3v3fl(ebone->tail, ebone->head, delta, length);
}

static void rna_Bone_bbone_handle_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  bArmature *arm = (bArmature *)ptr->owner_id;
  Bone *bone = (Bone *)ptr->data;

  /* Update all users of this armature after changing B-Bone handles. */
  for (Object *obt = static_cast<Object *>(bmain->objects.first); obt;
       obt = static_cast<Object *>(obt->id.next))
  {
    if (obt->data == arm && obt->pose) {
      bPoseChannel *pchan = BKE_pose_channel_find_name(obt->pose, bone->name);

      if (pchan && pchan->bone == bone) {
        BKE_pchan_rebuild_bbone_handles(obt->pose, pchan);
        DEG_id_tag_update(&obt->id, ID_RECALC_SYNC_TO_EVAL);
      }
    }
  }

  rna_Armature_dependency_update(bmain, scene, ptr);
}

static PointerRNA rna_EditBone_bbone_prev_get(PointerRNA *ptr)
{
  EditBone *data = (EditBone *)(ptr->data);
  return RNA_pointer_create_with_parent(*ptr, &RNA_EditBone, data->bbone_prev);
}

static void rna_EditBone_bbone_prev_set(PointerRNA *ptr,
                                        PointerRNA value,
                                        ReportList * /*reports*/)
{
  EditBone *ebone = (EditBone *)(ptr->data);
  EditBone *hbone = (EditBone *)value.data;

  /* Within the same armature? */
  if (hbone == nullptr || value.owner_id == ptr->owner_id) {
    ebone->bbone_prev = hbone;
  }
}

static void rna_Bone_bbone_prev_set(PointerRNA *ptr, PointerRNA value, ReportList * /*reports*/)
{
  Bone *bone = (Bone *)ptr->data;
  Bone *hbone = (Bone *)value.data;

  /* Within the same armature? */
  if (hbone == nullptr || value.owner_id == ptr->owner_id) {
    bone->bbone_prev = hbone;
  }
}

static PointerRNA rna_EditBone_bbone_next_get(PointerRNA *ptr)
{
  EditBone *data = (EditBone *)(ptr->data);
  return RNA_pointer_create_with_parent(*ptr, &RNA_EditBone, data->bbone_next);
}

static void rna_EditBone_bbone_next_set(PointerRNA *ptr,
                                        PointerRNA value,
                                        ReportList * /*reports*/)
{
  EditBone *ebone = (EditBone *)(ptr->data);
  EditBone *hbone = (EditBone *)value.data;

  /* Within the same armature? */
  if (hbone == nullptr || value.owner_id == ptr->owner_id) {
    ebone->bbone_next = hbone;
  }
}

static void rna_Bone_bbone_next_set(PointerRNA *ptr, PointerRNA value, ReportList * /*reports*/)
{
  Bone *bone = (Bone *)ptr->data;
  Bone *hbone = (Bone *)value.data;

  /* Within the same armature? */
  if (hbone == nullptr || value.owner_id == ptr->owner_id) {
    bone->bbone_next = hbone;
  }
}

static PointerRNA rna_EditBone_color_get(PointerRNA *ptr)
{
  EditBone *data = (EditBone *)(ptr->data);
  return RNA_pointer_create_with_parent(*ptr, &RNA_BoneColor, &data->color);
}

static void rna_Armature_editbone_transform_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  bArmature *arm = (bArmature *)ptr->owner_id;
  EditBone *ebone = (EditBone *)ptr->data;
  EditBone *child;

  /* update our parent */
  if (ebone->parent && ebone->flag & BONE_CONNECTED) {
    copy_v3_v3(ebone->parent->tail, ebone->head);
    ebone->parent->rad_tail = ebone->rad_head;
  }

  /* update our children if necessary */
  for (child = static_cast<EditBone *>(arm->edbo->first); child; child = child->next) {
    if (child->parent == ebone && (child->flag & BONE_CONNECTED)) {
      copy_v3_v3(child->head, ebone->tail);
      child->rad_head = ebone->rad_tail;
    }
  }

  if (arm->flag & ARM_MIRROR_EDIT) {
    ED_armature_ebone_transform_mirror_update(arm, ebone, false);
  }

  rna_Armature_update_data(bmain, scene, ptr);
}

static void rna_Armature_bones_next(CollectionPropertyIterator *iter)
{
  ListBaseIterator *internal = &iter->internal.listbase;
  Bone *bone = (Bone *)internal->link;

  if (bone->childbase.first) {
    internal->link = (Link *)bone->childbase.first;
  }
  else if (bone->next) {
    internal->link = (Link *)bone->next;
  }
  else {
    internal->link = nullptr;

    do {
      bone = bone->parent;
      if (bone && bone->next) {
        internal->link = (Link *)bone->next;
        break;
      }
    } while (bone);
  }

  iter->valid = (internal->link != nullptr);
}

/* not essential, but much faster than the default lookup function */
static bool rna_Armature_bones_lookup_string(PointerRNA *ptr, const char *key, PointerRNA *r_ptr)
{
  bArmature *arm = (bArmature *)ptr->data;
  Bone *bone = BKE_armature_find_bone_name(arm, key);
  if (bone) {
    rna_pointer_create_with_ancestors(*ptr, &RNA_Bone, bone, *r_ptr);
    return true;
  }
  else {
    return false;
  }
}

static bool rna_Armature_is_editmode_get(PointerRNA *ptr)
{
  bArmature *arm = (bArmature *)ptr->owner_id;
  return (arm->edbo != nullptr);
}

static void rna_Armature_transform(bArmature *arm, const float mat[16])
{
  ED_armature_transform(arm, (const float (*)[4])mat, true);
}

static int rna_Armature_relation_line_position_get(PointerRNA *ptr)
{
  bArmature *arm = (bArmature *)ptr->data;
  /* Translate the bitflag to an EnumPropertyItem prop_relation_lines_items item ID. */
  return (arm->flag & ARM_DRAW_RELATION_FROM_HEAD) ? 1 : 0;
}

static void rna_Armature_relation_line_position_set(PointerRNA *ptr, const int value)
{
  bArmature *arm = (bArmature *)ptr->data;

  /* Translate the EnumPropertyItem prop_relation_lines_items item ID to a bitflag */
  switch (value) {
    case 0:
      arm->flag &= ~ARM_DRAW_RELATION_FROM_HEAD;
      break;
    case 1:
      arm->flag |= ARM_DRAW_RELATION_FROM_HEAD;
      break;
    default:
      return;
  }
}

#else

static void rna_def_bonecolor(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "BoneColor", nullptr);
  RNA_def_struct_ui_text(srna, "BoneColor", "Theme color or custom color of a bone");
  RNA_def_struct_ui_icon(srna, ICON_BONE_DATA);
  RNA_def_struct_path_func(srna, "rna_BoneColor_path");

  prop = RNA_def_property(srna, "palette", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "palette_index");
  RNA_def_property_enum_items(prop, rna_enum_color_palettes_items);
  RNA_def_property_enum_funcs(prop, nullptr, "rna_BoneColor_palette_index_set", nullptr);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Color Set", "Color palette to use");
  RNA_def_property_update(prop, 0, "rna_BoneColor_update");

  prop = RNA_def_property(srna, "is_custom", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop, "rna_BoneColor_is_custom_get", nullptr);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(
      prop,
      "Use Custom Color",
      "A color palette is user-defined, instead of using a theme-defined one");

  prop = RNA_def_property(srna, "custom", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_struct_type(prop, "ThemeBoneColorSet");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(
      prop, "Custom", "The custom bone colors, used when palette is 'CUSTOM'");
  RNA_def_property_update(prop, 0, "rna_BoneColor_update");
}

void rna_def_bone_curved_common(StructRNA *srna, bool is_posebone, bool is_editbone)
{
  /* NOTE: The pose-mode values get applied over the top of the edit-mode ones. */

#  define RNA_DEF_CURVEBONE_UPDATE(prop, is_posebone, is_editbone) \
    { \
      if (is_posebone) { \
        RNA_def_property_update(prop, NC_OBJECT | ND_POSE, "rna_Pose_update"); \
      } \
      else if (is_editbone) { \
        RNA_def_property_update(prop, 0, "rna_Armature_editbone_transform_update"); \
      } \
      else { \
        RNA_def_property_update(prop, 0, "rna_Armature_update_data"); \
      } \
    } \
    ((void)0)

  PropertyRNA *prop;

  /* Roll In/Out */
  prop = RNA_def_property(srna, "bbone_rollin", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, nullptr, "roll1");
  RNA_def_property_ui_range(prop, -M_PI * 2, M_PI * 2, 10, 2);
  RNA_def_property_ui_text(
      prop, "Roll In", "Roll offset for the start of the B-Bone, adjusts twist");
  RNA_DEF_CURVEBONE_UPDATE(prop, is_posebone, is_editbone);

  prop = RNA_def_property(srna, "bbone_rollout", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, nullptr, "roll2");
  RNA_def_property_ui_range(prop, -M_PI * 2, M_PI * 2, 10, 2);
  RNA_def_property_ui_text(
      prop, "Roll Out", "Roll offset for the end of the B-Bone, adjusts twist");
  RNA_DEF_CURVEBONE_UPDATE(prop, is_posebone, is_editbone);

  if (is_posebone == false) {
    prop = RNA_def_property(srna, "use_endroll_as_inroll", PROP_BOOLEAN, PROP_NONE);
    RNA_def_property_ui_text(
        prop, "Inherit End Roll", "Add Roll Out of the Start Handle bone to the Roll In value");
    RNA_def_property_boolean_sdna(prop, nullptr, "bbone_flag", BBONE_ADD_PARENT_END_ROLL);
    RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
    RNA_def_property_update(prop, 0, "rna_Armature_dependency_update");
  }

  /* Curve X/Y Offsets */
  prop = RNA_def_property(srna, "bbone_curveinx", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "curve_in_x");
  RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, RNA_TRANSLATION_PREC_DEFAULT);
  RNA_def_property_ui_text(
      prop, "In X", "X-axis handle offset for start of the B-Bone's curve, adjusts curvature");
  RNA_DEF_CURVEBONE_UPDATE(prop, is_posebone, is_editbone);

  prop = RNA_def_property(srna, "bbone_curveinz", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "curve_in_z");
  RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, RNA_TRANSLATION_PREC_DEFAULT);
  RNA_def_property_ui_text(
      prop, "In Z", "Z-axis handle offset for start of the B-Bone's curve, adjusts curvature");
  RNA_DEF_CURVEBONE_UPDATE(prop, is_posebone, is_editbone);

  prop = RNA_def_property(srna, "bbone_curveoutx", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "curve_out_x");
  RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, RNA_TRANSLATION_PREC_DEFAULT);
  RNA_def_property_ui_text(
      prop, "Out X", "X-axis handle offset for end of the B-Bone's curve, adjusts curvature");
  RNA_DEF_CURVEBONE_UPDATE(prop, is_posebone, is_editbone);

  prop = RNA_def_property(srna, "bbone_curveoutz", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "curve_out_z");
  RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, RNA_TRANSLATION_PREC_DEFAULT);
  RNA_def_property_ui_text(
      prop, "Out Z", "Z-axis handle offset for end of the B-Bone's curve, adjusts curvature");
  RNA_DEF_CURVEBONE_UPDATE(prop, is_posebone, is_editbone);

  /* Ease In/Out */
  prop = RNA_def_property(srna, "bbone_easein", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "ease1");
  RNA_def_property_ui_range(prop, -5.0f, 5.0f, 1, 3);
  RNA_def_property_float_default(prop, is_posebone ? 0.0f : 1.0f);
  RNA_def_property_ui_text(prop, "Ease In", "Length of first Bézier Handle (for B-Bones only)");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_ARMATURE);
  RNA_DEF_CURVEBONE_UPDATE(prop, is_posebone, is_editbone);

  prop = RNA_def_property(srna, "bbone_easeout", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "ease2");
  RNA_def_property_ui_range(prop, -5.0f, 5.0f, 1, 3);
  RNA_def_property_float_default(prop, is_posebone ? 0.0f : 1.0f);
  RNA_def_property_ui_text(prop, "Ease Out", "Length of second Bézier Handle (for B-Bones only)");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_ARMATURE);
  RNA_DEF_CURVEBONE_UPDATE(prop, is_posebone, is_editbone);

  if (is_posebone == false) {
    prop = RNA_def_property(srna, "use_scale_easing", PROP_BOOLEAN, PROP_NONE);
    RNA_def_property_ui_text(
        prop, "Scale Easing", "Multiply the final easing values by the Scale In/Out Y factors");
    RNA_def_property_boolean_sdna(prop, nullptr, "bbone_flag", BBONE_SCALE_EASING);
    RNA_DEF_CURVEBONE_UPDATE(prop, is_posebone, is_editbone);
  }

  /* Scale In/Out */
  prop = RNA_def_property(srna, "bbone_scalein", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_float_sdna(prop, nullptr, "scale_in");
  RNA_def_property_array(prop, 3);
  RNA_def_property_flag(prop, PROP_PROPORTIONAL);
  RNA_def_property_ui_range(prop, 0.0f, FLT_MAX, 1, 3);
  RNA_def_property_float_array_default(prop, rna_default_scale_3d);
  RNA_def_property_ui_text(
      prop,
      "Scale In",
      "Scale factors for the start of the B-Bone, adjusts thickness (for tapering effects)");
  RNA_DEF_CURVEBONE_UPDATE(prop, is_posebone, is_editbone);

  prop = RNA_def_property(srna, "bbone_scaleout", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_float_sdna(prop, nullptr, "scale_out");
  RNA_def_property_array(prop, 3);
  RNA_def_property_flag(prop, PROP_PROPORTIONAL);
  RNA_def_property_ui_range(prop, 0.0f, FLT_MAX, 1, 3);
  RNA_def_property_float_array_default(prop, rna_default_scale_3d);
  RNA_def_property_ui_text(
      prop,
      "Scale Out",
      "Scale factors for the end of the B-Bone, adjusts thickness (for tapering effects)");
  RNA_DEF_CURVEBONE_UPDATE(prop, is_posebone, is_editbone);

#  undef RNA_DEF_CURVEBONE_UPDATE
}

static void rna_def_bone_common(StructRNA *srna, int editbone)
{
  static const EnumPropertyItem prop_bbone_handle_type[] = {
      {BBONE_HANDLE_AUTO,
       "AUTO",
       0,
       "Automatic",
       "Use connected parent and children to compute the handle"},
      {BBONE_HANDLE_ABSOLUTE,
       "ABSOLUTE",
       0,
       "Absolute",
       "Use the position of the specified bone to compute the handle"},
      {BBONE_HANDLE_RELATIVE,
       "RELATIVE",
       0,
       "Relative",
       "Use the offset of the specified bone from rest pose to compute the handle"},
      {BBONE_HANDLE_TANGENT,
       "TANGENT",
       0,
       "Tangent",
       "Use the orientation of the specified bone to compute the handle, ignoring the location"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem prop_bbone_mapping_mode[] = {
      {BBONE_MAPPING_STRAIGHT,
       "STRAIGHT",
       0,
       "Straight",
       "Fast mapping that is good for most situations, but ignores the rest pose "
       "curvature of the B-Bone"},
      {BBONE_MAPPING_CURVED,
       "CURVED",
       0,
       "Curved",
       "Slower mapping that gives better deformation for B-Bones that are sharply "
       "curved in rest pose"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem prop_inherit_scale_mode[] = {
      {BONE_INHERIT_SCALE_FULL, "FULL", 0, "Full", "Inherit all effects of parent scaling"},
      {BONE_INHERIT_SCALE_FIX_SHEAR,
       "FIX_SHEAR",
       0,
       "Fix Shear",
       "Inherit scaling, but remove shearing of the child in the rest orientation"},
      {BONE_INHERIT_SCALE_ALIGNED,
       "ALIGNED",
       0,
       "Aligned",
       "Rotate non-uniform parent scaling to align with the child, applying parent X "
       "scale to child X axis, and so forth"},
      {BONE_INHERIT_SCALE_AVERAGE,
       "AVERAGE",
       0,
       "Average",
       "Inherit uniform scaling representing the overall change in the volume of the parent"},
      {BONE_INHERIT_SCALE_NONE, "NONE", 0, "None", "Completely ignore parent scaling"},
      {BONE_INHERIT_SCALE_NONE_LEGACY,
       "NONE_LEGACY",
       0,
       "None (Legacy)",
       "Ignore parent scaling without compensating for parent shear. "
       "Replicates the effect of disabling the original Inherit Scale checkbox."},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem prop_drawtype_items[] = {
      {ARM_DRAW_TYPE_ARMATURE_DEFINED,
       "ARMATURE_DEFINED",
       0,
       "Armature Defined",
       "Use display mode from armature (default)"},
      {ARM_DRAW_TYPE_OCTA, "OCTAHEDRAL", 0, "Octahedral", "Display bones as octahedral shape"},
      {ARM_DRAW_TYPE_STICK, "STICK", 0, "Stick", "Display bones as simple 2D lines with dots"},
      {ARM_DRAW_TYPE_B_BONE,
       "BBONE",
       0,
       "B-Bone",
       "Display bones as boxes, showing subdivision and B-Splines"},
      {ARM_DRAW_TYPE_ENVELOPE,
       "ENVELOPE",
       0,
       "Envelope",
       "Display bones as extruded spheres, showing deformation influence volume"},
      {ARM_DRAW_TYPE_WIRE,
       "WIRE",
       0,
       "Wire",
       "Display bones as thin wires, showing subdivision and B-Splines"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  PropertyRNA *prop;

  /* strings */
  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "name");
  RNA_def_property_ui_text(prop, "Name", "");
  RNA_def_struct_name_property(srna, prop);
  if (editbone) {
    RNA_def_property_string_funcs(prop, nullptr, nullptr, "rna_EditBone_name_set");
  }
  else {
    RNA_def_property_string_funcs(prop, nullptr, nullptr, "rna_Bone_name_set");
  }
  RNA_def_property_update(prop, 0, "rna_Bone_update_renamed");

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "color", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "BoneColor");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  if (editbone) {
    RNA_def_property_pointer_funcs(prop, "rna_EditBone_color_get", nullptr, nullptr, nullptr);
  }

  prop = RNA_def_property(srna, "display_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "drawtype");
  RNA_def_property_enum_items(prop, prop_drawtype_items);
  RNA_def_property_ui_text(prop, "Display Type", "");
  RNA_def_property_update(prop, 0, "rna_Armature_redraw_data");
  RNA_def_property_flag(prop, PROP_LIB_EXCEPTION);

  /* flags */
  prop = RNA_def_property(srna, "use_connect", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", BONE_CONNECTED);
  if (editbone) {
    RNA_def_property_boolean_funcs(prop, nullptr, "rna_EditBone_connected_set");
  }
  else {
    RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  }
  RNA_def_property_ui_text(
      prop, "Connected", "When bone has a parent, bone's head is stuck to the parent's tail");
  RNA_def_property_update(prop, 0, "rna_Armature_update_data");

  prop = RNA_def_property(srna, "use_inherit_rotation", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "flag", BONE_HINGE);
  RNA_def_property_ui_text(
      prop, "Inherit Rotation", "Bone inherits rotation or scale from parent bone");
  RNA_def_property_update(prop, 0, "rna_Armature_update_data");

  prop = RNA_def_property(srna, "use_envelope_multiply", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", BONE_MULT_VG_ENV);
  RNA_def_property_ui_text(
      prop,
      "Multiply Vertex Group with Envelope",
      "When deforming bone, multiply effects of Vertex Group weights with Envelope influence");
  RNA_def_property_update(prop, 0, "rna_Armature_update_data");

  prop = RNA_def_property(srna, "use_deform", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "flag", BONE_NO_DEFORM);
  RNA_def_property_ui_text(prop, "Deform", "Enable Bone to deform geometry");
  RNA_def_property_update(prop, 0, "rna_Armature_update_data");

  prop = RNA_def_property(srna, "inherit_scale", PROP_ENUM, PROP_NONE);
  RNA_def_property_ui_text(
      prop, "Inherit Scale", "Specifies how the bone inherits scaling from the parent bone");
  RNA_def_property_enum_sdna(prop, nullptr, "inherit_scale_mode");
  RNA_def_property_enum_items(prop, prop_inherit_scale_mode);
  RNA_def_property_update(prop, 0, "rna_Armature_update_data");

  prop = RNA_def_property(srna, "use_local_location", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_ui_text(prop, "Local Location", "Bone location is set in local space");
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "flag", BONE_NO_LOCAL_LOCATION);
  RNA_def_property_update(prop, 0, "rna_Armature_update_data");

  prop = RNA_def_property(srna, "use_relative_parent", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_ui_text(
      prop, "Relative Parenting", "Object children will use relative transform, like deform");
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", BONE_RELATIVE_PARENTING);
  RNA_def_property_update(prop, 0, "rna_Armature_update_data");

  prop = RNA_def_property(srna, "show_wire", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", BONE_DRAWWIRE);
  RNA_def_property_ui_text(
      prop,
      "Display Wire",
      "Bone is always displayed in wireframe regardless of viewport shading mode "
      "(useful for non-obstructive custom bone shapes)");
  RNA_def_property_update(prop, 0, "rna_Armature_redraw_data");

  /* XXX: use_cyclic_offset is deprecated in 2.5. May/may not return */
  prop = RNA_def_property(srna, "use_cyclic_offset", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "flag", BONE_NO_CYCLICOFFSET);
  RNA_def_property_ui_text(
      prop,
      "Cyclic Offset",
      "When bone does not have a parent, it receives cyclic offset effects (Deprecated)");
  RNA_def_property_update(prop, 0, "rna_Armature_update_data");

  prop = RNA_def_property(srna, "hide_select", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", BONE_UNSELECTABLE);
  RNA_def_property_ui_text(prop, "Selectable", "Bone is able to be selected");
  if (editbone) {
    RNA_def_property_update(prop, 0, "rna_EditBone_hide_update");
  }
  else {
    RNA_def_property_update(prop, 0, "rna_Bone_hide_update");
  }

  /* Number values */
  /* envelope deform settings */
  prop = RNA_def_property(srna, "envelope_distance", PROP_FLOAT, PROP_DISTANCE);
  if (editbone) {
    RNA_def_property_update(prop, 0, "rna_Armature_editbone_transform_update");
  }
  else {
    RNA_def_property_update(prop, 0, "rna_Armature_update_data");
  }
  RNA_def_property_float_sdna(prop, nullptr, "dist");
  RNA_def_property_range(prop, 0.0f, 1000.0f);
  RNA_def_property_ui_text(
      prop, "Envelope Deform Distance", "Bone deformation distance (for Envelope deform only)");

  prop = RNA_def_property(srna, "envelope_weight", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "weight");
  RNA_def_property_range(prop, 0.0f, 1000.0f);
  RNA_def_property_ui_text(
      prop, "Envelope Deform Weight", "Bone deformation weight (for Envelope deform only)");
  RNA_def_property_update(prop, 0, "rna_Armature_update_data");

  prop = RNA_def_property(srna, "head_radius", PROP_FLOAT, PROP_DISTANCE);
  if (editbone) {
    RNA_def_property_update(prop, 0, "rna_Armature_editbone_transform_update");
  }
  else {
    RNA_def_property_update(prop, 0, "rna_Armature_update_data");
  }
  RNA_def_property_float_sdna(prop, nullptr, "rad_head");
  /* XXX: range is 0 to limit, where `limit = 10000.0f * std::max(1.0, view3d->grid)`. */
  // RNA_def_property_range(prop, 0, 1000);
  RNA_def_property_ui_range(prop, 0.01, 100, 0.1, 3);
  RNA_def_property_ui_text(
      prop, "Envelope Head Radius", "Radius of head of bone (for Envelope deform only)");

  prop = RNA_def_property(srna, "tail_radius", PROP_FLOAT, PROP_DISTANCE);
  if (editbone) {
    RNA_def_property_update(prop, 0, "rna_Armature_editbone_transform_update");
  }
  else {
    RNA_def_property_update(prop, 0, "rna_Armature_update_data");
  }
  RNA_def_property_float_sdna(prop, nullptr, "rad_tail");
  /* XXX range is 0 to limit, where limit = `10000.0f * std::max(1.0, view3d->grid)`. */
  // RNA_def_property_range(prop, 0, 1000);
  RNA_def_property_ui_range(prop, 0.01, 100, 0.1, 3);
  RNA_def_property_ui_text(
      prop, "Envelope Tail Radius", "Radius of tail of bone (for Envelope deform only)");

  /* b-bones deform settings */
  prop = RNA_def_property(srna, "bbone_segments", PROP_INT, PROP_NONE);
  if (editbone) {
    RNA_def_property_update(prop, 0, "rna_Armature_editbone_transform_update");
  }
  else {
    RNA_def_property_update(prop, 0, "rna_Armature_dependency_update");
  }
  RNA_def_property_int_sdna(prop, nullptr, "segments");
  RNA_def_property_range(prop, 1, 32);
  RNA_def_property_ui_text(
      prop, "B-Bone Segments", "Number of subdivisions of bone (for B-Bones only)");

  prop = RNA_def_property(srna, "bbone_mapping_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "bbone_mapping_mode");
  RNA_def_property_enum_items(prop, prop_bbone_mapping_mode);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(
      prop,
      "B-Bone Vertex Mapping Mode",
      "Selects how the vertices are mapped to B-Bone segments based on their position");
  RNA_def_property_update(prop, 0, "rna_Armature_update_data");

  prop = RNA_def_property(srna, "bbone_x", PROP_FLOAT, PROP_NONE);
  if (editbone) {
    RNA_def_property_update(prop, 0, "rna_Armature_editbone_transform_update");
  }
  else {
    RNA_def_property_update(prop, 0, "rna_Armature_update_data");
  }
  RNA_def_property_float_sdna(prop, nullptr, "xwidth");
  RNA_def_property_ui_range(prop, 0.0f, 1000.0f, 1, RNA_TRANSLATION_PREC_DEFAULT);
  RNA_def_property_ui_text(prop, "B-Bone Display X Width", "B-Bone X size");

  prop = RNA_def_property(srna, "bbone_z", PROP_FLOAT, PROP_NONE);
  if (editbone) {
    RNA_def_property_update(prop, 0, "rna_Armature_editbone_transform_update");
  }
  else {
    RNA_def_property_update(prop, 0, "rna_Armature_update_data");
  }
  RNA_def_property_float_sdna(prop, nullptr, "zwidth");
  RNA_def_property_ui_range(prop, 0.0f, 1000.0f, 1, RNA_TRANSLATION_PREC_DEFAULT);
  RNA_def_property_ui_text(prop, "B-Bone Display Z Width", "B-Bone Z size");

  /* B-Bone Start Handle settings. */
  prop = RNA_def_property(srna, "bbone_handle_type_start", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "bbone_prev_type");
  RNA_def_property_enum_items(prop, prop_bbone_handle_type);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(
      prop, "B-Bone Start Handle Type", "Selects how the start handle of the B-Bone is computed");
  RNA_def_property_update(prop, 0, "rna_Armature_dependency_update");

  prop = RNA_def_property(srna, "bbone_custom_handle_start", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "bbone_prev");
  RNA_def_property_struct_type(prop, editbone ? "EditBone" : "Bone");
  if (editbone) {
    RNA_def_property_pointer_funcs(
        prop, "rna_EditBone_bbone_prev_get", "rna_EditBone_bbone_prev_set", nullptr, nullptr);
    RNA_def_property_update(prop, 0, "rna_Armature_dependency_update");
  }
  else {
    RNA_def_property_pointer_funcs(prop, nullptr, "rna_Bone_bbone_prev_set", nullptr, nullptr);
    RNA_def_property_update(prop, 0, "rna_Bone_bbone_handle_update");
  }
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_PTR_NO_OWNERSHIP);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_NO_COMPARISON);
  RNA_def_property_ui_text(
      prop, "B-Bone Start Handle", "Bone that serves as the start handle for the B-Bone curve");

  prop = RNA_def_property(srna, "bbone_handle_use_scale_start", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_ui_text(
      prop,
      "Start Handle Scale",
      "Multiply B-Bone Scale In channels by the local scale values of the start handle. "
      "This is done after the Scale Easing option and isn't affected by it.");
  RNA_def_property_boolean_bitset_array_sdna(
      prop, nullptr, "bbone_prev_flag", BBONE_HANDLE_SCALE_X, 3);
  RNA_def_property_update(prop, 0, "rna_Armature_update_data");

  prop = RNA_def_property(srna, "bbone_handle_use_ease_start", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_ui_text(
      prop,
      "Start Handle Ease",
      "Multiply the B-Bone Ease In channel by the local Y scale value of the start handle. "
      "This is done after the Scale Easing option and isn't affected by it.");
  RNA_def_property_boolean_sdna(prop, nullptr, "bbone_prev_flag", BBONE_HANDLE_SCALE_EASE);
  RNA_def_property_update(prop, 0, "rna_Armature_update_data");

  /* B-Bone End Handle settings. */
  prop = RNA_def_property(srna, "bbone_handle_type_end", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "bbone_next_type");
  RNA_def_property_enum_items(prop, prop_bbone_handle_type);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(
      prop, "B-Bone End Handle Type", "Selects how the end handle of the B-Bone is computed");
  RNA_def_property_update(prop, 0, "rna_Armature_dependency_update");

  prop = RNA_def_property(srna, "bbone_custom_handle_end", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "bbone_next");
  RNA_def_property_struct_type(prop, editbone ? "EditBone" : "Bone");
  if (editbone) {
    RNA_def_property_pointer_funcs(
        prop, "rna_EditBone_bbone_next_get", "rna_EditBone_bbone_next_set", nullptr, nullptr);
    RNA_def_property_update(prop, 0, "rna_Armature_dependency_update");
  }
  else {
    RNA_def_property_pointer_funcs(prop, nullptr, "rna_Bone_bbone_next_set", nullptr, nullptr);
    RNA_def_property_update(prop, 0, "rna_Bone_bbone_handle_update");
  }
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_PTR_NO_OWNERSHIP);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_NO_COMPARISON);
  RNA_def_property_ui_text(
      prop, "B-Bone End Handle", "Bone that serves as the end handle for the B-Bone curve");

  prop = RNA_def_property(srna, "bbone_handle_use_scale_end", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_ui_text(
      prop,
      "End Handle Scale",
      "Multiply B-Bone Scale Out channels by the local scale values of the end handle. "
      "This is done after the Scale Easing option and isn't affected by it.");
  RNA_def_property_boolean_bitset_array_sdna(
      prop, nullptr, "bbone_next_flag", BBONE_HANDLE_SCALE_X, 3);
  RNA_def_property_update(prop, 0, "rna_Armature_update_data");

  prop = RNA_def_property(srna, "bbone_handle_use_ease_end", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_ui_text(
      prop,
      "End Handle Ease",
      "Multiply the B-Bone Ease Out channel by the local Y scale value of the end handle. "
      "This is done after the Scale Easing option and isn't affected by it.");
  RNA_def_property_boolean_sdna(prop, nullptr, "bbone_next_flag", BBONE_HANDLE_SCALE_EASE);
  RNA_def_property_update(prop, 0, "rna_Armature_update_data");

  RNA_define_lib_overridable(false);
}

/** Bone.collections collection-of-bone-collections interface. */
static void rna_def_bone_collection_memberships(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  FunctionRNA *func;

  RNA_def_property_srna(cprop, "BoneCollectionMemberships");
  srna = RNA_def_struct(brna, "BoneCollectionMemberships", nullptr);
  RNA_def_struct_sdna(srna, "Bone");
  RNA_def_struct_ui_text(
      srna, "Bone Collection Memberships", "The Bone Collections that contain this Bone");

  /* Bone.collections.clear(...) */
  func = RNA_def_function(srna, "clear", "rna_BoneCollectionMemberships_clear");
  RNA_def_function_ui_description(func, "Remove this bone from all bone collections");
}

/* Err... bones should not be directly edited (only edit-bones should be...). */
static void rna_def_bone(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "Bone", nullptr);
  RNA_def_struct_ui_text(srna, "Bone", "Bone in an Armature data-block");
  RNA_def_struct_ui_icon(srna, ICON_BONE_DATA);
  RNA_def_struct_path_func(srna, "rna_Bone_path");
  RNA_def_struct_idprops_func(srna, "rna_Bone_idprops");
  RNA_def_struct_system_idprops_func(srna, "rna_Bone_system_idprops");

  /* pointers/collections */
  /* parent (pointer) */
  prop = RNA_def_property(srna, "parent", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "Bone");
  RNA_def_property_pointer_sdna(prop, nullptr, "parent");
  RNA_def_property_flag(prop, PROP_PTR_NO_OWNERSHIP);
  RNA_def_property_ui_text(prop, "Parent", "Parent bone (in same Armature)");
  RNA_def_property_update(prop, 0, "rna_Armature_redraw_data");

  /* children (collection) */
  prop = RNA_def_property(srna, "children", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "childbase", nullptr);
  RNA_def_property_struct_type(prop, "Bone");
  RNA_def_property_flag(prop, PROP_PTR_NO_OWNERSHIP);
  RNA_def_property_ui_text(prop, "Children", "Bones which are children of this bone");

  prop = RNA_def_property(srna, "collections", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "BoneCollection");
  RNA_def_property_collection_funcs(prop,
                                    "rna_Bone_collections_begin",
                                    "rna_iterator_listbase_next",
                                    "rna_iterator_listbase_end",
                                    "rna_Bone_collections_get",
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr);
  RNA_def_property_flag(prop, PROP_PTR_NO_OWNERSHIP);
  RNA_def_property_ui_text(prop, "Collections", "Bone Collections that contain this bone");
  rna_def_bone_collection_memberships(brna, prop);

  rna_def_bone_common(srna, 0);
  rna_def_bone_curved_common(srna, false, false);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "hide", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", BONE_HIDDEN_A);
  RNA_def_property_ui_text(prop, "Hide", "Bone is not visible when it is in Edit Mode");
  RNA_def_property_ui_icon(prop, ICON_RESTRICT_VIEW_OFF, -1);
  RNA_def_property_update(prop, 0, "rna_Bone_hide_update");

  /* XXX better matrix descriptions possible (Arystan) */
  prop = RNA_def_property(srna, "matrix", PROP_FLOAT, PROP_MATRIX);
  RNA_def_property_float_sdna(prop, nullptr, "bone_mat");
  RNA_def_property_multi_array(prop, 2, rna_matrix_dimsize_3x3);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(
      prop, "Bone Matrix", "3" BLI_STR_UTF8_MULTIPLICATION_SIGN "3 bone matrix");

  prop = RNA_def_property(srna, "matrix_local", PROP_FLOAT, PROP_MATRIX);
  RNA_def_property_float_sdna(prop, nullptr, "arm_mat");
  RNA_def_property_multi_array(prop, 2, rna_matrix_dimsize_4x4);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop,
                           "Bone Armature-Relative Matrix",
                           "4" BLI_STR_UTF8_MULTIPLICATION_SIGN
                           "4 bone matrix relative to armature");

  prop = RNA_def_property(srna, "tail", PROP_FLOAT, PROP_TRANSLATION);
  RNA_def_property_float_sdna(prop, nullptr, "tail");
  RNA_def_property_array(prop, 3);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(
      prop, "Tail", "Location of tail end of the bone relative to its parent");
  RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, RNA_TRANSLATION_PREC_DEFAULT);

  prop = RNA_def_property(srna, "tail_local", PROP_FLOAT, PROP_TRANSLATION);
  RNA_def_property_float_sdna(prop, nullptr, "arm_tail");
  RNA_def_property_array(prop, 3);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(
      prop, "Armature-Relative Tail", "Location of tail end of the bone relative to armature");
  RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, RNA_TRANSLATION_PREC_DEFAULT);

  prop = RNA_def_property(srna, "head", PROP_FLOAT, PROP_TRANSLATION);
  RNA_def_property_float_sdna(prop, nullptr, "head");
  RNA_def_property_array(prop, 3);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(
      prop, "Head", "Location of head end of the bone relative to its parent");
  RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, RNA_TRANSLATION_PREC_DEFAULT);

  prop = RNA_def_property(srna, "head_local", PROP_FLOAT, PROP_TRANSLATION);
  RNA_def_property_float_sdna(prop, nullptr, "arm_head");
  RNA_def_property_array(prop, 3);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(
      prop, "Armature-Relative Head", "Location of head end of the bone relative to armature");
  RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, RNA_TRANSLATION_PREC_DEFAULT);

  prop = RNA_def_property(srna, "length", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, nullptr, "length");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Length", "Length of the bone");

  RNA_define_lib_overridable(false);

  RNA_api_bone(srna);
}

static void rna_def_edit_bone(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "EditBone", nullptr);
  RNA_def_struct_sdna(srna, "EditBone");
  RNA_def_struct_path_func(srna, "rna_EditBone_path");
  RNA_def_struct_idprops_func(srna, "rna_EditBone_idprops");
  RNA_def_struct_system_idprops_func(srna, "rna_EditBone_system_idprops");
  RNA_def_struct_ui_text(srna, "Edit Bone", "Edit mode bone in an armature data-block");
  RNA_def_struct_ui_icon(srna, ICON_BONE_DATA);

  prop = RNA_def_property(srna, "collections", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "BoneCollection");
  RNA_def_property_collection_funcs(prop,
                                    "rna_EditBone_collections_begin",
                                    "rna_iterator_listbase_next",
                                    "rna_iterator_listbase_end",
                                    "rna_Bone_collections_get",
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr);
  RNA_def_property_flag(prop, PROP_PTR_NO_OWNERSHIP);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Collections", "Bone Collections that contain this bone");

  RNA_define_verify_sdna(false); /* not in sdna */

  prop = RNA_def_property(srna, "parent", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "EditBone");
  RNA_def_property_pointer_funcs(
      prop, "rna_EditBone_parent_get", "rna_EditBone_parent_set", nullptr, nullptr);
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Parent", "Parent edit bone (in same Armature)");
  RNA_def_property_update(prop, 0, "rna_Armature_redraw_data");

  prop = RNA_def_property(srna, "roll", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, nullptr, "roll");
  RNA_def_property_ui_range(prop, -M_PI * 2, M_PI * 2, 10, 2);
  RNA_def_property_ui_text(prop, "Roll", "Bone rotation around head-tail axis");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, 0, "rna_Armature_editbone_transform_update");

  prop = RNA_def_property(srna, "head", PROP_FLOAT, PROP_TRANSLATION);
  RNA_def_property_float_sdna(prop, nullptr, "head");
  RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 10, RNA_TRANSLATION_PREC_DEFAULT);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Head", "Location of head end of the bone");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, 0, "rna_Armature_editbone_transform_update");

  prop = RNA_def_property(srna, "tail", PROP_FLOAT, PROP_TRANSLATION);
  RNA_def_property_float_sdna(prop, nullptr, "tail");
  RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 10, RNA_TRANSLATION_PREC_DEFAULT);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Tail", "Location of tail end of the bone");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, 0, "rna_Armature_editbone_transform_update");

  prop = RNA_def_property(srna, "length", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_funcs(
      prop, "rna_EditBone_length_get", "rna_EditBone_length_set", nullptr);
  RNA_def_property_range(prop, 0, FLT_MAX);
  RNA_def_property_ui_range(prop, 0, FLT_MAX, 1, RNA_TRANSLATION_PREC_DEFAULT);
  RNA_def_property_ui_text(prop, "Length", "Length of the bone. Changing moves the tail end.");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, 0, "rna_Armature_editbone_transform_update");

  rna_def_bone_common(srna, 1);
  rna_def_bone_curved_common(srna, false, true);

  prop = RNA_def_property(srna, "hide", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", BONE_HIDDEN_A);
  RNA_def_property_ui_text(prop, "Hide", "Bone is not visible when in Edit Mode");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, 0, "rna_EditBone_hide_update");

  prop = RNA_def_property(srna, "lock", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", BONE_EDITMODE_LOCKED);
  RNA_def_property_ui_text(prop, "Lock", "Bone is not able to be transformed when in Edit Mode");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, 0, "rna_Armature_redraw_data");

  prop = RNA_def_property(srna, "select", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", BONE_SELECTED);
  RNA_def_property_ui_text(prop, "Select", "");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, 0, "rna_Armature_redraw_data");

  prop = RNA_def_property(srna, "select_head", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", BONE_ROOTSEL);
  RNA_def_property_ui_text(prop, "Head Select", "");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, 0, "rna_Armature_redraw_data");

  prop = RNA_def_property(srna, "select_tail", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", BONE_TIPSEL);
  RNA_def_property_ui_text(prop, "Tail Select", "");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, 0, "rna_Armature_redraw_data");

  /* calculated and read only, not actual data access */
  prop = RNA_def_property(srna, "matrix", PROP_FLOAT, PROP_MATRIX);
  // RNA_def_property_float_sdna(prop, nullptr, ""); /* Doesn't access any real data. */
  RNA_def_property_multi_array(prop, 2, rna_matrix_dimsize_4x4);
  // RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_flag(prop, PROP_THICK_WRAP); /* no reference to original data */
  RNA_def_property_ui_text(
      prop,
      "Edit Bone Matrix",
      "Matrix combining location and rotation of the bone (head position, direction and roll), "
      "in armature space (does not include/support bone's length/size)");
  RNA_def_property_float_funcs(
      prop, "rna_EditBone_matrix_get", "rna_EditBone_matrix_set", nullptr);

  RNA_api_armature_edit_bone(srna);

  RNA_define_verify_sdna(true);
}

/* `armature.bones.*`. */
static void rna_def_armature_bones(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  PropertyRNA *prop;

  // FunctionRNA *func;
  // PropertyRNA *parm;

  RNA_def_property_srna(cprop, "ArmatureBones");
  srna = RNA_def_struct(brna, "ArmatureBones", nullptr);
  RNA_def_struct_sdna(srna, "bArmature");
  RNA_def_struct_ui_text(srna, "Armature Bones", "Collection of armature bones");

  prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "Bone");
  RNA_def_property_pointer_sdna(prop, nullptr, "act_bone");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Active Bone", "Armature's active bone");
  RNA_def_property_pointer_funcs(prop, nullptr, "rna_Armature_act_bone_set", nullptr, nullptr);
  RNA_def_property_update(prop, 0, "rna_Armature_update");

  /* TODO: redraw. */
  // RNA_def_property_collection_active(prop, prop_act);
}

/* `armature.bones.*`. */
static void rna_def_armature_edit_bones(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  PropertyRNA *prop;

  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "ArmatureEditBones");
  srna = RNA_def_struct(brna, "ArmatureEditBones", nullptr);
  RNA_def_struct_sdna(srna, "bArmature");
  RNA_def_struct_ui_text(srna, "Armature EditBones", "Collection of armature edit bones");

  prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "EditBone");
  RNA_def_property_pointer_sdna(prop, nullptr, "act_edbone");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Active EditBone", "Armatures active edit bone");
  RNA_def_property_update(prop, 0, "rna_Armature_update");
  RNA_def_property_pointer_funcs(
      prop, nullptr, "rna_Armature_act_edit_bone_set", nullptr, nullptr);

  /* TODO: redraw. */
  // RNA_def_property_collection_active(prop, prop_act);

  /* add target */
  func = RNA_def_function(srna, "new", "rna_Armature_edit_bone_new");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  RNA_def_function_ui_description(func, "Add a new bone");
  parm = RNA_def_string(func, "name", "Object", 0, "", "New name for the bone");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  /* return type */
  parm = RNA_def_pointer(func, "bone", "EditBone", "", "Newly created edit bone");
  RNA_def_function_return(func, parm);

  /* remove target */
  func = RNA_def_function(srna, "remove", "rna_Armature_edit_bone_remove");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  RNA_def_function_ui_description(func, "Remove an existing bone from the armature");
  /* Target to remove. */
  parm = RNA_def_pointer(func, "bone", "EditBone", "", "EditBone to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));
}

/** Armature.collections collection-of-bone-collections interface. */
static void rna_def_armature_collections(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  PropertyRNA *prop;

  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "BoneCollections");
  srna = RNA_def_struct(brna, "BoneCollections", nullptr);
  RNA_def_struct_sdna(srna, "bArmature");
  RNA_def_struct_ui_text(
      srna, "Armature Bone Collections", "The Bone Collections of this Armature");

  prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "BoneCollection");
  RNA_def_property_pointer_sdna(prop, nullptr, "runtime.active_collection");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_IGNORE);
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_pointer_funcs(
      prop, nullptr, "rna_BoneCollections_active_set", nullptr, nullptr);
  RNA_def_property_ui_text(prop, "Active Collection", "Armature's active bone collection");
  RNA_def_property_update(prop, NC_OBJECT | ND_BONE_COLLECTION, nullptr);

  prop = RNA_def_property(srna, "active_index", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "runtime.active_collection_index");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_IGNORE);
  RNA_def_property_flag(prop, PROP_LIB_EXCEPTION);
  RNA_def_property_ui_text(
      prop,
      "Active Collection Index",
      "The index of the Armature's active bone collection; -1 when there "
      "is no active collection. Note that this is indexing the underlying array of bone "
      "collections, which may not be in the order you expect. Root collections are listed first, "
      "and siblings are always sequential. Apart from that, bone collections can be in any order, "
      "and thus incrementing or decrementing this index can make the active bone collection jump "
      "around in unexpected ways. For a more predictable interface, use ``active`` or "
      "``active_name``.");
  RNA_def_property_int_funcs(prop,
                             "rna_BoneCollections_active_index_get",
                             "rna_BoneCollections_active_index_set",
                             "rna_BoneCollections_active_index_range");
  RNA_def_property_update(prop, NC_OBJECT | ND_BONE_COLLECTION, nullptr);

  prop = RNA_def_property(srna, "active_name", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "active_collection_name");
  /* TODO: For some reason the overrides system doesn't register a new operation when this property
   * changes. Needs further investigation to figure out why & fix it. */
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop,
                           "Active Collection Name",
                           "The name of the Armature's active bone collection; empty when there "
                           "is no active collection");
  RNA_def_property_string_funcs(prop, nullptr, nullptr, "rna_BoneCollections_active_name_set");
  RNA_def_property_update(prop, NC_OBJECT | ND_BONE_COLLECTION, nullptr);

  prop = RNA_def_property(srna, "is_solo_active", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", ARM_BCOLL_SOLO_ACTIVE);
  RNA_def_property_ui_text(
      prop,
      "Solo Active",
      "Read-only flag that indicates there is at least one bone collection marked as 'solo'");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  /* Armature.collections.new(...) */
  func = RNA_def_function(srna, "new", "rna_BoneCollections_new");
  RNA_def_function_ui_description(func, "Add a new empty bone collection to the armature");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_string(func,
                        "name",
                        nullptr,
                        0,
                        "Name",
                        "Name of the new collection. Blender will ensure it is unique within the "
                        "collections of the Armature.");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_pointer(
      func,
      "parent",
      "BoneCollection",
      "Parent Collection",
      "If not None, the new bone collection becomes a child of this collection");
  /* Return value. */
  parm = RNA_def_pointer(
      func, "bonecollection", "BoneCollection", "", "Newly created bone collection");
  RNA_def_function_return(func, parm);

  /* Armature.collections.remove(...) */
  func = RNA_def_function(srna, "remove", "ANIM_armature_bonecoll_remove");
  RNA_def_function_ui_description(
      func,
      "Remove the bone collection from the armature. If this bone collection has any children, "
      "they will be reassigned to their grandparent; in other words, the children will take the "
      "place of the removed bone collection.");
  parm = RNA_def_pointer(func,
                         "bone_collection",
                         "BoneCollection",
                         "Bone Collection",
                         "The bone collection to remove");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  /* Armature.collections.move(...) */
  func = RNA_def_function(srna, "move", "rna_BoneCollections_move");
  RNA_def_function_ui_description(func,
                                  "Move a bone collection to a different position in the "
                                  "collection list. This can only be used to reorder siblings, "
                                  "and not to change parent-child relationships.");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_int(
      func, "from_index", -1, INT_MIN, INT_MAX, "From Index", "Index to move", 0, 10000);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_int(func, "to_index", -1, INT_MIN, INT_MAX, "To Index", "Target index", 0, 10000);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
}

static void rna_def_armature(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  FunctionRNA *func;
  PropertyRNA *parm;

  static const EnumPropertyItem prop_drawtype_items[] = {
      {ARM_DRAW_TYPE_OCTA,
       "OCTAHEDRAL",
       0,
       "Octahedral",
       "Display bones as octahedral shape (default)"},
      {ARM_DRAW_TYPE_STICK, "STICK", 0, "Stick", "Display bones as simple 2D lines with dots"},
      {ARM_DRAW_TYPE_B_BONE,
       "BBONE",
       0,
       "B-Bone",
       "Display bones as boxes, showing subdivision and B-Splines"},
      {ARM_DRAW_TYPE_ENVELOPE,
       "ENVELOPE",
       0,
       "Envelope",
       "Display bones as extruded spheres, showing deformation influence volume"},
      {ARM_DRAW_TYPE_WIRE,
       "WIRE",
       0,
       "Wire",
       "Display bones as thin wires, showing subdivision and B-Splines"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem prop_pose_position_items[] = {
      {0, "POSE", 0, "Pose Position", "Show armature in posed state"},
      {ARM_RESTPOS,
       "REST",
       0,
       "Rest Position",
       "Show Armature in binding pose state (no posing possible)"},
      {0, nullptr, 0, nullptr, nullptr},
  };
  static const EnumPropertyItem prop_relation_lines_items[] = {
      {0, "TAIL", 0, "Tail", "Draw the relationship line from the parent tail to the child head"},
      {1, "HEAD", 0, "Head", "Draw the relationship line from the parent head to the child head"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "Armature", "ID");
  RNA_def_struct_ui_text(
      srna,
      "Armature",
      "Armature data-block containing a hierarchy of bones, usually used for rigging characters");
  RNA_def_struct_ui_icon(srna, ICON_ARMATURE_DATA);
  RNA_def_struct_sdna(srna, "bArmature");

  func = RNA_def_function(srna, "transform", "rna_Armature_transform");
  RNA_def_function_ui_description(func, "Transform armature bones by a matrix");
  parm = RNA_def_float_matrix(func, "matrix", 4, 4, nullptr, 0.0f, 0.0f, "", "Matrix", 0.0f, 0.0f);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  /* Animation Data */
  rna_def_animdata_common(srna);

  RNA_define_lib_overridable(true);

  /* Collection Properties */
  prop = RNA_def_property(srna, "bones", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "bonebase", nullptr);
  RNA_def_property_collection_funcs(prop,
                                    nullptr,
                                    "rna_Armature_bones_next",
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    "rna_Armature_bones_lookup_string",
                                    nullptr);
  RNA_def_property_struct_type(prop, "Bone");
  RNA_def_property_ui_text(prop, "Bones", "");
  rna_def_armature_bones(brna, prop);

  prop = RNA_def_property(srna, "edit_bones", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "edbo", nullptr);
  RNA_def_property_struct_type(prop, "EditBone");
  RNA_def_property_ui_text(prop, "Edit Bones", "");
  rna_def_armature_edit_bones(brna, prop);

  /* Bone Collection properties. */
  prop = RNA_def_property(srna, "collections", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "BoneCollection");
  RNA_def_property_collection_funcs(prop,
                                    "rna_iterator_bone_collections_roots_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_dereference_get",
                                    "rna_iterator_bone_collections_roots_length",
                                    nullptr, /* TODO */
                                    nullptr, /* TODO */
                                    nullptr);
  RNA_def_property_ui_text(prop, "Bone Collections (Roots)", "");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_override_funcs(
      prop, nullptr, nullptr, "rna_Armature_collections_override_apply");
  RNA_def_property_override_flag(
      prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY | PROPOVERRIDE_LIBRARY_INSERTION);
  rna_def_armature_collections(brna, prop);

  prop = RNA_def_property(srna, "collections_all", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "BoneCollection");
  RNA_def_property_collection_funcs(prop,
                                    "rna_iterator_bone_collections_all_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_dereference_get",
                                    "rna_iterator_bone_collections_all_length",
                                    nullptr, /* TODO */
                                    nullptr, /* TODO */
                                    nullptr);
  RNA_def_property_ui_text(
      prop, "Bone Collections (All)", "List of all bone collections of the armature");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  /* Overrides on `armature.collections_all` are only there to override specific properties, like
   * is_visible.
   *
   * New Bone collections are added as overrides via the `armature.collections` (the roots)
   * property. It's up to its 'apply' function to also copy the children of a
   * library-override-added root. */
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);

  /* Enum values */
  prop = RNA_def_property(srna, "pose_position", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, nullptr, "flag");
  RNA_def_property_enum_items(prop, prop_pose_position_items);
  RNA_def_property_ui_text(
      prop, "Pose Position", "Show armature in binding pose or final posed state");
  RNA_def_property_update(prop, 0, "rna_Armature_update_data");
  RNA_def_property_flag(prop, PROP_LIB_EXCEPTION);

  prop = RNA_def_property(srna, "display_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "drawtype");
  RNA_def_property_enum_items(prop, prop_drawtype_items);
  RNA_def_property_ui_text(prop, "Display Type", "");
  RNA_def_property_update(prop, 0, "rna_Armature_redraw_data");
  RNA_def_property_flag(prop, PROP_LIB_EXCEPTION);

  /* flag */
  prop = RNA_def_property(srna, "show_axes", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", ARM_DRAWAXES);
  RNA_def_property_ui_text(prop, "Display Axes", "Display bone axes");
  RNA_def_property_update(prop, 0, "rna_Armature_redraw_data");
  RNA_def_property_flag(prop, PROP_LIB_EXCEPTION);

  prop = RNA_def_property(srna, "axes_position", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "axes_position");
  RNA_def_property_range(prop, 0.0, 1.0);
  RNA_def_property_ui_range(prop, 0.0, 1.0, 10, 1);
  RNA_def_property_ui_text(prop,
                           "Axes Position",
                           "The position for the axes on the bone. Increasing the value moves it "
                           "closer to the tip; decreasing moves it closer to the root.");
  RNA_def_property_update(prop, 0, "rna_Armature_redraw_data");

  RNA_define_verify_sdna(false); /* This property does not live in DNA. */
  prop = RNA_def_property(srna, "relation_line_position", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, prop_relation_lines_items);
  RNA_def_property_ui_text(prop,
                           "Relation Line Position",
                           "The start position of the relation lines from parent to child bones");
  RNA_def_property_update(prop, 0, "rna_Armature_redraw_data");
  RNA_def_property_flag(prop, PROP_LIB_EXCEPTION);
  RNA_def_property_enum_funcs(prop,
                              "rna_Armature_relation_line_position_get",
                              "rna_Armature_relation_line_position_set",
                              nullptr);
  RNA_define_verify_sdna(true); /* Restore default. */

  prop = RNA_def_property(srna, "show_names", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", ARM_DRAWNAMES);
  RNA_def_property_ui_text(prop, "Display Names", "Display bone names");
  RNA_def_property_update(prop, 0, "rna_Armature_redraw_data");
  RNA_def_property_flag(prop, PROP_LIB_EXCEPTION);

  prop = RNA_def_property(srna, "use_mirror_x", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", ARM_MIRROR_EDIT);
  RNA_def_property_ui_text(
      prop, "X-Axis Mirror", "Apply changes to matching bone on opposite side of X-Axis");
  RNA_def_property_update(prop, 0, "rna_Armature_redraw_data");
  RNA_def_property_flag(prop, PROP_LIB_EXCEPTION);

  prop = RNA_def_property(srna, "show_bone_custom_shapes", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "flag", ARM_NO_CUSTOM);
  RNA_def_property_ui_text(
      prop, "Display Custom Bone Shapes", "Display bones with their custom shapes");
  RNA_def_property_update(prop, 0, "rna_Armature_redraw_data");

  prop = RNA_def_property(srna, "show_bone_colors", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", ARM_COL_CUSTOM);
  RNA_def_property_ui_text(prop, "Display Bone Colors", "Display bone colors");
  RNA_def_property_update(prop, 0, "rna_Armature_redraw_data");

  prop = RNA_def_property(srna, "is_editmode", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop, "rna_Armature_is_editmode_get", nullptr);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Is Editmode", "True when used in editmode");

  RNA_define_lib_overridable(false);
}

static void rna_def_bonecollection(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "BoneCollection", nullptr);
  RNA_def_struct_ui_text(srna, "BoneCollection", "Bone collection in an Armature data-block");
  RNA_def_struct_path_func(srna, "rna_BoneCollection_path");
  RNA_def_struct_idprops_func(srna, "rna_BoneCollection_idprops");
  RNA_def_struct_system_idprops_func(srna, "rna_BoneCollection_system_idprops");

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "name");
  RNA_def_property_ui_text(prop, "Name", "Unique within the Armature");
  RNA_def_struct_name_property(srna, prop);
  RNA_def_property_string_funcs(prop, nullptr, nullptr, "rna_BoneCollection_name_set");
  RNA_def_property_update(prop, NC_OBJECT | ND_BONE_COLLECTION, nullptr);

  prop = RNA_def_property(srna, "is_expanded", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flags", BONE_COLLECTION_EXPANDED);
  RNA_def_property_ui_text(
      prop, "Expanded", "This bone collection is expanded in the bone collections tree view");
  RNA_def_property_flag(prop, PROP_LIB_EXCEPTION);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_boolean_funcs(prop, nullptr, "rna_BoneCollection_is_expanded_set");
  RNA_def_property_update(prop, NC_OBJECT | ND_BONE_COLLECTION, nullptr);

  prop = RNA_def_property(srna, "is_visible", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flags", BONE_COLLECTION_VISIBLE);
  RNA_def_property_ui_text(
      prop, "Visible", "Bones in this collection will be visible in pose/object mode");
  RNA_def_property_flag(prop, PROP_LIB_EXCEPTION);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_boolean_funcs(prop, nullptr, "rna_BoneCollection_is_visible_set");
  RNA_def_property_update(prop, NC_OBJECT | ND_BONE_COLLECTION, nullptr);

  prop = RNA_def_property(srna, "is_visible_ancestors", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flags", BONE_COLLECTION_ANCESTORS_VISIBLE);
  RNA_def_property_ui_text(prop,
                           "Ancestors Effectively Visible",
                           "True when all of the ancestors of this bone collection are marked as "
                           "visible; always True for root bone collections");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  prop = RNA_def_property(srna, "is_visible_effectively", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop, "rna_BoneCollection_is_visible_effectively_get", nullptr);
  RNA_def_property_ui_text(
      prop,
      "Effective Visibility",
      "Whether this bone collection is effectively visible in the viewport. This is True when "
      "this bone collection and all of its ancestors are visible, or when it is marked as "
      "'solo'.");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  prop = RNA_def_property(srna, "is_solo", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flags", BONE_COLLECTION_SOLO);
  RNA_def_property_ui_text(
      prop, "Solo", "Show only this bone collection, and others also marked as 'solo'");
  RNA_def_property_flag(prop, PROP_LIB_EXCEPTION);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_boolean_funcs(prop, nullptr, "rna_BoneCollection_is_solo_set");
  RNA_def_property_update(prop, NC_OBJECT | ND_BONE_COLLECTION, nullptr);

  prop = RNA_def_property(srna, "is_local_override", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flags", BONE_COLLECTION_OVERRIDE_LIBRARY_LOCAL);
  RNA_def_property_ui_text(
      prop,
      "Is Local Override",
      "This collection was added via a library override in the current blend file");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  prop = RNA_def_property(srna, "is_editable", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop, "rna_BoneCollection_is_editable_get", nullptr);
  RNA_def_property_ui_text(prop,
                           "Is Editable",
                           "This collection is owned by a local Armature, or was added via a "
                           "library override in the current blend file");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  prop = RNA_def_property(srna, "bones", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "Bone");
  RNA_def_property_collection_funcs(prop,
                                    "rna_BoneCollection_bones_begin",
                                    "rna_iterator_listbase_next",
                                    "rna_iterator_listbase_end",
                                    "rna_BoneCollection_bones_get",
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr);
  RNA_def_property_flag(prop, PROP_PTR_NO_OWNERSHIP);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop,
                           "Bones",
                           "Bones assigned to this bone collection. In armature edit mode this "
                           "will always return an empty list of bones, as the bone collection "
                           "memberships are only synchronized when exiting edit mode.");

  prop = RNA_def_property(srna, "children", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "BoneCollection");
  RNA_def_property_collection_funcs(prop,
                                    "rna_iterator_bone_collection_children_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_dereference_get",
                                    "rna_iterator_bone_collection_children_length",
                                    nullptr, /* TODO */
                                    nullptr, /* TODO */
                                    nullptr);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_NO_COMPARISON);

  prop = RNA_def_property(srna, "parent", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "BoneCollection");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_NO_COMPARISON);
  RNA_def_property_pointer_funcs(
      prop, "rna_BoneCollection_parent_get", "rna_BoneCollection_parent_set", nullptr, nullptr);
  RNA_def_property_ui_text(prop,
                           "Parent",
                           "Parent bone collection. Note that accessing this requires a scan of "
                           "all the bone collections to find the parent.");

  prop = RNA_def_property(srna, "index", PROP_INT, PROP_NONE);
  RNA_def_property_int_funcs(prop, "rna_BoneCollection_index_get", nullptr, nullptr);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_NO_COMPARISON);
  RNA_def_property_ui_text(
      prop,
      "Index",
      "Index of this bone collection in the armature.collections_all array. Note that finding "
      "this index requires a scan of all the bone collections, so do access this with care.");

  prop = RNA_def_property(srna, "child_number", PROP_INT, PROP_NONE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_NO_COMPARISON);
  RNA_def_property_int_funcs(
      prop, "rna_BoneCollection_child_number_get", "rna_BoneCollection_child_number_set", nullptr);
  RNA_def_property_ui_text(
      prop,
      "Child Number",
      "Index of this collection into its parent's list of children. Note that finding "
      "this index requires a scan of all the bone collections, so do access this with care.");

  RNA_api_bonecollection(srna);
}

void RNA_def_armature(BlenderRNA *brna)
{
  rna_def_bonecolor(brna);
  rna_def_bonecollection(brna);
  rna_def_armature(brna);
  rna_def_bone(brna);
  rna_def_edit_bone(brna);
}

#endif
