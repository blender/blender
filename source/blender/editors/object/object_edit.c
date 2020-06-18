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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

/** \file
 * \ingroup edobj
 */

#include <ctype.h>
#include <float.h>
#include <math.h>
#include <stddef.h>  //for offsetof
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_ghash.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "DNA_armature_types.h"
#include "DNA_collection_types.h"
#include "DNA_curve_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_lattice_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_meta_types.h"
#include "DNA_object_force_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_vfont_types.h"
#include "DNA_workspace_types.h"

#include "IMB_imbuf_types.h"

#include "BKE_anim_visualization.h"
#include "BKE_collection.h"
#include "BKE_constraint.h"
#include "BKE_context.h"
#include "BKE_curve.h"
#include "BKE_editlattice.h"
#include "BKE_editmesh.h"
#include "BKE_effect.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_lattice.h"
#include "BKE_layer.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_mball.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_particle.h"
#include "BKE_pointcache.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_softbody.h"
#include "BKE_workspace.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"

#include "ED_anim_api.h"
#include "ED_armature.h"
#include "ED_curve.h"
#include "ED_gpencil.h"
#include "ED_image.h"
#include "ED_lattice.h"
#include "ED_mball.h"
#include "ED_mesh.h"
#include "ED_object.h"
#include "ED_outliner.h"
#include "ED_screen.h"
#include "ED_undo.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "CLG_log.h"

/* for menu/popup icons etc etc*/

#include "UI_interface.h"
#include "UI_resources.h"

#include "WM_api.h"
#include "WM_message.h"
#include "WM_toolsystem.h"
#include "WM_types.h"

#include "object_intern.h"  // own include

static CLG_LogRef LOG = {"ed.object.edit"};

/* prototypes */
typedef struct MoveToCollectionData MoveToCollectionData;
static void move_to_collection_menus_items(struct uiLayout *layout,
                                           struct MoveToCollectionData *menu);
static ListBase selected_objects_get(bContext *C);

/* -------------------------------------------------------------------- */
/** \name Internal Utilities
 * \{ */

Object *ED_object_context(bContext *C)
{
  return CTX_data_pointer_get_type(C, "object", &RNA_Object).data;
}

/* find the correct active object per context
 * note: context can be NULL when called from a enum with PROP_ENUM_NO_CONTEXT */
Object *ED_object_active_context(bContext *C)
{
  Object *ob = NULL;
  if (C) {
    ob = ED_object_context(C);
    if (!ob) {
      ob = CTX_data_active_object(C);
    }
  }
  return ob;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Hide Operator
 * \{ */

static bool object_hide_poll(bContext *C)
{
  if (CTX_wm_space_outliner(C) != NULL) {
    return ED_outliner_collections_editor_poll(C);
  }
  else {
    return ED_operator_view3d_active(C);
  }
}

static int object_hide_view_clear_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  const bool select = RNA_boolean_get(op->ptr, "select");
  bool changed = false;

  LISTBASE_FOREACH (Base *, base, &view_layer->object_bases) {
    if (base->flag & BASE_HIDDEN) {
      base->flag &= ~BASE_HIDDEN;
      changed = true;

      if (select) {
        /* We cannot call `ED_object_base_select` because
         * base is not selectable while it is hidden. */
        base->flag |= BASE_SELECTED;
        BKE_scene_object_base_flag_sync_from_base(base);
      }
    }
  }

  if (!changed) {
    return OPERATOR_CANCELLED;
  }

  BKE_layer_collection_sync(scene, view_layer);
  DEG_id_tag_update(&scene->id, ID_RECALC_BASE_FLAGS);
  WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_hide_view_clear(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Show Hidden Objects";
  ot->description = "Reveal temporarily hidden objects";
  ot->idname = "OBJECT_OT_hide_view_clear";

  /* api callbacks */
  ot->exec = object_hide_view_clear_exec;
  ot->poll = object_hide_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  PropertyRNA *prop = RNA_def_boolean(ot->srna, "select", true, "Select", "");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE | PROP_HIDDEN);
}

static int object_hide_view_set_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  const bool unselected = RNA_boolean_get(op->ptr, "unselected");
  bool changed = false;

  /* Hide selected or unselected objects. */
  LISTBASE_FOREACH (Base *, base, &view_layer->object_bases) {
    if (!(base->flag & BASE_VISIBLE_VIEWLAYER)) {
      continue;
    }

    if (!unselected) {
      if (base->flag & BASE_SELECTED) {
        ED_object_base_select(base, BA_DESELECT);
        base->flag |= BASE_HIDDEN;
        changed = true;
      }
    }
    else {
      if (!(base->flag & BASE_SELECTED)) {
        ED_object_base_select(base, BA_DESELECT);
        base->flag |= BASE_HIDDEN;
        changed = true;
      }
    }
  }
  if (!changed) {
    return OPERATOR_CANCELLED;
  }

  BKE_layer_collection_sync(scene, view_layer);
  DEG_id_tag_update(&scene->id, ID_RECALC_BASE_FLAGS);
  WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_hide_view_set(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Hide Objects";
  ot->description = "Temporarily hide objects from the viewport";
  ot->idname = "OBJECT_OT_hide_view_set";

  /* api callbacks */
  ot->exec = object_hide_view_set_exec;
  ot->poll = object_hide_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  PropertyRNA *prop;
  prop = RNA_def_boolean(
      ot->srna, "unselected", 0, "Unselected", "Hide unselected rather than selected objects");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE | PROP_HIDDEN);
}

static int object_hide_collection_exec(bContext *C, wmOperator *op)
{
  wmWindow *win = CTX_wm_window(C);
  View3D *v3d = CTX_wm_view3d(C);

  int index = RNA_int_get(op->ptr, "collection_index");
  const bool extend = (win->eventstate->shift != 0);
  const bool toggle = RNA_boolean_get(op->ptr, "toggle");

  if (win->eventstate->alt != 0) {
    index += 10;
  }

  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  LayerCollection *lc = BKE_layer_collection_from_index(view_layer, index);

  if (!lc) {
    return OPERATOR_CANCELLED;
  }

  DEG_id_tag_update(&scene->id, ID_RECALC_BASE_FLAGS);

  if (v3d->flag & V3D_LOCAL_COLLECTIONS) {
    if (lc->runtime_flag & LAYER_COLLECTION_RESTRICT_VIEWPORT) {
      return OPERATOR_CANCELLED;
    }
    if (toggle) {
      lc->local_collections_bits ^= v3d->local_collections_uuid;
      BKE_layer_collection_local_sync(view_layer, v3d);
    }
    else {
      BKE_layer_collection_isolate_local(view_layer, v3d, lc, extend);
    }
  }
  else {
    BKE_layer_collection_isolate_global(scene, view_layer, lc, extend);
  }

  WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);

  return OPERATOR_FINISHED;
}

#define COLLECTION_INVALID_INDEX -1

void ED_collection_hide_menu_draw(const bContext *C, uiLayout *layout)
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  LayerCollection *lc_scene = view_layer->layer_collections.first;

  uiLayoutSetOperatorContext(layout, WM_OP_EXEC_REGION_WIN);

  LISTBASE_FOREACH (LayerCollection *, lc, &lc_scene->layer_collections) {
    int index = BKE_layer_collection_findindex(view_layer, lc);
    uiLayout *row = uiLayoutRow(layout, false);

    if (lc->flag & LAYER_COLLECTION_EXCLUDE) {
      continue;
    }

    if (lc->collection->flag & COLLECTION_RESTRICT_VIEWPORT) {
      continue;
    }

    int icon = ICON_NONE;
    if (BKE_layer_collection_has_selected_objects(view_layer, lc)) {
      icon = ICON_LAYER_ACTIVE;
    }
    else if (lc->runtime_flag & LAYER_COLLECTION_HAS_OBJECTS) {
      icon = ICON_LAYER_USED;
    }

    uiItemIntO(row,
               lc->collection->id.name + 2,
               icon,
               "OBJECT_OT_hide_collection",
               "collection_index",
               index);
  }
}

static int object_hide_collection_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  /* Immediately execute if collection index was specified. */
  int index = RNA_int_get(op->ptr, "collection_index");
  if (index != COLLECTION_INVALID_INDEX) {
    return object_hide_collection_exec(C, op);
  }

  /* Open popup menu. */
  const char *title = CTX_IFACE_(op->type->translation_context, op->type->name);
  uiPopupMenu *pup = UI_popup_menu_begin(C, title, ICON_GROUP);
  uiLayout *layout = UI_popup_menu_layout(pup);

  ED_collection_hide_menu_draw(C, layout);

  UI_popup_menu_end(C, pup);

  return OPERATOR_INTERFACE;
}

void OBJECT_OT_hide_collection(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Hide Collection";
  ot->description = "Show only objects in collection (Shift to extend)";
  ot->idname = "OBJECT_OT_hide_collection";

  /* api callbacks */
  ot->exec = object_hide_collection_exec;
  ot->invoke = object_hide_collection_invoke;
  ot->poll = ED_operator_view3d_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* Properties. */
  PropertyRNA *prop;
  prop = RNA_def_int(ot->srna,
                     "collection_index",
                     COLLECTION_INVALID_INDEX,
                     COLLECTION_INVALID_INDEX,
                     INT_MAX,
                     "Collection Index",
                     "Index of the collection to change visibility",
                     0,
                     INT_MAX);
  RNA_def_property_flag(prop, PROP_SKIP_SAVE | PROP_HIDDEN);
  prop = RNA_def_boolean(ot->srna, "toggle", 0, "Toggle", "Toggle visibility");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE | PROP_HIDDEN);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Toggle Edit-Mode Operator
 * \{ */

static bool mesh_needs_keyindex(Main *bmain, const Mesh *me)
{
  if (me->key) {
    return false; /* will be added */
  }

  for (const Object *ob = bmain->objects.first; ob; ob = ob->id.next) {
    if ((ob->parent) && (ob->parent->data == me) && ELEM(ob->partype, PARVERT1, PARVERT3)) {
      return true;
    }
    if (ob->data == me) {
      LISTBASE_FOREACH (const ModifierData *, md, &ob->modifiers) {
        if (md->type == eModifierType_Hook) {
          return true;
        }
      }
    }
  }
  return false;
}

/**
 * Load EditMode data back into the object,
 * optionally freeing the editmode data.
 */
static bool ED_object_editmode_load_ex(Main *bmain, Object *obedit, const bool freedata)
{
  if (obedit == NULL) {
    return false;
  }

  if (obedit->type == OB_MESH) {
    Mesh *me = obedit->data;
    if (me->edit_mesh == NULL) {
      return false;
    }

    if (me->edit_mesh->bm->totvert > MESH_MAX_VERTS) {
      /* This used to be warned int the UI, we could warn again although it's quite rare. */
      CLOG_WARN(&LOG,
                "Too many vertices for mesh '%s' (%d)",
                me->id.name + 2,
                me->edit_mesh->bm->totvert);
      return false;
    }

    EDBM_mesh_load_ex(bmain, obedit, freedata);

    if (freedata) {
      EDBM_mesh_free(me->edit_mesh);
      MEM_freeN(me->edit_mesh);
      me->edit_mesh = NULL;
    }
    /* will be recalculated as needed. */
    {
      ED_mesh_mirror_spatial_table_end(obedit);
      ED_mesh_mirror_topo_table_end(obedit);
    }
  }
  else if (obedit->type == OB_ARMATURE) {
    const bArmature *arm = obedit->data;
    if (arm->edbo == NULL) {
      return false;
    }
    ED_armature_from_edit(bmain, obedit->data);
    if (freedata) {
      ED_armature_edit_free(obedit->data);
    }
    /* TODO(sergey): Pose channels might have been changed, so need
     * to inform dependency graph about this. But is it really the
     * best place to do this?
     */
    DEG_relations_tag_update(bmain);
  }
  else if (ELEM(obedit->type, OB_CURVE, OB_SURF)) {
    const Curve *cu = obedit->data;
    if (cu->editnurb == NULL) {
      return false;
    }
    ED_curve_editnurb_load(bmain, obedit);
    if (freedata) {
      ED_curve_editnurb_free(obedit);
    }
  }
  else if (obedit->type == OB_FONT) {
    const Curve *cu = obedit->data;
    if (cu->editfont == NULL) {
      return false;
    }
    ED_curve_editfont_load(obedit);
    if (freedata) {
      ED_curve_editfont_free(obedit);
    }
  }
  else if (obedit->type == OB_LATTICE) {
    const Lattice *lt = obedit->data;
    if (lt->editlatt == NULL) {
      return false;
    }
    BKE_editlattice_load(obedit);
    if (freedata) {
      BKE_editlattice_free(obedit);
    }
  }
  else if (obedit->type == OB_MBALL) {
    const MetaBall *mb = obedit->data;
    if (mb->editelems == NULL) {
      return false;
    }
    ED_mball_editmball_load(obedit);
    if (freedata) {
      ED_mball_editmball_free(obedit);
    }
  }
  else {
    return false;
  }

  char *needs_flush_ptr = BKE_object_data_editmode_flush_ptr_get(obedit->data);
  if (needs_flush_ptr) {
    *needs_flush_ptr = false;
  }

  return true;
}

bool ED_object_editmode_load(Main *bmain, Object *obedit)
{
  return ED_object_editmode_load_ex(bmain, obedit, false);
}

/**
 * \param flag:
 * - If #EM_FREEDATA isn't in the flag, use ED_object_editmode_load directly.
 */
bool ED_object_editmode_exit_ex(Main *bmain, Scene *scene, Object *obedit, int flag)
{
  const bool freedata = (flag & EM_FREEDATA) != 0;

  if (ED_object_editmode_load_ex(bmain, obedit, freedata) == false) {
    /* in rare cases (background mode) its possible active object
     * is flagged for editmode, without 'obedit' being set [#35489] */
    if (UNLIKELY(obedit && obedit->mode & OB_MODE_EDIT)) {
      obedit->mode &= ~OB_MODE_EDIT;
      /* Also happens when mesh is shared across multiple objects. [#T69834] */
      DEG_id_tag_update(&obedit->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
    }
    return true;
  }

  /* freedata only 0 now on file saves and render */
  if (freedata) {
    ListBase pidlist;
    PTCacheID *pid;

    /* flag object caches as outdated */
    BKE_ptcache_ids_from_object(&pidlist, obedit, scene, 0);
    for (pid = pidlist.first; pid; pid = pid->next) {
      /* particles don't need reset on geometry change */
      if (pid->type != PTCACHE_TYPE_PARTICLES) {
        pid->cache->flag |= PTCACHE_OUTDATED;
      }
    }
    BLI_freelistN(&pidlist);

    BKE_particlesystem_reset_all(obedit);
    BKE_ptcache_object_reset(scene, obedit, PTCACHE_RESET_OUTDATED);

    /* also flush ob recalc, doesn't take much overhead, but used for particles */
    DEG_id_tag_update(&obedit->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);

    WM_main_add_notifier(NC_SCENE | ND_MODE | NS_MODE_OBJECT, scene);

    obedit->mode &= ~OB_MODE_EDIT;
  }

  return (obedit->mode & OB_MODE_EDIT) == 0;
}

bool ED_object_editmode_exit(bContext *C, int flag)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  Object *obedit = CTX_data_edit_object(C);
  return ED_object_editmode_exit_ex(bmain, scene, obedit, flag);
}

bool ED_object_editmode_enter_ex(Main *bmain, Scene *scene, Object *ob, int flag)
{
  bool ok = false;

  if (ELEM(NULL, ob, ob->data) || ID_IS_LINKED(ob)) {
    return false;
  }

  /* this checks actual object->data, for cases when other scenes have it in editmode context */
  if (BKE_object_is_in_editmode(ob)) {
    return true;
  }

  if (BKE_object_obdata_is_libdata(ob)) {
    /* Ideally the caller should check this. */
    CLOG_WARN(&LOG, "Unable to enter edit-mode on library data for object '%s'", ob->id.name + 2);
    return false;
  }

  ob->restore_mode = ob->mode;

  ob->mode = OB_MODE_EDIT;

  if (ob->type == OB_MESH) {
    BMEditMesh *em;
    ok = 1;

    const bool use_key_index = mesh_needs_keyindex(bmain, ob->data);

    EDBM_mesh_make(ob, scene->toolsettings->selectmode, use_key_index);

    em = BKE_editmesh_from_object(ob);
    if (LIKELY(em)) {
      /* order doesn't matter */
      EDBM_mesh_normals_update(em);
      BKE_editmesh_looptri_calc(em);
    }

    WM_main_add_notifier(NC_SCENE | ND_MODE | NS_EDITMODE_MESH, NULL);
  }
  else if (ob->type == OB_ARMATURE) {
    bArmature *arm = ob->data;
    ok = 1;
    ED_armature_to_edit(arm);
    /* To ensure all goes in rest-position and without striding. */

    arm->needs_flush_to_id = 0;

    /* XXX: should this be ID_RECALC_GEOMETRY? */
    DEG_id_tag_update(&ob->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY | ID_RECALC_ANIMATION);

    WM_main_add_notifier(NC_SCENE | ND_MODE | NS_EDITMODE_ARMATURE, scene);
  }
  else if (ob->type == OB_FONT) {
    ok = 1;
    ED_curve_editfont_make(ob);

    WM_main_add_notifier(NC_SCENE | ND_MODE | NS_EDITMODE_TEXT, scene);
  }
  else if (ob->type == OB_MBALL) {
    MetaBall *mb = ob->data;

    ok = 1;
    ED_mball_editmball_make(ob);

    mb->needs_flush_to_id = 0;

    WM_main_add_notifier(NC_SCENE | ND_MODE | NS_EDITMODE_MBALL, scene);
  }
  else if (ob->type == OB_LATTICE) {
    ok = 1;
    BKE_editlattice_make(ob);

    WM_main_add_notifier(NC_SCENE | ND_MODE | NS_EDITMODE_LATTICE, scene);
  }
  else if (ob->type == OB_SURF || ob->type == OB_CURVE) {
    ok = 1;
    ED_curve_editnurb_make(ob);

    WM_main_add_notifier(NC_SCENE | ND_MODE | NS_EDITMODE_CURVE, scene);
  }

  if (ok) {
    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  }
  else {
    if ((flag & EM_NO_CONTEXT) == 0) {
      ob->mode &= ~OB_MODE_EDIT;
    }
    WM_main_add_notifier(NC_SCENE | ND_MODE | NS_MODE_OBJECT, scene);
  }

  return (ob->mode & OB_MODE_EDIT) != 0;
}

bool ED_object_editmode_enter(bContext *C, int flag)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  Object *ob;

  /* Active layer checked here for view3d,
   * callers that don't want view context can call the extended version. */
  ob = CTX_data_active_object(C);
  if ((ob == NULL) || ID_IS_LINKED(ob)) {
    return false;
  }
  return ED_object_editmode_enter_ex(bmain, scene, ob, flag);
}

static int editmode_toggle_exec(bContext *C, wmOperator *op)
{
  struct wmMsgBus *mbus = CTX_wm_message_bus(C);
  const int mode_flag = OB_MODE_EDIT;
  const bool is_mode_set = (CTX_data_edit_object(C) != NULL);
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  View3D *v3d = CTX_wm_view3d(C);
  Object *obact = OBACT(view_layer);

  if (!is_mode_set) {
    if (!ED_object_mode_compat_set(C, obact, mode_flag, op->reports)) {
      return OPERATOR_CANCELLED;
    }
  }

  if (!is_mode_set) {
    ED_object_editmode_enter(C, 0);
    if (obact->mode & mode_flag) {
      FOREACH_SELECTED_OBJECT_BEGIN (view_layer, v3d, ob) {
        if ((ob != obact) && (ob->type == obact->type)) {
          ED_object_editmode_enter_ex(bmain, scene, ob, EM_NO_CONTEXT);
        }
      }
      FOREACH_SELECTED_OBJECT_END;
    }
  }
  else {
    ED_object_editmode_exit(C, EM_FREEDATA);
    if ((obact->mode & mode_flag) == 0) {
      FOREACH_OBJECT_BEGIN (view_layer, ob) {
        if ((ob != obact) && (ob->type == obact->type)) {
          ED_object_editmode_exit_ex(bmain, scene, ob, EM_FREEDATA);
        }
      }
      FOREACH_OBJECT_END;
    }
  }

  WM_msg_publish_rna_prop(mbus, &obact->id, obact, Object, mode);

  if (G.background == false) {
    WM_toolsystem_update_from_context_view3d(C);
  }

  return OPERATOR_FINISHED;
}

static bool editmode_toggle_poll(bContext *C)
{
  Object *ob = CTX_data_active_object(C);

  /* covers proxies too */
  if (ELEM(NULL, ob, ob->data) || ID_IS_LINKED(ob->data)) {
    return 0;
  }

  /* if hidden but in edit mode, we still display */
  if ((ob->restrictflag & OB_RESTRICT_VIEWPORT) && !(ob->mode & OB_MODE_EDIT)) {
    return 0;
  }

  return OB_TYPE_SUPPORT_EDITMODE(ob->type);
}

void OBJECT_OT_editmode_toggle(wmOperatorType *ot)
{

  /* identifiers */
  ot->name = "Toggle Editmode";
  ot->description = "Toggle object's editmode";
  ot->idname = "OBJECT_OT_editmode_toggle";

  /* api callbacks */
  ot->exec = editmode_toggle_exec;
  ot->poll = editmode_toggle_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Toggle Pose-Mode Operator
 * \{ */

static int posemode_exec(bContext *C, wmOperator *op)
{
  struct wmMsgBus *mbus = CTX_wm_message_bus(C);
  Base *base = CTX_data_active_base(C);

  /* If the base is NULL it means we have an active object, but the object itself is hidden. */
  if (base == NULL) {
    return OPERATOR_CANCELLED;
  }

  Object *obact = base->object;
  const int mode_flag = OB_MODE_POSE;
  bool is_mode_set = (obact->mode & mode_flag) != 0;

  if (!is_mode_set) {
    if (!ED_object_mode_compat_set(C, obact, mode_flag, op->reports)) {
      return OPERATOR_CANCELLED;
    }
  }

  if (obact->type != OB_ARMATURE) {
    return OPERATOR_PASS_THROUGH;
  }

  if (obact == CTX_data_edit_object(C)) {
    ED_object_editmode_exit(C, EM_FREEDATA);
    is_mode_set = false;
  }

  if (is_mode_set) {
    bool ok = ED_object_posemode_exit(C, obact);
    if (ok) {
      struct Main *bmain = CTX_data_main(C);
      ViewLayer *view_layer = CTX_data_view_layer(C);
      FOREACH_OBJECT_BEGIN (view_layer, ob) {
        if ((ob != obact) && (ob->type == OB_ARMATURE) && (ob->mode & mode_flag)) {
          ED_object_posemode_exit_ex(bmain, ob);
        }
      }
      FOREACH_OBJECT_END;
    }
  }
  else {
    bool ok = ED_object_posemode_enter(C, obact);
    if (ok) {
      struct Main *bmain = CTX_data_main(C);
      ViewLayer *view_layer = CTX_data_view_layer(C);
      View3D *v3d = CTX_wm_view3d(C);
      FOREACH_SELECTED_OBJECT_BEGIN (view_layer, v3d, ob) {
        if ((ob != obact) && (ob->type == OB_ARMATURE) && (ob->mode == OB_MODE_OBJECT) &&
            (!ID_IS_LINKED(ob))) {
          ED_object_posemode_enter_ex(bmain, ob);
        }
      }
      FOREACH_SELECTED_OBJECT_END;
    }
  }

  WM_msg_publish_rna_prop(mbus, &obact->id, obact, Object, mode);

  if (G.background == false) {
    WM_toolsystem_update_from_context_view3d(C);
  }

  return OPERATOR_FINISHED;
}

void OBJECT_OT_posemode_toggle(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Toggle Pose Mode";
  ot->idname = "OBJECT_OT_posemode_toggle";
  ot->description = "Enable or disable posing/selecting bones";

  /* api callbacks */
  ot->exec = posemode_exec;
  ot->poll = ED_operator_object_active_editable;

  /* flag */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Force Field Toggle Operator
 * \{ */

void ED_object_check_force_modifiers(Main *bmain, Scene *scene, Object *object)
{
  PartDeflect *pd = object->pd;
  ModifierData *md = BKE_modifiers_findby_type(object, eModifierType_Surface);

  /* add/remove modifier as needed */
  if (!md) {
    if (pd && (pd->shape == PFIELD_SHAPE_SURFACE) &&
        !ELEM(pd->forcefield, 0, PFIELD_GUIDE, PFIELD_TEXTURE)) {
      if (ELEM(object->type, OB_MESH, OB_SURF, OB_FONT, OB_CURVE)) {
        ED_object_modifier_add(NULL, bmain, scene, object, NULL, eModifierType_Surface);
      }
    }
  }
  else {
    if (!pd || (pd->shape != PFIELD_SHAPE_SURFACE) ||
        ELEM(pd->forcefield, 0, PFIELD_GUIDE, PFIELD_TEXTURE)) {
      ED_object_modifier_remove(NULL, bmain, object, md);
    }
  }
}

static int forcefield_toggle_exec(bContext *C, wmOperator *UNUSED(op))
{
  Object *ob = CTX_data_active_object(C);

  if (ob->pd == NULL) {
    ob->pd = BKE_partdeflect_new(PFIELD_FORCE);
  }
  else if (ob->pd->forcefield == 0) {
    ob->pd->forcefield = PFIELD_FORCE;
  }
  else {
    ob->pd->forcefield = 0;
  }

  ED_object_check_force_modifiers(CTX_data_main(C), CTX_data_scene(C), ob);
  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);
  WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);

  DEG_id_tag_update(&ob->id, ID_RECALC_TRANSFORM);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_forcefield_toggle(wmOperatorType *ot)
{

  /* identifiers */
  ot->name = "Toggle Force Field";
  ot->description = "Toggle object's force field";
  ot->idname = "OBJECT_OT_forcefield_toggle";

  /* api callbacks */
  ot->exec = forcefield_toggle_exec;
  ot->poll = ED_operator_object_active_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Calculate Motion Paths Operator
 * \{ */

static eAnimvizCalcRange object_path_convert_range(eObjectPathCalcRange range)
{
  switch (range) {
    case OBJECT_PATH_CALC_RANGE_CURRENT_FRAME:
      return ANIMVIZ_CALC_RANGE_CURRENT_FRAME;
    case OBJECT_PATH_CALC_RANGE_CHANGED:
      return ANIMVIZ_CALC_RANGE_CHANGED;
    case OBJECT_PATH_CALC_RANGE_FULL:
      return ANIMVIZ_CALC_RANGE_FULL;
  }
  return ANIMVIZ_CALC_RANGE_FULL;
}

/* For the objects with animation: update paths for those that have got them
 * This should selectively update paths that exist...
 *
 * To be called from various tools that do incremental updates
 */
void ED_objects_recalculate_paths(bContext *C, Scene *scene, eObjectPathCalcRange range)
{
  /* Transform doesn't always have context available to do update. */
  if (C == NULL) {
    return;
  }

  Main *bmain = CTX_data_main(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);

  ListBase targets = {NULL, NULL};
  /* loop over objects in scene */
  CTX_DATA_BEGIN (C, Object *, ob, selected_editable_objects) {
    /* set flag to force recalc, then grab path(s) from object */
    ob->avs.recalc |= ANIMVIZ_RECALC_PATHS;
    animviz_get_object_motionpaths(ob, &targets);
  }
  CTX_DATA_END;

  Depsgraph *depsgraph;
  bool free_depsgraph = false;
  /* For a single frame update it's faster to re-use existing dependency graph and avoid overhead
   * of building all the relations and so on for a temporary one.  */
  if (range == OBJECT_PATH_CALC_RANGE_CURRENT_FRAME) {
    /* NOTE: Dependency graph will be evaluated at all the frames, but we first need to access some
     * nested pointers, like animation data. */
    depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
    free_depsgraph = false;
  }
  else {
    depsgraph = animviz_depsgraph_build(bmain, scene, view_layer, &targets);
    free_depsgraph = true;
  }

  /* recalculate paths, then free */
  animviz_calc_motionpaths(
      depsgraph, bmain, scene, &targets, object_path_convert_range(range), true);
  BLI_freelistN(&targets);

  if (range != OBJECT_PATH_CALC_RANGE_CURRENT_FRAME) {
    /* Tag objects for copy on write - so paths will draw/redraw
     * For currently frame only we update evaluated object directly. */
    CTX_DATA_BEGIN (C, Object *, ob, selected_editable_objects) {
      if (ob->mpath) {
        DEG_id_tag_update(&ob->id, ID_RECALC_COPY_ON_WRITE);
      }
    }
    CTX_DATA_END;
  }

  /* Free temporary depsgraph. */
  if (free_depsgraph) {
    DEG_graph_free(depsgraph);
  }
}

/* show popup to determine settings */
static int object_calculate_paths_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  Object *ob = CTX_data_active_object(C);

  if (ob == NULL) {
    return OPERATOR_CANCELLED;
  }

  /* set default settings from existing/stored settings */
  {
    bAnimVizSettings *avs = &ob->avs;

    RNA_int_set(op->ptr, "start_frame", avs->path_sf);
    RNA_int_set(op->ptr, "end_frame", avs->path_ef);
  }

  /* show popup dialog to allow editing of range... */
  /* FIXME: hard-coded dimensions here are just arbitrary. */
  return WM_operator_props_dialog_popup(C, op, 200);
}

/* Calculate/recalculate whole paths (avs.path_sf to avs.path_ef) */
static int object_calculate_paths_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  int start = RNA_int_get(op->ptr, "start_frame");
  int end = RNA_int_get(op->ptr, "end_frame");

  /* set up path data for bones being calculated */
  CTX_DATA_BEGIN (C, Object *, ob, selected_editable_objects) {
    bAnimVizSettings *avs = &ob->avs;

    /* grab baking settings from operator settings */
    avs->path_sf = start;
    avs->path_ef = end;

    /* verify that the selected object has the appropriate settings */
    animviz_verify_motionpaths(op->reports, scene, ob, NULL);
  }
  CTX_DATA_END;

  /* calculate the paths for objects that have them (and are tagged to get refreshed) */
  ED_objects_recalculate_paths(C, scene, OBJECT_PATH_CALC_RANGE_FULL);

  /* notifiers for updates */
  WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, NULL);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_paths_calculate(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Calculate Object Paths";
  ot->idname = "OBJECT_OT_paths_calculate";
  ot->description = "Calculate motion paths for the selected objects";

  /* api callbacks */
  ot->invoke = object_calculate_paths_invoke;
  ot->exec = object_calculate_paths_exec;
  ot->poll = ED_operator_object_active_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  RNA_def_int(ot->srna,
              "start_frame",
              1,
              MINAFRAME,
              MAXFRAME,
              "Start",
              "First frame to calculate object paths on",
              MINFRAME,
              MAXFRAME / 2.0);
  RNA_def_int(ot->srna,
              "end_frame",
              250,
              MINAFRAME,
              MAXFRAME,
              "End",
              "Last frame to calculate object paths on",
              MINFRAME,
              MAXFRAME / 2.0);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Update Motion Paths Operator
 * \{ */

static bool object_update_paths_poll(bContext *C)
{
  if (ED_operator_object_active_editable(C)) {
    Object *ob = ED_object_active_context(C);
    return (ob->avs.path_bakeflag & MOTIONPATH_BAKE_HAS_PATHS) != 0;
  }

  return false;
}

static int object_update_paths_exec(bContext *C, wmOperator *UNUSED(op))
{
  Scene *scene = CTX_data_scene(C);

  if (scene == NULL) {
    return OPERATOR_CANCELLED;
  }

  /* calculate the paths for objects that have them (and are tagged to get refreshed) */
  ED_objects_recalculate_paths(C, scene, OBJECT_PATH_CALC_RANGE_FULL);

  /* notifiers for updates */
  WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, NULL);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_paths_update(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Update Object Paths";
  ot->idname = "OBJECT_OT_paths_update";
  ot->description = "Recalculate paths for selected objects";

  /* api callbacks */
  ot->exec = object_update_paths_exec;
  ot->poll = object_update_paths_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Clear Motion Paths Operator
 * \{ */

/* Helper for ED_objects_clear_paths() */
static void object_clear_mpath(Object *ob)
{
  if (ob->mpath) {
    animviz_free_motionpath(ob->mpath);
    ob->mpath = NULL;
    ob->avs.path_bakeflag &= ~MOTIONPATH_BAKE_HAS_PATHS;

    /* tag object for copy on write - so removed paths don't still show */
    DEG_id_tag_update(&ob->id, ID_RECALC_COPY_ON_WRITE);
  }
}

/* Clear motion paths for all objects */
void ED_objects_clear_paths(bContext *C, bool only_selected)
{
  if (only_selected) {
    /* Loop over all selected + editable objects in scene. */
    CTX_DATA_BEGIN (C, Object *, ob, selected_editable_objects) {
      object_clear_mpath(ob);
    }
    CTX_DATA_END;
  }
  else {
    /* Loop over all editable objects in scene. */
    CTX_DATA_BEGIN (C, Object *, ob, editable_objects) {
      object_clear_mpath(ob);
    }
    CTX_DATA_END;
  }
}

/* operator callback for this */
static int object_clear_paths_exec(bContext *C, wmOperator *op)
{
  bool only_selected = RNA_boolean_get(op->ptr, "only_selected");

  /* use the backend function for this */
  ED_objects_clear_paths(C, only_selected);

  /* notifiers for updates */
  WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, NULL);

  return OPERATOR_FINISHED;
}

/* operator callback/wrapper */
static int object_clear_paths_invoke(bContext *C, wmOperator *op, const wmEvent *evt)
{
  if ((evt->shift) && !RNA_struct_property_is_set(op->ptr, "only_selected")) {
    RNA_boolean_set(op->ptr, "only_selected", true);
  }
  return object_clear_paths_exec(C, op);
}

void OBJECT_OT_paths_clear(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Clear Object Paths";
  ot->idname = "OBJECT_OT_paths_clear";
  ot->description = "Clear path caches for all objects, hold Shift key for selected objects only";

  /* api callbacks */
  ot->invoke = object_clear_paths_invoke;
  ot->exec = object_clear_paths_exec;
  ot->poll = ED_operator_object_active_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  ot->prop = RNA_def_boolean(
      ot->srna, "only_selected", false, "Only Selected", "Only clear paths from selected objects");
  RNA_def_property_flag(ot->prop, PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Update Motion Paths Range from Scene Operator
 * \{ */

static int object_update_paths_range_exec(bContext *C, wmOperator *UNUSED(op))
{
  Scene *scene = CTX_data_scene(C);

  /* Loop over all editable objects in scene. */
  CTX_DATA_BEGIN (C, Object *, ob, editable_objects) {
    /* use Preview Range or Full Frame Range - whichever is in use */
    ob->avs.path_sf = PSFRA;
    ob->avs.path_ef = PEFRA;

    /* tag for updates */
    DEG_id_tag_update(&ob->id, ID_RECALC_COPY_ON_WRITE);
    WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, NULL);
  }
  CTX_DATA_END;

  return OPERATOR_FINISHED;
}

void OBJECT_OT_paths_range_update(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Update Range from Scene";
  ot->idname = "OBJECT_OT_paths_range_update";
  ot->description = "Update frame range for motion paths from the Scene's current frame range";

  /* callbacks */
  ot->exec = object_update_paths_range_exec;
  ot->poll = ED_operator_object_active_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Object Shade Smooth/Flat Operator
 * \{ */

static int shade_smooth_exec(bContext *C, wmOperator *op)
{
  const bool use_smooth = STREQ(op->idname, "OBJECT_OT_shade_smooth");
  bool changed_multi = false;
  bool has_linked_data = false;

  ListBase ctx_objects = {NULL, NULL};
  CollectionPointerLink ctx_ob_single_active = {NULL};

  /* For modes that only use an active object, don't handle the whole selection. */
  {
    ViewLayer *view_layer = CTX_data_view_layer(C);
    Object *obact = OBACT(view_layer);
    if (obact && ((obact->mode & OB_MODE_ALL_PAINT))) {
      ctx_ob_single_active.ptr.data = obact;
      BLI_addtail(&ctx_objects, &ctx_ob_single_active);
    }
  }

  if (ctx_objects.first != &ctx_ob_single_active) {
    CTX_data_selected_editable_objects(C, &ctx_objects);
  }

  for (CollectionPointerLink *ctx_ob = ctx_objects.first; ctx_ob; ctx_ob = ctx_ob->next) {
    Object *ob = ctx_ob->ptr.data;
    ID *data = ob->data;
    if (data != NULL) {
      data->tag |= LIB_TAG_DOIT;
    }
  }

  for (CollectionPointerLink *ctx_ob = ctx_objects.first; ctx_ob; ctx_ob = ctx_ob->next) {
    /* Always un-tag all object data-blocks irrespective of our ability to operate on them. */
    Object *ob = ctx_ob->ptr.data;
    ID *data = ob->data;
    if ((data == NULL) || ((data->tag & LIB_TAG_DOIT) == 0)) {
      continue;
    }
    data->tag &= ~LIB_TAG_DOIT;
    /* Finished un-tagging, continue with regular logic. */

    if (data && ID_IS_LINKED(data)) {
      has_linked_data = true;
      continue;
    }

    bool changed = false;
    if (ob->type == OB_MESH) {
      BKE_mesh_smooth_flag_set(ob->data, use_smooth);
      BKE_mesh_batch_cache_dirty_tag(ob->data, BKE_MESH_BATCH_DIRTY_ALL);
      changed = true;
    }
    else if (ELEM(ob->type, OB_SURF, OB_CURVE)) {
      BKE_curve_smooth_flag_set(ob->data, use_smooth);
      changed = true;
    }

    if (changed) {
      changed_multi = true;

      DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
      WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);
    }
  }

  if (ctx_objects.first != &ctx_ob_single_active) {
    BLI_freelistN(&ctx_objects);
  }

  if (has_linked_data) {
    BKE_report(op->reports, RPT_WARNING, "Can't edit linked mesh or curve data");
  }

  return (changed_multi) ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

static bool shade_poll(bContext *C)
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Object *obact = OBACT(view_layer);
  if (obact != NULL) {
    /* Doesn't handle edit-data, sculpt dynamic-topology, or their undo systems. */
    if (obact->mode & (OB_MODE_EDIT | OB_MODE_SCULPT)) {
      return false;
    }
  }
  return true;
}

void OBJECT_OT_shade_flat(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Shade Flat";
  ot->description = "Render and display faces uniform, using Face Normals";
  ot->idname = "OBJECT_OT_shade_flat";

  /* api callbacks */
  ot->poll = shade_poll;
  ot->exec = shade_smooth_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

void OBJECT_OT_shade_smooth(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Shade Smooth";
  ot->description = "Render and display faces smooth, using interpolated Vertex Normals";
  ot->idname = "OBJECT_OT_shade_smooth";

  /* api callbacks */
  ot->poll = shade_poll;
  ot->exec = shade_smooth_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Object Mode Set Operator
 * \{ */

static const EnumPropertyItem *object_mode_set_itemsf(bContext *C,
                                                      PointerRNA *UNUSED(ptr),
                                                      PropertyRNA *UNUSED(prop),
                                                      bool *r_free)
{
  const EnumPropertyItem *input = rna_enum_object_mode_items;
  EnumPropertyItem *item = NULL;
  Object *ob;
  int totitem = 0;

  if (!C) { /* needed for docs */
    return rna_enum_object_mode_items;
  }

  ob = CTX_data_active_object(C);
  if (ob) {
    const bool use_mode_particle_edit = (BLI_listbase_is_empty(&ob->particlesystem) == false) ||
                                        (ob->soft != NULL) ||
                                        (BKE_modifiers_findby_type(ob, eModifierType_Cloth) !=
                                         NULL);
    while (input->identifier) {
      if ((input->value == OB_MODE_EDIT && OB_TYPE_SUPPORT_EDITMODE(ob->type)) ||
          (input->value == OB_MODE_POSE && (ob->type == OB_ARMATURE)) ||
          (input->value == OB_MODE_PARTICLE_EDIT && use_mode_particle_edit) ||
          (ELEM(input->value,
                OB_MODE_SCULPT,
                OB_MODE_VERTEX_PAINT,
                OB_MODE_WEIGHT_PAINT,
                OB_MODE_TEXTURE_PAINT) &&
           (ob->type == OB_MESH)) ||
          (ELEM(input->value,
                OB_MODE_EDIT_GPENCIL,
                OB_MODE_PAINT_GPENCIL,
                OB_MODE_SCULPT_GPENCIL,
                OB_MODE_WEIGHT_GPENCIL,
                OB_MODE_VERTEX_GPENCIL) &&
           (ob->type == OB_GPENCIL)) ||
          (input->value == OB_MODE_OBJECT)) {
        RNA_enum_item_add(&item, &totitem, input);
      }
      input++;
    }
  }
  else {
    /* We need at least this one! */
    RNA_enum_items_add_value(&item, &totitem, input, OB_MODE_OBJECT);
  }

  RNA_enum_item_end(&item, &totitem);

  *r_free = true;

  return item;
}

static bool object_mode_set_poll(bContext *C)
{
  /* Needed as #ED_operator_object_active_editable doesn't call use 'active_object'. */
  Object *ob = CTX_data_active_object(C);
  return ED_operator_object_active_editable_ex(C, ob);
}

static int object_mode_set_exec(bContext *C, wmOperator *op)
{
  const bool use_submode = STREQ(op->idname, "OBJECT_OT_mode_set_with_submode");
  Object *ob = CTX_data_active_object(C);
  eObjectMode mode = RNA_enum_get(op->ptr, "mode");
  const bool toggle = RNA_boolean_get(op->ptr, "toggle");

  /* by default the operator assume is a mesh, but if gp object change mode */
  if ((ob->type == OB_GPENCIL) && (mode == OB_MODE_EDIT)) {
    mode = OB_MODE_EDIT_GPENCIL;
  }

  if (!ED_object_mode_compat_test(ob, mode)) {
    return OPERATOR_PASS_THROUGH;
  }

  /**
   * Mode Switching Logic (internal details).
   *
   * Notes:
   * - Code below avoids calling mode switching functions more than once,
   *   as this causes unnecessary calculations and undo steps to be added.
   * - The previous mode (#Object.restore_mode) is object mode by default.
   *
   * Supported Cases:
   * - Setting the mode (when the 'toggle' setting is off).
   * - Toggle the mode:
   *   - Toggle between object mode and non-object mode property.
   *   - Toggle between the previous mode (#Object.restore_mode) and the mode property.
   *   - Toggle object mode.
   *     While this is similar to regular toggle,
   *     this operator depends on there being a previous mode set
   *     (this isn't bound to a key with the default key-map).
   */
  if (toggle == false) {
    if (ob->mode != mode) {
      ED_object_mode_set_ex(C, mode, true, op->reports);
    }
  }
  else {
    const eObjectMode mode_prev = ob->mode;
    /* When toggling object mode, we always use the restore mode,
     * otherwise there is nothing to do. */
    if (mode == OB_MODE_OBJECT) {
      if (ob->mode != OB_MODE_OBJECT) {
        if (ED_object_mode_set_ex(C, OB_MODE_OBJECT, true, op->reports)) {
          /* Store old mode so we know what to go back to. */
          ob->restore_mode = mode_prev;
        }
      }
      else {
        if (ob->restore_mode != OB_MODE_OBJECT) {
          ED_object_mode_set_ex(C, ob->restore_mode, true, op->reports);
        }
      }
    }
    else {
      /* Non-object modes, enter the 'mode' unless it's already set,
       * in that case use restore mode. */
      if (ob->mode != mode) {
        if (ED_object_mode_set_ex(C, mode, true, op->reports)) {
          /* Store old mode so we know what to go back to. */
          ob->restore_mode = mode_prev;
        }
      }
      else {
        if (ob->restore_mode != OB_MODE_OBJECT) {
          ED_object_mode_set_ex(C, ob->restore_mode, true, op->reports);
        }
        else {
          ED_object_mode_set_ex(C, OB_MODE_OBJECT, true, op->reports);
        }
      }
    }
  }

  if (use_submode) {
    if (ob->type == OB_MESH) {
      if (ob->mode & OB_MODE_EDIT) {
        PropertyRNA *prop = RNA_struct_find_property(op->ptr, "mesh_select_mode");
        if (RNA_property_is_set(op->ptr, prop)) {
          int mesh_select_mode = RNA_property_enum_get(op->ptr, prop);
          if (mesh_select_mode != 0) {
            EDBM_selectmode_set_multi(C, mesh_select_mode);
          }
        }
      }
    }
  }

  return OPERATOR_FINISHED;
}

void OBJECT_OT_mode_set(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Set Object Mode";
  ot->description = "Sets the object interaction mode";
  ot->idname = "OBJECT_OT_mode_set";

  /* api callbacks */
  ot->exec = object_mode_set_exec;
  ot->poll = object_mode_set_poll;

  /* flags */
  ot->flag = 0; /* no register/undo here, leave it to operators being called */

  ot->prop = RNA_def_enum(
      ot->srna, "mode", rna_enum_object_mode_items, OB_MODE_OBJECT, "Mode", "");
  RNA_def_enum_funcs(ot->prop, object_mode_set_itemsf);
  RNA_def_property_flag(ot->prop, PROP_SKIP_SAVE);

  prop = RNA_def_boolean(ot->srna, "toggle", 0, "Toggle", "");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

void OBJECT_OT_mode_set_with_submode(wmOperatorType *ot)
{
  OBJECT_OT_mode_set(ot);

  /* identifiers */
  ot->name = "Set Object Mode with Submode";
  ot->idname = "OBJECT_OT_mode_set_with_submode";

  /* properties */
  /* we could add other types - particle for eg. */
  PropertyRNA *prop;
  prop = RNA_def_enum_flag(
      ot->srna, "mesh_select_mode", rna_enum_mesh_select_mode_items, 0, "Mesh Mode", "");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Object Link/Move to Collection Operator
 * \{ */

static ListBase selected_objects_get(bContext *C)
{
  ListBase objects = {NULL};

  if (CTX_wm_space_outliner(C) != NULL) {
    ED_outliner_selected_objects_get(C, &objects);
  }
  else {
    CTX_DATA_BEGIN (C, Object *, ob, selected_objects) {
      BLI_addtail(&objects, BLI_genericNodeN(ob));
    }
    CTX_DATA_END;
  }

  return objects;
}

static bool move_to_collection_poll(bContext *C)
{
  if (CTX_wm_space_outliner(C) != NULL) {
    return ED_outliner_collections_editor_poll(C);
  }
  else {
    View3D *v3d = CTX_wm_view3d(C);

    if (v3d && v3d->localvd) {
      return false;
    }

    return ED_operator_objectmode(C);
  }
}

static int move_to_collection_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  PropertyRNA *prop = RNA_struct_find_property(op->ptr, "collection_index");
  const bool is_link = STREQ(op->idname, "OBJECT_OT_link_to_collection");
  const bool is_new = RNA_boolean_get(op->ptr, "is_new");
  Collection *collection;
  ListBase objects = {NULL};

  if (!RNA_property_is_set(op->ptr, prop)) {
    BKE_report(op->reports, RPT_ERROR, "No collection selected");
    return OPERATOR_CANCELLED;
  }

  int collection_index = RNA_property_int_get(op->ptr, prop);
  collection = BKE_collection_from_index(scene, collection_index);
  if (collection == NULL) {
    BKE_report(op->reports, RPT_ERROR, "Unexpected error, collection not found");
    return OPERATOR_CANCELLED;
  }

  objects = selected_objects_get(C);

  if (is_new) {
    char new_collection_name[MAX_NAME];
    RNA_string_get(op->ptr, "new_collection_name", new_collection_name);
    collection = BKE_collection_add(bmain, collection, new_collection_name);
  }

  Object *single_object = BLI_listbase_is_single(&objects) ? ((LinkData *)objects.first)->data :
                                                             NULL;

  if ((single_object != NULL) && is_link &&
      BLI_findptr(&collection->gobject, single_object, offsetof(CollectionObject, ob))) {
    BKE_reportf(op->reports,
                RPT_ERROR,
                "%s already in %s",
                single_object->id.name + 2,
                collection->id.name + 2);
    BLI_freelistN(&objects);
    return OPERATOR_CANCELLED;
  }

  LISTBASE_FOREACH (LinkData *, link, &objects) {
    Object *ob = link->data;

    if (!is_link) {
      BKE_collection_object_move(bmain, scene, collection, NULL, ob);
    }
    else {
      BKE_collection_object_add(bmain, collection, ob);
    }
  }
  BLI_freelistN(&objects);

  BKE_reportf(op->reports,
              RPT_INFO,
              "%s %s to %s",
              (single_object != NULL) ? single_object->id.name + 2 : "Objects",
              is_link ? "linked" : "moved",
              collection->id.name + 2);

  DEG_relations_tag_update(bmain);
  DEG_id_tag_update(&scene->id, ID_RECALC_COPY_ON_WRITE | ID_RECALC_SELECT);

  WM_event_add_notifier(C, NC_SCENE | ND_LAYER, scene);
  WM_event_add_notifier(C, NC_SCENE | ND_OB_ACTIVE, scene);
  WM_event_add_notifier(C, NC_SCENE | ND_LAYER_CONTENT, scene);

  return OPERATOR_FINISHED;
}

struct MoveToCollectionData {
  struct MoveToCollectionData *next, *prev;
  int index;
  struct Collection *collection;
  struct ListBase submenus;
  PointerRNA ptr;
  struct wmOperatorType *ot;
};

static int move_to_collection_menus_create(wmOperator *op, MoveToCollectionData *menu)
{
  int index = menu->index;
  for (CollectionChild *child = menu->collection->children.first; child != NULL;
       child = child->next) {
    Collection *collection = child->collection;
    MoveToCollectionData *submenu = MEM_callocN(sizeof(MoveToCollectionData),
                                                "MoveToCollectionData submenu - expected memleak");
    BLI_addtail(&menu->submenus, submenu);
    submenu->collection = collection;
    submenu->index = ++index;
    index = move_to_collection_menus_create(op, submenu);
    submenu->ot = op->type;
  }
  return index;
}

static void move_to_collection_menus_free_recursive(MoveToCollectionData *menu)
{
  for (MoveToCollectionData *submenu = menu->submenus.first; submenu != NULL;
       submenu = submenu->next) {
    move_to_collection_menus_free_recursive(submenu);
  }
  BLI_freelistN(&menu->submenus);
}

static void move_to_collection_menus_free(MoveToCollectionData **menu)
{
  if (*menu == NULL) {
    return;
  }

  move_to_collection_menus_free_recursive(*menu);
  MEM_freeN(*menu);
  *menu = NULL;
}

static void move_to_collection_menu_create(bContext *UNUSED(C), uiLayout *layout, void *menu_v)
{
  MoveToCollectionData *menu = menu_v;
  const char *name = BKE_collection_ui_name_get(menu->collection);

  UI_block_flag_enable(uiLayoutGetBlock(layout), UI_BLOCK_IS_FLIP);

  // uiItemS(layout);

  WM_operator_properties_create_ptr(&menu->ptr, menu->ot);
  RNA_int_set(&menu->ptr, "collection_index", menu->index);
  RNA_boolean_set(&menu->ptr, "is_new", true);

  uiItemFullO_ptr(
      layout, menu->ot, "New Collection", ICON_ADD, menu->ptr.data, WM_OP_INVOKE_DEFAULT, 0, NULL);

  uiItemS(layout);

  uiItemIntO(layout, name, ICON_SCENE_DATA, menu->ot->idname, "collection_index", menu->index);

  for (MoveToCollectionData *submenu = menu->submenus.first; submenu != NULL;
       submenu = submenu->next) {
    move_to_collection_menus_items(layout, submenu);
  }
}

static void move_to_collection_menus_items(uiLayout *layout, MoveToCollectionData *menu)
{
  if (BLI_listbase_is_empty(&menu->submenus)) {
    uiItemIntO(layout,
               menu->collection->id.name + 2,
               ICON_NONE,
               menu->ot->idname,
               "collection_index",
               menu->index);
  }
  else {
    uiItemMenuF(
        layout, menu->collection->id.name + 2, ICON_NONE, move_to_collection_menu_create, menu);
  }
}

/* This is allocated statically because we need this available for the menus creation callback. */
static MoveToCollectionData *master_collection_menu = NULL;

static int move_to_collection_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  Scene *scene = CTX_data_scene(C);

  ListBase objects = selected_objects_get(C);
  if (BLI_listbase_is_empty(&objects)) {
    BKE_report(op->reports, RPT_ERROR, "No objects selected");
    return OPERATOR_CANCELLED;
  }
  BLI_freelistN(&objects);

  /* Reset the menus data for the current master collection, and free previously allocated data. */
  move_to_collection_menus_free(&master_collection_menu);

  PropertyRNA *prop;
  prop = RNA_struct_find_property(op->ptr, "collection_index");
  if (RNA_property_is_set(op->ptr, prop)) {
    int collection_index = RNA_property_int_get(op->ptr, prop);

    if (RNA_boolean_get(op->ptr, "is_new")) {
      prop = RNA_struct_find_property(op->ptr, "new_collection_name");
      if (!RNA_property_is_set(op->ptr, prop)) {
        char name[MAX_NAME];
        Collection *collection;

        collection = BKE_collection_from_index(scene, collection_index);
        BKE_collection_new_name_get(collection, name);

        RNA_property_string_set(op->ptr, prop, name);
        return WM_operator_props_dialog_popup(C, op, 200);
      }
    }
    return move_to_collection_exec(C, op);
  }

  Collection *master_collection = scene->master_collection;

  /* We need the data to be allocated so it's available during menu drawing.
   * Technically we could use wmOperator->customdata. However there is no free callback
   * called to an operator that exit with OPERATOR_INTERFACE to launch a menu.
   *
   * So we are left with a memory that will necessarily leak. It's a small leak though.*/
  if (master_collection_menu == NULL) {
    master_collection_menu = MEM_callocN(sizeof(MoveToCollectionData),
                                         "MoveToCollectionData menu - expected eventual memleak");
  }

  master_collection_menu->collection = master_collection;
  master_collection_menu->ot = op->type;
  move_to_collection_menus_create(op, master_collection_menu);

  uiPopupMenu *pup;
  uiLayout *layout;

  /* Build the menus. */
  const char *title = CTX_IFACE_(op->type->translation_context, op->type->name);
  pup = UI_popup_menu_begin(C, title, ICON_NONE);
  layout = UI_popup_menu_layout(pup);

  uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_DEFAULT);

  move_to_collection_menu_create(C, layout, master_collection_menu);

  UI_popup_menu_end(C, pup);

  return OPERATOR_INTERFACE;
}

void OBJECT_OT_move_to_collection(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Move to Collection";
  ot->description = "Move objects to a collection";
  ot->idname = "OBJECT_OT_move_to_collection";

  /* api callbacks */
  ot->exec = move_to_collection_exec;
  ot->invoke = move_to_collection_invoke;
  ot->poll = move_to_collection_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  prop = RNA_def_int(ot->srna,
                     "collection_index",
                     COLLECTION_INVALID_INDEX,
                     COLLECTION_INVALID_INDEX,
                     INT_MAX,
                     "Collection Index",
                     "Index of the collection to move to",
                     0,
                     INT_MAX);
  RNA_def_property_flag(prop, PROP_SKIP_SAVE | PROP_HIDDEN);
  prop = RNA_def_boolean(ot->srna, "is_new", false, "New", "Move objects to a new collection");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE | PROP_HIDDEN);
  prop = RNA_def_string(ot->srna,
                        "new_collection_name",
                        NULL,
                        MAX_NAME,
                        "Name",
                        "Name of the newly added collection");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  ot->prop = prop;
}

void OBJECT_OT_link_to_collection(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Link to Collection";
  ot->description = "Link objects to a collection";
  ot->idname = "OBJECT_OT_link_to_collection";

  /* api callbacks */
  ot->exec = move_to_collection_exec;
  ot->invoke = move_to_collection_invoke;
  ot->poll = move_to_collection_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  prop = RNA_def_int(ot->srna,
                     "collection_index",
                     COLLECTION_INVALID_INDEX,
                     COLLECTION_INVALID_INDEX,
                     INT_MAX,
                     "Collection Index",
                     "Index of the collection to move to",
                     0,
                     INT_MAX);
  RNA_def_property_flag(prop, PROP_SKIP_SAVE | PROP_HIDDEN);
  prop = RNA_def_boolean(ot->srna, "is_new", false, "New", "Move objects to a new collection");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE | PROP_HIDDEN);
  prop = RNA_def_string(ot->srna,
                        "new_collection_name",
                        NULL,
                        MAX_NAME,
                        "Name",
                        "Name of the newly added collection");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  ot->prop = prop;
}

/** \} */
