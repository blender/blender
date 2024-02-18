/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edgpencil
 * Operator for converting Grease Pencil data to geometry.
 */

#include "MEM_guardedalloc.h"

#include "BLI_ghash.h"
#include "BLI_math_rotation.h"

#include "DNA_anim_types.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BKE_anim_data.h"
#include "BKE_context.hh"
#include "BKE_duplilist.hh"
#include "BKE_gpencil_geom_legacy.h"
#include "BKE_layer.hh"
#include "BKE_material.h"
#include "BKE_object.hh"
#include "BKE_report.hh"
#include "BKE_scene.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_query.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "ED_gpencil_legacy.hh"
#include "ED_transform_snap_object_context.hh"

#include "gpencil_intern.h"

/* Check frame_end is always > start frame! */
static void gpencil_bake_set_frame_end(Main * /*main*/, Scene * /*scene*/, PointerRNA *ptr)
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

struct GpBakeOb {
  GpBakeOb *next, *prev;
  Object *ob;
};

/* Get list of keyframes used by selected objects. */
static void animdata_keyframe_list_get(ListBase *ob_list,
                                       const bool only_selected,
                                       GHash *r_keyframes)
{
  /* Loop all objects to get the list of keyframes used. */
  LISTBASE_FOREACH (GpBakeOb *, elem, ob_list) {
    Object *ob = elem->ob;
    AnimData *adt = BKE_animdata_from_id(&ob->id);
    if ((adt == nullptr) || (adt->action == nullptr)) {
      continue;
    }
    LISTBASE_FOREACH (FCurve *, fcurve, &adt->action->curves) {
      int i;
      BezTriple *bezt;
      for (i = 0, bezt = fcurve->bezt; i < fcurve->totvert; i++, bezt++) {
        /* Keyframe number is x value of point. */
        if ((bezt->f2 & SELECT) || (!only_selected)) {
          /* Insert only one key for each keyframe number. */
          int key = int(bezt->vec[1][0]);
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
  GpBakeOb *elem = nullptr;
  ListBase *lb;
  lb = object_duplilist(depsgraph, scene, ob);
  LISTBASE_FOREACH (DupliObject *, dob, lb) {
    if (dob->ob->type != OB_MESH) {
      continue;
    }
    elem = MEM_cnew<GpBakeOb>(__func__);
    elem->ob = dob->ob;
    BLI_addtail(list, elem);
  }

  free_object_duplilist(lb);
}

static bool gpencil_bake_ob_list(bContext *C, Depsgraph *depsgraph, Scene *scene, ListBase *list)
{
  GpBakeOb *elem = nullptr;
  bool simple_material = false;

  /* Add active object. In some files this could not be in selected array. */
  Object *obact = CTX_data_active_object(C);
  if (obact == nullptr) {
    return false;
  }

  if (obact->type == OB_MESH) {
    elem = MEM_cnew<GpBakeOb>(__func__);
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
    /* Add selected meshes. */
    if (ob->type == OB_MESH) {
      elem = MEM_cnew<GpBakeOb>(__func__);
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
  View3D *v3d = CTX_wm_view3d(C);

  ListBase ob_selected_list = {nullptr, nullptr};
  gpencil_bake_ob_list(C, depsgraph, scene, &ob_selected_list);

  /* Cannot check this in poll because the active object changes. */
  if (ob_selected_list.first == nullptr) {
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
  const eGP_ReprojectModes project_type = (eGP_ReprojectModes)RNA_enum_get(op->ptr,
                                                                           "project_type");
  const eGP_TargetObjectMode target = (eGP_TargetObjectMode)RNA_enum_get(op->ptr, "target");

  /* Create a new grease pencil object in origin or reuse selected. */
  Object *ob_gpencil = nullptr;
  bool newob = false;

  if (target == GP_TARGET_OB_SELECTED) {
    ob_gpencil = BKE_view_layer_non_active_selected_object(scene, CTX_data_view_layer(C), v3d);
    if (ob_gpencil != nullptr) {
      if (ob_gpencil->type != OB_GPENCIL_LEGACY) {
        BKE_report(op->reports, RPT_WARNING, "Target object not a grease pencil, ignoring!");
        ob_gpencil = nullptr;
      }
      else if (BKE_object_obdata_is_libdata(ob_gpencil)) {
        BKE_report(op->reports, RPT_WARNING, "Target object library-data, ignoring!");
        ob_gpencil = nullptr;
      }
    }
  }

  if (ob_gpencil == nullptr) {
    ushort local_view_bits = (v3d && v3d->localvd) ? v3d->local_view_uid : 0;
    const float loc[3] = {0.0f, 0.0f, 0.0f};
    ob_gpencil = ED_gpencil_add_object(C, loc, local_view_bits);
    newob = true;
  }

  bGPdata *gpd = (bGPdata *)ob_gpencil->data;
  gpd->draw_mode = (project_type == GP_REPROJECT_KEEP) ? GP_DRAWMODE_3D : GP_DRAWMODE_2D;

  /* Set cursor to indicate working. */
  WM_cursor_wait(true);

  GP_SpaceConversion gsc = {nullptr};
  SnapObjectContext *sctx = nullptr;
  if (project_type != GP_REPROJECT_KEEP) {
    /* Init space conversion stuff. */
    gpencil_point_conversion_init(C, &gsc);
    /* Move the grease pencil object to conversion data. */
    gsc.ob = ob_gpencil;

    /* Init snap context for geometry projection. */
    sctx = ED_transform_snap_object_context_create(scene, 0);

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
  int oldframe = int(DEG_get_ctime(depsgraph));
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
    if ((only_selected) && !BLI_ghash_haskey(keyframe_list, POINTER_FROM_INT(i))) {
      continue;
    }

    /* Move scene to new frame. */
    scene->r.cfra = i;
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
                               ob_eval->object_to_world().ptr(),
                               frame_offset,
                               use_seams,
                               use_faces,
                               true);

      /* Reproject all un-tagged created strokes. */
      if (project_type != GP_REPROJECT_KEEP) {
        LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
          bGPDframe *gpf = gpl->actframe;
          if (gpf == nullptr) {
            continue;
          }
          LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
            if ((gps->flag & GP_STROKE_TAG) == 0) {
              ED_gpencil_stroke_reproject(
                  depsgraph, &gsc, sctx, gpl, gpf, gps, project_type, false, 0.0f);
              gps->flag |= GP_STROKE_TAG;
            }
          }
        }
      }
    }
  }

  /* Return scene frame state and DB to original state. */
  scene->r.cfra = oldframe;
  BKE_scene_graph_update_for_newframe(depsgraph);

  /* Remove unused materials. */
  int actcol = ob_gpencil->actcol;
  for (int slot = 1; slot <= ob_gpencil->totcol; slot++) {
    while (slot <= ob_gpencil->totcol && !BKE_object_material_slot_used(ob_gpencil, slot)) {
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
  if (sctx != nullptr) {
    ED_transform_snap_object_context_destroy(sctx);
  }
  /* Free temp hash table. */
  if (keyframe_list != nullptr) {
    BLI_ghash_free(keyframe_list, nullptr, nullptr);
  }

  /* notifiers */
  if (newob) {
    DEG_relations_tag_update(bmain);
  }
  DEG_id_tag_update(&scene->id, ID_RECALC_SELECT);
  WM_event_add_notifier(C, NC_OBJECT | NA_ADDED, nullptr);
  WM_event_add_notifier(C, NC_SCENE | ND_OB_ACTIVE, scene);

  /* Reset cursor. */
  WM_cursor_wait(false);

  /* done */
  return OPERATOR_FINISHED;
}

static int gpencil_bake_mesh_animation_invoke(bContext *C,
                                              wmOperator *op,
                                              const wmEvent * /*event*/)
{
  /* Show popup dialog to allow editing. */
  /* FIXME: hard-coded dimensions here are just arbitrary. */
  return WM_operator_props_dialog_popup(C, op, 250);
}

void GPENCIL_OT_bake_mesh_animation(wmOperatorType *ot)
{
  static const EnumPropertyItem target_object_modes[] = {
      {GP_TARGET_OB_NEW, "NEW", 0, "New Object", ""},
      {GP_TARGET_OB_SELECTED, "SELECTED", 0, "Selected Object", ""},
      {0, nullptr, 0, nullptr, nullptr},
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
                                nullptr,
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

  RNA_def_boolean(ot->srna, "seams", false, "Only Seam Edges", "Convert only seam edges");
  RNA_def_boolean(ot->srna, "faces", true, "Export Faces", "Export faces as filled strokes");
  RNA_def_boolean(ot->srna,
                  "only_selected",
                  false,
                  "Only Selected Keyframes",
                  "Convert only selected keyframes");
  RNA_def_int(
      ot->srna, "frame_target", 1, 1, 100000, "Target Frame", "Destination frame", 1, 100000);

  RNA_def_enum(ot->srna,
               "project_type",
               rna_gpencil_reproject_type_items,
               GP_REPROJECT_VIEW,
               "Projection Type",
               "");
}
