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
#include "BLI_math.h"

#include "DNA_gpencil_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BKE_context.h"
#include "BKE_duplilist.h"
#include "BKE_global.h"
#include "BKE_gpencil.h"
#include "BKE_gpencil_geom.h"
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
static void gp_bake_set_frame_end(struct Main *UNUSED(main),
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
static bool gp_bake_mesh_animation_poll(bContext *C)
{
  if (CTX_data_mode_enum(C) != CTX_MODE_OBJECT) {
    return false;
  }

  /* Only if the current view is 3D View. */
  ScrArea *area = CTX_wm_area(C);
  return (area && area->spacetype);
}

typedef struct GpBakeOb {
  struct GPBakelist *next, *prev;
  Object *ob;
} GpBakeOb;

static void gp_bake_duplilist(Depsgraph *depsgraph, Scene *scene, Object *ob, ListBase *list)
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

static void gp_bake_ob_list(bContext *C, Depsgraph *depsgraph, Scene *scene, ListBase *list)
{
  GpBakeOb *elem = NULL;

  /* Add active object. In some files this could not be in selected array. */
  Object *obact = CTX_data_active_object(C);

  if (obact->type == OB_MESH) {
    elem = MEM_callocN(sizeof(GpBakeOb), __func__);
    elem->ob = obact;
    BLI_addtail(list, elem);
  }
  /* Add duplilist. */
  else if (obact->type == OB_EMPTY) {
    gp_bake_duplilist(depsgraph, scene, obact, list);
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
      gp_bake_duplilist(depsgraph, scene, obact, list);
    }
  }
  CTX_DATA_END;
}

static void gp_bake_free_ob_list(ListBase *list)
{
  LISTBASE_FOREACH_MUTABLE (GpBakeOb *, elem, list) {
    MEM_SAFE_FREE(elem);
  }
}

static int gp_bake_mesh_animation_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Scene *scene = CTX_data_scene(C);
  ARegion *region = CTX_wm_region(C);
  View3D *v3d = CTX_wm_view3d(C);
  Object *ob_gpencil = NULL;

  ListBase list = {NULL, NULL};
  gp_bake_ob_list(C, depsgraph, scene, &list);

  /* Cannot check this in poll because the active object changes. */
  if (list.first == NULL) {
    BKE_report(op->reports, RPT_INFO, "No valid object selected");
    gp_bake_free_ob_list(&list);
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
  const float offset = RNA_float_get(op->ptr, "offset");
  const int frame_offset = RNA_int_get(op->ptr, "frame_target") - frame_start;
  char target[64];
  RNA_string_get(op->ptr, "target", target);
  const int project_type = RNA_enum_get(op->ptr, "project_type");

  /* Create a new grease pencil object in origin. */
  bool newob = false;
  if (STREQ(target, "*NEW")) {
    ushort local_view_bits = (v3d && v3d->localvd) ? v3d->local_view_uuid : 0;
    float loc[3] = {0.0f, 0.0f, 0.0f};
    ob_gpencil = ED_gpencil_add_object(C, loc, local_view_bits);
    newob = true;
  }
  else {
    ob_gpencil = BLI_findstring(&bmain->objects, target, offsetof(ID, name) + 2);
  }

  if ((ob_gpencil == NULL) || (ob_gpencil->type != OB_GPENCIL)) {
    BKE_report(op->reports, RPT_ERROR, "Target grease pencil object not valid");
    gp_bake_free_ob_list(&list);
    return OPERATOR_CANCELLED;
  }

  bGPdata *gpd = (bGPdata *)ob_gpencil->data;
  gpd->draw_mode = (project_type == GP_REPROJECT_KEEP) ? GP_DRAWMODE_3D : GP_DRAWMODE_2D;

  /* Set cursor to indicate working. */
  WM_cursor_wait(1);

  GP_SpaceConversion gsc = {NULL};
  SnapObjectContext *sctx = NULL;
  if (project_type != GP_REPROJECT_KEEP) {
    /* Init space conversion stuff. */
    gp_point_conversion_init(C, &gsc);
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
  for (int i = frame_start; i < frame_end + 1; i++) {
    key++;
    /* Jump if not step limit but include last frame always. */
    if ((key % step != 0) && (i != frame_end)) {
      continue;
    }

    /* Move scene to new frame. */
    CFRA = i;
    BKE_scene_graph_update_for_newframe(depsgraph, bmain);

    /* Loop all objects in the list. */
    LISTBASE_FOREACH (GpBakeOb *, elem, &list) {
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

      /* Reproject all untaged created strokes. */
      if (project_type != GP_REPROJECT_KEEP) {
        LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
          bGPDframe *gpf = gpl->actframe;
          if (gpf != NULL) {
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
  }

  /* Return scene frame state and DB to original state. */
  CFRA = oldframe;
  BKE_scene_graph_update_for_newframe(depsgraph, bmain);

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
  gp_bake_free_ob_list(&list);
  if (sctx != NULL) {
    ED_transform_snap_object_context_destroy(sctx);
  }

  /* notifiers */
  if (newob) {
    DEG_relations_tag_update(bmain);
  }
  DEG_id_tag_update(&scene->id, ID_RECALC_SELECT);
  WM_event_add_notifier(C, NC_OBJECT | NA_ADDED, NULL);
  WM_event_add_notifier(C, NC_SCENE | ND_OB_ACTIVE, scene);

  /* Reset cursor. */
  WM_cursor_wait(0);

  /* done */
  return OPERATOR_FINISHED;
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

  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Bake Mesh Animation to Grease Pencil";
  ot->idname = "GPENCIL_OT_bake_mesh_animation";
  ot->description = "Bake Mesh Animation to Grease Pencil strokes";

  /* callbacks */
  ot->exec = gp_bake_mesh_animation_exec;
  ot->poll = gp_bake_mesh_animation_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  ot->prop = RNA_def_int(
      ot->srna, "frame_start", 1, 1, 100000, "Start Frame", "The start frame", 1, 100000);

  prop = RNA_def_int(
      ot->srna, "frame_end", 250, 1, 100000, "End Frame", "The end frame of animation", 1, 100000);
  RNA_def_property_update_runtime(prop, gp_bake_set_frame_end);

  prop = RNA_def_int(ot->srna, "step", 1, 1, 100, "Step", "Step between generated frames", 1, 100);

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

  RNA_def_int(ot->srna, "thickness", 1, 1, 100, "Thickness", "", 1, 100);
  RNA_def_boolean(ot->srna, "seams", 0, "Only Seam Edges", "Convert only seam edges");
  RNA_def_boolean(ot->srna, "faces", 1, "Export Faces", "Export faces as filled strokes");
  RNA_def_float_distance(
      ot->srna, "offset", 0.001f, 0.0, 100.0, "Offset", "Offset strokes from fill", 0.0, 100.00);
  RNA_def_int(ot->srna, "frame_target", 1, 1, 100000, "Frame Target", "", 1, 100000);
  RNA_def_string(ot->srna,
                 "target",
                 "*NEW",
                 64,
                 "Target Object",
                 "Target grease pencil object name. Leave empty for new object");

  RNA_def_enum(ot->srna, "project_type", reproject_type, GP_REPROJECT_VIEW, "Projection Type", "");
}
