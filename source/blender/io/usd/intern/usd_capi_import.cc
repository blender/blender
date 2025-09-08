/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "IO_types.hh"
#include "usd.hh"
#include "usd_hook.hh"
#include "usd_reader_domelight.hh"
#include "usd_reader_geom.hh"
#include "usd_reader_prim.hh"
#include "usd_reader_stage.hh"

#include "BKE_cachefile.hh"
#include "BKE_collection.hh"
#include "BKE_context.hh"
#include "BKE_global.hh"
#include "BKE_layer.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_object.hh"
#include "BKE_report.hh"

#include "BLI_listbase.h"
#include "BLI_math_matrix.h"
#include "BLI_path_utils.hh"
#include "BLI_string.h"
#include "BLI_timeit.hh"

#include "BLT_translation.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"

#include "DNA_cachefile_types.h"
#include "DNA_collection_types.h"
#include "DNA_layer_types.h"
#include "DNA_listBase.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_windowmanager_types.h"

#include "ED_undo.hh"

#include "MEM_guardedalloc.h"

#include "RNA_access.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdGeom/metrics.h>

#include <fmt/core.h>

namespace blender::io::usd {

static CacheArchiveHandle *handle_from_stage_reader(USDStageReader *reader)
{
  return reinterpret_cast<CacheArchiveHandle *>(reader);
}

static USDStageReader *stage_reader_from_handle(CacheArchiveHandle *handle)
{
  return reinterpret_cast<USDStageReader *>(handle);
}

static bool gather_objects_paths(const pxr::UsdPrim &object, ListBase *object_paths)
{
  if (!object.IsValid()) {
    return false;
  }

  for (const pxr::UsdPrim &childPrim : object.GetChildren()) {
    gather_objects_paths(childPrim, object_paths);
  }

  CacheObjectPath *usd_path = MEM_callocN<CacheObjectPath>("CacheObjectPath");

  STRNCPY(usd_path->path, object.GetPrimPath().GetString().c_str());
  BLI_addtail(object_paths, usd_path);

  return true;
}

enum {
  USD_NO_ERROR = 0,
  USD_ARCHIVE_FAIL,
};

struct ImportJobData {
  bContext *C;
  Main *bmain;
  Scene *scene;
  ViewLayer *view_layer;
  wmWindowManager *wm;

  char filepath[1024];
  USDImportParams params;

  USDStageReader *archive;

  bool *stop;
  bool *do_update;
  float *progress;

  char error_code;
  bool was_canceled;
  bool import_ok;
  bool is_background_job;
  timeit::TimePoint start_time;

  CacheFile *cache_file;
};

static void report_job_duration(const ImportJobData *data)
{
  timeit::Nanoseconds duration = timeit::Clock::now() - data->start_time;
  fmt::print("USD import of '{}' took ", data->filepath);
  timeit::print_duration(duration);
  fmt::print("\n");
}

static void import_startjob(void *customdata, wmJobWorkerStatus *worker_status)
{
  ImportJobData *data = static_cast<ImportJobData *>(customdata);

  data->stop = &worker_status->stop;
  data->do_update = &worker_status->do_update;
  data->progress = &worker_status->progress;
  data->was_canceled = false;
  data->archive = nullptr;
  data->start_time = timeit::Clock::now();
  data->cache_file = nullptr;

  data->params.worker_status = worker_status;

  WM_locked_interface_set(data->wm, true);
  G.is_break = false;

  if (data->params.create_collection) {
    char display_name[MAX_ID_NAME - 2];
    BLI_path_to_display_name(
        display_name, sizeof(display_name), BLI_path_basename(data->filepath));
    Collection *import_collection = BKE_collection_add(
        data->bmain, data->scene->master_collection, display_name);

    DEG_id_tag_update(&import_collection->id, ID_RECALC_SYNC_TO_EVAL);
    DEG_relations_tag_update(data->bmain);

    BKE_view_layer_synced_ensure(data->scene, data->view_layer);
    data->view_layer->active_collection = BKE_layer_collection_first_from_scene_collection(
        data->view_layer, import_collection);
  }

  BLI_path_abs(data->filepath, BKE_main_blendfile_path_from_global());

  *data->do_update = true;
  *data->progress = 0.05f;

  if (G.is_break) {
    data->was_canceled = true;
    return;
  }

  *data->do_update = true;
  *data->progress = 0.1f;

  pxr::UsdStagePopulationMask pop_mask;
  for (const std::string &mask_token : pxr::TfStringTokenize(data->params.prim_path_mask, ",;")) {
    pxr::SdfPath prim_path(mask_token);
    if (!prim_path.IsEmpty()) {
      pop_mask.Add(prim_path);
    }
  }

  pxr::UsdStageRefPtr stage = pop_mask.IsEmpty() ?
                                  pxr::UsdStage::Open(data->filepath) :
                                  pxr::UsdStage::OpenMasked(data->filepath, pop_mask);

  if (!stage) {
    BKE_reportf(worker_status->reports,
                RPT_ERROR,
                "USD Import: unable to open stage to read %s",
                data->filepath);
    data->import_ok = false;
    data->error_code = USD_ARCHIVE_FAIL;
    return;
  }

  double scene_scale = data->params.scale;
  if (data->params.apply_unit_conversion_scale) {
    scene_scale *= pxr::UsdGeomGetStageMetersPerUnit(stage);
  }

  /* Set up the stage for animated data. */
  if (data->params.set_frame_range) {
    data->scene->r.sfra = stage->GetStartTimeCode();
    data->scene->r.efra = stage->GetEndTimeCode();
  }

  *data->do_update = true;
  *data->progress = 0.15f;

  /* Callback function to lazily create a cache file when converting
   * time varying data. */
  auto get_cache_file = [data, scene_scale]() {
    if (!data->cache_file) {
      data->cache_file = static_cast<CacheFile *>(
          BKE_cachefile_add(data->bmain, BLI_path_basename(data->filepath)));

      /* Decrement the ID ref-count because it is going to be incremented for each
       * modifier and constraint that it will be attached to, so since currently
       * it is not used by anyone, its use count will off by one. */
      id_us_min(&data->cache_file->id);

      data->cache_file->is_sequence = data->params.is_sequence;
      data->cache_file->scale = scene_scale;
      STRNCPY(data->cache_file->filepath, data->filepath);
      if (data->params.relative_path && !BLI_path_is_rel(data->cache_file->filepath)) {
        BLI_path_rel(data->cache_file->filepath, BKE_main_blendfile_path_from_global());
      }
    }
    return data->cache_file;
  };

  USDStageReader *archive = new USDStageReader(stage, data->params, get_cache_file);

  /* Ensure Python types for invoking hooks are registered. */
  register_hook_converters();

  archive->find_material_import_hook_sources();

  data->archive = archive;

  archive->collect_readers();

  if (data->params.import_lights && data->params.create_world_material &&
      !archive->dome_light_readers().is_empty())
  {
    USDDomeLightReader *dome_light_reader = archive->dome_light_readers().first();
    dome_light_reader->create_object(data->scene, data->bmain);
  }

  if (data->params.import_materials && data->params.import_all_materials) {
    archive->import_all_materials(data->bmain);
  }

  *data->do_update = true;
  *data->progress = 0.2f;

  const float size = float(archive->readers().size());
  size_t i = 0;

  /* Sort readers by name: when creating a lot of objects in Blender,
   * it is much faster if the order is sorted by name. */
  archive->sort_readers();
  *data->do_update = true;
  *data->progress = 0.25f;

  /* Create blender objects. */
  for (USDPrimReader *reader : archive->readers()) {
    if (!reader) {
      continue;
    }
    reader->create_object(data->bmain);
    if ((++i & 1023) == 0) {
      *data->do_update = true;
      *data->progress = 0.25f + 0.25f * (i / size);
    }
  }

  /* Setup parenthood and read actual object data. */
  i = 0;
  for (USDPrimReader *reader : archive->readers()) {
    if (!reader) {
      continue;
    }

    Object *ob = reader->object();
    reader->read_object_data(data->bmain, 0.0);

    USDPrimReader *parent = reader->parent();
    if (parent == nullptr) {
      ob->parent = nullptr;
    }
    else {
      ob->parent = parent->object();
    }

    *data->progress = 0.5f + 0.5f * (++i / size);
    *data->do_update = true;

    if (G.is_break) {
      data->was_canceled = true;
      return;
    }
  }

  if (data->params.import_skeletons) {
    archive->process_armature_modifiers();
  }

  data->import_ok = !data->was_canceled;

  worker_status->progress = 1.0f;
  worker_status->do_update = true;
}

static void import_endjob(void *customdata)
{
  ImportJobData *data = static_cast<ImportJobData *>(customdata);

  /* Delete objects on cancellation. */
  if (data->was_canceled && data->archive) {

    for (const USDPrimReader *reader : data->archive->readers()) {

      if (!reader) {
        continue;
      }

      /* It's possible that cancellation occurred between the creation of
       * the reader and the creation of the Blender object. */
      if (Object *ob = reader->object()) {
        BKE_id_free_us(data->bmain, ob);
      }
    }
  }
  else if (data->archive) {
    Base *base;
    LayerCollection *lc;
    const Scene *scene = data->scene;
    ViewLayer *view_layer = data->view_layer;

    BKE_view_layer_base_deselect_all(scene, view_layer);

    lc = BKE_layer_collection_get_active(view_layer);

    /* Create prototype collections for instancing. */
    data->archive->create_proto_collections(data->bmain, lc->collection);

    /* Add all objects to the collection. */
    for (const USDPrimReader *reader : data->archive->readers()) {
      if (!reader) {
        continue;
      }
      if (reader->is_in_proto()) {
        /* Skip prototype prims, as these are added to prototype collections. */
        continue;
      }
      Object *ob = reader->object();
      if (!ob) {
        continue;
      }
      BKE_collection_object_add(data->bmain, lc->collection, ob);
    }

    /* Sync and do the view layer operations. */
    BKE_view_layer_synced_ensure(scene, view_layer);
    for (const USDPrimReader *reader : data->archive->readers()) {
      if (!reader) {
        continue;
      }

      Object *ob = reader->object();
      if (!ob) {
        continue;
      }
      base = BKE_view_layer_base_find(view_layer, ob);
      /* TODO: is setting active needed? */
      BKE_view_layer_base_select_and_set_active(view_layer, base);

      DEG_id_tag_update(&lc->collection->id, ID_RECALC_SYNC_TO_EVAL);
      DEG_id_tag_update_ex(data->bmain,
                           &ob->id,
                           ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY | ID_RECALC_ANIMATION |
                               ID_RECALC_BASE_FLAGS);
    }

    DEG_id_tag_update(&data->scene->id, ID_RECALC_BASE_FLAGS);
    DEG_relations_tag_update(data->bmain);

    if (data->params.import_materials && data->params.import_all_materials) {
      data->archive->fake_users_for_unused_materials();
    }

    data->archive->call_material_import_hooks(data->bmain);

    call_import_hooks(data->archive, data->params.worker_status->reports);

    if (data->is_background_job) {
      /* Blender already returned from the import operator, so we need to store our own extra undo
       * step. */
      ED_undo_push(data->C, "USD Import Finished");
    }
  }

  WM_locked_interface_set(data->wm, false);

  switch (data->error_code) {
    default:
    case USD_NO_ERROR:
      data->import_ok = !data->was_canceled;
      break;
    case USD_ARCHIVE_FAIL:
      BKE_report(data->params.worker_status->reports,
                 RPT_ERROR,
                 "Could not open USD archive for reading, see console for detail");
      break;
  }

  WM_main_add_notifier(NC_ID | NA_ADDED, nullptr);
  report_job_duration(data);
}

static void import_freejob(void *user_data)
{
  ImportJobData *data = static_cast<ImportJobData *>(user_data);

  delete data->archive;
  delete data;
}

bool USD_import(const bContext *C,
                const char *filepath,
                const USDImportParams *params,
                bool as_background_job,
                ReportList *reports)
{
  /* Using new here since `MEM_*` functions do not call constructor to properly initialize data. */
  ImportJobData *job = new ImportJobData();
  job->C = const_cast<bContext *>(C);
  job->bmain = CTX_data_main(C);
  job->scene = CTX_data_scene(C);
  job->view_layer = CTX_data_view_layer(C);
  job->wm = CTX_wm_manager(C);
  job->import_ok = false;
  job->is_background_job = as_background_job;
  STRNCPY(job->filepath, filepath);

  job->error_code = USD_NO_ERROR;
  job->was_canceled = false;
  job->archive = nullptr;

  job->params = *params;

  G.is_break = false;

  bool import_ok = false;
  if (as_background_job) {
    wmJob *wm_job = WM_jobs_get(CTX_wm_manager(C),
                                CTX_wm_window(C),
                                job->scene,
                                "Importing USD...",
                                WM_JOB_PROGRESS,
                                WM_JOB_TYPE_USD_IMPORT);

    /* setup job */
    WM_jobs_customdata_set(wm_job, job, import_freejob);
    WM_jobs_timer(wm_job, 0.1, NC_SCENE, NC_SCENE);
    WM_jobs_callbacks(wm_job, import_startjob, nullptr, nullptr, import_endjob);

    WM_jobs_start(CTX_wm_manager(C), wm_job);
  }
  else {
    wmJobWorkerStatus worker_status = {};
    /* Use the operator's reports in non-background case. */
    worker_status.reports = reports;

    import_startjob(job, &worker_status);
    import_endjob(job);
    import_ok = job->import_ok;

    import_freejob(job);
  }

  return import_ok;
}

/* TODO(makowalski): Extend this function with basic validation that the
 * USD reader is compatible with the type of the given (currently unused) 'ob'
 * Object parameter, similar to the logic in get_abc_reader() in the
 * Alembic importer code. */
static USDPrimReader *get_usd_reader(CacheReader *reader,
                                     const Object * /*ob*/,
                                     const char **r_err_str)
{
  USDPrimReader *usd_reader = reinterpret_cast<USDPrimReader *>(reader);
  pxr::UsdPrim iobject = usd_reader->prim();

  if (!iobject.IsValid()) {
    *r_err_str = RPT_("Invalid object: verify object path");
    return nullptr;
  }

  return usd_reader;
}

USDMeshReadParams create_mesh_read_params(const double motion_sample_time, const int read_flags)
{
  USDMeshReadParams params = {};
  params.motion_sample_time = motion_sample_time;
  params.read_flags = read_flags;
  return params;
}

void USD_read_geometry(CacheReader *reader,
                       const Object *ob,
                       blender::bke::GeometrySet &geometry_set,
                       const USDMeshReadParams params,
                       const char **r_err_str)
{
  USDGeomReader *usd_reader = dynamic_cast<USDGeomReader *>(get_usd_reader(reader, ob, r_err_str));

  if (usd_reader == nullptr) {
    return;
  }

  usd_reader->read_geometry(geometry_set, params, r_err_str);
}

bool USD_mesh_topology_changed(CacheReader *reader,
                               const Object *ob,
                               const Mesh *existing_mesh,
                               const double time,
                               const char **r_err_str)
{
  USDGeomReader *usd_reader = dynamic_cast<USDGeomReader *>(get_usd_reader(reader, ob, r_err_str));

  if (usd_reader == nullptr) {
    return false;
  }

  return usd_reader->topology_changed(existing_mesh, time);
}

CacheReader *CacheReader_open_usd_object(CacheArchiveHandle *handle,
                                         CacheReader *reader,
                                         Object *object,
                                         const char *object_path)
{
  if (object_path[0] == '\0') {
    return reader;
  }

  USDStageReader *archive = stage_reader_from_handle(handle);

  if (!archive || !archive->valid()) {
    return reader;
  }

  if (reader) {
    USD_CacheReader_free(reader);
  }

  pxr::UsdPrim prim = archive->stage()->GetPrimAtPath(pxr::SdfPath(object_path));

  if (!prim) {
    return nullptr;
  }

  /* TODO(makowalski): The handle does not have the proper import params or settings. */
  USDPrimReader *usd_reader = archive->create_reader(prim);

  if (usd_reader == nullptr) {
    /* This object is not supported. */
    return nullptr;
  }
  if (!usd_reader->valid()) {
    /* This object is invalid for some reason. */
    return nullptr;
  }
  usd_reader->object(object);
  usd_reader->incref();

  return reinterpret_cast<CacheReader *>(usd_reader);
}

void USD_CacheReader_free(CacheReader *reader)
{
  USDPrimReader *usd_reader = reinterpret_cast<USDPrimReader *>(reader);
  usd_reader->decref();

  if (usd_reader->refcount() == 0) {
    delete usd_reader;
  }
}

CacheArchiveHandle *USD_create_handle(Main * /*bmain*/,
                                      const char *filepath,
                                      ListBase *object_paths)
{
  pxr::UsdStageRefPtr stage = pxr::UsdStage::Open(filepath);

  if (!stage) {
    return nullptr;
  }

  USDImportParams params{};

  USDStageReader *stage_reader = new USDStageReader(stage, params);

  if (object_paths) {
    gather_objects_paths(stage->GetPseudoRoot(), object_paths);
  }

  return handle_from_stage_reader(stage_reader);
}

void USD_free_handle(CacheArchiveHandle *handle)
{
  USDStageReader *stage_reader = stage_reader_from_handle(handle);
  delete stage_reader;
}

void USD_get_transform(CacheReader *reader, float r_mat_world[4][4], float time, float scale)
{
  if (!reader) {
    return;
  }
  const USDXformReader *usd_reader = reinterpret_cast<USDXformReader *>(reader);

  bool is_constant = false;

  /* Convert from the local matrix we obtain from USD to world coordinates
   * for Blender. This conversion is done here rather than by Blender due to
   * work around the non-standard interpretation of CONSTRAINT_SPACE_LOCAL in
   * BKE_constraint_mat_convertspace(). */
  Object *object = usd_reader->object();
  if (object->parent == nullptr) {
    /* No parent, so local space is the same as world space. */
    usd_reader->read_matrix(r_mat_world, time, scale, &is_constant);
    return;
  }

  float mat_parent[4][4];
  BKE_object_get_parent_matrix(object, object->parent, mat_parent);

  float mat_local[4][4];
  usd_reader->read_matrix(mat_local, time, scale, &is_constant);
  mul_m4_m4m4(r_mat_world, mat_parent, object->parentinv);
  mul_m4_m4m4(r_mat_world, r_mat_world, mat_local);
}

}  // namespace blender::io::usd
