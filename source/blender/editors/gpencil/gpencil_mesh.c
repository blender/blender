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
 * The Original Code is Copyright (C) 2008, Blender Foundation
 * This is a new part of Blender
 * Operator for converting Grease Pencil data to geometry
 */

/** \file
 * \ingroup edgpencil
 */

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_ghash.h"
#include "BLI_math.h"

#include "DNA_anim_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BKE_anim_data.h"
#include "BKE_context.h"
#include "BKE_duplilist.h"
#include "BKE_gpencil_geom.h"
#include "BKE_layer.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_object.h"
#include "BKE_report.h"
#include "BKE_scene.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "ED_gpencil.h"
#include "ED_transform_snap_object_context.h"

#include "gpencil_intern.h"

/* Check frame_end is always > start frame! */
static void gpencil_bake_set_frame_end(struct Main *UNUSED(main),
                                       struct Scene *UNUSED(scene),
                                       struct PointerRNA *ptr)
{
  int frame_start = RNA_int_get(ptr, "frame_start");
  int frame_end = RNA_int_get(ptr, "frame_end");

  if (frame_end <= frame_start) {
    RNA_int_set(ptr, "frame_end", frame_start + 1);
  }
}

/* Extract mesh animation to Grease Pencil. */
static bool gpencil_bake_mesh_animation_poll(bContext *C)
{
  if (CTX_data_mode_enum(C) != CTX_MODE_OBJECT) {
    return false;
  }

  /* Only if the current view is 3D View. */
  ScrArea *area = CTX_wm_area(C);
  return (area && area->spacetype);
}

typedef struct GpBakeOb {
  struct GpBakeOb *next, *prev;
  Object *ob;
} GpBakeOb;

/* Get list of keyframes used by selected objects. */
static void animdata_keyframe_list_get(ListBase *ob_list,
                                       const bool only_selected,
                                       GHash *r_keyframes)
{
  /* Loop all objects to get the list of keyframes used. */
  LISTBASE_FOREACH (GpBakeOb *, elem, ob_list) {
    Object *ob = elem->ob;
    AnimData *adt = BKE_animdata_from_id(&ob->id);
    if ((adt == NULL) || (adt->action == NULL)) {
      continue;
    }
    LISTBASE_FOREACH (FCurve *, fcurve, &adt->action->curves) {
      int i;
      BezTriple *bezt;
      for (i = 0, bezt = fcurve->bezt; i < fcurve->totvert; i++, bezt++) {
        /* Keyframe number is x value of point. */
        if ((bezt->f2 & SELECT) || (!only_selected)) {
          /* Insert only one key for each keyframe number. */
          int key = (int)bezt->vec[1][0];
          if (!BLI_ghash_haskey(r_keyframes, POINTER_FROM_INT(key))) {
            BLI_ghash_insert(r_keyframes, POINTER_FROM_INT(key), POINTER_FROM_INT(key));
          }
        }
      }
    }
  }
}

static void gpencil_bake_duplilist(Depsgraph *depsgraph, Scene *scene, Object *ob, ListBase *list)
{
  GpBakeOb *elem = NULL;
  ListBase *lb;
  DupliObject *dob;
  lb = object_duplilist(depsgraph, scene, ob);
  for (dob = lb->first; dob; dob = dob->next) {
    if (dob->ob->type != OB_MESH) {
      continue;
    }
    elem = MEM_callocN(sizeof(GpBakeOb), __func__);
    elem->ob = dob->ob;
    BLI_addtail(list, elem);
  }

  free_object_duplilist(lb);
}

static bool gpencil_bake_ob_list(bContext *C, Depsgraph *depsgraph, Scene *scene, ListBase *list)
{
  GpBakeOb *elem = NULL;
  bool simple_material = false;

  /* Add active object. In some files this could not be in selected array. */
  Object *obact = CTX_data_active_object(C);

  if (obact->type == OB_MESH) {
    elem = MEM_callocN(sizeof(GpBakeOb), __func__);
    elem->ob = obact;
    BLI_addtail(list, elem);
  }
  /* Add duplilist. */
  else if (obact->type == OB_EMPTY) {
    gpencil_bake_duplilist(depsgraph, scene, obact, list);
    simple_material |= true;
  }

  /* Add other selected objects. */
  CTX_DATA_BEGIN (C, Object *, ob, selected_objects) {
    if (ob == obact) {
      continue;
    }
    /* Add selected meshes.*/
    if (ob->type == OB_MESH) {
      elem = MEM_callocN(sizeof(GpBakeOb), __func__);
      elem->ob = ob;
      BLI_addtail(list, elem);
    }

    /* Add duplilist. */
    if (ob->type == OB_EMPTY) {
      gpencil_bake_duplilist(depsgraph, scene, obact, list);
    }
  }
  CTX_DATA_END;

  return simple_material;
}

static void gpencil_bake_free_ob_list(ListBase *list)
{
  LISTBASE_FOREACH_MUTABLE (GpBakeOb *, elem, list) {
    MEM_SAFE_FREE(elem);
  }
}

static int gpencil_bake_mesh_animation_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Scene *scene = CTX_data_scene(C);
  ARegion *region = CTX_wm_region(C);
  View3D *v3d = CTX_wm_view3d(C);

  ListBase ob_selected_list = {NULL, NULL};
  gpencil_bake_ob_list(C, depsgraph, scene, &ob_selected_list);

  /* Cannot check this in poll because the active object changes. */
  if (ob_selected_list.first == NULL) {
    BKE_report(op->reports, RPT_INFO, "No valid object selected");
    gpencil_bake_free_ob_list(&ob_selected_list);
    return OPERATOR_CANCELLED;
  }

  /* Grab all relevant settings. */
  const int step = RNA_int_get(op->ptr, "step");

  const int frame_start = (scene->r.sfra > RNA_int_get(op->ptr, "frame_start")) ?
                              scene->r.sfra :
                              RNA_int_get(op->ptr, "frame_start");

  const int frame_end = (scene->r.efra < RNA_int_get(op->ptr, "frame_end")) ?
                            scene->r.efra :
                            RNA_int_get(op->ptr, "frame_end");

  const float angle = RNA_float_get(op->ptr, "angle");
  const int thickness = RNA_int_get(op->ptr, "thickness");
  const bool use_seams = RNA_boolean_get(op->ptr, "seams");
  const bool use_faces = RNA_boolean_get(op->ptr, "faces");
  const bool only_selected = RNA_boolean_get(op->ptr, "only_selected");
  const float offset = RNA_float_get(op->ptr, "offset");
  const int frame_offset = RNA_int_get(op->ptr, "frame_target") - frame_start;
  const int project_type = RNA_enum_get(op->ptr, "project_type");
  eGP_TargetObjectMode target = RNA_enum_get(op->ptr, "target");

  /* Create a new grease pencil object in origin or reuse selected. */
  Object *ob_gpencil = NULL;
  bool newob = false;

  if (target == GP_TARGET_OB_SELECTED) {
    ob_gpencil = BKE_view_layer_non_active_selected_object(CTX_data_view_layer(C), v3d);
    if (ob_gpencil != NULL) {
      if (ob_gpencil->type != OB_GPENCIL) {
        BKE_report(op->reports, RPT_WARNING, "Target object not a grease pencil, ignoring!");
        ob_gpencil = NULL;
      }
      else if (BKE_object_obdata_is_libdata(ob_gpencil)) {
        BKE_report(op->reports, RPT_WARNING, "Target object library-data, ignoring!");
        ob_gpencil = NULL;
      }
    }
  }

  if (ob_gpencil == NULL) {
    ushort local_view_bits = (v3d && v3d->localvd) ? v3d->local_view_uuid : 0;
    const float loc[3] = {0.0f, 0.0f, 0.0f};
    ob_gpencil = ED_gpencil_add_object(C, loc, local_view_bits);
    newob = true;
  }

  bGPdata *gpd = (bGPdata *)ob_gpencil->data;
  gpd->draw_mode = (project_type == GP_REPROJECT_KEEP) ? GP_DRAWMODE_3D : GP_DRAWMODE_2D;

  /* Set cursor to indicate working. */
  WM_cursor_wait(true);

  GP_SpaceConversion gsc = {NULL};
  SnapObjectContext *sctx = NULL;
  if (project_type != GP_REPROJECT_KEEP) {
    /* Init space conversion stuff. */
    gpencil_point_conversion_init(C, &gsc);
    /* Move the grease pencil object to conversion data. */
    gsc.ob = ob_gpencil;

    /* Init snap context for geometry projection. */
    sctx = ED_transform_snap_object_context_create_view3d(scene, 0, region, CTX_wm_view3d(C));

    /* Tag all existing strokes to avoid reprojections. */
    LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
      LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
        LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
          gps->flag |= GP_STROKE_TAG;
        }
      }
    }
  }

  /* Loop all frame range. */
  int oldframe = (int)DEG_get_ctime(depsgraph);
  int key = -1;

  /* Get list of keyframes. */
  GHash *keyframe_list = BLI_ghash_int_new(__func__);
  if (only_selected) {
    animdata_keyframe_list_get(&ob_selected_list, only_selected, keyframe_list);
  }

  for (int i = frame_start; i < frame_end + 1; i++) {
    key++;
    /* Jump if not step limit but include last frame always. */
    if ((key % step != 0) && (i != frame_end)) {
      continue;
    }

    /* Check if frame is in the list of frames to be exported. */
    if ((only_selected) && (!BLI_ghash_haskey(keyframe_list, POINTER_FROM_INT(i)))) {
      continue;
    }

    /* Move scene to new frame. */
    CFRA = i;
    BKE_scene_graph_update_for_newframe(depsgraph);

    /* Loop all objects in the list. */
    LISTBASE_FOREACH (GpBakeOb *, elem, &ob_selected_list) {
      Object *ob_eval = (Object *)DEG_get_evaluated_object(depsgraph, elem->ob);

      /* Generate strokes. */
      BKE_gpencil_convert_mesh(bmain,
                               depsgraph,
                               scene,
                               ob_gpencil,
                               elem->ob,
                               angle,
                               thickness,
                               offset,
                               ob_eval->obmat,
                               frame_offset,
                               use_seams,
                               use_faces);

      /* Reproject all un-tagged created strokes. */
      if (project_type != GP_REPROJECT_KEEP) {
        LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
          bGPDframe *gpf = gpl->actframe;
          if (gpf == NULL) {
            continue;
          }
          LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
            if ((gps->flag & GP_STROKE_TAG) == 0) {
              ED_gpencil_stroke_reproject(
                  depsgraph, &gsc, sctx, gpl, gpf, gps, project_type, false);
              gps->flag |= GP_STROKE_TAG;
            }
          }
        }
      }
    }
  }

  /* Return scene frame state and DB to original state. */
  CFRA = oldframe;
  BKE_scene_graph_update_for_newframe(depsgraph);

  /* Remove unused materials. */
  int actcol = ob_gpencil->actcol;
  for (int slot = 1; slot <= ob_gpencil->totcol; slot++) {
    while (slot <= ob_gpencil->totcol && !BKE_object_material_slot_used(ob_gpencil->data, slot)) {
      ob_gpencil->actcol = slot;
      BKE_object_material_slot_remove(CTX_data_main(C), ob_gpencil);

      if (actcol >= slot) {
        actcol--;
      }
    }
  }
  ob_gpencil->actcol = actcol;

  /* Untag all strokes. */
  if (project_type != GP_REPROJECT_KEEP) {
    LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
      LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
        LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
          gps->flag &= ~GP_STROKE_TAG;
        }
      }
    }
  }

  /* Free memory. */
  gpencil_bake_free_ob_list(&ob_selected_list);
  if (sctx != NULL) {
    ED_transform_snap_object_context_destroy(sctx);
  }
  /* Free temp hash table. */
  if (keyframe_list != NULL) {
    BLI_ghash_free(keyframe_list, NULL, NULL);
  }

  /* notifiers */
  if (newob) {
    DEG_relations_tag_update(bmain);
  }
  DEG_id_tag_update(&scene->id, ID_RECALC_SELECT);
  WM_event_add_notifier(C, NC_OBJECT | NA_ADDED, NULL);
  WM_event_add_notifier(C, NC_SCENE | ND_OB_ACTIVE, scene);

  /* Reset cursor. */
  WM_cursor_wait(false);

  /* done */
  return OPERATOR_FINISHED;
}

static int gpencil_bake_mesh_animation_invoke(bContext *C,
                                              wmOperator *op,
                                              const wmEvent *UNUSED(event))
{
  /* Show popup dialog to allow editing. */
  /* FIXME: hard-coded dimensions here are just arbitrary. */
  return WM_operator_props_dialog_popup(C, op, 250);
}

void GPENCIL_OT_bake_mesh_animation(wmOperatorType *ot)
{
  static const EnumPropertyItem reproject_type[] = {
      {GP_REPROJECT_KEEP, "KEEP", 0, "No Reproject", ""},
      {GP_REPROJECT_FRONT, "FRONT", 0, "Front", "Reproject the strokes using the X-Z plane"},
      {GP_REPROJECT_SIDE, "SIDE", 0, "Side", "Reproject the strokes using the Y-Z plane"},
      {GP_REPROJECT_TOP, "TOP", 0, "Top", "Reproject the strokes using the X-Y plane"},
      {GP_REPROJECT_VIEW,
       "VIEW",
       0,
       "View",
       "Reproject the strokes to end up on the same plane, as if drawn from the current viewpoint "
       "using 'Cursor' Stroke Placement"},
      {GP_REPROJECT_CURSOR,
       "CURSOR",
       0,
       "Cursor",
       "Reproject the strokes using the orientation of 3D cursor"},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem target_object_modes[] = {
      {GP_TARGET_OB_NEW, "NEW", 0, "New Object", ""},
      {GP_TARGET_OB_SELECTED, "SELECTED", 0, "Selected Object", ""},
      {0, NULL, 0, NULL, NULL},
  };

  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Bake Mesh Animation to Grease Pencil";
  ot->idname = "GPENCIL_OT_bake_mesh_animation";
  ot->description = "Bake mesh animation to grease pencil strokes";

  /* callbacks */
  ot->invoke = gpencil_bake_mesh_animation_invoke;
  ot->exec = gpencil_bake_mesh_animation_exec;
  ot->poll = gpencil_bake_mesh_animation_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  ot->prop = RNA_def_enum(ot->srna,
                          "target",
                          target_object_modes,
                          GP_TARGET_OB_NEW,
                          "Target Object",
                          "Target grease pencil");
  RNA_def_property_flag(ot->prop, PROP_SKIP_SAVE);

  prop = RNA_def_int(
      ot->srna, "frame_start", 1, 1, 100000, "Start Frame", "The start frame", 1, 100000);

  prop = RNA_def_int(
      ot->srna, "frame_end", 250, 1, 100000, "End Frame", "The end frame of animation", 1, 100000);
  RNA_def_property_update_runtime(prop, gpencil_bake_set_frame_end);

  prop = RNA_def_int(ot->srna, "step", 1, 1, 100, "Step", "Step between generated frames", 1, 100);

  RNA_def_int(ot->srna, "thickness", 1, 1, 100, "Thickness", "", 1, 100);

  prop = RNA_def_float_rotation(ot->srna,
                                "angle",
                                0,
                                NULL,
                                DEG2RADF(0.0f),
                                DEG2RADF(180.0f),
                                "Threshold Angle",
                                "Threshold to determine ends of the strokes",
                                DEG2RADF(0.0f),
                                DEG2RADF(180.0f));
  RNA_def_property_float_default(prop, DEG2RADF(70.0f));

  RNA_def_float_distance(ot->srna,
                         "offset",
                         0.001f,
                         0.0,
                         100.0,
                         "Stroke Offset",
                         "Offset strokes from fill",
                         0.0,
                         100.00);

  RNA_def_boolean(ot->srna, "seams", 0, "Only Seam Edges", "Convert only seam edges");
  RNA_def_boolean(ot->srna, "faces", 1, "Export Faces", "Export faces as filled strokes");
  RNA_def_boolean(
      ot->srna, "only_selected", 0, "Only Selected Keyframes", "Convert only selected keyframes");
  RNA_def_int(
      ot->srna, "frame_target", 1, 1, 100000, "Target Frame", "Destination frame", 1, 100000);

  RNA_def_enum(ot->srna, "project_type", reproject_type, GP_REPROJECT_VIEW, "Projection Type", "");
}
