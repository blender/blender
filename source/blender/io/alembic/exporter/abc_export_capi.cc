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
 * The Original Code is Copyright (C) 2020 Blender Foundation.
 * All rights reserved.
 */

#include "ABC_alembic.h"
#include "abc_writer_camera.h"
#include "abc_writer_curves.h"
#include "abc_writer_hair.h"
#include "abc_writer_mesh.h"
#include "abc_writer_nurbs.h"
#include "abc_writer_points.h"
#include "abc_writer_transform.h"

#include "MEM_guardedalloc.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"

#include "DNA_modifier_types.h"
#include "DNA_scene_types.h"

#include "BKE_blender_version.h"
#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_scene.h"

#include "BLI_fileops.h"
#include "BLI_path_util.h"
#include "BLI_string.h"

#include "WM_api.h"
#include "WM_types.h"

using namespace blender::io::alembic;

struct ExportJobData {
  ViewLayer *view_layer;
  Main *bmain;
  wmWindowManager *wm;

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
  WM_set_locked_interface(data->wm, true);
  G.is_break = false;

  DEG_graph_build_from_view_layer(
      data->settings.depsgraph, data->bmain, data->settings.scene, data->view_layer);
  BKE_scene_graph_update_tagged(data->settings.depsgraph, data->bmain);

  try {
    AbcExporter exporter(data->bmain, data->filename, data->settings);

    Scene *scene = data->settings.scene; /* for the CFRA macro */
    const int orig_frame = CFRA;

    data->was_canceled = false;
    exporter(do_update, progress, &data->was_canceled);

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
  WM_set_locked_interface(data->wm, false);
}

bool ABC_export(struct Scene *scene,
                struct bContext *C,
                const char *filepath,
                const struct AlembicExportParams *params,
                bool as_background_job)
{
  ExportJobData *job = static_cast<ExportJobData *>(
      MEM_mallocN(sizeof(ExportJobData), "ExportJobData"));

  job->view_layer = CTX_data_view_layer(C);
  job->bmain = CTX_data_main(C);
  job->wm = CTX_wm_manager(C);
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
  job->settings.depsgraph = DEG_graph_new(job->bmain, scene, job->view_layer, DAG_EVAL_RENDER);

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
  job->settings.visible_objects_only = params->visible_objects_only;
  job->settings.renderable_only = params->renderable_only;

  job->settings.use_subdiv_schema = params->use_subdiv_schema;
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
