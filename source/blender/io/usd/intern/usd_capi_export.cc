/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <fmt/core.h>

#include "IO_subdiv_disabler.hh"
#include "usd.hh"
#include "usd_hierarchy_iterator.hh"
#include "usd_hook.hh"
#include "usd_instancing_utils.hh"
#include "usd_light_convert.hh"
#include "usd_private.hh"

#include <pxr/base/tf/token.h>
#include <pxr/pxr.h>
#include <pxr/usd/sdf/assetPath.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/usd/usd/primRange.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdGeom/metrics.h>
#include <pxr/usd/usdGeom/pointInstancer.h>
#include <pxr/usd/usdGeom/tokens.h>
#include <pxr/usd/usdGeom/xform.h>
#include <pxr/usd/usdGeom/xformCommonAPI.h>
#include <pxr/usd/usdUtils/usdzPackage.h>

#include "MEM_guardedalloc.h"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"
#include "DEG_depsgraph_query.hh"

#include "DNA_collection_types.h"
#include "DNA_scene_types.h"

#include "BKE_appdir.hh"
#include "BKE_blender_version.h"
#include "BKE_context.hh"
#include "BKE_global.hh"
#include "BKE_image.hh"
#include "BKE_image_save.hh"
#include "BKE_lib_id.hh"
#include "BKE_report.hh"
#include "BKE_scene.hh"

#include "BLI_fileops.h"
#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"
#include "BLI_path_utils.hh"
#include "BLI_string.h"
#include "BLI_timeit.hh"

#include <IMB_imbuf.hh>
#include <IMB_imbuf_types.hh>

#include "WM_api.hh"
#include "WM_types.hh"

#include "CLG_log.h"
static CLG_LogRef LOG = {"io.usd"};

namespace blender::io::usd {

struct ExportJobData {
  Main *bmain = nullptr;
  Depsgraph *depsgraph = nullptr;
  wmWindowManager *wm = nullptr;
  Scene *scene = nullptr;

  /** Unarchived_filepath is used for USDA/USDC/USD export. */
  char unarchived_filepath[FILE_MAX] = {};
  char usdz_filepath[FILE_MAX] = {};
  USDExportParams params = {};

  bool export_ok = false;
  timeit::TimePoint start_time = {};

  bool targets_usdz() const
  {
    return usdz_filepath[0] != '\0';
  }

  const char *export_filepath() const
  {
    if (targets_usdz()) {
      return usdz_filepath;
    }
    return unarchived_filepath;
  }
};

/* Returns true if the given prim path is valid, per
 * the requirements of the prim path manipulation logic
 * of the exporter. Also returns true if the path is
 * the empty string. Returns false otherwise. */
static bool prim_path_valid(const std::string &path)
{
  if (path.empty()) {
    /* Empty paths are ignored in the code,
     * so they can be passed through. */
    return true;
  }

  /* Check path syntax. */
  std::string errMsg;
  if (!pxr::SdfPath::IsValidPathString(path, &errMsg)) {
    WM_global_reportf(
        RPT_ERROR, "USD Export: invalid path string '%s': %s", path.c_str(), errMsg.c_str());
    return false;
  }

  /* Verify that an absolute prim path can be constructed
   * from this path string. */

  pxr::SdfPath sdf_path(path);
  if (!sdf_path.IsAbsolutePath()) {
    WM_global_reportf(RPT_ERROR, "USD Export: path '%s' is not an absolute path", path.c_str());
    return false;
  }

  if (!sdf_path.IsPrimPath()) {
    WM_global_reportf(RPT_ERROR, "USD Export: path string '%s' is not a prim path", path.c_str());
    return false;
  }

  return true;
}

/**
 * Perform validation of export parameter settings.
 * \return true if the parameters are valid; returns false otherwise.
 *
 * \warning Do not call from worker thread, only from main thread (i.e. before starting the wmJob).
 */
static bool export_params_valid(const USDExportParams &params)
{
  bool valid = true;

  if (!prim_path_valid(params.root_prim_path)) {
    valid = false;
  }

  return valid;
}

/**
 * Create the root Xform primitive, if the Root Prim path has been set
 * in the export options. In the future, this function can be extended
 * to author transforms and additional schema data (e.g., model Kind)
 * on the root prim.
 */
static void ensure_root_prim(pxr::UsdStageRefPtr stage, const USDExportParams &params)
{
  if (params.root_prim_path.empty()) {
    return;
  }

  pxr::UsdGeomXform root_xf = pxr::UsdGeomXform::Define(stage,
                                                        pxr::SdfPath(params.root_prim_path));

  if (!root_xf) {
    return;
  }

  pxr::UsdGeomXformCommonAPI xf_api(root_xf.GetPrim());

  if (!xf_api) {
    return;
  }

  if (params.convert_scene_units) {
    xf_api.SetScale(pxr::GfVec3f(float(1.0 / get_meters_per_unit(params))));
  }

  if (params.convert_orientation) {
    float mrot[3][3];
    mat3_from_axis_conversion(IO_AXIS_Y, IO_AXIS_Z, params.forward_axis, params.up_axis, mrot);
    transpose_m3(mrot);

    float eul[3];
    mat3_to_eul(eul, mrot);

    /* Convert radians to degrees. */
    mul_v3_fl(eul, 180.0f / M_PI);

    xf_api.SetRotate(pxr::GfVec3f(eul[0], eul[1], eul[2]));
  }

  for (const auto &path : pxr::SdfPath(params.root_prim_path).GetPrefixes()) {
    auto xform = pxr::UsdGeomXform::Define(stage, path);
    /* Tag generated primitives to allow filtering on import. */
    xform.GetPrim().SetCustomDataByKey(pxr::TfToken("Blender:generated"), pxr::VtValue(true));
  }
}

static void report_job_duration(const ExportJobData *data)
{
  timeit::Nanoseconds duration = timeit::Clock::now() - data->start_time;
  const char *export_filepath = data->export_filepath();
  fmt::print("USD export of '{}' took ", export_filepath);
  timeit::print_duration(duration);
  fmt::print("\n");
}

static void process_usdz_textures(const ExportJobData *data, const char *path)
{
  const eUSDZTextureDownscaleSize enum_value = data->params.usdz_downscale_size;
  if (enum_value == USD_TEXTURE_SIZE_KEEP) {
    return;
  }

  const int image_size = (enum_value == USD_TEXTURE_SIZE_CUSTOM) ?
                             data->params.usdz_downscale_custom_size :
                             enum_value;

  char texture_path[FILE_MAX];
  STRNCPY(texture_path, path);
  BLI_path_append(texture_path, FILE_MAX, "textures");
  BLI_path_slash_ensure(texture_path, sizeof(texture_path));

  direntry *entries;
  uint num_files = BLI_filelist_dir_contents(texture_path, &entries);

  for (int index = 0; index < num_files; index++) {
    /* We can skip checking extensions as this folder is only created
     * when we're doing a USDZ export. */
    if (!BLI_is_dir(entries[index].path)) {
      Image *im = BKE_image_load(data->bmain, entries[index].path);
      if (!im) {
        CLOG_WARN(&LOG, "Unable to open file for downscaling: %s", entries[index].path);
        continue;
      }

      int width, height;
      BKE_image_get_size(im, nullptr, &width, &height);
      const int longest = width >= height ? width : height;
      const float scale = 1.0 / (float(longest) / float(image_size));

      if (longest > image_size) {
        const int width_adjusted = float(width) * scale;
        const int height_adjusted = float(height) * scale;
        BKE_image_scale(im, width_adjusted, height_adjusted, nullptr);

        ImageSaveOptions opts;

        if (BKE_image_save_options_init(
                &opts, data->bmain, data->scene, im, nullptr, false, false))
        {
          bool result = BKE_image_save(nullptr, data->bmain, im, nullptr, &opts);
          if (!result) {
            CLOG_ERROR(&LOG,
                       "Unable to resave '%s' (new size: %dx%d)",
                       data->usdz_filepath,
                       width_adjusted,
                       height_adjusted);
          }
          else {
            CLOG_DEBUG(&LOG,
                       "Downscaled '%s' to %dx%d",
                       entries[index].path,
                       width_adjusted,
                       height_adjusted);
          }
        }

        BKE_image_save_options_free(&opts);
      }

      /* Make sure to free the image so it doesn't stick
       * around in the library of the open file. */
      BKE_id_free(data->bmain, (void *)im);
    }
  }

  BLI_filelist_free(entries, num_files);
}

/**
 * For usdz export, we must first create a usd/a/c file and then covert it to usdz. In Blender's
 * case, we first create a usdc file in Blender's temporary working directory, and store the path
 * to the usdc file in `unarchived_filepath`. This function then does the conversion of that usdc
 * file into usdz.
 *
 * \return true when the conversion from usdc to usdz is successful.
 */
static bool perform_usdz_conversion(const ExportJobData *data)
{
  char usdc_temp_dir[FILE_MAX], usdc_file[FILE_MAX];
  BLI_path_split_dir_file(data->unarchived_filepath,
                          usdc_temp_dir,
                          sizeof(usdc_temp_dir),
                          usdc_file,
                          sizeof(usdc_file));

  char usdz_file[FILE_MAX];
  BLI_path_split_file_part(data->usdz_filepath, usdz_file, FILE_MAX);

  char original_working_dir_buff[FILE_MAX];
  const char *original_working_dir = BLI_current_working_dir(original_working_dir_buff,
                                                             sizeof(original_working_dir_buff));
  /* Buffer is expected to be returned by #BLI_current_working_dir, although in theory other
   * returns are possible on some platforms, this is not handled by this code. */
  BLI_assert(original_working_dir == original_working_dir_buff);

  BLI_change_working_dir(usdc_temp_dir);

  process_usdz_textures(data, usdc_temp_dir);

  pxr::UsdUtilsCreateNewUsdzPackage(pxr::SdfAssetPath(usdc_file), usdz_file);
  BLI_change_working_dir(original_working_dir);

  char usdz_temp_full_path[FILE_MAX];
  BLI_path_join(usdz_temp_full_path, FILE_MAX, usdc_temp_dir, usdz_file);

  int result = 0;
  if (BLI_exists(data->usdz_filepath)) {
    result = BLI_delete(data->usdz_filepath, false, false);
    if (result != 0) {
      BKE_reportf(data->params.worker_status->reports,
                  RPT_ERROR,
                  "USD Export: Unable to delete existing usdz file %s",
                  data->usdz_filepath);
      return false;
    }
  }
  result = BLI_path_move(usdz_temp_full_path, data->usdz_filepath);
  if (result != 0) {
    BKE_reportf(data->params.worker_status->reports,
                RPT_ERROR,
                "USD Export: Couldn't move new usdz file from temporary location %s to %s",
                usdz_temp_full_path,
                data->usdz_filepath);
    return false;
  }

  return true;
}

std::string image_cache_file_path()
{
  char dir_path[FILE_MAX];
  BLI_path_join(dir_path, sizeof(dir_path), BKE_tempdir_session(), "usd", "image_cache");
  return dir_path;
}

std::string get_image_cache_file(const std::string &file_name, bool mkdir)
{
  std::string dir_path = image_cache_file_path();
  if (mkdir) {
    BLI_dir_create_recursive(dir_path.c_str());
  }

  char file_path[FILE_MAX];
  BLI_path_join(file_path, sizeof(file_path), dir_path.c_str(), file_name.c_str());
  return file_path;
}

std::string cache_image_color(const float color[4])
{
  std::string name = fmt::format("color_{:02X}{:02X}{:02X}.exr",
                                 int(color[0] * 255),
                                 int(color[1] * 255),
                                 int(color[2] * 255));
  std::string file_path = get_image_cache_file(name);
  if (BLI_exists(file_path.c_str())) {
    return file_path;
  }

  ImBuf *ibuf = IMB_allocImBuf(1, 1, 32, IB_float_data);
  IMB_rectfill(ibuf, color);
  ibuf->ftype = IMB_FTYPE_OPENEXR;
  ibuf->foptions.flag = R_IMF_EXR_CODEC_RLE;

  if (IMB_save_image(ibuf, file_path.c_str(), IB_float_data)) {
    CLOG_INFO(&LOG, "%s", file_path.c_str());
  }
  else {
    CLOG_ERROR(&LOG, "Can't save %s", file_path.c_str());
    file_path = "";
  }
  IMB_freeImBuf(ibuf);

  return file_path;
}

static void collect_point_instancer_prototypes_and_set_extent(
    pxr::UsdGeomPointInstancer instancer,
    const pxr::UsdStageRefPtr &stage,
    const pxr::SdfPath &wrapper_path,
    std::vector<pxr::UsdPrim> &proto_list)
{
  /* Compute extent of the current point instancer. */
  pxr::VtArray<pxr::GfVec3f> extent;
  instancer.ComputeExtentAtTime(&extent, pxr::UsdTimeCode::Default(), pxr::UsdTimeCode::Default());
  instancer.CreateExtentAttr().Set(extent);

  pxr::UsdPrim wrapper_prim = stage->GetPrimAtPath(wrapper_path);
  if (!wrapper_prim || !wrapper_prim.IsValid()) {
    return;
  }

  std::string real_path_str;

  for (const pxr::SdfPrimSpecHandle &primSpec : wrapper_prim.GetPrimStack()) {
    if (!primSpec || !primSpec->HasReferences()) {
      continue;
    }

    for (const pxr::SdfReference &ref : primSpec->GetReferenceList().GetPrependedItems()) {
      if (ref.GetAssetPath().empty() && !ref.GetPrimPath().IsEmpty()) {
        real_path_str = ref.GetPrimPath().GetString();
        break;
      }
    }
    if (!real_path_str.empty()) {
      break;
    }
  }

  if (real_path_str.empty()) {
    CLOG_WARN(&LOG, "No prototype reference found for: %s", wrapper_path.GetText());
    return;
  }

  const pxr::SdfPath real_path(real_path_str);
  pxr::UsdPrim proto_prim = stage->GetPrimAtPath(real_path);

  if (!proto_prim || !proto_prim.IsValid()) {
    CLOG_WARN(&LOG, "Referenced prototype not found at: %s", real_path.GetText());
    return;
  }

  proto_list.push_back(proto_prim);
  proto_list.push_back(wrapper_prim.GetParent());

  std::string doc_message = fmt::format(
      "This prim is used as a prototype by the PointInstancer \"{}\" so we override the def "
      "with an \"over\" so that it isn't imaged in the scene, but is available as a prototype "
      "that can be referenced.",
      wrapper_prim.GetName().GetString());
  proto_prim.SetDocumentation(doc_message);

  /* Check if the proto prim itself is a PointInstancer. */
  if (proto_prim.IsA<pxr::UsdGeomPointInstancer>()) {
    pxr::UsdGeomPointInstancer nested_instancer(proto_prim);
    pxr::SdfPathVector nested_targets;
    if (nested_instancer.GetPrototypesRel().GetTargets(&nested_targets)) {
      for (const pxr::SdfPath &nested_wrapper_path : nested_targets) {
        collect_point_instancer_prototypes_and_set_extent(
            nested_instancer, stage, nested_wrapper_path, proto_list);
      }
    }
  }

  /* Also check all children of the proto prim for nested PointInstancers. */
  for (const pxr::UsdPrim &child : proto_prim.GetAllChildren()) {
    if (child.IsA<pxr::UsdGeomPointInstancer>()) {
      pxr::UsdGeomPointInstancer nested_instancer(child);
      pxr::SdfPathVector nested_targets;
      if (nested_instancer.GetPrototypesRel().GetTargets(&nested_targets)) {
        for (const pxr::SdfPath &nested_wrapper_path : nested_targets) {
          collect_point_instancer_prototypes_and_set_extent(
              nested_instancer, stage, nested_wrapper_path, proto_list);
        }
      }
    }
  }
}

pxr::UsdStageRefPtr export_to_stage(const USDExportParams &params,
                                    Depsgraph *depsgraph,
                                    const char *filepath)
{
  pxr::UsdStageRefPtr usd_stage = pxr::UsdStage::CreateNew(filepath);
  if (!usd_stage) {
    return usd_stage;
  }

  wmJobWorkerStatus *worker_status = params.worker_status;
  Scene *scene = DEG_get_input_scene(depsgraph);
  Main *bmain = DEG_get_bmain(depsgraph);

  SubdivModifierDisabler mod_disabler(depsgraph);

  /* If we want to set the subdiv scheme, then we need to the export the mesh
   * without the subdiv modifier applied. */
  if (ELEM(params.export_subdiv, USD_SUBDIV_BEST_MATCH, USD_SUBDIV_IGNORE)) {
    mod_disabler.disable_modifiers();
    BKE_scene_graph_update_tagged(depsgraph, bmain);
  }

  /* This whole `export_to_stage` function is assumed to cover about 80% of the whole export
   * process, from 0.1f to 0.9f. */
  worker_status->progress = 0.10f;
  worker_status->do_update = true;

  usd_stage->SetMetadata(pxr::UsdGeomTokens->metersPerUnit, double(scene->unit.scale_length));
  usd_stage->GetRootLayer()->SetDocumentation(std::string("Blender v") +
                                              BKE_blender_version_string());

  /* Set up the stage for animated data. */
  if (params.export_animation) {
    usd_stage->SetTimeCodesPerSecond(scene->frames_per_second());
    usd_stage->SetStartTimeCode(scene->r.sfra);
    usd_stage->SetEndTimeCode(scene->r.efra);
  }

  /* For restoring the current frame after exporting animation is done. */
  const int orig_frame = scene->r.cfra;

  /* Ensure Python types for invoking hooks are registered. */
  register_hook_converters();

  pxr::VtValue upAxis = pxr::VtValue(pxr::UsdGeomTokens->z);
  if (params.convert_orientation) {
    if (params.up_axis == IO_AXIS_X) {
      upAxis = pxr::VtValue(pxr::UsdGeomTokens->x);
    }
    else if (params.up_axis == IO_AXIS_Y) {
      upAxis = pxr::VtValue(pxr::UsdGeomTokens->y);
    }
  }

  usd_stage->SetMetadata(pxr::UsdGeomTokens->upAxis, upAxis);

  const double meters_per_unit = get_meters_per_unit(params);
  pxr::UsdGeomSetStageMetersPerUnit(usd_stage, meters_per_unit);

  ensure_root_prim(usd_stage, params);

  USDHierarchyIterator iter(bmain, depsgraph, usd_stage, params);

  worker_status->progress = 0.11f;
  worker_status->do_update = true;

  if (params.export_animation) {
    /* Writing the animated frames is not 100% of the work, here it's assumed to be 75% of it. */
    float progress_per_frame = 0.75f / std::max(1, (scene->r.efra - scene->r.sfra + 1));

    for (float frame = scene->r.sfra; frame <= scene->r.efra; frame++) {
      if (G.is_break || worker_status->stop) {
        break;
      }

      /* Update the scene for the next frame to render. */
      scene->r.cfra = int(frame);
      scene->r.subframe = frame - scene->r.cfra;
      BKE_scene_graph_update_for_newframe(depsgraph);

      iter.set_export_frame(frame);
      iter.iterate_and_write();

      worker_status->progress += progress_per_frame;
      worker_status->do_update = true;
    }
  }
  else {
    /* If we're not animating, a single iteration over all objects is enough. */
    iter.iterate_and_write();
  }

  worker_status->progress = 0.86f;
  worker_status->do_update = true;

  iter.release_writers();

  if (params.export_shapekeys || params.export_armatures) {
    iter.process_usd_skel();
  }

  /* Creating dome lights should be called after writers have
   * completed, to avoid a name collision when creating the light
   * prim. */
  if (params.convert_world_material) {
    world_material_to_dome_light(params, scene, usd_stage);
  }

  /* Set the default prim if it doesn't exist */
  if (!usd_stage->GetDefaultPrim()) {
    /* Use TraverseAll since it's guaranteed to be depth first and will get the first top level
     * prim, and is less verbose than getting the PseudoRoot + iterating its children. */
    for (auto prim : usd_stage->TraverseAll()) {
      usd_stage->SetDefaultPrim(prim);
      break;
    }
  }

  if (params.use_instancing) {
    process_scene_graph_instances(params, usd_stage);
  }

  call_export_hooks(depsgraph, &iter, params.worker_status->reports);

  worker_status->progress = 0.88f;
  worker_status->do_update = true;

  /* Finish up by going back to the keyframe that was current before we started. */
  if (scene->r.cfra != orig_frame) {
    scene->r.cfra = orig_frame;
    BKE_scene_graph_update_for_newframe(depsgraph);
  }

  worker_status->progress = 0.9f;
  worker_status->do_update = true;

  return usd_stage;
}

static void export_startjob(void *customdata, wmJobWorkerStatus *worker_status)
{
  ExportJobData *data = static_cast<ExportJobData *>(customdata);
  data->export_ok = false;
  data->start_time = timeit::Clock::now();

  G.is_rendering = true;
  if (data->wm) {
    WM_locked_interface_set(data->wm, true);
  }
  G.is_break = false;

  worker_status->progress = 0.01f;
  worker_status->do_update = true;

  /* Evaluate the depsgraph for exporting.
   *
   * Note that, unlike with its building, this is expected to be safe to perform from worker
   * thread, since UI is locked during export, so there should not be any more changes in the Main
   * original data concurrently done from the main thread at this point. All necessary (deferred)
   * changes are expected to have been triggered and processed during depsgraph building in
   * #USD_export. */
  BKE_scene_graph_update_tagged(data->depsgraph, data->bmain);

  worker_status->progress = 0.1f;
  worker_status->do_update = true;
  data->params.worker_status = worker_status;

  pxr::UsdStageRefPtr usd_stage = export_to_stage(
      data->params, data->depsgraph, data->unarchived_filepath);
  if (!usd_stage) {
    /* This happens when the USD JSON files cannot be found. When that happens,
     * the USD library doesn't know it has the functionality to write USDA and
     * USDC files, and creating a new UsdStage fails. */
    BKE_reportf(worker_status->reports,
                RPT_ERROR,
                "USD Export: unable to find suitable USD plugin to write %s",
                data->unarchived_filepath);
    return;
  }

  /* Traverse the point instancer to make sure the prototype referenced by nested point instancers
   * are also marked as over. */
  std::vector<pxr::UsdPrim> proto_list;
  for (const pxr::UsdPrim &prim : usd_stage->Traverse()) {
    if (!prim.IsA<pxr::UsdGeomPointInstancer>()) {
      continue;
    }
    pxr::UsdGeomPointInstancer instancer(prim);
    pxr::SdfPathVector targets;
    if (instancer.GetPrototypesRel().GetTargets(&targets)) {
      for (const pxr::SdfPath &wrapper_path : targets) {
        collect_point_instancer_prototypes_and_set_extent(
            instancer, usd_stage, wrapper_path, proto_list);
      }
    }
  }

  /* The standard way is to mark the point instancer's prototypes as over. Reference in OpenUSD:
   * https://openusd.org/docs/api/class_usd_geom_point_instancer.html#:~:text=place%20them%20under%20a%20prim%20that%20is%20just%20an%20%22over%22
   */
  for (pxr::UsdPrim &proto : proto_list) {
    proto.SetSpecifier(pxr::SdfSpecifierOver);
  }

  usd_stage->GetRootLayer()->Save();

  data->export_ok = true;
  worker_status->progress = 1.0f;
  worker_status->do_update = true;
}

static void export_endjob_usdz_cleanup(const ExportJobData *data)
{
  if (!BLI_exists(data->unarchived_filepath)) {
    return;
  }

  char dir[FILE_MAX];
  BLI_path_split_dir_part(data->unarchived_filepath, dir, FILE_MAX);

  char usdc_temp_dir[FILE_MAX];
  BLI_path_join(usdc_temp_dir, FILE_MAX, BKE_tempdir_session(), "USDZ", SEP_STR);

  BLI_assert_msg(BLI_strcasecmp(dir, usdc_temp_dir) == 0,
                 "USD Export: Attempting to delete directory that doesn't match the expected "
                 "temporary directory for usdz export.");
  BLI_delete(usdc_temp_dir, true, true);
}

static void export_endjob(void *customdata)
{
  ExportJobData *data = static_cast<ExportJobData *>(customdata);

  DEG_graph_free(data->depsgraph);

  if (data->targets_usdz()) {
    /* NOTE: call to #perform_usdz_conversion has to be done here instead of the main threaded
     * worker callback (#export_startjob) because USDZ conversion requires changing the current
     * working directory. This is not safe to do from a non-main thread. Once the USD library fix
     * this weird requirement, this call can be moved back at the end of #export_startjob, and not
     * block the main user interface anymore. */
    bool usd_conversion_success = perform_usdz_conversion(data);
    if (!usd_conversion_success) {
      data->export_ok = false;
    }

    export_endjob_usdz_cleanup(data);
  }

  if (!data->export_ok && BLI_exists(data->unarchived_filepath)) {
    BLI_delete(data->unarchived_filepath, false, false);
  }

  G.is_rendering = false;
  if (data->wm) {
    WM_locked_interface_set(data->wm, false);
  }
  report_job_duration(data);
}

/**
 * To create a USDZ file, we must first create a `.usd/a/c` file and then covert it to `.usdz`.
 * The temporary files will be created in Blender's temporary session storage.
 * The `.usdz` file will then be moved to `job->usdz_filepath`.
 */
static void create_temp_path_for_usdz_export(const char *filepath,
                                             blender::io::usd::ExportJobData *job)
{
  char usdc_file[FILE_MAX];
  STRNCPY(usdc_file, BLI_path_basename(filepath));

  if (BLI_path_extension_check(usdc_file, ".usdz")) {
    BLI_path_extension_replace(usdc_file, sizeof(usdc_file), ".usdc");
  }

  char usdc_temp_filepath[FILE_MAX];
  BLI_path_join(usdc_temp_filepath, FILE_MAX, BKE_tempdir_session(), "USDZ", usdc_file);

  STRNCPY(job->unarchived_filepath, usdc_temp_filepath);
  STRNCPY(job->usdz_filepath, filepath);
}

static void set_job_filepath(blender::io::usd::ExportJobData *job, const char *filepath)
{
  if (BLI_path_extension_check_n(filepath, ".usdz", nullptr)) {
    create_temp_path_for_usdz_export(filepath, job);
    return;
  }

  STRNCPY(job->unarchived_filepath, filepath);
  job->usdz_filepath[0] = '\0';
}

bool USD_export(const bContext *C,
                const char *filepath,
                const USDExportParams *params,
                bool as_background_job,
                ReportList *reports)
{
  if (!blender::io::usd::export_params_valid(*params)) {
    return false;
  }

  ViewLayer *view_layer = CTX_data_view_layer(C);
  Scene *scene = CTX_data_scene(C);

  blender::io::usd::ExportJobData *job = MEM_new<blender::io::usd::ExportJobData>("ExportJobData");

  job->bmain = CTX_data_main(C);
  job->wm = CTX_wm_manager(C);
  job->scene = scene;
  job->export_ok = false;
  set_job_filepath(job, filepath);

  job->depsgraph = DEG_graph_new(job->bmain, scene, view_layer, params->evaluation_mode);
  job->params = *params;

  /* Construct the depsgraph for exporting.
   *
   * Has to be done from main thread currently, as it may affect Main original data (e.g. when
   * doing deferred update of the view-layers, see #112534 for details). */
  if (job->params.collection[0]) {
    Collection *collection = reinterpret_cast<Collection *>(
        BKE_libblock_find_name(job->bmain, ID_GR, job->params.collection));
    if (!collection) {
      BKE_reportf(reports,
                  RPT_ERROR,
                  "USD Export: Unable to find collection '%s'",
                  job->params.collection);
      return false;
    }

    DEG_graph_build_from_collection(job->depsgraph, collection);
  }
  else {
    DEG_graph_build_from_view_layer(job->depsgraph);
  }

  bool export_ok = false;
  if (as_background_job) {
    wmJob *wm_job = WM_jobs_get(job->wm,
                                CTX_wm_window(C),
                                scene,
                                "Exporting USD...",
                                WM_JOB_PROGRESS,
                                WM_JOB_TYPE_USD_EXPORT);

    /* setup job */
    WM_jobs_customdata_set(wm_job, job, [](void *j) {
      MEM_delete(static_cast<blender::io::usd::ExportJobData *>(j));
    });
    WM_jobs_timer(wm_job, 0.1, NC_SCENE | ND_FRAME, NC_SCENE | ND_FRAME);
    WM_jobs_callbacks(wm_job,
                      blender::io::usd::export_startjob,
                      nullptr,
                      nullptr,
                      blender::io::usd::export_endjob);

    WM_jobs_start(CTX_wm_manager(C), wm_job);
  }
  else {
    wmJobWorkerStatus worker_status = {};
    /* Use the operator's reports in non-background case. */
    worker_status.reports = reports;

    blender::io::usd::export_startjob(job, &worker_status);
    blender::io::usd::export_endjob(job);
    export_ok = job->export_ok;

    MEM_delete(job);
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

double get_meters_per_unit(const USDExportParams &params)
{
  double result;
  switch (params.convert_scene_units) {
    case USD_SCENE_UNITS_CENTIMETERS:
      result = 0.01;
      break;
    case USD_SCENE_UNITS_MILLIMETERS:
      result = 0.001;
      break;
    case USD_SCENE_UNITS_KILOMETERS:
      result = 1000.0;
      break;
    case USD_SCENE_UNITS_INCHES:
      result = 0.0254;
      break;
    case USD_SCENE_UNITS_FEET:
      result = 0.3048;
      break;
    case USD_SCENE_UNITS_YARDS:
      result = 0.9144;
      break;
    case USD_SCENE_UNITS_CUSTOM:
      result = double(params.custom_meters_per_unit);
      break;
    default:
      result = 1.0;
      break;
  }

  return result;
}

}  // namespace blender::io::usd
