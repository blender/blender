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
 * The Original Code is Copyright (C) 2021 Blender Foundation
 * This is a new part of Blender
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
#include "DNA_material_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BKE_anim_data.h"
#include "BKE_context.h"
#include "BKE_duplilist.h"
#include "BKE_gpencil.h"
#include "BKE_gpencil_geom.h"
#include "BKE_gpencil_modifier.h"
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

const EnumPropertyItem rna_gpencil_reproject_type_items[] = {
    {GP_REPROJECT_KEEP, "KEEP", 0, "No Reproject", ""},
    {GP_REPROJECT_FRONT, "FRONT", 0, "Front", "Reproject the strokes using the X-Z plane"},
    {GP_REPROJECT_SIDE, "SIDE", 0, "Side", "Reproject the strokes using the Y-Z plane"},
    {GP_REPROJECT_TOP, "TOP", 0, "Top", "Reproject the strokes using the X-Y plane"},
    {GP_REPROJECT_VIEW,
     "VIEW",
     0,
     "View",
     "Reproject the strokes to end up on the same plane, as if drawn from the current "
     "viewpoint "
     "using 'Cursor' Stroke Placement"},
    {GP_REPROJECT_CURSOR,
     "CURSOR",
     0,
     "Cursor",
     "Reproject the strokes using the orientation of 3D cursor"},
    {0, NULL, 0, NULL, NULL},
};

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
static bool gpencil_bake_grease_pencil_animation_poll(bContext *C)
{
  Object *obact = CTX_data_active_object(C);
  if (CTX_data_mode_enum(C) != CTX_MODE_OBJECT) {
    return false;
  }

  /* Check if grease pencil or empty for dupli groups. */
  if ((obact == NULL) || ((obact->type != OB_GPENCIL) && (obact->type != OB_EMPTY))) {
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
    if (dob->ob->type != OB_GPENCIL) {
      continue;
    }

    elem = MEM_callocN(sizeof(GpBakeOb), __func__);
    elem->ob = dob->ob;
    BLI_addtail(list, elem);
  }

  free_object_duplilist(lb);
}

static void gpencil_bake_ob_list(bContext *C, Depsgraph *depsgraph, Scene *scene, ListBase *list)
{
  GpBakeOb *elem = NULL;

  /* Add active object. In some files this could not be in selected array. */
  Object *obact = CTX_data_active_object(C);

  if (obact->type == OB_GPENCIL) {
    elem = MEM_callocN(sizeof(GpBakeOb), __func__);
    elem->ob = obact;
    BLI_addtail(list, elem);
  }
  /* Add duplilist. */
  else if (obact->type == OB_EMPTY) {
    gpencil_bake_duplilist(depsgraph, scene, obact, list);
  }

  /* Add other selected objects. */
  CTX_DATA_BEGIN (C, Object *, ob, selected_objects) {
    if (ob == obact) {
      continue;
    }
    /* Add selected objects. */
    if (ob->type == OB_GPENCIL) {
      elem = MEM_callocN(sizeof(GpBakeOb), __func__);
      elem->ob = ob;
      BLI_addtail(list, elem);
    }

    /* Add duplilist. */
    if (ob->type == OB_EMPTY) {
      gpencil_bake_duplilist(depsgraph, scene, ob, list);
    }
  }
  CTX_DATA_END;
}

static void gpencil_bake_free_ob_list(ListBase *list)
{
  LISTBASE_FOREACH_MUTABLE (GpBakeOb *, elem, list) {
    MEM_SAFE_FREE(elem);
  }
}

static int gpencil_bake_grease_pencil_animation_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Scene *scene = CTX_data_scene(C);
  View3D *v3d = CTX_wm_view3d(C);

  ListBase ob_selected_list = {NULL, NULL};
  gpencil_bake_ob_list(C, depsgraph, scene, &ob_selected_list);

  /* Grab all relevant settings. */
  const int step = RNA_int_get(op->ptr, "step");

  const int frame_start = (scene->r.sfra > RNA_int_get(op->ptr, "frame_start")) ?
                              scene->r.sfra :
                              RNA_int_get(op->ptr, "frame_start");

  const int frame_end = (scene->r.efra < RNA_int_get(op->ptr, "frame_end")) ?
                            scene->r.efra :
                            RNA_int_get(op->ptr, "frame_end");

  const bool only_selected = RNA_boolean_get(op->ptr, "only_selected");
  const int frame_offset = RNA_int_get(op->ptr, "frame_target") - frame_start;
  const int project_type = RNA_enum_get(op->ptr, "project_type");

  /* Create a new grease pencil object. */
  Object *ob_gpencil = NULL;
  ushort local_view_bits = (v3d && v3d->localvd) ? v3d->local_view_uuid : 0;
  ob_gpencil = ED_gpencil_add_object(C, scene->cursor.location, local_view_bits);
  float invmat[4][4];
  invert_m4_m4(invmat, ob_gpencil->obmat);

  bGPdata *gpd_dst = (bGPdata *)ob_gpencil->data;
  gpd_dst->draw_mode = GP_DRAWMODE_2D;

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
    sctx = ED_transform_snap_object_context_create(scene, 0);
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
      bGPdata *gpd_src = ob_eval->data;

      LISTBASE_FOREACH (bGPDlayer *, gpl_src, &gpd_src->layers) {
        /* Create destination layer. */
        char *layer_name;
        layer_name = BLI_sprintfN("%s_%s", elem->ob->id.name + 2, gpl_src->info);
        bGPDlayer *gpl_dst = BKE_gpencil_layer_named_get(gpd_dst, layer_name);
        if (gpl_dst == NULL) {
          gpl_dst = BKE_gpencil_layer_addnew(gpd_dst, layer_name, true, false);
        }
        MEM_freeN(layer_name);

        /* Layer Transform matrix. */
        float matrix[4][4];
        BKE_gpencil_layer_transform_matrix_get(depsgraph, elem->ob, gpl_src, matrix);

        /* Apply time modifier. */
        int remap_cfra = BKE_gpencil_time_modifier_cfra(
            depsgraph, scene, elem->ob, gpl_src, CFRA, false);
        /* Duplicate frame. */
        bGPDframe *gpf_src = BKE_gpencil_layer_frame_get(
            gpl_src, remap_cfra, GP_GETFRAME_USE_PREV);
        if (gpf_src == NULL) {
          continue;
        }
        bGPDframe *gpf_dst = BKE_gpencil_frame_duplicate(gpf_src, true);
        gpf_dst->framenum = CFRA + frame_offset;
        gpf_dst->flag &= ~GP_FRAME_SELECT;
        BLI_addtail(&gpl_dst->frames, gpf_dst);

        LISTBASE_FOREACH (bGPDstroke *, gps, &gpf_dst->strokes) {
          /* Create material of the stroke. */
          Material *ma_src = BKE_object_material_get(elem->ob, gps->mat_nr + 1);
          bool found = false;
          for (int index = 0; index < ob_gpencil->totcol; index++) {
            Material *ma_dst = BKE_object_material_get(ob_gpencil, index + 1);
            if (ma_src == ma_dst) {
              found = true;
              break;
            }
          }
          if (!found) {
            BKE_object_material_slot_add(bmain, ob_gpencil);
            BKE_object_material_assign(
                bmain, ob_gpencil, ma_src, ob_gpencil->totcol, BKE_MAT_ASSIGN_USERPREF);
          }

          /* Set new material index. */
          gps->mat_nr = BKE_gpencil_object_material_index_get(ob_gpencil, ma_src);

          /* Update point location to new object space. */
          for (int j = 0; j < gps->totpoints; j++) {
            bGPDspoint *pt = &gps->points[j];
            mul_m4_v3(matrix, &pt->x);
            mul_m4_v3(invmat, &pt->x);
          }

          /* Reproject stroke. */
          if (project_type != GP_REPROJECT_KEEP) {
            ED_gpencil_stroke_reproject(
                depsgraph, &gsc, sctx, gpl_dst, gpf_dst, gps, project_type, false);
          }
          else {
            BKE_gpencil_stroke_geometry_update(gpd_dst, gps);
          }
        }
      }
    }
  }
  /* Return scene frame state and DB to original state. */
  CFRA = oldframe;
  BKE_scene_graph_update_for_newframe(depsgraph);

  /* Free memory. */
  gpencil_bake_free_ob_list(&ob_selected_list);
  if (sctx != NULL) {
    ED_transform_snap_object_context_destroy(sctx);
  }
  /* Free temp hash table. */
  if (keyframe_list != NULL) {
    BLI_ghash_free(keyframe_list, NULL, NULL);
  }

  /* Notifiers. */
  DEG_relations_tag_update(bmain);
  DEG_id_tag_update(&scene->id, ID_RECALC_SELECT);
  DEG_id_tag_update(&gpd_dst->id, ID_RECALC_COPY_ON_WRITE);
  WM_event_add_notifier(C, NC_OBJECT | NA_ADDED, NULL);
  WM_event_add_notifier(C, NC_SCENE | ND_OB_ACTIVE, scene);

  /* Reset cursor. */
  WM_cursor_wait(false);

  /* done */
  return OPERATOR_FINISHED;
}

static int gpencil_bake_grease_pencil_animation_invoke(bContext *C,
                                                       wmOperator *op,
                                                       const wmEvent *UNUSED(event))
{
  PropertyRNA *prop;
  Scene *scene = CTX_data_scene(C);

  prop = RNA_struct_find_property(op->ptr, "frame_start");
  if (!RNA_property_is_set(op->ptr, prop)) {
    const int frame_start = RNA_property_int_get(op->ptr, prop);
    if (frame_start < scene->r.sfra) {
      RNA_property_int_set(op->ptr, prop, scene->r.sfra);
    }
  }

  prop = RNA_struct_find_property(op->ptr, "frame_end");
  if (!RNA_property_is_set(op->ptr, prop)) {
    const int frame_end = RNA_property_int_get(op->ptr, prop);
    if (frame_end > scene->r.efra) {
      RNA_property_int_set(op->ptr, prop, scene->r.efra);
    }
  }

  /* Show popup dialog to allow editing. */
  return WM_operator_props_dialog_popup(C, op, 250);
}

void GPENCIL_OT_bake_grease_pencil_animation(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Bake Object Transform to Grease Pencil";
  ot->idname = "GPENCIL_OT_bake_grease_pencil_animation";
  ot->description = "Bake grease pencil object transform to grease pencil keyframes";

  /* callbacks */
  ot->invoke = gpencil_bake_grease_pencil_animation_invoke;
  ot->exec = gpencil_bake_grease_pencil_animation_exec;
  ot->poll = gpencil_bake_grease_pencil_animation_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  prop = RNA_def_int(
      ot->srna, "frame_start", 1, 1, 100000, "Start Frame", "The start frame", 1, 100000);

  prop = RNA_def_int(
      ot->srna, "frame_end", 250, 1, 100000, "End Frame", "The end frame of animation", 1, 100000);
  RNA_def_property_update_runtime(prop, gpencil_bake_set_frame_end);

  prop = RNA_def_int(ot->srna, "step", 1, 1, 100, "Step", "Step between generated frames", 1, 100);

  RNA_def_boolean(
      ot->srna, "only_selected", 0, "Only Selected Keyframes", "Convert only selected keyframes");
  RNA_def_int(
      ot->srna, "frame_target", 1, 1, 100000, "Target Frame", "Destination frame", 1, 100000);

  RNA_def_enum(ot->srna,
               "project_type",
               rna_gpencil_reproject_type_items,
               GP_REPROJECT_KEEP,
               "Projection Type",
               "");
}
