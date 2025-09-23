/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "ABC_alembic.h"
#include "IO_subdiv_disabler.hh"
#include "abc_archive.h"
#include "abc_hierarchy_iterator.h"

#include "MEM_guardedalloc.h"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"
#include "DEG_depsgraph_query.hh"

#include "DNA_scene_types.h"

#include "BKE_context.hh"
#include "BKE_global.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_scene.hh"

#include "BLI_fileops.h"
#include "BLI_path_utils.hh"
#include "BLI_string.h"
#include "BLI_timeit.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "CLG_log.h"
static CLG_LogRef LOG = {"io.alembic"};

#include <memory>

struct ExportJobData {
  Main *bmain = nullptr;
  Depsgraph *depsgraph = nullptr;
  wmWindowManager *wm = nullptr;

  char filepath[FILE_MAX] = {};
  AlembicExportParams params = {};

  bool was_canceled = false;
  bool export_ok = false;
  blender::timeit::TimePoint start_time = {};
};

namespace blender::io::alembic {

/* Construct the depsgraph for exporting. */
static bool build_depsgraph(ExportJobData *job)
{
  if (job->params.collection[0]) {
    Collection *collection = reinterpret_cast<Collection *>(
        BKE_libblock_find_name(job->bmain, ID_GR, job->params.collection));
    if (!collection) {
      WM_global_reportf(
          RPT_ERROR, "Alembic Export: Unable to find collection '%s'", job->params.collection);
      return false;
    }

    DEG_graph_build_from_collection(job->depsgraph, collection);
  }
  else {
    DEG_graph_build_from_view_layer(job->depsgraph);
  }

  return true;
}

static void report_job_duration(const ExportJobData *data)
{
  blender::timeit::Nanoseconds duration = blender::timeit::Clock::now() - data->start_time;
  std::cout << "Alembic export of '" << data->filepath << "' took ";
  blender::timeit::print_duration(duration);
  std::cout << '\n';
}

static void export_startjob(void *customdata, wmJobWorkerStatus *worker_status)
{
  ExportJobData *data = static_cast<ExportJobData *>(customdata);
  data->was_canceled = false;
  data->start_time = blender::timeit::Clock::now();

  G.is_rendering = true;
  WM_locked_interface_set(data->wm, true);
  G.is_break = false;

  worker_status->progress = 0.0f;
  worker_status->do_update = true;

  BKE_scene_graph_update_tagged(data->depsgraph, data->bmain);

  SubdivModifierDisabler subdiv_disabler(data->depsgraph);
  if (!data->params.apply_subdiv) {
    subdiv_disabler.disable_modifiers();
    BKE_scene_graph_update_tagged(data->depsgraph, data->bmain);
  }

  /* For restoring the current frame after exporting animation is done. */
  Scene *scene = DEG_get_input_scene(data->depsgraph);
  const int orig_frame = scene->r.cfra;
  const bool export_animation = (data->params.frame_start != data->params.frame_end);

  /* Create the Alembic archive. */
  std::unique_ptr<ABCArchive> abc_archive;
  try {
    abc_archive = std::make_unique<ABCArchive>(
        data->bmain, scene, data->params, std::string(data->filepath));
  }
  catch (const std::exception &ex) {
    std::stringstream error_message_stream;
    error_message_stream << "Error writing to " << data->filepath;
    const std::string &error_message = error_message_stream.str();

    /* The exception message can be very cryptic (just "iostream error" on Linux, for example),
     * so better not to include it in the report. */
    CLOG_ERROR(&LOG, "%s: %s", error_message.c_str(), ex.what());
    WM_global_report(RPT_ERROR, error_message.c_str());
    data->export_ok = false;
    return;
  }
  catch (...) {
    /* Unknown exception class, so we cannot include its message. */
    std::stringstream error_message_stream;
    error_message_stream << "Unknown error writing to " << data->filepath;
    WM_global_report(RPT_ERROR, error_message_stream.str().c_str());
    data->export_ok = false;
    return;
  }

  ABCHierarchyIterator iter(data->bmain, data->depsgraph, abc_archive.get(), data->params);

  if (export_animation) {
    CLOG_STR_DEBUG(&LOG, "Exporting animation");

    /* Writing the animated frames is not 100% of the work, but it's our best guess. */
    const float progress_per_frame = 1.0f / std::max(size_t(1), abc_archive->total_frame_count());
    ABCArchive::Frames::const_iterator frame_it = abc_archive->frames_begin();
    const ABCArchive::Frames::const_iterator frames_end = abc_archive->frames_end();

    for (; frame_it != frames_end; frame_it++) {
      double frame = *frame_it;

      if (G.is_break || worker_status->stop) {
        break;
      }

      /* Update the scene for the next frame to render. */
      scene->r.cfra = int(frame);
      scene->r.subframe = float(frame - scene->r.cfra);
      BKE_scene_graph_update_for_newframe(data->depsgraph);

      CLOG_DEBUG(&LOG, "Exporting frame %.2f", frame);
      ExportSubset export_subset = abc_archive->export_subset_for_frame(frame);
      iter.set_export_subset(export_subset);
      iter.iterate_and_write();

      worker_status->progress += progress_per_frame;
      worker_status->do_update = true;
    }
  }
  else {
    /* If we're not animating, a single iteration over all objects is enough. */
    iter.iterate_and_write();
  }

  iter.release_writers();

  /* Finish up by going back to the keyframe that was current before we started. */
  if (scene->r.cfra != orig_frame) {
    scene->r.cfra = orig_frame;
    BKE_scene_graph_update_for_newframe(data->depsgraph);
  }

  data->export_ok = !data->was_canceled;

  worker_status->progress = 1.0f;
  worker_status->do_update = true;
}

static void export_endjob(void *customdata)
{
  ExportJobData *data = static_cast<ExportJobData *>(customdata);

  DEG_graph_free(data->depsgraph);

  if (data->was_canceled && BLI_exists(data->filepath)) {
    BLI_delete(data->filepath, false, false);
  }

  G.is_rendering = false;
  WM_locked_interface_set(data->wm, false);
  report_job_duration(data);
}

}  // namespace blender::io::alembic

bool ABC_export(Scene *scene,
                bContext *C,
                const char *filepath,
                const AlembicExportParams *params,
                bool as_background_job)
{
  ViewLayer *view_layer = CTX_data_view_layer(C);

  ExportJobData *job = MEM_new<ExportJobData>("ExportJobData");

  job->bmain = CTX_data_main(C);
  job->wm = CTX_wm_manager(C);
  job->export_ok = false;
  STRNCPY(job->filepath, filepath);

  job->depsgraph = DEG_graph_new(job->bmain, scene, view_layer, params->evaluation_mode);
  job->params = *params;

  /* Construct the depsgraph for exporting.
   *
   * Has to be done from main thread currently, as it may affect Main original data (e.g. when
   * doing deferred update of the view-layers, see #112534 for details). */
  if (!blender::io::alembic::build_depsgraph(job)) {
    return false;
  }

  bool export_ok = false;
  if (as_background_job) {
    wmJob *wm_job = WM_jobs_get(job->wm,
                                CTX_wm_window(C),
                                scene,
                                "Exporting Alembic...",
                                WM_JOB_PROGRESS,
                                WM_JOB_TYPE_ALEMBIC_EXPORT);

    /* setup job */
    WM_jobs_customdata_set(
        wm_job, job, [](void *j) { MEM_delete(static_cast<ExportJobData *>(j)); });
    WM_jobs_timer(wm_job, 0.1, NC_SCENE | ND_FRAME, NC_SCENE | ND_FRAME);
    WM_jobs_callbacks(wm_job,
                      blender::io::alembic::export_startjob,
                      nullptr,
                      nullptr,
                      blender::io::alembic::export_endjob);

    WM_jobs_start(CTX_wm_manager(C), wm_job);
  }
  else {
    wmJobWorkerStatus worker_status = {};
    blender::io::alembic::export_startjob(job, &worker_status);
    blender::io::alembic::export_endjob(job);
    export_ok = job->export_ok;

    MEM_delete(job);
  }

  return export_ok;
}
