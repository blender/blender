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
 * The Original Code is Copyright (C) 2019 Blender Foundation.
 * All rights reserved.
 */

#include "usd.h"
#include "usd_common.h"
#include "usd_hierarchy_iterator.h"
#include "usd_light_convert.h"
#include "usd_umm.h"
#include "usd_writer_material.h"
#include "usd_writer_skel_root.h"

#include <pxr/base/plug/registry.h>
#include <pxr/pxr.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdGeom/metrics.h>
#include <pxr/usd/usdGeom/scope.h>
#include <pxr/usd/usdGeom/tokens.h>
#include <pxr/usd/usdGeom/xformCommonAPI.h>

#include "MEM_guardedalloc.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"
#include "DEG_depsgraph_query.h"

#include "DNA_scene_types.h"

#include "BKE_appdir.h"
#include "BKE_blender_version.h"
#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_scene.h"

#include "BLI_fileops.h"
#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"
#include "BLI_path_util.h"
#include "BLI_string.h"

#include "WM_api.h"
#include "WM_types.h"

namespace blender::io::usd {

struct ExportJobData {
  ViewLayer *view_layer;
  Main *bmain;
  Depsgraph *depsgraph;
  wmWindowManager *wm;

  char filename[FILE_MAX];
  USDExportParams params;

  short *stop;
  short *do_update;
  float *progress;

  bool was_canceled;
  bool export_ok;
};

/* Perform validation of export parameter settings. Returns
 * true if the paramters are valid.  Returns false otherwise. */
static bool validate_params(const USDExportParams &params)
{
  bool valid = true;

  if (params.export_materials && !pxr::SdfPath::IsValidPathString(params.material_prim_path)) {
    WM_reportf(RPT_ERROR,
               "USD Export: invalid material prim path parameter '%s'",
               params.material_prim_path);
    valid = false;
  }

  if (strlen(params.root_prim_path) != 0 &&
      !pxr::SdfPath::IsValidPathString(params.root_prim_path)) {
    WM_reportf(
        RPT_ERROR, "USD Export: invalid root prim path parameter '%s'", params.root_prim_path);
    valid = false;
  }

  if (strlen(params.default_prim_path) != 0 &&
      !pxr::SdfPath::IsValidPathString(params.default_prim_path)) {
    WM_reportf(RPT_ERROR,
               "USD Export: invalid default prim path parameter '%s'",
               params.default_prim_path);
    valid = false;
  }

  return valid;
}

/* If a root prim path is set in the params, check if a
 * root object matching the root path name already exists.
 * If it does, clear the root prim path in the params.
 * This is to avoid prepending the root prim path
 * redundantly.
 * TODO(makowalski): ideally, this functionality belongs
 * in the USD hierarchy iterator, so that we don't iterate
 * over the scene graph separately here. */
static void validate_unique_root_prim_path(USDExportParams &params, Depsgraph *depsgraph)
{
  if (!depsgraph || strlen(params.root_prim_path) == 0) {
    return;
  }

  pxr::SdfPath path(params.root_prim_path);

  if (path.IsEmpty()) {
    return;
  }

  pxr::SdfPath parent = path.GetParentPath();

  while (!parent.IsEmpty() && !parent.IsAbsoluteRootPath()) {
    path = parent;
    parent = path.GetParentPath();
  }

  Object *match = nullptr;
  std::string root_name = path.GetName();

  DEG_OBJECT_ITER_BEGIN(depsgraph,
    object,
    DEG_ITER_OBJECT_FLAG_LINKED_DIRECTLY |
    DEG_ITER_OBJECT_FLAG_LINKED_VIA_SET) {

    if (!match && !object->parent) {
      /* We only care about root objects. */

      if (pxr::TfMakeValidIdentifier(object->id.name + 2) == root_name) {
        match = object;
      }
    }
  }
  DEG_OBJECT_ITER_END;

  if (match) {
    WM_reportf(
      RPT_WARNING, "USD Export: the root prim will not be added because a root object named '%s' already exists", root_name.c_str());
    params.root_prim_path[0] = '\0';
  }
}

/* Create root prim if defined. */
static void ensure_root_prim(pxr::UsdStageRefPtr stage, const USDExportParams &params)
{
  if (strlen(params.root_prim_path) == 0) {
    return;
  }

  pxr::UsdPrim root_prim = stage->DefinePrim(pxr::SdfPath(params.root_prim_path),
                                             pxr::TfToken("Xform"));

  if (!(params.convert_orientation || params.convert_to_cm)) {
    return;
  }

  if (!root_prim) {
    return;
  }

  pxr::UsdGeomXformCommonAPI xf_api(root_prim);

  if (!xf_api) {
    return;
  }

  if (params.convert_to_cm) {
    xf_api.SetScale(pxr::GfVec3f(100.0f));
  }

  if (params.convert_orientation) {
    float mrot[3][3];
    mat3_from_axis_conversion(
        USD_GLOBAL_FORWARD_Y, USD_GLOBAL_UP_Z, params.forward_axis, params.up_axis, mrot);
    transpose_m3(mrot);

    float eul[3];
    mat3_to_eul(eul, mrot);

    /* Convert radians to degrees. */
    mul_v3_fl(eul, 180.0f / M_PI);

    xf_api.SetRotate(pxr::GfVec3f(eul[0], eul[1], eul[2]));
  }
}

static void export_startjob(void *customdata,
                            /* Cannot be const, this function implements wm_jobs_start_callback.
                             * NOLINTNEXTLINE: readability-non-const-parameter. */
                            short *stop,
                            short *do_update,
                            float *progress)
{
  ExportJobData *data = static_cast<ExportJobData *>(customdata);

  data->stop = stop;
  data->do_update = do_update;
  data->progress = progress;
  data->was_canceled = false;

  G.is_rendering = true;
  WM_set_locked_interface(data->wm, true);
  G.is_break = false;

  if (!validate_params(data->params)) {
    data->export_ok = false;
    return;
  }

  /* Construct the depsgraph for exporting. */
  Scene *scene = DEG_get_input_scene(data->depsgraph);
  if (data->params.visible_objects_only) {
    DEG_graph_build_from_view_layer(data->depsgraph);
  }
  else {
    DEG_graph_build_for_all_objects(data->depsgraph);
  }
  BKE_scene_graph_update_tagged(data->depsgraph, data->bmain);

  validate_unique_root_prim_path(data->params, data->depsgraph);

  *progress = 0.0f;
  *do_update = true;

  /* For restoring the current frame after exporting animation is done. */
  const int orig_frame = CFRA;

  if (!BLI_path_extension_check_glob(data->filename, "*.usd;*.usda;*.usdc"))
    BLI_path_extension_ensure(data->filename, FILE_MAX, ".usd");

  pxr::UsdStageRefPtr usd_stage = pxr::UsdStage::CreateNew(data->filename);
  if (!usd_stage) {
    /* This may happen when the USD JSON files cannot be found. When that happens,
     * the USD library doesn't know it has the functionality to write USDA and
     * USDC files, and creating a new UsdStage fails. */
    WM_reportf(RPT_ERROR, "USD Export: unable to create a stage for writing %s", data->filename);

    pxr::SdfLayerRefPtr existing_layer = pxr::SdfLayer::FindOrOpen(data->filename);
    if (existing_layer) {
      WM_reportf(RPT_ERROR,
                 "USD Export: layer %s is currently open in the scene, "
                 "possibly because it's referenced by modifiers, "
                 "and can't be overwritten",
                 data->filename);
    }

    data->export_ok = false;
    return;
  }

  if (data->params.export_lights && !data->params.selected_objects_only &&
      data->params.convert_world_material) {
    world_material_to_dome_light(data->params, scene, usd_stage);
  }

  /* Define the material prim path as a scope. */
  if (data->params.export_materials) {
    pxr::SdfPath mtl_prim_path(data->params.material_prim_path);

    blender::io::usd::usd_define_or_over<pxr::UsdGeomScope>(
        usd_stage, mtl_prim_path, data->params.export_as_overs);
  }

  pxr::VtValue upAxis = pxr::VtValue(pxr::UsdGeomTokens->z);
  if (data->params.convert_orientation) {
    if (data->params.up_axis == USD_GLOBAL_UP_X)
      upAxis = pxr::VtValue(pxr::UsdGeomTokens->x);
    else if (data->params.up_axis == USD_GLOBAL_UP_Y)
      upAxis = pxr::VtValue(pxr::UsdGeomTokens->y);
  }

  usd_stage->SetMetadata(pxr::UsdGeomTokens->upAxis, upAxis);
  usd_stage->GetRootLayer()->SetDocumentation(std::string("Blender v") +
                                              BKE_blender_version_string());

  /* Set up the stage for animated data. */
  if (data->params.export_animation) {
    usd_stage->SetTimeCodesPerSecond(FPS);
    usd_stage->SetStartTimeCode(data->params.frame_start);
    usd_stage->SetEndTimeCode(data->params.frame_end);
  }

  ensure_root_prim(usd_stage, data->params);

  USDHierarchyIterator iter(data->depsgraph, usd_stage, data->params);

  if (data->params.export_animation) {

    // Writing the animated frames is not 100% of the work, but it's our best guess.
    float progress_per_frame = 1.0f / std::max(1.0f,
                                               (float)(data->params.frame_end -
                                                       data->params.frame_start + 1.0) /
                                                   data->params.frame_step);

    for (float frame = data->params.frame_start; frame <= data->params.frame_end;
         frame += data->params.frame_step) {
      if (G.is_break || (stop != nullptr && *stop)) {
        break;
      }

      /* Update the scene for the next frame to render. */
      scene->r.cfra = static_cast<int>(frame);
      scene->r.subframe = frame - scene->r.cfra;
      BKE_scene_graph_update_for_newframe(data->depsgraph);

      iter.set_export_frame(frame);
      iter.iterate_and_write();

      *progress += progress_per_frame;
      *do_update = true;
    }
  }
  else {
    /* If we're not animating, a single iteration over all objects is enough. */
    iter.iterate_and_write();
  }

  iter.release_writers();

  if (data->params.export_armatures) {
    validate_skel_roots(usd_stage, data->params);
  }

  // Set Stage Default Prim Path
  if (strlen(data->params.default_prim_path) > 0) {
    std::string valid_default_prim_path = pxr::TfMakeValidIdentifier(
        data->params.default_prim_path);

    if (valid_default_prim_path[0] == '_') {
      valid_default_prim_path[0] = '/';
    }
    if (valid_default_prim_path[0] != '/') {
      valid_default_prim_path = "/" + valid_default_prim_path;
    }

    pxr::UsdPrim defaultPrim = usd_stage->GetPrimAtPath(pxr::SdfPath(valid_default_prim_path));

    if (defaultPrim.IsValid()) {
      usd_stage->SetDefaultPrim(defaultPrim);
    }
  }

  /* Set unit scale.
   * TODO(makowalsk): Add an option to use scene->unit.scale_length as well? */
  double meters_per_unit = data->params.convert_to_cm ? pxr::UsdGeomLinearUnits::centimeters :
                                                        pxr::UsdGeomLinearUnits::meters;
  pxr::UsdGeomSetStageMetersPerUnit(usd_stage, meters_per_unit);

  usd_stage->GetRootLayer()->Save();

  /* Finish up by going back to the keyframe that was current before we started. */
  if (CFRA != orig_frame) {
    CFRA = orig_frame;
    BKE_scene_graph_update_for_newframe(data->depsgraph);
  }

  data->export_ok = !data->was_canceled;

  *progress = 1.0f;
  *do_update = true;
}

static void export_endjob(void *customdata)
{
  ExportJobData *data = static_cast<ExportJobData *>(customdata);

  DEG_graph_free(data->depsgraph);

  MEM_freeN(data->params.default_prim_path);
  MEM_freeN(data->params.root_prim_path);
  MEM_freeN(data->params.material_prim_path);

  if (data->was_canceled && BLI_exists(data->filename)) {
    BLI_delete(data->filename, false, false);
  }

  G.is_rendering = false;
  WM_set_locked_interface(data->wm, false);
}

}  // namespace blender::io::usd

bool USD_export(bContext *C,
                const char *filepath,
                const USDExportParams *params,
                bool as_background_job)
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Scene *scene = CTX_data_scene(C);

  blender::io::usd::ensure_usd_plugin_path_registered();

  blender::io::usd::ExportJobData *job = static_cast<blender::io::usd::ExportJobData *>(
      MEM_mallocN(sizeof(blender::io::usd::ExportJobData), "ExportJobData"));

  job->bmain = CTX_data_main(C);
  job->wm = CTX_wm_manager(C);
  job->export_ok = false;
  BLI_strncpy(job->filename, filepath, sizeof(job->filename));

  job->depsgraph = DEG_graph_new(job->bmain, scene, view_layer, params->evaluation_mode);
  job->params = *params;

  bool export_ok = false;
  if (as_background_job) {
    wmJob *wm_job = WM_jobs_get(
        job->wm, CTX_wm_window(C), scene, "USD Export", WM_JOB_PROGRESS, WM_JOB_TYPE_ALEMBIC);

    /* setup job */
    WM_jobs_customdata_set(wm_job, job, MEM_freeN);
    WM_jobs_timer(wm_job, 0.1, NC_SCENE | ND_FRAME, NC_SCENE | ND_FRAME);
    WM_jobs_callbacks(wm_job,
                      blender::io::usd::export_startjob,
                      nullptr,
                      nullptr,
                      blender::io::usd::export_endjob);

    WM_jobs_start(CTX_wm_manager(C), wm_job);
  }
  else {
    /* Fake a job context, so that we don't need NULL pointer checks while exporting. */
    short stop = 0, do_update = 0;
    float progress = 0.0f;

    blender::io::usd::export_startjob(job, &stop, &do_update, &progress);
    blender::io::usd::export_endjob(job);
    export_ok = job->export_ok;

    MEM_freeN(job);
  }

  return export_ok;
}

int USD_get_version(void)
{
  /* USD 19.11 defines:
   *
   * #define PXR_MAJOR_VERSION 0
   * #define PXR_MINOR_VERSION 19
   * #define PXR_PATCH_VERSION 11
   * #define PXR_VERSION 1911
   *
   * So the major version is implicit/invisible in the public version number.
   */
  return PXR_VERSION;
}

bool USD_umm_module_loaded(void)
{
#ifdef WITH_PYTHON
  return blender::io::usd::umm_module_loaded();
#else
  return fasle;
#endif
}
