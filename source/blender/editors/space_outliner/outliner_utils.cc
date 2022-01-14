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

#include <cstring>

#include "BLI_listbase.h"
#include "BLI_utildefines.h"

#include "DNA_action_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "BKE_armature.h"
#include "BKE_context.h"
#include "BKE_layer.h"
#include "BKE_object.h"
#include "BKE_outliner_treehash.h"

#include "ED_outliner.h"
#include "ED_screen.h"

#include "UI_interface.h"
#include "UI_view2d.h"

#include "outliner_intern.hh"

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
  if (tvc->obact != nullptr) {
    tvc->ob_edit = OBEDIT_FROM_OBACT(tvc->obact);

    if ((tvc->obact->type == OB_ARMATURE) ||
        /* This could be made into its own function. */
        ((tvc->obact->type == OB_MESH) && tvc->obact->mode & OB_MODE_WEIGHT_PAINT)) {
      tvc->ob_pose = BKE_object_pose_armature_get(tvc->obact);
    }
  }
}

/** \} */

TreeElement *outliner_find_item_at_y(const SpaceOutliner *space_outliner,
                                     const ListBase *tree,
                                     float view_co_y)
{
  LISTBASE_FOREACH (TreeElement *, te_iter, tree) {
    if (view_co_y < (te_iter->ys + UI_UNIT_Y)) {
      if (view_co_y >= te_iter->ys) {
        /* co_y is inside this element */
        return te_iter;
      }

      if (BLI_listbase_is_empty(&te_iter->subtree) ||
          !TSELEM_OPEN(TREESTORE(te_iter), space_outliner)) {
        /* No need for recursion. */
        continue;
      }

      /* If the coordinate is lower than the next element, we can continue with that one and skip
       * recursion too. */
      const TreeElement *te_next = te_iter->next;
      if (te_next && (view_co_y < (te_next->ys + UI_UNIT_Y))) {
        continue;
      }

      /* co_y is lower than current element (but not lower than the next one), possibly inside
       * children */
      TreeElement *te_sub = outliner_find_item_at_y(space_outliner, &te_iter->subtree, view_co_y);
      if (te_sub) {
        return te_sub;
      }
    }
  }

  return nullptr;
}

static TreeElement *outliner_find_item_at_x_in_row_recursive(const TreeElement *parent_te,
                                                             float view_co_x,
                                                             bool *r_is_merged_icon)
{
  TreeElement *child_te = reinterpret_cast<TreeElement *>(parent_te->subtree.first);

  while (child_te) {
    const bool over_element = (view_co_x > child_te->xs) && (view_co_x < child_te->xend);
    if ((child_te->flag & TE_ICONROW) && over_element) {
      return child_te;
    }
    if ((child_te->flag & TE_ICONROW_MERGED) && over_element) {
      if (r_is_merged_icon) {
        *r_is_merged_icon = true;
      }
      return child_te;
    }

    TreeElement *te = outliner_find_item_at_x_in_row_recursive(
        child_te, view_co_x, r_is_merged_icon);
    if (te != child_te) {
      return te;
    }

    child_te = child_te->next;
  }

  /* return parent if no child is hovered */
  return (TreeElement *)parent_te;
}

TreeElement *outliner_find_item_at_x_in_row(const SpaceOutliner *space_outliner,
                                            TreeElement *parent_te,
                                            float view_co_x,
                                            bool *r_is_merged_icon,
                                            bool *r_is_over_icon)
{
  /* if parent_te is opened, it doesn't show children in row */
  TreeElement *te = parent_te;
  if (!TSELEM_OPEN(TREESTORE(parent_te), space_outliner)) {
    te = outliner_find_item_at_x_in_row_recursive(parent_te, view_co_x, r_is_merged_icon);
  }

  if ((te != parent_te) || outliner_item_is_co_over_icon(parent_te, view_co_x)) {
    *r_is_over_icon = true;
  }

  return te;
}

TreeElement *outliner_find_tree_element(ListBase *lb, const TreeStoreElem *store_elem)
{
  LISTBASE_FOREACH (TreeElement *, te, lb) {
    if (te->store_elem == store_elem) {
      return te;
    }
    TreeElement *tes = outliner_find_tree_element(&te->subtree, store_elem);
    if (tes) {
      return tes;
    }
  }
  return nullptr;
}

TreeElement *outliner_find_parent_element(ListBase *lb,
                                          TreeElement *parent_te,
                                          const TreeElement *child_te)
{
  LISTBASE_FOREACH (TreeElement *, te, lb) {
    if (te == child_te) {
      return parent_te;
    }

    TreeElement *find_te = outliner_find_parent_element(&te->subtree, te, child_te);
    if (find_te) {
      return find_te;
    }
  }
  return nullptr;
}

TreeElement *outliner_find_tse(SpaceOutliner *space_outliner, const TreeStoreElem *tse)
{
  TreeStoreElem *tselem;

  if (tse->id == nullptr) {
    return nullptr;
  }

  /* Check if 'tse' is in tree-store. */
  tselem = BKE_outliner_treehash_lookup_any(
      space_outliner->runtime->treehash, tse->type, tse->nr, tse->id);
  if (tselem) {
    return outliner_find_tree_element(&space_outliner->tree, tselem);
  }

  return nullptr;
}

TreeElement *outliner_find_id(SpaceOutliner *space_outliner, ListBase *lb, const ID *id)
{
  LISTBASE_FOREACH (TreeElement *, te, lb) {
    TreeStoreElem *tselem = TREESTORE(te);
    if (tselem->type == TSE_SOME_ID) {
      if (tselem->id == id) {
        return te;
      }
    }

    TreeElement *tes = outliner_find_id(space_outliner, &te->subtree, id);
    if (tes) {
      return tes;
    }
  }
  return nullptr;
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
  return nullptr;
}

TreeElement *outliner_find_editbone(ListBase *lb, const EditBone *ebone)
{
  LISTBASE_FOREACH (TreeElement *, te, lb) {
    if (te->directdata == ebone) {
      return te;
    }

    TreeStoreElem *tselem = TREESTORE(te);
    if (ELEM(tselem->type, TSE_SOME_ID, TSE_EBONE)) {
      TreeElement *tes = outliner_find_editbone(&te->subtree, ebone);
      if (tes) {
        return tes;
      }
    }
  }
  return nullptr;
}

TreeElement *outliner_search_back_te(TreeElement *te, short idcode)
{
  TreeStoreElem *tselem;
  te = te->parent;

  while (te) {
    tselem = TREESTORE(te);
    if ((tselem->type == TSE_SOME_ID) && (te->idcode == idcode)) {
      return te;
    }
    te = te->parent;
  }
  return nullptr;
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
  return nullptr;
}

bool outliner_tree_traverse(const SpaceOutliner *space_outliner,
                            ListBase *tree,
                            int filter_te_flag,
                            int filter_tselem_flag,
                            TreeTraversalFunc func,
                            void *customdata)
{
  for (TreeElement *te = reinterpret_cast<TreeElement *>(tree->first), *te_next; te;
       te = te_next) {
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
                 space_outliner, &subtree, filter_te_flag, filter_tselem_flag, func, customdata)) {
      return false;
    }
  }

  return true;
}

float outliner_restrict_columns_width(const SpaceOutliner *space_outliner)
{
  int num_columns = 0;

  switch (space_outliner->outlinevis) {
    case SO_DATA_API:
    case SO_SEQUENCE:
    case SO_LIBRARIES:
    case SO_OVERRIDES_LIBRARY:
      return 0.0f;
    case SO_ID_ORPHANS:
      num_columns = 3;
      break;
    case SO_VIEW_LAYER:
      if (space_outliner->show_restrict_flags & SO_RESTRICT_ENABLE) {
        num_columns++;
      }
      if (space_outliner->show_restrict_flags & SO_RESTRICT_HOLDOUT) {
        num_columns++;
      }
      if (space_outliner->show_restrict_flags & SO_RESTRICT_INDIRECT_ONLY) {
        num_columns++;
      }
      ATTR_FALLTHROUGH;
    case SO_SCENES:
      if (space_outliner->show_restrict_flags & SO_RESTRICT_SELECT) {
        num_columns++;
      }
      if (space_outliner->show_restrict_flags & SO_RESTRICT_HIDE) {
        num_columns++;
      }
      if (space_outliner->show_restrict_flags & SO_RESTRICT_VIEWPORT) {
        num_columns++;
      }
      if (space_outliner->show_restrict_flags & SO_RESTRICT_RENDER) {
        num_columns++;
      }
      break;
  }
  return (num_columns * UI_UNIT_X + V2D_SCROLL_WIDTH);
}

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
  return nullptr;
}

bool outliner_is_element_visible(const TreeElement *te)
{
  TreeStoreElem *tselem;

  while (te->parent) {
    tselem = TREESTORE(te->parent);

    if (tselem->flag & TSE_CLOSED) {
      return false;
    }
    te = te->parent;
  }

  return true;
}

bool outliner_item_is_co_over_name_icons(const TreeElement *te, float view_co_x)
{
  /* Special case: count area left of Scene Collection as empty space */
  bool outside_left = (TREESTORE(te)->type == TSE_VIEW_COLLECTION_BASE) ?
                          (view_co_x > te->xs + UI_UNIT_X) :
                          (view_co_x > te->xs);

  return outside_left && (view_co_x < te->xend);
}

bool outliner_item_is_co_over_icon(const TreeElement *te, float view_co_x)
{
  return (view_co_x > (te->xs + UI_UNIT_X)) && (view_co_x < (te->xs + UI_UNIT_X * 2));
}

bool outliner_item_is_co_over_name(const TreeElement *te, float view_co_x)
{
  return (view_co_x > (te->xs + UI_UNIT_X * 2)) && (view_co_x < te->xend);
}

bool outliner_item_is_co_within_close_toggle(const TreeElement *te, float view_co_x)
{
  return (view_co_x > te->xs) && (view_co_x < te->xs + UI_UNIT_X);
}

void outliner_scroll_view(SpaceOutliner *space_outliner, ARegion *region, int delta_y)
{
  int tree_width, tree_height;
  outliner_tree_dimensions(space_outliner, &tree_width, &tree_height);
  int y_min = MIN2(region->v2d.cur.ymin, -tree_height);

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

void outliner_tag_redraw_avoid_rebuild_on_open_change(const SpaceOutliner *space_outliner,
                                                      ARegion *region)
{
  /* Avoid rebuild if possible. */
  if (outliner_requires_rebuild_on_open_change(space_outliner)) {
    ED_region_tag_redraw(region);
  }
  else {
    ED_region_tag_redraw_no_rebuild(region);
  }
}

Base *ED_outliner_give_base_under_cursor(bContext *C, const int mval[2])
{
  ARegion *region = CTX_wm_region(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  SpaceOutliner *space_outliner = CTX_wm_space_outliner(C);
  TreeElement *te;
  Base *base = nullptr;
  float view_mval[2];

  UI_view2d_region_to_view(&region->v2d, mval[0], mval[1], &view_mval[0], &view_mval[1]);

  te = outliner_find_item_at_y(space_outliner, &space_outliner->tree, view_mval[1]);
  if (te) {
    TreeStoreElem *tselem = TREESTORE(te);
    if ((tselem->type == TSE_SOME_ID) && (te->idcode == ID_OB)) {
      Object *ob = (Object *)tselem->id;
      base = (te->directdata) ? (Base *)te->directdata : BKE_view_layer_base_find(view_layer, ob);
    }
  }

  return base;
}
