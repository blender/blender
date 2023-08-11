/* SPDX-FileCopyrightText: 2004 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#include <cmath>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "DNA_collection_types.h"

#include "BLI_blenlib.h"
#include "BLI_fnmatch.h"
#include "BLI_listbase.h"
#include "BLI_mempool.h"
#include "BLI_utildefines.h"

#include "BKE_layer.h"
#include "BKE_main.h"
#include "BKE_modifier.h"
#include "BKE_outliner_treehash.hh"

#include "ED_screen.hh"

#include "UI_interface.hh"

#include "outliner_intern.hh"
#include "tree/common.hh"
#include "tree/tree_display.hh"
#include "tree/tree_element.hh"
#include "tree/tree_element_overrides.hh"

#ifdef WIN32
#  include "BLI_math_base.h" /* M_PI */
#endif

namespace blender::ed::outliner {

/* prototypes */
static int outliner_exclude_filter_get(const SpaceOutliner *space_outliner);

/* -------------------------------------------------------------------- */
/** \name Persistent Data
 * \{ */

static void outliner_storage_cleanup(SpaceOutliner *space_outliner)
{
  BLI_mempool *ts = space_outliner->treestore;

  if (ts) {
    TreeStoreElem *tselem;
    int unused = 0;

    /* each element used once, for ID blocks with more users to have each a treestore */
    BLI_mempool_iter iter;

    BLI_mempool_iternew(ts, &iter);
    while ((tselem = static_cast<TreeStoreElem *>(BLI_mempool_iterstep(&iter)))) {
      tselem->used = 0;
    }

    /* cleanup only after reading file or undo step, and always for
     * RNA data-blocks view in order to save memory */
    if (space_outliner->storeflag & SO_TREESTORE_CLEANUP) {
      space_outliner->storeflag &= ~SO_TREESTORE_CLEANUP;

      BLI_mempool_iternew(ts, &iter);
      while ((tselem = static_cast<TreeStoreElem *>(BLI_mempool_iterstep(&iter)))) {
        if (tselem->id == nullptr) {
          unused++;
        }
      }

      if (unused) {
        if (BLI_mempool_len(ts) == unused) {
          BLI_mempool_destroy(ts);
          space_outliner->treestore = nullptr;
          space_outliner->runtime->tree_hash = nullptr;
        }
        else {
          TreeStoreElem *tsenew;
          BLI_mempool *new_ts = BLI_mempool_create(
              sizeof(TreeStoreElem), BLI_mempool_len(ts) - unused, 512, BLI_MEMPOOL_ALLOW_ITER);
          BLI_mempool_iternew(ts, &iter);
          while ((tselem = static_cast<TreeStoreElem *>(BLI_mempool_iterstep(&iter)))) {
            if (tselem->id) {
              tsenew = static_cast<TreeStoreElem *>(BLI_mempool_alloc(new_ts));
              *tsenew = *tselem;
            }
          }
          BLI_mempool_destroy(ts);
          space_outliner->treestore = new_ts;
          if (space_outliner->runtime->tree_hash) {
            /* update hash table to fix broken pointers */
            space_outliner->runtime->tree_hash->rebuild_from_treestore(*space_outliner->treestore);
          }
        }
      }
    }
    else if (space_outliner->runtime->tree_hash) {
      space_outliner->runtime->tree_hash->clear_used();
    }
  }
}

static void check_persistent(
    SpaceOutliner *space_outliner, TreeElement *te, ID *id, short type, short nr)
{
  if (space_outliner->treestore == nullptr) {
    /* If treestore was not created in `readfile.cc`, create it here. */
    space_outliner->treestore = BLI_mempool_create(
        sizeof(TreeStoreElem), 1, 512, BLI_MEMPOOL_ALLOW_ITER);
  }
  if (space_outliner->runtime->tree_hash == nullptr) {
    space_outliner->runtime->tree_hash = treehash::TreeHash::create_from_treestore(
        *space_outliner->treestore);
  }

  /* find any unused tree element in treestore and mark it as used
   * (note that there may be multiple unused elements in case of linked objects) */
  TreeStoreElem *tselem = space_outliner->runtime->tree_hash->lookup_unused(type, nr, id);
  if (tselem) {
    te->store_elem = tselem;
    tselem->used = 1;
    return;
  }

  /* add 1 element to treestore */
  tselem = static_cast<TreeStoreElem *>(BLI_mempool_alloc(space_outliner->treestore));
  tselem->type = type;
  tselem->nr = type ? nr : 0;
  tselem->id = id;
  tselem->used = 0;
  tselem->flag = TSE_CLOSED;
  te->store_elem = tselem;
  space_outliner->runtime->tree_hash->add_element(*tselem);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Tree Management
 * \{ */

void outliner_free_tree(ListBase *tree)
{
  LISTBASE_FOREACH_MUTABLE (TreeElement *, element, tree) {
    outliner_free_tree_element(element, tree);
  }
}

void outliner_cleanup_tree(SpaceOutliner *space_outliner)
{
  outliner_free_tree(&space_outliner->tree);
  outliner_storage_cleanup(space_outliner);
}

void outliner_free_tree_element(TreeElement *element, ListBase *parent_subtree)
{
  BLI_assert(BLI_findindex(parent_subtree, element) > -1);
  BLI_remlink(parent_subtree, element);

  outliner_free_tree(&element->subtree);

  if (element->flag & TE_FREE_NAME) {
    MEM_freeN((void *)element->name);
  }
  element->abstract_element = nullptr;
  MEM_delete(element);
}

/* ********************************************************* */

/* -------------------------------------------------------- */

bool outliner_requires_rebuild_on_select_or_active_change(const SpaceOutliner *space_outliner)
{
  int exclude_flags = outliner_exclude_filter_get(space_outliner);
  /* Need to rebuild tree to re-apply filter if select/active changed while filtering based on
   * select/active. */
  return exclude_flags & (SO_FILTER_OB_STATE_SELECTED | SO_FILTER_OB_STATE_ACTIVE);
}

#ifdef WITH_FREESTYLE
static void outliner_add_line_styles(SpaceOutliner *space_outliner,
                                     ListBase *lb,
                                     Scene *sce,
                                     TreeElement *te)
{
  ViewLayer *view_layer;
  FreestyleLineSet *lineset;

  for (view_layer = sce->view_layers.first; view_layer; view_layer = view_layer->next) {
    for (lineset = view_layer->freestyle_config.linesets.first; lineset; lineset = lineset->next) {
      FreestyleLineStyle *linestyle = lineset->linestyle;
      if (linestyle) {
        linestyle->id.tag |= LIB_TAG_DOIT;
      }
    }
  }
  for (view_layer = sce->view_layers.first; view_layer; view_layer = view_layer->next) {
    for (lineset = view_layer->freestyle_config.linesets.first; lineset; lineset = lineset->next) {
      FreestyleLineStyle *linestyle = lineset->linestyle;
      if (linestyle) {
        if (!(linestyle->id.tag & LIB_TAG_DOIT)) {
          continue;
        }
        linestyle->id.tag &= ~LIB_TAG_DOIT;
        outliner_add_element(space_outliner, lb, linestyle, te, TSE_SOME_ID, 0);
      }
    }
  }
}
#endif

TreeElement *outliner_add_element(SpaceOutliner *space_outliner,
                                  ListBase *lb,
                                  void *idv,
                                  TreeElement *parent,
                                  short type,
                                  short index,
                                  const bool expand)
{
  ID *id = static_cast<ID *>(idv);

  if (ELEM(type, TSE_RNA_STRUCT, TSE_RNA_PROPERTY, TSE_RNA_ARRAY_ELEM)) {
    id = ((PointerRNA *)idv)->owner_id;
    if (!id) {
      id = static_cast<ID *>(((PointerRNA *)idv)->data);
    }
  }
  else if (type == TSE_GP_LAYER) {
    /* idv is the layer itself */
    id = TREESTORE(parent)->id;
  }
  else if (ELEM(type, TSE_GENERIC_LABEL)) {
    id = nullptr;
  }
  else if (ELEM(type, TSE_LIBRARY_OVERRIDE, TSE_LIBRARY_OVERRIDE_OPERATION)) {
    id = &static_cast<TreeElementOverridesData *>(idv)->id;
  }
  else if (type == TSE_BONE) {
    id = static_cast<BoneElementCreateData *>(idv)->armature_id;
  }
  else if (type == TSE_EBONE) {
    id = static_cast<EditBoneElementCreateData *>(idv)->armature_id;
  }
  else if (type == TSE_GPENCIL_EFFECT) {
    id = &static_cast<GPencilEffectElementCreateData *>(idv)->object->id;
  }
  else if (type == TSE_DEFGROUP) {
    id = &static_cast<DeformGroupElementCreateData *>(idv)->object->id;
  }
  else if (type == TSE_LINKED_PSYS) {
    id = &static_cast<ParticleSystemElementCreateData *>(idv)->object->id;
  }
  else if (type == TSE_POSEGRP) {
    id = &static_cast<PoseGroupElementCreateData *>(idv)->object->id;
  }
  else if (type == TSE_R_LAYER) {
    id = &static_cast<ViewLayerElementCreateData *>(idv)->scene->id;
  }

  /* exceptions */
  if (ELEM(type, TSE_ID_BASE, TSE_GENERIC_LABEL)) {
    /* pass */
  }
  else if (id == nullptr) {
    return nullptr;
  }

  if (type == TSE_SOME_ID) {
    /* Real ID, ensure we do not get non-outliner ID types here... */
    BLI_assert(TREESTORE_ID_TYPE(id));
  }

  TreeElement *te = MEM_new<TreeElement>(__func__);
  /* add to the visual tree */
  BLI_addtail(lb, te);
  /* add to the storage */
  check_persistent(space_outliner, te, id, type, index);
  TreeStoreElem *tselem = TREESTORE(te);

  /* if we are searching for something expand to see child elements */
  if (SEARCHING_OUTLINER(space_outliner)) {
    tselem->flag |= TSE_CHILDSEARCH;
  }

  te->parent = parent;
  te->index = index; /* For data arrays. */

  /* New inheritance based element representation. Not all element types support this yet,
   * eventually it should replace #TreeElement entirely. */
  te->abstract_element = AbstractTreeElement::createFromType(type, *te, idv);
  if (te->abstract_element) {
    /* Element types ported to the new design are expected to have their name set at this point! */
    BLI_assert(te->name != nullptr);
  }

  if (ELEM(type, TSE_SEQUENCE, TSE_SEQ_STRIP, TSE_SEQUENCE_DUP)) {
    /* pass */
  }
  else if (ELEM(type, TSE_RNA_STRUCT, TSE_RNA_PROPERTY, TSE_RNA_ARRAY_ELEM)) {
    /* pass */
  }
  else if (ELEM(type, TSE_ANIM_DATA, TSE_NLA, TSE_NLA_TRACK, TSE_DRIVER_BASE)) {
    /* pass */
  }
  else if (type == TSE_GP_LAYER) {
    /* pass */
  }
  else if (ELEM(type, TSE_LAYER_COLLECTION, TSE_SCENE_COLLECTION_BASE, TSE_VIEW_COLLECTION_BASE)) {
    /* pass */
  }
  else if (ELEM(type, TSE_ID_BASE, TSE_GENERIC_LABEL)) {
    /* pass */
  }
  else if (ELEM(type, TSE_BONE, TSE_EBONE)) {
    /* pass */
  }
  else if (ELEM(type, TSE_GPENCIL_EFFECT_BASE, TSE_GPENCIL_EFFECT)) {
    /* pass */
  }
  else if (ELEM(type, TSE_DEFGROUP, TSE_DEFGROUP_BASE)) {
    /* pass */
  }
  else if (type == TSE_LINKED_PSYS) {
    /* pass */
  }
  else if (ELEM(type, TSE_POSEGRP, TSE_POSEGRP_BASE)) {
    /* pass */
  }
  else if (ELEM(type, TSE_R_LAYER, TSE_R_LAYER_BASE)) {
    /* pass */
  }
  else if (type == TSE_SOME_ID) {
    if (!te->abstract_element) {
      BLI_assert_msg(0, "Expected this ID type to be ported to new Outliner tree-element design");
    }
  }
  else if (ELEM(type,
                TSE_LIBRARY_OVERRIDE_BASE,
                TSE_LIBRARY_OVERRIDE,
                TSE_LIBRARY_OVERRIDE_OPERATION))
  {
    if (!te->abstract_element) {
      BLI_assert_msg(0,
                     "Expected override types to be ported to new Outliner tree-element design");
    }
  }
  else {
    /* Other cases must be caught above. */
    BLI_assert(TSE_IS_REAL_ID(tselem));

    /* The new type design sets the name already, don't override that here. We need to figure out
     * how to deal with the idcode for non-TSE_SOME_ID types still. Some rely on it... */
    if (!te->abstract_element) {
      te->name = id->name + 2; /* Default, can be overridden by Library or non-ID data. */
    }
    te->idcode = GS(id->name);
  }

  if (!expand) {
    /* Pass */
  }
  else if (te->abstract_element) {
    tree_element_expand(*te->abstract_element, *space_outliner);
  }
  else if (ELEM(type,
                TSE_ANIM_DATA,
                TSE_BONE,
                TSE_DRIVER_BASE,
                TSE_EBONE,
                TSE_LINKED_PSYS,
                TSE_NLA,
                TSE_NLA_ACTION,
                TSE_NLA_TRACK,
                TSE_GP_LAYER,
                TSE_RNA_STRUCT,
                TSE_RNA_PROPERTY,
                TSE_RNA_ARRAY_ELEM,
                TSE_SEQUENCE,
                TSE_SEQ_STRIP,
                TSE_SEQUENCE_DUP,
                TSE_GENERIC_LABEL) ||
           ELEM(type,
                TSE_DEFGROUP,
                TSE_DEFGROUP_BASE,
                TSE_GPENCIL_EFFECT,
                TSE_GPENCIL_EFFECT_BASE,
                TSE_POSEGRP,
                TSE_POSEGRP_BASE,
                TSE_R_LAYER,
                TSE_R_LAYER_BASE))
  {
    BLI_assert_msg(false, "Element type should already use new AbstractTreeElement design");
  }

  return te;
}

/* ======================================================= */

BLI_INLINE void outliner_add_collection_init(TreeElement *te, Collection *collection)
{
  te->name = BKE_collection_ui_name_get(collection);
  te->directdata = collection;
}

BLI_INLINE void outliner_add_collection_objects(SpaceOutliner *space_outliner,
                                                ListBase *tree,
                                                Collection *collection,
                                                TreeElement *parent)
{
  LISTBASE_FOREACH (CollectionObject *, cob, &collection->gobject) {
    outliner_add_element(space_outliner, tree, cob->ob, parent, TSE_SOME_ID, 0);
  }
}

TreeElement *outliner_add_collection_recursive(SpaceOutliner *space_outliner,
                                               Collection *collection,
                                               TreeElement *ten)
{
  outliner_add_collection_init(ten, collection);

  LISTBASE_FOREACH (CollectionChild *, child, &collection->children) {
    outliner_add_element(
        space_outliner, &ten->subtree, &child->collection->id, ten, TSE_SOME_ID, 0);
  }

  if (space_outliner->outlinevis != SO_SCENES) {
    outliner_add_collection_objects(space_outliner, &ten->subtree, collection, ten);
  }

  return ten;
}

/** \} */

/* ======================================================= */
/* Generic Tree Building helpers - order these are called is top to bottom */

/* -------------------------------------------------------------------- */
/** \name Tree Sorting Helper
 * \{ */

struct tTreeSort {
  TreeElement *te;
  ID *id;
  const char *name;
  short idcode;
};

/* alphabetical comparator, trying to put objects first */
static int treesort_alpha_ob(const void *v1, const void *v2)
{
  const tTreeSort *x1 = static_cast<const tTreeSort *>(v1);
  const tTreeSort *x2 = static_cast<const tTreeSort *>(v2);

  /* first put objects last (hierarchy) */
  int comp = (x1->idcode == ID_OB);
  if (x2->idcode == ID_OB) {
    comp += 2;
  }

  if (comp == 1) {
    return 1;
  }
  if (comp == 2) {
    return -1;
  }
  if (comp == 3) {
    /* Among objects first come the ones in the collection, followed by the ones not on it.
     * This way we can have the dashed lines in a separate style connecting the former. */
    if ((x1->te->flag & TE_CHILD_NOT_IN_COLLECTION) != (x2->te->flag & TE_CHILD_NOT_IN_COLLECTION))
    {
      return (x1->te->flag & TE_CHILD_NOT_IN_COLLECTION) ? 1 : -1;
    }

    comp = BLI_strcasecmp_natural(x1->name, x2->name);

    if (comp > 0) {
      return 1;
    }
    if (comp < 0) {
      return -1;
    }
    return 0;
  }
  return 0;
}

/* Move children that are not in the collection to the end of the list. */
static int treesort_child_not_in_collection(const void *v1, const void *v2)
{
  const tTreeSort *x1 = static_cast<const tTreeSort *>(v1);
  const tTreeSort *x2 = static_cast<const tTreeSort *>(v2);

  /* Among objects first come the ones in the collection, followed by the ones not on it.
   * This way we can have the dashed lines in a separate style connecting the former. */
  if ((x1->te->flag & TE_CHILD_NOT_IN_COLLECTION) != (x2->te->flag & TE_CHILD_NOT_IN_COLLECTION)) {
    return (x1->te->flag & TE_CHILD_NOT_IN_COLLECTION) ? 1 : -1;
  }
  return 0;
}

/* alphabetical comparator */
static int treesort_alpha(const void *v1, const void *v2)
{
  const tTreeSort *x1 = static_cast<const tTreeSort *>(v1);
  const tTreeSort *x2 = static_cast<const tTreeSort *>(v2);

  int comp = BLI_strcasecmp_natural(x1->name, x2->name);

  if (comp > 0) {
    return 1;
  }
  if (comp < 0) {
    return -1;
  }
  return 0;
}

/* this is nice option for later? doesn't look too useful... */
#if 0
static int treesort_obtype_alpha(const void *v1, const void *v2)
{
  const tTreeSort *x1 = v1, *x2 = v2;

  /* first put objects last (hierarchy) */
  if (x1->idcode == ID_OB && x2->idcode != ID_OB) {
    return 1;
  }
  else if (x2->idcode == ID_OB && x1->idcode != ID_OB) {
    return -1;
  }
  else {
    /* 2nd we check ob type */
    if (x1->idcode == ID_OB && x2->idcode == ID_OB) {
      if (((Object *)x1->id)->type > ((Object *)x2->id)->type) {
        return 1;
      }
      else if (((Object *)x1->id)->type > ((Object *)x2->id)->type) {
        return -1;
      }
      else {
        return 0;
      }
    }
    else {
      int comp = BLI_strcasecmp_natural(x1->name, x2->name);

      if (comp > 0) {
        return 1;
      }
      else if (comp < 0) {
        return -1;
      }
      return 0;
    }
  }
}
#endif

/* sort happens on each subtree individual */
static void outliner_sort(ListBase *lb)
{
  TreeElement *last_te = static_cast<TreeElement *>(lb->last);
  if (last_te == nullptr) {
    return;
  }
  TreeStoreElem *last_tselem = TREESTORE(last_te);

  /* Sorting rules; only object lists, ID lists, or deform-groups. */
  if (ELEM(last_tselem->type, TSE_DEFGROUP, TSE_ID_BASE) ||
      ((last_tselem->type == TSE_SOME_ID) && (last_te->idcode == ID_OB)))
  {
    int totelem = BLI_listbase_count(lb);

    if (totelem > 1) {
      tTreeSort *tear = static_cast<tTreeSort *>(
          MEM_mallocN(totelem * sizeof(tTreeSort), "tree sort array"));
      tTreeSort *tp = tear;
      int skip = 0;

      LISTBASE_FOREACH (TreeElement *, te, lb) {
        TreeStoreElem *tselem = TREESTORE(te);
        tp->te = te;
        tp->name = te->name;
        tp->idcode = te->idcode;

        if (!ELEM(tselem->type, TSE_SOME_ID, TSE_DEFGROUP)) {
          tp->idcode = 0; /* Don't sort this. */
        }
        if (tselem->type == TSE_ID_BASE) {
          tp->idcode = 1; /* Do sort this. */
        }

        tp->id = tselem->id;
        tp++;
      }

      /* just sort alphabetically */
      if (tear->idcode == 1) {
        qsort(tear, totelem, sizeof(tTreeSort), treesort_alpha);
      }
      else {
        /* keep beginning of list */
        for (tp = tear, skip = 0; skip < totelem; skip++, tp++) {
          if (tp->idcode) {
            break;
          }
        }

        if (skip < totelem) {
          qsort(tear + skip, totelem - skip, sizeof(tTreeSort), treesort_alpha_ob);
        }
      }

      BLI_listbase_clear(lb);
      tp = tear;
      while (totelem--) {
        BLI_addtail(lb, tp->te);
        tp++;
      }
      MEM_freeN(tear);
    }
  }

  LISTBASE_FOREACH (TreeElement *, te_iter, lb) {
    outliner_sort(&te_iter->subtree);
  }
}

static void outliner_collections_children_sort(ListBase *lb)
{
  TreeElement *last_te = static_cast<TreeElement *>(lb->last);
  if (last_te == nullptr) {
    return;
  }
  TreeStoreElem *last_tselem = TREESTORE(last_te);

  /* Sorting rules: only object lists. */
  if ((last_tselem->type == TSE_SOME_ID) && (last_te->idcode == ID_OB)) {
    int totelem = BLI_listbase_count(lb);

    if (totelem > 1) {
      tTreeSort *tear = static_cast<tTreeSort *>(
          MEM_mallocN(totelem * sizeof(tTreeSort), "tree sort array"));
      tTreeSort *tp = tear;

      LISTBASE_FOREACH (TreeElement *, te, lb) {
        TreeStoreElem *tselem = TREESTORE(te);
        tp->te = te;
        tp->name = te->name;
        tp->idcode = te->idcode;
        tp->id = tselem->id;
        tp++;
      }

      qsort(tear, totelem, sizeof(tTreeSort), treesort_child_not_in_collection);

      BLI_listbase_clear(lb);
      tp = tear;
      while (totelem--) {
        BLI_addtail(lb, tp->te);
        tp++;
      }
      MEM_freeN(tear);
    }
  }

  LISTBASE_FOREACH (TreeElement *, te_iter, lb) {
    outliner_collections_children_sort(&te_iter->subtree);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Tree Filtering Helper
 * \{ */

struct OutlinerTreeElementFocus {
  TreeStoreElem *tselem;
  int ys;
};

/**
 * Bring the outliner scrolling back to where it was in relation to the original focus element
 * Caller is expected to handle redrawing of ARegion.
 */
static void outliner_restore_scrolling_position(SpaceOutliner *space_outliner,
                                                ARegion *region,
                                                OutlinerTreeElementFocus *focus)
{
  View2D *v2d = &region->v2d;

  if (focus->tselem != nullptr) {
    outliner_set_coordinates(region, space_outliner);

    TreeElement *te_new = outliner_find_tree_element(&space_outliner->tree, focus->tselem);

    if (te_new != nullptr) {
      int ys_new = te_new->ys;
      int ys_old = focus->ys;

      float y_move = MIN2(ys_new - ys_old, -v2d->cur.ymax);
      BLI_rctf_translate(&v2d->cur, 0, y_move);
    }
    else {
      return;
    }
  }
}

static bool test_collection_callback(TreeElement *te)
{
  return outliner_is_collection_tree_element(te);
}

static bool test_object_callback(TreeElement *te)
{
  TreeStoreElem *tselem = TREESTORE(te);
  return ((tselem->type == TSE_SOME_ID) && (te->idcode == ID_OB));
}

/**
 * See if TreeElement or any of its children pass the callback_test.
 */
static TreeElement *outliner_find_first_desired_element_at_y_recursive(
    const SpaceOutliner *space_outliner,
    TreeElement *te,
    const float limit,
    bool (*callback_test)(TreeElement *))
{
  if (callback_test(te)) {
    return te;
  }

  if (TSELEM_OPEN(te->store_elem, space_outliner)) {
    LISTBASE_FOREACH (TreeElement *, te_iter, &te->subtree) {
      TreeElement *te_sub = outliner_find_first_desired_element_at_y_recursive(
          space_outliner, te_iter, limit, callback_test);
      if (te_sub != nullptr) {
        return te_sub;
      }
    }
  }

  return nullptr;
}

/**
 * Find the first element that passes a test starting from a reference vertical coordinate
 *
 * If the element that is in the position is not what we are looking for, keep looking for its
 * children, siblings, and eventually, aunts, cousins, distant families, ... etc.
 *
 * Basically we keep going up and down the outliner tree from that point forward, until we find
 * what we are looking for. If we are past the visible range and we can't find a valid element
 * we return nullptr.
 */
static TreeElement *outliner_find_first_desired_element_at_y(const SpaceOutliner *space_outliner,
                                                             const float view_co,
                                                             const float view_co_limit)
{
  TreeElement *te = outliner_find_item_at_y(space_outliner, &space_outliner->tree, view_co);

  bool (*callback_test)(TreeElement *);
  if ((space_outliner->outlinevis == SO_VIEW_LAYER) &&
      (space_outliner->filter & SO_FILTER_NO_COLLECTION))
  {
    callback_test = test_object_callback;
  }
  else {
    callback_test = test_collection_callback;
  }

  while (te != nullptr) {
    TreeElement *te_sub = outliner_find_first_desired_element_at_y_recursive(
        space_outliner, te, view_co_limit, callback_test);
    if (te_sub != nullptr) {
      /* Skip the element if it was not visible to start with. */
      if (te->ys + UI_UNIT_Y > view_co_limit) {
        return te_sub;
      }
      return nullptr;
    }

    if (te->next) {
      te = te->next;
      continue;
    }

    if (te->parent == nullptr) {
      break;
    }

    while (te->parent) {
      if (te->parent->next) {
        te = te->parent->next;
        break;
      }
      te = te->parent;
    }
  }

  return nullptr;
}

/**
 * Store information of current outliner scrolling status to be restored later.
 *
 * Finds the top-most collection visible in the outliner and populates the
 * #OutlinerTreeElementFocus struct to retrieve this element later to make sure it is in the same
 * original position as before filtering.
 */
static void outliner_store_scrolling_position(SpaceOutliner *space_outliner,
                                              ARegion *region,
                                              OutlinerTreeElementFocus *focus)
{
  float limit = region->v2d.cur.ymin;

  outliner_set_coordinates(region, space_outliner);

  TreeElement *te = outliner_find_first_desired_element_at_y(
      space_outliner, region->v2d.cur.ymax, limit);

  if (te != nullptr) {
    focus->tselem = TREESTORE(te);
    focus->ys = te->ys;
  }
  else {
    focus->tselem = nullptr;
  }
}

static int outliner_exclude_filter_get(const SpaceOutliner *space_outliner)
{
  int exclude_filter = space_outliner->filter & ~SO_FILTER_OB_STATE;

  if (space_outliner->search_string[0] != 0) {
    exclude_filter |= SO_FILTER_SEARCH;
  }
  else {
    exclude_filter &= ~SO_FILTER_SEARCH;
  }

  /* Let's have this for the collection options at first. */
  if (!SUPPORT_FILTER_OUTLINER(space_outliner)) {
    return (exclude_filter & SO_FILTER_SEARCH);
  }

  if (space_outliner->filter & SO_FILTER_NO_OBJECT) {
    exclude_filter |= SO_FILTER_OB_TYPE;
  }

  switch (space_outliner->filter_state) {
    case SO_FILTER_OB_VISIBLE:
      exclude_filter |= SO_FILTER_OB_STATE_VISIBLE;
      break;
    case SO_FILTER_OB_SELECTED:
      exclude_filter |= SO_FILTER_OB_STATE_SELECTED;
      break;
    case SO_FILTER_OB_ACTIVE:
      exclude_filter |= SO_FILTER_OB_STATE_ACTIVE;
      break;
    case SO_FILTER_OB_SELECTABLE:
      exclude_filter |= SO_FILTER_OB_STATE_SELECTABLE;
      break;
  }

  return exclude_filter;
}

static bool outliner_element_visible_get(const Scene *scene,
                                         ViewLayer *view_layer,
                                         TreeElement *te,
                                         const int exclude_filter)
{
  if ((exclude_filter & SO_FILTER_ANY) == 0) {
    return true;
  }

  TreeStoreElem *tselem = TREESTORE(te);
  if ((tselem->type == TSE_SOME_ID) && (te->idcode == ID_OB)) {
    if ((exclude_filter & SO_FILTER_OB_TYPE) == SO_FILTER_OB_TYPE) {
      return false;
    }

    Object *ob = (Object *)tselem->id;
    Base *base = (Base *)te->directdata;
    BLI_assert((base == nullptr) || (base->object == ob));

    if (exclude_filter & SO_FILTER_OB_TYPE) {
      switch (ob->type) {
        case OB_MESH:
          if (exclude_filter & SO_FILTER_NO_OB_MESH) {
            return false;
          }
          break;
        case OB_ARMATURE:
          if (exclude_filter & SO_FILTER_NO_OB_ARMATURE) {
            return false;
          }
          break;
        case OB_EMPTY:
          if (exclude_filter & SO_FILTER_NO_OB_EMPTY) {
            return false;
          }
          break;
        case OB_LAMP:
          if (exclude_filter & SO_FILTER_NO_OB_LAMP) {
            return false;
          }
          break;
        case OB_CAMERA:
          if (exclude_filter & SO_FILTER_NO_OB_CAMERA) {
            return false;
          }
          break;
        case OB_GPENCIL_LEGACY:
          if (exclude_filter & SO_FILTER_NO_OB_GPENCIL_LEGACY) {
            return false;
          }
          break;
        default:
          if (exclude_filter & SO_FILTER_NO_OB_OTHERS) {
            return false;
          }
          break;
      }
    }

    if (exclude_filter & SO_FILTER_OB_STATE) {
      if (base == nullptr) {
        BKE_view_layer_synced_ensure(scene, view_layer);
        base = BKE_view_layer_base_find(view_layer, ob);

        if (base == nullptr) {
          return false;
        }
      }

      bool is_visible = true;
      if (exclude_filter & SO_FILTER_OB_STATE_VISIBLE) {
        if ((base->flag & BASE_ENABLED_AND_VISIBLE_IN_DEFAULT_VIEWPORT) == 0) {
          is_visible = false;
        }
      }
      else if (exclude_filter & SO_FILTER_OB_STATE_SELECTED) {
        if ((base->flag & BASE_SELECTED) == 0) {
          is_visible = false;
        }
      }
      else if (exclude_filter & SO_FILTER_OB_STATE_SELECTABLE) {
        if ((base->flag & BASE_SELECTABLE) == 0) {
          is_visible = false;
        }
      }
      else {
        BLI_assert(exclude_filter & SO_FILTER_OB_STATE_ACTIVE);
        BKE_view_layer_synced_ensure(scene, view_layer);
        if (base != BKE_view_layer_active_base_get(view_layer)) {
          is_visible = false;
        }
      }

      if (exclude_filter & SO_FILTER_OB_STATE_INVERSE) {
        is_visible = !is_visible;
      }

      return is_visible;
    }

    if ((te->parent != nullptr) && (TREESTORE(te->parent)->type == TSE_SOME_ID) &&
        (te->parent->idcode == ID_OB))
    {
      if (exclude_filter & SO_FILTER_NO_CHILDREN) {
        return false;
      }
    }
  }
  else if ((te->parent != nullptr) && (TREESTORE(te->parent)->type == TSE_SOME_ID) &&
           (te->parent->idcode == ID_OB))
  {
    if (exclude_filter & SO_FILTER_NO_OB_CONTENT) {
      return false;
    }
  }

  return true;
}

static bool outliner_filter_has_name(TreeElement *te, const char *name, int flags)
{
  int fn_flag = 0;

  if ((flags & SO_FIND_CASE_SENSITIVE) == 0) {
    fn_flag |= FNM_CASEFOLD;
  }

  return fnmatch(name, te->name, fn_flag) == 0;
}

static bool outliner_element_is_collection_or_object(TreeElement *te)
{
  TreeStoreElem *tselem = TREESTORE(te);

  if ((tselem->type == TSE_SOME_ID) && (te->idcode == ID_OB)) {
    return true;
  }

  /* Collection instance datablocks should not be extracted. */
  if (outliner_is_collection_tree_element(te) && !(te->parent && te->parent->idcode == ID_OB)) {
    return true;
  }

  return false;
}

static TreeElement *outliner_extract_children_from_subtree(TreeElement *element,
                                                           ListBase *parent_subtree)
{
  TreeElement *te_next = element->next;

  if (outliner_element_is_collection_or_object(element)) {
    TreeElement *te_prev = nullptr;
    for (TreeElement *te = static_cast<TreeElement *>(element->subtree.last); te; te = te_prev) {
      te_prev = te->prev;

      if (!outliner_element_is_collection_or_object(te)) {
        continue;
      }

      te_next = te;
      BLI_remlink(&element->subtree, te);
      BLI_insertlinkafter(parent_subtree, element->prev, te);
      te->parent = element->parent;
    }
  }

  outliner_free_tree_element(element, parent_subtree);
  return te_next;
}

static int outliner_filter_subtree(SpaceOutliner *space_outliner,
                                   const Scene *scene,
                                   ViewLayer *view_layer,
                                   ListBase *lb,
                                   const char *search_string,
                                   const int exclude_filter)
{
  TreeElement *te, *te_next;
  TreeStoreElem *tselem;

  for (te = static_cast<TreeElement *>(lb->first); te; te = te_next) {
    te_next = te->next;
    if (outliner_element_visible_get(scene, view_layer, te, exclude_filter) == false) {
      /* Don't free the tree, but extract the children from the parent and add to this tree. */
      /* This also needs filtering the subtree prior (see #69246). */
      outliner_filter_subtree(
          space_outliner, scene, view_layer, &te->subtree, search_string, exclude_filter);
      te_next = outliner_extract_children_from_subtree(te, lb);
      continue;
    }
    if ((exclude_filter & SO_FILTER_SEARCH) == 0) {
      /* Filter subtree too. */
      outliner_filter_subtree(
          space_outliner, scene, view_layer, &te->subtree, search_string, exclude_filter);
      continue;
    }

    if (!outliner_filter_has_name(te, search_string, space_outliner->search_flags)) {
      /* item isn't something we're looking for, but...
       * - if the subtree is expanded, check if there are any matches that can be easily found
       *     so that searching for "cu" in the default scene will still match the Cube
       * - otherwise, we can't see within the subtree and the item doesn't match,
       *     so these can be safely ignored (i.e. the subtree can get freed)
       */
      tselem = TREESTORE(te);

      /* flag as not a found item */
      tselem->flag &= ~TSE_SEARCHMATCH;

      if (!TSELEM_OPEN(tselem, space_outliner) ||
          outliner_filter_subtree(
              space_outliner, scene, view_layer, &te->subtree, search_string, exclude_filter) == 0)
      {
        outliner_free_tree_element(te, lb);
      }
    }
    else {
      tselem = TREESTORE(te);

      /* flag as a found item - we can then highlight it */
      tselem->flag |= TSE_SEARCHMATCH;

      /* filter subtree too */
      outliner_filter_subtree(
          space_outliner, scene, view_layer, &te->subtree, search_string, exclude_filter);
    }
  }

  /* if there are still items in the list, that means that there were still some matches */
  return (BLI_listbase_is_empty(lb) == false);
}

static void outliner_filter_tree(SpaceOutliner *space_outliner,
                                 const Scene *scene,
                                 ViewLayer *view_layer)
{
  char search_buff[sizeof(SpaceOutliner::search_string) + 2];
  char *search_string;

  const int exclude_filter = outliner_exclude_filter_get(space_outliner);

  if (exclude_filter == 0) {
    return;
  }

  if (space_outliner->search_flags & SO_FIND_COMPLETE) {
    search_string = space_outliner->search_string;
  }
  else {
    /* Implicitly add heading/trailing wildcards if needed. */
    BLI_strncpy_ensure_pad(search_buff, space_outliner->search_string, '*', sizeof(search_buff));
    search_string = search_buff;
  }

  outliner_filter_subtree(
      space_outliner, scene, view_layer, &space_outliner->tree, search_string, exclude_filter);
}

static void outliner_clear_newid_from_main(Main *bmain)
{
  ID *id_iter;
  FOREACH_MAIN_ID_BEGIN (bmain, id_iter) {
    id_iter->newid = nullptr;
  }
  FOREACH_MAIN_ID_END;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Main Tree Building API
 * \{ */

void outliner_build_tree(Main *mainvar,
                         Scene *scene,
                         ViewLayer *view_layer,
                         SpaceOutliner *space_outliner,
                         ARegion *region)
{
  /* Are we looking for something - we want to tag parents to filter child matches
   * - NOT in data-blocks view - searching all data-blocks takes way too long to be useful
   * - this variable is only set once per tree build */
  if (space_outliner->search_string[0] != 0 && space_outliner->outlinevis != SO_DATA_API) {
    space_outliner->search_flags |= SO_SEARCH_RECURSIVE;
  }
  else {
    space_outliner->search_flags &= ~SO_SEARCH_RECURSIVE;
  }

  if (space_outliner->runtime->tree_hash && (space_outliner->storeflag & SO_TREESTORE_REBUILD) &&
      space_outliner->treestore)
  {
    space_outliner->runtime->tree_hash->rebuild_from_treestore(*space_outliner->treestore);
  }
  space_outliner->storeflag &= ~SO_TREESTORE_REBUILD;

  if (region->do_draw & RGN_DRAW_NO_REBUILD) {
    BLI_assert_msg(space_outliner->runtime->tree_display != nullptr,
                   "Skipping rebuild before tree was built properly, a full redraw should be "
                   "triggered instead");
    return;
  }

  /* Enable for benchmarking. Starts a timer, results will be printed on function exit. */
  // SCOPED_TIMER("Outliner Rebuild");
  // SCOPED_TIMER_AVERAGED("Outliner Rebuild");

  OutlinerTreeElementFocus focus;
  outliner_store_scrolling_position(space_outliner, region, &focus);

  outliner_free_tree(&space_outliner->tree);
  outliner_storage_cleanup(space_outliner);

  space_outliner->runtime->tree_display = AbstractTreeDisplay::createFromDisplayMode(
      space_outliner->outlinevis, *space_outliner);

  /* All tree displays should be created as sub-classes of AbstractTreeDisplay. */
  BLI_assert(space_outliner->runtime->tree_display != nullptr);

  TreeSourceData source_data{*mainvar, *scene, *view_layer};
  space_outliner->tree = space_outliner->runtime->tree_display->buildTree(source_data);

  if ((space_outliner->flag & SO_SKIP_SORT_ALPHA) == 0) {
    outliner_sort(&space_outliner->tree);
  }
  else if ((space_outliner->filter & SO_FILTER_NO_CHILDREN) == 0) {
    /* We group the children that are in the collection before the ones that are not.
     * This way we can try to draw them in a different style altogether.
     * We also have to respect the original order of the elements in case alphabetical
     * sorting is not enabled. This keep object data and modifiers before its children. */
    outliner_collections_children_sort(&space_outliner->tree);
  }

  outliner_filter_tree(space_outliner, scene, view_layer);
  outliner_restore_scrolling_position(space_outliner, region, &focus);

  /* `ID.newid` pointer is abused when building tree, DO NOT call #BKE_main_id_newptr_and_tag_clear
   * as this expects valid IDs in this pointer, not random unknown data. */
  outliner_clear_newid_from_main(mainvar);
}

/** \} */

}  // namespace blender::ed::outliner
