/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup animrig
 */

#include "BLI_listbase.h"
#include "BLI_map.hh"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_string_utils.hh"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "DNA_armature_types.h"

#include "MEM_guardedalloc.h"

#include "BKE_animsys.h"
#include "BKE_idprop.hh"
#include "BKE_lib_id.hh"
#include "BKE_lib_override.hh"

#include "ANIM_armature_iter.hh"
#include "ANIM_bone_collections.hh"

#include "intern/bone_collections_internal.hh"

#include <cstring>
#include <string>

using std::strcmp;

using namespace blender::animrig;

namespace {

/** Default flags for new bone collections. */
constexpr eBoneCollection_Flag default_flags = BONE_COLLECTION_VISIBLE |
                                               BONE_COLLECTION_SELECTABLE |
                                               BONE_COLLECTION_ANCESTORS_VISIBLE;
constexpr auto bonecoll_default_name = "Bones";

}  // namespace

static void ancestors_visible_update(bArmature *armature,
                                     const BoneCollection *parent_bcoll,
                                     BoneCollection *bcoll);

BoneCollection *ANIM_bonecoll_new(const char *name)
{
  if (name == nullptr || name[0] == '\0') {
    /* Use a default name if no name was given. */
    name = DATA_(bonecoll_default_name);
  }

  /* NOTE: the collection name may change after the collection is added to an
   * armature, to ensure it is unique within the armature. */
  BoneCollection *bcoll = MEM_cnew<BoneCollection>(__func__);

  STRNCPY_UTF8(bcoll->name, name);
  bcoll->flags = default_flags;

  bcoll->prop = nullptr;

  return bcoll;
}

void ANIM_bonecoll_free(BoneCollection *bcoll, const bool do_id_user_count)
{
  BLI_assert_msg(BLI_listbase_is_empty(&bcoll->bones),
                 "bone collection still has bones assigned to it, will cause dangling pointers in "
                 "bone runtime data");
  if (bcoll->prop) {
    IDP_FreeProperty_ex(bcoll->prop, do_id_user_count);
  }
  MEM_delete(bcoll);
}

/**
 * Construct the mapping from the bones to this collection.
 *
 * This assumes that the bones do not have such a pointer yet, i.e. calling this
 * twice for the same bone collection will cause duplicate pointers. */
static void add_reverse_pointers(BoneCollection *bcoll)
{
  LISTBASE_FOREACH (BoneCollectionMember *, member, &bcoll->bones) {
    BoneCollectionReference *ref = MEM_cnew<BoneCollectionReference>(__func__);
    ref->bcoll = bcoll;
    BLI_addtail(&member->bone->runtime.collections, ref);
  }
}

void ANIM_armature_runtime_refresh(bArmature *armature)
{
  ANIM_armature_runtime_free(armature);
  ANIM_armature_bonecoll_active_runtime_refresh(armature);

  /* Make sure the BONE_COLLECTION_ANCESTORS_VISIBLE flags are set correctly. */
  for (BoneCollection *bcoll : armature->collections_roots()) {
    ancestors_visible_update(armature, nullptr, bcoll);
  }

  /* Construct the bone-to-collections mapping. */
  for (BoneCollection *bcoll : armature->collections_span()) {
    add_reverse_pointers(bcoll);
  }
}

void ANIM_armature_runtime_free(bArmature *armature)
{
  /* Free the bone-to-its-collections mapping. */
  ANIM_armature_foreach_bone(&armature->bonebase,
                             [&](Bone *bone) { BLI_freelistN(&bone->runtime.collections); });
}

/**
 * Ensure the bone collection's name is unique within the armature.
 *
 * This assumes that the bone collection has already been inserted into the array.
 */
static void bonecoll_ensure_name_unique(bArmature *armature, BoneCollection *bcoll)
{
  struct DupNameCheckData {
    bArmature *arm;
    BoneCollection *bcoll;
  };

  /* Cannot capture armature & bcoll by reference in the lambda, as that would change its signature
   * and no longer be compatible with BLI_uniquename_cb(). */
  auto bonecoll_name_is_duplicate = [](void *arg, const char *name) -> bool {
    DupNameCheckData *data = static_cast<DupNameCheckData *>(arg);
    for (BoneCollection *bcoll : data->arm->collections_span()) {
      if (bcoll != data->bcoll && STREQ(bcoll->name, name)) {
        return true;
      }
    }
    return false;
  };

  DupNameCheckData check_data = {armature, bcoll};
  BLI_uniquename_cb(bonecoll_name_is_duplicate,
                    &check_data,
                    DATA_(bonecoll_default_name),
                    '.',
                    bcoll->name,
                    sizeof(bcoll->name));
}

/**
 * Inserts bcoll into armature's array of bone collections at index.
 *
 * NOTE: the specified index is where the given bone collection will end up.
 * This means, for example, that for a collection array of length N, you can
 * pass N as the index to append to the end.
 */
static void bonecoll_insert_at_index(bArmature *armature, BoneCollection *bcoll, const int index)
{
  BLI_assert(index <= armature->collection_array_num);

  armature->collection_array = (BoneCollection **)MEM_reallocN_id(
      armature->collection_array,
      sizeof(BoneCollection *) * (armature->collection_array_num + 1),
      __func__);

  /* To keep the memory consistent, insert the new element at the end of the
   * now-grown array, then rotate it into place. */
  armature->collection_array[armature->collection_array_num] = bcoll;
  armature->collection_array_num++;

  const int rotate_count = armature->collection_array_num - index - 1;
  internal::bonecolls_rotate_block(armature, index, rotate_count, +1);

  if (armature->runtime.active_collection_index >= index) {
    ANIM_armature_bonecoll_active_index_set(armature,
                                            armature->runtime.active_collection_index + 1);
  }
}

static void bonecoll_insert_as_root(bArmature *armature, BoneCollection *bcoll, int at_index)
{
  BLI_assert(at_index >= -1);
  BLI_assert(at_index <= armature->collection_root_count);
  if (at_index < 0) {
    at_index = armature->collection_root_count;
  }

  bonecoll_insert_at_index(armature, bcoll, at_index);
  armature->collection_root_count++;

  ancestors_visible_update(armature, nullptr, bcoll);
}

static int bonecoll_insert_as_child(bArmature *armature,
                                    BoneCollection *bcoll,
                                    const int parent_index)
{
  BLI_assert_msg(parent_index >= 0, "Armature bone collection index should be 0 or larger");
  BLI_assert_msg(parent_index < armature->collection_array_num,
                 "Parent bone collection index should not point beyond the end of the array");

  BoneCollection *parent = armature->collection_array[parent_index];
  if (parent->child_index == 0) {
    /* This parent doesn't have any children yet, so place them at the end of the array. */
    parent->child_index = armature->collection_array_num;
  }
  const int insert_at_index = parent->child_index + parent->child_count;
  bonecoll_insert_at_index(armature, bcoll, insert_at_index);
  parent->child_count++;

  ancestors_visible_update(armature, parent, bcoll);

  return insert_at_index;
}

BoneCollection *ANIM_armature_bonecoll_new(bArmature *armature,
                                           const char *name,
                                           const int parent_index)
{
  BoneCollection *bcoll = ANIM_bonecoll_new(name);

  if (!ID_IS_LINKED(&armature->id) && ID_IS_OVERRIDE_LIBRARY(&armature->id)) {
    /* Mark this bone collection as local override, so that certain operations can be allowed. */
    bcoll->flags |= BONE_COLLECTION_OVERRIDE_LIBRARY_LOCAL;
  }

  bonecoll_ensure_name_unique(armature, bcoll);

  if (parent_index < 0) {
    bonecoll_insert_as_root(armature, bcoll, -1);
  }
  else {
    bonecoll_insert_as_child(armature, bcoll, parent_index);
  }

  /* Restore the active bone collection pointer, as its index might have changed. */
  ANIM_armature_bonecoll_active_set(armature, armature->runtime.active_collection);

  return bcoll;
}

/**
 * Copy a BoneCollection to a new armature, updating its internal pointers to
 * point to the new armature.
 *
 * This *only* updates the cloned BoneCollection, and does *not* actually add it
 * to the armature.
 *
 * Child collections are not taken into account; the returned bone collection is
 * without children, regardless of `bcoll_to_copy`.
 */
static BoneCollection *copy_and_update_ownership(const bArmature *armature_dst,
                                                 const BoneCollection *bcoll_to_copy)
{
  BoneCollection *bcoll = static_cast<BoneCollection *>(MEM_dupallocN(bcoll_to_copy));

  /* Reset the child_index and child_count properties. These are unreliable when
   * coming from an override, as the original array might have been completely
   * reshuffled. Children will have to be copied separately. */
  bcoll->child_index = 0;
  bcoll->child_count = 0;

  if (bcoll->prop) {
    bcoll->prop = IDP_CopyProperty_ex(bcoll_to_copy->prop,
                                      0 /*do_id_user ? 0 : LIB_ID_CREATE_NO_USER_REFCOUNT*/);
  }

  /* Remap the bone pointers to the given armature, as `bcoll_to_copy` is
   * assumed to be owned by another armature. */
  BLI_duplicatelist(&bcoll->bones, &bcoll->bones);
  BLI_assert_msg(armature_dst->bonehash, "Expected armature bone hash to be there");
  LISTBASE_FOREACH (BoneCollectionMember *, member, &bcoll->bones) {
    member->bone = BKE_armature_find_bone_name(const_cast<bArmature *>(armature_dst),
                                               member->bone->name);
  }

  /* Now that the collection points to the right bones, these bones can be
   * updated to point to this collection. */
  add_reverse_pointers(bcoll);

  return bcoll;
}

/**
 * Copy all the child collections of the specified parent, from `armature_src` to `armature_dst`.
 *
 * This assumes that the parent itself has already been copied.
 */
static void liboverride_recursively_add_children(bArmature *armature_dst,
                                                 const bArmature *armature_src,
                                                 const int parent_bcoll_dst_index,
                                                 const BoneCollection *parent_bcoll_src)
{
  BLI_assert_msg(parent_bcoll_dst_index >= 0,
                 "this function can only add children to another collection, it cannot add roots");

  /* Iterate over the children in `armature_src`, and clone them one by one into `armature_dst`.
   *
   * This uses two loops. The first one adds all the children, the second loop iterates over those
   * children for the recursion step. As this performs a "breadth-first insertion", it requires
   * considerably less shuffling of the array as when the recursion was done immediately after
   * inserting a child. */
  BoneCollection *parent_bcoll_dst = armature_dst->collection_array[parent_bcoll_dst_index];

  /* Big Fat Assumption: because this code runs as part of the library override system, it is
   * assumed that the parent is either a newly added root, or another child that was also added by
   * the liboverride system. Because this would never add a child to an original "sequence of
   * siblings", insertions of children always happen at the end of the array. This means that
   * `parent_bcoll_dst_index` remains constant during this entire function. */

  /* Copy & insert all the children. */
  for (int bcoll_src_index = parent_bcoll_src->child_index;
       bcoll_src_index < parent_bcoll_src->child_index + parent_bcoll_src->child_count;
       bcoll_src_index++)
  {
    const BoneCollection *bcoll_src = armature_src->collection_array[bcoll_src_index];
    BoneCollection *bcoll_dst = copy_and_update_ownership(armature_dst, bcoll_src);

    const int bcoll_index_dst = bonecoll_insert_as_child(
        armature_dst, bcoll_dst, parent_bcoll_dst_index);

#ifndef NDEBUG
    /* Check that the above Big Fat Assumption holds. */
    BLI_assert_msg(bcoll_index_dst > parent_bcoll_dst_index,
                   "expecting children to be added to the array AFTER their parent");
#else
    (void)bcoll_index_dst;
#endif

    bonecoll_ensure_name_unique(armature_dst, bcoll_dst);
  }

  /* Double-check that the above Big Fat Assumption holds. */
#ifndef NDEBUG
  const int new_parent_bcoll_dst_index = armature_bonecoll_find_index(armature_dst,
                                                                      parent_bcoll_dst);
  BLI_assert_msg(new_parent_bcoll_dst_index == parent_bcoll_dst_index,
                 "did not expect parent_bcoll_dst_index to change");
#endif

  /* Recurse into the children to copy grandchildren. */
  BLI_assert_msg(parent_bcoll_dst->child_count == parent_bcoll_src->child_count,
                 "all children should have been copied");
  for (int child_num = 0; child_num < parent_bcoll_dst->child_count; child_num++) {
    const int bcoll_src_index = parent_bcoll_src->child_index + child_num;
    const int bcoll_dst_index = parent_bcoll_dst->child_index + child_num;

    const BoneCollection *bcoll_src = armature_src->collection_array[bcoll_src_index];
    liboverride_recursively_add_children(armature_dst, armature_src, bcoll_dst_index, bcoll_src);
  }
}

BoneCollection *ANIM_armature_bonecoll_insert_copy_after(bArmature *armature_dst,
                                                         const bArmature *armature_src,
                                                         const BoneCollection *anchor_in_dst,
                                                         const BoneCollection *bcoll_to_copy)
{
#ifndef NDEBUG
  /* Check that this bone collection is really a root, as this is assumed by the
   * rest of this function. This is an O(n) check, though, so that's why it's
   * only running in debug builds. */
  const int bcoll_index_src = armature_bonecoll_find_index(armature_src, bcoll_to_copy);
  if (!armature_bonecoll_is_root(armature_src, bcoll_index_src)) {
    printf(
        "Armature \"%s\" has library override operation that adds non-root bone collection "
        "\"%s\". This is unexpected, please file a bug report.\n",
        armature_src->id.name + 2,
        bcoll_to_copy->name);
  }
#endif

  BoneCollection *bcoll = copy_and_update_ownership(armature_dst, bcoll_to_copy);

  const int anchor_index = armature_bonecoll_find_index(armature_dst, anchor_in_dst);
  const int bcoll_index = anchor_index + 1;
  BLI_assert_msg(
      bcoll_index <= armature_dst->collection_root_count,
      "did not expect library override to add a child bone collection, only roots are expected");
  bonecoll_insert_as_root(armature_dst, bcoll, bcoll_index);
  bonecoll_ensure_name_unique(armature_dst, bcoll);

  /* Library override operations are only constructed for the root bones. This means that handling
   * this operation should also include copying the children. */
  liboverride_recursively_add_children(armature_dst, armature_src, bcoll_index, bcoll_to_copy);

  ANIM_armature_bonecoll_active_runtime_refresh(armature_dst);
  return bcoll;
}

static void armature_bonecoll_active_clear(bArmature *armature)
{
  armature->runtime.active_collection_index = -1;
  armature->runtime.active_collection = nullptr;
  armature->active_collection_name[0] = '\0';
}

void ANIM_armature_bonecoll_active_set(bArmature *armature, BoneCollection *bcoll)
{
  if (bcoll == nullptr) {
    armature_bonecoll_active_clear(armature);
    return;
  }

  const int index = armature_bonecoll_find_index(armature, bcoll);
  if (index == -1) {
    /* TODO: print warning? Or just ignore this case? */
    armature_bonecoll_active_clear(armature);
    return;
  }

  STRNCPY(armature->active_collection_name, bcoll->name);
  armature->runtime.active_collection_index = index;
  armature->runtime.active_collection = bcoll;
}

void ANIM_armature_bonecoll_active_index_set(bArmature *armature, const int bone_collection_index)
{
  if (bone_collection_index < 0 || bone_collection_index >= armature->collection_array_num) {
    armature_bonecoll_active_clear(armature);
    return;
  }

  BoneCollection *bcoll = armature->collection_array[bone_collection_index];

  STRNCPY(armature->active_collection_name, bcoll->name);
  armature->runtime.active_collection_index = bone_collection_index;
  armature->runtime.active_collection = bcoll;
}

void ANIM_armature_bonecoll_active_name_set(bArmature *armature, const char *name)
{
  BoneCollection *bcoll = ANIM_armature_bonecoll_get_by_name(armature, name);
  ANIM_armature_bonecoll_active_set(armature, bcoll);
}

void ANIM_armature_bonecoll_active_runtime_refresh(bArmature *armature)
{
  const std::string_view active_name = armature->active_collection_name;
  if (active_name.empty()) {
    armature_bonecoll_active_clear(armature);
    return;
  }

  int index = 0;
  for (BoneCollection *bcoll : armature->collections_span()) {
    if (bcoll->name == active_name) {
      armature->runtime.active_collection_index = index;
      armature->runtime.active_collection = bcoll;
      return;
    }
    index++;
  }

  /* No bone collection with the name was found, so better to clear everything. */
  armature_bonecoll_active_clear(armature);
}

bool ANIM_armature_bonecoll_is_editable(const bArmature *armature, const BoneCollection *bcoll)
{
  const bool is_override = ID_IS_OVERRIDE_LIBRARY(armature);
  if (ID_IS_LINKED(armature) && !is_override) {
    return false;
  }

  if (is_override && BKE_lib_override_library_is_system_defined(nullptr, &armature->id)) {
    /* A system override is still not editable. */
    return false;
  }

  if (is_override && (bcoll->flags & BONE_COLLECTION_OVERRIDE_LIBRARY_LOCAL) == 0) {
    /* This particular collection was not added in the local override, so not editable. */
    return false;
  }
  return true;
}

bool ANIM_armature_bonecoll_move_to_index(bArmature *armature,
                                          const int from_index,
                                          const int to_index)
{
  if (from_index >= armature->collection_array_num || to_index >= armature->collection_array_num ||
      from_index == to_index)
  {
    return false;
  }

  /* Only allow moving within the same parent. This is written a bit awkwardly to avoid two calls
   * to `armature_bonecoll_find_parent_index()` as that is O(n) in the number of bone collections.
   */
  const int parent_index = armature_bonecoll_find_parent_index(armature, from_index);
  if (!armature_bonecoll_is_child_of(armature, parent_index, to_index)) {
    return false;
  }

  if (parent_index < 0) {
    /* Roots can just be moved around, as there is no `child_index` to update in this case. */
    internal::bonecolls_move_to_index(armature, from_index, to_index);
    return true;
  }

  /* Store the parent's child_index, as that might move if to_index is the first child
   * (bonecolls_move_to_index() will keep it pointing at that first child). */
  BoneCollection *parent_bcoll = armature->collection_array[parent_index];
  const int old_parent_child_index = parent_bcoll->child_index;

  internal::bonecolls_move_to_index(armature, from_index, to_index);

  parent_bcoll->child_index = old_parent_child_index;

  return true;
}

static int bonecoll_child_number(const bArmature *armature,
                                 const int parent_bcoll_index,
                                 const int bcoll_index)
{
  if (parent_bcoll_index < 0) {
    /* Root bone collections are always at the start of the array, and thus their index is the
     * 'child number'. */
    return bcoll_index;
  }

  const BoneCollection *parent_bcoll = armature->collection_array[parent_bcoll_index];
  return bcoll_index - parent_bcoll->child_index;
}

int ANIM_armature_bonecoll_move_before_after_index(bArmature *armature,
                                                   const int from_index,
                                                   int to_index,
                                                   const MoveLocation before_after)
{
  const int from_parent_index = armature_bonecoll_find_parent_index(armature, from_index);
  const int to_parent_index = armature_bonecoll_find_parent_index(armature, to_index);

  if (from_parent_index != to_parent_index) {
    /* Moving between parents. */
    int to_child_num = bonecoll_child_number(armature, to_parent_index, to_index);
    if (before_after == MoveLocation::After) {
      to_child_num++;
    }

    return armature_bonecoll_move_to_parent(
        armature, from_index, to_child_num, from_parent_index, to_parent_index);
  }

  /* Moving between siblings. */
  switch (before_after) {
    case MoveLocation::Before:
      if (to_index > from_index) {
        /* Moving to the right, but needs to go before that one, so needs a decrement. */
        to_index--;
      }
      break;

    case MoveLocation::After:
      if (to_index < from_index) {
        /* Moving to the left, but needs to go after that one, so needs a decrement. */
        to_index++;
      }
      break;
  }

  if (!ANIM_armature_bonecoll_move_to_index(armature, from_index, to_index)) {
    return -1;
  }
  return to_index;
}

bool ANIM_armature_bonecoll_move(bArmature *armature, BoneCollection *bcoll, const int step)
{
  if (bcoll == nullptr) {
    return false;
  }

  const int bcoll_index = armature_bonecoll_find_index(armature, bcoll);
  const int to_index = bcoll_index + step;
  if (bcoll_index < 0 || to_index < 0 || to_index >= armature->collection_array_num) {
    return false;
  }

  ANIM_armature_bonecoll_move_to_index(armature, bcoll_index, to_index);

  return true;
}

void ANIM_armature_bonecoll_name_set(bArmature *armature, BoneCollection *bcoll, const char *name)
{
  char old_name[sizeof(bcoll->name)];

  STRNCPY(old_name, bcoll->name);

  if (name[0] == '\0') {
    /* Refuse to have nameless collections. The name of the active collection is stored in DNA, and
     * an empty string means 'no active collection'. */
    STRNCPY(bcoll->name, DATA_(bonecoll_default_name));
  }
  else {
    STRNCPY_UTF8(bcoll->name, name);
  }

  bonecoll_ensure_name_unique(armature, bcoll);

  /* Bone collections can be reached via .collections (4.0+) and .collections_all (4.1+).
   * Animation data from 4.0 should have been versioned to only use `.collections_all`. */
  BKE_animdata_fix_paths_rename_all(&armature->id, "collections", old_name, bcoll->name);
  BKE_animdata_fix_paths_rename_all(&armature->id, "collections_all", old_name, bcoll->name);
}

void ANIM_armature_bonecoll_remove_from_index(bArmature *armature, int index)
{
  BLI_assert(0 <= index && index < armature->collection_array_num);

  BoneCollection *bcoll = armature->collection_array[index];

  /* Get the active bone collection index before the armature is manipulated. */
  const int active_collection_index = armature->runtime.active_collection_index;

  /* The parent needs updating, so better to find it before this bone collection is removed. */
  int parent_bcoll_index = armature_bonecoll_find_parent_index(armature, index);
  BoneCollection *parent_bcoll = parent_bcoll_index >= 0 ?
                                     armature->collection_array[parent_bcoll_index] :
                                     nullptr;

  /* Move all the children of the to-be-removed bone collection to their grandparent. */
  int move_to_child_num = bonecoll_child_number(armature, parent_bcoll_index, index);
  while (bcoll->child_count > 0) {
    /* Move the child to its grandparent, at the same spot as the to-be-removed
     * bone collection. The latter thus (potentially) shifts by 1 in the array.
     * After removal, this effectively makes it appear like the removed bone
     * collection is replaced by all its children. */
    armature_bonecoll_move_to_parent(armature,
                                     bcoll->child_index, /* Move from index... */
                                     move_to_child_num,  /* to this child number. */
                                     index,              /* From this parent... */
                                     parent_bcoll_index  /* to that parent. */
    );

    /* Both 'index' and 'parent_bcoll_index' can change each iteration. */
    index = internal::bonecolls_find_index_near(armature, bcoll, index);
    BLI_assert_msg(index >= 0, "could not find bone collection after moving things around");

    if (parent_bcoll_index >= 0) { /* If there is no parent, its index should stay -1. */
      parent_bcoll_index = internal::bonecolls_find_index_near(
          armature, parent_bcoll, parent_bcoll_index);
      BLI_assert_msg(parent_bcoll_index >= 0,
                     "could not find bone collection parent after moving things around");
    }

    move_to_child_num++;
  }

  /* Adjust the parent for the removal of its child. */
  if (parent_bcoll_index < 0) {
    /* Removing a root, so the armature itself needs to be updated. */
    armature->collection_root_count--;
    BLI_assert_msg(armature->collection_root_count >= 0, "armature root count cannot be negative");
  }
  else {
    parent_bcoll->child_count--;
    if (parent_bcoll->child_count == 0) {
      parent_bcoll->child_index = 0;
    }
  }

  /* Rotate the to-be-removed collection to the last array element. */
  internal::bonecolls_move_to_index(armature, index, armature->collection_array_num - 1);

  /* NOTE: we don't bother to shrink the allocation.  It's okay if the
   * capacity has extra space, because the number of valid items is tracked. */
  armature->collection_array_num--;
  armature->collection_array[armature->collection_array_num] = nullptr;

  /* Update the active BoneCollection. */
  if (active_collection_index >= 0) {
    /* Default: select the next sibling.
     * If there is none: select the previous sibling.
     * If there is none: select the parent.
     */
    if (armature_bonecoll_is_child_of(armature, parent_bcoll_index, active_collection_index)) {
      /* active_collection_index still points to a sibling of the removed collection. */
      ANIM_armature_bonecoll_active_index_set(armature, active_collection_index);
    }
    else if (active_collection_index > 0 &&
             armature_bonecoll_is_child_of(
                 armature, parent_bcoll_index, active_collection_index - 1))
    {
      /* The child preceding active_collection_index is a sibling of the removed collection. */
      ANIM_armature_bonecoll_active_index_set(armature, active_collection_index - 1);
    }
    else {
      /* Select the parent, or nothing if this was a root collection. In that case, if there are no
       * siblings either, this just means all bone collections have been removed. */
      ANIM_armature_bonecoll_active_index_set(armature, parent_bcoll_index);
    }
  }

  const bool is_solo = bcoll->is_solo();
  internal::bonecoll_unassign_and_free(armature, bcoll);
  if (is_solo) {
    /* This might have been the last solo'ed bone collection, so check whether
     * solo'ing should still be active on the armature. */
    ANIM_armature_refresh_solo_active(armature);
  }
}

void ANIM_armature_bonecoll_remove(bArmature *armature, BoneCollection *bcoll)
{
  ANIM_armature_bonecoll_remove_from_index(armature,
                                           armature_bonecoll_find_index(armature, bcoll));
}

template<typename MaybeConstBoneCollection>
static MaybeConstBoneCollection *bonecolls_get_by_name(
    blender::Span<MaybeConstBoneCollection *> bonecolls, const char *name)
{
  for (MaybeConstBoneCollection *bcoll : bonecolls) {
    if (STREQ(bcoll->name, name)) {
      return bcoll;
    }
  }
  return nullptr;
}

BoneCollection *ANIM_armature_bonecoll_get_by_name(bArmature *armature, const char *name)
{
  return bonecolls_get_by_name(armature->collections_span(), name);
}

int ANIM_armature_bonecoll_get_index_by_name(bArmature *armature, const char *name)
{
  for (int index = 0; index < armature->collection_array_num; index++) {
    const BoneCollection *bcoll = armature->collection_array[index];
    if (STREQ(bcoll->name, name)) {
      return index;
    }
  }
  return -1;
}

/** Clear #BONE_COLLECTION_ANCESTORS_VISIBLE on all descendants of this bone collection. */
static void ancestors_visible_descendants_clear(bArmature *armature, BoneCollection *parent_bcoll)
{
  for (BoneCollection *bcoll : armature->collection_children(parent_bcoll)) {
    bcoll->flags &= ~BONE_COLLECTION_ANCESTORS_VISIBLE;
    ancestors_visible_descendants_clear(armature, bcoll);
  }
}

/** Set or clear #BONE_COLLECTION_ANCESTORS_VISIBLE on all descendants of this bone collection. */
static void ancestors_visible_descendants_update(bArmature *armature, BoneCollection *parent_bcoll)
{
  if (!parent_bcoll->is_visible_with_ancestors()) {
    /* If this bone collection is not visible itself, or any of its ancestors are
     * invisible, all descendants have an invisible ancestor. */
    ancestors_visible_descendants_clear(armature, parent_bcoll);
    return;
  }

  /* parent_bcoll is visible, and so are its ancestors. This means that all direct children have
   * visible ancestors. The grandchildren depend on the children's visibility as well, hence the
   * recursion. */
  for (BoneCollection *bcoll : armature->collection_children(parent_bcoll)) {
    bcoll->flags |= BONE_COLLECTION_ANCESTORS_VISIBLE;
    ancestors_visible_descendants_update(armature, bcoll);
  }
}

/** Set/clear BONE_COLLECTION_ANCESTORS_VISIBLE on this bone collection and all its descendants. */
static void ancestors_visible_update(bArmature *armature,
                                     const BoneCollection *parent_bcoll,
                                     BoneCollection *bcoll)
{
  if (parent_bcoll == nullptr || parent_bcoll->is_visible_with_ancestors()) {
    bcoll->flags |= BONE_COLLECTION_ANCESTORS_VISIBLE;
  }
  else {
    bcoll->flags &= ~BONE_COLLECTION_ANCESTORS_VISIBLE;
  }
  ancestors_visible_descendants_update(armature, bcoll);
}

void ANIM_bonecoll_show(bArmature *armature, BoneCollection *bcoll)
{
  bcoll->flags |= BONE_COLLECTION_VISIBLE;
  ancestors_visible_descendants_update(armature, bcoll);
}

void ANIM_bonecoll_hide(bArmature *armature, BoneCollection *bcoll)
{
  bcoll->flags &= ~BONE_COLLECTION_VISIBLE;
  ancestors_visible_descendants_update(armature, bcoll);
}

void ANIM_armature_bonecoll_is_visible_set(bArmature *armature,
                                           BoneCollection *bcoll,
                                           const bool is_visible)
{
  if (is_visible) {
    ANIM_bonecoll_show(armature, bcoll);
  }
  else {
    ANIM_bonecoll_hide(armature, bcoll);
  }
}

void ANIM_armature_bonecoll_solo_set(bArmature *armature,
                                     BoneCollection *bcoll,
                                     const bool is_solo)
{
  if (is_solo) {
    /* Enabling solo is simple. */
    bcoll->flags |= BONE_COLLECTION_SOLO;
    armature->flag |= ARM_BCOLL_SOLO_ACTIVE;
    return;
  }

  /* Disabling is harder, as the armature flag can only be disabled when there
   * are no more bone collections with the SOLO flag set. */
  bcoll->flags &= ~BONE_COLLECTION_SOLO;
  ANIM_armature_refresh_solo_active(armature);
}

void ANIM_armature_refresh_solo_active(bArmature *armature)
{
  bool any_bcoll_solo = false;
  for (const BoneCollection *bcoll : armature->collections_span()) {
    if (bcoll->flags & BONE_COLLECTION_SOLO) {
      any_bcoll_solo = true;
      break;
    }
  }

  if (any_bcoll_solo) {
    armature->flag |= ARM_BCOLL_SOLO_ACTIVE;
  }
  else {
    armature->flag &= ~ARM_BCOLL_SOLO_ACTIVE;
  }
}

bool ANIM_armature_bonecoll_is_visible_effectively(const bArmature *armature,
                                                   const BoneCollection *bcoll)
{
  const bool is_solo_active = armature->flag & ARM_BCOLL_SOLO_ACTIVE;

  if (is_solo_active) {
    /* If soloing is active, nothing in the hierarchy matters except the solo flag. */
    return bcoll->is_solo();
  }

  return bcoll->is_visible_with_ancestors();
}

void ANIM_armature_bonecoll_is_expanded_set(BoneCollection *bcoll, bool is_expanded)
{
  if (is_expanded) {
    bcoll->flags |= BONE_COLLECTION_EXPANDED;
  }
  else {
    bcoll->flags &= ~BONE_COLLECTION_EXPANDED;
  }
}

/* Store the bone's membership on the collection. */
static void add_membership(BoneCollection *bcoll, Bone *bone)
{
  BoneCollectionMember *member = MEM_cnew<BoneCollectionMember>(__func__);
  member->bone = bone;
  BLI_addtail(&bcoll->bones, member);
}
/* Store reverse membership on the bone. */
static void add_reference(Bone *bone, BoneCollection *bcoll)
{
  BoneCollectionReference *ref = MEM_cnew<BoneCollectionReference>(__func__);
  ref->bcoll = bcoll;
  BLI_addtail(&bone->runtime.collections, ref);
}

bool ANIM_armature_bonecoll_assign(BoneCollection *bcoll, Bone *bone)
{
  /* Precondition check: bail out if already a member. */
  LISTBASE_FOREACH (BoneCollectionMember *, member, &bcoll->bones) {
    if (member->bone == bone) {
      return false;
    }
  }

  add_membership(bcoll, bone);
  add_reference(bone, bcoll);

  return true;
}

bool ANIM_armature_bonecoll_assign_editbone(BoneCollection *bcoll, EditBone *ebone)
{
  /* Precondition check: bail out if already a member. */
  LISTBASE_FOREACH (BoneCollectionReference *, ref, &ebone->bone_collections) {
    if (ref->bcoll == bcoll) {
      return false;
    }
  }

  /* Store membership on the edit bone. Bones will be rebuilt when the armature
   * goes out of edit mode, and by then the newly created bones will be added to
   * the actual collection on the Armature. */
  BoneCollectionReference *ref = MEM_cnew<BoneCollectionReference>(__func__);
  ref->bcoll = bcoll;
  BLI_addtail(&ebone->bone_collections, ref);

  return true;
}

bool ANIM_armature_bonecoll_assign_and_move(BoneCollection *bcoll, Bone *bone)
{
  ANIM_armature_bonecoll_unassign_all(bone);
  return ANIM_armature_bonecoll_assign(bcoll, bone);
}

bool ANIM_armature_bonecoll_assign_and_move_editbone(BoneCollection *bcoll, EditBone *ebone)
{
  ANIM_armature_bonecoll_unassign_all_editbone(ebone);
  return ANIM_armature_bonecoll_assign_editbone(bcoll, ebone);
}

bool ANIM_armature_bonecoll_unassign(BoneCollection *bcoll, Bone *bone)
{
  bool was_found = false;

  /* Remove membership from collection. */
  LISTBASE_FOREACH_MUTABLE (BoneCollectionMember *, member, &bcoll->bones) {
    if (member->bone == bone) {
      BLI_freelinkN(&bcoll->bones, member);
      was_found = true;
      break;
    }
  }

  /* Remove reverse membership from the bone.
   * For data consistency sake, this is always done, regardless of whether the
   * above loop found the membership. */
  LISTBASE_FOREACH_MUTABLE (BoneCollectionReference *, ref, &bone->runtime.collections) {
    if (ref->bcoll == bcoll) {
      BLI_freelinkN(&bone->runtime.collections, ref);
      break;
    }
  }

  return was_found;
}

void ANIM_armature_bonecoll_unassign_all(Bone *bone)
{
  LISTBASE_FOREACH_MUTABLE (BoneCollectionReference *, ref, &bone->runtime.collections) {
    /* TODO: include Armature as parameter, and check that the bone collection to unassign from is
     * actually editable. */
    ANIM_armature_bonecoll_unassign(ref->bcoll, bone);
  }
}

void ANIM_armature_bonecoll_unassign_all_editbone(EditBone *ebone)
{
  LISTBASE_FOREACH_MUTABLE (BoneCollectionReference *, ref, &ebone->bone_collections) {
    ANIM_armature_bonecoll_unassign_editbone(ref->bcoll, ebone);
  }
}

bool ANIM_armature_bonecoll_unassign_editbone(BoneCollection *bcoll, EditBone *ebone)
{
  bool was_found = false;

  /* Edit bone membership is only stored on the edit bone itself. */
  LISTBASE_FOREACH_MUTABLE (BoneCollectionReference *, ref, &ebone->bone_collections) {
    if (ref->bcoll == bcoll) {
      BLI_freelinkN(&ebone->bone_collections, ref);
      was_found = true;
      break;
    }
  }
  return was_found;
}

void ANIM_armature_bonecoll_reconstruct(bArmature *armature)
{
  /* Remove all the old collection memberships. */
  for (BoneCollection *bcoll : armature->collections_span()) {
    BLI_freelistN(&bcoll->bones);
  }

  /* For all bones, restore their collection memberships. */
  ANIM_armature_foreach_bone(&armature->bonebase, [&](Bone *bone) {
    LISTBASE_FOREACH (BoneCollectionReference *, ref, &bone->runtime.collections) {
      add_membership(ref->bcoll, bone);
    }
  });
}

static bool any_bone_collection_visible(const bArmature *armature,
                                        const ListBase /*BoneCollectionRef*/ *collection_refs)
{
  /* Special case: when a bone is not in any collection, it is visible. */
  if (BLI_listbase_is_empty(collection_refs)) {
    return true;
  }

  LISTBASE_FOREACH (const BoneCollectionReference *, bcoll_ref, collection_refs) {
    const BoneCollection *bcoll = bcoll_ref->bcoll;
    if (ANIM_armature_bonecoll_is_visible_effectively(armature, bcoll)) {
      return true;
    }
  }
  return false;
}

bool ANIM_bone_in_visible_collection(const bArmature *armature, const Bone *bone)
{
  return any_bone_collection_visible(armature, &bone->runtime.collections);
}
bool ANIM_bonecoll_is_visible_editbone(const bArmature *armature, const EditBone *ebone)
{
  return any_bone_collection_visible(armature, &ebone->bone_collections);
}

void ANIM_armature_bonecoll_show_all(bArmature *armature)
{
  for (BoneCollection *bcoll : armature->collections_span()) {
    ANIM_bonecoll_show(armature, bcoll);
  }
}

void ANIM_armature_bonecoll_hide_all(bArmature *armature)
{
  for (BoneCollection *bcoll : armature->collections_span()) {
    ANIM_bonecoll_hide(armature, bcoll);
  }
}

/* ********************************* */
/* Armature Layers transitional API. */

void ANIM_armature_bonecoll_assign_active(const bArmature *armature, EditBone *ebone)
{
  if (armature->runtime.active_collection == nullptr) {
    /* No active collection, do not assign to any. */
    return;
  }

  ANIM_armature_bonecoll_assign_editbone(armature->runtime.active_collection, ebone);
}

static bool bcoll_list_contains(const ListBase /*BoneCollectionRef*/ *collection_refs,
                                const BoneCollection *bcoll)
{
  LISTBASE_FOREACH (const BoneCollectionReference *, bcoll_ref, collection_refs) {
    if (bcoll == bcoll_ref->bcoll) {
      return true;
    }
  }
  return false;
}

bool ANIM_armature_bonecoll_contains_active_bone(const bArmature *armature,
                                                 const BoneCollection *bcoll)
{
  if (armature->edbo) {
    if (!armature->act_edbone) {
      return false;
    }
    return bcoll_list_contains(&armature->act_edbone->bone_collections, bcoll);
  }

  if (!armature->act_bone) {
    return false;
  }
  return bcoll_list_contains(&armature->act_bone->runtime.collections, bcoll);
}

void ANIM_armature_bonecoll_show_from_bone(bArmature *armature, const Bone *bone)
{
  if (ANIM_bone_in_visible_collection(armature, bone)) {
    return;
  }

  /* Making the first collection visible is enough to make the bone visible.
   *
   * Since bones without collection are considered visible,
   * bone->runtime.collections.first is certainly a valid pointer. */
  BoneCollectionReference *ref = static_cast<BoneCollectionReference *>(
      bone->runtime.collections.first);
  ref->bcoll->flags |= BONE_COLLECTION_VISIBLE;
}

void ANIM_armature_bonecoll_show_from_ebone(bArmature *armature, const EditBone *ebone)
{
  if (ANIM_bonecoll_is_visible_editbone(armature, ebone)) {
    return;
  }

  /* Making the first collection visible is enough to make the bone visible.
   *
   * Since bones without collection are considered visible,
   * ebone->bone_collections.first is certainly a valid pointer. */
  BoneCollectionReference *ref = static_cast<BoneCollectionReference *>(
      ebone->bone_collections.first);
  ref->bcoll->flags |= BONE_COLLECTION_VISIBLE;
}

void ANIM_armature_bonecoll_show_from_pchan(bArmature *armature, const bPoseChannel *pchan)
{
  ANIM_armature_bonecoll_show_from_bone(armature, pchan->bone);
}

/* ********* */
/* C++ only. */
namespace blender::animrig {

int armature_bonecoll_find_index(const bArmature *armature, const BoneCollection *bcoll)
{
  int index = 0;
  for (const BoneCollection *arm_bcoll : armature->collections_span()) {
    if (arm_bcoll == bcoll) {
      return index;
    }
    index++;
  }

  return -1;
}

int armature_bonecoll_find_parent_index(const bArmature *armature, const int bcoll_index)
{
  if (bcoll_index < armature->collection_root_count) {
    /* Don't bother iterating all collections when it's known to be a root. */
    return -1;
  }

  int index = 0;
  for (const BoneCollection *potential_parent : armature->collections_span()) {
    if (potential_parent->child_index <= bcoll_index &&
        bcoll_index < potential_parent->child_index + potential_parent->child_count)
    {
      return index;
    }

    index++;
  }

  return -1;
}

int armature_bonecoll_child_number_find(const bArmature *armature, const ::BoneCollection *bcoll)
{
  const int bcoll_index = armature_bonecoll_find_index(armature, bcoll);
  const int parent_index = armature_bonecoll_find_parent_index(armature, bcoll_index);
  return bonecoll_child_number(armature, parent_index, bcoll_index);
}

int armature_bonecoll_child_number_set(bArmature *armature,
                                       ::BoneCollection *bcoll,
                                       int new_child_number)
{
  const int bcoll_index = armature_bonecoll_find_index(armature, bcoll);
  const int parent_index = armature_bonecoll_find_parent_index(armature, bcoll_index);

  BoneCollection fake_armature_parent = {};
  fake_armature_parent.child_count = armature->collection_root_count;

  BoneCollection *parent_bcoll;
  if (parent_index < 0) {
    parent_bcoll = &fake_armature_parent;
  }
  else {
    parent_bcoll = armature->collection_array[parent_index];
  }

  /* Bounds checks. */
  if (new_child_number >= parent_bcoll->child_count) {
    return -1;
  }
  if (new_child_number < 0) {
    new_child_number = parent_bcoll->child_count - 1;
  }

  /* Store the parent's child_index, as that might move if to_index is the first child
   * (bonecolls_move_to_index() will keep it pointing at that first child). */
  const int old_parent_child_index = parent_bcoll->child_index;
  const int to_index = parent_bcoll->child_index + new_child_number;
  internal::bonecolls_move_to_index(armature, bcoll_index, to_index);

  parent_bcoll->child_index = old_parent_child_index;

  /* Make sure that if this was the active bone collection, its index also changes. */
  if (armature->runtime.active_collection_index == bcoll_index) {
    ANIM_armature_bonecoll_active_index_set(armature, to_index);
  }

  return to_index;
}

bool armature_bonecoll_is_root(const bArmature *armature, const int bcoll_index)
{
  BLI_assert(bcoll_index >= 0);
  return bcoll_index < armature->collection_root_count;
}

bool armature_bonecoll_is_child_of(const bArmature *armature,
                                   const int potential_parent_index,
                                   const int potential_child_index)
{
  /* Check for roots, before we try and access collection_array[-1]. */
  if (armature_bonecoll_is_root(armature, potential_child_index)) {
    return potential_parent_index == -1;
  }
  if (potential_parent_index < 0) {
    return false;
  }

  const BoneCollection *potential_parent = armature->collection_array[potential_parent_index];
  const int upper_bound = potential_parent->child_index + potential_parent->child_count;

  return potential_parent->child_index <= potential_child_index &&
         potential_child_index < upper_bound;
}

bool armature_bonecoll_is_descendant_of(const bArmature *armature,
                                        const int potential_parent_index,
                                        const int potential_descendant_index)
{
  BLI_assert_msg(potential_descendant_index >= 0,
                 "Potential descendant has to exist for this function call to make sense.");

  if (armature_bonecoll_is_child_of(armature, potential_parent_index, potential_descendant_index))
  {
    /* Found a direct child. */
    return true;
  }

  const BoneCollection *potential_parent = armature->collection_array[potential_parent_index];
  const int upper_bound = potential_parent->child_index + potential_parent->child_count;

  for (int visit_index = potential_parent->child_index; visit_index < upper_bound; visit_index++) {
    if (armature_bonecoll_is_descendant_of(armature, visit_index, potential_descendant_index)) {
      return true;
    }
  }

  return false;
}

bool bonecoll_has_children(const BoneCollection *bcoll)
{
  return bcoll->child_count > 0;
}

void bonecolls_copy_expanded_flag(Span<BoneCollection *> bcolls_dest,
                                  Span<const BoneCollection *> bcolls_source)
{
  /* Try to preserve the bone collection expanded/collapsed states. These are UI
   * changes that shouldn't impact undo steps. Care has to be taken to match the
   * old and the new bone collections, though, as they may have been reordered
   * or renamed.
   *
   * Reordering is handled by looking up collections by name.
   * Renames are handled by skipping those that cannot be found by name. */

  auto find_old = [bcolls_source](const char *name, const int index) -> const BoneCollection * {
    /* Only check index when it's valid in the old armature. */
    if (index < bcolls_source.size()) {
      const BoneCollection *bcoll = bcolls_source[index];
      if (STREQ(bcoll->name, name)) {
        /* Index and name matches, let's use */
        return bcoll;
      }
    }

    /* Try to find by name as a last resort. This function only works with
     * non-const pointers, hence the const_cast.  */
    const BoneCollection *bcoll = bonecolls_get_by_name(bcolls_source, name);
    return bcoll;
  };

  for (int i = 0; i < bcolls_dest.size(); i++) {
    BoneCollection *bcoll_new = bcolls_dest[i];

    const BoneCollection *bcoll_old = find_old(bcoll_new->name, i);
    if (!bcoll_old) {
      continue;
    }

    ANIM_armature_bonecoll_is_expanded_set(bcoll_new, bcoll_old->is_expanded());
  }
}

int armature_bonecoll_move_to_parent(bArmature *armature,
                                     const int from_bcoll_index,
                                     int to_child_num,
                                     const int from_parent_index,
                                     const int to_parent_index)
{
  BLI_assert(0 <= from_bcoll_index && from_bcoll_index < armature->collection_array_num);
  BLI_assert(-1 <= from_parent_index && from_parent_index < armature->collection_array_num);
  BLI_assert(-1 <= to_parent_index && to_parent_index < armature->collection_array_num);

  if (from_parent_index == to_parent_index) {
    /* TODO: use `to_child_num` to still move the child to the desired position. */
    return from_bcoll_index;
  }

  /* The Armature itself acts like some sort of 'parent' for the root collections. By having this
   * as a 'fake' BoneCollection, all the code below can just be blissfully unaware of the special
   * 'all root collections should be at the start of the array' rule. */
  BoneCollection armature_root;
  armature_root.child_count = armature->collection_root_count;
  armature_root.child_index = 0;
  armature_root.flags = default_flags;

  BoneCollection *from_parent = from_parent_index >= 0 ?
                                    armature->collection_array[from_parent_index] :
                                    &armature_root;
  BoneCollection *to_parent = to_parent_index >= 0 ? armature->collection_array[to_parent_index] :
                                                     &armature_root;

  BLI_assert_msg(-1 <= to_child_num && to_child_num <= to_parent->child_count,
                 "to_child_num must point to an index of a child of the new parent, or the index "
                 "of the last child + 1, or be -1 to indicate 'after last child'");
  if (to_child_num < 0) {
    to_child_num = to_parent->child_count;
  }

  /* The new parent might not have children yet. */
  int to_bcoll_index;
  if (to_parent->child_count == 0) {
    /* New parents always get their children at the end of the array. */
    to_bcoll_index = armature->collection_array_num - 1;
  }
  else {
    to_bcoll_index = to_parent->child_index + to_child_num;

    /* Check whether the new parent's children are to the left or right of bcoll_index.
     * This determines which direction the collections have to shift, and thus which index to
     * move the bcoll to. */
    if (to_bcoll_index > from_bcoll_index) {
      to_bcoll_index--;
    }
  }

  /* In certain cases the 'from_parent' gets its first child removed, and needs to have its
   * child_index incremented. This needs to be done by comparing these fields before the actual
   * move happens (as that could also change the child_index). */
  const bool needs_post_move_child_index_bump = from_parent->child_index == from_bcoll_index &&
                                                to_bcoll_index <= from_bcoll_index;
  /* bonecolls_move_to_index() will try and keep the hierarchy correct, and thus change
   * to_parent->child_index to keep pointing to its current-first child. */
  const bool becomes_new_first_child = to_child_num == 0 || to_parent->child_count == 0;
  internal::bonecolls_move_to_index(armature, from_bcoll_index, to_bcoll_index);

  /* Update child index & count of the old parent. */
  from_parent->child_count--;
  if (from_parent->child_count == 0) {
    /* Clean up the child index when the parent has no more children. */
    from_parent->child_index = 0;
  }
  else if (needs_post_move_child_index_bump) {
    /* The start of the block of children of the old parent has moved, because
     * we took out the first child. This only needs to be compensated for when
     * moving it to the left (or staying put), as then its old siblings stay in
     * place.
     *
     * This only needs to be done if there are any children left, though. */
    from_parent->child_index++;
  }

  /* Update child index & count of the new parent. */
  if (becomes_new_first_child) {
    to_parent->child_index = to_bcoll_index;
  }
  to_parent->child_count++;

  /* Copy the information from the 'fake' BoneCollection back to the armature. */
  armature->collection_root_count = armature_root.child_count;
  BLI_assert(armature_root.child_index == 0);

  /* Since the parent changed, the effective visibility might change too. */
  ancestors_visible_update(armature, to_parent, armature->collection_array[to_bcoll_index]);

  return to_bcoll_index;
}

/* Utility functions for Armature edit-mode undo. */

blender::Map<BoneCollection *, BoneCollection *> ANIM_bonecoll_array_copy_no_membership(
    BoneCollection ***bcoll_array_dst,
    int *bcoll_array_dst_num,
    BoneCollection **bcoll_array_src,
    const int bcoll_array_src_num,
    const bool do_id_user)
{
  BLI_assert(*bcoll_array_dst == nullptr);
  BLI_assert(*bcoll_array_dst_num == 0);

  *bcoll_array_dst = static_cast<BoneCollection **>(
      MEM_malloc_arrayN(bcoll_array_src_num, sizeof(BoneCollection *), __func__));
  *bcoll_array_dst_num = bcoll_array_src_num;

  blender::Map<BoneCollection *, BoneCollection *> bcoll_map{};
  for (int i = 0; i < bcoll_array_src_num; i++) {
    BoneCollection *bcoll_src = bcoll_array_src[i];
    BoneCollection *bcoll_dst = static_cast<BoneCollection *>(MEM_dupallocN(bcoll_src));

    /* This will be rebuilt from the edit bones, so we don't need to copy it. */
    BLI_listbase_clear(&bcoll_dst->bones);

    if (bcoll_src->prop) {
      bcoll_dst->prop = IDP_CopyProperty_ex(bcoll_src->prop,
                                            do_id_user ? 0 : LIB_ID_CREATE_NO_USER_REFCOUNT);
    }

    (*bcoll_array_dst)[i] = bcoll_dst;

    bcoll_map.add(bcoll_src, bcoll_dst);
  }

  return bcoll_map;
}

void ANIM_bonecoll_array_free(BoneCollection ***bcoll_array,
                              int *bcoll_array_num,
                              const bool do_id_user)
{
  for (int i = 0; i < *bcoll_array_num; i++) {
    BoneCollection *bcoll = (*bcoll_array)[i];

    if (bcoll->prop) {
      IDP_FreeProperty_ex(bcoll->prop, do_id_user);
    }

    /* This will usually already be empty, because the passed BoneCollection
     * list is usually from ANIM_bonecoll_listbase_copy_no_membership().
     * However, during undo this is also used to free the BoneCollection
     * list on the Armature itself before copying over the undo BoneCollection
     * list, in which case this of Bone pointers may not be empty. */
    BLI_freelistN(&bcoll->bones);

    MEM_freeN(bcoll);
  }
  MEM_freeN(*bcoll_array);

  *bcoll_array = nullptr;
  *bcoll_array_num = 0;
}

/** Functions declared in bone_collections_internal.hh. */
namespace internal {

void bonecolls_rotate_block(bArmature *armature,
                            const int start_index,
                            const int count,
                            const int direction)
{
  BLI_assert_msg(direction == 1 || direction == -1, "`direction` must be either -1 or +1");

  if (count == 0) {
    return;
  }

  /* When the block [start_index:start_index+count] is moved, it causes a duplication of one
   * element and overwrites another element. For example: given an array [0, 1, 2, 3, 4], moving
   * indices [1, 2] by +1 would result in one double element (1) and one missing element (3): [0,
   * 1, 1, 2, 4].
   *
   * This is resolved by moving that element to the other side of the block, so the result will be
   * [0, 3, 1, 2, 4]. This breaks the hierarchical information, so it's up to the caller to update
   * this one moved element.
   */

  const int move_from_index = (direction > 0 ? start_index + count : start_index - 1);
  const int move_to_index = (direction > 0 ? start_index : start_index + count - 1);
  BoneCollection *bcoll_to_move = armature->collection_array[move_from_index];

  BoneCollection **start = armature->collection_array + start_index;
  memmove((void *)(start + direction), (void *)start, count * sizeof(BoneCollection *));

  armature->collection_array[move_to_index] = bcoll_to_move;

  /* Update all child indices that reference something in the moved block. */
  for (BoneCollection *bcoll : armature->collections_span()) {
    /* Having both child_index and child_count zeroed out just means "no children"; these shouldn't
     * be updated at all, as here child_index is not really referencing the element at index 0. */
    if (bcoll->child_index == 0 && bcoll->child_count == 0) {
      continue;
    }

    /* Compare to the original start & end of the block (i.e. pre-move). If a
     * child_index is within this range, it'll need updating. */
    if (start_index <= bcoll->child_index && bcoll->child_index < start_index + count) {
      bcoll->child_index += direction;
    }
  }

  /* Make sure the active bone collection index is moved as well. */
  const int active_index = armature->runtime.active_collection_index;
  if (active_index == move_from_index) {
    armature->runtime.active_collection_index = move_to_index;
  }
  else if (start_index <= active_index && active_index < start_index + count) {
    armature->runtime.active_collection_index += direction;
  }
}

void bonecolls_move_to_index(bArmature *armature, const int from_index, const int to_index)
{
  if (from_index == to_index) {
    return;
  }

  BLI_assert(0 <= from_index);
  BLI_assert(from_index < armature->collection_array_num);
  BLI_assert(0 <= to_index);
  BLI_assert(to_index < armature->collection_array_num);

  if (from_index < to_index) {
    const int block_start_index = from_index + 1;
    const int block_count = to_index - from_index;
    bonecolls_rotate_block(armature, block_start_index, block_count, -1);
  }
  else {
    const int block_start_index = to_index;
    const int block_count = from_index - to_index;
    bonecolls_rotate_block(armature, block_start_index, block_count, +1);
  }
}

int bonecolls_find_index_near(bArmature *armature, BoneCollection *bcoll, const int index)
{
  BoneCollection **collections = armature->collection_array;

  if (collections[index] == bcoll) {
    return index;
  }
  if (index > 0 && collections[index - 1] == bcoll) {
    return index - 1;
  }
  if (index < armature->collection_array_num - 1 && collections[index + 1] == bcoll) {
    return index + 1;
  }
  return -1;
}

void bonecolls_debug_list(const bArmature *armature)
{
  printf("\033[38;5;214mBone collections of armature \"%s\":\033[0m\n", armature->id.name + 2);
  constexpr int root_ansi_color = 95;
  printf(
      "    - \033[%dmroot\033[0m count: %d\n", root_ansi_color, armature->collection_root_count);
  for (int i = 0; i < armature->collection_array_num; ++i) {
    const BoneCollection *bcoll = armature->collection_array[i];
    printf("    - \033[%dmcolls[%d] = %24s\033[0m ",
           i < armature->collection_root_count ? root_ansi_color : 0,
           i,
           bcoll->name);
    if (bcoll->child_index == 0 && bcoll->child_count == 0) {
      printf("(leaf)");
    }
    else {
      printf("(child index: %d, count: %d)", bcoll->child_index, bcoll->child_count);
    }
    printf("\n");
  }
}

void bonecoll_unassign_and_free(bArmature *armature, BoneCollection *bcoll)
{
  /* Remove bone membership. */
  LISTBASE_FOREACH_MUTABLE (BoneCollectionMember *, member, &bcoll->bones) {
    ANIM_armature_bonecoll_unassign(bcoll, member->bone);
  }
  if (armature->edbo) {
    LISTBASE_FOREACH (EditBone *, ebone, armature->edbo) {
      ANIM_armature_bonecoll_unassign_editbone(bcoll, ebone);
    }
  }

  ANIM_bonecoll_free(bcoll);
}

}  // namespace internal

}  // namespace blender::animrig
