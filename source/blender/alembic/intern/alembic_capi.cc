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
 */

/** \file
 * \ingroup balembic
 */

#include "../ABC_alembic.h"

#include <Alembic/AbcMaterial/IMaterial.h>

#include "abc_archive.h"
#include "abc_camera.h"
#include "abc_curves.h"
#include "abc_hair.h"
#include "abc_mesh.h"
#include "abc_nurbs.h"
#include "abc_points.h"
#include "abc_transform.h"
#include "abc_util.h"

extern "C" {
#include "MEM_guardedalloc.h"

#include "DNA_cachefile_types.h"
#include "DNA_curve_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_cachefile.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_context.h"
#include "BKE_curve.h"
#include "BKE_global.h"
#include "BKE_layer.h"
#include "BKE_library.h"
#include "BKE_scene.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"

/* SpaceType struct has a member called 'new' which obviously conflicts with C++
 * so temporarily redefining the new keyword to make it compile. */
#define new extern_new
#include "BKE_screen.h"
#undef new

#include "BLI_fileops.h"
#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_path_util.h"
#include "BLI_string.h"

#include "WM_api.h"
#include "WM_types.h"
}

using Alembic::Abc::Int32ArraySamplePtr;
using Alembic::Abc::ObjectHeader;

using Alembic::AbcGeom::kWrapExisting;
using Alembic::AbcGeom::MetaData;
using Alembic::AbcGeom::P3fArraySamplePtr;

using Alembic::AbcGeom::ICamera;
using Alembic::AbcGeom::ICompoundProperty;
using Alembic::AbcGeom::ICurves;
using Alembic::AbcGeom::ICurvesSchema;
using Alembic::AbcGeom::IFaceSet;
using Alembic::AbcGeom::ILight;
using Alembic::AbcGeom::IN3fArrayProperty;
using Alembic::AbcGeom::IN3fGeomParam;
using Alembic::AbcGeom::INuPatch;
using Alembic::AbcGeom::IObject;
using Alembic::AbcGeom::IPoints;
using Alembic::AbcGeom::IPointsSchema;
using Alembic::AbcGeom::IPolyMesh;
using Alembic::AbcGeom::IPolyMeshSchema;
using Alembic::AbcGeom::ISampleSelector;
using Alembic::AbcGeom::ISubD;
using Alembic::AbcGeom::IV2fGeomParam;
using Alembic::AbcGeom::IXform;
using Alembic::AbcGeom::IXformSchema;
using Alembic::AbcGeom::N3fArraySamplePtr;
using Alembic::AbcGeom::V3fArraySamplePtr;
using Alembic::AbcGeom::XformSample;

using Alembic::AbcMaterial::IMaterial;

struct AbcArchiveHandle {
  int unused;
};

ABC_INLINE ArchiveReader *archive_from_handle(AbcArchiveHandle *handle)
{
  return reinterpret_cast<ArchiveReader *>(handle);
}

ABC_INLINE AbcArchiveHandle *handle_from_archive(ArchiveReader *archive)
{
  return reinterpret_cast<AbcArchiveHandle *>(archive);
}

//#define USE_NURBS

/* NOTE: this function is similar to visit_objects below, need to keep them in
 * sync. */
static bool gather_objects_paths(const IObject &object, ListBase *object_paths)
{
  if (!object.valid()) {
    return false;
  }

  size_t children_claiming_this_object = 0;
  size_t num_children = object.getNumChildren();

  for (size_t i = 0; i < num_children; ++i) {
    bool child_claims_this_object = gather_objects_paths(object.getChild(i), object_paths);
    children_claiming_this_object += child_claims_this_object ? 1 : 0;
  }

  const MetaData &md = object.getMetaData();
  bool get_path = false;
  bool parent_is_part_of_this_object = false;

  if (!object.getParent()) {
    /* The root itself is not an object we should import. */
  }
  else if (IXform::matches(md)) {
    if (has_property(object.getProperties(), "locator")) {
      get_path = true;
    }
    else {
      get_path = children_claiming_this_object == 0;
    }

    /* Transforms are never "data" for their parent. */
    parent_is_part_of_this_object = false;
  }
  else {
    /* These types are "data" for their parent. */
    get_path = IPolyMesh::matches(md) || ISubD::matches(md) ||
#ifdef USE_NURBS
               INuPatch::matches(md) ||
#endif
               ICamera::matches(md) || IPoints::matches(md) || ICurves::matches(md);
    parent_is_part_of_this_object = get_path;
  }

  if (get_path) {
    void *abc_path_void = MEM_callocN(sizeof(AlembicObjectPath), "AlembicObjectPath");
    AlembicObjectPath *abc_path = static_cast<AlembicObjectPath *>(abc_path_void);

    BLI_strncpy(abc_path->path, object.getFullName().c_str(), sizeof(abc_path->path));
    BLI_addtail(object_paths, abc_path);
  }

  return parent_is_part_of_this_object;
}

AbcArchiveHandle *ABC_create_handle(const char *filename, ListBase *object_paths)
{
  ArchiveReader *archive = new ArchiveReader(filename);

  if (!archive->valid()) {
    delete archive;
    return NULL;
  }

  if (object_paths) {
    gather_objects_paths(archive->getTop(), object_paths);
  }

  return handle_from_archive(archive);
}

void ABC_free_handle(AbcArchiveHandle *handle)
{
  delete archive_from_handle(handle);
}

int ABC_get_version()
{
  return ALEMBIC_LIBRARY_VERSION;
}

static void find_iobject(const IObject &object, IObject &ret, const std::string &path)
{
  if (!object.valid()) {
    return;
  }

  std::vector<std::string> tokens;
  split(path, '/', tokens);

  IObject tmp = object;

  std::vector<std::string>::iterator iter;
  for (iter = tokens.begin(); iter != tokens.end(); ++iter) {
    IObject child = tmp.getChild(*iter);
    tmp = child;
  }

  ret = tmp;
}

struct ExportJobData {
  ViewLayer *view_layer;
  Main *bmain;

  char filename[1024];
  ExportSettings settings;

  short *stop;
  short *do_update;
  float *progress;

  bool was_canceled;
  bool export_ok;
};

static void export_startjob(void *customdata, short *stop, short *do_update, float *progress)
{
  ExportJobData *data = static_cast<ExportJobData *>(customdata);

  data->stop = stop;
  data->do_update = do_update;
  data->progress = progress;

  /* XXX annoying hack: needed to prevent data corruption when changing
   * scene frame in separate threads
   */
  G.is_rendering = true;
  BKE_spacedata_draw_locks(true);

  G.is_break = false;

  DEG_graph_build_from_view_layer(
      data->settings.depsgraph, data->bmain, data->settings.scene, data->view_layer);
  BKE_scene_graph_update_tagged(data->settings.depsgraph, data->bmain);

  try {
    AbcExporter exporter(data->bmain, data->filename, data->settings);

    Scene *scene = data->settings.scene; /* for the CFRA macro */
    const int orig_frame = CFRA;

    data->was_canceled = false;
    exporter(*data->progress, data->was_canceled);

    if (CFRA != orig_frame) {
      CFRA = orig_frame;

      BKE_scene_graph_update_for_newframe(data->settings.depsgraph, data->bmain);
    }

    data->export_ok = !data->was_canceled;
  }
  catch (const std::exception &e) {
    ABC_LOG(data->settings.logger) << "Abc Export error: " << e.what() << '\n';
  }
  catch (...) {
    ABC_LOG(data->settings.logger) << "Abc Export: unknown error...\n";
  }
}

static void export_endjob(void *customdata)
{
  ExportJobData *data = static_cast<ExportJobData *>(customdata);

  DEG_graph_free(data->settings.depsgraph);

  if (data->was_canceled && BLI_exists(data->filename)) {
    BLI_delete(data->filename, false, false);
  }

  std::string log = data->settings.logger.str();
  if (!log.empty()) {
    std::cerr << log;
    WM_report(RPT_ERROR, "Errors occurred during the export, look in the console to know more...");
  }

  G.is_rendering = false;
  BKE_spacedata_draw_locks(false);
}

bool ABC_export(Scene *scene,
                bContext *C,
                const char *filepath,
                const struct AlembicExportParams *params,
                bool as_background_job)
{
  ExportJobData *job = static_cast<ExportJobData *>(
      MEM_mallocN(sizeof(ExportJobData), "ExportJobData"));

  job->view_layer = CTX_data_view_layer(C);
  job->bmain = CTX_data_main(C);
  job->export_ok = false;
  BLI_strncpy(job->filename, filepath, 1024);

  /* Alright, alright, alright....
   *
   * ExportJobData contains an ExportSettings containing a SimpleLogger.
   *
   * Since ExportJobData is a C-style struct dynamically allocated with
   * MEM_mallocN (see above), its constructor is never called, therefore the
   * ExportSettings constructor is not called which implies that the
   * SimpleLogger one is not called either. SimpleLogger in turn does not call
   * the constructor of its data members which ultimately means that its
   * std::ostringstream member has a NULL pointer. To be able to properly use
   * the stream's operator<<, the pointer needs to be set, therefore we have
   * to properly construct everything. And this is done using the placement
   * new operator as here below. It seems hackish, but I'm too lazy to
   * do bigger refactor and maybe there is a better way which does not involve
   * hardcore refactoring. */
  new (&job->settings) ExportSettings();
  job->settings.scene = scene;
  job->settings.depsgraph = DEG_graph_new(scene, job->view_layer, DAG_EVAL_RENDER);

  /* TODO(Sybren): for now we only export the active scene layer.
   * Later in the 2.8 development process this may be replaced by using
   * a specific collection for Alembic I/O, which can then be toggled
   * between "real" objects and cached Alembic files. */
  job->settings.view_layer = job->view_layer;

  job->settings.frame_start = params->frame_start;
  job->settings.frame_end = params->frame_end;
  job->settings.frame_samples_xform = params->frame_samples_xform;
  job->settings.frame_samples_shape = params->frame_samples_shape;
  job->settings.shutter_open = params->shutter_open;
  job->settings.shutter_close = params->shutter_close;

  /* TODO(Sybren): For now this is ignored, until we can get selection
   * detection working through Base pointers (instead of ob->flags). */
  job->settings.selected_only = params->selected_only;

  job->settings.export_face_sets = params->face_sets;
  job->settings.export_normals = params->normals;
  job->settings.export_uvs = params->uvs;
  job->settings.export_vcols = params->vcolors;
  job->settings.export_hair = params->export_hair;
  job->settings.export_particles = params->export_particles;
  job->settings.apply_subdiv = params->apply_subdiv;
  job->settings.curves_as_mesh = params->curves_as_mesh;
  job->settings.flatten_hierarchy = params->flatten_hierarchy;

  /* TODO(Sybren): visible_layer & renderable only is ignored for now,
   * to be replaced with collections later in the 2.8 dev process
   * (also see note above). */
  job->settings.visible_layers_only = params->visible_layers_only;
  job->settings.renderable_only = params->renderable_only;

  job->settings.use_subdiv_schema = params->use_subdiv_schema;
  job->settings.export_ogawa = (params->compression_type == ABC_ARCHIVE_OGAWA);
  job->settings.pack_uv = params->packuv;
  job->settings.global_scale = params->global_scale;
  job->settings.triangulate = params->triangulate;
  job->settings.quad_method = params->quad_method;
  job->settings.ngon_method = params->ngon_method;

  if (job->settings.frame_start > job->settings.frame_end) {
    std::swap(job->settings.frame_start, job->settings.frame_end);
  }

  bool export_ok = false;
  if (as_background_job) {
    wmJob *wm_job = WM_jobs_get(CTX_wm_manager(C),
                                CTX_wm_window(C),
                                job->settings.scene,
                                "Alembic Export",
                                WM_JOB_PROGRESS,
                                WM_JOB_TYPE_ALEMBIC);

    /* setup job */
    WM_jobs_customdata_set(wm_job, job, MEM_freeN);
    WM_jobs_timer(wm_job, 0.1, NC_SCENE | ND_FRAME, NC_SCENE | ND_FRAME);
    WM_jobs_callbacks(wm_job, export_startjob, NULL, NULL, export_endjob);

    WM_jobs_start(CTX_wm_manager(C), wm_job);
  }
  else {
    /* Fake a job context, so that we don't need NULL pointer checks while exporting. */
    short stop = 0, do_update = 0;
    float progress = 0.f;

    export_startjob(job, &stop, &do_update, &progress);
    export_endjob(job);
    export_ok = job->export_ok;

    MEM_freeN(job);
  }

  return export_ok;
}

/* ********************** Import file ********************** */

/**
 * Generates an AbcObjectReader for this Alembic object and its children.
 *
 * \param object: The Alembic IObject to visit.
 * \param readers: The created AbcObjectReader * will be appended to this vector.
 * \param settings: Import settings, not used directly but passed to the
 *                 AbcObjectReader subclass constructors.
 * \param r_assign_as_parent: Return parameter, contains a list of reader
 *                 pointers, whose parent pointer should still be set.
 *                 This is filled when this call to visit_object() didn't create
 *                 a reader that should be the parent.
 * \return A pair of boolean and reader pointer. The boolean indicates whether
 *         this IObject claims its parent as part of the same object
 *         (for example an IPolyMesh object would claim its parent, as the mesh
 *         is interpreted as the object's data, and the parent IXform as its
 *         Blender object). The pointer is the AbcObjectReader that represents
 *         the IObject parameter.
 *
 * NOTE: this function is similar to gather_object_paths above, need to keep
 * them in sync. */
static std::pair<bool, AbcObjectReader *> visit_object(
    const IObject &object,
    AbcObjectReader::ptr_vector &readers,
    ImportSettings &settings,
    AbcObjectReader::ptr_vector &r_assign_as_parent)
{
  const std::string &full_name = object.getFullName();

  if (!object.valid()) {
    std::cerr << "  - " << full_name << ": object is invalid, skipping it and all its children.\n";
    return std::make_pair(false, static_cast<AbcObjectReader *>(NULL));
  }

  /* The interpretation of data by the children determine the role of this
   * object. This is especially important for Xform objects, as they can be
   * either part of a Blender object or a Blender object (Empty) themselves.
   */
  size_t children_claiming_this_object = 0;
  size_t num_children = object.getNumChildren();
  AbcObjectReader::ptr_vector claiming_child_readers;
  AbcObjectReader::ptr_vector nonclaiming_child_readers;
  AbcObjectReader::ptr_vector assign_as_parent;
  for (size_t i = 0; i < num_children; ++i) {
    const IObject ichild = object.getChild(i);

    /* TODO: When we only support C++11, use std::tie() instead. */
    std::pair<bool, AbcObjectReader *> child_result;
    child_result = visit_object(ichild, readers, settings, assign_as_parent);

    bool child_claims_this_object = child_result.first;
    AbcObjectReader *child_reader = child_result.second;

    if (child_reader == NULL) {
      BLI_assert(!child_claims_this_object);
    }
    else {
      if (child_claims_this_object) {
        claiming_child_readers.push_back(child_reader);
      }
      else {
        nonclaiming_child_readers.push_back(child_reader);
      }
    }

    children_claiming_this_object += child_claims_this_object ? 1 : 0;
  }
  BLI_assert(children_claiming_this_object == claiming_child_readers.size());

  AbcObjectReader *reader = NULL;
  const MetaData &md = object.getMetaData();
  bool parent_is_part_of_this_object = false;

  if (!object.getParent()) {
    /* The root itself is not an object we should import. */
  }
  else if (IXform::matches(md)) {
    bool create_empty;

    /* An xform can either be a Blender Object (if it contains a mesh, for
     * example), but it can also be an Empty. Its correct translation to
     * Blender's data model depends on its children. */

    /* Check whether or not this object is a Maya locator, which is
     * similar to empties used as parent object in Blender. */
    if (has_property(object.getProperties(), "locator")) {
      create_empty = true;
    }
    else {
      create_empty = claiming_child_readers.empty();
    }

    if (create_empty) {
      reader = new AbcEmptyReader(object, settings);
    }
  }
  else if (IPolyMesh::matches(md)) {
    reader = new AbcMeshReader(object, settings);
    parent_is_part_of_this_object = true;
  }
  else if (ISubD::matches(md)) {
    reader = new AbcSubDReader(object, settings);
    parent_is_part_of_this_object = true;
  }
  else if (INuPatch::matches(md)) {
#ifdef USE_NURBS
    /* TODO(kevin): importing cyclic NURBS from other software crashes
     * at the moment. This is due to the fact that NURBS in other
     * software have duplicated points which causes buffer overflows in
     * Blender. Need to figure out exactly how these points are
     * duplicated, in all cases (cyclic U, cyclic V, and cyclic UV).
     * Until this is fixed, disabling NURBS reading. */
    reader = new AbcNurbsReader(object, settings);
    parent_is_part_of_this_object = true;
#endif
  }
  else if (ICamera::matches(md)) {
    reader = new AbcCameraReader(object, settings);
    parent_is_part_of_this_object = true;
  }
  else if (IPoints::matches(md)) {
    reader = new AbcPointsReader(object, settings);
    parent_is_part_of_this_object = true;
  }
  else if (IMaterial::matches(md)) {
    /* Pass for now. */
  }
  else if (ILight::matches(md)) {
    /* Pass for now. */
  }
  else if (IFaceSet::matches(md)) {
    /* Pass, those are handled in the mesh reader. */
  }
  else if (ICurves::matches(md)) {
    reader = new AbcCurveReader(object, settings);
    parent_is_part_of_this_object = true;
  }
  else {
    std::cerr << "Alembic object " << full_name << " is of unsupported schema type '"
              << object.getMetaData().get("schemaObjTitle") << "'" << std::endl;
  }

  if (reader) {
    /* We have created a reader, which should imply that this object is
     * not claimed as part of any child Alembic object. */
    BLI_assert(claiming_child_readers.empty());

    readers.push_back(reader);
    reader->incref();

    AlembicObjectPath *abc_path = static_cast<AlembicObjectPath *>(
        MEM_callocN(sizeof(AlembicObjectPath), "AlembicObjectPath"));
    BLI_strncpy(abc_path->path, full_name.c_str(), sizeof(abc_path->path));
    BLI_addtail(&settings.cache_file->object_paths, abc_path);

    /* We can now assign this reader as parent for our children. */
    if (nonclaiming_child_readers.size() + assign_as_parent.size() > 0) {
      for (AbcObjectReader *child_reader : nonclaiming_child_readers) {
        child_reader->parent_reader = reader;
      }
      for (AbcObjectReader *child_reader : assign_as_parent) {
        child_reader->parent_reader = reader;
      }
    }
  }
  else if (object.getParent()) {
    if (claiming_child_readers.size() > 0) {
      /* The first claiming child will serve just fine as parent to
       * our non-claiming children. Since all claiming children share
       * the same XForm, it doesn't really matter which one we pick. */
      AbcObjectReader *claiming_child = claiming_child_readers[0];
      for (AbcObjectReader *child_reader : nonclaiming_child_readers) {
        child_reader->parent_reader = claiming_child;
      }
      for (AbcObjectReader *child_reader : assign_as_parent) {
        child_reader->parent_reader = claiming_child;
      }
      /* Claiming children should have our parent set as their parent. */
      for (AbcObjectReader *child_reader : claiming_child_readers) {
        r_assign_as_parent.push_back(child_reader);
      }
    }
    else {
      /* This object isn't claimed by any child, and didn't produce
       * a reader. Odd situation, could be the top Alembic object, or
       * an unsupported Alembic schema. Delegate to our parent. */
      for (AbcObjectReader *child_reader : claiming_child_readers) {
        r_assign_as_parent.push_back(child_reader);
      }
      for (AbcObjectReader *child_reader : nonclaiming_child_readers) {
        r_assign_as_parent.push_back(child_reader);
      }
      for (AbcObjectReader *child_reader : assign_as_parent) {
        r_assign_as_parent.push_back(child_reader);
      }
    }
  }

  return std::make_pair(parent_is_part_of_this_object, reader);
}

enum {
  ABC_NO_ERROR = 0,
  ABC_ARCHIVE_FAIL,
  ABC_UNSUPPORTED_HDF5,
};

struct ImportJobData {
  Main *bmain;
  Scene *scene;
  ViewLayer *view_layer;
  wmWindowManager *wm;

  char filename[1024];
  ImportSettings settings;

  ArchiveReader *archive;
  std::vector<AbcObjectReader *> readers;

  short *stop;
  short *do_update;
  float *progress;

  char error_code;
  bool was_cancelled;
  bool import_ok;
};

static void import_startjob(void *user_data, short *stop, short *do_update, float *progress)
{
  SCOPE_TIMER("Alembic import, objects reading and creation");

  ImportJobData *data = static_cast<ImportJobData *>(user_data);

  data->stop = stop;
  data->do_update = do_update;
  data->progress = progress;

  WM_set_locked_interface(data->wm, true);

  ArchiveReader *archive = new ArchiveReader(data->filename);

  if (!archive->valid()) {
#ifndef WITH_ALEMBIC_HDF5
    data->error_code = archive->is_hdf5() ? ABC_UNSUPPORTED_HDF5 : ABC_ARCHIVE_FAIL;
#else
    data->error_code = ABC_ARCHIVE_FAIL;
#endif
    delete archive;
    return;
  }

  CacheFile *cache_file = static_cast<CacheFile *>(
      BKE_cachefile_add(data->bmain, BLI_path_basename(data->filename)));

  /* Decrement the ID ref-count because it is going to be incremented for each
   * modifier and constraint that it will be attached to, so since currently
   * it is not used by anyone, its use count will off by one. */
  id_us_min(&cache_file->id);

  cache_file->is_sequence = data->settings.is_sequence;
  cache_file->scale = data->settings.scale;
  STRNCPY(cache_file->filepath, data->filename);

  data->archive = archive;
  data->settings.cache_file = cache_file;

  *data->do_update = true;
  *data->progress = 0.05f;

  /* Parse Alembic Archive. */
  AbcObjectReader::ptr_vector assign_as_parent;
  visit_object(archive->getTop(), data->readers, data->settings, assign_as_parent);

  /* There shouldn't be any orphans. */
  BLI_assert(assign_as_parent.size() == 0);

  if (G.is_break) {
    data->was_cancelled = true;
    return;
  }

  *data->do_update = true;
  *data->progress = 0.1f;

  /* Create objects and set scene frame range. */

  const float size = static_cast<float>(data->readers.size());
  size_t i = 0;

  chrono_t min_time = std::numeric_limits<chrono_t>::max();
  chrono_t max_time = std::numeric_limits<chrono_t>::min();

  ISampleSelector sample_sel(0.0f);
  std::vector<AbcObjectReader *>::iterator iter;
  for (iter = data->readers.begin(); iter != data->readers.end(); ++iter) {
    AbcObjectReader *reader = *iter;

    if (reader->valid()) {
      reader->readObjectData(data->bmain, sample_sel);

      min_time = std::min(min_time, reader->minTime());
      max_time = std::max(max_time, reader->maxTime());
    }
    else {
      std::cerr << "Object " << reader->name() << " in Alembic file " << data->filename
                << " is invalid.\n";
    }

    *data->progress = 0.1f + 0.3f * (++i / size);
    *data->do_update = true;

    if (G.is_break) {
      data->was_cancelled = true;
      return;
    }
  }

  if (data->settings.set_frame_range) {
    Scene *scene = data->scene;

    if (data->settings.is_sequence) {
      SFRA = data->settings.sequence_offset;
      EFRA = SFRA + (data->settings.sequence_len - 1);
      CFRA = SFRA;
    }
    else if (min_time < max_time) {
      SFRA = static_cast<int>(round(min_time * FPS));
      EFRA = static_cast<int>(round(max_time * FPS));
      CFRA = SFRA;
    }
  }

  /* Setup parenthood. */
  for (iter = data->readers.begin(); iter != data->readers.end(); ++iter) {
    const AbcObjectReader *reader = *iter;
    const AbcObjectReader *parent_reader = reader->parent_reader;
    Object *ob = reader->object();

    if (parent_reader == NULL || !reader->inherits_xform()) {
      ob->parent = NULL;
    }
    else {
      ob->parent = parent_reader->object();
    }
  }

  /* Setup transformations and constraints. */
  i = 0;
  for (iter = data->readers.begin(); iter != data->readers.end(); ++iter) {
    AbcObjectReader *reader = *iter;
    reader->setupObjectTransform(0.0f);

    *data->progress = 0.7f + 0.3f * (++i / size);
    *data->do_update = true;

    if (G.is_break) {
      data->was_cancelled = true;
      return;
    }
  }
}

static void import_endjob(void *user_data)
{
  SCOPE_TIMER("Alembic import, cleanup");

  ImportJobData *data = static_cast<ImportJobData *>(user_data);

  std::vector<AbcObjectReader *>::iterator iter;

  /* Delete objects on cancelation. */
  if (data->was_cancelled) {
    for (iter = data->readers.begin(); iter != data->readers.end(); ++iter) {
      Object *ob = (*iter)->object();

      /* It's possible that cancellation occurred between the creation of
       * the reader and the creation of the Blender object. */
      if (ob == NULL) {
        continue;
      }

      BKE_id_free_us(data->bmain, ob);
    }
  }
  else {
    /* Add object to scene. */
    Base *base;
    LayerCollection *lc;
    ViewLayer *view_layer = data->view_layer;

    BKE_view_layer_base_deselect_all(view_layer);

    lc = BKE_layer_collection_get_active(view_layer);

    for (iter = data->readers.begin(); iter != data->readers.end(); ++iter) {
      Object *ob = (*iter)->object();

      BKE_collection_object_add(data->bmain, lc->collection, ob);

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
    DEG_relations_tag_update(data->bmain);
  }

  for (iter = data->readers.begin(); iter != data->readers.end(); ++iter) {
    AbcObjectReader *reader = *iter;
    reader->decref();

    if (reader->refcount() == 0) {
      delete reader;
    }
  }

  WM_set_locked_interface(data->wm, false);

  switch (data->error_code) {
    default:
    case ABC_NO_ERROR:
      data->import_ok = !data->was_cancelled;
      break;
    case ABC_ARCHIVE_FAIL:
      WM_report(RPT_ERROR, "Could not open Alembic archive for reading! See console for detail.");
      break;
    case ABC_UNSUPPORTED_HDF5:
      WM_report(RPT_ERROR, "Alembic archive in obsolete HDF5 format is not supported.");
      break;
  }

  WM_main_add_notifier(NC_SCENE | ND_FRAME, data->scene);
}

static void import_freejob(void *user_data)
{
  ImportJobData *data = static_cast<ImportJobData *>(user_data);
  delete data->archive;
  delete data;
}

bool ABC_import(bContext *C,
                const char *filepath,
                float scale,
                bool is_sequence,
                bool set_frame_range,
                int sequence_len,
                int offset,
                bool validate_meshes,
                bool as_background_job)
{
  /* Using new here since MEM_* funcs do not call ctor to properly initialize
   * data. */
  ImportJobData *job = new ImportJobData();
  job->bmain = CTX_data_main(C);
  job->scene = CTX_data_scene(C);
  job->view_layer = CTX_data_view_layer(C);
  job->wm = CTX_wm_manager(C);
  job->import_ok = false;
  BLI_strncpy(job->filename, filepath, 1024);

  job->settings.scale = scale;
  job->settings.is_sequence = is_sequence;
  job->settings.set_frame_range = set_frame_range;
  job->settings.sequence_len = sequence_len;
  job->settings.sequence_offset = offset;
  job->settings.validate_meshes = validate_meshes;
  job->error_code = ABC_NO_ERROR;
  job->was_cancelled = false;
  job->archive = NULL;

  G.is_break = false;

  bool import_ok = false;
  if (as_background_job) {
    wmJob *wm_job = WM_jobs_get(CTX_wm_manager(C),
                                CTX_wm_window(C),
                                job->scene,
                                "Alembic Import",
                                WM_JOB_PROGRESS,
                                WM_JOB_TYPE_ALEMBIC);

    /* setup job */
    WM_jobs_customdata_set(wm_job, job, import_freejob);
    WM_jobs_timer(wm_job, 0.1, NC_SCENE | ND_FRAME, NC_SCENE | ND_FRAME);
    WM_jobs_callbacks(wm_job, import_startjob, NULL, NULL, import_endjob);

    WM_jobs_start(CTX_wm_manager(C), wm_job);
  }
  else {
    /* Fake a job context, so that we don't need NULL pointer checks while importing. */
    short stop = 0, do_update = 0;
    float progress = 0.f;

    import_startjob(job, &stop, &do_update, &progress);
    import_endjob(job);
    import_ok = job->import_ok;

    import_freejob(job);
  }

  return import_ok;
}

/* ************************************************************************** */

void ABC_get_transform(CacheReader *reader, float r_mat[4][4], float time, float scale)
{
  if (!reader) {
    return;
  }

  AbcObjectReader *abc_reader = reinterpret_cast<AbcObjectReader *>(reader);

  bool is_constant = false;
  abc_reader->read_matrix(r_mat, time, scale, is_constant);
}

/* ************************************************************************** */

Mesh *ABC_read_mesh(CacheReader *reader,
                    Object *ob,
                    Mesh *existing_mesh,
                    const float time,
                    const char **err_str,
                    int read_flag)
{
  AbcObjectReader *abc_reader = reinterpret_cast<AbcObjectReader *>(reader);
  IObject iobject = abc_reader->iobject();

  if (!iobject.valid()) {
    *err_str = "Invalid object: verify object path";
    return NULL;
  }

  const ObjectHeader &header = iobject.getHeader();
  if (!abc_reader->accepts_object_type(header, ob, err_str)) {
    /* err_str is set by acceptsObjectType() */
    return NULL;
  }

  /* kFloorIndex is used to be compatible with non-interpolating
   * properties; they use the floor. */
  ISampleSelector sample_sel(time, ISampleSelector::kFloorIndex);
  return abc_reader->read_mesh(existing_mesh, sample_sel, read_flag, err_str);
}

/* ************************************************************************** */

void CacheReader_free(CacheReader *reader)
{
  AbcObjectReader *abc_reader = reinterpret_cast<AbcObjectReader *>(reader);
  abc_reader->decref();

  if (abc_reader->refcount() == 0) {
    delete abc_reader;
  }
}

void CacheReader_incref(CacheReader *reader)
{
  AbcObjectReader *abc_reader = reinterpret_cast<AbcObjectReader *>(reader);
  abc_reader->incref();
}

CacheReader *CacheReader_open_alembic_object(AbcArchiveHandle *handle,
                                             CacheReader *reader,
                                             Object *object,
                                             const char *object_path)
{
  if (object_path[0] == '\0') {
    return reader;
  }

  ArchiveReader *archive = archive_from_handle(handle);

  if (!archive || !archive->valid()) {
    return reader;
  }

  IObject iobject;
  find_iobject(archive->getTop(), iobject, object_path);

  if (reader) {
    CacheReader_free(reader);
  }

  ImportSettings settings;
  AbcObjectReader *abc_reader = create_reader(iobject, settings);
  if (abc_reader == NULL) {
    /* This object is not supported */
    return NULL;
  }
  abc_reader->object(object);
  abc_reader->incref();

  return reinterpret_cast<CacheReader *>(abc_reader);
}
