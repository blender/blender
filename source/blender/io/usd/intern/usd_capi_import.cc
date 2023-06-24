/* SPDX-FileCopyrightText: 2019 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "IO_types.h"
#include "usd.h"
#include "usd_hierarchy_iterator.h"

#include "usd_light_convert.h"
#include "usd_reader_geom.h"
#include "usd_reader_instance.h"
#include "usd_reader_prim.h"
#include "usd_reader_stage.h"

#include <pxr/base/plug/registry.h>

#include "usd_writer_material.h"

#include <pxr/pxr.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdGeom/metrics.h>
#include <pxr/usd/usdGeom/scope.h>
#include <pxr/usd/usdGeom/tokens.h>
#include <pxr/usd/usdGeom/xformCommonAPI.h>
#include <pxr/usd/usdLux/domeLight.h>
#include <pxr/usd/usdShade/materialBindingAPI.h>

#include "MEM_guardedalloc.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"
#include "DEG_depsgraph_query.h"

#include "DNA_cachefile_types.h"

#include "DNA_collection_types.h"

#include "DNA_node_types.h"
#include "DNA_scene_types.h"
#include "DNA_world_types.h"

#include "BKE_appdir.h"
#include "BKE_blender_version.h"
#include "BKE_cachefile.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_layer.h"
#include "BKE_lib_id.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_node.hh"
#include "BKE_object.h"
#include "BKE_scene.h"
#include "BKE_world.h"

#include "BLI_fileops.h"
#include "BLI_listbase.h"
#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"
#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_timeit.hh"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"
#include "DEG_depsgraph_query.h"

#include "DNA_cachefile_types.h"
#include "DNA_collection_types.h"
#include "DNA_node_types.h"
#include "DNA_scene_types.h"
#include "DNA_world_types.h"

#include "MEM_guardedalloc.h"

#include "WM_api.h"
#include "WM_types.h"

#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdGeom/metrics.h>
#include <pxr/usd/usdGeom/scope.h>
#include <pxr/usd/usdGeom/tokens.h>
#include <pxr/usd/usdGeom/xformCommonAPI.h>

#include <iostream>

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

  void *usd_path_void = MEM_callocN(sizeof(CacheObjectPath), "CacheObjectPath");
  CacheObjectPath *usd_path = static_cast<CacheObjectPath *>(usd_path_void);

  BLI_strncpy(usd_path->path, object.GetPrimPath().GetString().c_str(), sizeof(usd_path->path));
  BLI_addtail(object_paths, usd_path);

  return true;
}

/* Create a collection with the given parent and name. */
static Collection *create_collection(Main *bmain, Collection *parent, const char *name)
{
  if (!bmain) {
    return nullptr;
  }

  Collection *coll = BKE_collection_add(bmain, parent, name);

  if (coll) {
    id_fake_user_set(&coll->id);
    DEG_id_tag_update(&coll->id, ID_RECALC_COPY_ON_WRITE);
  }

  return coll;
}

/* Set the instance collection on the given instance reader.
 *  The collection is assigned from the given map based on
 *  the prototype (maser) prim path. */
static void set_instance_collection(
    USDInstanceReader *instance_reader,
    const std::map<pxr::SdfPath, Collection *> &proto_collection_map)
{
  if (!instance_reader) {
    return;
  }

  pxr::SdfPath proto_path = instance_reader->proto_path();

  std::map<pxr::SdfPath, Collection *>::const_iterator it = proto_collection_map.find(proto_path);

  if (it != proto_collection_map.end()) {
    instance_reader->set_instance_collection(it->second);
  }
  else {
    std::cerr << "WARNING: Couldn't find prototype collection for " << instance_reader->prim_path()
              << std::endl;
  }
}

/* Create instance collections for the USD instance readers. */
static void create_proto_collections(Main *bmain,
                                     ViewLayer *view_layer,
                                     Collection *parent_collection,
                                     const ProtoReaderMap &proto_readers,
                                     const std::vector<USDPrimReader *> &readers)
{
  Collection *all_protos_collection = create_collection(bmain, parent_collection, "prototypes");

  if (all_protos_collection) {
    all_protos_collection->flag |= COLLECTION_HIDE_VIEWPORT;
    all_protos_collection->flag |= COLLECTION_HIDE_RENDER;
  }

  std::map<pxr::SdfPath, Collection *> proto_collection_map;

  for (const auto &pair : proto_readers) {

    std::string proto_collection_name = pair.first.GetString();

    // TODO(makowalski): Is it acceptable to have slashes in the collection names? Or should we
    // replace them with another character, like an underscore, as in the following?
    // std::replace(proto_collection_name.begin(), proto_collection_name.end(), '/', '_');

    Collection *proto_collection = create_collection(
        bmain, all_protos_collection, proto_collection_name.c_str());

    proto_collection_map.insert(std::make_pair(pair.first, proto_collection));
  }

  // Set the instance collections on the readers, including the prototype
  // readers, as instancing may be recursive.

  for (const auto &pair : proto_readers) {
    for (USDPrimReader *reader : pair.second) {
      if (USDInstanceReader *instance_reader = dynamic_cast<USDInstanceReader *>(reader)) {
        set_instance_collection(instance_reader, proto_collection_map);
      }
    }
  }

  for (USDPrimReader *reader : readers) {
    if (USDInstanceReader *instance_reader = dynamic_cast<USDInstanceReader *>(reader)) {
      set_instance_collection(instance_reader, proto_collection_map);
    }
  }

  // Add the prototype objects to the collections.
  for (const auto &pair : proto_readers) {

    std::map<pxr::SdfPath, Collection *>::const_iterator it = proto_collection_map.find(
        pair.first);

    if (it == proto_collection_map.end()) {
      std::cerr << "WARNING: Couldn't find collection when adding objects for prototype "
                << pair.first << std::endl;
      continue;
    }

    for (USDPrimReader *reader : pair.second) {
      Object *ob = reader->object();

      if (!ob) {
        continue;
      }

      Collection *coll = it->second;

      BKE_collection_object_add(bmain, coll, ob);

      DEG_id_tag_update(&coll->id, ID_RECALC_COPY_ON_WRITE);
      DEG_id_tag_update_ex(bmain,
                           &ob->id,
                           ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY | ID_RECALC_ANIMATION |
                               ID_RECALC_BASE_FLAGS);
    }
  }
}

/* Update the given import settings with the global rotation matrix to orient
 * imported objects with Z-up, if necessary */
static void convert_to_z_up(pxr::UsdStageRefPtr stage, ImportSettings *r_settings)
{
  if (!stage || pxr::UsdGeomGetStageUpAxis(stage) == pxr::UsdGeomTokens->z) {
    return;
  }

  if (!r_settings) {
    return;
  }

  r_settings->do_convert_mat = true;

  /* Rotate 90 degrees about the X-axis. */
  float rmat[3][3];
  float axis[3] = {1.0f, 0.0f, 0.0f};
  axis_angle_normalized_to_mat3(rmat, axis, M_PI_2);

  unit_m4(r_settings->conversion_mat);
  copy_m4_m3(r_settings->conversion_mat, rmat);
}

enum {
  USD_NO_ERROR = 0,
  USD_ARCHIVE_FAIL,
};

struct ImportJobData {
  Main *bmain;
  Scene *scene;
  ViewLayer *view_layer;
  wmWindowManager *wm;

  char filepath[1024];
  USDImportParams params;
  ImportSettings settings;

  USDStageReader *archive;

  bool *stop;
  bool *do_update;
  float *progress;

  char error_code;
  bool was_canceled;
  bool import_ok;
  timeit::TimePoint start_time;

  wmJob *wm_job;
};

static void main_thread_lock_acquire(ImportJobData *data)
{
  if (data->wm_job) {
    WM_job_main_thread_lock_acquire(data->wm_job);
  }
}

static void main_thread_lock_release(ImportJobData *data)
{
  if (data->wm_job) {
    WM_job_main_thread_lock_release(data->wm_job);
  }
}


static CacheFile *create_cache_file(const ImportJobData *data)
{
  if (!data) {
    return nullptr;
  }

  CacheFile *cache_file = static_cast<CacheFile *>(
      BKE_cachefile_add(data->bmain, BLI_path_basename(data->filepath)));

  /* Decrement the ID ref-count because it is going to be incremented for each
   * modifier and constraint that it will be attached to, so since currently
   * it is not used by anyone, its use count will off by one. */
  id_us_min(&cache_file->id);

  cache_file->is_sequence = data->params.is_sequence;
  cache_file->scale = data->params.scale;
  STRNCPY(cache_file->filepath, data->filepath);

  cache_file->scale = data->settings.scale;

  return cache_file;
}

/* Apply the given cache file to the given reader, if needed.  Will create a cache file
 * and return it in the r_cache_file out prameter, if needed. */
static void apply_cache_file(USDPrimReader *reader,
                             const ImportJobData *data,
                             CacheFile **r_cache_file)
{
  if (!(reader && reader->needs_cachefile())) {
    return;
  }

  if (!(data && r_cache_file)) {
    return;
  }

  if (*r_cache_file == nullptr) {
    *r_cache_file = create_cache_file(data);
  }

  reader->apply_cache_file(*r_cache_file);
}

static void report_job_duration(const ImportJobData *data)
{
  timeit::Nanoseconds duration = timeit::Clock::now() - data->start_time;
  std::cout << "USD import of '" << data->filepath << "' took ";
  timeit::print_duration(duration);
  std::cout << '\n';
}

static void import_startjob(void *customdata, bool *stop, bool *do_update, float *progress)
{
  ImportJobData *data = static_cast<ImportJobData *>(customdata);

  data->stop = stop;
  data->do_update = do_update;
  data->progress = progress;
  data->was_canceled = false;
  data->archive = nullptr;
  data->start_time = timeit::Clock::now();

  WM_set_locked_interface(data->wm, true);
  G.is_break = false;

  if (data->params.create_collection) {
    char display_name[MAX_ID_NAME - 2];
    BLI_path_to_display_name(
        display_name, sizeof(display_name), BLI_path_basename(data->filepath));
    Collection *import_collection = BKE_collection_add(
        data->bmain, data->scene->master_collection, display_name);
    id_fake_user_set(&import_collection->id);

    DEG_id_tag_update(&import_collection->id, ID_RECALC_COPY_ON_WRITE);
    DEG_relations_tag_update(data->bmain);

    WM_main_add_notifier(NC_SCENE | ND_LAYER, nullptr);

    data->view_layer->active_collection = BKE_layer_collection_first_from_scene_collection(
        data->view_layer, import_collection);
  }

  USD_path_abs(data->filepath, BKE_main_blendfile_path_from_global(), true);

  *data->do_update = true;
  *data->progress = 0.05f;

  if (G.is_break) {
    data->was_canceled = true;
    return;
  }

  *data->do_update = true;
  *data->progress = 0.1f;

  std::string prim_path_mask(data->params.prim_path_mask);
  pxr::UsdStagePopulationMask pop_mask;
  if (!prim_path_mask.empty()) {
    const std::vector<std::string> mask_tokens = pxr::TfStringTokenize(prim_path_mask, ",;");
    for (const std::string &tok : mask_tokens) {
      pxr::SdfPath prim_path(tok);
      if (!prim_path.IsEmpty()) {
        pop_mask.Add(prim_path);
      }
    }
  }

  pxr::UsdStageRefPtr stage = pop_mask.IsEmpty() ?
                                  pxr::UsdStage::Open(data->filepath) :
                                  pxr::UsdStage::OpenMasked(data->filepath, pop_mask);
  if (!stage) {
    WM_reportf(RPT_ERROR, "USD Import: unable to open stage to read %s", data->filepath);
    data->import_ok = false;
    data->error_code = USD_ARCHIVE_FAIL;
    return;
  }

  convert_to_z_up(stage, &data->settings);
  data->settings.stage_meters_per_unit = UsdGeomGetStageMetersPerUnit(stage);

  if (data->params.apply_unit_conversion_scale) {
    const double meters_per_unit = pxr::UsdGeomGetStageMetersPerUnit(stage);
    data->settings.scale *= meters_per_unit;
  }

  /* Set up the stage for animated data. */
  if (data->params.set_frame_range) {
    data->scene->r.sfra = stage->GetStartTimeCode();
    data->scene->r.efra = stage->GetEndTimeCode();
  }

  *data->do_update = true;
  *data->progress = 0.15f;

  USDStageReader *archive = new USDStageReader(stage, data->params, data->settings);

  data->archive = archive;

  archive->collect_readers(data->bmain);

  if (data->params.import_lights && data->params.create_background_shader &&
      !archive->dome_lights().empty()) {
    dome_light_to_world_material(
        data->params, data->settings, data->scene, data->bmain, archive->dome_lights().front());
  }

  if (data->params.import_materials && data->params.import_all_materials) {
    archive->import_all_materials(data->bmain);
  }

  *data->do_update = true;
  *data->progress = 0.2f;

  const float size = float(archive->readers().size());
  size_t i = 0;

  /* Read data, set prenting and create a cache file, if needed. */

  /* We defer creating a cache file until we know that we need
   * one. This is not only more efficient, but also avoids
   * the problem where we can't overwrite the USD the
   * cachefile is referencing because it has a pointer to the
   * open stage for the lifetime of the scene. */
  CacheFile *cache_file = nullptr;

  /* Handle instance prototypes.
   * TODO(makowalski): Move this logic inside USDReaderStage? */

  /* Create prototype objects.
   * TODO(makowalski): Sort prototype objects by name, as below? */
  for (const auto &pair : archive->proto_readers()) {
    for (USDPrimReader *reader : pair.second) {
      if (reader) {
        reader->create_object(data->bmain, 0.0);
      }   
    }
  }

  for (const auto &pair : archive->proto_readers()) {

    for (USDPrimReader *reader : pair.second) {

      if (!reader) {
        continue;
      }

      /* TODO(makowalski): Here and below, should we call
       * read_object_data() with the actual time? */
      reader->read_object_data(data->bmain, 0.0);

      apply_cache_file(reader, data, &cache_file);

      Object *ob = reader->object();

      if (!ob) {
        continue;
      }

      const USDPrimReader *parent_reader = reader->parent();

      ob->parent = parent_reader ? parent_reader->object() : nullptr;

      /* TODO(makowalski): Handle progress update. */
    }
  }

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
    reader->create_object(data->bmain, 0.0);
    if ((++i & 1023) == 0) {
      *data->do_update = true;
      *data->progress = 0.25f + 0.25f * (i / size);
    }
  }

  *data->do_update = true;
  *data->progress = 0.5f;

  /* Reading materials may trigger adding event notifiers, which
   * isn't thread safe when the importer is invoked in a background
   * job.  We therefore acquire the main thread lock before reading
   * object data, to avoid possible crashes when events are added
   * in job timers for progress updates in the main thread.
   * (See wm_jobs_timer()). */
  main_thread_lock_acquire(data);

  /* Setup parenthood and read actual object data. */
  i = 0;
  for (USDPrimReader *reader : archive->readers()) {

    if (!reader) {
      continue;
    }

    Object *ob = reader->object();

    reader->read_object_data(data->bmain, 0.0);

    apply_cache_file(reader, data, &cache_file);

    USDPrimReader *parent = reader->parent();

    if (parent == nullptr) {
      ob->parent = nullptr;
    }
    else {
      ob->parent = parent->object();
    }

    if ((++i & 255) == 0) {
      main_thread_lock_release(data);
      *data->progress = 0.5f + 0.5f * (i / size);
      *data->do_update = true;
      main_thread_lock_acquire(data);
    }

    if (G.is_break) {
      data->was_canceled = true;
      main_thread_lock_release(data);
      return;
    }
  }

  main_thread_lock_release(data);

  if (data->params.import_skeletons) {
    archive->process_armature_modifiers();
  }

  data->import_ok = !data->was_canceled;

  *progress = 1.0f;
  *do_update = true;
}

static void import_endjob(void *customdata)
{
  ImportJobData *data = static_cast<ImportJobData *>(customdata);

  /* Delete objects on cancellation. */
  if (data->was_canceled && data->archive) {

    for (USDPrimReader *reader : data->archive->readers()) {

      if (!reader) {
        continue;
      }

      /* It's possible that cancellation occurred between the creation of
       * the reader and the creation of the Blender object. */
      if (Object *ob = reader->object()) {
        BKE_id_free_us(data->bmain, ob);
      }
    }

    for (const auto &pair : data->archive->proto_readers()) {
      for (USDPrimReader *reader : pair.second) {

        /* It's possible that cancellation occurred between the creation of
         * the reader and the creation of the Blender object. */
        if (Object *ob = reader->object()) {
          BKE_id_free_us(data->bmain, ob);
        }
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

    if (!data->archive->proto_readers().empty()) {
      create_proto_collections(data->bmain,
                               view_layer,
                               lc->collection,
                               data->archive->proto_readers(),
                               data->archive->readers());
    }

    /* Add all objects to the collection. */
    for (USDPrimReader *reader : data->archive->readers()) {
      if (!reader) {
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
    for (USDPrimReader *reader : data->archive->readers()) {
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

      DEG_id_tag_update(&lc->collection->id, ID_RECALC_COPY_ON_WRITE);
      DEG_id_tag_update_ex(data->bmain,
                           &ob->id,
                           ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY | ID_RECALC_ANIMATION |
                               ID_RECALC_BASE_FLAGS);
    }

    DEG_id_tag_update(&data->scene->id, ID_RECALC_BASE_FLAGS);
    if (!data->archive->dome_lights().empty()) {
      DEG_id_tag_update(&data->scene->world->id, ID_RECALC_COPY_ON_WRITE);
    }
    DEG_relations_tag_update(data->bmain);

    if (data->params.import_materials && data->params.import_all_materials) {
      data->archive->fake_users_for_unused_materials();
    }
  }

  WM_set_locked_interface(data->wm, false);

  switch (data->error_code) {
    default:
    case USD_NO_ERROR:
      data->import_ok = !data->was_canceled;
      break;
    case USD_ARCHIVE_FAIL:
      WM_report(RPT_ERROR, "Could not open USD archive for reading, see console for detail");
      break;
  }

  MEM_SAFE_FREE(data->params.prim_path_mask);

  WM_main_add_notifier(NC_SCENE | ND_FRAME, data->scene);
  report_job_duration(data);
}

static void import_freejob(void *user_data)
{
  ImportJobData *data = static_cast<ImportJobData *>(user_data);

  delete data->archive;
  delete data;
}

}  // namespace blender::io::usd

using namespace blender::io::usd;

bool USD_import(struct bContext *C,
                const char *filepath,
                const USDImportParams *params,
                bool as_background_job)
{
  /* Using new here since `MEM_*` functions do not call constructor to properly initialize data. */
  ImportJobData *job = new ImportJobData();
  job->bmain = CTX_data_main(C);
  job->scene = CTX_data_scene(C);
  job->view_layer = CTX_data_view_layer(C);
  job->wm = CTX_wm_manager(C);
  job->import_ok = false;
  BLI_strncpy(job->filepath, filepath, 1024);

  job->settings.scale = params->scale;
  job->settings.sequence_offset = params->offset;
  job->settings.is_sequence = params->is_sequence;
  job->settings.sequence_len = params->sequence_len;
  job->settings.validate_meshes = params->validate_meshes;
  job->settings.sequence_len = params->sequence_len;
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
                                "USD Import",
                                WM_JOB_PROGRESS,
                                WM_JOB_TYPE_ALEMBIC);

    job->wm_job = wm_job;

    /* setup job */
    WM_jobs_customdata_set(wm_job, job, import_freejob);
    WM_jobs_timer(wm_job, 0.1, NC_SCENE, NC_SCENE);
    WM_jobs_callbacks(wm_job, import_startjob, nullptr, nullptr, import_endjob);

    WM_jobs_start(CTX_wm_manager(C), wm_job);
  }
  else {
    /* Fake a job context, so that we don't need null pointer checks while importing. */
    bool stop = false, do_update = false;
    float progress = 0.0f;

    import_startjob(job, &stop, &do_update, &progress);
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
                                     const Object * /* ob */,
                                     const char **err_str)
{
  USDPrimReader *usd_reader = reinterpret_cast<USDPrimReader *>(reader);
  pxr::UsdPrim iobject = usd_reader->prim();

  if (!iobject.IsValid()) {
    *err_str = "Invalid object: verify object path";
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

struct Mesh *USD_read_mesh(struct CacheReader *reader,
                           struct Object *ob,
                           struct Mesh *existing_mesh,
                           const USDMeshReadParams params,
                           const char **err_str)
{
  USDGeomReader *usd_reader = dynamic_cast<USDGeomReader *>(get_usd_reader(reader, ob, err_str));

  if (usd_reader == nullptr) {
    return nullptr;
  }

  return usd_reader->read_mesh(existing_mesh, params, err_str);
}

bool USD_mesh_topology_changed(CacheReader *reader,
                               const Object *ob,
                               const Mesh *existing_mesh,
                               const double time,
                               const char **err_str)
{
  USDGeomReader *usd_reader = dynamic_cast<USDGeomReader *>(get_usd_reader(reader, ob, err_str));

  if (usd_reader == nullptr) {
    return false;
  }

  return usd_reader->topology_changed(existing_mesh, time);
}

void USD_CacheReader_incref(CacheReader *reader)
{
  USDPrimReader *usd_reader = reinterpret_cast<USDPrimReader *>(reader);
  usd_reader->incref();
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

  pxr::UsdPrim prim = archive->stage()->GetPrimAtPath(pxr::SdfPath(object_path));

  if (reader) {
    USD_CacheReader_free(reader);
  }

  /* TODO(makowalski): The handle does not have the proper import params or settings. */
  pxr::UsdGeomXformCache xf_cache;
  USDPrimReader *usd_reader = archive->create_reader(prim, &xf_cache);

  if (usd_reader == nullptr) {
    /* This object is not supported */
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

CacheArchiveHandle *USD_create_handle(struct Main * /*bmain*/,
                                      const char *filepath,
                                      ListBase *object_paths)
{
  pxr::UsdStageRefPtr stage = pxr::UsdStage::Open(filepath);

  if (!stage) {
    return nullptr;
  }

  USDImportParams params{};

  blender::io::usd::ImportSettings settings{};
  convert_to_z_up(stage, &settings);

  USDStageReader *stage_reader = new USDStageReader(stage, params, settings);

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

void USD_get_transform(struct CacheReader *reader,
                       float r_mat_world[4][4],
                       float time,
                       float scale)
{
  if (!reader) {
    return;
  }
  USDXformReader *usd_reader = reinterpret_cast<USDXformReader *>(reader);

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
