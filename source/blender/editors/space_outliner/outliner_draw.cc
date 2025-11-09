/* SPDX-FileCopyrightText: 2004 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#include "DNA_armature_types.h"
#include "DNA_collection_types.h"
#include "DNA_constraint_types.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_light_types.h"
#include "DNA_lightprobe_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"
#include "DNA_text_types.h"

#include "BLI_fileops.h"
#include "BLI_listbase.h"
#include "BLI_math_vector.h"
#include "BLI_path_utils.hh"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_string_utils.hh"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "BKE_action.hh"
#include "BKE_armature.hh"
#include "BKE_context.hh"
#include "BKE_curve.hh"
#include "BKE_deform.hh"
#include "BKE_gpencil_legacy.h"
#include "BKE_grease_pencil.hh"
#include "BKE_idtype.hh"
#include "BKE_layer.hh"
#include "BKE_lib_id.hh"
#include "BKE_lib_override.hh"
#include "BKE_library.hh"
#include "BKE_main.hh"
#include "BKE_main_namemap.hh"
#include "BKE_modifier.hh"
#include "BKE_node.hh"
#include "BKE_object.hh"
#include "BKE_particle.h"
#include "BKE_report.hh"
#include "BKE_scene.hh"

#include "ANIM_armature.hh"
#include "ANIM_bone_collections.hh"
#include "ANIM_keyframing.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"

#include "ED_armature.hh"
#include "ED_fileselect.hh"
#include "ED_id_management.hh"
#include "ED_outliner.hh"
#include "ED_screen.hh"
#include "ED_undo.hh"

#include "WM_api.hh"
#include "WM_message.hh"
#include "WM_types.hh"

#include "GPU_immediate.hh"
#include "GPU_state.hh"

#include "UI_interface.hh"
#include "UI_interface_icons.hh"
#include "UI_resources.hh"
#include "UI_view2d.hh"

#include "RNA_access.hh"

#include "outliner_intern.hh"
#include "tree/tree_element.hh"
#include "tree/tree_element_grease_pencil_node.hh"
#include "tree/tree_element_id.hh"
#include "tree/tree_element_overrides.hh"
#include "tree/tree_element_rna.hh"
#include "tree/tree_element_seq.hh"
#include "tree/tree_iterator.hh"

namespace blender::ed::outliner {

/* -------------------------------------------------------------------- */
/** \name Tree Size Functions
 * \{ */

static void outliner_tree_dimensions_impl(SpaceOutliner *space_outliner,
                                          ListBase *lb,
                                          int *width,
                                          int *height)
{
  LISTBASE_FOREACH (TreeElement *, te, lb) {
    *width = std::max(*width, int(te->xend));
    if (height != nullptr) {
      *height += UI_UNIT_Y;
    }

    TreeStoreElem *tselem = TREESTORE(te);
    if (TSELEM_OPEN(tselem, space_outliner)) {
      outliner_tree_dimensions_impl(space_outliner, &te->subtree, width, height);
    }
    else {
      outliner_tree_dimensions_impl(space_outliner, &te->subtree, width, nullptr);
    }
  }
}

void outliner_tree_dimensions(SpaceOutliner *space_outliner, int *r_width, int *r_height)
{
  *r_width = 0;
  *r_height = 0;
  outliner_tree_dimensions_impl(space_outliner, &space_outliner->tree, r_width, r_height);
}

/**
 * The active object is only needed for reference.
 */
static bool is_object_data_in_editmode(const ID *id, const Object *obact)
{
  if (id == nullptr) {
    return false;
  }

  const short id_type = GS(id->name);

  return ((obact && (obact->mode & OB_MODE_EDIT)) && (id && OB_DATA_SUPPORT_EDITMODE(id_type)) &&
          (GS(((ID *)obact->data)->name) == id_type) && BKE_object_data_is_in_editmode(obact, id));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Button Callbacks
 * \{ */

static void restrictbutton_recursive_ebone(bArmature *arm,
                                           EditBone *ebone_parent,
                                           int flag,
                                           bool set_flag)
{
  LISTBASE_FOREACH (EditBone *, ebone, arm->edbo) {
    if (ED_armature_ebone_is_child_recursive(ebone_parent, ebone)) {
      if (set_flag) {
        ebone->flag &= ~(BONE_TIPSEL | BONE_SELECTED | BONE_ROOTSEL);
        ebone->flag |= flag;
      }
      else {
        ebone->flag &= ~flag;
      }
    }
  }
}

static void restrictbutton_recursive_bone(Bone *bone_parent, int flag, bool set_flag)
{
  LISTBASE_FOREACH (Bone *, bone, &bone_parent->childbase) {
    if (set_flag) {
      bone->flag &= ~(BONE_TIPSEL | BONE_SELECTED | BONE_ROOTSEL);
      bone->flag |= flag;
    }
    else {
      bone->flag &= ~flag;
    }
    restrictbutton_recursive_bone(bone, flag, set_flag);
  }
}

static void restrictbutton_r_lay_fn(bContext *C, void *poin, void * /*poin2*/)
{
  WM_event_add_notifier(C, NC_SCENE | ND_RENDER_OPTIONS, poin);
}

static void restrictbutton_bone_visibility_fn(bContext *C, void *poin, void *poin2)
{
  const Object *ob = (Object *)poin;
  bPoseChannel *pchan = (bPoseChannel *)poin2;
  if (CTX_wm_window(C)->eventstate->modifier & KM_SHIFT) {
    blender::animrig::pose_bone_descendent_iterator(
        *ob->pose, *pchan, [&](bPoseChannel &descendent) {
          if (pchan->drawflag & PCHAN_DRAW_HIDDEN) {
            descendent.drawflag |= PCHAN_DRAW_HIDDEN;
          }
          else {
            descendent.drawflag &= ~PCHAN_DRAW_HIDDEN;
          }
        });
  }
}

static void restrictbutton_bone_select_fn(bContext *C, void *poin, void *poin2)
{
  bArmature *arm = static_cast<bArmature *>(poin);
  Bone *bone = static_cast<Bone *>(poin2);

  if (bone->flag & BONE_UNSELECTABLE) {
    bone->flag &= ~(BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL);
  }

  if (CTX_wm_window(C)->eventstate->modifier & KM_SHIFT) {
    restrictbutton_recursive_bone(bone, BONE_UNSELECTABLE, (bone->flag & BONE_UNSELECTABLE) != 0);
  }

  DEG_id_tag_update(&arm->id, ID_RECALC_SYNC_TO_EVAL);
  WM_event_add_notifier(C, NC_OBJECT | ND_POSE, nullptr);
}

static void restrictbutton_ebone_select_fn(bContext *C, void *poin, void *poin2)
{
  bArmature *arm = (bArmature *)poin;
  EditBone *ebone = (EditBone *)poin2;

  if (ebone->flag & BONE_UNSELECTABLE) {
    ebone->flag &= ~(BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL);
  }

  if (CTX_wm_window(C)->eventstate->modifier & KM_SHIFT) {
    restrictbutton_recursive_ebone(
        arm, ebone, BONE_UNSELECTABLE, (ebone->flag & BONE_UNSELECTABLE) != 0);
  }

  WM_event_add_notifier(C, NC_OBJECT | ND_POSE, nullptr);
}

static void restrictbutton_ebone_visibility_fn(bContext *C, void *poin, void *poin2)
{
  bArmature *arm = (bArmature *)poin;
  EditBone *ebone = (EditBone *)poin2;
  if (ebone->flag & BONE_HIDDEN_A) {
    ebone->flag &= ~(BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL);
  }

  if (CTX_wm_window(C)->eventstate->modifier & KM_SHIFT) {
    restrictbutton_recursive_ebone(arm, ebone, BONE_HIDDEN_A, (ebone->flag & BONE_HIDDEN_A) != 0);
  }

  WM_event_add_notifier(C, NC_OBJECT | ND_POSE, nullptr);
}

static void restrictbutton_gp_layer_flag_fn(bContext *C, void *poin, void * /*poin2*/)
{
  ID *id = (ID *)poin;

  DEG_id_tag_update(id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);
}

static void restrictbutton_id_user_toggle(bContext * /*C*/, void *poin, void * /*poin2*/)
{
  ID *id = (ID *)poin;

  BLI_assert(id != nullptr);

  if (id->flag & ID_FLAG_FAKEUSER) {
    id_us_plus(id);
  }
  else {
    id_us_min(id);
  }
}

static void outliner_object_set_flag_recursive_fn(bContext *C,
                                                  Base *base,
                                                  Object *ob,
                                                  const char *propname)
{
  Main *bmain = CTX_data_main(C);
  wmWindow *win = CTX_wm_window(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);

  bool extend = (win->eventstate->modifier & KM_SHIFT);

  if (!extend) {
    return;
  }

  /* Create PointerRNA and PropertyRNA for either Object or Base. */
  ID *id = ob ? &ob->id : &scene->id;
  StructRNA *struct_rna = ob ? &RNA_Object : &RNA_ObjectBase;
  void *data = ob ? (void *)ob : (void *)base;

  PointerRNA ptr = RNA_pointer_create_discrete(id, struct_rna, data);
  PropertyRNA *base_or_object_prop = RNA_struct_type_find_property(struct_rna, propname);
  const bool value = RNA_property_boolean_get(&ptr, base_or_object_prop);

  Object *ob_parent = ob ? ob : base->object;

  for (Object *ob_iter = static_cast<Object *>(bmain->objects.first); ob_iter;
       ob_iter = static_cast<Object *>(ob_iter->id.next))
  {
    if (BKE_object_is_child_recursive(ob_parent, ob_iter)) {
      if (ob) {
        ptr = RNA_id_pointer_create(&ob_iter->id);
        DEG_id_tag_update(&ob_iter->id, ID_RECALC_SYNC_TO_EVAL);
      }
      else {
        BKE_view_layer_synced_ensure(scene, view_layer);
        Base *base_iter = BKE_view_layer_base_find(view_layer, ob_iter);
        /* Child can be in a collection excluded from view-layer. */
        if (base_iter == nullptr) {
          continue;
        }
        ptr = RNA_pointer_create_discrete(&scene->id, &RNA_ObjectBase, base_iter);
      }
      RNA_property_boolean_set(&ptr, base_or_object_prop, value);
      blender::animrig::autokeyframe_property(
          C, scene, &ptr, base_or_object_prop, -1, BKE_scene_frame_get(scene), true);
    }
  }

  /* We don't call RNA_property_update() due to performance, so we batch update them. */
  if (ob) {
    BKE_main_collection_sync_remap(bmain);
    DEG_relations_tag_update(bmain);
  }
  else {
    BKE_view_layer_need_resync_tag(view_layer);
    DEG_id_tag_update(&scene->id, ID_RECALC_BASE_FLAGS);
  }
}

/**
 * Object properties.
 */
static void outliner__object_set_flag_recursive_fn(bContext *C, void *poin, void *poin2)
{
  Object *ob = static_cast<Object *>(poin);
  const char *propname = static_cast<const char *>(poin2);
  outliner_object_set_flag_recursive_fn(C, nullptr, ob, propname);
}

/**
 * Base properties.
 */
static void outliner__base_set_flag_recursive_fn(bContext *C, void *poin, void *poin2)
{
  Base *base = static_cast<Base *>(poin);
  const char *propname = static_cast<const char *>(poin2);
  outliner_object_set_flag_recursive_fn(C, base, nullptr, propname);
}

/** Create either a RNA_LayerCollection or a RNA_Collection pointer. */
static void outliner_layer_or_collection_pointer_create(Scene *scene,
                                                        LayerCollection *layer_collection,
                                                        Collection *collection,
                                                        PointerRNA *ptr)
{
  if (collection) {
    *ptr = RNA_id_pointer_create(&collection->id);
  }
  else {
    *ptr = RNA_pointer_create_discrete(&scene->id, &RNA_LayerCollection, layer_collection);
  }
}

/** Create either a RNA_ObjectBase or a RNA_Object pointer. */
static void outliner_base_or_object_pointer_create(
    Scene *scene, ViewLayer *view_layer, Collection *collection, Object *ob, PointerRNA *ptr)
{
  if (collection) {
    *ptr = RNA_id_pointer_create(&ob->id);
  }
  else {
    BKE_view_layer_synced_ensure(scene, view_layer);
    Base *base = BKE_view_layer_base_find(view_layer, ob);
    *ptr = RNA_pointer_create_discrete(&scene->id, &RNA_ObjectBase, base);
  }
}

/* NOTE: Collection is only valid when we want to change the collection data, otherwise we get it
 * from layer collection. Layer collection is valid whenever we are looking at a view layer. */
static void outliner_collection_set_flag_recursive(Scene *scene,
                                                   ViewLayer *view_layer,
                                                   LayerCollection *layer_collection,
                                                   Collection *collection,
                                                   PropertyRNA *layer_or_collection_prop,
                                                   PropertyRNA *base_or_object_prop,
                                                   const bool value)
{
  if (!layer_collection) {
    return;
  }

  PointerRNA ptr;
  outliner_layer_or_collection_pointer_create(scene, layer_collection, collection, &ptr);
  if (layer_or_collection_prop && !RNA_property_editable(&ptr, layer_or_collection_prop)) {
    return;
  }

  RNA_property_boolean_set(&ptr, layer_or_collection_prop, value);

  /* Set the same flag for the nested objects as well. */
  if (base_or_object_prop && !(layer_collection->flag & LAYER_COLLECTION_EXCLUDE)) {
    /* NOTE: We can't use BKE_collection_object_cache_get()
     * otherwise we would not take collection exclusion into account. */
    LISTBASE_FOREACH (CollectionObject *, cob, &layer_collection->collection->gobject) {

      outliner_base_or_object_pointer_create(scene, view_layer, collection, cob->ob, &ptr);
      if (!RNA_property_editable(&ptr, base_or_object_prop)) {
        continue;
      }

      RNA_property_boolean_set(&ptr, base_or_object_prop, value);

      if (collection) {
        DEG_id_tag_update(&cob->ob->id, ID_RECALC_SYNC_TO_EVAL);
      }
    }
  }

  /* Keep going recursively. */
  ListBase *lb = (layer_collection ? &layer_collection->layer_collections : &collection->children);
  LISTBASE_FOREACH (Link *, link, lb) {
    LayerCollection *layer_collection_iter = layer_collection ? (LayerCollection *)link : nullptr;
    Collection *collection_iter = layer_collection ?
                                      (collection ? layer_collection_iter->collection : nullptr) :
                                      ((CollectionChild *)link)->collection;
    outliner_collection_set_flag_recursive(scene,
                                           view_layer,
                                           layer_collection_iter,
                                           collection_iter,
                                           layer_or_collection_prop,
                                           base_or_object_prop,
                                           value);
  }

  if (collection) {
    DEG_id_tag_update(&collection->id, ID_RECALC_SYNC_TO_EVAL);
  }
}

/**
 * Check if collection is already isolated.
 *
 * A collection is isolated if all its parents and children are "visible".
 * All the other collections must be "invisible".
 *
 * NOTE: We could/should boost performance by iterating over the tree twice.
 * First tagging all the children/parent collections, then getting their values and comparing.
 * To run BKE_collection_has_collection() so many times is silly and slow.
 */
static bool outliner_collection_is_isolated(Scene *scene,
                                            const LayerCollection *layer_collection_cmp,
                                            const Collection *collection_cmp,
                                            const bool value_cmp,
                                            PropertyRNA *layer_or_collection_prop,
                                            LayerCollection *layer_collection,
                                            Collection *collection)
{
  PointerRNA ptr;
  outliner_layer_or_collection_pointer_create(scene, layer_collection, collection, &ptr);
  const bool value = RNA_property_boolean_get(&ptr, layer_or_collection_prop);
  Collection *collection_ensure = collection ? collection : layer_collection->collection;
  const Collection *collection_ensure_cmp = collection_cmp ? collection_cmp :
                                                             layer_collection_cmp->collection;

  /* Exclude linked collections, otherwise toggling property won't reset the visibility for
   * editable collections, see: #130579. */
  if (layer_or_collection_prop && !RNA_property_editable(&ptr, layer_or_collection_prop)) {
    return true;
  }

  if (collection_ensure->flag & COLLECTION_IS_MASTER) {
  }
  else if (collection_ensure == collection_ensure_cmp) {
  }
  else if (BKE_collection_has_collection(collection_ensure, (Collection *)collection_ensure_cmp) ||
           BKE_collection_has_collection((Collection *)collection_ensure_cmp, collection_ensure))
  {
    /* This collection is either a parent or a child of the collection.
     * We expect it to be set "visible" already. */
    if (value != value_cmp) {
      return false;
    }
  }
  else {
    /* This collection is neither a parent nor a child of the collection.
     * We expect it to be "invisible". */
    if (value == value_cmp) {
      return false;
    }
  }

  /* Keep going recursively. */
  ListBase *lb = (layer_collection ? &layer_collection->layer_collections : &collection->children);
  LISTBASE_FOREACH (Link *, link, lb) {
    LayerCollection *layer_collection_iter = layer_collection ? (LayerCollection *)link : nullptr;
    Collection *collection_iter = layer_collection ?
                                      (collection ? layer_collection_iter->collection : nullptr) :
                                      ((CollectionChild *)link)->collection;
    if (layer_collection_iter && layer_collection_iter->flag & LAYER_COLLECTION_EXCLUDE) {
      continue;
    }
    if (!outliner_collection_is_isolated(scene,
                                         layer_collection_cmp,
                                         collection_cmp,
                                         value_cmp,
                                         layer_or_collection_prop,
                                         layer_collection_iter,
                                         collection_iter))
    {
      return false;
    }
  }

  return true;
}

void outliner_collection_isolate_flag(Scene *scene,
                                      ViewLayer *view_layer,
                                      LayerCollection *layer_collection,
                                      Collection *collection,
                                      PropertyRNA *layer_or_collection_prop,
                                      const char *propname,
                                      const bool value)
{
  PointerRNA ptr;
  const bool is_hide = strstr(propname, "hide_") || STREQ(propname, "exclude");

  LayerCollection *top_layer_collection = layer_collection ?
                                              static_cast<LayerCollection *>(
                                                  view_layer->layer_collections.first) :
                                              nullptr;
  Collection *top_collection = collection ? scene->master_collection : nullptr;

  bool was_isolated = (value == is_hide);
  was_isolated &= outliner_collection_is_isolated(scene,
                                                  layer_collection,
                                                  collection,
                                                  !is_hide,
                                                  layer_or_collection_prop,
                                                  top_layer_collection,
                                                  top_collection);

  if (was_isolated) {
    const bool default_value = RNA_property_boolean_get_default(nullptr, layer_or_collection_prop);
    /* Make every collection go back to its default "visibility" state. */
    outliner_collection_set_flag_recursive(scene,
                                           view_layer,
                                           top_layer_collection,
                                           top_collection,
                                           layer_or_collection_prop,
                                           nullptr,
                                           default_value);
    return;
  }

  /* Make every collection "invisible". */
  outliner_collection_set_flag_recursive(scene,
                                         view_layer,
                                         top_layer_collection,
                                         top_collection,
                                         layer_or_collection_prop,
                                         nullptr,
                                         is_hide);

  /* Make this collection and its children collections the only "visible". */
  outliner_collection_set_flag_recursive(scene,
                                         view_layer,
                                         layer_collection,
                                         collection,
                                         layer_or_collection_prop,
                                         nullptr,
                                         !is_hide);

  /* Make this collection direct parents also "visible". */
  if (layer_collection) {
    LayerCollection *lc_parent = layer_collection;
    LISTBASE_FOREACH (LayerCollection *, lc_iter, &top_layer_collection->layer_collections) {
      if (BKE_layer_collection_has_layer_collection(lc_iter, layer_collection)) {
        lc_parent = lc_iter;
        break;
      }
    }

    while (lc_parent != layer_collection) {
      outliner_layer_or_collection_pointer_create(
          scene, lc_parent, collection ? lc_parent->collection : nullptr, &ptr);
      RNA_property_boolean_set(&ptr, layer_or_collection_prop, !is_hide);

      LISTBASE_FOREACH (LayerCollection *, lc_iter, &lc_parent->layer_collections) {
        if (BKE_layer_collection_has_layer_collection(lc_iter, layer_collection)) {
          lc_parent = lc_iter;
          break;
        }
      }
    }
  }
  else {
    CollectionParent *parent;
    Collection *child = collection;
    while ((parent = static_cast<CollectionParent *>(child->runtime->parents.first))) {
      if (parent->collection->flag & COLLECTION_IS_MASTER) {
        break;
      }
      ptr = RNA_id_pointer_create(&parent->collection->id);
      RNA_property_boolean_set(&ptr, layer_or_collection_prop, !is_hide);
      child = parent->collection;
    }
  }
}

static void outliner_collection_set_flag_recursive_fn(bContext *C,
                                                      LayerCollection *layer_collection,
                                                      Collection *collection,
                                                      const char *propname)
{
  Main *bmain = CTX_data_main(C);
  wmWindow *win = CTX_wm_window(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);

  bool do_isolate = (win->eventstate->modifier & KM_CTRL);
  bool extend = (win->eventstate->modifier & KM_SHIFT);

  if (!ELEM(true, do_isolate, extend)) {
    return;
  }

  /* Create PointerRNA and PropertyRNA for either Collection or LayerCollection. */
  ID *id = collection ? &collection->id : &scene->id;
  StructRNA *struct_rna = collection ? &RNA_Collection : &RNA_LayerCollection;
  void *data = collection ? (void *)collection : (void *)layer_collection;

  PointerRNA ptr = RNA_pointer_create_discrete(id, struct_rna, data);
  outliner_layer_or_collection_pointer_create(scene, layer_collection, collection, &ptr);
  PropertyRNA *layer_or_collection_prop = RNA_struct_type_find_property(struct_rna, propname);
  const bool value = RNA_property_boolean_get(&ptr, layer_or_collection_prop);

  PropertyRNA *base_or_object_prop = nullptr;
  if (layer_collection != nullptr) {
    /* If we are toggling Layer collections we still want to change the properties of the base
     * or the objects. If we have a matching property, toggle it as well, it can be nullptr. */
    struct_rna = collection ? &RNA_Object : &RNA_ObjectBase;
    base_or_object_prop = RNA_struct_type_find_property(struct_rna, propname);
  }

  if (extend) {
    outliner_collection_set_flag_recursive(scene,
                                           view_layer,
                                           layer_collection,
                                           collection,
                                           layer_or_collection_prop,
                                           base_or_object_prop,
                                           value);
  }
  else {
    outliner_collection_isolate_flag(scene,
                                     view_layer,
                                     layer_collection,
                                     collection,
                                     layer_or_collection_prop,
                                     propname,
                                     value);
  }

  /* We don't call RNA_property_update() due to performance, so we batch update them. */
  BKE_main_collection_sync_remap(bmain);
  DEG_relations_tag_update(bmain);
}

/**
 * Layer collection properties called from the ViewLayer mode.
 * Change the (non-excluded) collection children, and the objects nested to them all.
 */
static void view_layer__layer_collection_set_flag_recursive_fn(bContext *C,
                                                               void *poin,
                                                               void *poin2)
{
  LayerCollection *layer_collection = static_cast<LayerCollection *>(poin);
  const char *propname = static_cast<const char *>(poin2);
  outliner_collection_set_flag_recursive_fn(C, layer_collection, nullptr, propname);
}

/**
 * Collection properties called from the ViewLayer mode.
 * Change the (non-excluded) collection children, and the objects nested to them all.
 */
static void view_layer__collection_set_flag_recursive_fn(bContext *C, void *poin, void *poin2)
{
  LayerCollection *layer_collection = static_cast<LayerCollection *>(poin);
  const char *propname = static_cast<const char *>(poin2);
  outliner_collection_set_flag_recursive_fn(
      C, layer_collection, layer_collection->collection, propname);
}

/**
 * Collection properties called from the Scenes mode.
 * Change the collection children but no objects.
 */
static void scenes__collection_set_flag_recursive_fn(bContext *C, void *poin, void *poin2)
{
  Collection *collection = static_cast<Collection *>(poin);
  const char *propname = static_cast<const char *>(poin2);
  outliner_collection_set_flag_recursive_fn(C, nullptr, collection, propname);
}

static void namebutton_fn(bContext *C, void *tsep, char *oldname)
{
  Main *bmain = CTX_data_main(C);
  SpaceOutliner *space_outliner = CTX_wm_space_outliner(C);
  wmMsgBus *mbus = CTX_wm_message_bus(C);
  BLI_mempool *ts = space_outliner->treestore;
  TreeStoreElem *tselem = static_cast<TreeStoreElem *>(tsep);

  const char *undo_str = nullptr;

  /* Unfortunately at this point, the name of the ID has already been set to its new value. Revert
   * it to its old name, to be able to use the generic 'rename' function for IDs.
   *
   * NOTE: While utterly inelegant, performances are not really a concern here, so this is an
   * acceptable solution for now. */
  auto id_rename_helper = [bmain, tselem, oldname]() -> bool {
    std::string new_name = tselem->id->name + 2;
    BLI_strncpy_utf8(tselem->id->name + 2, oldname, sizeof(tselem->id->name) - 2);
    return ED_id_rename(*bmain, *tselem->id, new_name);
  };

  if (ts && tselem) {
    TreeElement *te = outliner_find_tree_element(&space_outliner->tree, tselem);

    if (ELEM(tselem->type, TSE_SOME_ID, TSE_LINKED_NODE_TREE)) {
      if (id_rename_helper()) {
        undo_str = CTX_N_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Rename Data-Block");
      }

      WM_msg_publish_rna_prop(mbus, tselem->id, tselem->id, ID, name);

      switch (GS(tselem->id->name)) {
        case ID_MA:
          WM_event_add_notifier(C, NC_MATERIAL, nullptr);
          break;
        case ID_TE:
          WM_event_add_notifier(C, NC_TEXTURE, nullptr);
          break;
        case ID_IM:
          WM_event_add_notifier(C, NC_IMAGE, nullptr);
          break;
        case ID_SCE:
          WM_event_add_notifier(C, NC_SCENE, nullptr);
          break;
        default:
          break;
      }
      WM_event_add_notifier(C, NC_ID | NA_RENAME, nullptr);

      /* Check the library target exists */
      if (te->idcode == ID_LI) {
        Library *lib = (Library *)tselem->id;
        char expanded[FILE_MAX];

        BKE_library_filepath_set(bmain, lib, lib->filepath);

        STRNCPY(expanded, lib->filepath);
        BLI_path_abs(expanded, BKE_main_blendfile_path(bmain));
        if (!BLI_exists(expanded)) {
          BKE_reportf(CTX_wm_reports(C),
                      RPT_ERROR,
                      "Library path '%s' does not exist, correct this before saving",
                      expanded);
        }
        else if (lib->id.tag & ID_TAG_MISSING) {
          BKE_reportf(CTX_wm_reports(C),
                      RPT_INFO,
                      "Library path '%s' is now valid, please reload the library",
                      expanded);
          lib->id.tag &= ~ID_TAG_MISSING;
        }
      }

      DEG_id_tag_update(tselem->id, ID_RECALC_SYNC_TO_EVAL);
    }
    else {
      switch (tselem->type) {
        case TSE_DEFGROUP: {
          Object *ob = (Object *)tselem->id;
          bDeformGroup *vg = static_cast<bDeformGroup *>(te->directdata);
          BKE_object_defgroup_unique_name(vg, ob);
          WM_msg_publish_rna_prop(mbus, &ob->id, vg, VertexGroup, name);
          DEG_id_tag_update(tselem->id, ID_RECALC_SYNC_TO_EVAL);
          undo_str = CTX_N_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Rename Vertex Group");
          break;
        }
        case TSE_NLA_ACTION: {
          /* The #tselem->id is a #bAction. */
          if (id_rename_helper()) {
            undo_str = CTX_N_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Rename Data-Block");
          }
          WM_msg_publish_rna_prop(mbus, tselem->id, tselem->id, ID, name);
          DEG_id_tag_update(tselem->id, ID_RECALC_SYNC_TO_EVAL);
          break;
        }
        case TSE_NLA_TRACK: {
          WM_event_add_notifier(C, NC_ANIMATION | ND_ANIMCHAN | NA_RENAME, nullptr);
          undo_str = CTX_N_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Rename NLA Track");
          break;
        }
        case TSE_MODIFIER: {
          WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER | NA_RENAME, nullptr);
          DEG_relations_tag_update(bmain);
          undo_str = CTX_N_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Rename Modifier");

          break;
        }
        case TSE_EBONE: {
          bArmature *arm = (bArmature *)tselem->id;
          if (arm->edbo) {
            EditBone *ebone = static_cast<EditBone *>(te->directdata);
            char newname[sizeof(ebone->name)];

            /* restore bone name */
            STRNCPY_UTF8(newname, ebone->name);
            STRNCPY_UTF8(ebone->name, oldname);
            ED_armature_bone_rename(bmain, arm, oldname, newname);
            WM_msg_publish_rna_prop(mbus, &arm->id, ebone, EditBone, name);
            WM_event_add_notifier(C, NC_OBJECT | ND_POSE, nullptr);
            DEG_id_tag_update(tselem->id, ID_RECALC_SYNC_TO_EVAL);
            undo_str = CTX_N_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Rename Edit Bone");
          }
          break;
        }

        case TSE_BONE: {
          TreeViewContext tvc;
          outliner_viewcontext_init(C, &tvc);

          bArmature *arm = (bArmature *)tselem->id;
          Bone *bone = static_cast<Bone *>(te->directdata);
          char newname[sizeof(bone->name)];

          /* always make current object active */
          tree_element_activate(C, tvc, te, OL_SETSEL_NORMAL, true);

          /* restore bone name */
          STRNCPY_UTF8(newname, bone->name);
          STRNCPY_UTF8(bone->name, oldname);
          ED_armature_bone_rename(bmain, arm, oldname, newname);
          WM_msg_publish_rna_prop(mbus, &arm->id, bone, Bone, name);
          WM_event_add_notifier(C, NC_OBJECT | ND_POSE, nullptr);
          DEG_id_tag_update(tselem->id, ID_RECALC_SYNC_TO_EVAL);
          undo_str = CTX_N_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Rename Bone");
          break;
        }
        case TSE_POSE_CHANNEL: {
          TreeViewContext tvc;
          outliner_viewcontext_init(C, &tvc);

          Object *ob = (Object *)tselem->id;
          bArmature *arm = (bArmature *)ob->data;
          bPoseChannel *pchan = static_cast<bPoseChannel *>(te->directdata);
          char newname[sizeof(pchan->name)];

          /* always make current pose-bone active */
          tree_element_activate(C, tvc, te, OL_SETSEL_NORMAL, true);

          BLI_assert(ob->type == OB_ARMATURE);

          /* restore bone name */
          STRNCPY_UTF8(newname, pchan->name);
          STRNCPY_UTF8(pchan->name, oldname);
          ED_armature_bone_rename(bmain, static_cast<bArmature *>(ob->data), oldname, newname);
          WM_msg_publish_rna_prop(mbus, &arm->id, pchan->bone, Bone, name);
          WM_event_add_notifier(C, NC_OBJECT | ND_POSE, nullptr);
          DEG_id_tag_update(tselem->id, ID_RECALC_SYNC_TO_EVAL);
          DEG_id_tag_update(&arm->id, ID_RECALC_SYNC_TO_EVAL);
          undo_str = CTX_N_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Rename Pose Bone");
          break;
        }
        case TSE_GP_LAYER: {
          bGPdata *gpd = (bGPdata *)tselem->id; /* id = GP Datablock */
          bGPDlayer *gpl = static_cast<bGPDlayer *>(te->directdata);

          /* always make layer active */
          BKE_gpencil_layer_active_set(gpd, gpl);

          /* XXX: name needs translation stuff. */
          BLI_uniquename(
              &gpd->layers, gpl, "GP Layer", '.', offsetof(bGPDlayer, info), sizeof(gpl->info));

          WM_msg_publish_rna_prop(mbus, &gpd->id, gpl, AnnotationLayer, info);
          DEG_id_tag_update(&gpd->id, ID_RECALC_GEOMETRY);
          WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_SELECTED, gpd);
          DEG_id_tag_update(tselem->id, ID_RECALC_SYNC_TO_EVAL);
          undo_str = CTX_N_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Rename Annotation Layer");
          break;
        }
        case TSE_GREASE_PENCIL_NODE: {
          GreasePencil &grease_pencil = *(GreasePencil *)tselem->id;
          bke::greasepencil::TreeNode &node =
              tree_element_cast<TreeElementGreasePencilNode>(te)->node();

          /* The node already has the new name set. To properly rename the node, we need to first
           * store the new name, restore the old name in the node, and then call the rename
           * function. */
          std::string new_name(node.name());
          node.set_name(oldname);
          grease_pencil.rename_node(*bmain, node, new_name);
          DEG_id_tag_update(&grease_pencil.id, ID_RECALC_SYNC_TO_EVAL);
          WM_event_add_notifier(C, NC_ID | NA_RENAME, nullptr);
          undo_str = CTX_N_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Rename Grease Pencil Drawing");
          break;
        }
        case TSE_R_LAYER: {
          Scene *scene = (Scene *)tselem->id;
          ViewLayer *view_layer = static_cast<ViewLayer *>(te->directdata);

          /* Restore old name. */
          char newname[sizeof(view_layer->name)];
          STRNCPY_UTF8(newname, view_layer->name);
          STRNCPY_UTF8(view_layer->name, oldname);

          /* Rename, preserving animation and compositing data. */
          BKE_view_layer_rename(bmain, scene, view_layer, newname);
          WM_msg_publish_rna_prop(mbus, &scene->id, view_layer, ViewLayer, name);
          WM_event_add_notifier(C, NC_ID | NA_RENAME, nullptr);
          DEG_id_tag_update(tselem->id, ID_RECALC_SYNC_TO_EVAL);
          undo_str = CTX_N_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Rename View Layer");
          break;
        }
        case TSE_LAYER_COLLECTION: {
          /* The #tselem->id is a #Collection, not a #LayerCollection */
          if (id_rename_helper()) {
            undo_str = CTX_N_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Rename Data-Block");
          }
          WM_msg_publish_rna_prop(mbus, tselem->id, tselem->id, ID, name);
          WM_event_add_notifier(C, NC_ID | NA_RENAME, nullptr);
          DEG_id_tag_update(tselem->id, ID_RECALC_SYNC_TO_EVAL);
          break;
        }

        case TSE_BONE_COLLECTION: {
          bArmature *arm = (bArmature *)tselem->id;
          BoneCollection *bcoll = static_cast<BoneCollection *>(te->directdata);

          ANIM_armature_bonecoll_name_set(arm, bcoll, bcoll->name);
          WM_msg_publish_rna_prop(mbus, &arm->id, bcoll, BoneCollection, name);
          WM_event_add_notifier(C, NC_OBJECT | ND_BONE_COLLECTION, arm);
          DEG_id_tag_update(&arm->id, ID_RECALC_SYNC_TO_EVAL);
          undo_str = CTX_N_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Rename Bone Collection");
          break;
        }

        case TSE_ACTION_SLOT: {
          WM_event_add_notifier(C, NC_ID | NA_RENAME, nullptr);
          undo_str = CTX_N_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Rename Action Slot");
          break;
        }
      }
    }
    tselem->flag &= ~TSE_TEXTBUT;
  }

  if (undo_str) {
    ED_undo_push(C, undo_str);
  }
}

struct RestrictProperties {
  bool initialized;

  PropertyRNA *object_hide_viewport, *object_hide_select, *object_hide_render;
  PropertyRNA *base_hide_viewport;
  PropertyRNA *collection_hide_viewport, *collection_hide_select, *collection_hide_render;
  PropertyRNA *layer_collection_exclude, *layer_collection_holdout,
      *layer_collection_indirect_only, *layer_collection_hide_viewport;
  PropertyRNA *modifier_show_viewport, *modifier_show_render;
  PropertyRNA *constraint_enable;
  PropertyRNA *bone_hide_viewport;
};

/* We don't care about the value of the property
 * but whether the property should be active or grayed out. */
struct RestrictPropertiesActive {
  bool object_hide_viewport;
  bool object_hide_select;
  bool object_hide_render;
  bool base_hide_viewport;
  bool collection_hide_viewport;
  bool collection_hide_select;
  bool collection_hide_render;
  bool layer_collection_exclude;
  bool layer_collection_holdout;
  bool layer_collection_indirect_only;
  bool layer_collection_hide_viewport;
  bool modifier_show_viewport;
  bool modifier_show_render;
  bool constraint_enable;
  bool bone_hide_viewport;
};

static void outliner_restrict_properties_enable_collection_set(
    PointerRNA *collection_ptr, RestrictProperties *props, RestrictPropertiesActive *props_active)
{
  if (props_active->collection_hide_render) {
    props_active->collection_hide_render = !RNA_property_boolean_get(
        collection_ptr, props->collection_hide_render);
    if (!props_active->collection_hide_render) {
      props_active->layer_collection_holdout = false;
      props_active->layer_collection_indirect_only = false;
      props_active->object_hide_render = false;
      props_active->modifier_show_render = false;
      props_active->constraint_enable = false;
    }
  }

  if (props_active->collection_hide_viewport) {
    props_active->collection_hide_viewport = !RNA_property_boolean_get(
        collection_ptr, props->collection_hide_viewport);
    if (!props_active->collection_hide_viewport) {
      props_active->collection_hide_select = false;
      props_active->object_hide_select = false;
      props_active->layer_collection_hide_viewport = false;
      props_active->object_hide_viewport = false;
      props_active->base_hide_viewport = false;
      props_active->modifier_show_viewport = false;
      props_active->constraint_enable = false;
    }
  }

  if (props_active->collection_hide_select) {
    props_active->collection_hide_select = !RNA_property_boolean_get(
        collection_ptr, props->collection_hide_select);
    if (!props_active->collection_hide_select) {
      props_active->object_hide_select = false;
    }
  }
}

static void outliner_restrict_properties_enable_layer_collection_set(
    PointerRNA *layer_collection_ptr,
    PointerRNA *collection_ptr,
    RestrictProperties *props,
    RestrictPropertiesActive *props_active)
{
  outliner_restrict_properties_enable_collection_set(collection_ptr, props, props_active);

  if (props_active->layer_collection_holdout) {
    props_active->layer_collection_holdout = RNA_property_boolean_get(
        layer_collection_ptr, props->layer_collection_holdout);
  }

  props_active->layer_collection_indirect_only = RNA_property_boolean_get(
      layer_collection_ptr, props->layer_collection_indirect_only);

  if (props_active->layer_collection_hide_viewport) {
    props_active->layer_collection_hide_viewport = !RNA_property_boolean_get(
        layer_collection_ptr, props->layer_collection_hide_viewport);

    if (!props_active->layer_collection_hide_viewport) {
      props_active->base_hide_viewport = false;
      props_active->collection_hide_select = false;
      props_active->object_hide_select = false;
    }
  }

  if (props_active->layer_collection_exclude) {
    props_active->layer_collection_exclude = !RNA_property_boolean_get(
        layer_collection_ptr, props->layer_collection_exclude);

    if (!props_active->layer_collection_exclude) {
      props_active->collection_hide_viewport = false;
      props_active->collection_hide_select = false;
      props_active->collection_hide_render = false;
      props_active->layer_collection_hide_viewport = false;
      props_active->layer_collection_holdout = false;
      props_active->layer_collection_indirect_only = false;
    }
  }
}

static bool outliner_restrict_properties_collection_set(Scene *scene,
                                                        TreeElement *te,
                                                        PointerRNA *collection_ptr,
                                                        PointerRNA *layer_collection_ptr,
                                                        RestrictProperties *props,
                                                        RestrictPropertiesActive *props_active)
{
  TreeStoreElem *tselem = TREESTORE(te);
  LayerCollection *layer_collection = (tselem->type == TSE_LAYER_COLLECTION) ?
                                          static_cast<LayerCollection *>(te->directdata) :
                                          nullptr;
  Collection *collection = outliner_collection_from_tree_element(te);

  if (collection->flag & COLLECTION_IS_MASTER) {
    return false;
  }

  /* Create the PointerRNA. */
  *collection_ptr = RNA_id_pointer_create(&collection->id);
  if (layer_collection != nullptr) {
    *layer_collection_ptr = RNA_pointer_create_discrete(
        &scene->id, &RNA_LayerCollection, layer_collection);
  }

  /* Update the restriction column values for the collection children. */
  if (layer_collection) {
    outliner_restrict_properties_enable_layer_collection_set(
        layer_collection_ptr, collection_ptr, props, props_active);
  }
  else {
    outliner_restrict_properties_enable_collection_set(collection_ptr, props, props_active);
  }
  return true;
}

static void outliner_draw_restrictbuts(uiBlock *block,
                                       Scene *scene,
                                       ViewLayer *view_layer,
                                       ARegion *region,
                                       SpaceOutliner *space_outliner,
                                       ListBase *lb,
                                       RestrictPropertiesActive props_active_parent)
{
  /* Get RNA properties (once for speed). */
  static RestrictProperties props = {false};
  if (!props.initialized) {
    props.object_hide_viewport = RNA_struct_type_find_property(&RNA_Object, "hide_viewport");
    props.object_hide_select = RNA_struct_type_find_property(&RNA_Object, "hide_select");
    props.object_hide_render = RNA_struct_type_find_property(&RNA_Object, "hide_render");
    props.base_hide_viewport = RNA_struct_type_find_property(&RNA_ObjectBase, "hide_viewport");
    props.collection_hide_viewport = RNA_struct_type_find_property(&RNA_Collection,
                                                                   "hide_viewport");
    props.collection_hide_select = RNA_struct_type_find_property(&RNA_Collection, "hide_select");
    props.collection_hide_render = RNA_struct_type_find_property(&RNA_Collection, "hide_render");
    props.layer_collection_exclude = RNA_struct_type_find_property(&RNA_LayerCollection,
                                                                   "exclude");
    props.layer_collection_holdout = RNA_struct_type_find_property(&RNA_LayerCollection,
                                                                   "holdout");
    props.layer_collection_indirect_only = RNA_struct_type_find_property(&RNA_LayerCollection,
                                                                         "indirect_only");
    props.layer_collection_hide_viewport = RNA_struct_type_find_property(&RNA_LayerCollection,
                                                                         "hide_viewport");
    props.modifier_show_viewport = RNA_struct_type_find_property(&RNA_Modifier, "show_viewport");
    props.modifier_show_render = RNA_struct_type_find_property(&RNA_Modifier, "show_render");

    props.constraint_enable = RNA_struct_type_find_property(&RNA_Constraint, "enabled");

    props.bone_hide_viewport = RNA_struct_type_find_property(&RNA_PoseBone, "hide");

    props.initialized = true;
  }

  struct {
    int enable;
    int select;
    int hide;
    int viewport;
    int render;
    int indirect_only;
    int holdout;
  } restrict_offsets = {0};
  int restrict_column_offset = 0;

  /* This will determine the order of drawing from RIGHT to LEFT. */
  if (space_outliner->outlinevis == SO_VIEW_LAYER) {
    if (space_outliner->show_restrict_flags & SO_RESTRICT_INDIRECT_ONLY) {
      restrict_offsets.indirect_only = (++restrict_column_offset) * UI_UNIT_X + V2D_SCROLL_WIDTH;
    }
    if (space_outliner->show_restrict_flags & SO_RESTRICT_HOLDOUT) {
      restrict_offsets.holdout = (++restrict_column_offset) * UI_UNIT_X + V2D_SCROLL_WIDTH;
    }
  }
  if (space_outliner->show_restrict_flags & SO_RESTRICT_RENDER) {
    restrict_offsets.render = (++restrict_column_offset) * UI_UNIT_X + V2D_SCROLL_WIDTH;
  }
  if (space_outliner->show_restrict_flags & SO_RESTRICT_VIEWPORT) {
    restrict_offsets.viewport = (++restrict_column_offset) * UI_UNIT_X + V2D_SCROLL_WIDTH;
  }
  if (space_outliner->show_restrict_flags & SO_RESTRICT_HIDE) {
    restrict_offsets.hide = (++restrict_column_offset) * UI_UNIT_X + V2D_SCROLL_WIDTH;
  }
  if (space_outliner->show_restrict_flags & SO_RESTRICT_SELECT) {
    restrict_offsets.select = (++restrict_column_offset) * UI_UNIT_X + V2D_SCROLL_WIDTH;
  }
  if (space_outliner->outlinevis == SO_VIEW_LAYER &&
      space_outliner->show_restrict_flags & SO_RESTRICT_ENABLE)
  {
    restrict_offsets.enable = (++restrict_column_offset) * UI_UNIT_X + V2D_SCROLL_WIDTH;
  }

  BLI_assert((restrict_column_offset * UI_UNIT_X + V2D_SCROLL_WIDTH) ==
             outliner_right_columns_width(space_outliner));

  /* Create buttons. */
  uiBut *bt;

  LISTBASE_FOREACH (TreeElement *, te, lb) {
    TreeStoreElem *tselem = TREESTORE(te);
    RestrictPropertiesActive props_active = props_active_parent;

    if (te->ys + 2 * UI_UNIT_Y >= region->v2d.cur.ymin && te->ys <= region->v2d.cur.ymax) {
      if (tselem->type == TSE_R_LAYER &&
          ELEM(space_outliner->outlinevis, SO_SCENES, SO_VIEW_LAYER))
      {
        if (space_outliner->show_restrict_flags & SO_RESTRICT_RENDER) {
          /* View layer render toggle. */
          ViewLayer *layer = static_cast<ViewLayer *>(te->directdata);

          bt = uiDefIconButBitS(block,
                                ButType::IconToggleN,
                                VIEW_LAYER_RENDER,
                                0,
                                ICON_RESTRICT_RENDER_OFF,
                                int(region->v2d.cur.xmax - restrict_offsets.render),
                                te->ys,
                                UI_UNIT_X,
                                UI_UNIT_Y,
                                &layer->flag,
                                0,
                                0,
                                TIP_("Use view layer for rendering"));
          UI_but_func_set(bt, restrictbutton_r_lay_fn, tselem->id, nullptr);
          UI_but_flag_enable(bt, UI_BUT_DRAG_LOCK);
          UI_but_drawflag_enable(bt, UI_BUT_ICON_REVERSE);
        }
      }
      else if (((tselem->type == TSE_SOME_ID) && (te->idcode == ID_OB)) &&
               (te->flag & TE_CHILD_NOT_IN_COLLECTION))
      {
        /* Don't show restrict columns for children that are not directly inside the collection. */
      }
      else if ((tselem->type == TSE_SOME_ID) && (te->idcode == ID_OB)) {
        Object *ob = (Object *)tselem->id;
        PointerRNA ptr = RNA_id_pointer_create(&ob->id);

        if (space_outliner->show_restrict_flags & SO_RESTRICT_HIDE) {
          BKE_view_layer_synced_ensure(scene, view_layer);
          Base *base = (te->directdata) ? (Base *)te->directdata :
                                          BKE_view_layer_base_find(view_layer, ob);
          if (base) {
            PointerRNA base_ptr = RNA_pointer_create_discrete(&scene->id, &RNA_ObjectBase, base);
            bt = uiDefIconButR_prop(block,
                                    ButType::IconToggle,
                                    0,
                                    ICON_NONE,
                                    int(region->v2d.cur.xmax - restrict_offsets.hide),
                                    te->ys,
                                    UI_UNIT_X,
                                    UI_UNIT_Y,
                                    &base_ptr,
                                    props.base_hide_viewport,
                                    -1,
                                    0,
                                    0,
                                    TIP_("Temporarily hide in viewport\n"
                                         " \u2022 Shift to set children"));
            UI_but_func_set(
                bt, outliner__base_set_flag_recursive_fn, base, (void *)"hide_viewport");
            UI_but_flag_enable(bt, UI_BUT_DRAG_LOCK);
            if (!props_active.base_hide_viewport) {
              UI_but_flag_enable(bt, UI_BUT_INACTIVE);
            }
          }
        }

        if (space_outliner->show_restrict_flags & SO_RESTRICT_SELECT) {
          bt = uiDefIconButR_prop(block,
                                  ButType::IconToggle,
                                  0,
                                  ICON_NONE,
                                  int(region->v2d.cur.xmax - restrict_offsets.select),
                                  te->ys,
                                  UI_UNIT_X,
                                  UI_UNIT_Y,
                                  &ptr,
                                  props.object_hide_select,
                                  -1,
                                  0,
                                  0,
                                  TIP_("Disable selection in viewport\n"
                                       " \u2022 Shift to set children"));
          UI_but_func_set(bt, outliner__object_set_flag_recursive_fn, ob, (char *)"hide_select");
          UI_but_flag_enable(bt, UI_BUT_DRAG_LOCK);
          if (!props_active.object_hide_select) {
            UI_but_flag_enable(bt, UI_BUT_INACTIVE);
          }
        }

        if (space_outliner->show_restrict_flags & SO_RESTRICT_VIEWPORT) {
          bt = uiDefIconButR_prop(block,
                                  ButType::IconToggle,
                                  0,
                                  ICON_NONE,
                                  int(region->v2d.cur.xmax - restrict_offsets.viewport),
                                  te->ys,
                                  UI_UNIT_X,
                                  UI_UNIT_Y,
                                  &ptr,
                                  props.object_hide_viewport,
                                  -1,
                                  0,
                                  0,
                                  TIP_("Globally disable in viewports\n"
                                       " \u2022 Shift to set children"));
          UI_but_func_set(bt, outliner__object_set_flag_recursive_fn, ob, (void *)"hide_viewport");
          UI_but_flag_enable(bt, UI_BUT_DRAG_LOCK);
          if (!props_active.object_hide_viewport) {
            UI_but_flag_enable(bt, UI_BUT_INACTIVE);
          }
        }

        if (space_outliner->show_restrict_flags & SO_RESTRICT_RENDER) {
          bt = uiDefIconButR_prop(block,
                                  ButType::IconToggle,
                                  0,
                                  ICON_NONE,
                                  int(region->v2d.cur.xmax - restrict_offsets.render),
                                  te->ys,
                                  UI_UNIT_X,
                                  UI_UNIT_Y,
                                  &ptr,
                                  props.object_hide_render,
                                  -1,
                                  0,
                                  0,
                                  TIP_("Globally disable in renders\n"
                                       " \u2022 Shift to set children"));
          UI_but_func_set(bt, outliner__object_set_flag_recursive_fn, ob, (char *)"hide_render");
          UI_but_flag_enable(bt, UI_BUT_DRAG_LOCK);
          if (!props_active.object_hide_render) {
            UI_but_flag_enable(bt, UI_BUT_INACTIVE);
          }
        }
      }
      else if (tselem->type == TSE_CONSTRAINT) {
        bConstraint *con = (bConstraint *)te->directdata;

        PointerRNA ptr = RNA_pointer_create_discrete(tselem->id, &RNA_Constraint, con);

        if (space_outliner->show_restrict_flags & SO_RESTRICT_HIDE) {
          bt = uiDefIconButR_prop(block,
                                  ButType::IconToggle,
                                  0,
                                  ICON_NONE,
                                  int(region->v2d.cur.xmax - restrict_offsets.hide),
                                  te->ys,
                                  UI_UNIT_X,
                                  UI_UNIT_Y,
                                  &ptr,
                                  props.constraint_enable,
                                  -1,
                                  0,
                                  0,
                                  nullptr);
          UI_but_flag_enable(bt, UI_BUT_DRAG_LOCK);
          if (!props_active.constraint_enable) {
            UI_but_flag_enable(bt, UI_BUT_INACTIVE);
          }
        }
      }
      else if (tselem->type == TSE_MODIFIER) {
        ModifierData *md = (ModifierData *)te->directdata;

        PointerRNA ptr = RNA_pointer_create_discrete(tselem->id, &RNA_Modifier, md);

        if (space_outliner->show_restrict_flags & SO_RESTRICT_VIEWPORT) {
          bt = uiDefIconButR_prop(block,
                                  ButType::IconToggle,
                                  0,
                                  ICON_NONE,
                                  int(region->v2d.cur.xmax - restrict_offsets.viewport),
                                  te->ys,
                                  UI_UNIT_X,
                                  UI_UNIT_Y,
                                  &ptr,
                                  props.modifier_show_viewport,
                                  -1,
                                  0,
                                  0,
                                  std::nullopt);
          UI_but_flag_enable(bt, UI_BUT_DRAG_LOCK);
          if (!props_active.modifier_show_viewport) {
            UI_but_flag_enable(bt, UI_BUT_INACTIVE);
          }
        }

        if (space_outliner->show_restrict_flags & SO_RESTRICT_RENDER) {
          bt = uiDefIconButR_prop(block,
                                  ButType::IconToggle,
                                  0,
                                  ICON_NONE,
                                  int(region->v2d.cur.xmax - restrict_offsets.render),
                                  te->ys,
                                  UI_UNIT_X,
                                  UI_UNIT_Y,
                                  &ptr,
                                  props.modifier_show_render,
                                  -1,
                                  0,
                                  0,
                                  std::nullopt);
          UI_but_flag_enable(bt, UI_BUT_DRAG_LOCK);
          if (!props_active.modifier_show_render) {
            UI_but_flag_enable(bt, UI_BUT_INACTIVE);
          }
        }
      }
      else if (tselem->type == TSE_POSE_CHANNEL) {
        bPoseChannel *pchan = (bPoseChannel *)te->directdata;
        Bone *bone = pchan->bone;
        Object *ob = (Object *)tselem->id;
        bArmature *arm = static_cast<bArmature *>(ob->data);

        PointerRNA ptr = RNA_pointer_create_discrete(&arm->id, &RNA_PoseBone, pchan);

        if (space_outliner->show_restrict_flags & SO_RESTRICT_VIEWPORT) {
          bt = uiDefIconButR_prop(block,
                                  ButType::IconToggle,
                                  0,
                                  ICON_NONE,
                                  int(region->v2d.cur.xmax - restrict_offsets.viewport),
                                  te->ys,
                                  UI_UNIT_X,
                                  UI_UNIT_Y,
                                  &ptr,
                                  props.bone_hide_viewport,
                                  -1,
                                  0,
                                  0,
                                  TIP_("Restrict visibility in the 3D View\n"
                                       " \u2022 Shift to set children"));
          UI_but_func_set(bt, restrictbutton_bone_visibility_fn, ob, pchan);
          UI_but_flag_enable(bt, UI_BUT_DRAG_LOCK);
          UI_but_drawflag_enable(bt, UI_BUT_ICON_REVERSE);
        }

        if (space_outliner->show_restrict_flags & SO_RESTRICT_SELECT) {
          bt = uiDefIconButBitI(block,
                                ButType::IconToggle,
                                BONE_UNSELECTABLE,
                                0,
                                ICON_RESTRICT_SELECT_OFF,
                                int(region->v2d.cur.xmax - restrict_offsets.select),
                                te->ys,
                                UI_UNIT_X,
                                UI_UNIT_Y,
                                &(bone->flag),
                                0,
                                0,
                                TIP_("Restrict selection in the 3D View\n"
                                     " \u2022 Shift to set children"));
          UI_but_func_set(bt, restrictbutton_bone_select_fn, ob->data, bone);
          UI_but_flag_enable(bt, UI_BUT_DRAG_LOCK);
          UI_but_drawflag_enable(bt, UI_BUT_ICON_REVERSE);
        }
      }
      else if (tselem->type == TSE_EBONE) {
        bArmature *arm = (bArmature *)tselem->id;
        EditBone *ebone = (EditBone *)te->directdata;

        if (space_outliner->show_restrict_flags & SO_RESTRICT_VIEWPORT) {
          bt = uiDefIconButBitI(block,
                                ButType::IconToggle,
                                BONE_HIDDEN_A,
                                0,
                                ICON_RESTRICT_VIEW_OFF,
                                int(region->v2d.cur.xmax - restrict_offsets.viewport),
                                te->ys,
                                UI_UNIT_X,
                                UI_UNIT_Y,
                                &(ebone->flag),
                                0,
                                0,
                                TIP_("Restrict visibility in the 3D View\n"
                                     " \u2022 Shift to set children"));
          UI_but_func_set(bt, restrictbutton_ebone_visibility_fn, arm, ebone);
          UI_but_flag_enable(bt, UI_BUT_DRAG_LOCK);
          UI_but_drawflag_enable(bt, UI_BUT_ICON_REVERSE);
        }

        if (space_outliner->show_restrict_flags & SO_RESTRICT_SELECT) {
          bt = uiDefIconButBitI(block,
                                ButType::IconToggle,
                                BONE_UNSELECTABLE,
                                0,
                                ICON_RESTRICT_SELECT_OFF,
                                int(region->v2d.cur.xmax - restrict_offsets.select),
                                te->ys,
                                UI_UNIT_X,
                                UI_UNIT_Y,
                                &(ebone->flag),
                                0,
                                0,
                                TIP_("Restrict selection in the 3D View\n"
                                     " \u2022 Shift to set children"));
          UI_but_func_set(bt, restrictbutton_ebone_select_fn, arm, ebone);
          UI_but_flag_enable(bt, UI_BUT_DRAG_LOCK);
          UI_but_drawflag_enable(bt, UI_BUT_ICON_REVERSE);
        }
      }
      else if (tselem->type == TSE_GP_LAYER) {
        ID *id = tselem->id;
        bGPDlayer *gpl = (bGPDlayer *)te->directdata;

        if (space_outliner->show_restrict_flags & SO_RESTRICT_HIDE) {
          bt = uiDefIconButBitS(block,
                                ButType::IconToggle,
                                GP_LAYER_HIDE,
                                0,
                                ICON_HIDE_OFF,
                                int(region->v2d.cur.xmax - restrict_offsets.hide),
                                te->ys,
                                UI_UNIT_X,
                                UI_UNIT_Y,
                                &gpl->flag,
                                0,
                                0,
                                TIP_("Restrict visibility in the 3D View"));
          UI_but_func_set(bt, restrictbutton_gp_layer_flag_fn, id, gpl);
          UI_but_flag_enable(bt, UI_BUT_DRAG_LOCK);
          UI_but_drawflag_enable(bt, UI_BUT_ICON_REVERSE);
        }

        if (space_outliner->show_restrict_flags & SO_RESTRICT_SELECT) {
          bt = uiDefIconButBitS(block,
                                ButType::IconToggle,
                                GP_LAYER_LOCKED,
                                0,
                                ICON_UNLOCKED,
                                int(region->v2d.cur.xmax - restrict_offsets.select),
                                te->ys,
                                UI_UNIT_X,
                                UI_UNIT_Y,
                                &gpl->flag,
                                0,
                                0,
                                TIP_("Restrict editing of strokes and keyframes in this layer"));
          UI_but_func_set(bt, restrictbutton_gp_layer_flag_fn, id, gpl);
          UI_but_flag_enable(bt, UI_BUT_DRAG_LOCK);
        }
      }
      else if (tselem->type == TSE_GREASE_PENCIL_NODE) {
        bke::greasepencil::TreeNode &node =
            tree_element_cast<TreeElementGreasePencilNode>(te)->node();
        PointerRNA ptr;
        PropertyRNA *hide_prop;
        if (node.is_layer()) {
          ptr = RNA_pointer_create_discrete(tselem->id, &RNA_GreasePencilLayer, &node);
          hide_prop = RNA_struct_type_find_property(&RNA_GreasePencilLayer, "hide");
        }
        else if (node.is_group()) {
          ptr = RNA_pointer_create_discrete(tselem->id, &RNA_GreasePencilLayerGroup, &node);
          hide_prop = RNA_struct_type_find_property(&RNA_GreasePencilLayerGroup, "hide");
        }

        if (space_outliner->show_restrict_flags & SO_RESTRICT_HIDE) {
          bt = uiDefIconButR_prop(block,
                                  ButType::IconToggle,
                                  0,
                                  0,
                                  int(region->v2d.cur.xmax - restrict_offsets.hide),
                                  te->ys,
                                  UI_UNIT_X,
                                  UI_UNIT_Y,
                                  &ptr,
                                  hide_prop,
                                  -1,
                                  0,
                                  0,
                                  std::nullopt);
          UI_but_flag_enable(bt, UI_BUT_DRAG_LOCK);
          if (node.parent_group() && !node.parent_group()->is_visible()) {
            UI_but_flag_enable(bt, UI_BUT_INACTIVE);
          }
        }
      }
      else if (outliner_is_collection_tree_element(te)) {
        PointerRNA collection_ptr;
        PointerRNA layer_collection_ptr;

        if (outliner_restrict_properties_collection_set(
                scene, te, &collection_ptr, &layer_collection_ptr, &props, &props_active))
        {

          LayerCollection *layer_collection = (tselem->type == TSE_LAYER_COLLECTION) ?
                                                  static_cast<LayerCollection *>(te->directdata) :
                                                  nullptr;
          Collection *collection = outliner_collection_from_tree_element(te);

          if (layer_collection != nullptr) {
            if (space_outliner->show_restrict_flags & SO_RESTRICT_ENABLE) {
              bt = uiDefIconButR_prop(block,
                                      ButType::IconToggle,
                                      0,
                                      ICON_NONE,
                                      int(region->v2d.cur.xmax) - restrict_offsets.enable,
                                      te->ys,
                                      UI_UNIT_X,
                                      UI_UNIT_Y,
                                      &layer_collection_ptr,
                                      props.layer_collection_exclude,
                                      -1,
                                      0,
                                      0,
                                      std::nullopt);
              UI_but_func_set(bt,
                              view_layer__layer_collection_set_flag_recursive_fn,
                              layer_collection,
                              (char *)"exclude");
              UI_but_flag_enable(bt, UI_BUT_DRAG_LOCK);
            }

            if (space_outliner->show_restrict_flags & SO_RESTRICT_HIDE) {
              bt = uiDefIconButR_prop(block,
                                      ButType::IconToggle,
                                      0,
                                      ICON_NONE,
                                      int(region->v2d.cur.xmax - restrict_offsets.hide),
                                      te->ys,
                                      UI_UNIT_X,
                                      UI_UNIT_Y,
                                      &layer_collection_ptr,
                                      props.layer_collection_hide_viewport,
                                      -1,
                                      0,
                                      0,
                                      TIP_("Temporarily hide in viewport\n"
                                           " \u2022 Ctrl to isolate collection\n"
                                           " \u2022 Shift to set inside collections and objects"));
              UI_but_func_set(bt,
                              view_layer__layer_collection_set_flag_recursive_fn,
                              layer_collection,
                              (char *)"hide_viewport");
              UI_but_flag_enable(bt, UI_BUT_DRAG_LOCK);
              if (!props_active.layer_collection_hide_viewport) {
                UI_but_flag_enable(bt, UI_BUT_INACTIVE);
              }
            }

            if (space_outliner->show_restrict_flags & SO_RESTRICT_HOLDOUT) {
              bt = uiDefIconButR_prop(block,
                                      ButType::IconToggle,
                                      0,
                                      ICON_NONE,
                                      int(region->v2d.cur.xmax - restrict_offsets.holdout),
                                      te->ys,
                                      UI_UNIT_X,
                                      UI_UNIT_Y,
                                      &layer_collection_ptr,
                                      props.layer_collection_holdout,
                                      -1,
                                      0,
                                      0,
                                      TIP_("Mask out objects in collection from view layer\n"
                                           " \u2022 Ctrl to isolate collection\n"
                                           " \u2022 Shift to set inside collections"));
              UI_but_func_set(bt,
                              view_layer__layer_collection_set_flag_recursive_fn,
                              layer_collection,
                              (char *)"holdout");
              UI_but_flag_enable(bt, UI_BUT_DRAG_LOCK);
              if (!props_active.layer_collection_holdout) {
                UI_but_flag_enable(bt, UI_BUT_INACTIVE);
              }
            }

            if (space_outliner->show_restrict_flags & SO_RESTRICT_INDIRECT_ONLY) {
              bt = uiDefIconButR_prop(
                  block,
                  ButType::IconToggle,
                  0,
                  ICON_NONE,
                  int(region->v2d.cur.xmax - restrict_offsets.indirect_only),
                  te->ys,
                  UI_UNIT_X,
                  UI_UNIT_Y,
                  &layer_collection_ptr,
                  props.layer_collection_indirect_only,
                  -1,
                  0,
                  0,
                  TIP_("Objects in collection only contribute indirectly (through shadows and "
                       "reflections) in the view layer\n"
                       " \u2022 Ctrl to isolate collection\n"
                       " \u2022 Shift to set inside collections"));
              UI_but_func_set(bt,
                              view_layer__layer_collection_set_flag_recursive_fn,
                              layer_collection,
                              (char *)"indirect_only");
              UI_but_flag_enable(bt, UI_BUT_DRAG_LOCK);
              if (props_active.layer_collection_holdout ||
                  !props_active.layer_collection_indirect_only)
              {
                UI_but_flag_enable(bt, UI_BUT_INACTIVE);
              }
            }
          }

          if (space_outliner->show_restrict_flags & SO_RESTRICT_VIEWPORT) {
            bt = uiDefIconButR_prop(block,
                                    ButType::IconToggle,
                                    0,
                                    ICON_NONE,
                                    int(region->v2d.cur.xmax - restrict_offsets.viewport),
                                    te->ys,
                                    UI_UNIT_X,
                                    UI_UNIT_Y,
                                    &collection_ptr,
                                    props.collection_hide_viewport,
                                    -1,
                                    0,
                                    0,
                                    TIP_("Globally disable in viewports\n"
                                         " \u2022 Ctrl to isolate collection\n"
                                         " \u2022 Shift to set inside collections and objects"));
            if (layer_collection != nullptr) {
              UI_but_func_set(bt,
                              view_layer__collection_set_flag_recursive_fn,
                              layer_collection,
                              (char *)"hide_viewport");
            }
            else {
              UI_but_func_set(bt,
                              scenes__collection_set_flag_recursive_fn,
                              collection,
                              (char *)"hide_viewport");
            }
            UI_but_flag_enable(bt, UI_BUT_DRAG_LOCK);
            if (!props_active.collection_hide_viewport) {
              UI_but_flag_enable(bt, UI_BUT_INACTIVE);
            }
          }

          if (space_outliner->show_restrict_flags & SO_RESTRICT_RENDER) {
            bt = uiDefIconButR_prop(block,
                                    ButType::IconToggle,
                                    0,
                                    ICON_NONE,
                                    int(region->v2d.cur.xmax - restrict_offsets.render),
                                    te->ys,
                                    UI_UNIT_X,
                                    UI_UNIT_Y,
                                    &collection_ptr,
                                    props.collection_hide_render,
                                    -1,
                                    0,
                                    0,
                                    TIP_("Globally disable in renders\n"
                                         " \u2022 Ctrl to isolate collection\n"
                                         " \u2022 Shift to set inside collections and objects"));
            if (layer_collection != nullptr) {
              UI_but_func_set(bt,
                              view_layer__collection_set_flag_recursive_fn,
                              layer_collection,
                              (char *)"hide_render");
            }
            else {
              UI_but_func_set(
                  bt, scenes__collection_set_flag_recursive_fn, collection, (char *)"hide_render");
            }
            UI_but_flag_enable(bt, UI_BUT_DRAG_LOCK);
            if (!props_active.collection_hide_render) {
              UI_but_flag_enable(bt, UI_BUT_INACTIVE);
            }
          }

          if (space_outliner->show_restrict_flags & SO_RESTRICT_SELECT) {
            bt = uiDefIconButR_prop(block,
                                    ButType::IconToggle,
                                    0,
                                    ICON_NONE,
                                    int(region->v2d.cur.xmax - restrict_offsets.select),
                                    te->ys,
                                    UI_UNIT_X,
                                    UI_UNIT_Y,
                                    &collection_ptr,
                                    props.collection_hide_select,
                                    -1,
                                    0,
                                    0,
                                    TIP_("Disable selection in viewport\n"
                                         " \u2022 Ctrl to isolate collection\n"
                                         " \u2022 Shift to set inside collections and objects"));
            if (layer_collection != nullptr) {
              UI_but_func_set(bt,
                              view_layer__collection_set_flag_recursive_fn,
                              layer_collection,
                              (char *)"hide_select");
            }
            else {
              UI_but_func_set(
                  bt, scenes__collection_set_flag_recursive_fn, collection, (char *)"hide_select");
            }
            UI_but_flag_enable(bt, UI_BUT_DRAG_LOCK);
            if (!props_active.collection_hide_select) {
              UI_but_flag_enable(bt, UI_BUT_INACTIVE);
            }
          }
        }
      }
    }
    else if (outliner_is_collection_tree_element(te)) {
      PointerRNA collection_ptr;
      PointerRNA layer_collection_ptr;
      outliner_restrict_properties_collection_set(
          scene, te, &collection_ptr, &layer_collection_ptr, &props, &props_active);
    }

    if (TSELEM_OPEN(tselem, space_outliner)) {
      outliner_draw_restrictbuts(
          block, scene, view_layer, region, space_outliner, &te->subtree, props_active);
    }
  }
}

static void outliner_draw_userbuts(uiBlock *block,
                                   const ARegion *region,
                                   const SpaceOutliner *space_outliner)
{
  tree_iterator::all_open(*space_outliner, [&](const TreeElement *te) {
    if (!outliner_is_element_in_view(te, &region->v2d)) {
      return;
    }

    const TreeStoreElem *tselem = TREESTORE(te);
    ID *id = tselem->id;

    if (tselem->type != TSE_SOME_ID || id->tag & ID_TAG_EXTRAUSER) {
      return;
    }

    uiBut *bt;
    std::optional<StringRef> tip;
    const int real_users = id->us - ID_FAKE_USERS(id);
    const bool has_fake_user = id->flag & ID_FLAG_FAKEUSER;
    const bool is_linked = ID_IS_LINKED(id);
    const bool is_object = GS(id->name) == ID_OB;
    char overlay[5];
    BLI_str_format_integer_unit(overlay, id->us);

    if (is_object) {
      bt = uiDefBut(block,
                    ButType::But,
                    0,
                    overlay,
                    int(region->v2d.cur.xmax - OL_TOG_USER_BUTS_USERS),
                    te->ys,
                    UI_UNIT_X,
                    UI_UNIT_Y,
                    nullptr,
                    0.0,
                    0.0,
                    TIP_("Number of users"));
    }
    else {

      if (has_fake_user) {
        tip = is_linked ? TIP_("Item is protected from deletion") :
                          TIP_("Click to remove protection from deletion");
      }
      else {
        if (real_users) {
          tip = is_linked ? TIP_("Item is not protected from deletion") :
                            TIP_("Click to add protection from deletion");
        }
        else {
          tip = is_linked ?
                    TIP_("Item has no users and will be removed") :
                    TIP_("Item has no users and will be removed.\nClick to protect from deletion");
        }
      }

      bt = uiDefIconButBitS(block,
                            ButType::IconToggle,
                            ID_FLAG_FAKEUSER,
                            1,
                            ICON_FAKE_USER_OFF,
                            int(region->v2d.cur.xmax - OL_TOG_USER_BUTS_USERS),
                            te->ys,
                            UI_UNIT_X,
                            UI_UNIT_Y,
                            &id->flag,
                            0,
                            0,
                            tip);

      if (is_linked) {
        UI_but_flag_enable(bt, UI_BUT_DISABLED);
      }
      else {
        UI_but_func_set(bt, restrictbutton_id_user_toggle, id, nullptr);
        /* Allow _inaccurate_ dragging over multiple toggles. */
        UI_but_flag_enable(bt, UI_BUT_DRAG_LOCK);
      }

      if (!real_users && !has_fake_user) {
        uchar overlay_color[4];
        UI_GetThemeColor4ubv(TH_REDALERT, overlay_color);
        UI_but_icon_indicator_color_set(bt, overlay_color);
      }
      UI_but_icon_indicator_set(bt, overlay);
    }
  });
}

static void outliner_draw_overrides_rna_buts(uiBlock *block,
                                             const ARegion *region,
                                             const SpaceOutliner *space_outliner,
                                             const ListBase *lb,
                                             const int x)
{
  const float pad_x = 2.0f * UI_SCALE_FAC;
  const float pad_y = 0.5f * U.pixelsize;
  const float item_max_width = round_fl_to_int(OL_RNA_COL_SIZEX - 2 * pad_x);
  const float item_height = round_fl_to_int(UI_UNIT_Y - 2.0f * pad_y);

  LISTBASE_FOREACH (const TreeElement *, te, lb) {
    const TreeStoreElem *tselem = TREESTORE(te);
    if (TSELEM_OPEN(tselem, space_outliner)) {
      outliner_draw_overrides_rna_buts(block, region, space_outliner, &te->subtree, x);
    }

    if (!outliner_is_element_in_view(te, &region->v2d)) {
      continue;
    }
    TreeElementOverridesProperty *override_elem = tree_element_cast<TreeElementOverridesProperty>(
        te);
    if (!override_elem) {
      continue;
    }

    if (!override_elem->is_rna_path_valid) {
      uiBut *but = uiDefBut(block,
                            ButType::Label,
                            0,
                            override_elem->rna_path,
                            x + pad_x,
                            te->ys + pad_y,
                            item_max_width,
                            item_height,
                            nullptr,
                            0.0f,
                            0.0f,
                            "");
      UI_but_flag_enable(but, UI_BUT_REDALERT);
      continue;
    }

    if (const TreeElementOverridesPropertyOperation *override_op_elem =
            tree_element_cast<TreeElementOverridesPropertyOperation>(te))
    {
      StringRefNull op_label = override_op_elem->get_override_operation_label();
      if (!op_label.is_empty()) {
        uiDefBut(block,
                 ButType::Label,
                 0,
                 op_label,
                 x + pad_x,
                 te->ys + pad_y,
                 item_max_width,
                 item_height,
                 nullptr,
                 0,
                 0,
                 "");
        continue;
      }
    }

    PointerRNA *ptr = &override_elem->override_rna_ptr;
    PropertyRNA *prop = &override_elem->override_rna_prop;
    const PropertyType prop_type = RNA_property_type(prop);

    uiBut *auto_but = uiDefAutoButR(block,
                                    ptr,
                                    prop,
                                    -1,
                                    (prop_type == PROP_ENUM) ? std::nullopt : std::optional(""),
                                    ICON_NONE,
                                    x + pad_x,
                                    te->ys + pad_y,
                                    item_max_width,
                                    item_height);
    /* Added the button successfully, nothing else to do. Otherwise, cases for multiple buttons
     * need to be handled. */
    if (auto_but) {
      continue;
    }

    if (!auto_but) {
      /* TODO what if the array is longer, and doesn't fit nicely? What about multi-dimension
       * arrays? */
      uiDefAutoButsArrayR(
          block, ptr, prop, ICON_NONE, x + pad_x, te->ys + pad_y, item_max_width, item_height);
    }
  }
}

static bool outliner_but_identity_cmp_context_id_fn(const uiBut *a, const uiBut *b)
{
  const std::optional<int64_t> session_uid_a = UI_but_context_int_get(a, "session_uid");
  const std::optional<int64_t> session_uid_b = UI_but_context_int_get(b, "session_uid");
  if (!session_uid_a || !session_uid_b) {
    return false;
  }
  /* Using session UID to compare is safer than using the pointer. */
  return session_uid_a == session_uid_b;
}

static void outliner_draw_overrides_restrictbuts(Main *bmain,
                                                 uiBlock *block,
                                                 const ARegion *region,
                                                 const SpaceOutliner *space_outliner,
                                                 const ListBase *lb,
                                                 const int x)
{
  LISTBASE_FOREACH (const TreeElement *, te, lb) {
    const TreeStoreElem *tselem = TREESTORE(te);
    if (TSELEM_OPEN(tselem, space_outliner)) {
      outliner_draw_overrides_restrictbuts(bmain, block, region, space_outliner, &te->subtree, x);
    }

    if (!outliner_is_element_in_view(te, &region->v2d)) {
      continue;
    }
    TreeElementID *te_id = tree_element_cast<TreeElementID>(te);
    if (!te_id) {
      continue;
    }

    ID &id = te_id->get_ID();
    if (ID_IS_LINKED(&id)) {
      continue;
    }
    if (!ID_IS_OVERRIDE_LIBRARY(&id)) {
      /* Some items may not be liboverrides, e.g. the root item for all linked libraries (see
       * #TreeDisplayOverrideLibraryHierarchies::build_tree). */
      continue;
    }

    const bool is_system_override = BKE_lib_override_library_is_system_defined(bmain, &id);
    const BIFIconID icon = is_system_override ? ICON_LIBRARY_DATA_OVERRIDE_NONEDITABLE :
                                                ICON_LIBRARY_DATA_OVERRIDE;
    uiBut *but = uiDefIconButO(block,
                               ButType::But,
                               "ED_OT_lib_id_override_editable_toggle",
                               wm::OpCallContext::ExecDefault,
                               icon,
                               x,
                               te->ys,
                               UI_UNIT_X,
                               UI_UNIT_Y,
                               "");
    /* "id" is used by the operator #ED_OT_lib_id_override_editable_toggle. */
    PointerRNA idptr = RNA_id_pointer_create(&id);
    UI_but_context_ptr_set(block, but, "id", &idptr);

    /* "session_uid" is used to compare buttons (in redraws). */
    UI_but_context_int_set(block, but, "session_uid", id.session_uid);
    UI_but_func_identity_compare_set(but, outliner_but_identity_cmp_context_id_fn);

    UI_but_flag_enable(but, UI_BUT_DRAG_LOCK);
  }
}

static void outliner_draw_separator(ARegion *region, const int x)
{
  View2D *v2d = &region->v2d;

  GPU_line_width(1.0f);

  uint pos = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", blender::gpu::VertAttrType::SFLOAT_32_32);
  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
  immUniformThemeColorShadeAlpha(TH_BACK, -15, -200);

  immBegin(GPU_PRIM_LINES, 2);

  immVertex2f(pos, x, v2d->cur.ymax);
  immVertex2f(pos, x, v2d->cur.ymin);

  immEnd();

  immUnbindProgram();
}

static void outliner_draw_rnabuts(uiBlock *block,
                                  ARegion *region,
                                  SpaceOutliner *space_outliner,
                                  int sizex)
{
  PointerRNA ptr;
  PropertyRNA *prop;

  tree_iterator::all_open(*space_outliner, [&](TreeElement *te) {
    TreeStoreElem *tselem = TREESTORE(te);

    if (!outliner_is_element_in_view(te, &region->v2d)) {
      return;
    }

    if (TreeElementRNAProperty *te_rna_prop = tree_element_cast<TreeElementRNAProperty>(te)) {
      ptr = te_rna_prop->get_pointer_rna();
      prop = te_rna_prop->get_property_rna();

      if (!TSELEM_OPEN(tselem, space_outliner)) {
        if (RNA_property_type(prop) == PROP_POINTER) {
          uiBut *but = uiDefAutoButR(block,
                                     &ptr,
                                     prop,
                                     -1,
                                     "",
                                     ICON_NONE,
                                     sizex,
                                     te->ys,
                                     OL_RNA_COL_SIZEX,
                                     UI_UNIT_Y - 1);
          UI_but_flag_enable(but, UI_BUT_DISABLED);
        }
        else if (RNA_property_type(prop) == PROP_ENUM) {
          uiDefAutoButR(block,
                        &ptr,
                        prop,
                        -1,
                        std::nullopt,
                        ICON_NONE,
                        sizex,
                        te->ys,
                        OL_RNA_COL_SIZEX,
                        UI_UNIT_Y - 1);
        }
        else {
          uiDefAutoButR(block,
                        &ptr,
                        prop,
                        -1,
                        "",
                        ICON_NONE,
                        sizex,
                        te->ys,
                        OL_RNA_COL_SIZEX,
                        UI_UNIT_Y - 1);
        }
      }
    }
    else if (TreeElementRNAArrayElement *te_rna_array_elem =
                 tree_element_cast<TreeElementRNAArrayElement>(te))
    {
      ptr = te_rna_array_elem->get_pointer_rna();
      prop = te_rna_array_elem->get_property_rna();

      uiDefAutoButR(block,
                    &ptr,
                    prop,
                    te->index,
                    "",
                    ICON_NONE,
                    sizex,
                    te->ys,
                    OL_RNA_COL_SIZEX,
                    UI_UNIT_Y - 1);
    }
  });
}

static void outliner_buttons(const bContext *C,
                             uiBlock *block,
                             ARegion *region,
                             const float restrict_column_width,
                             TreeElement *te)
{
  uiBut *bt;
  TreeStoreElem *tselem;
  int spx, dx, len;

  tselem = TREESTORE(te);

  BLI_assert(tselem->flag & TSE_TEXTBUT);
  /* If we add support to rename Sequence, need change this. */

  if (tselem->type == TSE_EBONE) {
    len = sizeof(EditBone::name);
  }
  else if (tselem->type == TSE_MODIFIER) {
    len = sizeof(ModifierData::name);
  }
  else if (tselem->id && GS(tselem->id->name) == ID_LI) {
    len = sizeof(Library::filepath);
  }
  else {
    len = MAX_ID_NAME - 2;
  }

  spx = te->xs + 1.8f * UI_UNIT_X;
  dx = region->v2d.cur.xmax - (spx + restrict_column_width + 0.2f * UI_UNIT_X);

  bt = uiDefBut(block,
                ButType::Text,
                OL_NAMEBUTTON,
                "",
                spx,
                te->ys,
                dx,
                UI_UNIT_Y - 1,
                (void *)te->name,
                1.0,
                float(len),
                "");
  /* Handle undo through the #template_id_cb set below. Default undo handling from the button
   * code (see #ui_apply_but_undo) would not work here, as the new name is not yet applied to the
   * ID. */
  UI_but_flag_disable(bt, UI_BUT_UNDO);
  UI_but_func_rename_set(bt, namebutton_fn, tselem);

  /* Returns false if button got removed. */
  if (false == UI_but_active_only(C, region, block, bt)) {
    tselem->flag &= ~TSE_TEXTBUT;

    /* Bad! (notifier within draw) without this, we don't get a refresh. */
    WM_event_add_notifier(C, NC_SPACE | ND_SPACE_OUTLINER, nullptr);
  }
}

static void outliner_mode_toggle_fn(bContext *C, void *tselem_poin, void * /*arg2*/)
{
  SpaceOutliner *space_outliner = CTX_wm_space_outliner(C);
  TreeStoreElem *tselem = (TreeStoreElem *)tselem_poin;
  TreeViewContext tvc;
  outliner_viewcontext_init(C, &tvc);

  TreeElement *te = outliner_find_tree_element(&space_outliner->tree, tselem);
  if (!te) {
    return;
  }

  /* Check that the item is actually an object. */
  BLI_assert(tselem->id != nullptr && GS(tselem->id->name) == ID_OB);

  Object *ob = (Object *)tselem->id;
  const bool object_data_shared = (ob->data == tvc.obact->data);

  wmWindow *win = CTX_wm_window(C);
  const bool do_extend = (win->eventstate->modifier & KM_CTRL) && !object_data_shared;
  outliner_item_mode_toggle(C, tvc, te, do_extend);
}

/* Draw icons for adding and removing objects from the current interaction mode. */
static void outliner_draw_mode_column_toggle(uiBlock *block,
                                             const TreeViewContext &tvc,
                                             TreeElement *te,
                                             const bool lock_object_modes)
{
  TreeStoreElem *tselem = TREESTORE(te);
  if ((tselem->type != TSE_SOME_ID) || (te->idcode != ID_OB)) {
    return;
  }

  Object *ob = (Object *)tselem->id;
  Object *ob_active = tvc.obact;
  const int x_pad = 3 * UI_SCALE_FAC;

  /* Not all objects support particle systems. */
  if (ob_active->mode == OB_MODE_PARTICLE_EDIT && !psys_get_current(ob)) {
    return;
  }

  /* Only for objects with the same type. */
  if (ob->type != ob_active->type) {
    return;
  }

  if (ob->mode == OB_MODE_OBJECT && BKE_object_is_in_editmode(ob)) {
    /* Another object has our (shared) data in edit mode, so nothing we can change. */
    uiBut *but = uiDefIconBut(block,
                              ButType::But,
                              0,
                              UI_icon_from_object_mode(ob_active->mode),
                              x_pad,
                              te->ys,
                              UI_UNIT_X,
                              UI_UNIT_Y,
                              nullptr,
                              0.0,
                              0.0,
                              TIP_("Another object has this shared data in edit mode"));
    UI_but_flag_enable(but, UI_BUT_DISABLED);
    return;
  }

  bool draw_active_icon = ob->mode == ob_active->mode;

  /* When not locking object modes, objects can remain in non-object modes. For modes that do not
   * allow multi-object editing, these other objects should still show be viewed as not in the
   * mode. Otherwise multiple objects show the same mode icon in the outliner even though only
   * one object is actually editable in the mode. */
  if (!lock_object_modes && ob != ob_active && !(tvc.ob_edit || tvc.ob_pose)) {
    draw_active_icon = false;
  }

  const bool object_data_shared = (ob->data == ob_active->data);
  draw_active_icon = draw_active_icon || object_data_shared;

  int icon;
  StringRef tip;
  if (draw_active_icon) {
    icon = UI_icon_from_object_mode(ob_active->mode);
    tip = object_data_shared ? TIP_("Change the object in the current mode") :
                               TIP_("Remove from the current mode");
  }
  else {
    icon = ICON_DOT;
    tip = TIP_(
        "Change the object in the current mode\n"
        " \u2022 Ctrl to add to the current mode");
  }
  UI_block_emboss_set(block, ui::EmbossType::NoneOrStatus);
  uiBut *but = uiDefIconBut(block,
                            ButType::IconToggle,
                            0,
                            icon,
                            x_pad,
                            te->ys,
                            UI_UNIT_X,
                            UI_UNIT_Y,
                            nullptr,
                            0.0,
                            0.0,
                            tip);
  UI_but_func_set(but, outliner_mode_toggle_fn, tselem, nullptr);
  UI_but_flag_enable(but, UI_BUT_DRAG_LOCK);
  /* Mode toggling handles its own undo state because undo steps need to be grouped. */
  UI_but_flag_disable(but, UI_BUT_UNDO);

  if (!ID_IS_EDITABLE(&ob->id) ||
      (ID_IS_OVERRIDE_LIBRARY_REAL(ob) &&
       (ob->id.override_library->flag & LIBOVERRIDE_FLAG_SYSTEM_DEFINED) != 0))
  {
    UI_but_disable(but, "Cannot edit library or non-editable override data");
  }
}

static void outliner_draw_mode_column(uiBlock *block,
                                      TreeViewContext &tvc,
                                      SpaceOutliner *space_outliner)
{
  const bool lock_object_modes = tvc.scene->toolsettings->object_flag & SCE_OBJECT_MODE_LOCK;

  tree_iterator::all_open(*space_outliner, [&](TreeElement *te) {
    if (tvc.obact && tvc.obact->mode != OB_MODE_OBJECT) {
      outliner_draw_mode_column_toggle(block, tvc, te, lock_object_modes);
    }
  });
}

static StringRefNull outliner_draw_get_warning_tree_element_subtree(const TreeElement *parent_te)
{
  LISTBASE_FOREACH (const TreeElement *, sub_te, &parent_te->subtree) {
    const AbstractTreeElement *abstract_te = tree_element_cast<AbstractTreeElement>(sub_te);
    StringRefNull warning_msg = abstract_te ? abstract_te->get_warning() : "";

    if (!warning_msg.is_empty()) {
      return warning_msg;
    }

    warning_msg = outliner_draw_get_warning_tree_element_subtree(sub_te);
    if (!warning_msg.is_empty()) {
      return warning_msg;
    }
  }

  return "";
}

static StringRefNull outliner_draw_get_warning_tree_element(const SpaceOutliner &space_outliner,
                                                            const TreeElement *te)
{
  const AbstractTreeElement *abstract_te = tree_element_cast<AbstractTreeElement>(te);
  const StringRefNull warning_msg = abstract_te ? abstract_te->get_warning() : "";

  if (!warning_msg.is_empty()) {
    return warning_msg;
  }

  /* If given element has no warning, recursively try to display the first sub-element's warning.
   */
  if (!TSELEM_OPEN(te->store_elem, &space_outliner)) {
    return outliner_draw_get_warning_tree_element_subtree(te);
  }

  return "";
}

static void outliner_draw_warning_tree_element(uiBlock *block,
                                               const SpaceOutliner *space_outliner,
                                               const StringRef warning_msg,
                                               const bool use_mode_column,
                                               const int te_ys)
{
  /* Move the warnings a unit left in view layer mode. */
  const short mode_column_offset = (use_mode_column && (space_outliner->outlinevis == SO_SCENES)) ?
                                       UI_UNIT_X :
                                       0;

  UI_block_emboss_set(block, ui::EmbossType::NoneOrStatus);
  uiBut *but = uiDefIconBut(block,
                            ButType::IconToggle,
                            0,
                            ICON_ERROR,
                            mode_column_offset,
                            te_ys,
                            UI_UNIT_X,
                            UI_UNIT_Y,
                            nullptr,
                            0.0,
                            0.0,
                            warning_msg);
  /* No need for undo here, this is a pure info widget. */
  UI_but_flag_disable(but, UI_BUT_UNDO);
}

static void outliner_draw_warning_column(uiBlock *block,
                                         const SpaceOutliner *space_outliner,
                                         const bool use_mode_column)
{
  tree_iterator::all_open(*space_outliner, [&](const TreeElement *te) {
    /* Get warning for this element, or if there is none and the element is collapsed, the first
     * warning in the collapsed sub-tree. */
    StringRefNull warning_msg = outliner_draw_get_warning_tree_element(*space_outliner, te);

    if (!warning_msg.is_empty()) {
      outliner_draw_warning_tree_element(
          block, space_outliner, warning_msg, use_mode_column, te->ys);
    }
  });
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Normal Drawing
 * \{ */

static BIFIconID tree_element_get_icon_from_id(const ID *id)
{
  if (GS(id->name) == ID_OB) {
    return UI_icon_from_object_type((Object *)id);
  }

  /* TODO(sergey): Casting to short here just to handle ID_NLA which is
   * NOT inside of IDType enum.
   */
  switch (short(GS(id->name))) {
    case ID_SCE:
      return ICON_SCENE_DATA;
    case ID_ME:
      return ICON_OUTLINER_DATA_MESH;
    case ID_CU_LEGACY: {
      const Curve *cu = (Curve *)id;
      switch (cu->ob_type) {
        case OB_FONT:
          return ICON_OUTLINER_DATA_FONT;
        case OB_SURF:
          return ICON_OUTLINER_DATA_SURFACE;
        default:
          return ICON_OUTLINER_DATA_CURVE;
      }
      break;
    }
    case ID_MB:
      return ICON_OUTLINER_DATA_META;
    case ID_LT:
      return ICON_OUTLINER_DATA_LATTICE;
    case ID_LA: {
      const Light *la = (Light *)id;
      switch (la->type) {
        case LA_LOCAL:
          return ICON_LIGHT_POINT;
        case LA_SUN:
          return ICON_LIGHT_SUN;
        case LA_SPOT:
          return ICON_LIGHT_SPOT;
        case LA_AREA:
          return ICON_LIGHT_AREA;
        default:
          return ICON_OUTLINER_DATA_LIGHT;
      }
    }
    case ID_MA:
      return ICON_MATERIAL_DATA;
    case ID_TE:
      return ICON_TEXTURE_DATA;
    case ID_IM:
      return ICON_IMAGE_DATA;
    case ID_SPK:
    case ID_SO:
      return ICON_OUTLINER_DATA_SPEAKER;
    case ID_AR:
      return ICON_OUTLINER_DATA_ARMATURE;
    case ID_CA:
      return ICON_OUTLINER_DATA_CAMERA;
    case ID_KE:
      return ICON_SHAPEKEY_DATA;
    case ID_WO:
      return ICON_WORLD_DATA;
    case ID_AC:
      return ICON_ACTION;
    case ID_NLA:
      return ICON_NLA;
    case ID_TXT: {
      const Text *text = (Text *)id;
      if (text->filepath == nullptr || (text->flags & TXT_ISMEM)) {
        return ICON_FILE_TEXT;
      }
      /* Helps distinguish text-based formats like the file-browser does. */
      return ED_file_extension_icon(text->filepath);
    }
    case ID_GR:
      return ICON_OUTLINER_COLLECTION;
    case ID_CV:
      return ICON_OUTLINER_DATA_CURVES;
    case ID_PT:
      return ICON_OUTLINER_DATA_POINTCLOUD;
    case ID_VO:
      return ICON_OUTLINER_DATA_VOLUME;
    case ID_LI:
      if (id->tag & ID_TAG_MISSING) {
        return ICON_LIBRARY_DATA_BROKEN;
      }
      else if (reinterpret_cast<const Library *>(id)->flag & LIBRARY_FLAG_IS_ARCHIVE) {
        return ICON_PACKAGE;
      }
      else if (((Library *)id)->runtime->parent) {
        return ICON_LIBRARY_DATA_INDIRECT;
      }
      else {
        return ICON_LIBRARY_DATA_DIRECT;
      }
    case ID_LS:
      return ICON_LINE_DATA;
    case ID_GP:
    case ID_GD_LEGACY:
      return ICON_OUTLINER_DATA_GREASEPENCIL;
    case ID_LP: {
      const LightProbe *lp = (LightProbe *)id;
      switch (lp->type) {
        case LIGHTPROBE_TYPE_SPHERE:
          return ICON_LIGHTPROBE_SPHERE;
        case LIGHTPROBE_TYPE_PLANE:
          return ICON_LIGHTPROBE_PLANE;
        case LIGHTPROBE_TYPE_VOLUME:
          return ICON_LIGHTPROBE_VOLUME;
        default:
          return ICON_LIGHTPROBE_SPHERE;
      }
    }
    case ID_BR:
      return ICON_BRUSH_DATA;
    case ID_SCR:
    case ID_WS:
      return ICON_WORKSPACE;
    case ID_MSK:
      return ICON_MOD_MASK;
    case ID_NT: {
      const bNodeTree *ntree = (bNodeTree *)id;
      const bke::bNodeTreeType *ntreetype = ntree->typeinfo;
      return ntreetype->ui_icon;
    }
    case ID_MC:
      return ICON_SEQUENCE;
    case ID_PC:
      return ICON_CURVE_BEZCURVE;
    case ID_PA:
      return ICON_PARTICLES;
    case ID_PAL:
      return ICON_COLOR;
    case ID_VF:
      return ICON_FILE_FONT;
    default:
      return ICON_NONE;
  }
}

TreeElementIcon tree_element_get_icon(TreeStoreElem *tselem, TreeElement *te)
{
  TreeElementIcon data = {nullptr};

  if (tselem->type != TSE_SOME_ID) {
    switch (tselem->type) {
      case TSE_ACTION_SLOT:
        data.icon = ICON_ACTION_SLOT;
        break;
      case TSE_ANIM_DATA:
        data.icon = ICON_ANIM_DATA; /* XXX */
        break;
      case TSE_NLA:
        data.icon = ICON_NLA;
        break;
      case TSE_NLA_TRACK:
        data.icon = ICON_NLA; /* XXX */
        break;
      case TSE_NLA_ACTION:
        data.icon = ICON_ACTION;
        break;
      case TSE_DRIVER_BASE:
        data.icon = ICON_DRIVER;
        break;
      case TSE_DEFGROUP_BASE:
        data.icon = ICON_GROUP_VERTEX;
        break;
      case TSE_DEFGROUP:
        data.icon = ICON_GROUP_VERTEX;
        break;
      case TSE_BONE:
      case TSE_EBONE:
        data.icon = ICON_BONE_DATA;
        break;
      case TSE_CONSTRAINT_BASE:
        data.icon = ICON_CONSTRAINT;
        data.drag_id = tselem->id;
        break;
      case TSE_CONSTRAINT: {
        bConstraint *con = static_cast<bConstraint *>(te->directdata);
        data.drag_id = tselem->id;
        switch ((eBConstraint_Types)con->type) {
          case CONSTRAINT_TYPE_CAMERASOLVER:
            data.icon = ICON_CON_CAMERASOLVER;
            break;
          case CONSTRAINT_TYPE_FOLLOWTRACK:
            data.icon = ICON_CON_FOLLOWTRACK;
            break;
          case CONSTRAINT_TYPE_OBJECTSOLVER:
            data.icon = ICON_CON_OBJECTSOLVER;
            break;
          case CONSTRAINT_TYPE_LOCLIKE:
            data.icon = ICON_CON_LOCLIKE;
            break;
          case CONSTRAINT_TYPE_ROTLIKE:
            data.icon = ICON_CON_ROTLIKE;
            break;
          case CONSTRAINT_TYPE_SIZELIKE:
            data.icon = ICON_CON_SIZELIKE;
            break;
          case CONSTRAINT_TYPE_TRANSLIKE:
            data.icon = ICON_CON_TRANSLIKE;
            break;
          case CONSTRAINT_TYPE_DISTLIMIT:
            data.icon = ICON_CON_DISTLIMIT;
            break;
          case CONSTRAINT_TYPE_LOCLIMIT:
            data.icon = ICON_CON_LOCLIMIT;
            break;
          case CONSTRAINT_TYPE_ROTLIMIT:
            data.icon = ICON_CON_ROTLIMIT;
            break;
          case CONSTRAINT_TYPE_SIZELIMIT:
            data.icon = ICON_CON_SIZELIMIT;
            break;
          case CONSTRAINT_TYPE_SAMEVOL:
            data.icon = ICON_CON_SAMEVOL;
            break;
          case CONSTRAINT_TYPE_TRANSFORM:
            data.icon = ICON_CON_TRANSFORM;
            break;
          case CONSTRAINT_TYPE_TRANSFORM_CACHE:
            data.icon = ICON_CON_TRANSFORM_CACHE;
            break;
          case CONSTRAINT_TYPE_CLAMPTO:
            data.icon = ICON_CON_CLAMPTO;
            break;
          case CONSTRAINT_TYPE_DAMPTRACK:
            data.icon = ICON_CON_TRACKTO;
            break;
          case CONSTRAINT_TYPE_KINEMATIC:
            data.icon = ICON_CON_KINEMATIC;
            break;
          case CONSTRAINT_TYPE_LOCKTRACK:
            data.icon = ICON_CON_LOCKTRACK;
            break;
          case CONSTRAINT_TYPE_SPLINEIK:
            data.icon = ICON_CON_SPLINEIK;
            break;
          case CONSTRAINT_TYPE_STRETCHTO:
            data.icon = ICON_CON_STRETCHTO;
            break;
          case CONSTRAINT_TYPE_TRACKTO:
            data.icon = ICON_CON_TRACKTO;
            break;
          case CONSTRAINT_TYPE_ACTION:
            data.icon = ICON_CON_ACTION;
            break;
          case CONSTRAINT_TYPE_ARMATURE:
            data.icon = ICON_CON_ARMATURE;
            break;
          case CONSTRAINT_TYPE_CHILDOF:
            data.icon = ICON_CON_CHILDOF;
            break;
          case CONSTRAINT_TYPE_MINMAX:
            data.icon = ICON_CON_FLOOR;
            break;
          case CONSTRAINT_TYPE_FOLLOWPATH:
            data.icon = ICON_CON_FOLLOWPATH;
            break;
          case CONSTRAINT_TYPE_PIVOT:
            data.icon = ICON_CON_PIVOT;
            break;
          case CONSTRAINT_TYPE_SHRINKWRAP:
            data.icon = ICON_CON_SHRINKWRAP;
            break;
          case CONSTRAINT_TYPE_GEOMETRY_ATTRIBUTE:
            data.icon = ICON_CON_GEOMETRYATTRIBUTE;
            break;

          default:
            data.icon = ICON_DOT;
            break;
        }
        break;
      }
      case TSE_MODIFIER_BASE:
        data.icon = ICON_MODIFIER_DATA;
        data.drag_id = tselem->id;
        break;
      case TSE_LIBRARY_OVERRIDE_BASE: {
        TreeElementOverridesBase *base_te = tree_element_cast<TreeElementOverridesBase>(te);
        data.icon = tree_element_get_icon_from_id(&base_te->id);
        break;
      }
      case TSE_LIBRARY_OVERRIDE:
        data.icon = ICON_LIBRARY_DATA_OVERRIDE;
        break;
      case TSE_LINKED_OB:
        data.icon = ICON_OBJECT_DATA;
        break;
      case TSE_LINKED_PSYS:
        data.icon = ICON_PARTICLES;
        break;
      case TSE_MODIFIER: {
        Object *ob = (Object *)tselem->id;
        data.drag_id = tselem->id;

        ModifierData *md = static_cast<ModifierData *>(BLI_findlink(&ob->modifiers, tselem->nr));
        if (const ModifierTypeInfo *modifier_type = BKE_modifier_get_info(ModifierType(md->type)))
        {
          data.icon = modifier_type->icon;
        }
        else {
          data.icon = ICON_DOT;
        }
        break;
      }
      case TSE_LINKED_NODE_TREE:
        data.icon = ICON_NODETREE;
        break;
      case TSE_POSE_BASE:
        data.icon = ICON_ARMATURE_DATA;
        break;
      case TSE_POSE_CHANNEL:
        data.icon = ICON_BONE_DATA;
        break;
      case TSE_R_LAYER_BASE:
        data.icon = ICON_RENDERLAYERS;
        break;
      case TSE_SCENE_OBJECTS_BASE:
        data.icon = ICON_OUTLINER_OB_GROUP_INSTANCE;
        break;
      case TSE_R_LAYER:
        data.icon = ICON_RENDER_RESULT;
        break;
      case TSE_BONE_COLLECTION_BASE:
      case TSE_BONE_COLLECTION:
        data.icon = ICON_GROUP_BONE;
        break;
      case TSE_STRIP: {
        const TreeElementStrip *te_strip = tree_element_cast<TreeElementStrip>(te);
        switch (te_strip->get_strip_type()) {
          case STRIP_TYPE_SCENE:
            data.icon = ICON_SCENE_DATA;
            break;
          case STRIP_TYPE_MOVIECLIP:
            data.icon = ICON_TRACKER;
            break;
          case STRIP_TYPE_MASK:
            data.icon = ICON_MOD_MASK;
            break;
          case STRIP_TYPE_MOVIE:
            data.icon = ICON_FILE_MOVIE;
            break;
          case STRIP_TYPE_SOUND_RAM:
            data.icon = ICON_SOUND;
            break;
          case STRIP_TYPE_IMAGE:
            data.icon = ICON_FILE_IMAGE;
            break;
          case STRIP_TYPE_COLOR:
          case STRIP_TYPE_ADJUSTMENT:
            data.icon = ICON_COLOR;
            break;
          case STRIP_TYPE_TEXT:
            data.icon = ICON_FONT_DATA;
            break;
          case STRIP_TYPE_ADD:
          case STRIP_TYPE_SUB:
          case STRIP_TYPE_MUL:
          case STRIP_TYPE_ALPHAOVER:
          case STRIP_TYPE_ALPHAUNDER:
          case STRIP_TYPE_COLORMIX:
          case STRIP_TYPE_MULTICAM:
          case STRIP_TYPE_SPEED:
          case STRIP_TYPE_GLOW:
          case STRIP_TYPE_GAUSSIAN_BLUR:
            data.icon = ICON_SHADERFX;
            break;
          case STRIP_TYPE_CROSS:
          case STRIP_TYPE_GAMCROSS:
          case STRIP_TYPE_WIPE:
            data.icon = ICON_ARROW_LEFTRIGHT;
            break;
          case STRIP_TYPE_META:
            data.icon = ICON_SEQ_STRIP_META;
            break;
          default:
            data.icon = ICON_DOT;
            break;
        }
        break;
      }
      case TSE_STRIP_DATA:
        data.icon = ICON_LIBRARY_DATA_DIRECT;
        break;
      case TSE_STRIP_DUP:
        data.icon = ICON_SEQ_STRIP_DUPLICATE;
        break;
      case TSE_RNA_STRUCT: {
        const TreeElementRNAStruct *te_rna_struct = tree_element_cast<TreeElementRNAStruct>(te);
        const PointerRNA &ptr = te_rna_struct->get_pointer_rna();

        if (RNA_struct_is_ID(ptr.type)) {
          ID *id = static_cast<ID *>(ptr.data);
          data.drag_id = id;
          if (id && GS(id->name) == ID_LI &&
              id_cast<Library *>(id)->flag & LIBRARY_FLAG_IS_ARCHIVE)
          {
            data.icon = ICON_PACKAGE;
          }
          else {
            data.icon = RNA_struct_ui_icon(ptr.type);
          }
        }
        else {
          data.icon = RNA_struct_ui_icon(ptr.type);
        }
        break;
      }
      case TSE_LAYER_COLLECTION:
      case TSE_SCENE_COLLECTION_BASE:
      case TSE_VIEW_COLLECTION_BASE: {
        Collection *collection = outliner_collection_from_tree_element(te);
        if (collection && !(collection->flag & COLLECTION_IS_MASTER)) {
          data.drag_id = tselem->id;
          data.drag_parent = (data.drag_id && te->parent) ? TREESTORE(te->parent)->id : nullptr;
        }

        data.icon = ICON_OUTLINER_COLLECTION;
        break;
      }
      case TSE_GP_LAYER: {
        data.icon = ICON_OUTLINER_DATA_GP_LAYER;
        break;
      }
      case TSE_GREASE_PENCIL_NODE: {
        bke::greasepencil::TreeNode &node =
            tree_element_cast<TreeElementGreasePencilNode>(te)->node();
        if (node.is_layer()) {
          data.icon = ICON_OUTLINER_DATA_GP_LAYER;
        }
        else if (node.is_group()) {
          const bke::greasepencil::LayerGroup &group = node.as_group();

          data.icon = ICON_GREASEPENCIL_LAYER_GROUP;
          if (group.color_tag != LAYERGROUP_COLOR_NONE) {
            data.icon = ICON_LAYERGROUP_COLOR_01 + group.color_tag;
          }
        }
        break;
      }
      case TSE_GPENCIL_EFFECT_BASE:
      case TSE_GPENCIL_EFFECT:
        data.drag_id = tselem->id;
        data.icon = ICON_SHADERFX;
        break;
      default:
        data.icon = ICON_DOT;
        break;
    }
  }
  else if (tselem->id) {
    data.drag_id = tselem->id;
    data.drag_parent = (data.drag_id && te->parent) ? TREESTORE(te->parent)->id : nullptr;
    data.icon = tree_element_get_icon_from_id(tselem->id);
  }

  if (!te->abstract_element) {
    /* Pass */
  }
  else if (auto icon = te->abstract_element->get_icon()) {
    data.icon = *icon;
  }

  return data;
}

/**
 * \return true if the element has an icon that was drawn, false if it doesn't have an icon.
 */
static bool tselem_draw_icon(uiBlock *block,
                             int xmax,
                             float x,
                             float y,
                             TreeStoreElem *tselem,
                             TreeElement *te,
                             float alpha,
                             const bool is_clickable,
                             const int num_elements)
{
  TreeElementIcon data = tree_element_get_icon(tselem, te);
  if (data.icon == 0) {
    return false;
  }

  const bool is_collection = outliner_is_collection_tree_element(te);
  IconTextOverlay text_overlay;
  UI_icon_text_overlay_init_from_count(&text_overlay, num_elements);

  /* Collection colors and icons covered by restrict buttons. */
  if (!is_clickable || x >= xmax || is_collection) {
    /* Placement of icons, copied from `interface_widgets.cc`. */
    float aspect = (0.8f * UI_UNIT_Y) / ICON_DEFAULT_HEIGHT;
    x += 2.0f * aspect;
    y += 2.0f * aspect;
    bTheme *btheme = UI_GetTheme();

    if (is_collection) {
      Collection *collection = outliner_collection_from_tree_element(te);
      if (collection->color_tag != COLLECTION_COLOR_NONE) {
        UI_icon_draw_ex(x,
                        y,
                        ICON_COLLECTION_COLOR_01 + collection->color_tag,
                        UI_INV_SCALE_FAC,
                        alpha,
                        0.0f,
                        btheme->collection_color[collection->color_tag].color,
                        btheme->tui.icon_border_intensity > 0.0f,
                        &text_overlay);
        return true;
      }
    }

    /* Reduce alpha to match icon buttons */
    alpha *= 0.8f;

    /* Restrict column clip. it has been coded by simply overdrawing, doesn't work for buttons. */
    uchar color[4];
    if (UI_icon_get_theme_color(data.icon, color)) {
      UI_icon_draw_ex(x,
                      y,
                      data.icon,
                      UI_INV_SCALE_FAC,
                      alpha,
                      0.0f,
                      color,
                      btheme->tui.icon_border_intensity > 0.0f,
                      &text_overlay);
    }
    else {
      UI_icon_draw_ex(
          x, y, data.icon, UI_INV_SCALE_FAC, alpha, 0.0f, nullptr, false, &text_overlay);
    }
  }
  else {
    uiBut *but = uiDefIconBut(
        block,
        ButType::Label,
        0,
        data.icon,
        x,
        y,
        UI_UNIT_X,
        UI_UNIT_Y,
        nullptr,
        0.0,
        0.0,
        (data.drag_id && ID_IS_LINKED(data.drag_id)) ? data.drag_id->lib->filepath : "");
    UI_but_label_alpha_factor_set(but, alpha);
  }

  return true;
}

static void outliner_icon_background_colors(float icon_color[4], float icon_border[4])
{
  float text[4];
  UI_GetThemeColor4fv(TH_TEXT, text);

  copy_v3_v3(icon_color, text);
  icon_color[3] = 0.4f;
  copy_v3_v3(icon_border, text);
  icon_border[3] = 0.2f;
}

/* Draw a rounded rectangle behind icons of active elements. */
static void outliner_draw_active_indicator(const float minx,
                                           const float miny,
                                           const float maxx,
                                           const float maxy,
                                           const float icon_color[4],
                                           const float icon_border[4])
{
  const float ufac = UI_UNIT_X / 20.0f;
  const float radius = UI_UNIT_Y / 4.0f;
  rctf rect{};
  BLI_rctf_init(&rect, minx, maxx, miny + ufac, maxy - ufac);

  UI_draw_roundbox_corner_set(UI_CNR_ALL);
  UI_draw_roundbox_aa(&rect, true, radius, icon_color);
  UI_draw_roundbox_aa(&rect, false, radius, icon_border);
  GPU_blend(GPU_BLEND_ALPHA); /* Round-box disables. */
}

static void outliner_draw_iconrow_doit(uiBlock *block,
                                       TreeElement *te,
                                       int xmax,
                                       int *offsx,
                                       int ys,
                                       float alpha_fac,
                                       const eOLDrawState active,
                                       const int num_elements)
{
  TreeStoreElem *tselem = TREESTORE(te);

  if (active != OL_DRAWSEL_NONE) {
    float icon_color[4], icon_border[4];
    outliner_icon_background_colors(icon_color, icon_border);
    if (active == OL_DRAWSEL_ACTIVE) {
      UI_GetThemeColor4fv(TH_EDITED_OBJECT, icon_color);
      icon_border[3] = 0.3f;
    }

    outliner_draw_active_indicator(float(*offsx),
                                   float(ys),
                                   float(*offsx) + UI_UNIT_X,
                                   float(ys) + UI_UNIT_Y,
                                   icon_color,
                                   icon_border);
  }

  if (tselem->flag & TSE_HIGHLIGHTED_ICON) {
    alpha_fac += 0.5;
  }
  tselem_draw_icon(
      block, xmax, float(*offsx), float(ys), tselem, te, alpha_fac, false, num_elements);
  te->xs = *offsx;
  te->ys = ys;
  te->xend = short(*offsx) + UI_UNIT_X;

  if (num_elements > 1) {
    te->flag |= TE_ICONROW_MERGED;
  }
  else {
    te->flag |= TE_ICONROW;
  }

  (*offsx) += UI_UNIT_X;
}

int tree_element_id_type_to_index(TreeElement *te)
{
  TreeStoreElem *tselem = TREESTORE(te);

  int id_index = 0;
  if (tselem->type == TSE_SOME_ID) {
    id_index = BKE_idtype_idcode_to_index(te->idcode);
  }
  else if (tselem->type == TSE_GREASE_PENCIL_NODE) {
    /* Use the index of the grease pencil ID for the grease pencil tree nodes (which are not IDs).
     * All the Grease Pencil layer tree stats are stored in this index in #MergedIconRow. */
    id_index = INDEX_ID_GP;
  }
  else {
    id_index = INDEX_ID_GR;
  }

  if (id_index < INDEX_ID_OB) {
    return id_index;
  }
  if (id_index == INDEX_ID_OB) {
    const Object *ob = (Object *)tselem->id;
    return INDEX_ID_OB + ob->type;
  }
  return id_index + OB_TYPE_MAX;
}

struct MergedIconRow {
  eOLDrawState active[INDEX_ID_MAX + OB_TYPE_MAX];
  int num_elements[INDEX_ID_MAX + OB_TYPE_MAX];
  TreeElement *tree_element[INDEX_ID_MAX + OB_TYPE_MAX];
};

static void outliner_draw_iconrow(uiBlock *block,
                                  const uiFontStyle *fstyle,
                                  const TreeViewContext &tvc,
                                  SpaceOutliner *space_outliner,
                                  ListBase *lb,
                                  int level,
                                  int xmax,
                                  int *offsx,
                                  int ys,
                                  float alpha_fac,
                                  bool in_bone_hierarchy,
                                  const bool is_grease_pencil_node_hierarchy,
                                  MergedIconRow *merged)
{
  eOLDrawState active = OL_DRAWSEL_NONE;

  LISTBASE_FOREACH (TreeElement *, te, lb) {
    TreeStoreElem *tselem = TREESTORE(te);
    te->flag &= ~(TE_ICONROW | TE_ICONROW_MERGED);

    /* object hierarchy always, further constrained on level */
    /* Bones are also hierarchies and get a merged count, but we only start recursing into them if
     * an they are at the root level of a collapsed subtree (e.g. not "hidden" in a collapsed
     * collection). */
    const bool is_bone = ELEM(tselem->type, TSE_BONE, TSE_EBONE, TSE_POSE_CHANNEL);
    /* The Grease Pencil layer tree is a hierarchy where we merge and count the total number of
     * layers in a node. We merge the counts for all the layers and skip counting nodes that are
     * layer groups (for less visual clutter in the outliner). */
    const bool is_grease_pencil_node = (tselem->type == TSE_GREASE_PENCIL_NODE);
    if ((level < 1) || ((tselem->type == TSE_SOME_ID) && (te->idcode == ID_OB)) ||
        (is_grease_pencil_node_hierarchy && is_grease_pencil_node) ||
        (in_bone_hierarchy && is_bone))
    {
      /* active blocks get white circle */
      if (tselem->type == TSE_SOME_ID) {
        if (te->idcode == ID_OB) {
          active = (tvc.obact == (Object *)tselem->id) ? OL_DRAWSEL_NORMAL : OL_DRAWSEL_NONE;
        }
        else if (is_object_data_in_editmode(tselem->id, tvc.obact)) {
          active = OL_DRAWSEL_ACTIVE;
        }
        else {
          active = tree_element_active_state_get(tvc, te, tselem);
        }
      }
      else {
        active = tree_element_type_active_state_get(tvc, te, tselem);
      }

      if (!ELEM(tselem->type,
                TSE_ID_BASE,
                TSE_SOME_ID,
                TSE_LAYER_COLLECTION,
                TSE_R_LAYER,
                TSE_GP_LAYER,
                TSE_GREASE_PENCIL_NODE,
                TSE_LIBRARY_OVERRIDE_BASE,
                TSE_LIBRARY_OVERRIDE,
                TSE_LIBRARY_OVERRIDE_OPERATION,
                TSE_BONE,
                TSE_EBONE,
                TSE_POSE_CHANNEL,
                TSE_BONE_COLLECTION,
                TSE_DEFGROUP,
                TSE_ACTION_SLOT,
                TSE_NLA_TRACK))
      {
        outliner_draw_iconrow_doit(block, te, xmax, offsx, ys, alpha_fac, active, 1);
      }
      else if (tselem->type == TSE_GREASE_PENCIL_NODE &&
               tree_element_cast<TreeElementGreasePencilNode>(te)->node().is_group())
      {
        /* Grease Pencil layer groups are tree nodes, but they shouldn't be counted. We only want
         * to keep track of the nodes that are layers and show the total number of layers in a
         * node. Adding the count of groups would add a lot of clutter. */
      }
      else {
        const int index = tree_element_id_type_to_index(te);
        merged->num_elements[index]++;
        if ((merged->tree_element[index] == nullptr) || (active > merged->active[index])) {
          merged->tree_element[index] = te;
        }
        merged->active[index] = std::max(active, merged->active[index]);
      }
    }

    /* TSE_R_LAYER tree element always has same amount of branches, so don't draw. */
    /* Also only recurse into bone hierarchies if a direct child of the collapsed element to
     * merge into. */
    const bool is_root_level_bone = is_bone && (level == 0);
    in_bone_hierarchy |= is_root_level_bone;
    /* Recurse into the grease pencil layer tree if we already are in the hierarchy or if we're at
     * the root level and find a grease pencil node. */
    const bool in_grease_pencil_node_hierarchy = is_grease_pencil_node_hierarchy ||
                                                 (is_grease_pencil_node && level == 0);
    if (!ELEM(tselem->type, TSE_R_LAYER, TSE_BONE, TSE_EBONE, TSE_POSE_CHANNEL) ||
        in_bone_hierarchy || in_grease_pencil_node_hierarchy)
    {
      outliner_draw_iconrow(block,
                            fstyle,
                            tvc,
                            space_outliner,
                            &te->subtree,
                            level + 1,
                            xmax,
                            offsx,
                            ys,
                            alpha_fac,
                            in_bone_hierarchy,
                            in_grease_pencil_node_hierarchy,
                            merged);
    }
  }

  if (level == 0) {
    for (int i = 0; i < INDEX_ID_MAX; i++) {
      const int num_subtypes = (i == INDEX_ID_OB) ? OB_TYPE_MAX : 1;
      /* See tree_element_id_type_to_index for the index logic. */
      int index_base = i;
      if (i > INDEX_ID_OB) {
        index_base += OB_TYPE_MAX;
      }
      for (int j = 0; j < num_subtypes; j++) {
        const int index = index_base + j;
        if (merged->num_elements[index] != 0) {
          outliner_draw_iconrow_doit(block,
                                     merged->tree_element[index],
                                     xmax,
                                     offsx,
                                     ys,
                                     alpha_fac,
                                     merged->active[index],
                                     merged->num_elements[index]);
        }
      }
    }
  }
}

/* closed tree element */
static void outliner_set_subtree_coords(const TreeElement *te)
{
  tree_iterator::all(te->subtree, [&](TreeElement *te) {
    /* closed items may be displayed in row of parent, don't change their coordinate! */
    if ((te->flag & TE_ICONROW) == 0 && (te->flag & TE_ICONROW_MERGED) == 0) {
      te->xs = 0;
      te->ys = 0;
      te->xend = 0;
    }
  });
}

static bool element_should_draw_faded(const TreeViewContext &tvc,
                                      const TreeElement *te,
                                      const TreeStoreElem *tselem)
{
  if (tselem->type == TSE_SOME_ID) {
    switch (te->idcode) {
      case ID_OB: {
        const Object *ob = (const Object *)tselem->id;
        /* Lookup in view layer is logically const as it only checks a cache. */
        BKE_view_layer_synced_ensure(tvc.scene, tvc.view_layer);
        const Base *base = (te->directdata) ?
                               (const Base *)te->directdata :
                               BKE_view_layer_base_find((ViewLayer *)tvc.view_layer, (Object *)ob);
        const bool is_visible = (base != nullptr) &&
                                (base->flag & BASE_ENABLED_AND_VISIBLE_IN_DEFAULT_VIEWPORT) &&
                                !(te->flag & TE_CHILD_NOT_IN_COLLECTION);

        return !is_visible;
      }
      default: {
        if (te->parent) {
          return element_should_draw_faded(tvc, te->parent, te->parent->store_elem);
        }
      }
    }
  }
  switch (tselem->type) {
    case TSE_LAYER_COLLECTION: {
      const LayerCollection *layer_collection = (const LayerCollection *)te->directdata;
      const bool is_visible = layer_collection->runtime_flag & LAYER_COLLECTION_VISIBLE_VIEW_LAYER;
      const bool is_excluded = layer_collection->flag & LAYER_COLLECTION_EXCLUDE;
      return !is_visible || is_excluded;
    }
    case TSE_GREASE_PENCIL_NODE: {
      bke::greasepencil::TreeNode &node =
          tree_element_cast<TreeElementGreasePencilNode>(te)->node();
      return !node.is_visible();
    }
    default: {
      if (te->parent) {
        return element_should_draw_faded(tvc, te->parent, te->parent->store_elem);
      }
    }
  }

  if (te->flag & TE_CHILD_NOT_IN_COLLECTION) {
    return true;
  }

  return false;
}

static void outliner_draw_tree_element(uiBlock *block,
                                       const uiFontStyle *fstyle,
                                       const TreeViewContext &tvc,
                                       ARegion *region,
                                       SpaceOutliner *space_outliner,
                                       TreeElement *te,
                                       bool draw_grayed_out,
                                       int startx,
                                       int *starty,
                                       const float restrict_column_width,
                                       TreeElement **te_edit)
{
  TreeStoreElem *tselem = TREESTORE(te);
  float ufac = UI_UNIT_X / 20.0f;
  int offsx = 0;
  eOLDrawState active = OL_DRAWSEL_NONE;
  uchar text_color[4];
  UI_GetThemeColor4ubv(TH_TEXT, text_color);
  float icon_bgcolor[4], icon_border[4];
  outliner_icon_background_colors(icon_bgcolor, icon_border);

  if (*starty + 2 * UI_UNIT_Y >= region->v2d.cur.ymin && *starty <= region->v2d.cur.ymax) {
    const float alpha_fac = element_should_draw_faded(tvc, te, tselem) ? 0.5f : 1.0f;
    int xmax = region->v2d.cur.xmax;

    if ((tselem->flag & TSE_TEXTBUT) && (*te_edit == nullptr)) {
      *te_edit = te;
    }

    /* Icons can be UI buts, we don't want it to overlap with restrict. */
    if (restrict_column_width > 0) {
      xmax -= restrict_column_width + UI_UNIT_X;
    }

    GPU_blend(GPU_BLEND_ALPHA);

    /* Colors for active/selected data. */
    if (tselem->type == TSE_SOME_ID) {
      if (te->idcode == ID_OB) {
        Object *ob = (Object *)tselem->id;
        BKE_view_layer_synced_ensure(tvc.scene, tvc.view_layer);
        Base *base = (te->directdata) ? (Base *)te->directdata :
                                        BKE_view_layer_base_find(tvc.view_layer, ob);
        const bool is_selected = (base != nullptr) && ((base->flag & BASE_SELECTED) != 0);

        if (ob == tvc.obact) {
          active = OL_DRAWSEL_ACTIVE;
        }

        if (is_selected) {
          if (ob == tvc.obact) {
            /* Active selected object. */
            UI_GetThemeColor3ubv(TH_ACTIVE_OBJECT, text_color);
            text_color[3] = 255;
          }
          else {
            /* Other selected objects. */
            UI_GetThemeColor3ubv(TH_SELECTED_OBJECT, text_color);
            text_color[3] = 255;
          }
        }
      }
      else if (is_object_data_in_editmode(tselem->id, tvc.obact)) {
        /* Objects being edited. */
        UI_GetThemeColor4fv(TH_EDITED_OBJECT, icon_bgcolor);
        icon_border[3] = 0.3f;
        active = OL_DRAWSEL_ACTIVE;
      }
      else {
        if (tree_element_active_state_get(tvc, te, tselem)) {
          /* Active items like camera or material. */
          icon_bgcolor[3] = 0.2f;
          active = OL_DRAWSEL_ACTIVE;
          if (te->idcode == ID_SCE) {
            UI_GetThemeColor3ubv(TH_TEXT_HI, text_color);
            text_color[3] = 255;
          }
        }
      }
    }
    else {
      active = tree_element_type_active_state_get(tvc, te, tselem);
      if (active != OL_DRAWSEL_NONE) {
        UI_GetThemeColor3ubv(TH_TEXT_HI, text_color);
        text_color[3] = 255;
      }
    }

    /* Active circle. */
    if (active != OL_DRAWSEL_NONE) {
      outliner_draw_active_indicator(float(startx) + offsx + UI_UNIT_X,
                                     float(*starty),
                                     float(startx) + offsx + 2.0f * UI_UNIT_X,
                                     float(*starty) + UI_UNIT_Y,
                                     icon_bgcolor,
                                     icon_border);

      te->flag |= TE_ACTIVE; /* For lookup in display hierarchies. */
    }

    if (tselem->type == TSE_VIEW_COLLECTION_BASE) {
      /* Scene collection in view layer can't expand/collapse. */
    }
    else if (te->subtree.first || (te->flag & TE_PRETEND_HAS_CHILDREN)) {
      /* Open/close icon, only when sub-levels, except for scene. */
      int icon_x = startx;

      /* Icons a bit higher. */
      if (TSELEM_OPEN(tselem, space_outliner)) {
        UI_icon_draw_alpha(
            float(icon_x) + 2 * ufac, float(*starty) + 1 * ufac, ICON_DOWNARROW_HLT, alpha_fac);
      }
      else {
        UI_icon_draw_alpha(
            float(icon_x) + 2 * ufac, float(*starty) + 1 * ufac, ICON_RIGHTARROW, alpha_fac);
      }
    }
    offsx += UI_UNIT_X;

    /* Data-type icon. */
    if (!ELEM(tselem->type, TSE_RNA_PROPERTY, TSE_RNA_ARRAY_ELEM, TSE_ID_BASE) &&
        tselem_draw_icon(block,
                         xmax,
                         float(startx) + offsx,
                         float(*starty),
                         tselem,
                         te,
                         (tselem->flag & TSE_HIGHLIGHTED_ICON) ? alpha_fac + 0.5f : alpha_fac,
                         true,
                         1))
    {
      offsx += UI_UNIT_X + 4 * ufac;
    }
    else {
      offsx += 2 * ufac;
    }

    const TreeElementRNAStruct *te_rna_struct = tree_element_cast<TreeElementRNAStruct>(te);
    if (ELEM(tselem->type, TSE_SOME_ID, TSE_LAYER_COLLECTION, TSE_LINKED_NODE_TREE) ||
        (te_rna_struct && RNA_struct_is_ID(te_rna_struct->get_pointer_rna().type)))
    {
      const BIFIconID lib_icon = UI_icon_from_library(tselem->id);
      if (lib_icon != ICON_NONE) {
        UI_icon_draw_alpha(
            float(startx) + offsx + 2 * ufac, float(*starty) + 2 * ufac, lib_icon, alpha_fac);
        offsx += UI_UNIT_X + 4 * ufac;
      }

      if (tselem->type == TSE_LAYER_COLLECTION) {
        const Collection *collection = (Collection *)tselem->id;
        if (!BLI_listbase_is_empty(&collection->exporters)) {
          UI_icon_draw_alpha(
              float(startx) + offsx + 2 * ufac, float(*starty) + 2 * ufac, ICON_EXPORT, alpha_fac);
          offsx += UI_UNIT_X + 4 * ufac;
        }
      }
    }
    GPU_blend(GPU_BLEND_NONE);

    /* Name. */
    if ((tselem->flag & TSE_TEXTBUT) == 0) {
      if (ELEM(tselem->type, TSE_RNA_PROPERTY, TSE_RNA_ARRAY_ELEM)) {
        UI_GetThemeColorBlend3ubv(TH_BACK, TH_TEXT, 0.75f, text_color);
        text_color[3] = 255;
      }
      text_color[3] *= alpha_fac;
      UI_fontstyle_draw_simple(fstyle, startx + offsx, *starty + 5 * ufac, te->name, text_color);
    }

    offsx += int(UI_UNIT_X + UI_fontstyle_string_width(fstyle, te->name));

    /* Closed item, we draw the icons, not when it's a scene, or master-server list though. */
    if (!TSELEM_OPEN(tselem, space_outliner)) {
      if (te->subtree.first) {
        if ((tselem->type == TSE_SOME_ID) && (te->idcode == ID_SCE)) {
          /* Pass. */
        }
        /* this tree element always has same amount of branches, so don't draw */
        else if (tselem->type != TSE_R_LAYER) {
          int tempx = startx + offsx;

          GPU_blend(GPU_BLEND_ALPHA);

          MergedIconRow merged{};
          outliner_draw_iconrow(block,
                                fstyle,
                                tvc,
                                space_outliner,
                                &te->subtree,
                                0,
                                xmax,
                                &tempx,
                                *starty,
                                alpha_fac,
                                false,
                                false,
                                &merged);

          GPU_blend(GPU_BLEND_NONE);
        }
      }
    }
  }
  /* Store coord and continue, we need coordinates for elements outside view too. */
  te->xs = startx;
  te->ys = *starty;
  te->xend = startx + offsx;

  if (TSELEM_OPEN(tselem, space_outliner)) {
    *starty -= UI_UNIT_Y;

    LISTBASE_FOREACH (TreeElement *, ten, &te->subtree) {
      /* Check if element needs to be drawn grayed out, but also gray out
       * children of a grayed out parent (pass on draw_grayed_out to children). */
      bool draw_children_grayed_out = draw_grayed_out || (ten->flag & TE_DRAGGING);
      outliner_draw_tree_element(block,
                                 fstyle,
                                 tvc,
                                 region,
                                 space_outliner,
                                 ten,
                                 draw_children_grayed_out,
                                 startx + UI_UNIT_X,
                                 starty,
                                 restrict_column_width,
                                 te_edit);
    }
  }
  else {
    outliner_set_subtree_coords(te);
    *starty -= UI_UNIT_Y;
  }
}

static bool subtree_contains_object(ListBase *lb)
{
  LISTBASE_FOREACH (TreeElement *, te, lb) {
    TreeStoreElem *tselem = TREESTORE(te);
    if ((tselem->type == TSE_SOME_ID) && (te->idcode == ID_OB)) {
      return true;
    }
  }
  return false;
}

static void outliner_draw_hierarchy_line(
    const uint pos, const int x, const int y1, const int y2, const bool draw_dashed)
{
  /* Small vertical padding. */
  const short line_padding = UI_UNIT_Y / 4.0f;

  /* >= is 1.0 for un-dashed lines. */
  immUniform1f("udash_factor", draw_dashed ? 0.5f : 1.0f);

  immBegin(GPU_PRIM_LINES, 2);
  /* Intentionally draw from top to bottom, so collapsing a child item doesn't make the dashes
   * appear to move. */
  immVertex2f(pos, x, y2 + line_padding);
  immVertex2f(pos, x, y1 - line_padding);
  immEnd();
}

static void outliner_draw_hierarchy_lines_recursive(uint pos,
                                                    SpaceOutliner *space_outliner,
                                                    ListBase *lb,
                                                    const TreeViewContext &tvc,
                                                    int startx,
                                                    const uchar col[4],
                                                    bool draw_grayed_out,
                                                    int *starty)
{
  bTheme *btheme = UI_GetTheme();
  int y = *starty;

  /* Draw vertical lines between collections */
  bool draw_hierarchy_line;
  bool is_object_line;
  LISTBASE_FOREACH (TreeElement *, te, lb) {
    TreeStoreElem *tselem = TREESTORE(te);
    draw_hierarchy_line = false;
    is_object_line = false;
    *starty -= UI_UNIT_Y;
    short color_tag = COLLECTION_COLOR_NONE;

    /* Only draw hierarchy lines for expanded collections and objects with children. */
    if (TSELEM_OPEN(tselem, space_outliner) && !BLI_listbase_is_empty(&te->subtree)) {
      if (tselem->type == TSE_LAYER_COLLECTION) {
        draw_hierarchy_line = true;

        Collection *collection = outliner_collection_from_tree_element(te);
        color_tag = collection->color_tag;

        y = *starty;
      }
      else if ((tselem->type == TSE_SOME_ID) && (te->idcode == ID_OB)) {
        if (subtree_contains_object(&te->subtree)) {
          draw_hierarchy_line = true;
          is_object_line = true;
          y = *starty;
        }
      }
      else if (tselem->type == TSE_GREASE_PENCIL_NODE) {
        bke::greasepencil::TreeNode &node =
            tree_element_cast<TreeElementGreasePencilNode>(te)->node();
        if (node.is_group() && node.as_group().num_direct_nodes() > 0) {
          draw_hierarchy_line = true;
          y = *starty;
        }
      }

      outliner_draw_hierarchy_lines_recursive(pos,
                                              space_outliner,
                                              &te->subtree,
                                              tvc,
                                              startx + UI_UNIT_X,
                                              col,
                                              draw_grayed_out,
                                              starty);
    }

    if (draw_hierarchy_line) {
      const short alpha_fac = element_should_draw_faded(tvc, te, tselem) ? 127 : 255;
      uchar line_color[4];
      if (color_tag != COLLECTION_COLOR_NONE) {
        copy_v4_v4_uchar(line_color, btheme->collection_color[color_tag].color);
      }
      else {
        copy_v4_v4_uchar(line_color, col);
      }

      line_color[3] = alpha_fac;
      immUniformColor4ubv(line_color);
      outliner_draw_hierarchy_line(pos, startx, y, *starty, is_object_line);
    }
  }
}

static void outliner_draw_hierarchy_lines(SpaceOutliner *space_outliner,
                                          ListBase *lb,
                                          const TreeViewContext &tvc,
                                          int startx,
                                          int *starty)
{
  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", blender::gpu::VertAttrType::SFLOAT_32_32);
  uchar col[4];

  immBindBuiltinProgram(GPU_SHADER_3D_LINE_DASHED_UNIFORM_COLOR);

  float viewport_size[4];
  GPU_viewport_size_get_f(viewport_size);
  immUniform2f("viewport_size", viewport_size[2] / UI_SCALE_FAC, viewport_size[3] / UI_SCALE_FAC);
  immUniform1i("colors_len", 0); /* "simple"  mode */
  immUniform1f("dash_width", 8.0f);
  UI_GetThemeColorBlend3ubv(TH_BACK, TH_TEXT, 0.4f, col);
  col[3] = 255;

  GPU_line_width(1.0f);
  GPU_blend(GPU_BLEND_ALPHA);
  outliner_draw_hierarchy_lines_recursive(
      pos, space_outliner, lb, tvc, startx, col, false, starty);
  GPU_blend(GPU_BLEND_NONE);

  immUnbindProgram();
}

static void outliner_draw_struct_marks(ARegion *region,
                                       SpaceOutliner *space_outliner,
                                       ListBase *lb,
                                       int *starty)
{
  LISTBASE_FOREACH (TreeElement *, te, lb) {
    TreeStoreElem *tselem = TREESTORE(te);

    /* Selection status. */
    if (TSELEM_OPEN(tselem, space_outliner)) {
      if (tselem->type == TSE_RNA_STRUCT) {
        GPUVertFormat *format = immVertexFormat();
        uint pos = GPU_vertformat_attr_add(
            format, "pos", blender::gpu::VertAttrType::SFLOAT_32_32);
        immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
        immThemeColorShadeAlpha(TH_BACK, -15, -200);
        immRectf(pos, 0, *starty + 1, int(region->v2d.cur.xmax), *starty + UI_UNIT_Y - 1);
        immUnbindProgram();
      }
    }

    *starty -= UI_UNIT_Y;
    if (TSELEM_OPEN(tselem, space_outliner)) {
      outliner_draw_struct_marks(region, space_outliner, &te->subtree, starty);
      if (tselem->type == TSE_RNA_STRUCT) {
        GPUVertFormat *format = immVertexFormat();
        uint pos = GPU_vertformat_attr_add(
            format, "pos", blender::gpu::VertAttrType::SFLOAT_32_32);
        immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
        immThemeColorShadeAlpha(TH_BACK, -15, -200);

        immBegin(GPU_PRIM_LINES, 2);
        immVertex2f(pos, 0, float(*starty) + UI_UNIT_Y);
        immVertex2f(pos, region->v2d.cur.xmax, float(*starty) + UI_UNIT_Y);
        immEnd();

        immUnbindProgram();
      }
    }
  }
}

static void outliner_draw_highlights(const ARegion *region,
                                     const SpaceOutliner *space_outliner,
                                     const float col_selection[4],
                                     const float col_active[4],
                                     const float col_highlight[4],
                                     const float col_searchmatch[4],
                                     int /* start_x */,
                                     int *io_start_y)
{
  const bool is_searching = (SEARCHING_OUTLINER(space_outliner) ||
                             (space_outliner->outlinevis == SO_DATA_API &&
                              space_outliner->search_string[0] != 0));

  tree_iterator::all_open(*space_outliner, [&](const TreeElement *te) {
    const TreeStoreElem *tselem = TREESTORE(te);
    const int start_y = *io_start_y;

    const float ufac = UI_UNIT_X / 20.0f;
    const float radius = UI_UNIT_Y / 8.0f;
    const int padding_x = 3 * UI_SCALE_FAC;
    rctf rect{};
    BLI_rctf_init(&rect,
                  padding_x,
                  int(region->v2d.cur.xmax) - padding_x,
                  start_y + ufac,
                  start_y + UI_UNIT_Y - ufac);
    UI_draw_roundbox_corner_set(UI_CNR_ALL);

    /* Selection status. */
    if ((tselem->flag & TSE_ACTIVE) && (tselem->flag & TSE_SELECTED)) {
      UI_draw_roundbox_4fv(&rect, true, radius, col_active);

      float col_active_outline[4];
      UI_GetThemeColorShade4fv(TH_SELECT_ACTIVE, 40, col_active_outline);
      UI_draw_roundbox_4fv(&rect, false, radius, col_active_outline);
    }
    else if (tselem->flag & TSE_SELECTED) {
      UI_draw_roundbox_4fv(&rect, true, radius, col_selection);
    }

    /* Highlights. */
    if (tselem->flag & (TSE_DRAG_ANY | TSE_HIGHLIGHTED | TSE_SEARCHMATCH)) {
      if (tselem->flag & TSE_DRAG_ANY) {
        /* Drag and drop highlight. */
        float col_outline[4];
        UI_GetThemeColorBlend4f(TH_TEXT, TH_BACK, 0.4f, col_outline);

        if (tselem->flag & TSE_DRAG_BEFORE) {
          rect.ymax += (1.0f * UI_SCALE_FAC) + (1.0f * U.pixelsize);
          rect.ymin = rect.ymax - (2.0f * U.pixelsize);
          UI_draw_roundbox_4fv(&rect, true, 0.0f, col_outline);
        }
        else if (tselem->flag & TSE_DRAG_AFTER) {
          rect.ymin -= (1.0f * UI_SCALE_FAC) + (1.0f * U.pixelsize);
          rect.ymax = rect.ymin + (2.0f * U.pixelsize);
          UI_draw_roundbox_4fv(&rect, true, 0.0f, col_outline);
        }
        else {
          float col_bg[4];
          UI_GetThemeColorShade4fv(TH_BACK, 40, col_bg);
          UI_draw_roundbox_4fv_ex(&rect, col_bg, nullptr, 1.0f, col_outline, U.pixelsize, radius);
        }
      }
      else {
        if (is_searching && (tselem->flag & TSE_SEARCHMATCH)) {
          /* Search match highlights. We don't expand items when searching in the data-blocks,
           * but we still want to highlight any filter matches. */
          UI_draw_roundbox_4fv(&rect, true, radius, col_searchmatch);
        }
        else if (tselem->flag & TSE_HIGHLIGHTED) {
          /* Mouse hover highlight. */
          UI_draw_roundbox_4fv(&rect, true, radius, col_highlight);
        }
      }
    }

    *io_start_y -= UI_UNIT_Y;
  });
}

static void outliner_draw_highlights(ARegion *region,
                                     SpaceOutliner *space_outliner,
                                     int startx,
                                     int *starty)
{
  const float col_highlight[4] = {1.0f, 1.0f, 1.0f, 0.13f};
  float col_selection[4], col_active[4], col_searchmatch[4];

  UI_GetThemeColor3fv(TH_SELECT_HIGHLIGHT, col_selection);
  col_selection[3] = 1.0f; /* No alpha. */
  UI_GetThemeColor3fv(TH_SELECT_ACTIVE, col_active);
  col_active[3] = 1.0f; /* No alpha. */
  UI_GetThemeColor4fv(TH_MATCH, col_searchmatch);
  col_searchmatch[3] = 0.5f;

  GPU_blend(GPU_BLEND_ALPHA);
  outliner_draw_highlights(region,
                           space_outliner,
                           col_selection,
                           col_active,
                           col_highlight,
                           col_searchmatch,
                           startx,
                           starty);
  GPU_blend(GPU_BLEND_NONE);
}

static void outliner_draw_tree(uiBlock *block,
                               const TreeViewContext &tvc,
                               ARegion *region,
                               SpaceOutliner *space_outliner,
                               const float right_column_width,
                               const bool use_mode_column,
                               const bool use_warning_column,
                               TreeElement **te_edit)
{
  const uiFontStyle *fstyle = UI_FSTYLE_WIDGET;

  short columns_offset = use_mode_column ? UI_UNIT_X : 0;

  /* Move the tree a unit left in view layer mode */
  if ((space_outliner->outlinevis == SO_VIEW_LAYER) &&
      !(space_outliner->filter & SO_FILTER_NO_COLLECTION) &&
      (space_outliner->filter & SO_FILTER_NO_VIEW_LAYERS))
  {
    columns_offset -= UI_UNIT_X;
  }

  if (use_warning_column) {
    columns_offset += UI_UNIT_X;
  }

  GPU_blend(GPU_BLEND_ALPHA); /* Only once. */

  if (space_outliner->outlinevis == SO_DATA_API) {
    /* struct marks */
    int starty = int(region->v2d.tot.ymax) - UI_UNIT_Y - OL_Y_OFFSET;
    outliner_draw_struct_marks(region, space_outliner, &space_outliner->tree, &starty);
  }

  /* Draw highlights before hierarchy. */
  int scissor[4] = {0};
  {
    int starty = int(region->v2d.tot.ymax) - UI_UNIT_Y - OL_Y_OFFSET;
    int startx = 0;
    outliner_draw_highlights(region, space_outliner, startx, &starty);

    /* Set scissor so tree elements or lines can't overlap restriction icons. */
    if (right_column_width > 0.0f) {
      int mask_x = BLI_rcti_size_x(&region->v2d.mask) - int(right_column_width) + 1;
      CLAMP_MIN(mask_x, 0);

      GPU_scissor_get(scissor);
      GPU_scissor(0, 0, mask_x, region->winy);
    }
  }

  /* Draw hierarchy lines for collections and object children. */
  {
    int starty = int(region->v2d.tot.ymax) - OL_Y_OFFSET;
    int startx = columns_offset + UI_UNIT_X / 2 - (U.pixelsize + 1) / 2;
    outliner_draw_hierarchy_lines(space_outliner, &space_outliner->tree, tvc, startx, &starty);
  }

  /* Items themselves. */
  {
    int starty = int(region->v2d.tot.ymax) - UI_UNIT_Y - OL_Y_OFFSET;
    int startx = columns_offset;
    LISTBASE_FOREACH (TreeElement *, te, &space_outliner->tree) {
      outliner_draw_tree_element(block,
                                 fstyle,
                                 tvc,
                                 region,
                                 space_outliner,
                                 te,
                                 (te->flag & TE_DRAGGING) != 0,
                                 startx,
                                 &starty,
                                 right_column_width,
                                 te_edit);
    }

    if (right_column_width > 0.0f) {
      /* Reset scissor. */
      GPU_scissor(UNPACK4(scissor));
    }
  }
}

static void outliner_back(ARegion *region)
{
  int ystart;

  ystart = int(region->v2d.tot.ymax);
  ystart = UI_UNIT_Y * (ystart / (UI_UNIT_Y)) - OL_Y_OFFSET;

  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", blender::gpu::VertAttrType::SFLOAT_32_32);

  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

  float col_alternating[4];
  UI_GetThemeColor4fv(TH_ROW_ALTERNATE, col_alternating);
  immUniformThemeColorBlend(TH_BACK, TH_ROW_ALTERNATE, col_alternating[3]);

  const float x1 = 0.0f, x2 = region->v2d.cur.xmax;
  float y1 = ystart, y2;
  int tot = int(floor(ystart - region->v2d.cur.ymin + 2 * UI_UNIT_Y)) / (2 * UI_UNIT_Y);

  if (tot > 0) {
    immBegin(GPU_PRIM_TRIS, 6 * tot);
    while (tot--) {
      y1 -= 2 * UI_UNIT_Y;
      y2 = y1 + UI_UNIT_Y;
      immVertex2f(pos, x1, y1);
      immVertex2f(pos, x2, y1);
      immVertex2f(pos, x2, y2);

      immVertex2f(pos, x1, y1);
      immVertex2f(pos, x2, y2);
      immVertex2f(pos, x1, y2);
    }
    immEnd();
  }
  immUnbindProgram();
}

static int outliner_data_api_buttons_start_x(int max_tree_width)
{
  return max_ii(OL_RNA_COLX, max_tree_width + OL_RNA_COL_SPACEX);
}

static int outliner_width(SpaceOutliner *space_outliner,
                          int max_tree_width,
                          float right_column_width)
{
  if (space_outliner->outlinevis == SO_DATA_API) {
    return outliner_data_api_buttons_start_x(max_tree_width) + OL_RNA_COL_SIZEX +
           10 * UI_SCALE_FAC;
  }
  return max_tree_width + right_column_width;
}

static void outliner_update_viewable_area(ARegion *region,
                                          SpaceOutliner *space_outliner,
                                          int tree_width,
                                          int tree_height,
                                          float right_column_width)
{
  int sizex = outliner_width(space_outliner, tree_width, right_column_width);
  int sizey = tree_height;

  /* Extend size to allow for horizontal scroll-bar and extra offset. */
  sizey += V2D_SCROLL_HEIGHT + OL_Y_OFFSET;

  UI_view2d_totRect_set(&region->v2d, sizex, sizey);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Main Entry-point
 *
 * Draw contents of Outliner editor.
 * \{ */

void draw_outliner(const bContext *C, bool do_rebuild)
{
  Main *mainvar = CTX_data_main(C);
  WorkSpace *workspace = CTX_wm_workspace(C);
  ARegion *region = CTX_wm_region(C);
  View2D *v2d = &region->v2d;
  SpaceOutliner *space_outliner = CTX_wm_space_outliner(C);
  uiBlock *block;
  TreeElement *te_edit = nullptr;

  TreeViewContext tvc;
  outliner_viewcontext_init(C, &tvc);

  /* FIXME(@ideasman42): There is an order of initialization problem here between
   * `v2d->cur` & `v2d->tot` where this function reads from `v2d->cur` for the scroll position
   * but may reset the scroll position *without* drawing into the clamped position.
   *
   * The `on_scroll` argument is used for an optional second draw pass.
   *
   * See `USE_OUTLINER_DRAW_CLAMPS_SCROLL_HACK` & #128346 for a full description. */

  if (do_rebuild) {
    outliner_build_tree(
        mainvar, workspace, tvc.scene, tvc.view_layer, space_outliner, region); /* Always. */

    /* If global sync select is dirty, flag other outliners. */
    if (ED_outliner_select_sync_is_dirty(C)) {
      ED_outliner_select_sync_flag_outliners(C);
    }

    /* Sync selection state from view layer. */
    if (space_outliner->flag & SO_SYNC_SELECT) {
      if (!ELEM(space_outliner->outlinevis,
                SO_LIBRARIES,
                SO_OVERRIDES_LIBRARY,
                SO_DATA_API,
                SO_ID_ORPHANS))
      {
        outliner_sync_selection(C, tvc, space_outliner);
      }
    }
  }

  /* Force display to pixel coords. */
  v2d->flag |= (V2D_PIXELOFS_X | V2D_PIXELOFS_Y);
  /* Set matrix for 2D-view controls. */
  UI_view2d_view_ortho(v2d);

  /* Only show mode column in View Layers and Scenes view. */
  const bool use_mode_column = outliner_shows_mode_column(*space_outliner);
  const bool use_warning_column = outliner_has_element_warnings(*space_outliner);

  /* Draw outliner stuff (background, hierarchy lines and names). */
  const float right_column_width = outliner_right_columns_width(space_outliner);
  outliner_back(region);
  block = UI_block_begin(C, region, __func__, ui::EmbossType::Emboss);
  outliner_draw_tree(block,
                     tvc,
                     region,
                     space_outliner,
                     right_column_width,
                     use_mode_column,
                     use_warning_column,
                     &te_edit);

  /* Compute outliner dimensions after it has been drawn. */
  int tree_width, tree_height;
  outliner_tree_dimensions(space_outliner, &tree_width, &tree_height);

  /* Default to no emboss for outliner UI. */
  UI_block_emboss_set(block, ui::EmbossType::NoneOrStatus);

  if (space_outliner->outlinevis == SO_DATA_API) {
    int buttons_start_x = outliner_data_api_buttons_start_x(tree_width);
    /* draw rna buttons */
    outliner_draw_separator(region, buttons_start_x);
    outliner_draw_separator(region, buttons_start_x + OL_RNA_COL_SIZEX);

    UI_block_emboss_set(block, ui::EmbossType::Emboss);
    outliner_draw_rnabuts(block, region, space_outliner, buttons_start_x);
    UI_block_emboss_set(block, ui::EmbossType::NoneOrStatus);
  }
  else if (ELEM(space_outliner->outlinevis, SO_ID_ORPHANS, SO_LIBRARIES)) {
    outliner_draw_userbuts(block, region, space_outliner);
  }
  else if (space_outliner->outlinevis == SO_OVERRIDES_LIBRARY) {
    const int x = region->v2d.cur.xmax - right_column_width;
    outliner_draw_separator(region, x);
    if (space_outliner->lib_override_view_mode == SO_LIB_OVERRIDE_VIEW_PROPERTIES) {
      UI_block_emboss_set(block, ui::EmbossType::Emboss);
      UI_block_flag_enable(block, UI_BLOCK_NO_DRAW_OVERRIDDEN_STATE);
      outliner_draw_overrides_rna_buts(block, region, space_outliner, &space_outliner->tree, x);
      UI_block_emboss_set(block, ui::EmbossType::NoneOrStatus);
    }
    else if (space_outliner->lib_override_view_mode == SO_LIB_OVERRIDE_VIEW_HIERARCHIES) {
      outliner_draw_overrides_restrictbuts(
          mainvar, block, region, space_outliner, &space_outliner->tree, x);
    }
  }
  else if (right_column_width > 0.0f) {
    /* draw restriction columns */
    RestrictPropertiesActive props_active;
    memset(&props_active, 1, sizeof(RestrictPropertiesActive));
    outliner_draw_restrictbuts(block,
                               tvc.scene,
                               tvc.view_layer,
                               region,
                               space_outliner,
                               &space_outliner->tree,
                               props_active);
  }

  /* Draw mode icons */
  if (use_mode_column) {
    outliner_draw_mode_column(block, tvc, space_outliner);
  }

  /* Draw warning icons */
  if (use_warning_column) {
    outliner_draw_warning_column(block, space_outliner, use_mode_column);
  }

  UI_block_emboss_set(block, ui::EmbossType::Emboss);

  /* Draw edit buttons if necessary. */
  if (te_edit) {
    outliner_buttons(C, block, region, right_column_width, te_edit);
  }

  UI_block_end(C, block);
  UI_block_draw(C, block);

  /* Update total viewable region. */
  outliner_update_viewable_area(
      region, space_outliner, tree_width, tree_height, right_column_width);
}

/** \} */

}  // namespace blender::ed::outliner

int ED_outliner_icon_from_id(const ID &id)
{
  return blender::ed::outliner::tree_element_get_icon_from_id(&id);
}

bool ED_outliner_support_searching(const SpaceOutliner *space_outliner)
{
  return !((space_outliner->outlinevis == SO_OVERRIDES_LIBRARY) &&
           (space_outliner->lib_override_view_mode == SO_LIB_OVERRIDE_VIEW_HIERARCHIES));
}
