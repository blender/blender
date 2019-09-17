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
 * The Original Code is Copyright (C) 2019 by Blender Foundation
 * All rights reserved.
 */

/** \file
 * \ingroup edobj
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <ctype.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "DNA_scene_types.h"
#include "DNA_object_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_mesh_types.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_mesh.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_customdata.h"
#include "BKE_mesh_remesh_voxel.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"

#include "ED_mesh.h"
#include "ED_object.h"
#include "ED_screen.h"
#include "ED_sculpt.h"
#include "ED_undo.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "WM_api.h"
#include "WM_types.h"
#include "WM_message.h"
#include "WM_toolsystem.h"

#include "object_intern.h"  // own include

static bool object_remesh_poll(bContext *C)
{
  Object *ob = CTX_data_active_object(C);

  if (ob == NULL) {
    return false;
  }

  if (BKE_object_is_in_editmode(ob)) {
    CTX_wm_operator_poll_msg_set(C, "The voxel remesher cannot run from edit mode.");
    return false;
  }

  if (ob->mode == OB_MODE_SCULPT && ob->sculpt->bm) {
    CTX_wm_operator_poll_msg_set(C, "The voxel remesher cannot run with dyntopo activated.");
  }

  return ED_operator_object_active_editable_mesh(C);
}

static int voxel_remesh_exec(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);

  Mesh *mesh = ob->data;
  Mesh *new_mesh;

  if (mesh->remesh_voxel_size <= 0.0f) {
    BKE_report(op->reports, RPT_ERROR, "Voxel remesher cannot run with a voxel size of 0.0.");
    return OPERATOR_CANCELLED;
  }

  if (ob->mode == OB_MODE_SCULPT) {
    ED_sculpt_undo_geometry_begin(ob);
  }

  new_mesh = BKE_mesh_remesh_voxel_to_mesh_nomain(mesh, mesh->remesh_voxel_size);

  if (!new_mesh) {
    return OPERATOR_CANCELLED;
  }

  Mesh *obj_mesh_copy = NULL;
  if (mesh->flag & ME_REMESH_REPROJECT_PAINT_MASK) {
    obj_mesh_copy = BKE_mesh_new_nomain_from_template(mesh, mesh->totvert, 0, 0, 0, 0);
    CustomData_copy(
        &mesh->vdata, &obj_mesh_copy->vdata, CD_MASK_MESH.vmask, CD_DUPLICATE, mesh->totvert);
    for (int i = 0; i < mesh->totvert; i++) {
      copy_v3_v3(obj_mesh_copy->mvert[i].co, mesh->mvert[i].co);
    }
  }

  BKE_mesh_nomain_to_mesh(new_mesh, mesh, ob, &CD_MASK_MESH, true);

  if (mesh->flag & ME_REMESH_REPROJECT_PAINT_MASK) {
    BKE_remesh_reproject_paint_mask(mesh, obj_mesh_copy);
    BKE_mesh_free(obj_mesh_copy);
  }

  if (mesh->flag & ME_REMESH_SMOOTH_NORMALS) {
    BKE_mesh_smooth_flag_set(ob->data, true);
  }

  if (ob->mode == OB_MODE_SCULPT) {
    ED_sculpt_undo_geometry_end(ob);
  }

  BKE_mesh_batch_cache_dirty_tag(ob->data, BKE_MESH_BATCH_DIRTY_ALL);
  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GEOM | ND_DATA, ob->data);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_voxel_remesh(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Voxel Remesh";
  ot->description =
      "Calculates a new manifold mesh based on the volume of the current mesh. All data layers "
      "will be lost";
  ot->idname = "OBJECT_OT_voxel_remesh";

  /* api callbacks */
  ot->poll = object_remesh_poll;
  ot->exec = voxel_remesh_exec;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

enum {
  QUADRIFLOW_REMESH_RATIO = 1,
  QUADRIFLOW_REMESH_EDGE_LENGTH,
  QUADRIFLOW_REMESH_FACES,
};

/****************** quadriflow remesh operator *********************/

typedef struct QuadriFlowJob {
  /* from wmJob */
  struct Object *owner;
  short *stop, *do_update;
  float *progress;

  int target_faces;
  int seed;
  bool use_preserve_sharp;
  bool use_preserve_boundary;
  bool use_mesh_curvature;

  bool preserve_paint_mask;
  bool smooth_normals;

  int success;
} QuadriFlowJob;

static void quadriflow_free_job(void *customdata)
{
  QuadriFlowJob *qj = customdata;
  MEM_freeN(qj);
}

/* called by quadriflowjob, only to check job 'stop' value */
static int quadriflow_break_job(void *customdata)
{
  QuadriFlowJob *qj = (QuadriFlowJob *)customdata;
  // return *(qj->stop);

  /* this is not nice yet, need to make the jobs list template better
   * for identifying/acting upon various different jobs */
  /* but for now we'll reuse the render break... */
  bool should_break = (G.is_break);

  if (should_break) {
    qj->success = -1;
  }

  return should_break;
}

/* called by oceanbake, wmJob sends notifier */
static void quadriflow_update_job(void *customdata, float progress, int *cancel)
{
  QuadriFlowJob *qj = customdata;

  if (quadriflow_break_job(qj)) {
    *cancel = 1;
  }
  else {
    *cancel = 0;
  }

  *(qj->do_update) = true;
  *(qj->progress) = progress;
}

static void quadriflow_start_job(void *customdata, short *stop, short *do_update, float *progress)
{
  QuadriFlowJob *qj = customdata;

  qj->stop = stop;
  qj->do_update = do_update;
  qj->progress = progress;
  qj->success = 1;

  G.is_break = false; /* XXX shared with render - replace with job 'stop' switch */

  Object *ob = qj->owner;
  Mesh *mesh = ob->data;
  Mesh *new_mesh;

  new_mesh = BKE_mesh_remesh_quadriflow_to_mesh_nomain(mesh,
                                                       qj->target_faces,
                                                       qj->seed,
                                                       qj->use_preserve_sharp,
                                                       qj->use_preserve_boundary,
                                                       qj->use_mesh_curvature,
                                                       quadriflow_update_job,
                                                       (void *)qj);

  if (!new_mesh) {
    *do_update = true;
    *stop = 0;
    if (qj->success == 1) {
      /* This is not a user cancelation event */
      qj->success = 0;
    }
    return;
  }

  if (ob->mode == OB_MODE_SCULPT) {
    ED_sculpt_undo_geometry_begin(ob);
  }

  Mesh *obj_mesh_copy = NULL;
  if (qj->preserve_paint_mask) {
    obj_mesh_copy = BKE_mesh_new_nomain_from_template(mesh, mesh->totvert, 0, 0, 0, 0);
    CustomData_copy(
        &mesh->vdata, &obj_mesh_copy->vdata, CD_MASK_MESH.vmask, CD_DUPLICATE, mesh->totvert);
    for (int i = 0; i < mesh->totvert; i++) {
      copy_v3_v3(obj_mesh_copy->mvert[i].co, mesh->mvert[i].co);
    }
  }

  BKE_mesh_nomain_to_mesh(new_mesh, mesh, ob, &CD_MASK_MESH, true);

  if (qj->preserve_paint_mask) {
    BKE_remesh_reproject_paint_mask(mesh, obj_mesh_copy);
    BKE_mesh_free(obj_mesh_copy);
  }

  if (qj->smooth_normals) {
    BKE_mesh_smooth_flag_set(ob->data, true);
  }

  if (ob->mode == OB_MODE_SCULPT) {
    ED_sculpt_undo_geometry_end(ob);
  }

  BKE_mesh_batch_cache_dirty_tag(ob->data, BKE_MESH_BATCH_DIRTY_ALL);

  *do_update = true;
  *stop = 0;
}

static void quadriflow_end_job(void *customdata)
{
  QuadriFlowJob *qj = customdata;

  Object *ob = qj->owner;

  WM_set_locked_interface(G_MAIN->wm.first, false);

  if (qj->success > 0) {
    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
    WM_reportf(RPT_INFO, "QuadriFlow: Completed remeshing!");
  }
  else {
    if (qj->success == 0) {
      WM_reportf(RPT_ERROR, "QuadriFlow: remeshing failed!");
    }
    else {
      WM_report(RPT_WARNING, "QuadriFlow: remeshing canceled!");
    }
  }
}

static int quadriflow_remesh_exec(bContext *C, wmOperator *op)
{
  QuadriFlowJob *job = MEM_mallocN(sizeof(QuadriFlowJob), "QuadriFlowJob");

  job->owner = CTX_data_active_object(C);

  job->target_faces = RNA_int_get(op->ptr, "target_faces");
  job->seed = RNA_int_get(op->ptr, "seed");

  job->use_preserve_sharp = RNA_boolean_get(op->ptr, "use_preserve_sharp");
  job->use_preserve_boundary = RNA_boolean_get(op->ptr, "use_preserve_boundary");

  job->use_mesh_curvature = RNA_boolean_get(op->ptr, "use_mesh_curvature");

  job->preserve_paint_mask = RNA_boolean_get(op->ptr, "preserve_paint_mask");
  job->smooth_normals = RNA_boolean_get(op->ptr, "smooth_normals");

  wmJob *wm_job = WM_jobs_get(CTX_wm_manager(C),
                              CTX_wm_window(C),
                              CTX_data_scene(C),
                              "QuadriFlow Remesh",
                              WM_JOB_PROGRESS,
                              WM_JOB_TYPE_QUADRIFLOW_REMESH);

  WM_jobs_customdata_set(wm_job, job, quadriflow_free_job);
  WM_jobs_timer(wm_job, 0.1, NC_GEOM | ND_DATA, NC_GEOM | ND_DATA);
  WM_jobs_callbacks(wm_job, quadriflow_start_job, NULL, NULL, quadriflow_end_job);

  WM_set_locked_interface(CTX_wm_manager(C), true);

  WM_jobs_start(CTX_wm_manager(C), wm_job);

  return OPERATOR_FINISHED;
}

static bool quadriflow_check(bContext *C, wmOperator *op)
{
  int mode = RNA_enum_get(op->ptr, "mode");

  if (mode == QUADRIFLOW_REMESH_EDGE_LENGTH) {
    float area = RNA_float_get(op->ptr, "mesh_area");
    if (area < 0.0f) {
      Object *ob = CTX_data_active_object(C);
      area = BKE_mesh_calc_area(ob->data);
      RNA_float_set(op->ptr, "mesh_area", area);
    }
    int num_faces;
    float edge_len = RNA_float_get(op->ptr, "target_edge_length");

    num_faces = area / (edge_len * edge_len);
    RNA_int_set(op->ptr, "target_faces", num_faces);
  }
  else if (mode == QUADRIFLOW_REMESH_RATIO) {
    Object *ob = CTX_data_active_object(C);
    Mesh *mesh = ob->data;

    int num_faces;
    float ratio = RNA_float_get(op->ptr, "target_ratio");

    num_faces = mesh->totpoly * ratio;

    RNA_int_set(op->ptr, "target_faces", num_faces);
  }

  return true;
}

/* Hide the target variables if they are not active */
static bool quadriflow_poll_property(const bContext *C, wmOperator *op, const PropertyRNA *prop)
{
  const char *prop_id = RNA_property_identifier(prop);

  if (STRPREFIX(prop_id, "target")) {
    int mode = RNA_enum_get(op->ptr, "mode");

    if (STREQ(prop_id, "target_edge_length") && mode != QUADRIFLOW_REMESH_EDGE_LENGTH) {
      return false;
    }
    else if (STREQ(prop_id, "target_faces")) {
      if (mode != QUADRIFLOW_REMESH_FACES) {
        /* Make sure we can edit the target_faces value even if it doesn't start as EDITABLE */
        float area = RNA_float_get(op->ptr, "mesh_area");
        if (area < -0.8f) {
          area += 0.2f;
          /* Make sure we have up to date values from the start */
          RNA_def_property_flag((PropertyRNA *)prop, PROP_EDITABLE);
          quadriflow_check((bContext *)C, op);
        }

        /* Only disable input */
        RNA_def_property_clear_flag((PropertyRNA *)prop, PROP_EDITABLE);
      }
      else {
        RNA_def_property_flag((PropertyRNA *)prop, PROP_EDITABLE);
      }
    }
    else if (STREQ(prop_id, "target_ratio") && mode != QUADRIFLOW_REMESH_RATIO) {
      return false;
    }
  }

  return true;
}

static const EnumPropertyItem mode_type_items[] = {
    {QUADRIFLOW_REMESH_RATIO,
     "RATIO",
     0,
     "Ratio",
     "Specify target number of faces relative to the current mesh"},
    {QUADRIFLOW_REMESH_EDGE_LENGTH,
     "EDGE",
     0,
     "Edge Length",
     "Input target edge length in the new mesh"},
    {QUADRIFLOW_REMESH_FACES, "FACES", 0, "Faces", "Input target number of faces in the new mesh"},
    {0, NULL, 0, NULL, NULL},
};

void OBJECT_OT_quadriflow_remesh(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "QuadriFlow Remesh";
  ot->description =
      "Create a new quad based mesh using the surface data of the current mesh. All data "
      "layers will be lost";
  ot->idname = "OBJECT_OT_quadriflow_remesh";

  /* api callbacks */
  ot->poll = object_remesh_poll;
  ot->poll_property = quadriflow_poll_property;
  ot->check = quadriflow_check;
  ot->invoke = WM_operator_props_popup_confirm;
  ot->exec = quadriflow_remesh_exec;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  PropertyRNA *prop;

  /* properties */
  RNA_def_boolean(ot->srna,
                  "use_preserve_sharp",
                  false,
                  "Preserve Sharp",
                  "Try to preserve sharp features on the mesh");

  RNA_def_boolean(ot->srna,
                  "use_preserve_boundary",
                  false,
                  "Preserve Mesh Boundary",
                  "Try to preserve mesh boundary on the mesh");

  RNA_def_boolean(ot->srna,
                  "use_mesh_curvature",
                  false,
                  "Use Mesh Curvature",
                  "Take the mesh curvature into account when remeshing");

  RNA_def_boolean(ot->srna,
                  "preserve_paint_mask",
                  false,
                  "Preserve Paint Mask",
                  "Reproject the paint mask onto the new mesh");

  RNA_def_boolean(ot->srna,
                  "smooth_normals",
                  false,
                  "Smooth Normals",
                  "Set the output mesh normals to smooth");

  RNA_def_enum(ot->srna,
               "mode",
               mode_type_items,
               0,
               "Mode",
               "How to specify the amount of detail for the new mesh");

  prop = RNA_def_float(ot->srna,
                       "target_ratio",
                       1,
                       0,
                       FLT_MAX,
                       "Ratio",
                       "Relative number of faces compared to the current mesh",
                       0.0f,
                       1.0f);

  prop = RNA_def_float(ot->srna,
                       "target_edge_length",
                       0.1f,
                       0.0000001f,
                       FLT_MAX,
                       "Edge Length",
                       "Target edge length in the new mesh",
                       0.00001f,
                       1.0f);

  prop = RNA_def_int(ot->srna,
                     "target_faces",
                     1,
                     1,
                     INT_MAX,
                     "Number of Faces",
                     "Approximate number of faces (quads) in the new mesh",
                     1,
                     INT_MAX);

  prop = RNA_def_float(
      ot->srna,
      "mesh_area",
      -1.0f,
      -FLT_MAX,
      FLT_MAX,
      "Old Object Face Area",
      "This property is only used to cache the object area for later calculations",
      0.0f,
      FLT_MAX);
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);

  RNA_def_int(ot->srna,
              "seed",
              0,
              0,
              INT_MAX,
              "Seed",
              "Random seed to use with the solver. Different seeds will cause the remesher to "
              "come up with different quad layouts on the mesh",
              0,
              255);
}
