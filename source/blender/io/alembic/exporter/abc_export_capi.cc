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
#include "abc_archive.h"
#include "abc_hierarchy_iterator.h"
#include "abc_subdiv_disabler.h"

#include "MEM_guardedalloc.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"
#include "DEG_depsgraph_query.h"

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

#include "CLG_log.h"
static CLG_LogRef LOG = {"io.alembic"};

#include <algorithm>
#include <memory>

struct ExportJobData {
  Main *bmain;
  Depsgraph *depsgraph;
  wmWindowManager *wm;

  char filename[FILE_MAX];
  AlembicExportParams params;

  bool was_canceled;
  bool export_ok;
};

namespace blender {
namespace io {
namespace alembic {

// Construct the depsgraph for exporting.
static void build_depsgraph(Depsgraph *depsgraph, Main *bmain)
{
  Scene *scene = DEG_get_input_scene(depsgraph);
  ViewLayer *view_layer = DEG_get_input_view_layer(depsgraph);
  DEG_graph_build_from_view_layer(depsgraph, bmain, scene, view_layer);
}

static void export_startjob(void *customdata,
                            /* Cannot be const, this function implements wm_jobs_start_callback.
                             * NOLINTNEXTLINE: readability-non-const-parameter. */
                            short *stop,
                            short *do_update,
                            float *progress)
{
  ExportJobData *data = static_cast<ExportJobData *>(customdata);
  data->was_canceled = false;

  G.is_rendering = true;
  WM_set_locked_interface(data->wm, true);
  G.is_break = false;

  *progress = 0.0f;
  *do_update = true;

  build_depsgraph(data->depsgraph, data->bmain);
  SubdivModifierDisabler subdiv_disabler(data->depsgraph);
  if (!data->params.apply_subdiv) {
    subdiv_disabler.disable_modifiers();
  }
  BKE_scene_graph_update_tagged(data->depsgraph, data->bmain);

  // For restoring the current frame after exporting animation is done.
  Scene *scene = DEG_get_input_scene(data->depsgraph);
  const int orig_frame = CFRA;
  const bool export_animation = (data->params.frame_start != data->params.frame_end);

  // Create the Alembic archive.
  std::unique_ptr<ABCArchive> abc_archive;
  try {
    abc_archive = std::make_unique<ABCArchive>(
        data->bmain, scene, data->params, std::string(data->filename));
  }
  catch (const std::exception &ex) {
    std::stringstream error_message_stream;
    error_message_stream << "Error writing to " << data->filename;
    const std::string &error_message = error_message_stream.str();

    // The exception message can be very cryptic (just "iostream error" on Linux, for example), so
    // better not to include it in the report.
    CLOG_ERROR(&LOG, "%s: %s", error_message.c_str(), ex.what());
    WM_report(RPT_ERROR, error_message.c_str());
    data->export_ok = false;
    return;
  }
  catch (...) {
    // Unknown exception class, so we cannot include its message.
    std::stringstream error_message_stream;
    error_message_stream << "Unknown error writing to " << data->filename;
    WM_report(RPT_ERROR, error_message_stream.str().c_str());
    data->export_ok = false;
    return;
  }

  ABCHierarchyIterator iter(data->depsgraph, abc_archive.get(), data->params);

  if (export_animation) {
    CLOG_INFO(&LOG, 2, "Exporting animation");

    // Writing the animated frames is not 100% of the work, but it's our best guess.
    const float progress_per_frame = 1.0f / std::max(size_t(1), abc_archive->total_frame_count());
    ABCArchive::Frames::const_iterator frame_it = abc_archive->frames_begin();
    const ABCArchive::Frames::const_iterator frames_end = abc_archive->frames_end();

    for (; frame_it != frames_end; frame_it++) {
      double frame = *frame_it;

      if (G.is_break || (stop != nullptr && *stop)) {
        break;
      }

      // Update the scene for the next frame to render.
      scene->r.cfra = static_cast<int>(frame);
      scene->r.subframe = frame - scene->r.cfra;
      BKE_scene_graph_update_for_newframe(data->depsgraph, data->bmain);

      CLOG_INFO(&LOG, 2, "Exporting frame %.2f", frame);
      ExportSubset export_subset = abc_archive->export_subset_for_frame(frame);
      iter.set_export_subset(export_subset);
      iter.iterate_and_write();

      *progress += progress_per_frame;
      *do_update = true;
    }
  }
  else {
    // If we're not animating, a single iteration over all objects is enough.
    iter.iterate_and_write();
  }

  iter.release_writers();

  // Finish up by going back to the keyframe that was current before we started.
  if (CFRA != orig_frame) {
    CFRA = orig_frame;
    BKE_scene_graph_update_for_newframe(data->depsgraph, data->bmain);
  }

  data->export_ok = !data->was_canceled;

  *progress = 1.0f;
  *do_update = true;
}

static void export_endjob(void *customdata)
{
  ExportJobData *data = static_cast<ExportJobData *>(customdata);

  DEG_graph_free(data->depsgraph);

  if (data->was_canceled && BLI_exists(data->filename)) {
    BLI_delete(data->filename, false, false);
  }

  G.is_rendering = false;
  WM_set_locked_interface(data->wm, false);
}

}  // namespace alembic
}  // namespace io
}  // namespace blender

bool ABC_export(Scene *scene,
                bContext *C,
                const char *filepath,
                const AlembicExportParams *params,
                bool as_background_job)
{
  ViewLayer *view_layer = CTX_data_view_layer(C);

  ExportJobData *job = static_cast<ExportJobData *>(
      MEM_mallocN(sizeof(ExportJobData), "ExportJobData"));

  job->bmain = CTX_data_main(C);
  job->wm = CTX_wm_manager(C);
  job->export_ok = false;
  BLI_strncpy(job->filename, filepath, sizeof(job->filename));

  job->depsgraph = DEG_graph_new(
      job->bmain, scene, view_layer, DAG_EVAL_RENDER /* TODO(Sybren): params->evaluation_mode */);
  job->params = *params;

  bool export_ok = false;
  if (as_background_job) {
    wmJob *wm_job = WM_jobs_get(
        job->wm, CTX_wm_window(C), scene, "Alembic Export", WM_JOB_PROGRESS, WM_JOB_TYPE_ALEMBIC);

    /* setup job */
    WM_jobs_customdata_set(wm_job, job, MEM_freeN);
    WM_jobs_timer(wm_job, 0.1, NC_SCENE | ND_FRAME, NC_SCENE | ND_FRAME);
    WM_jobs_callbacks(wm_job,
                      blender::io::alembic::export_startjob,
                      NULL,
                      NULL,
                      blender::io::alembic::export_endjob);

    WM_jobs_start(CTX_wm_manager(C), wm_job);
  }
  else {
    /* Fake a job context, so that we don't need NULL pointer checks while exporting. */
    short stop = 0, do_update = 0;
    float progress = 0.f;

    blender::io::alembic::export_startjob(job, &stop, &do_update, &progress);
    blender::io::alembic::export_endjob(job);
    export_ok = job->export_ok;

    MEM_freeN(job);
  }

  return export_ok;
}
