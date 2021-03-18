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
 * The Original Code is Copyright (C) 2004 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup spoutliner
 */

#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_collection_types.h"
#include "DNA_material_types.h"
#include "DNA_object_types.h"
#include "DNA_space_types.h"

#include "BLI_listbase.h"

#include "BLT_translation.h"

#include "BKE_collection.h"
#include "BKE_context.h"
#include "BKE_layer.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_object.h"
#include "BKE_report.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"

#include "ED_object.h"
#include "ED_outliner.h"
#include "ED_screen.h"

#include "UI_interface.h"
#include "UI_view2d.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"

#include "outliner_intern.h"

static Collection *collection_parent_from_ID(ID *id);

/* ******************** Drop Target Find *********************** */

static TreeElement *outliner_dropzone_element(TreeElement *te,
                                              const float fmval[2],
                                              const bool children)
{
  if ((fmval[1] > te->ys) && (fmval[1] < (te->ys + UI_UNIT_Y))) {
    /* name and first icon */
    if ((fmval[0] > te->xs + UI_UNIT_X) && (fmval[0] < te->xend)) {
      return te;
    }
  }
  /* Not it.  Let's look at its children. */
  if (children && (TREESTORE(te)->flag & TSE_CLOSED) == 0 && (te->subtree.first)) {
    LISTBASE_FOREACH (TreeElement *, te_sub, &te->subtree) {
      TreeElement *te_valid = outliner_dropzone_element(te_sub, fmval, children);
      if (te_valid) {
        return te_valid;
      }
    }
  }
  return NULL;
}

/* Find tree element to drop into. */
static TreeElement *outliner_dropzone_find(const SpaceOutliner *space_outliner,
                                           const float fmval[2],
                                           const bool children)
{
  LISTBASE_FOREACH (TreeElement *, te, &space_outliner->tree) {
    TreeElement *te_valid = outliner_dropzone_element(te, fmval, children);
    if (te_valid) {
      return te_valid;
    }
  }
  return NULL;
}

static TreeElement *outliner_drop_find(bContext *C, const wmEvent *event)
{
  ARegion *region = CTX_wm_region(C);
  SpaceOutliner *space_outliner = CTX_wm_space_outliner(C);
  float fmval[2];
  UI_view2d_region_to_view(&region->v2d, event->mval[0], event->mval[1], &fmval[0], &fmval[1]);

  return outliner_dropzone_find(space_outliner, fmval, true);
}

static ID *outliner_ID_drop_find(bContext *C, const wmEvent *event, short idcode)
{
  TreeElement *te = outliner_drop_find(C, event);
  TreeStoreElem *tselem = (te) ? TREESTORE(te) : NULL;

  if (te && (te->idcode == idcode) && (tselem->type == TSE_SOME_ID)) {
    return tselem->id;
  }
  return NULL;
}

/* Find tree element to drop into, with additional before and after reorder support. */
static TreeElement *outliner_drop_insert_find(bContext *C,
                                              const wmEvent *event,
                                              TreeElementInsertType *r_insert_type)
{
  SpaceOutliner *space_outliner = CTX_wm_space_outliner(C);
  ARegion *region = CTX_wm_region(C);
  TreeElement *te_hovered;
  float view_mval[2];

  /* Empty tree, e.g. while filtered. */
  if (BLI_listbase_is_empty(&space_outliner->tree)) {
    return NULL;
  }

  UI_view2d_region_to_view(
      &region->v2d, event->mval[0], event->mval[1], &view_mval[0], &view_mval[1]);
  te_hovered = outliner_find_item_at_y(space_outliner, &space_outliner->tree, view_mval[1]);

  if (te_hovered) {
    /* Mouse hovers an element (ignoring x-axis),
     * now find out how to insert the dragged item exactly. */
    const float margin = UI_UNIT_Y * (1.0f / 4);

    if (view_mval[1] < (te_hovered->ys + margin)) {
      if (TSELEM_OPEN(TREESTORE(te_hovered), space_outliner) &&
          !BLI_listbase_is_empty(&te_hovered->subtree)) {
        /* inserting after a open item means we insert into it, but as first child */
        if (BLI_listbase_is_empty(&te_hovered->subtree)) {
          *r_insert_type = TE_INSERT_INTO;
          return te_hovered;
        }
        *r_insert_type = TE_INSERT_BEFORE;
        return te_hovered->subtree.first;
      }
      *r_insert_type = TE_INSERT_AFTER;
      return te_hovered;
    }
    if (view_mval[1] > (te_hovered->ys + (3 * margin))) {
      *r_insert_type = TE_INSERT_BEFORE;
      return te_hovered;
    }
    *r_insert_type = TE_INSERT_INTO;
    return te_hovered;
  }

  /* Mouse doesn't hover any item (ignoring x-axis),
   * so it's either above list bounds or below. */
  TreeElement *first = space_outliner->tree.first;
  TreeElement *last = space_outliner->tree.last;

  if (view_mval[1] < last->ys) {
    *r_insert_type = TE_INSERT_AFTER;
    return last;
  }
  if (view_mval[1] > (first->ys + UI_UNIT_Y)) {
    *r_insert_type = TE_INSERT_BEFORE;
    return first;
  }
  BLI_assert(0);
  return NULL;
}

typedef bool (*CheckTypeFn)(TreeElement *te);

static TreeElement *outliner_data_from_tree_element_and_parents(CheckTypeFn check_type,
                                                                TreeElement *te)
{
  while (te != NULL) {
    if (check_type(te)) {
      return te;
    }
    te = te->parent;
  }
  return NULL;
}

static bool is_collection_element(TreeElement *te)
{
  return outliner_is_collection_tree_element(te);
}

static bool is_object_element(TreeElement *te)
{
  TreeStoreElem *tselem = TREESTORE(te);
  return (tselem->type == TSE_SOME_ID) && te->idcode == ID_OB;
}

static bool is_pchan_element(TreeElement *te)
{
  TreeStoreElem *tselem = TREESTORE(te);
  return tselem->type == TSE_POSE_CHANNEL;
}

static TreeElement *outliner_drop_insert_collection_find(bContext *C,
                                                         const wmEvent *event,
                                                         TreeElementInsertType *r_insert_type)
{
  TreeElement *te = outliner_drop_insert_find(C, event, r_insert_type);
  if (!te) {
    return NULL;
  }

  TreeElement *collection_te = outliner_data_from_tree_element_and_parents(is_collection_element,
                                                                           te);
  if (!collection_te) {
    return NULL;
  }
  Collection *collection = outliner_collection_from_tree_element(collection_te);

  if (collection_te != te) {
    *r_insert_type = TE_INSERT_INTO;
  }

  /* We can't insert before/after master collection. */
  if (collection->flag & COLLECTION_IS_MASTER) {
    *r_insert_type = TE_INSERT_INTO;
  }

  return collection_te;
}

static int outliner_get_insert_index(TreeElement *drag_te,
                                     TreeElement *drop_te,
                                     TreeElementInsertType insert_type,
                                     ListBase *listbase)
{
  /* Find the element to insert after. NULL is the start of the list. */
  if (drag_te->index < drop_te->index) {
    if (insert_type == TE_INSERT_BEFORE) {
      drop_te = drop_te->prev;
    }
  }
  else {
    if (insert_type == TE_INSERT_AFTER) {
      drop_te = drop_te->next;
    }
  }

  if (drop_te == NULL) {
    return 0;
  }

  return BLI_findindex(listbase, drop_te->directdata);
}

/* ******************** Parent Drop Operator *********************** */

static bool parent_drop_allowed(TreeElement *te, Object *potential_child)
{
  TreeStoreElem *tselem = TREESTORE(te);
  if ((te->idcode != ID_OB) || (tselem->type != TSE_SOME_ID)) {
    return false;
  }

  Object *potential_parent = (Object *)tselem->id;

  if (potential_parent == potential_child) {
    return false;
  }
  if (BKE_object_is_child_recursive(potential_child, potential_parent)) {
    return false;
  }
  if (potential_parent == potential_child->parent) {
    return false;
  }

  /* check that parent/child are both in the same scene */
  Scene *scene = (Scene *)outliner_search_back(te, ID_SCE);

  /* currently outliner organized in a way that if there's no parent scene
   * element for object it means that all displayed objects belong to
   * active scene and parenting them is allowed (sergey) */
  if (scene) {
    LISTBASE_FOREACH (ViewLayer *, view_layer, &scene->view_layers) {
      if (BKE_view_layer_base_find(view_layer, potential_child)) {
        return true;
      }
    }
    return false;
  }
  return true;
}

static bool allow_parenting_without_modifier_key(SpaceOutliner *space_outliner)
{
  switch (space_outliner->outlinevis) {
    case SO_VIEW_LAYER:
      return space_outliner->filter & SO_FILTER_NO_COLLECTION;
    case SO_SCENES:
      return true;
    default:
      return false;
  }
}

static bool parent_drop_poll(bContext *C,
                             wmDrag *drag,
                             const wmEvent *event,
                             const char **UNUSED(r_tooltip))
{
  SpaceOutliner *space_outliner = CTX_wm_space_outliner(C);

  bool changed = outliner_flag_set(&space_outliner->tree, TSE_DRAG_ANY, false);
  if (changed) {
    ED_region_tag_redraw_no_rebuild(CTX_wm_region(C));
  }

  Object *potential_child = (Object *)WM_drag_get_local_ID(drag, ID_OB);
  if (!potential_child) {
    return false;
  }

  if (!allow_parenting_without_modifier_key(space_outliner)) {
    if (!event->shift) {
      return false;
    }
  }

  TreeElement *te = outliner_drop_find(C, event);
  if (!te) {
    return false;
  }

  if (parent_drop_allowed(te, potential_child)) {
    TREESTORE(te)->flag |= TSE_DRAG_INTO;
    ED_region_tag_redraw_no_rebuild(CTX_wm_region(C));
    return true;
  }

  return false;
}

static void parent_drop_set_parents(bContext *C,
                                    ReportList *reports,
                                    wmDragID *drag,
                                    Object *parent,
                                    short parent_type,
                                    const bool keep_transform)
{
  Main *bmain = CTX_data_main(C);
  SpaceOutliner *space_outliner = CTX_wm_space_outliner(C);

  TreeElement *te = outliner_find_id(space_outliner, &space_outliner->tree, &parent->id);
  Scene *scene = (Scene *)outliner_search_back(te, ID_SCE);

  if (scene == NULL) {
    /* currently outliner organized in a way, that if there's no parent scene
     * element for object it means that all displayed objects belong to
     * active scene and parenting them is allowed (sergey)
     */

    scene = CTX_data_scene(C);
  }

  bool parent_set = false;
  bool linked_objects = false;

  for (wmDragID *drag_id = drag; drag_id; drag_id = drag_id->next) {
    if (GS(drag_id->id->name) == ID_OB) {
      Object *object = (Object *)drag_id->id;

      /* Do nothing to linked data */
      if (ID_IS_LINKED(object)) {
        linked_objects = true;
        continue;
      }

      if (ED_object_parent_set(
              reports, C, scene, object, parent, parent_type, false, keep_transform, NULL)) {
        parent_set = true;
      }
    }
  }

  if (linked_objects) {
    BKE_report(reports, RPT_INFO, "Can't edit library linked object(s)");
  }

  if (parent_set) {
    DEG_relations_tag_update(bmain);
    WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, NULL);
    WM_event_add_notifier(C, NC_OBJECT | ND_PARENT, NULL);
  }
}

static int parent_drop_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  TreeElement *te = outliner_drop_find(C, event);
  TreeStoreElem *tselem = te ? TREESTORE(te) : NULL;

  if (!(te && (te->idcode == ID_OB) && (tselem->type == TSE_SOME_ID))) {
    return OPERATOR_CANCELLED;
  }

  Object *par = (Object *)tselem->id;
  Object *ob = (Object *)WM_drag_get_local_ID_from_event(event, ID_OB);

  if (ELEM(NULL, ob, par)) {
    return OPERATOR_CANCELLED;
  }
  if (ob == par) {
    return OPERATOR_CANCELLED;
  }

  if (event->custom != EVT_DATA_DRAGDROP) {
    return OPERATOR_CANCELLED;
  }

  ListBase *lb = event->customdata;
  wmDrag *drag = lb->first;

  parent_drop_set_parents(C, op->reports, drag->ids.first, par, PAR_OBJECT, event->alt);

  return OPERATOR_FINISHED;
}

void OUTLINER_OT_parent_drop(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Drop to Set Parent (hold Alt to keep transforms)";
  ot->description = "Drag to parent in Outliner";
  ot->idname = "OUTLINER_OT_parent_drop";

  /* api callbacks */
  ot->invoke = parent_drop_invoke;

  ot->poll = ED_operator_outliner_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
}

/* ******************** Parent Clear Operator *********************** */

static bool parent_clear_poll(bContext *C,
                              wmDrag *drag,
                              const wmEvent *event,
                              const char **UNUSED(r_tooltip))
{
  SpaceOutliner *space_outliner = CTX_wm_space_outliner(C);

  if (!allow_parenting_without_modifier_key(space_outliner)) {
    if (!event->shift) {
      return false;
    }
  }

  Object *ob = (Object *)WM_drag_get_local_ID(drag, ID_OB);
  if (!ob) {
    return false;
  }
  if (!ob->parent) {
    return false;
  }

  TreeElement *te = outliner_drop_find(C, event);
  if (te) {
    TreeStoreElem *tselem = TREESTORE(te);
    ID *id = tselem->id;
    if (!id) {
      return true;
    }

    switch (GS(id->name)) {
      case ID_OB:
        return ELEM(tselem->type, TSE_MODIFIER_BASE, TSE_CONSTRAINT_BASE);
      case ID_GR:
        return event->shift || ELEM(tselem->type, TSE_LIBRARY_OVERRIDE_BASE);
      default:
        return true;
    }
  }
  else {
    return true;
  }
}

static int parent_clear_invoke(bContext *C, wmOperator *UNUSED(op), const wmEvent *event)
{
  Main *bmain = CTX_data_main(C);

  if (event->custom != EVT_DATA_DRAGDROP) {
    return OPERATOR_CANCELLED;
  }

  ListBase *lb = event->customdata;
  wmDrag *drag = lb->first;

  LISTBASE_FOREACH (wmDragID *, drag_id, &drag->ids) {
    if (GS(drag_id->id->name) == ID_OB) {
      Object *object = (Object *)drag_id->id;

      ED_object_parent_clear(object, event->alt ? CLEAR_PARENT_KEEP_TRANSFORM : CLEAR_PARENT_ALL);
    }
  }

  DEG_relations_tag_update(bmain);
  WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, NULL);
  WM_event_add_notifier(C, NC_OBJECT | ND_PARENT, NULL);
  return OPERATOR_FINISHED;
}

void OUTLINER_OT_parent_clear(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Drop to Clear Parent (hold Alt to keep transforms)";
  ot->description = "Drag to clear parent in Outliner";
  ot->idname = "OUTLINER_OT_parent_clear";

  /* api callbacks */
  ot->invoke = parent_clear_invoke;

  ot->poll = ED_operator_outliner_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
}

/* ******************** Scene Drop Operator *********************** */

static bool scene_drop_poll(bContext *C,
                            wmDrag *drag,
                            const wmEvent *event,
                            const char **UNUSED(r_tooltip))
{
  /* Ensure item under cursor is valid drop target */
  Object *ob = (Object *)WM_drag_get_local_ID(drag, ID_OB);
  return (ob && (outliner_ID_drop_find(C, event, ID_SCE) != NULL));
}

static int scene_drop_invoke(bContext *C, wmOperator *UNUSED(op), const wmEvent *event)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = (Scene *)outliner_ID_drop_find(C, event, ID_SCE);
  Object *ob = (Object *)WM_drag_get_local_ID_from_event(event, ID_OB);

  if (ELEM(NULL, ob, scene) || ID_IS_LINKED(scene)) {
    return OPERATOR_CANCELLED;
  }

  if (BKE_scene_has_object(scene, ob)) {
    return OPERATOR_CANCELLED;
  }

  Collection *collection;
  if (scene != CTX_data_scene(C)) {
    /* when linking to an inactive scene link to the master collection */
    collection = scene->master_collection;
  }
  else {
    collection = CTX_data_collection(C);
  }

  BKE_collection_object_add(bmain, collection, ob);

  LISTBASE_FOREACH (ViewLayer *, view_layer, &scene->view_layers) {
    Base *base = BKE_view_layer_base_find(view_layer, ob);
    if (base) {
      ED_object_base_select(base, BA_SELECT);
    }
  }

  DEG_relations_tag_update(bmain);

  DEG_id_tag_update(&scene->id, ID_RECALC_SELECT);
  WM_main_add_notifier(NC_SCENE | ND_OB_SELECT, scene);

  return OPERATOR_FINISHED;
}

void OUTLINER_OT_scene_drop(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Drop Object to Scene";
  ot->description = "Drag object to scene in Outliner";
  ot->idname = "OUTLINER_OT_scene_drop";

  /* api callbacks */
  ot->invoke = scene_drop_invoke;

  ot->poll = ED_operator_outliner_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
}

/* ******************** Material Drop Operator *********************** */

static bool material_drop_poll(bContext *C,
                               wmDrag *drag,
                               const wmEvent *event,
                               const char **UNUSED(r_tooltip))
{
  /* Ensure item under cursor is valid drop target */
  Material *ma = (Material *)WM_drag_get_local_ID(drag, ID_MA);
  return (ma && (outliner_ID_drop_find(C, event, ID_OB) != NULL));
}

static int material_drop_invoke(bContext *C, wmOperator *UNUSED(op), const wmEvent *event)
{
  Main *bmain = CTX_data_main(C);
  Object *ob = (Object *)outliner_ID_drop_find(C, event, ID_OB);
  Material *ma = (Material *)WM_drag_get_local_ID_from_event(event, ID_MA);

  if (ELEM(NULL, ob, ma)) {
    return OPERATOR_CANCELLED;
  }

  /* only drop grease pencil material on grease pencil objects */
  if ((ma->gp_style != NULL) && (ob->type != OB_GPENCIL)) {
    return OPERATOR_CANCELLED;
  }

  BKE_object_material_assign(bmain, ob, ma, ob->totcol + 1, BKE_MAT_ASSIGN_USERPREF);

  WM_event_add_notifier(C, NC_OBJECT | ND_OB_SHADING, ob);
  WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, NULL);
  WM_event_add_notifier(C, NC_MATERIAL | ND_SHADING_LINKS, ma);

  return OPERATOR_FINISHED;
}

void OUTLINER_OT_material_drop(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Drop Material on Object";
  ot->description = "Drag material to object in Outliner";
  ot->idname = "OUTLINER_OT_material_drop";

  /* api callbacks */
  ot->invoke = material_drop_invoke;

  ot->poll = ED_operator_outliner_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
}

/* ******************** Data Stack Drop Operator *********************** */

/* A generic operator to allow drag and drop for modifiers, constraints,
 * and shader effects which all share the same UI stack layout.
 *
 * The following operations are allowed:
 * - Reordering within an object.
 * - Copying a single modifier/constraint/effect to another object.
 * - Copying (linking) an object's modifiers/constraints/effects to another. */

typedef enum eDataStackDropAction {
  DATA_STACK_DROP_REORDER,
  DATA_STACK_DROP_COPY,
  DATA_STACK_DROP_LINK,
} eDataStackDropAction;

typedef struct StackDropData {
  Object *ob_parent;
  bPoseChannel *pchan_parent;
  TreeStoreElem *drag_tselem;
  void *drag_directdata;
  int drag_index;

  eDataStackDropAction drop_action;
  TreeElement *drop_te;
  TreeElementInsertType insert_type;
} StackDropData;

static void datastack_drop_data_init(wmDrag *drag,
                                     Object *ob,
                                     bPoseChannel *pchan,
                                     TreeElement *te,
                                     TreeStoreElem *tselem,
                                     void *directdata)
{
  StackDropData *drop_data = MEM_callocN(sizeof(*drop_data), "datastack drop data");

  drop_data->ob_parent = ob;
  drop_data->pchan_parent = pchan;
  drop_data->drag_tselem = tselem;
  drop_data->drag_directdata = directdata;
  drop_data->drag_index = te->index;

  drag->poin = drop_data;
  drag->flags |= WM_DRAG_FREE_DATA;
}

static bool datastack_drop_init(bContext *C, const wmEvent *event, StackDropData *drop_data)
{
  if (!ELEM(drop_data->drag_tselem->type,
            TSE_MODIFIER,
            TSE_MODIFIER_BASE,
            TSE_CONSTRAINT,
            TSE_CONSTRAINT_BASE,
            TSE_GPENCIL_EFFECT,
            TSE_GPENCIL_EFFECT_BASE)) {
    return false;
  }

  TreeElement *te_target = outliner_drop_insert_find(C, event, &drop_data->insert_type);
  if (!te_target) {
    return false;
  }
  TreeStoreElem *tselem_target = TREESTORE(te_target);

  if (drop_data->drag_tselem == tselem_target) {
    return false;
  }

  Object *ob = NULL;
  TreeElement *object_te = outliner_data_from_tree_element_and_parents(is_object_element,
                                                                       te_target);
  if (object_te) {
    ob = (Object *)TREESTORE(object_te)->id;
  }

  bPoseChannel *pchan = NULL;
  TreeElement *pchan_te = outliner_data_from_tree_element_and_parents(is_pchan_element, te_target);
  if (pchan_te) {
    pchan = (bPoseChannel *)pchan_te->directdata;
  }
  if (pchan) {
    ob = NULL;
  }

  if (ob && ID_IS_LINKED(&ob->id)) {
    return false;
  }

  /* Drag a base for linking. */
  if (ELEM(drop_data->drag_tselem->type,
           TSE_MODIFIER_BASE,
           TSE_CONSTRAINT_BASE,
           TSE_GPENCIL_EFFECT_BASE)) {
    drop_data->insert_type = TE_INSERT_INTO;
    drop_data->drop_action = DATA_STACK_DROP_LINK;

    if (pchan && pchan != drop_data->pchan_parent) {
      drop_data->drop_te = pchan_te;
      tselem_target = TREESTORE(pchan_te);
    }
    else if (ob && ob != drop_data->ob_parent) {
      drop_data->drop_te = object_te;
      tselem_target = TREESTORE(object_te);
    }
    else {
      return false;
    }
  }
  else if (ob || pchan) {
    /* Drag a single item. */
    if (pchan && pchan != drop_data->pchan_parent) {
      drop_data->insert_type = TE_INSERT_INTO;
      drop_data->drop_action = DATA_STACK_DROP_COPY;
      drop_data->drop_te = pchan_te;
      tselem_target = TREESTORE(pchan_te);
    }
    else if (ob && ob != drop_data->ob_parent) {
      drop_data->insert_type = TE_INSERT_INTO;
      drop_data->drop_action = DATA_STACK_DROP_COPY;
      drop_data->drop_te = object_te;
      tselem_target = TREESTORE(object_te);
    }
    else if (tselem_target->type == drop_data->drag_tselem->type) {
      if (drop_data->insert_type == TE_INSERT_INTO) {
        return false;
      }
      drop_data->drop_action = DATA_STACK_DROP_REORDER;
      drop_data->drop_te = te_target;
    }
    else {
      return false;
    }
  }
  else {
    return false;
  }

  return true;
}

/* Ensure that grease pencil and object data remain separate. */
static bool datastack_drop_are_types_valid(StackDropData *drop_data)
{
  TreeStoreElem *tselem = TREESTORE(drop_data->drop_te);
  Object *ob_parent = drop_data->ob_parent;
  Object *ob_dst = (Object *)tselem->id;

  /* Don't allow data to be moved between objects and bones. */
  if (tselem->type == TSE_CONSTRAINT) {
  }
  else if ((drop_data->pchan_parent && tselem->type != TSE_POSE_CHANNEL) ||
           (!drop_data->pchan_parent && tselem->type == TSE_POSE_CHANNEL)) {
    return false;
  }

  switch (drop_data->drag_tselem->type) {
    case TSE_MODIFIER_BASE:
    case TSE_MODIFIER:
      if (ob_parent->type == OB_GPENCIL) {
        return ob_dst->type == OB_GPENCIL;
      }
      else if (ob_parent->type != OB_GPENCIL) {
        return ob_dst->type != OB_GPENCIL;
      }
      break;
    case TSE_CONSTRAINT_BASE:
    case TSE_CONSTRAINT:

      break;
    case TSE_GPENCIL_EFFECT_BASE:
    case TSE_GPENCIL_EFFECT:
      return ob_parent->type == OB_GPENCIL && ob_dst->type == OB_GPENCIL;
      break;
  }

  return true;
}

static bool datastack_drop_poll(bContext *C,
                                wmDrag *drag,
                                const wmEvent *event,
                                const char **r_tooltip)
{
  if (drag->type != WM_DRAG_DATASTACK) {
    return false;
  }

  SpaceOutliner *space_outliner = CTX_wm_space_outliner(C);
  ARegion *region = CTX_wm_region(C);
  bool changed = outliner_flag_set(
      &space_outliner->tree, TSE_HIGHLIGHTED_ANY | TSE_DRAG_ANY, false);

  StackDropData *drop_data = drag->poin;
  if (!drop_data) {
    return false;
  }

  if (!datastack_drop_init(C, event, drop_data)) {
    return false;
  }

  if (!datastack_drop_are_types_valid(drop_data)) {
    return false;
  }

  TreeStoreElem *tselem_target = TREESTORE(drop_data->drop_te);
  switch (drop_data->insert_type) {
    case TE_INSERT_BEFORE:
      tselem_target->flag |= TSE_DRAG_BEFORE;
      break;
    case TE_INSERT_AFTER:
      tselem_target->flag |= TSE_DRAG_AFTER;
      break;
    case TE_INSERT_INTO:
      tselem_target->flag |= TSE_DRAG_INTO;
      break;
  }

  switch (drop_data->drop_action) {
    case DATA_STACK_DROP_REORDER:
      *r_tooltip = TIP_("Reorder");
      break;
    case DATA_STACK_DROP_COPY:
      if (drop_data->pchan_parent) {
        *r_tooltip = TIP_("Copy to bone");
      }
      else {
        *r_tooltip = TIP_("Copy to object");
      }
      break;
    case DATA_STACK_DROP_LINK:
      if (drop_data->pchan_parent) {
        *r_tooltip = TIP_("Link all to bone");
      }
      else {
        *r_tooltip = TIP_("Link all to object");
      }
      break;
  }

  if (changed) {
    ED_region_tag_redraw_no_rebuild(region);
  }

  return true;
}

static void datastack_drop_link(bContext *C, StackDropData *drop_data)
{
  Main *bmain = CTX_data_main(C);
  TreeStoreElem *tselem = TREESTORE(drop_data->drop_te);
  Object *ob_dst = (Object *)tselem->id;

  switch (drop_data->drag_tselem->type) {
    case TSE_MODIFIER_BASE:
      ED_object_modifier_link(C, ob_dst, drop_data->ob_parent);
      break;
    case TSE_CONSTRAINT_BASE: {
      ListBase *src;

      if (drop_data->pchan_parent) {
        src = &drop_data->pchan_parent->constraints;
      }
      else {
        src = &drop_data->ob_parent->constraints;
      }

      ListBase *dst;
      if (tselem->type == TSE_POSE_CHANNEL) {
        bPoseChannel *pchan = (bPoseChannel *)drop_data->drop_te->directdata;
        dst = &pchan->constraints;
      }
      else {
        dst = &ob_dst->constraints;
      }

      ED_object_constraint_link(bmain, ob_dst, dst, src);
      break;
    }
    case TSE_GPENCIL_EFFECT_BASE:
      if (ob_dst->type != OB_GPENCIL) {
        return;
      }

      ED_object_shaderfx_link(ob_dst, drop_data->ob_parent);
      break;
  }
}

static void datastack_drop_copy(bContext *C, StackDropData *drop_data)
{
  Main *bmain = CTX_data_main(C);

  TreeStoreElem *tselem = TREESTORE(drop_data->drop_te);
  Object *ob_dst = (Object *)tselem->id;

  switch (drop_data->drag_tselem->type) {
    case TSE_MODIFIER:
      if (drop_data->ob_parent->type == OB_GPENCIL && ob_dst->type == OB_GPENCIL) {
        ED_object_gpencil_modifier_copy_to_object(ob_dst, drop_data->drag_directdata);
      }
      else if (drop_data->ob_parent->type != OB_GPENCIL && ob_dst->type != OB_GPENCIL) {
        ED_object_modifier_copy_to_object(
            C, ob_dst, drop_data->ob_parent, drop_data->drag_directdata);
      }
      break;
    case TSE_CONSTRAINT:
      if (tselem->type == TSE_POSE_CHANNEL) {
        ED_object_constraint_copy_for_pose(
            bmain, ob_dst, drop_data->drop_te->directdata, drop_data->drag_directdata);
      }
      else {
        ED_object_constraint_copy_for_object(bmain, ob_dst, drop_data->drag_directdata);
      }
      break;
    case TSE_GPENCIL_EFFECT: {
      if (ob_dst->type != OB_GPENCIL) {
        return;
      }

      ED_object_shaderfx_copy(ob_dst, drop_data->drag_directdata);
      break;
    }
  }
}

static void datastack_drop_reorder(bContext *C, ReportList *reports, StackDropData *drop_data)
{
  SpaceOutliner *space_outliner = CTX_wm_space_outliner(C);

  TreeElement *drag_te = outliner_find_tree_element(&space_outliner->tree, drop_data->drag_tselem);
  if (!drag_te) {
    return;
  }

  TreeElement *drop_te = drop_data->drop_te;
  TreeElementInsertType insert_type = drop_data->insert_type;

  Object *ob = drop_data->ob_parent;

  int index = 0;
  switch (drop_data->drag_tselem->type) {
    case TSE_MODIFIER:
      if (ob->type == OB_GPENCIL) {
        index = outliner_get_insert_index(
            drag_te, drop_te, insert_type, &ob->greasepencil_modifiers);
        ED_object_gpencil_modifier_move_to_index(reports, ob, drop_data->drag_directdata, index);
      }
      else if (ob->type != OB_GPENCIL) {
        index = outliner_get_insert_index(drag_te, drop_te, insert_type, &ob->modifiers);
        ED_object_modifier_move_to_index(reports, ob, drop_data->drag_directdata, index);
      }
      break;
    case TSE_CONSTRAINT:
      if (drop_data->pchan_parent) {
        index = outliner_get_insert_index(
            drag_te, drop_te, insert_type, &drop_data->pchan_parent->constraints);
      }
      else {
        index = outliner_get_insert_index(drag_te, drop_te, insert_type, &ob->constraints);
      }
      ED_object_constraint_move_to_index(ob, drop_data->drag_directdata, index);

      break;
    case TSE_GPENCIL_EFFECT:
      index = outliner_get_insert_index(drag_te, drop_te, insert_type, &ob->shader_fx);
      ED_object_shaderfx_move_to_index(reports, ob, drop_data->drag_directdata, index);
  }
}

static int datastack_drop_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  if (event->custom != EVT_DATA_DRAGDROP) {
    return OPERATOR_CANCELLED;
  }

  ListBase *lb = event->customdata;
  wmDrag *drag = lb->first;
  StackDropData *drop_data = drag->poin;

  switch (drop_data->drop_action) {
    case DATA_STACK_DROP_LINK:
      datastack_drop_link(C, drop_data);
      break;
    case DATA_STACK_DROP_COPY:
      datastack_drop_copy(C, drop_data);
      break;
    case DATA_STACK_DROP_REORDER:
      datastack_drop_reorder(C, op->reports, drop_data);
      break;
  }

  return OPERATOR_FINISHED;
}

void OUTLINER_OT_datastack_drop(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Data Stack Drop";
  ot->description = "Copy or reorder modifiers, constraints, and effects";
  ot->idname = "OUTLINER_OT_datastack_drop";

  /* api callbacks */
  ot->invoke = datastack_drop_invoke;

  ot->poll = ED_operator_outliner_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
}

/* ******************** Collection Drop Operator *********************** */

typedef struct CollectionDrop {
  Collection *from;
  Collection *to;

  TreeElement *te;
  TreeElementInsertType insert_type;
} CollectionDrop;

static Collection *collection_parent_from_ID(ID *id)
{
  /* Can't change linked parent collections. */
  if (!id || ID_IS_LINKED(id)) {
    return NULL;
  }

  /* Also support dropping into/from scene collection. */
  if (GS(id->name) == ID_SCE) {
    return ((Scene *)id)->master_collection;
  }
  if (GS(id->name) == ID_GR) {
    return (Collection *)id;
  }

  return NULL;
}

static bool collection_drop_init(bContext *C,
                                 wmDrag *drag,
                                 const wmEvent *event,
                                 CollectionDrop *data)
{
  SpaceOutliner *space_outliner = CTX_wm_space_outliner(C);

  /* Get collection to drop into. */
  TreeElementInsertType insert_type;
  TreeElement *te = outliner_drop_insert_collection_find(C, event, &insert_type);
  if (!te) {
    return false;
  }

  Collection *to_collection = outliner_collection_from_tree_element(te);
  if (ID_IS_LINKED(to_collection)) {
    return false;
  }
  /* Currently this should not be allowed (might be supported in the future though...). */
  if (ID_IS_OVERRIDE_LIBRARY(to_collection)) {
    return false;
  }

  /* Get drag datablocks. */
  if (drag->type != WM_DRAG_ID) {
    return false;
  }

  wmDragID *drag_id = drag->ids.first;
  if (drag_id == NULL) {
    return false;
  }

  ID *id = drag_id->id;
  if (!(id && ELEM(GS(id->name), ID_GR, ID_OB))) {
    return false;
  }

  /* Get collection to drag out of. */
  ID *parent = drag_id->from_parent;
  Collection *from_collection = collection_parent_from_ID(parent);
  if (event->ctrl || space_outliner->outlinevis == SO_SCENES) {
    from_collection = NULL;
  }

  /* Get collections. */
  if (GS(id->name) == ID_GR) {
    if (id == &to_collection->id) {
      return false;
    }
  }
  else {
    insert_type = TE_INSERT_INTO;
  }

  data->from = from_collection;
  data->to = to_collection;
  data->te = te;
  data->insert_type = insert_type;

  return true;
}

static bool collection_drop_poll(bContext *C,
                                 wmDrag *drag,
                                 const wmEvent *event,
                                 const char **r_tooltip)
{
  SpaceOutliner *space_outliner = CTX_wm_space_outliner(C);
  ARegion *region = CTX_wm_region(C);
  bool changed = outliner_flag_set(
      &space_outliner->tree, TSE_HIGHLIGHTED_ANY | TSE_DRAG_ANY, false);

  CollectionDrop data;
  if (!event->shift && collection_drop_init(C, drag, event, &data)) {
    TreeElement *te = data.te;
    TreeStoreElem *tselem = TREESTORE(te);
    if (!data.from || event->ctrl) {
      tselem->flag |= TSE_DRAG_INTO;
      changed = true;
      *r_tooltip = TIP_("Link inside Collection");
    }
    else {
      switch (data.insert_type) {
        case TE_INSERT_BEFORE:
          tselem->flag |= TSE_DRAG_BEFORE;
          changed = true;
          if (te->prev && outliner_is_collection_tree_element(te->prev)) {
            *r_tooltip = TIP_("Move between collections");
          }
          else {
            *r_tooltip = TIP_("Move before collection");
          }
          break;
        case TE_INSERT_AFTER:
          tselem->flag |= TSE_DRAG_AFTER;
          changed = true;
          if (te->next && outliner_is_collection_tree_element(te->next)) {
            *r_tooltip = TIP_("Move between collections");
          }
          else {
            *r_tooltip = TIP_("Move after collection");
          }
          break;
        case TE_INSERT_INTO: {
          tselem->flag |= TSE_DRAG_INTO;
          changed = true;

          /* Check the type of the drag IDs to avoid the incorrect "Shift to parent"
           * for collections. Checking the type of the first ID works fine here since
           * all drag IDs are the same type. */
          wmDragID *drag_id = (wmDragID *)drag->ids.first;
          const bool is_object = (GS(drag_id->id->name) == ID_OB);
          if (is_object) {
            *r_tooltip = TIP_("Move inside collection (Ctrl to link, Shift to parent)");
          }
          else {
            *r_tooltip = TIP_("Move inside collection (Ctrl to link)");
          }
          break;
        }
      }
    }
    if (changed) {
      ED_region_tag_redraw_no_rebuild(region);
    }
    return true;
  }
  if (changed) {
    ED_region_tag_redraw_no_rebuild(region);
  }
  return false;
}

static int collection_drop_invoke(bContext *C, wmOperator *UNUSED(op), const wmEvent *event)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);

  if (event->custom != EVT_DATA_DRAGDROP) {
    return OPERATOR_CANCELLED;
  }

  ListBase *lb = event->customdata;
  wmDrag *drag = lb->first;

  CollectionDrop data;
  if (!collection_drop_init(C, drag, event, &data)) {
    return OPERATOR_CANCELLED;
  }

  /* Before/after insert handling. */
  Collection *relative = NULL;
  bool relative_after = false;

  if (ELEM(data.insert_type, TE_INSERT_BEFORE, TE_INSERT_AFTER)) {
    SpaceOutliner *space_outliner = CTX_wm_space_outliner(C);

    relative = data.to;
    relative_after = (data.insert_type == TE_INSERT_AFTER);

    TreeElement *parent_te = outliner_find_parent_element(&space_outliner->tree, NULL, data.te);
    data.to = (parent_te) ? outliner_collection_from_tree_element(parent_te) : NULL;
  }

  if (!data.to) {
    return OPERATOR_CANCELLED;
  }

  if (BKE_collection_is_empty(data.to)) {
    TREESTORE(data.te)->flag &= ~TSE_CLOSED;
  }

  LISTBASE_FOREACH (wmDragID *, drag_id, &drag->ids) {
    /* Ctrl enables linking, so we don't need a from collection then. */
    Collection *from = (event->ctrl) ? NULL : collection_parent_from_ID(drag_id->from_parent);

    if (GS(drag_id->id->name) == ID_OB) {
      /* Move/link object into collection. */
      Object *object = (Object *)drag_id->id;

      if (from) {
        BKE_collection_object_move(bmain, scene, data.to, from, object);
      }
      else {
        BKE_collection_object_add(bmain, data.to, object);
      }
    }
    else if (GS(drag_id->id->name) == ID_GR) {
      /* Move/link collection into collection. */
      Collection *collection = (Collection *)drag_id->id;

      if (collection != from) {
        BKE_collection_move(bmain, data.to, from, relative, relative_after, collection);
      }
    }

    if (from) {
      DEG_id_tag_update(&from->id, ID_RECALC_COPY_ON_WRITE);
    }
  }

  /* Update dependency graph. */
  DEG_id_tag_update(&data.to->id, ID_RECALC_COPY_ON_WRITE);
  DEG_relations_tag_update(bmain);
  WM_event_add_notifier(C, NC_SCENE | ND_LAYER, scene);

  return OPERATOR_FINISHED;
}

void OUTLINER_OT_collection_drop(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Move to Collection";
  ot->description = "Drag to move to collection in Outliner";
  ot->idname = "OUTLINER_OT_collection_drop";

  /* api callbacks */
  ot->invoke = collection_drop_invoke;
  ot->poll = ED_operator_outliner_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
}

/* ********************* Outliner Drag Operator ******************** */

#define OUTLINER_DRAG_SCOLL_OUTSIDE_PAD 7 /* In UI units */

static TreeElement *outliner_item_drag_element_find(SpaceOutliner *space_outliner,
                                                    ARegion *region,
                                                    const wmEvent *event)
{
  /* note: using EVT_TWEAK_ events to trigger dragging is fine,
   * it sends coordinates from where dragging was started */
  const float my = UI_view2d_region_to_view_y(&region->v2d, event->mval[1]);
  return outliner_find_item_at_y(space_outliner, &space_outliner->tree, my);
}

static int outliner_item_drag_drop_invoke(bContext *C,
                                          wmOperator *UNUSED(op),
                                          const wmEvent *event)
{
  ARegion *region = CTX_wm_region(C);
  SpaceOutliner *space_outliner = CTX_wm_space_outliner(C);
  TreeElement *te = outliner_item_drag_element_find(space_outliner, region, event);

  if (!te) {
    return (OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH);
  }

  TreeStoreElem *tselem = TREESTORE(te);
  TreeElementIcon data = tree_element_get_icon(tselem, te);
  if (!data.drag_id) {
    return (OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH);
  }

  float view_mval[2];
  UI_view2d_region_to_view(
      &region->v2d, event->mval[0], event->mval[1], &view_mval[0], &view_mval[1]);
  if (outliner_item_is_co_within_close_toggle(te, view_mval[0])) {
    return (OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH);
  }
  if (outliner_is_co_within_mode_column(space_outliner, view_mval)) {
    return OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH;
  }

  /* Scroll the view when dragging near edges, but not
   * when the drag goes too far outside the region. */
  {
    wmOperatorType *ot = WM_operatortype_find("VIEW2D_OT_edge_pan", true);
    PointerRNA op_ptr;
    WM_operator_properties_create_ptr(&op_ptr, ot);
    RNA_int_set(&op_ptr, "outside_padding", OUTLINER_DRAG_SCOLL_OUTSIDE_PAD);
    WM_operator_name_call_ptr(C, ot, WM_OP_INVOKE_DEFAULT, &op_ptr);
    WM_operator_properties_free(&op_ptr);
  }

  const bool use_datastack_drag = ELEM(tselem->type,
                                       TSE_MODIFIER,
                                       TSE_MODIFIER_BASE,
                                       TSE_CONSTRAINT,
                                       TSE_CONSTRAINT_BASE,
                                       TSE_GPENCIL_EFFECT,
                                       TSE_GPENCIL_EFFECT_BASE);

  const int wm_drag_type = use_datastack_drag ? WM_DRAG_DATASTACK : WM_DRAG_ID;
  wmDrag *drag = WM_event_start_drag(C, data.icon, wm_drag_type, NULL, 0.0, WM_DRAG_NOP);

  if (use_datastack_drag) {
    TreeElement *te_bone = NULL;
    bPoseChannel *pchan = outliner_find_parent_bone(te, &te_bone);
    datastack_drop_data_init(drag, (Object *)tselem->id, pchan, te, tselem, te->directdata);
  }
  else if (ELEM(GS(data.drag_id->name), ID_OB, ID_GR)) {
    /* For collections and objects we cheat and drag all selected. */

    /* Only drag element under mouse if it was not selected before. */
    if ((tselem->flag & TSE_SELECTED) == 0) {
      outliner_flag_set(&space_outliner->tree, TSE_SELECTED, 0);
      tselem->flag |= TSE_SELECTED;
    }

    /* Gather all selected elements. */
    struct IDsSelectedData selected = {
        .selected_array = {NULL, NULL},
    };

    if (GS(data.drag_id->name) == ID_OB) {
      outliner_tree_traverse(space_outliner,
                             &space_outliner->tree,
                             0,
                             TSE_SELECTED,
                             outliner_find_selected_objects,
                             &selected);
    }
    else {
      outliner_tree_traverse(space_outliner,
                             &space_outliner->tree,
                             0,
                             TSE_SELECTED,
                             outliner_find_selected_collections,
                             &selected);
    }

    LISTBASE_FOREACH (LinkData *, link, &selected.selected_array) {
      TreeElement *te_selected = (TreeElement *)link->data;
      ID *id;

      if (GS(data.drag_id->name) == ID_OB) {
        id = TREESTORE(te_selected)->id;
      }
      else {
        /* Keep collection hierarchies intact when dragging. */
        bool parent_selected = false;
        for (TreeElement *te_parent = te_selected->parent; te_parent;
             te_parent = te_parent->parent) {
          if (outliner_is_collection_tree_element(te_parent)) {
            if (TREESTORE(te_parent)->flag & TSE_SELECTED) {
              parent_selected = true;
              break;
            }
          }
        }

        if (parent_selected) {
          continue;
        }

        id = &outliner_collection_from_tree_element(te_selected)->id;
      }

      /* Find parent collection. */
      Collection *parent = NULL;

      if (te_selected->parent) {
        for (TreeElement *te_parent = te_selected->parent; te_parent;
             te_parent = te_parent->parent) {
          if (outliner_is_collection_tree_element(te_parent)) {
            parent = outliner_collection_from_tree_element(te_parent);
            break;
          }
        }
      }
      else {
        Scene *scene = CTX_data_scene(C);
        parent = scene->master_collection;
      }

      WM_drag_add_local_ID(drag, id, &parent->id);
    }

    BLI_freelistN(&selected.selected_array);
  }
  else {
    /* Add single ID. */
    WM_drag_add_local_ID(drag, data.drag_id, data.drag_parent);
  }

  ED_outliner_select_sync_from_outliner(C, space_outliner);

  return (OPERATOR_FINISHED | OPERATOR_PASS_THROUGH);
}

/* Outliner drag and drop. This operator mostly exists to support dragging
 * from outliner text instead of only from the icon, and also to show a
 * hint in the status-bar key-map. */

void OUTLINER_OT_item_drag_drop(wmOperatorType *ot)
{
  ot->name = "Drag and Drop";
  ot->idname = "OUTLINER_OT_item_drag_drop";
  ot->description = "Drag and drop element to another place";

  ot->invoke = outliner_item_drag_drop_invoke;
  ot->poll = ED_operator_outliner_active;
}

#undef OUTLINER_DRAG_SCOLL_OUTSIDE_PAD

/* *************************** Drop Boxes ************************** */

/* region dropbox definition */
void outliner_dropboxes(void)
{
  ListBase *lb = WM_dropboxmap_find("Outliner", SPACE_OUTLINER, RGN_TYPE_WINDOW);

  WM_dropbox_add(lb, "OUTLINER_OT_parent_drop", parent_drop_poll, NULL, NULL);
  WM_dropbox_add(lb, "OUTLINER_OT_parent_clear", parent_clear_poll, NULL, NULL);
  WM_dropbox_add(lb, "OUTLINER_OT_scene_drop", scene_drop_poll, NULL, NULL);
  WM_dropbox_add(lb, "OUTLINER_OT_material_drop", material_drop_poll, NULL, NULL);
  WM_dropbox_add(lb, "OUTLINER_OT_datastack_drop", datastack_drop_poll, NULL, NULL);
  WM_dropbox_add(lb, "OUTLINER_OT_collection_drop", collection_drop_poll, NULL, NULL);
}
