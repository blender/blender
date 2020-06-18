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
 * The Original Code is Copyright (C) 2017 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup spoutliner
 */

#include <string.h>

#include "BLI_listbase.h"
#include "BLI_utildefines.h"

#include "DNA_action_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "BKE_context.h"
#include "BKE_layer.h"
#include "BKE_object.h"
#include "BKE_outliner_treehash.h"

#include "ED_armature.h"
#include "ED_outliner.h"

#include "UI_interface.h"
#include "UI_view2d.h"

#include "outliner_intern.h"

/* -------------------------------------------------------------------- */
/** \name Tree View Context
 * \{ */

void outliner_viewcontext_init(const bContext *C, TreeViewContext *tvc)
{
  memset(tvc, 0, sizeof(*tvc));

  /* Scene level. */
  tvc->scene = CTX_data_scene(C);
  tvc->view_layer = CTX_data_view_layer(C);

  /* Objects. */
  tvc->obact = OBACT(tvc->view_layer);
  if (tvc->obact != NULL) {
    tvc->ob_edit = OBEDIT_FROM_OBACT(tvc->obact);

    if ((tvc->obact->type == OB_ARMATURE) ||
        /* This could be made into it's own function. */
        ((tvc->obact->type == OB_MESH) && tvc->obact->mode & OB_MODE_WEIGHT_PAINT)) {
      tvc->ob_pose = BKE_object_pose_armature_get(tvc->obact);
    }
  }
}

/** \} */

/**
 * Try to find an item under y-coordinate \a view_co_y (view-space).
 * \note Recursive
 */
TreeElement *outliner_find_item_at_y(const SpaceOutliner *soops,
                                     const ListBase *tree,
                                     float view_co_y)
{
  LISTBASE_FOREACH (TreeElement *, te_iter, tree) {
    if (view_co_y < (te_iter->ys + UI_UNIT_Y)) {
      if (view_co_y >= te_iter->ys) {
        /* co_y is inside this element */
        return te_iter;
      }
      else if (TSELEM_OPEN(te_iter->store_elem, soops)) {
        /* co_y is lower than current element, possibly inside children */
        TreeElement *te_sub = outliner_find_item_at_y(soops, &te_iter->subtree, view_co_y);
        if (te_sub) {
          return te_sub;
        }
      }
    }
  }

  return NULL;
}

static TreeElement *outliner_find_item_at_x_in_row_recursive(const TreeElement *parent_te,
                                                             float view_co_x,
                                                             bool *r_merged)
{
  TreeElement *child_te = parent_te->subtree.first;

  bool over_element = false;

  while (child_te) {
    over_element = (view_co_x > child_te->xs) && (view_co_x < child_te->xend);
    if ((child_te->flag & TE_ICONROW) && over_element) {
      return child_te;
    }
    else if ((child_te->flag & TE_ICONROW_MERGED) && over_element) {
      if (r_merged) {
        *r_merged = true;
      }
      return child_te;
    }

    TreeElement *te = outliner_find_item_at_x_in_row_recursive(child_te, view_co_x, r_merged);
    if (te != child_te) {
      return te;
    }

    child_te = child_te->next;
  }

  /* return parent if no child is hovered */
  return (TreeElement *)parent_te;
}

/**
 * Collapsed items can show their children as click-able icons. This function tries to find
 * such an icon that represents the child item at x-coordinate \a view_co_x (view-space).
 *
 * \return a hovered child item or \a parent_te (if no hovered child found).
 */
TreeElement *outliner_find_item_at_x_in_row(const SpaceOutliner *soops,
                                            const TreeElement *parent_te,
                                            float view_co_x,
                                            bool *r_merged)
{
  /* if parent_te is opened, it doesn't show children in row */
  if (!TSELEM_OPEN(TREESTORE(parent_te), soops)) {
    return outliner_find_item_at_x_in_row_recursive(parent_te, view_co_x, r_merged);
  }

  return (TreeElement *)parent_te;
}

/* Find specific item from the treestore */
TreeElement *outliner_find_tree_element(ListBase *lb, const TreeStoreElem *store_elem)
{
  TreeElement *te, *tes;
  for (te = lb->first; te; te = te->next) {
    if (te->store_elem == store_elem) {
      return te;
    }
    tes = outliner_find_tree_element(&te->subtree, store_elem);
    if (tes) {
      return tes;
    }
  }
  return NULL;
}

/* Find parent element of te */
TreeElement *outliner_find_parent_element(ListBase *lb,
                                          TreeElement *parent_te,
                                          const TreeElement *child_te)
{
  TreeElement *te;
  for (te = lb->first; te; te = te->next) {
    if (te == child_te) {
      return parent_te;
    }

    TreeElement *find_te = outliner_find_parent_element(&te->subtree, te, child_te);
    if (find_te) {
      return find_te;
    }
  }
  return NULL;
}

/* tse is not in the treestore, we use its contents to find a match */
TreeElement *outliner_find_tse(SpaceOutliner *soops, const TreeStoreElem *tse)
{
  TreeStoreElem *tselem;

  if (tse->id == NULL) {
    return NULL;
  }

  /* check if 'tse' is in treestore */
  tselem = BKE_outliner_treehash_lookup_any(soops->treehash, tse->type, tse->nr, tse->id);
  if (tselem) {
    return outliner_find_tree_element(&soops->tree, tselem);
  }

  return NULL;
}

/* Find treestore that refers to given ID */
TreeElement *outliner_find_id(SpaceOutliner *soops, ListBase *lb, const ID *id)
{
  LISTBASE_FOREACH (TreeElement *, te, lb) {
    TreeStoreElem *tselem = TREESTORE(te);
    if (tselem->type == 0) {
      if (tselem->id == id) {
        return te;
      }
    }

    TreeElement *tes = outliner_find_id(soops, &te->subtree, id);
    if (tes) {
      return tes;
    }
  }
  return NULL;
}

TreeElement *outliner_find_posechannel(ListBase *lb, const bPoseChannel *pchan)
{
  LISTBASE_FOREACH (TreeElement *, te, lb) {
    if (te->directdata == pchan) {
      return te;
    }

    TreeStoreElem *tselem = TREESTORE(te);
    if (ELEM(tselem->type, TSE_POSE_BASE, TSE_POSE_CHANNEL)) {
      TreeElement *tes = outliner_find_posechannel(&te->subtree, pchan);
      if (tes) {
        return tes;
      }
    }
  }
  return NULL;
}

TreeElement *outliner_find_editbone(ListBase *lb, const EditBone *ebone)
{
  LISTBASE_FOREACH (TreeElement *, te, lb) {
    if (te->directdata == ebone) {
      return te;
    }

    TreeStoreElem *tselem = TREESTORE(te);
    if (ELEM(tselem->type, 0, TSE_EBONE)) {
      TreeElement *tes = outliner_find_editbone(&te->subtree, ebone);
      if (tes) {
        return tes;
      }
    }
  }
  return NULL;
}

TreeElement *outliner_search_back_te(TreeElement *te, short idcode)
{
  TreeStoreElem *tselem;
  te = te->parent;

  while (te) {
    tselem = TREESTORE(te);
    if (tselem->type == 0 && te->idcode == idcode) {
      return te;
    }
    te = te->parent;
  }
  return NULL;
}

ID *outliner_search_back(TreeElement *te, short idcode)
{
  TreeElement *search_te;
  TreeStoreElem *tselem;

  search_te = outliner_search_back_te(te, idcode);
  if (search_te) {
    tselem = TREESTORE(search_te);
    return tselem->id;
  }
  return NULL;
}

/**
 * Iterate over all tree elements (pre-order traversal), executing \a func callback for
 * each tree element matching the optional filters.
 *
 * \param filter_te_flag: If not 0, only TreeElements with this flag will be visited.
 * \param filter_tselem_flag: Same as \a filter_te_flag, but for the TreeStoreElem.
 * \param func: Custom callback to execute for each visited item.
 */
bool outliner_tree_traverse(const SpaceOutliner *soops,
                            ListBase *tree,
                            int filter_te_flag,
                            int filter_tselem_flag,
                            TreeTraversalFunc func,
                            void *customdata)
{
  for (TreeElement *te = tree->first, *te_next; te; te = te_next) {
    TreeTraversalAction func_retval = TRAVERSE_CONTINUE;
    /* in case te is freed in callback */
    TreeStoreElem *tselem = TREESTORE(te);
    ListBase subtree = te->subtree;
    te_next = te->next;

    if (filter_te_flag && (te->flag & filter_te_flag) == 0) {
      /* skip */
    }
    else if (filter_tselem_flag && (tselem->flag & filter_tselem_flag) == 0) {
      /* skip */
    }
    else {
      func_retval = func(te, customdata);
    }
    /* Don't access te or tselem from now on! Might've been freed... */

    if (func_retval == TRAVERSE_BREAK) {
      return false;
    }

    if (func_retval == TRAVERSE_SKIP_CHILDS) {
      /* skip */
    }
    else if (!outliner_tree_traverse(
                 soops, &subtree, filter_te_flag, filter_tselem_flag, func, customdata)) {
      return false;
    }
  }

  return true;
}

float outliner_restrict_columns_width(const SpaceOutliner *soops)
{
  int num_columns = 0;

  switch (soops->outlinevis) {
    case SO_DATA_API:
    case SO_SEQUENCE:
    case SO_LIBRARIES:
      return 0.0f;
    case SO_ID_ORPHANS:
      num_columns = 3;
      break;
    case SO_VIEW_LAYER:
      if (soops->show_restrict_flags & SO_RESTRICT_HOLDOUT) {
        num_columns++;
      }
      if (soops->show_restrict_flags & SO_RESTRICT_INDIRECT_ONLY) {
        num_columns++;
      }
      ATTR_FALLTHROUGH;
    case SO_SCENES:
      if (soops->show_restrict_flags & SO_RESTRICT_SELECT) {
        num_columns++;
      }
      if (soops->show_restrict_flags & SO_RESTRICT_HIDE) {
        num_columns++;
      }
      if (soops->show_restrict_flags & SO_RESTRICT_VIEWPORT) {
        num_columns++;
      }
      if (soops->show_restrict_flags & SO_RESTRICT_RENDER) {
        num_columns++;
      }
      break;
  }
  return (num_columns * UI_UNIT_X + V2D_SCROLL_WIDTH);
}

/* Find first tree element in tree with matching treestore flag */
TreeElement *outliner_find_element_with_flag(const ListBase *lb, short flag)
{
  LISTBASE_FOREACH (TreeElement *, te, lb) {
    if ((TREESTORE(te)->flag & flag) == flag) {
      return te;
    }
    TreeElement *active_element = outliner_find_element_with_flag(&te->subtree, flag);
    if (active_element) {
      return active_element;
    }
  }
  return NULL;
}

/* Find if element is visible in the outliner tree */
bool outliner_is_element_visible(const TreeElement *te)
{
  TreeStoreElem *tselem;

  while (te->parent) {
    tselem = TREESTORE(te->parent);

    if (tselem->flag & TSE_CLOSED) {
      return false;
    }
    else {
      te = te->parent;
    }
  }

  return true;
}

/* Find if x coordinate is over an icon or name */
bool outliner_item_is_co_over_name_icons(const TreeElement *te, float view_co_x)
{
  /* Special case: count area left of Scene Collection as empty space */
  bool outside_left = (TREESTORE(te)->type == TSE_VIEW_COLLECTION_BASE) ?
                          (view_co_x > te->xs + UI_UNIT_X) :
                          (view_co_x > te->xs);

  return outside_left && (view_co_x < te->xend);
}

/* Find if x coordinate is over element disclosure toggle */
bool outliner_item_is_co_within_close_toggle(const TreeElement *te, float view_co_x)
{
  return (view_co_x > te->xs) && (view_co_x < te->xs + UI_UNIT_X);
}

/* Scroll view vertically while keeping within total bounds */
void outliner_scroll_view(ARegion *region, int delta_y)
{
  int y_min = MIN2(region->v2d.cur.ymin, region->v2d.tot.ymin);

  region->v2d.cur.ymax += delta_y;
  region->v2d.cur.ymin += delta_y;

  /* Adjust view if delta placed view outside total area */
  int offset;
  if (region->v2d.cur.ymax > -UI_UNIT_Y) {
    offset = region->v2d.cur.ymax;
    region->v2d.cur.ymax -= offset;
    region->v2d.cur.ymin -= offset;
  }
  else if (region->v2d.cur.ymin < y_min) {
    offset = y_min - region->v2d.cur.ymin;
    region->v2d.cur.ymax += offset;
    region->v2d.cur.ymin += offset;
  }
}

/* Get base of object under cursor. Used for eyedropper tool */
Base *ED_outliner_give_base_under_cursor(bContext *C, const int mval[2])
{
  ARegion *region = CTX_wm_region(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  SpaceOutliner *soops = CTX_wm_space_outliner(C);
  TreeElement *te;
  Base *base = NULL;
  float view_mval[2];

  UI_view2d_region_to_view(&region->v2d, mval[0], mval[1], &view_mval[0], &view_mval[1]);

  te = outliner_find_item_at_y(soops, &soops->tree, view_mval[1]);
  if (te) {
    TreeStoreElem *tselem = TREESTORE(te);
    if (tselem->type == 0) {
      Object *ob = (Object *)tselem->id;
      base = (te->directdata) ? (Base *)te->directdata : BKE_view_layer_base_find(view_layer, ob);
    }
  }

  return base;
}
