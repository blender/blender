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
#include "BLI_string.h"

#include "BLT_translation.h"

#include "BKE_collection.h"
#include "BKE_context.h"
#include "BKE_layer.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_object.h"
#include "BKE_report.h"
#include "BKE_scene.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"

#include "ED_object.h"
#include "ED_outliner.h"
#include "ED_screen.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "GPU_state.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "WM_api.h"
#include "WM_types.h"

#include "outliner_intern.h"

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
    for (te = te->subtree.first; te; te = te->next) {
      TreeElement *te_valid = outliner_dropzone_element(te, fmval, children);
      if (te_valid) {
        return te_valid;
      }
    }
  }
  return NULL;
}

/* Find tree element to drop into. */
static TreeElement *outliner_dropzone_find(const SpaceOutliner *soops,
                                           const float fmval[2],
                                           const bool children)
{
  TreeElement *te;

  for (te = soops->tree.first; te; te = te->next) {
    TreeElement *te_valid = outliner_dropzone_element(te, fmval, children);
    if (te_valid) {
      return te_valid;
    }
  }
  return NULL;
}

static TreeElement *outliner_drop_find(bContext *C, const wmEvent *event)
{
  ARegion *ar = CTX_wm_region(C);
  SpaceOutliner *soops = CTX_wm_space_outliner(C);
  float fmval[2];
  UI_view2d_region_to_view(&ar->v2d, event->mval[0], event->mval[1], &fmval[0], &fmval[1]);

  return outliner_dropzone_find(soops, fmval, true);
}

static ID *outliner_ID_drop_find(bContext *C, const wmEvent *event, short idcode)
{
  TreeElement *te = outliner_drop_find(C, event);
  TreeStoreElem *tselem = (te) ? TREESTORE(te) : NULL;

  if (te && te->idcode == idcode && tselem->type == 0) {
    return tselem->id;
  }
  else {
    return NULL;
  }
}

/* Find tree element to drop into, with additional before and after reorder support. */
static TreeElement *outliner_drop_insert_find(bContext *C,
                                              const wmEvent *event,
                                              TreeElementInsertType *r_insert_type)
{
  SpaceOutliner *soops = CTX_wm_space_outliner(C);
  ARegion *ar = CTX_wm_region(C);
  TreeElement *te_hovered;
  float view_mval[2];

  UI_view2d_region_to_view(&ar->v2d, event->mval[0], event->mval[1], &view_mval[0], &view_mval[1]);
  te_hovered = outliner_find_item_at_y(soops, &soops->tree, view_mval[1]);

  if (te_hovered) {
    /* Mouse hovers an element (ignoring x-axis),
     * now find out how to insert the dragged item exactly. */
    const float margin = UI_UNIT_Y * (1.0f / 4);

    if (view_mval[1] < (te_hovered->ys + margin)) {
      if (TSELEM_OPEN(TREESTORE(te_hovered), soops)) {
        /* inserting after a open item means we insert into it, but as first child */
        if (BLI_listbase_is_empty(&te_hovered->subtree)) {
          *r_insert_type = TE_INSERT_INTO;
          return te_hovered;
        }
        else {
          *r_insert_type = TE_INSERT_BEFORE;
          return te_hovered->subtree.first;
        }
      }
      else {
        *r_insert_type = TE_INSERT_AFTER;
        return te_hovered;
      }
    }
    else if (view_mval[1] > (te_hovered->ys + (3 * margin))) {
      *r_insert_type = TE_INSERT_BEFORE;
      return te_hovered;
    }
    else {
      *r_insert_type = TE_INSERT_INTO;
      return te_hovered;
    }
  }
  else {
    /* Mouse doesn't hover any item (ignoring x-axis),
     * so it's either above list bounds or below. */
    TreeElement *first = soops->tree.first;
    TreeElement *last = soops->tree.last;

    if (view_mval[1] < last->ys) {
      *r_insert_type = TE_INSERT_AFTER;
      return last;
    }
    else if (view_mval[1] > (first->ys + UI_UNIT_Y)) {
      *r_insert_type = TE_INSERT_BEFORE;
      return first;
    }
    else {
      BLI_assert(0);
      return NULL;
    }
  }
}

static Collection *outliner_collection_from_tree_element_and_parents(TreeElement *te,
                                                                     TreeElement **r_te)
{
  while (te != NULL) {
    Collection *collection = outliner_collection_from_tree_element(te);
    if (collection) {
      *r_te = te;
      return collection;
    }
    te = te->parent;
  }
  return NULL;
}

static TreeElement *outliner_drop_insert_collection_find(bContext *C,
                                                         const wmEvent *event,
                                                         TreeElementInsertType *r_insert_type)
{
  TreeElement *te = outliner_drop_insert_find(C, event, r_insert_type);
  if (!te) {
    return NULL;
  }

  TreeElement *collection_te;
  Collection *collection = outliner_collection_from_tree_element_and_parents(te, &collection_te);
  if (!collection) {
    return NULL;
  }

  if (collection_te != te) {
    *r_insert_type = TE_INSERT_INTO;
  }

  /* We can't insert before/after master collection. */
  if (collection->flag & COLLECTION_IS_MASTER) {
    *r_insert_type = TE_INSERT_INTO;
  }

  return collection_te;
}

/* ******************** Parent Drop Operator *********************** */

static bool parent_drop_allowed(SpaceOutliner *soops, TreeElement *te, Object *potential_child)
{
  TreeStoreElem *tselem = TREESTORE(te);
  if (te->idcode != ID_OB || tselem->type != 0) {
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
  Scene *scene = (Scene *)outliner_search_back(soops, te, ID_SCE);

  /* currently outliner organized in a way that if there's no parent scene
   * element for object it means that all displayed objects belong to
   * active scene and parenting them is allowed (sergey) */
  if (scene) {
    for (ViewLayer *view_layer = scene->view_layers.first; view_layer;
         view_layer = view_layer->next) {
      if (BKE_view_layer_base_find(view_layer, potential_child)) {
        return true;
      }
    }
    return false;
  }
  else {
    return true;
  }
}

static bool allow_parenting_without_modifier_key(SpaceOutliner *soops)
{
  switch (soops->outlinevis) {
    case SO_VIEW_LAYER:
      return soops->filter & SO_FILTER_NO_COLLECTION;
    case SO_SCENES:
      return true;
    default:
      return false;
  }
}

static bool parent_drop_poll(bContext *C,
                             wmDrag *drag,
                             const wmEvent *event,
                             const char **UNUSED(tooltip))
{
  SpaceOutliner *soops = CTX_wm_space_outliner(C);

  bool changed = outliner_flag_set(&soops->tree, TSE_DRAG_ANY, false);
  if (changed) {
    ED_region_tag_redraw_no_rebuild(CTX_wm_region(C));
  }

  Object *potential_child = (Object *)WM_drag_ID(drag, ID_OB);
  if (!potential_child) {
    return false;
  }

  if (!allow_parenting_without_modifier_key(soops)) {
    if (!event->shift) {
      return false;
    }
  }

  TreeElement *te = outliner_drop_find(C, event);
  if (!te) {
    return false;
  }

  if (parent_drop_allowed(soops, te, potential_child)) {
    TREESTORE(te)->flag |= TSE_DRAG_INTO;
    ED_region_tag_redraw_no_rebuild(CTX_wm_region(C));
    return true;
  }

  return false;
}

static int parent_drop_exec(bContext *C, wmOperator *op)
{
  Object *par = NULL, *ob = NULL;
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  int partype = -1;
  char parname[MAX_NAME], childname[MAX_NAME];

  partype = RNA_enum_get(op->ptr, "type");
  RNA_string_get(op->ptr, "parent", parname);
  par = (Object *)BKE_libblock_find_name(bmain, ID_OB, parname);
  RNA_string_get(op->ptr, "child", childname);
  ob = (Object *)BKE_libblock_find_name(bmain, ID_OB, childname);

  if (ID_IS_LINKED(ob)) {
    BKE_report(op->reports, RPT_INFO, "Can't edit library linked object");
    return OPERATOR_CANCELLED;
  }

  ED_object_parent_set(op->reports, C, scene, ob, par, partype, false, false, NULL);

  DEG_relations_tag_update(bmain);
  WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, NULL);
  WM_event_add_notifier(C, NC_OBJECT | ND_PARENT, NULL);

  return OPERATOR_FINISHED;
}

static int parent_drop_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  Main *bmain = CTX_data_main(C);
  SpaceOutliner *soops = CTX_wm_space_outliner(C);
  TreeElement *te = outliner_drop_find(C, event);
  TreeStoreElem *tselem = te ? TREESTORE(te) : NULL;

  if (!(te && te->idcode == ID_OB && tselem->type == 0)) {
    return OPERATOR_CANCELLED;
  }

  Object *par = (Object *)tselem->id;
  Object *ob = (Object *)WM_drag_ID_from_event(event, ID_OB);

  if (ELEM(NULL, ob, par)) {
    return OPERATOR_CANCELLED;
  }
  if (ob == par) {
    return OPERATOR_CANCELLED;
  }
  if (ID_IS_LINKED(ob)) {
    BKE_report(op->reports, RPT_INFO, "Can't edit library linked object");
    return OPERATOR_CANCELLED;
  }

  char childname[MAX_NAME];
  char parname[MAX_NAME];
  STRNCPY(childname, ob->id.name + 2);
  STRNCPY(parname, par->id.name + 2);
  RNA_string_set(op->ptr, "child", childname);
  RNA_string_set(op->ptr, "parent", parname);

  Scene *scene = (Scene *)outliner_search_back(soops, te, ID_SCE);

  if (scene == NULL) {
    /* currently outlier organized in a way, that if there's no parent scene
     * element for object it means that all displayed objects belong to
     * active scene and parenting them is allowed (sergey)
     */

    scene = CTX_data_scene(C);
  }

  if ((par->type != OB_ARMATURE) && (par->type != OB_CURVE) && (par->type != OB_LATTICE)) {
    int partype = 0;
    if (ED_object_parent_set(op->reports, C, scene, ob, par, partype, false, false, NULL)) {
      DEG_relations_tag_update(bmain);
      WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, NULL);
      WM_event_add_notifier(C, NC_OBJECT | ND_PARENT, NULL);
    }
  }
  else {
    /* Menu creation */
    wmOperatorType *ot = WM_operatortype_find("OUTLINER_OT_parent_drop", false);
    uiPopupMenu *pup = UI_popup_menu_begin(C, IFACE_("Set Parent To"), ICON_NONE);
    uiLayout *layout = UI_popup_menu_layout(pup);
    PointerRNA ptr;

    /* Cannot use uiItemEnumO()... have multiple properties to set. */
    uiItemFullO_ptr(layout, ot, IFACE_("Object"), 0, NULL, WM_OP_EXEC_DEFAULT, 0, &ptr);
    RNA_string_set(&ptr, "parent", parname);
    RNA_string_set(&ptr, "child", childname);
    RNA_enum_set(&ptr, "type", PAR_OBJECT);

    /* par becomes parent, make the associated menus */
    if (par->type == OB_ARMATURE) {
      uiItemFullO_ptr(layout, ot, IFACE_("Armature Deform"), 0, NULL, WM_OP_EXEC_DEFAULT, 0, &ptr);
      RNA_string_set(&ptr, "parent", parname);
      RNA_string_set(&ptr, "child", childname);
      RNA_enum_set(&ptr, "type", PAR_ARMATURE);

      uiItemFullO_ptr(
          layout, ot, IFACE_("   With Empty Groups"), 0, NULL, WM_OP_EXEC_DEFAULT, 0, &ptr);
      RNA_string_set(&ptr, "parent", parname);
      RNA_string_set(&ptr, "child", childname);
      RNA_enum_set(&ptr, "type", PAR_ARMATURE_NAME);

      uiItemFullO_ptr(
          layout, ot, IFACE_("   With Envelope Weights"), 0, NULL, WM_OP_EXEC_DEFAULT, 0, &ptr);
      RNA_string_set(&ptr, "parent", parname);
      RNA_string_set(&ptr, "child", childname);
      RNA_enum_set(&ptr, "type", PAR_ARMATURE_ENVELOPE);

      uiItemFullO_ptr(
          layout, ot, IFACE_("   With Automatic Weights"), 0, NULL, WM_OP_EXEC_DEFAULT, 0, &ptr);
      RNA_string_set(&ptr, "parent", parname);
      RNA_string_set(&ptr, "child", childname);
      RNA_enum_set(&ptr, "type", PAR_ARMATURE_AUTO);

      uiItemFullO_ptr(layout, ot, IFACE_("Bone"), 0, NULL, WM_OP_EXEC_DEFAULT, 0, &ptr);
      RNA_string_set(&ptr, "parent", parname);
      RNA_string_set(&ptr, "child", childname);
      RNA_enum_set(&ptr, "type", PAR_BONE);
    }
    else if (par->type == OB_CURVE) {
      uiItemFullO_ptr(layout, ot, IFACE_("Curve Deform"), 0, NULL, WM_OP_EXEC_DEFAULT, 0, &ptr);
      RNA_string_set(&ptr, "parent", parname);
      RNA_string_set(&ptr, "child", childname);
      RNA_enum_set(&ptr, "type", PAR_CURVE);

      uiItemFullO_ptr(layout, ot, IFACE_("Follow Path"), 0, NULL, WM_OP_EXEC_DEFAULT, 0, &ptr);
      RNA_string_set(&ptr, "parent", parname);
      RNA_string_set(&ptr, "child", childname);
      RNA_enum_set(&ptr, "type", PAR_FOLLOW);

      uiItemFullO_ptr(layout, ot, IFACE_("Path Constraint"), 0, NULL, WM_OP_EXEC_DEFAULT, 0, &ptr);
      RNA_string_set(&ptr, "parent", parname);
      RNA_string_set(&ptr, "child", childname);
      RNA_enum_set(&ptr, "type", PAR_PATH_CONST);
    }
    else if (par->type == OB_LATTICE) {
      uiItemFullO_ptr(layout, ot, IFACE_("Lattice Deform"), 0, NULL, WM_OP_EXEC_DEFAULT, 0, &ptr);
      RNA_string_set(&ptr, "parent", parname);
      RNA_string_set(&ptr, "child", childname);
      RNA_enum_set(&ptr, "type", PAR_LATTICE);
    }

    UI_popup_menu_end(C, pup);

    return OPERATOR_INTERFACE;
  }

  return OPERATOR_FINISHED;
}

void OUTLINER_OT_parent_drop(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Drop to Set Parent";
  ot->description = "Drag to parent in Outliner";
  ot->idname = "OUTLINER_OT_parent_drop";

  /* api callbacks */
  ot->invoke = parent_drop_invoke;
  ot->exec = parent_drop_exec;

  ot->poll = ED_operator_outliner_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;

  /* properties */
  RNA_def_string(ot->srna, "child", "Object", MAX_NAME, "Child", "Child Object");
  RNA_def_string(ot->srna, "parent", "Object", MAX_NAME, "Parent", "Parent Object");
  RNA_def_enum(ot->srna, "type", prop_make_parent_types, 0, "Type", "");
}

/* ******************** Parent Clear Operator *********************** */

static bool parent_clear_poll(bContext *C,
                              wmDrag *drag,
                              const wmEvent *event,
                              const char **UNUSED(tooltip))
{
  SpaceOutliner *soops = CTX_wm_space_outliner(C);

  if (!allow_parenting_without_modifier_key(soops)) {
    if (!event->shift) {
      return false;
    }
  }

  Object *ob = (Object *)WM_drag_ID(drag, ID_OB);
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
        return event->shift;
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
  Object *ob = (Object *)WM_drag_ID_from_event(event, ID_OB);

  if (ob == NULL) {
    return OPERATOR_CANCELLED;
  }

  ED_object_parent_clear(ob, 0);

  DEG_relations_tag_update(bmain);
  WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, NULL);
  WM_event_add_notifier(C, NC_OBJECT | ND_PARENT, NULL);
  return OPERATOR_FINISHED;
}

void OUTLINER_OT_parent_clear(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Drop to Clear Parent";
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
                            const char **UNUSED(tooltip))
{
  /* Ensure item under cursor is valid drop target */
  Object *ob = (Object *)WM_drag_ID(drag, ID_OB);
  return (ob && (outliner_ID_drop_find(C, event, ID_SCE) != NULL));
}

static int scene_drop_invoke(bContext *C, wmOperator *UNUSED(op), const wmEvent *event)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = (Scene *)outliner_ID_drop_find(C, event, ID_SCE);
  Object *ob = (Object *)WM_drag_ID_from_event(event, ID_OB);

  if (ELEM(NULL, ob, scene) || ID_IS_LINKED(scene)) {
    return OPERATOR_CANCELLED;
  }

  if (BKE_scene_has_object(scene, ob)) {
    return OPERATOR_CANCELLED;
  }

  Collection *collection;
  if (scene != CTX_data_scene(C)) {
    /* when linking to an inactive scene link to the master collection */
    collection = BKE_collection_master(scene);
  }
  else {
    collection = CTX_data_collection(C);
  }

  BKE_collection_object_add(bmain, collection, ob);

  for (ViewLayer *view_layer = scene->view_layers.first; view_layer;
       view_layer = view_layer->next) {
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
                               const char **UNUSED(tooltip))
{
  /* Ensure item under cursor is valid drop target */
  Material *ma = (Material *)WM_drag_ID(drag, ID_MA);
  return (ma && (outliner_ID_drop_find(C, event, ID_OB) != NULL));
}

static int material_drop_invoke(bContext *C, wmOperator *UNUSED(op), const wmEvent *event)
{
  Main *bmain = CTX_data_main(C);
  Object *ob = (Object *)outliner_ID_drop_find(C, event, ID_OB);
  Material *ma = (Material *)WM_drag_ID_from_event(event, ID_MA);

  if (ELEM(NULL, ob, ma)) {
    return OPERATOR_CANCELLED;
  }

  /* only drop grease pencil material on grease pencil objects */
  if ((ma->gp_style != NULL) && (ob->type != OB_GPENCIL)) {
    return OPERATOR_CANCELLED;
  }

  assign_material(bmain, ob, ma, ob->totcol + 1, BKE_MAT_ASSIGN_USERPREF);

  WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, CTX_wm_view3d(C));
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
  else if (GS(id->name) == ID_GR) {
    return (Collection *)id;
  }

  return NULL;
}

static bool collection_drop_init(bContext *C,
                                 wmDrag *drag,
                                 const wmEvent *event,
                                 CollectionDrop *data)
{
  SpaceOutliner *soops = CTX_wm_space_outliner(C);

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
  if (event->ctrl || soops->outlinevis == SO_SCENES) {
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
                                 const char **tooltip)
{
  SpaceOutliner *soops = CTX_wm_space_outliner(C);
  ARegion *ar = CTX_wm_region(C);
  bool changed = outliner_flag_set(&soops->tree, TSE_HIGHLIGHTED | TSE_DRAG_ANY, false);

  CollectionDrop data;
  if (!event->shift && collection_drop_init(C, drag, event, &data)) {
    TreeElement *te = data.te;
    TreeStoreElem *tselem = TREESTORE(te);
    if (!data.from || event->ctrl) {
      tselem->flag |= TSE_DRAG_INTO;
      changed = true;
      *tooltip = TIP_("Link inside Collection");
    }
    else {
      switch (data.insert_type) {
        case TE_INSERT_BEFORE:
          tselem->flag |= TSE_DRAG_BEFORE;
          changed = true;
          if (te->prev && outliner_is_collection_tree_element(te->prev)) {
            *tooltip = TIP_("Move between collections");
          }
          else {
            *tooltip = TIP_("Move before collection");
          }
          break;
        case TE_INSERT_AFTER:
          tselem->flag |= TSE_DRAG_AFTER;
          changed = true;
          if (te->next && outliner_is_collection_tree_element(te->next)) {
            *tooltip = TIP_("Move between collections");
          }
          else {
            *tooltip = TIP_("Move after collection");
          }
          break;
        case TE_INSERT_INTO:
          tselem->flag |= TSE_DRAG_INTO;
          changed = true;
          *tooltip = TIP_("Move inside collection (Ctrl to link, Shift to parent)");
          break;
      }
    }
    if (changed) {
      ED_region_tag_redraw_no_rebuild(ar);
    }
    return true;
  }
  else {
    if (changed) {
      ED_region_tag_redraw_no_rebuild(ar);
    }
    return false;
  }
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
    SpaceOutliner *soops = CTX_wm_space_outliner(C);

    relative = data.to;
    relative_after = (data.insert_type == TE_INSERT_AFTER);

    TreeElement *parent_te = outliner_find_parent_element(&soops->tree, NULL, data.te);
    data.to = (parent_te) ? outliner_collection_from_tree_element(parent_te) : NULL;
  }

  if (!data.to) {
    return OPERATOR_CANCELLED;
  }

  if (BKE_collection_is_empty(data.to)) {
    TREESTORE(data.te)->flag &= ~TSE_CLOSED;
  }

  for (wmDragID *drag_id = drag->ids.first; drag_id; drag_id = drag_id->next) {
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

static TreeElement *outliner_item_drag_element_find(SpaceOutliner *soops,
                                                    ARegion *ar,
                                                    const wmEvent *event)
{
  /* note: using EVT_TWEAK_ events to trigger dragging is fine,
   * it sends coordinates from where dragging was started */
  const float my = UI_view2d_region_to_view_y(&ar->v2d, event->mval[1]);
  return outliner_find_item_at_y(soops, &soops->tree, my);
}

static int outliner_item_drag_drop_invoke(bContext *C,
                                          wmOperator *UNUSED(op),
                                          const wmEvent *event)
{
  ARegion *ar = CTX_wm_region(C);
  SpaceOutliner *soops = CTX_wm_space_outliner(C);
  TreeElement *te = outliner_item_drag_element_find(soops, ar, event);

  if (!te) {
    return (OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH);
  }

  TreeElementIcon data = tree_element_get_icon(TREESTORE(te), te);
  if (!data.drag_id) {
    return (OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH);
  }

  wmDrag *drag = WM_event_start_drag(C, data.icon, WM_DRAG_ID, NULL, 0.0, WM_DRAG_NOP);

  if (ELEM(GS(data.drag_id->name), ID_OB, ID_GR)) {
    /* For collections and objects we cheat and drag all selected. */

    /* Only drag element under mouse if it was not selected before. */
    if ((TREESTORE(te)->flag & TSE_SELECTED) == 0) {
      outliner_flag_set(&soops->tree, TSE_SELECTED, 0);
      TREESTORE(te)->flag |= TSE_SELECTED;
    }

    /* Gather all selected elements. */
    struct IDsSelectedData selected = {
        .selected_array = {NULL, NULL},
    };

    if (GS(data.drag_id->name) == ID_OB) {
      outliner_tree_traverse(
          soops, &soops->tree, 0, TSE_SELECTED, outliner_find_selected_objects, &selected);
    }
    else {
      outliner_tree_traverse(
          soops, &soops->tree, 0, TSE_SELECTED, outliner_find_selected_collections, &selected);
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
        parent = BKE_collection_master(scene);
      }

      WM_drag_add_ID(drag, id, &parent->id);
    }

    BLI_freelistN(&selected.selected_array);
  }
  else {
    /* Add single ID. */
    WM_drag_add_ID(drag, data.drag_id, data.drag_parent);
  }

  return (OPERATOR_FINISHED | OPERATOR_PASS_THROUGH);
}

/* Outliner drag and drop. This operator mostly exists to support dragging
 * from outliner text instead of only from the icon, and also to show a
 * hint in the statusbar keymap. */

void OUTLINER_OT_item_drag_drop(wmOperatorType *ot)
{
  ot->name = "Drag and Drop";
  ot->idname = "OUTLINER_OT_item_drag_drop";
  ot->description = "Drag and drop element to another place";

  ot->invoke = outliner_item_drag_drop_invoke;
  ot->poll = ED_operator_outliner_active;
}

/* *************************** Drop Boxes ************************** */

/* region dropbox definition */
void outliner_dropboxes(void)
{
  ListBase *lb = WM_dropboxmap_find("Outliner", SPACE_OUTLINER, RGN_TYPE_WINDOW);

  WM_dropbox_add(lb, "OUTLINER_OT_parent_drop", parent_drop_poll, NULL);
  WM_dropbox_add(lb, "OUTLINER_OT_parent_clear", parent_clear_poll, NULL);
  WM_dropbox_add(lb, "OUTLINER_OT_scene_drop", scene_drop_poll, NULL);
  WM_dropbox_add(lb, "OUTLINER_OT_material_drop", material_drop_poll, NULL);
  WM_dropbox_add(lb, "OUTLINER_OT_collection_drop", collection_drop_poll, NULL);
}
