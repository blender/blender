/* SPDX-FileCopyrightText: 2004 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#include <algorithm>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "DNA_collection_types.h"

#include "BLI_fnmatch.hh"
#include "BLI_listbase.hh"
#include "BLI_mempool.hh"
#include "BLI_rect.hh"
#include "BLI_string.hh"
#include "BLI_utildefines.hh"

#include "BKE_layer.hh"
#include "BKE_main.hh"
#include "BKE_modifier.hh"
#include "BKE_outliner_treehash.hh"
#include "BKE_screen.hh"

#include "ED_outliner.hh"
#include "ED_screen.hh"

#include "UI_interface.hh"

#include "outliner_intern.hh"
#include "tree/tree_display.hh"
#include "tree/tree_element.hh"
#include "tree/tree_element_id.hh"

#ifdef WIN32
#  include "BLI_math_base_c.hh" /* M_PI */
#endif

namespace blender::ed::outliner {

/* prototypes */
static eSpaceOutliner_Filter outliner_exclude_filter_get(const SpaceOutliner *space_outliner);

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
  tselem->type = eTreeStoreElemType(type);
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

void outliner_free_tree(ListBaseT<TreeElement> *tree)
{
  for (TreeElement &element : tree->items_mutable()) {
    outliner_free_tree_element(&element, tree);
  }
}

void outliner_cleanup_tree(SpaceOutliner *space_outliner)
{
  outliner_free_tree(&space_outliner->runtime->tree);
  outliner_storage_cleanup(space_outliner);
}

void outliner_free_tree_element(TreeElement *element, ListBaseT<TreeElement> *parent_subtree)
{
  BLI_assert(BLI_findindex(parent_subtree, element) > -1);
  BLI_remlink(parent_subtree, element);

  outliner_free_tree(&element->subtree);

  if (element->flag & TE_FREE_NAME) {
    MEM_delete(element->name);
  }
  element->abstract_element = nullptr;
  MEM_delete(element);
}

bool outliner_requires_rebuild_on_select_or_active_change(const SpaceOutliner *space_outliner)
{
  eSpaceOutliner_Filter exclude_flags = outliner_exclude_filter_get(space_outliner);
  /* Need to rebuild tree to re-apply filter if select/active changed while filtering based on
   * select/active. */
  return exclude_flags & (SO_FILTER_OB_STATE_SELECTED | SO_FILTER_OB_STATE_ACTIVE);
}

TreeElement *AbstractTreeDisplay::add_id_element(const TreeElementAddParams &params, ID *id)
{
  if (id == nullptr) {
    return nullptr;
  }
  /* Real ID, ensure we do not get non-outliner ID types here... */
  BLI_assert(TREESTORE_ID_TYPE(id));

  TreeElement *te = add_element_impl(
      params,
      TSE_SOME_ID,
      id,
      nullptr,
      false,
      [&](TreeElement &legacy_te) -> std::unique_ptr<AbstractTreeElement> {
        return TreeElementID::create_from_id(legacy_te, *id);
      });
  BLI_assert_msg(te->abstract_element != nullptr,
                 "Expected this ID type to be ported to new Outliner tree-element design");
  return te;
}

TreeElement *AbstractTreeDisplay::add_element_impl(
    const TreeElementAddParams &params,
    const short type,
    ID *owner_id,
    const void *persistent_ptr,
    const bool allow_null_identity,
    FunctionRef<std::unique_ptr<AbstractTreeElement>(TreeElement &)> construct_fn)
{
  ListBaseT<TreeElement> *lb = params.lb ? params.lb :
                                           (params.parent ? &params.parent->subtree : nullptr);
  BLI_assert_msg(lb != nullptr, "Either a sub-tree or a parent to add the element to is required");

  /* Pointer to store in #TreeStoreElem.id to identify the element over rebuilds and reconstruct it
   * on file read. This is never an arbitrary pointer that happens to be reinterpreted as an ID: it
   * is either an actual ID, or a pointer the element type explicitly nominated for identification
   * purposes only. */
  ID *persistent_dataptr = owner_id ? owner_id :
                                      static_cast<ID *>(const_cast<void *>(persistent_ptr));
  if (persistent_dataptr == nullptr && !allow_null_identity) {
    /* Nothing to identify the element by, so it could not be recognized over rebuilds. Matches the
     * behavior of the `void *` #add_element(), which skips such elements entirely. */
    return nullptr;
  }

  const short index = short(params.index);

  TreeElement *te = MEM_new<TreeElement>(__func__);
  /* add to the visual tree */
  BLI_addtail(lb, te);
  /* add to the storage */
  check_persistent(&space_outliner_, te, persistent_dataptr, type, index);

  /* if we are searching for something expand to see child elements */
  if (SEARCHING_OUTLINER(&space_outliner_)) {
    TREESTORE(te)->flag |= TSE_CHILDSEARCH;
  }

  te->parent = params.parent;
  te->index = index; /* For data arrays. */

  /* Note that this may fail, #TreeElementID::create_from_id() returns null for ID types the
   * Outliner doesn't build elements for (e.g. deprecated ones). */
  te->abstract_element = construct_fn(*te);
  if (te->abstract_element == nullptr) {
    return te;
  }
  /* Let the new element inherit the tree display that creates this current tree. */
  te->abstract_element->display_ = this;

  /* Element types are expected to have their name set at this point! */
  BLI_assert(te->name != nullptr);

  if (params.expand) {
    tree_element_expand(*te->abstract_element, space_outliner_);
  }

  return te;
}

BLI_INLINE void outliner_add_collection_init(TreeElement *te, Collection *collection)
{
  te->name = BKE_collection_ui_name_get(collection);
  te->directdata = collection;
}

BLI_INLINE void outliner_add_collection_objects(AbstractTreeDisplay &tree_display,
                                                ListBaseT<TreeElement> *tree,
                                                Collection *collection,
                                                TreeElement *parent)
{
  for (CollectionObject &cob : collection->gobject) {
    tree_display.add_id_element({.lb = tree, .parent = parent}, reinterpret_cast<ID *>(cob.ob));
  }
}

TreeElement *outliner_add_collection_recursive(AbstractTreeDisplay &tree_display,
                                               SpaceOutliner *space_outliner,
                                               Collection *collection,
                                               TreeElement *ten)
{
  outliner_add_collection_init(ten, collection);

  for (CollectionChild &child : collection->children) {
    tree_display.add_id_element({.lb = &ten->subtree, .parent = ten}, &child.collection->id);
  }

  if (space_outliner->outlinevis != SO_SCENES) {
    outliner_add_collection_objects(tree_display, &ten->subtree, collection, ten);
  }

  return ten;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Tree Sorting Helper
 *
 * Generic tree building helpers, the order these are called is top to bottom.
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
static void outliner_sort(ListBaseT<TreeElement> *lb)
{
  /* Sorting trying to handle these cases:
   * - contents of collections (where contained collections
   *   come first, stay first and are not sorted, followed by
   *   objects etc - which ARE sorted).
   * - contents of "Vertex Groups" (these ARE all sorted, nothing
   *   else in there).
   * - contents of armature data (where optional contained bone
   *   collections [editmode] come last, stay last and are not sorted)
   *   with bones coming first (and ARE sorted).
   */

  TreeElement *last_te = lb->last();
  if (last_te == nullptr) {
    return;
  }
  TreeStoreElem *last_tselem = TREESTORE(last_te);

  /* Check if we are expanding Armature data and if there are bone collections. */
  const TreeElement *first_te = lb->first();
  const TreeStoreElem *first_tselem = TREESTORE(first_te);
  const bool inside_armature_data = ELEM(
      first_tselem->type, TSE_BONE, TSE_EBONE, TSE_POSE_CHANNEL);
  const bool has_armature_data_bone_collections = ELEM(last_tselem->type,
                                                       TSE_BONE_COLLECTION_BASE);

  /* Sorting rules; only object lists, ID lists, bones or deform-groups. */
  if (inside_armature_data || ELEM(last_tselem->type, TSE_DEFGROUP, TSE_ID_BASE) ||
      ((last_tselem->type == TSE_SOME_ID) && (last_te->idcode == ID_OB)))
  {
    int totelem = lb->count();

    if (totelem > 1) {
      tTreeSort *tear = MEM_new_array_uninitialized<tTreeSort>(totelem, "tree sort array");
      tTreeSort *tp = tear;

      for (TreeElement &te : *lb) {
        TreeStoreElem *tselem = TREESTORE(&te);
        tp->te = &te;
        tp->name = te.name;
        tp->idcode = te.idcode;

        if (!ELEM(tselem->type, TSE_SOME_ID, TSE_DEFGROUP, TSE_BONE, TSE_EBONE, TSE_POSE_CHANNEL))
        {
          tp->idcode = 0; /* Don't sort this. */
        }
        if (ELEM(tselem->type, TSE_ID_BASE, TSE_DEFGROUP, TSE_BONE, TSE_EBONE, TSE_POSE_CHANNEL)) {
          tp->idcode = 1; /* Do sort this. */
        }

        tp->id = tselem->id;
        tp++;
      }

      /* Just sort alphabetically (but keep bone collections last when inside armature data). */
      if (tear->idcode == 1) {
        const int skip_back = has_armature_data_bone_collections ? 1 : 0;
        qsort(tear, totelem - skip_back, sizeof(tTreeSort), treesort_alpha);
      }
      else {
        /* keep beginning of list */
        int skip_front = 0;
        for (tp = tear, skip_front = 0; skip_front < totelem; skip_front++, tp++) {
          if (tp->idcode) {
            break;
          }
        }

        if (skip_front < totelem) {
          qsort(tear + skip_front, totelem - skip_front, sizeof(tTreeSort), treesort_alpha_ob);
        }
      }

      lb->clear_no_delete();
      tp = tear;
      while (totelem--) {
        BLI_addtail(lb, tp->te);
        tp++;
      }
      MEM_delete(tear);
    }
  }

  for (TreeElement &te_iter : *lb) {
    outliner_sort(&te_iter.subtree);
  }
}

static void outliner_collections_children_sort(ListBaseT<TreeElement> *lb)
{
  TreeElement *last_te = lb->last();
  if (last_te == nullptr) {
    return;
  }
  TreeStoreElem *last_tselem = TREESTORE(last_te);

  /* Sorting rules: only object lists. */
  if ((last_tselem->type == TSE_SOME_ID) && (last_te->idcode == ID_OB)) {
    int totelem = lb->count();

    if (totelem > 1) {
      tTreeSort *tear = MEM_new_array_uninitialized<tTreeSort>(totelem, "tree sort array");
      tTreeSort *tp = tear;

      for (TreeElement &te : *lb) {
        TreeStoreElem *tselem = TREESTORE(&te);
        tp->te = &te;
        tp->name = te.name;
        tp->idcode = te.idcode;
        tp->id = tselem->id;
        tp++;
      }

      qsort(tear, totelem, sizeof(tTreeSort), treesort_child_not_in_collection);

      lb->clear_no_delete();
      tp = tear;
      while (totelem--) {
        BLI_addtail(lb, tp->te);
        tp++;
      }
      MEM_delete(tear);
    }
  }

  for (TreeElement &te_iter : *lb) {
    outliner_collections_children_sort(&te_iter.subtree);
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

    TreeElement *te_new = outliner_find_tree_element(&space_outliner->runtime->tree,
                                                     focus->tselem);

    if (te_new != nullptr) {
      int ys_new = te_new->ys;
      int ys_old = focus->ys;

      float y_move = std::min(float(ys_new - ys_old), -v2d->cur.ymax);
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
    for (TreeElement &te_iter : te->subtree) {
      TreeElement *te_sub = outliner_find_first_desired_element_at_y_recursive(
          space_outliner, &te_iter, limit, callback_test);
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
  TreeElement *te = outliner_find_item_at_y(
      space_outliner, &space_outliner->runtime->tree, view_co);

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

static eSpaceOutliner_Filter outliner_exclude_filter_get(const SpaceOutliner *space_outliner)
{
  eSpaceOutliner_Filter exclude_filter = space_outliner->filter & ~SO_FILTER_OB_STATE;

  if ((space_outliner->search_string[0] != 0) && ED_outliner_support_searching(space_outliner)) {
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
    case SO_FILTER_OB_ALL:
    case SO_FILTER_OB_HIDDEN:
      break;
  }

  return exclude_filter;
}

static bool outliner_element_visible_get(const Main &bmain,
                                         const Scene *scene,
                                         ViewLayer *view_layer,
                                         TreeElement *te,
                                         const eSpaceOutliner_Filter exclude_filter)
{
  if ((exclude_filter & SO_FILTER_ANY) == 0) {
    return true;
  }

  TreeStoreElem *tselem = TREESTORE(te);
  if ((tselem->type == TSE_SOME_ID) && (te->idcode == ID_OB)) {
    if ((exclude_filter & SO_FILTER_OB_TYPE) == SO_FILTER_OB_TYPE) {
      return false;
    }

    Object *ob = id_cast<Object *>(tselem->id);
    Base *base = static_cast<Base *>(te->directdata);
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
        case OB_GREASE_PENCIL:
          if (exclude_filter & SO_FILTER_NO_OB_GREASE_PENCIL) {
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
        BKE_view_layer_synced_ensure(bmain, scene, view_layer);
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
        BKE_view_layer_synced_ensure(bmain, scene, view_layer);
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
  else if ((te->idcode == ID_MA) && (exclude_filter & SO_FILTER_NO_OB_MATERIAL)) {
    return false;
  }
  else if ((te->idcode == ID_KE) && (exclude_filter & SO_FILTER_NO_OB_SHAPE_KEYS)) {
    return false;
  }
  else if ((TREESTORE(te)->type == TSE_BONE_COLLECTION_BASE) &&
           (exclude_filter & SO_FILTER_NO_ARMATURE_BONE_COLLECTION))
  {
    return false;
  }
  else if ((te->parent != nullptr) && (TREESTORE(te->parent)->type == TSE_SOME_ID) &&
           (te->parent->idcode == ID_OB))
  {
    if (exclude_filter & SO_FILTER_NO_OB_CONTENT) {
      return false;
    }
    const eTreeStoreElemType type = eTreeStoreElemType(TREESTORE(te)->type);
    if (type == TSE_SOME_ID && te->idcode != ID_OB) {
      if (exclude_filter & SO_FILTER_NO_OB_DATA) {
        return false;
      }
    }
    else if ((type == TSE_ANIM_DATA) && (exclude_filter & SO_FILTER_NO_OB_ANIMATION)) {
      return false;
    }
    else if ((type == TSE_CONSTRAINT_BASE) && (exclude_filter & SO_FILTER_NO_OB_CONSTRAINTS)) {
      return false;
    }
    else if ((type == TSE_MODIFIER_BASE) && (exclude_filter & SO_FILTER_NO_OB_MODIFIERS)) {
      return false;
    }
    else if ((type == TSE_DEFGROUP_BASE) && (exclude_filter & SO_FILTER_NO_OB_DEFGROUP)) {
      return false;
    }
    else if ((type == TSE_GPENCIL_EFFECT_BASE) &&
             (exclude_filter & SO_FILTER_NO_GREASE_PENCIL_EFFECTS))
    {
      return false;
    }
    else if ((type == TSE_POSE_BASE) && (exclude_filter & SO_FILTER_NO_POSE_BONES)) {
      return false;
    }
  }

  return true;
}

static bool outliner_filter_has_name(TreeElement *te, const char *name, int flags)
{
  /* Use `fnmatch` for shell-style globing.
   * - Case-insensitive (optionally).
   * - Don't handle escape characters as "special" characters are not expected in names.
   *   Unlike shell input - `\` should be treated like any other character.
   */
  int fn_flag = FNM_NOESCAPE;

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
                                                           ListBaseT<TreeElement> *parent_subtree)
{
  TreeElement *te_next = element->next;

  if (outliner_element_is_collection_or_object(element)) {
    TreeElement *te_prev = nullptr;
    for (TreeElement *te = element->subtree.last(); te; te = te_prev) {
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
                                   const Main &bmain,
                                   const Scene *scene,
                                   ViewLayer *view_layer,
                                   ListBaseT<TreeElement> *lb,
                                   const char *search_string,
                                   const eSpaceOutliner_Filter exclude_filter)
{
  TreeElement *te, *te_next;
  TreeStoreElem *tselem;

  for (te = lb->first(); te; te = te_next) {
    te_next = te->next;
    if (outliner_element_visible_get(bmain, scene, view_layer, te, exclude_filter) == false) {
      /* Don't free the tree, but extract the children from the parent and add to this tree. */
      /* This also needs filtering the subtree prior (see #69246). */
      outliner_filter_subtree(
          space_outliner, bmain, scene, view_layer, &te->subtree, search_string, exclude_filter);
      te_next = outliner_extract_children_from_subtree(te, lb);
      continue;
    }
    if ((exclude_filter & SO_FILTER_SEARCH) == 0) {
      /* Filter subtree too. */
      outliner_filter_subtree(
          space_outliner, bmain, scene, view_layer, &te->subtree, search_string, exclude_filter);
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

      if (!TSELEM_OPEN(tselem, space_outliner) || outliner_filter_subtree(space_outliner,
                                                                          bmain,
                                                                          scene,
                                                                          view_layer,
                                                                          &te->subtree,
                                                                          search_string,
                                                                          exclude_filter) == 0)
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
          space_outliner, bmain, scene, view_layer, &te->subtree, search_string, exclude_filter);
    }
  }

  /* if there are still items in the list, that means that there were still some matches */
  return (lb->is_empty() == false);
}

static void outliner_filter_tree(const Main &bmain,
                                 SpaceOutliner *space_outliner,
                                 const Scene *scene,
                                 ViewLayer *view_layer)
{
  char search_buff[sizeof(SpaceOutliner::search_string) + 2];
  const char *search_string;

  const eSpaceOutliner_Filter exclude_filter = outliner_exclude_filter_get(space_outliner);

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

  outliner_filter_subtree(space_outliner,
                          bmain,
                          scene,
                          view_layer,
                          &space_outliner->runtime->tree,
                          search_string,
                          exclude_filter);
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
                         WorkSpace *workspace,
                         Scene *scene,
                         ViewLayer *view_layer,
                         SpaceOutliner *space_outliner,
                         ARegion *region)
{
  /* Are we looking for something - we want to tag parents to filter child matches
   * - NOT in data-blocks view - searching all data-blocks takes way too long to be useful
   * - this variable is only set once per tree build */
  if (space_outliner->search_string[0] != 0 && space_outliner->outlinevis != SO_DATA_API &&
      ED_outliner_support_searching(space_outliner))
  {
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

  if (region->runtime->do_draw & RGN_DRAW_NO_REBUILD) {
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

  outliner_free_tree(&space_outliner->runtime->tree);
  outliner_storage_cleanup(space_outliner);

  space_outliner->runtime->tree_display = AbstractTreeDisplay::create_from_display_mode(
      space_outliner->outlinevis, *space_outliner);

  /* All tree displays should be created as sub-classes of AbstractTreeDisplay. */
  BLI_assert(space_outliner->runtime->tree_display != nullptr);

  TreeSourceData source_data{*mainvar, *workspace, *scene, *view_layer};
  space_outliner->runtime->tree = ListBaseT<TreeElement>{
      space_outliner->runtime->tree_display->build_tree(source_data)};

  if ((space_outliner->flag & SO_SKIP_SORT_ALPHA) == 0) {
    outliner_sort(&space_outliner->runtime->tree);
  }
  else if ((space_outliner->filter & SO_FILTER_NO_CHILDREN) == 0) {
    /* We group the children that are in the collection before the ones that are not.
     * This way we can try to draw them in a different style altogether.
     * We also have to respect the original order of the elements in case alphabetical
     * sorting is not enabled. This keep object data and modifiers before its children. */
    outliner_collections_children_sort(&space_outliner->runtime->tree);
  }

  outliner_filter_tree(*mainvar, space_outliner, scene, view_layer);
  outliner_restore_scrolling_position(space_outliner, region, &focus);

  /* `ID.newid` pointer is abused when building tree, DO NOT call #BKE_main_id_newptr_and_tag_clear
   * as this expects valid IDs in this pointer, not random unknown data. */
  outliner_clear_newid_from_main(mainvar);
}

/** \} */

}  // namespace blender::ed::outliner
