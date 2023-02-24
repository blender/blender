/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2019 Blender Foundation. All rights reserved. */

#include "usd.h"
#include "usd_asset_utils.h"
#include "usd_common.h"
#include "usd_hierarchy_iterator.h"
#include "usd_light_convert.h"
#include "usd_umm.h"
#include "usd_writer_material.h"
#include "usd_writer_skel_root.h"

#include <pxr/base/plug/registry.h>
#include <pxr/pxr.h>
#include <pxr/usd/kind/registry.h>
#include <pxr/usd/usd/modelAPI.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/primRange.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdGeom/metrics.h>
#include <pxr/usd/usdGeom/scope.h>
#include <pxr/usd/usdGeom/tokens.h>
#include <pxr/usd/usdGeom/xformCommonAPI.h>
#include <pxr/usd/usdUtils/dependencies.h>

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
#include "BKE_image.h"
#include "BKE_image_save.h"
#include "BKE_image_format.h"
#include "BKE_lib_id.h"

#include "BLI_fileops.h"
#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"
#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_timeit.hh"

#include "WM_api.h"
#include "WM_types.h"

namespace blender::io::usd {

struct ExportJobData {
  Scene *scene;
  ViewLayer *view_layer;
  Main *bmain;
  Depsgraph *depsgraph;
  wmWindowManager *wm;

  char filepath[FILE_MAX];
  char usdz_filepath[FILE_MAX];
  bool is_usdz_export;
  USDExportParams params;

  float *progress;

  bool was_canceled;
  bool export_ok;
  timeit::TimePoint start_time;
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

  if (params.export_usd_kind && params.default_prim_kind == USD_KIND_CUSTOM && strlen(params.default_prim_custom_kind) == 0) {
    WM_reportf(RPT_ERROR,
               "USD Export: Default Prim Kind is set to Custom, but the value is empty.");
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

  DEGObjectIterSettings deg_iter_settings{};
  deg_iter_settings.depsgraph = depsgraph;
  deg_iter_settings.flags = DEG_ITER_OBJECT_FLAG_LINKED_DIRECTLY |
                            DEG_ITER_OBJECT_FLAG_LINKED_VIA_SET;
  DEG_OBJECT_ITER_BEGIN (&deg_iter_settings, object) {

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

  /* Handle root prim USD Kind. */
  if (params.export_usd_kind && params.default_prim_kind) {
    pxr::UsdModelAPI api(root_prim);
    switch (params.default_prim_kind) {
      case USD_KIND_COMPONENT:
        api.SetKind(pxr::KindTokens->component);
        break;

      case USD_KIND_GROUP:
        api.SetKind(pxr::KindTokens->group);
        break;

      case USD_KIND_ASSEMBLY:
        api.SetKind(pxr::KindTokens->assembly);
        break;

      case USD_KIND_CUSTOM:
        api.SetKind(pxr::TfToken(params.default_prim_custom_kind));
        break;

      default:
        break;
    }
  }
}

static void report_job_duration(const ExportJobData *data)
{
  timeit::Nanoseconds duration = timeit::Clock::now() - data->start_time;
  const char *export_filepath = data->is_usdz_export ? data->usdz_filepath : data->filepath;
  std::cout << "USD export of '" << export_filepath << "' took ";
  timeit::print_duration(duration);
  std::cout << '\n';
}

static void process_usdz_textures(const ExportJobData *data, const char *path) {
  const eUSDZTextureDownscaleSize enum_value = data->params.usdz_downscale_size;
  if (enum_value == USD_TEXTURE_SIZE_KEEP) {
    return;
  }

  int image_size = (
      (enum_value == USD_TEXTURE_SIZE_CUSTOM ? data->params.usdz_downscale_custom_size : enum_value)
  );

  image_size = image_size < 128 ? 128 : image_size;

  char texture_path[4096];
  BLI_strcpy_rlen(texture_path, path);
  BLI_path_append(texture_path, 4096, "textures");
  BLI_path_slash_ensure(texture_path, sizeof(texture_path));

  struct direntry *entries;
  unsigned int num_files = BLI_filelist_dir_contents(texture_path, &entries);

  for (int index = 0; index < num_files; index++) {
    /* We can skip checking extensions as this folder is only created
     * when we're doing a USDZ export. */
    if (!BLI_is_dir(entries[index].path)) {
      Image *im = BKE_image_load_ex(data->bmain, entries[index].path, LIB_ID_CREATE_NO_MAIN);
      if (!im) {
        std::cerr << "-- Unable to open file for downscaling: " << entries[index].path << std::endl;
        continue;
      }

      int width, height;
      BKE_image_get_size(im, NULL, &width, &height);
      const int longest = width >= height ? width : height;
      const float scale = 1.0 / ((float)longest / (float)image_size);

      if (longest > image_size) {
        const int width_adjusted  = (float)width * scale;
        const int height_adjusted = (float)height * scale;
        BKE_image_scale(im, width_adjusted, height_adjusted);

        ImageSaveOptions opts;

        if (BKE_image_save_options_init(&opts, data->bmain, data->scene, im, NULL, false, false)) {
          bool result = BKE_image_save(NULL, data->bmain, im, NULL, &opts);
          if (!result) {
            std::cerr << "-- Unable to resave " << data->filepath << " (new size: "
                      << width_adjusted << "x" << height_adjusted << ")" << std::endl;
          }
          else {
            std::cout << "Downscaled " << entries[index].path << " to "
                      << width_adjusted << "x" << height_adjusted << std::endl;
          }
        }

        BKE_image_save_options_free(&opts);
      }

      /* Make sure to free the image so it doesn't stick
       * around in the library of the open file. */
      BKE_id_free(data->bmain, (void*)im);
    }
  }

  BLI_filelist_free(entries, num_files);
}

static bool perform_usdz_conversion(const ExportJobData *data)
{
  char usdc_temp_dir[FILE_MAX], usdc_file[FILE_MAX];
  BLI_split_dirfile(data->filepath, usdc_temp_dir, usdc_file, FILE_MAX, FILE_MAX);

  char usdz_file[FILE_MAX];
  BLI_split_file_part(data->usdz_filepath, usdz_file, FILE_MAX);

  char original_working_dir[FILE_MAX];
  BLI_current_working_dir(original_working_dir, FILE_MAX);
  BLI_change_working_dir(usdc_temp_dir);

  process_usdz_textures(data, usdc_temp_dir);

  if (data->params.usdz_is_arkit) {
    std::cout << "USDZ Export: Creating ARKit Asset" << std::endl;
    pxr::UsdUtilsCreateNewARKitUsdzPackage(pxr::SdfAssetPath(usdc_file), usdz_file);
  }
  else {
    pxr::UsdUtilsCreateNewUsdzPackage(pxr::SdfAssetPath(usdc_file), usdz_file);
  }
  BLI_change_working_dir(original_working_dir);

  char usdz_temp_dirfile[FILE_MAX];
  BLI_path_join(usdz_temp_dirfile, FILE_MAX, usdc_temp_dir, usdz_file);

  int result = 0;
  if (BLI_exists(data->usdz_filepath)) {
    result = BLI_delete(data->usdz_filepath, false, false);
    if (result != 0) {
      WM_reportf(
          RPT_ERROR, "USD Export: Unable to delete existing usdz file %s", data->usdz_filepath);
      return false;
    }
  }
  if (!copy_asset(usdz_temp_dirfile, data->usdz_filepath, USD_TEX_NAME_COLLISION_OVERWRITE)) {
    WM_reportf(RPT_ERROR,
               "USD Export: Couldn't copy new usdz file from temporary location %s to %s",
               usdz_temp_dirfile,
               data->usdz_filepath);
    return false;
  }

  return true;
}

static void export_startjob(void *customdata,
                            /* Cannot be const, this function implements wm_jobs_start_callback.
                             * NOLINTNEXTLINE: readability-non-const-parameter. */
                            bool *stop,
                            bool *do_update,
                            float *progress)
{
  ExportJobData *data = static_cast<ExportJobData *>(customdata);

  data->progress = progress;
  data->was_canceled = false;
  data->start_time = timeit::Clock::now();

  G.is_rendering = true;
  if (data->wm) {
    WM_set_locked_interface(data->wm, true);
  }
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
  const int orig_frame = scene->r.cfra;

  if (!BLI_path_extension_check_glob(data->filepath, "*.usd;*.usda;*.usdc"))
    BLI_path_extension_ensure(data->filepath, FILE_MAX, ".usd");

  pxr::UsdStageRefPtr usd_stage = pxr::UsdStage::CreateNew(data->filepath);
  if (!usd_stage) {
    /* This may happen when the USD JSON files cannot be found. When that happens,
     * the USD library doesn't know it has the functionality to write USDA and
     * USDC files, and creating a new UsdStage fails. */
    WM_reportf(RPT_ERROR, "USD Export: unable to create a stage for writing %s", data->filepath);

    pxr::SdfLayerRefPtr existing_layer = pxr::SdfLayer::FindOrOpen(data->filepath);
    if (existing_layer) {
      WM_reportf(RPT_ERROR,
                 "USD Export: layer %s is currently open in the scene, "
                 "possibly because it's referenced by modifiers, "
                 "and can't be overwritten",
                 data->filepath);
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

  /* Add any Blender-specific custom export data */
  if (data->params.export_blender_metadata && strlen(data->bmain->filepath)) {
    auto root_layer = usd_stage->GetRootLayer();
    char full_path[1024];
    strcpy(full_path, data->bmain->filepath);

    // make all paths uniformly unix-like
    BLI_str_replace_char(full_path + 2, SEP, ALTSEP);

    char basename[128];
    strcpy(basename, BLI_path_basename(full_path));
    BLI_split_dir_part(full_path, full_path, 1024);

    pxr::VtDictionary custom_data;
    custom_data.SetValueAtPath(std::string("sourceFilename"), pxr::VtValue(basename));
    custom_data.SetValueAtPath(std::string("sourceDirPath"), pxr::VtValue(full_path));
    root_layer->SetCustomLayerData(custom_data);
  }

  /* Set up the stage for animated data. */
  if (data->params.export_animation) {
    usd_stage->SetTimeCodesPerSecond(FPS);
    usd_stage->SetStartTimeCode(data->params.frame_start);
    usd_stage->SetEndTimeCode(data->params.frame_end);
  }

  ensure_root_prim(usd_stage, data->params);

  USDHierarchyIterator iter(data->bmain, data->depsgraph, usd_stage, data->params);

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
      scene->r.cfra = int(frame);
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

  /* Set the default prim if it doesn't exist */
  if (!usd_stage->GetDefaultPrim()) {
    /* Use TraverseAll since it's guaranteed to be depth first and will get the first top level
     * prim, and is less verbose than getting the PseudoRoot + iterating its children.*/
    for (auto prim : usd_stage->TraverseAll()) {
      usd_stage->SetDefaultPrim(prim);
      break;
    }
  }

  /* Set unit scale.
   * TODO(makowalsk): Add an option to use scene->unit.scale_length as well? */
  double meters_per_unit = data->params.convert_to_cm ? pxr::UsdGeomLinearUnits::centimeters :
                                                        pxr::UsdGeomLinearUnits::meters;
  pxr::UsdGeomSetStageMetersPerUnit(usd_stage, meters_per_unit);

  usd_stage->GetRootLayer()->Save();

  if (data->is_usdz_export) {
    if (!perform_usdz_conversion(data)) {
      return;
    }
  }

  /* Finish up by going back to the keyframe that was current before we started. */
  if (scene->r.cfra != orig_frame) {
    scene->r.cfra = orig_frame;
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

  if (data->is_usdz_export && BLI_exists(data->filepath))
  {
    char dir[FILE_MAX];
    BLI_split_dir_part(data->filepath, dir, FILE_MAX);
    char usdc_temp_dir[FILE_MAX];
    BLI_path_join(usdc_temp_dir, FILE_MAX, BKE_tempdir_session(), "USDZ", SEP_STR);
    BLI_assert(BLI_strcasecmp(dir, usdc_temp_dir) == 0);
    BLI_delete(usdc_temp_dir, true, true);
  }

  MEM_freeN(data->params.default_prim_path);
  MEM_freeN(data->params.root_prim_path);
  MEM_freeN(data->params.material_prim_path);
  MEM_freeN(data->params.default_prim_custom_kind);

  if (data->was_canceled && BLI_exists(data->filepath)) {
    BLI_delete(data->filepath, false, false);
  }

  G.is_rendering = false;
  if (data->wm) {
    WM_set_locked_interface(data->wm, false);
  }
  report_job_duration(data);
}

}  // namespace blender::io::usd

/* To create a usdz file, we must first create a .usd/a/c file and then covert it to .usdz. The
 * temporary files will be created in Blender's temporary session storage. The .usdz file will then
 * copied to job->usdz_filepath. */
static void create_temp_path_for_usdz_export(const char *filepath,
                                             blender::io::usd::ExportJobData *job)
{
  char file[FILE_MAX];
  BLI_split_file_part(filepath, file, FILE_MAX);
  char *usdc_file = BLI_str_replaceN(file, ".usdz", ".usdc");

  char usdc_temp_filepath[FILE_MAX];
  BLI_path_join(usdc_temp_filepath, FILE_MAX, BKE_tempdir_session(), "USDZ", usdc_file);

  BLI_strncpy(job->filepath, usdc_temp_filepath, strlen(usdc_temp_filepath) + 1);
  BLI_strncpy(job->usdz_filepath, filepath, strlen(filepath) + 1);

  MEM_freeN(usdc_file);
}

bool USD_export(bContext *C,
                const char *filepath,
                const USDExportParams *params,
                bool as_background_job)
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Scene *scene = CTX_data_scene(C);

  blender::io::usd::ExportJobData *job = static_cast<blender::io::usd::ExportJobData *>(
      MEM_mallocN(sizeof(blender::io::usd::ExportJobData), "ExportJobData"));

  job->scene = scene;
  job->bmain = CTX_data_main(C);
  job->wm = CTX_wm_manager(C);
  job->export_ok = false;
  job->is_usdz_export = false;
  if (BLI_path_extension_check_n(filepath, ".usd", ".usda", ".usdc", NULL)) {
    BLI_strncpy(job->filepath, filepath, sizeof(job->filepath));
  }
  else if (BLI_path_extension_check_n(filepath, ".usdz", NULL)) {
    create_temp_path_for_usdz_export(filepath, job);
    job->is_usdz_export = true;
  }

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
    bool stop = false, do_update = false;
    float progress = 0.0f;

    blender::io::usd::export_startjob(job, &stop, &do_update, &progress);
    blender::io::usd::export_endjob(job);
    export_ok = job->export_ok;

    MEM_freeN(job);
  }

  return export_ok;
}

int USD_get_version()
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
