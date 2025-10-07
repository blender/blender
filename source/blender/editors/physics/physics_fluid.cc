/* SPDX-FileCopyrightText: Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edphys
 */

#include <cstdlib>
#include <cstring>
#include <sys/stat.h>

#include "MEM_guardedalloc.h"

/* types */
#include "DNA_object_types.h"

#include "BLI_fileops.h"
#include "BLI_path_utils.hh"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_time.h"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "BKE_context.hh"
#include "BKE_fluid.h"
#include "BKE_global.hh"
#include "BKE_main.hh"
#include "BKE_modifier.hh"
#include "BKE_report.hh"
#include "BKE_screen.hh"

#include "DEG_depsgraph.hh"

#include "ED_object.hh"
#include "ED_screen.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "physics_intern.hh" /* own include */

#include "DNA_fluid_types.h"
#include "DNA_scene_types.h"

#define FLUID_JOB_BAKE_ALL "FLUID_OT_bake_all"
#define FLUID_JOB_BAKE_DATA "FLUID_OT_bake_data"
#define FLUID_JOB_BAKE_NOISE "FLUID_OT_bake_noise"
#define FLUID_JOB_BAKE_MESH "FLUID_OT_bake_mesh"
#define FLUID_JOB_BAKE_PARTICLES "FLUID_OT_bake_particles"
#define FLUID_JOB_BAKE_GUIDES "FLUID_OT_bake_guides"
#define FLUID_JOB_FREE_ALL "FLUID_OT_free_all"
#define FLUID_JOB_FREE_DATA "FLUID_OT_free_data"
#define FLUID_JOB_FREE_NOISE "FLUID_OT_free_noise"
#define FLUID_JOB_FREE_MESH "FLUID_OT_free_mesh"
#define FLUID_JOB_FREE_PARTICLES "FLUID_OT_free_particles"
#define FLUID_JOB_FREE_GUIDES "FLUID_OT_free_guides"
#define FLUID_JOB_BAKE_PAUSE "FLUID_OT_pause_bake"

struct FluidJob {
  /* from wmJob */
  void *owner;
  bool *stop, *do_update;
  float *progress;
  const char *type;
  const char *name;

  Main *bmain;
  Scene *scene;
  Depsgraph *depsgraph;
  Object *ob;

  FluidModifierData *fmd;

  int success;
  double start;

  int *pause_frame;
};

static inline bool fluid_is_bake_all(FluidJob *job)
{
  return STREQ(job->type, FLUID_JOB_BAKE_ALL);
}
static inline bool fluid_is_bake_data(FluidJob *job)
{
  return STREQ(job->type, FLUID_JOB_BAKE_DATA);
}
static inline bool fluid_is_bake_noise(FluidJob *job)
{
  return STREQ(job->type, FLUID_JOB_BAKE_NOISE);
}
static inline bool fluid_is_bake_mesh(FluidJob *job)
{
  return STREQ(job->type, FLUID_JOB_BAKE_MESH);
}
static inline bool fluid_is_bake_particle(FluidJob *job)
{
  return STREQ(job->type, FLUID_JOB_BAKE_PARTICLES);
}
static inline bool fluid_is_bake_guiding(FluidJob *job)
{
  return STREQ(job->type, FLUID_JOB_BAKE_GUIDES);
}
static inline bool fluid_is_free_all(FluidJob *job)
{
  return STREQ(job->type, FLUID_JOB_FREE_ALL);
}
static inline bool fluid_is_free_data(FluidJob *job)
{
  return STREQ(job->type, FLUID_JOB_FREE_DATA);
}
static inline bool fluid_is_free_noise(FluidJob *job)
{
  return STREQ(job->type, FLUID_JOB_FREE_NOISE);
}
static inline bool fluid_is_free_mesh(FluidJob *job)
{
  return STREQ(job->type, FLUID_JOB_FREE_MESH);
}
static inline bool fluid_is_free_particles(FluidJob *job)
{
  return STREQ(job->type, FLUID_JOB_FREE_PARTICLES);
}
static inline bool fluid_is_free_guiding(FluidJob *job)
{
  return STREQ(job->type, FLUID_JOB_FREE_GUIDES);
}

static bool fluid_initjob(
    bContext *C, FluidJob *job, wmOperator *op, char *error_msg, int error_size)
{
  FluidModifierData *fmd = nullptr;
  FluidDomainSettings *fds;
  Object *ob = blender::ed::object::context_active_object(C);

  fmd = (FluidModifierData *)BKE_modifiers_findby_type(ob, eModifierType_Fluid);
  if (!fmd) {
    BLI_strncpy_utf8(error_msg, N_("Bake failed: no Fluid modifier found"), error_size);
    return false;
  }
  fds = fmd->domain;
  if (!fds) {
    BLI_strncpy_utf8(error_msg, N_("Bake failed: invalid domain"), error_size);
    return false;
  }

  job->bmain = CTX_data_main(C);
  job->scene = CTX_data_scene(C);
  job->depsgraph = CTX_data_depsgraph_pointer(C);
  job->ob = ob;
  job->fmd = fmd;
  job->type = op->type->idname;
  job->name = op->type->name;

  return true;
}

static bool fluid_validatepaths(FluidJob *job, ReportList *reports)
{
  FluidDomainSettings *fds = job->fmd->domain;
  char temp_dir[FILE_MAX];
  temp_dir[0] = '\0';
  bool is_relative = false;

  const char *relbase = BKE_modifier_path_relbase(job->bmain, job->ob);

  /* We do not accept empty paths, they can end in random places silently, see #51176. */
  if (fds->cache_directory[0] == '\0') {
    char cache_name[64];
    BKE_fluid_cache_new_name_for_current_session(sizeof(cache_name), cache_name);
    BKE_modifier_path_init(fds->cache_directory, sizeof(fds->cache_directory), cache_name);
    BKE_reportf(reports,
                RPT_WARNING,
                "Fluid: Empty cache path, reset to default '%s'",
                fds->cache_directory);
  }

  BLI_strncpy(temp_dir, fds->cache_directory, FILE_MAXDIR);
  is_relative = BLI_path_abs(temp_dir, relbase);

  /* Ensure whole path exists */
  const bool dir_exists = BLI_dir_create_recursive(temp_dir);

  /* We change path to some presumably valid default value, but do not allow bake process to
   * continue, this gives user chance to set manually another path. */
  if (!dir_exists) {
    char cache_name[64];
    BKE_fluid_cache_new_name_for_current_session(sizeof(cache_name), cache_name);
    BKE_modifier_path_init(fds->cache_directory, sizeof(fds->cache_directory), cache_name);

    BKE_reportf(reports,
                RPT_ERROR,
                "Fluid: Could not create cache directory '%s', reset to default '%s'",
                temp_dir,
                fds->cache_directory);

    /* Ensure whole path exists and is writable. */
    if (!BLI_dir_create_recursive(temp_dir)) {
      BKE_reportf(reports,
                  RPT_ERROR,
                  "Fluid: Could not use default cache directory '%s', "
                  "please define a valid cache path manually",
                  temp_dir);
      return false;
    }
    /* Copy final dir back into domain settings */
    BLI_strncpy(fds->cache_directory, temp_dir, FILE_MAXDIR);

    return false;
  }

  /* Change path back to is original state (ie relative or absolute). */
  if (is_relative) {
    BLI_path_rel(temp_dir, relbase);
  }

  /* Copy final dir back into domain settings */
  BLI_strncpy(fds->cache_directory, temp_dir, FILE_MAXDIR);
  return true;
}

static void fluid_bake_free(void *customdata)
{
  FluidJob *job = static_cast<FluidJob *>(customdata);
  MEM_freeN(job);
}

static void fluid_bake_sequence(FluidJob *job)
{
  FluidDomainSettings *fds = job->fmd->domain;
  Scene *scene = job->scene;
  int frame = 1, orig_frame;
  int frames;
  int *pause_frame = nullptr;
  bool is_first_frame;

  frames = fds->cache_frame_end - fds->cache_frame_start + 1;

  if (frames <= 0) {
    STRNCPY_UTF8(fds->error, N_("No frames to bake"));
    return;
  }

  /* Show progress bar. */
  if (job->do_update) {
    *(job->do_update) = true;
  }

  /* Get current pause frame (pointer) - depending on bake type. */
  pause_frame = job->pause_frame;

  /* Set frame to start point (depending on current pause frame value). */
  is_first_frame = ((*pause_frame) == 0);
  frame = is_first_frame ? fds->cache_frame_start : (*pause_frame);

  /* Save orig frame and update scene frame. */
  orig_frame = scene->r.cfra;
  scene->r.cfra = frame;

  /* Loop through selected frames. */
  for (; frame <= fds->cache_frame_end; frame++) {
    const float progress = (frame - fds->cache_frame_start) / float(frames);

    /* Keep track of pause frame - needed to init future loop. */
    (*pause_frame) = frame;

    /* If user requested stop, quit baking. */
    if (G.is_break) {
      job->success = 0;
      return;
    }

    /* Update progress bar. */
    if (job->do_update) {
      *(job->do_update) = true;
    }
    if (job->progress) {
      *(job->progress) = progress;
    }

    scene->r.cfra = frame;

    /* Update animation system. */
    ED_update_for_newframe(job->bmain, job->depsgraph);

    /* If user requested stop, quit baking. */
    if (G.is_break) {
      job->success = 0;
      return;
    }
  }

  /* Restore frame position that we were on before bake. */
  scene->r.cfra = orig_frame;
}

static void fluid_bake_endjob(void *customdata)
{
  FluidJob *job = static_cast<FluidJob *>(customdata);
  FluidDomainSettings *fds = job->fmd->domain;

  if (fluid_is_bake_noise(job) || fluid_is_bake_all(job)) {
    fds->cache_flag &= ~FLUID_DOMAIN_BAKING_NOISE;
    fds->cache_flag |= FLUID_DOMAIN_BAKED_NOISE;
    fds->cache_flag &= ~FLUID_DOMAIN_OUTDATED_NOISE;
  }
  if (fluid_is_bake_mesh(job) || fluid_is_bake_all(job)) {
    fds->cache_flag &= ~FLUID_DOMAIN_BAKING_MESH;
    fds->cache_flag |= FLUID_DOMAIN_BAKED_MESH;
    fds->cache_flag &= ~FLUID_DOMAIN_OUTDATED_MESH;
  }
  if (fluid_is_bake_particle(job) || fluid_is_bake_all(job)) {
    fds->cache_flag &= ~FLUID_DOMAIN_BAKING_PARTICLES;
    fds->cache_flag |= FLUID_DOMAIN_BAKED_PARTICLES;
    fds->cache_flag &= ~FLUID_DOMAIN_OUTDATED_PARTICLES;
  }
  if (fluid_is_bake_guiding(job) || fluid_is_bake_all(job)) {
    fds->cache_flag &= ~FLUID_DOMAIN_BAKING_GUIDE;
    fds->cache_flag |= FLUID_DOMAIN_BAKED_GUIDE;
    fds->cache_flag &= ~FLUID_DOMAIN_OUTDATED_GUIDE;
  }
  if (fluid_is_bake_data(job) || fluid_is_bake_all(job)) {
    fds->cache_flag &= ~FLUID_DOMAIN_BAKING_DATA;
    fds->cache_flag |= FLUID_DOMAIN_BAKED_DATA;
    fds->cache_flag &= ~FLUID_DOMAIN_OUTDATED_DATA;
  }
  DEG_id_tag_update(&job->ob->id, ID_RECALC_GEOMETRY);

  G.is_rendering = false;
  WM_locked_interface_set(static_cast<wmWindowManager *>(G_MAIN->wm.first), false);

  /* Bake was successful:
   * Report for ended bake and how long it took. */
  if (job->success) {
    /* Show bake info. */
    WM_global_reportf(RPT_INFO,
                      "Fluid: %s complete (%.2fs)",
                      CTX_RPT_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, job->name),
                      BLI_time_now_seconds() - job->start);
  }
  else {
    if (fds->error[0] != '\0') {
      WM_global_reportf(RPT_ERROR,
                        "Fluid: %s failed at frame %d: %s",
                        CTX_RPT_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, job->name),
                        *job->pause_frame,
                        fds->error);
    }
    else { /* User canceled the bake. */
      WM_global_reportf(RPT_WARNING,
                        "Fluid: %s canceled at frame %d!",
                        CTX_RPT_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, job->name),
                        *job->pause_frame);
    }
  }
}

static void fluid_bake_startjob(void *customdata, wmJobWorkerStatus *worker_status)
{
  FluidJob *job = static_cast<FluidJob *>(customdata);
  FluidDomainSettings *fds = job->fmd->domain;

  char temp_dir[FILE_MAX];
  const char *relbase = BKE_modifier_path_relbase_from_global(job->ob);

  job->stop = &worker_status->stop;
  job->do_update = &worker_status->do_update;
  job->progress = &worker_status->progress;
  job->start = BLI_time_now_seconds();
  job->success = 1;

  G.is_break = false;
  G.is_rendering = true;
  BKE_spacedata_draw_locks(REGION_DRAW_LOCK_BAKING);

  if (fluid_is_bake_noise(job) || fluid_is_bake_all(job)) {
    BLI_path_join(temp_dir, sizeof(temp_dir), fds->cache_directory, FLUID_DOMAIN_DIR_NOISE);
    BLI_path_abs(temp_dir, relbase);
    BLI_dir_create_recursive(temp_dir); /* Create 'noise' subdir if it does not exist already */
    fds->cache_flag &= ~(FLUID_DOMAIN_BAKED_NOISE | FLUID_DOMAIN_OUTDATED_NOISE);
    fds->cache_flag |= FLUID_DOMAIN_BAKING_NOISE;
    job->pause_frame = &fds->cache_frame_pause_noise;
  }
  if (fluid_is_bake_mesh(job) || fluid_is_bake_all(job)) {
    BLI_path_join(temp_dir, sizeof(temp_dir), fds->cache_directory, FLUID_DOMAIN_DIR_MESH);
    BLI_path_abs(temp_dir, relbase);
    BLI_dir_create_recursive(temp_dir); /* Create 'mesh' subdir if it does not exist already */
    fds->cache_flag &= ~(FLUID_DOMAIN_BAKED_MESH | FLUID_DOMAIN_OUTDATED_MESH);
    fds->cache_flag |= FLUID_DOMAIN_BAKING_MESH;
    job->pause_frame = &fds->cache_frame_pause_mesh;
  }
  if (fluid_is_bake_particle(job) || fluid_is_bake_all(job)) {
    BLI_path_join(temp_dir, sizeof(temp_dir), fds->cache_directory, FLUID_DOMAIN_DIR_PARTICLES);
    BLI_path_abs(temp_dir, relbase);

    /* Create 'particles' subdir if it does not exist already */
    BLI_dir_create_recursive(temp_dir);

    fds->cache_flag &= ~(FLUID_DOMAIN_BAKED_PARTICLES | FLUID_DOMAIN_OUTDATED_PARTICLES);
    fds->cache_flag |= FLUID_DOMAIN_BAKING_PARTICLES;
    job->pause_frame = &fds->cache_frame_pause_particles;
  }
  if (fluid_is_bake_guiding(job) || fluid_is_bake_all(job)) {
    BLI_path_join(temp_dir, sizeof(temp_dir), fds->cache_directory, FLUID_DOMAIN_DIR_GUIDE);
    BLI_path_abs(temp_dir, relbase);
    BLI_dir_create_recursive(temp_dir); /* Create 'guiding' subdir if it does not exist already */
    fds->cache_flag &= ~(FLUID_DOMAIN_BAKED_GUIDE | FLUID_DOMAIN_OUTDATED_GUIDE);
    fds->cache_flag |= FLUID_DOMAIN_BAKING_GUIDE;
    job->pause_frame = &fds->cache_frame_pause_guide;
  }
  if (fluid_is_bake_data(job) || fluid_is_bake_all(job)) {
    BLI_path_join(temp_dir, sizeof(temp_dir), fds->cache_directory, FLUID_DOMAIN_DIR_CONFIG);
    BLI_path_abs(temp_dir, relbase);
    BLI_dir_create_recursive(temp_dir); /* Create 'config' subdir if it does not exist already */

    BLI_path_join(temp_dir, sizeof(temp_dir), fds->cache_directory, FLUID_DOMAIN_DIR_DATA);
    BLI_path_abs(temp_dir, relbase);
    BLI_dir_create_recursive(temp_dir); /* Create 'data' subdir if it does not exist already */
    fds->cache_flag &= ~(FLUID_DOMAIN_BAKED_DATA | FLUID_DOMAIN_OUTDATED_DATA);
    fds->cache_flag |= FLUID_DOMAIN_BAKING_DATA;
    job->pause_frame = &fds->cache_frame_pause_data;

    if (fds->flags & FLUID_DOMAIN_EXPORT_MANTA_SCRIPT) {
      BLI_path_join(temp_dir, sizeof(temp_dir), fds->cache_directory, FLUID_DOMAIN_DIR_SCRIPT);
      BLI_path_abs(temp_dir, relbase);
      BLI_dir_create_recursive(temp_dir); /* Create 'script' subdir if it does not exist already */
    }
  }
  DEG_id_tag_update(&job->ob->id, ID_RECALC_GEOMETRY);

  fluid_bake_sequence(job);

  worker_status->do_update = true;
  worker_status->stop = false;
}

static void fluid_free_endjob(void *customdata)
{
  FluidJob *job = static_cast<FluidJob *>(customdata);
  FluidDomainSettings *fds = job->fmd->domain;

  G.is_rendering = false;
  WM_locked_interface_set(static_cast<wmWindowManager *>(G_MAIN->wm.first), false);

  /* Reflect the now empty cache in the viewport too. */
  DEG_id_tag_update(&job->ob->id, ID_RECALC_GEOMETRY);

  /* Free was successful:
   *  Report for ended free job and how long it took */
  if (job->success) {
    /* Show free job info */
    WM_global_reportf(RPT_INFO,
                      "Fluid: %s complete (%.2fs)",
                      CTX_RPT_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, job->name),
                      BLI_time_now_seconds() - job->start);
  }
  else {
    if (fds->error[0] != '\0') {
      WM_global_reportf(RPT_ERROR,
                        "Fluid: %s failed at frame %d: %s",
                        CTX_RPT_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, job->name),
                        *job->pause_frame,
                        fds->error);
    }
    else { /* User canceled the free job */
      WM_global_reportf(RPT_WARNING,
                        "Fluid: %s canceled at frame %d!",
                        CTX_RPT_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, job->name),
                        *job->pause_frame);
    }
  }
}

static void fluid_free_startjob(void *customdata, wmJobWorkerStatus *worker_status)
{
  FluidJob *job = static_cast<FluidJob *>(customdata);
  FluidDomainSettings *fds = job->fmd->domain;

  job->stop = &worker_status->stop;
  job->do_update = &worker_status->do_update;
  job->progress = &worker_status->progress;
  job->start = BLI_time_now_seconds();
  job->success = 1;

  G.is_break = false;
  G.is_rendering = true;
  BKE_spacedata_draw_locks(REGION_DRAW_LOCK_BAKING);

  int cache_map = 0;

  if (fluid_is_free_data(job) || fluid_is_free_all(job)) {
    cache_map |= (FLUID_DOMAIN_OUTDATED_DATA | FLUID_DOMAIN_OUTDATED_NOISE |
                  FLUID_DOMAIN_OUTDATED_MESH | FLUID_DOMAIN_OUTDATED_PARTICLES);
  }
  if (fluid_is_free_noise(job) || fluid_is_free_all(job)) {
    cache_map |= FLUID_DOMAIN_OUTDATED_NOISE;
  }
  if (fluid_is_free_mesh(job) || fluid_is_free_all(job)) {
    cache_map |= FLUID_DOMAIN_OUTDATED_MESH;
  }
  if (fluid_is_free_particles(job) || fluid_is_free_all(job)) {
    cache_map |= FLUID_DOMAIN_OUTDATED_PARTICLES;
  }
  if (fluid_is_free_guiding(job) || fluid_is_free_all(job)) {
    cache_map |= (FLUID_DOMAIN_OUTDATED_DATA | FLUID_DOMAIN_OUTDATED_NOISE |
                  FLUID_DOMAIN_OUTDATED_MESH | FLUID_DOMAIN_OUTDATED_PARTICLES |
                  FLUID_DOMAIN_OUTDATED_GUIDE);
  }
#ifdef WITH_FLUID
  BKE_fluid_cache_free(fds, job->ob, cache_map);
#else
  UNUSED_VARS(fds);
  UNUSED_VARS(cache_map);
#endif

  worker_status->do_update = true;
  worker_status->stop = false;

  /* Update scene so that viewport shows freed up scene */
  ED_update_for_newframe(job->bmain, job->depsgraph);
}

/***************************** Operators ******************************/

static wmOperatorStatus fluid_bake_exec(bContext *C, wmOperator *op)
{
  FluidJob *job = MEM_mallocN<FluidJob>("FluidJob");
  char error_msg[256] = "\0";

  if (!fluid_initjob(C, job, op, error_msg, sizeof(error_msg))) {
    if (error_msg[0]) {
      BKE_report(op->reports, RPT_ERROR, error_msg);
    }
    fluid_bake_free(job);
    return OPERATOR_CANCELLED;
  }
  if (!fluid_validatepaths(job, op->reports)) {
    fluid_bake_free(job);
    return OPERATOR_CANCELLED;
  }
  WM_report_banners_cancel(job->bmain);

  wmJobWorkerStatus worker_status = {};
  fluid_bake_startjob(job, &worker_status);
  fluid_bake_endjob(job);
  fluid_bake_free(job);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus fluid_bake_invoke(bContext *C, wmOperator *op, const wmEvent * /*_event*/)
{
  Scene *scene = CTX_data_scene(C);
  FluidJob *job = MEM_mallocN<FluidJob>("FluidJob");
  char error_msg[256] = "\0";

  if (!fluid_initjob(C, job, op, error_msg, sizeof(error_msg))) {
    if (error_msg[0]) {
      BKE_report(op->reports, RPT_ERROR, error_msg);
    }
    fluid_bake_free(job);
    return OPERATOR_CANCELLED;
  }

  if (!fluid_validatepaths(job, op->reports)) {
    fluid_bake_free(job);
    return OPERATOR_CANCELLED;
  }

  /* Clear existing banners so that the upcoming progress bar from this job has more room. */
  WM_report_banners_cancel(job->bmain);

  wmJob *wm_job = WM_jobs_get(CTX_wm_manager(C),
                              CTX_wm_window(C),
                              scene,
                              "Baking fluid...",
                              WM_JOB_PROGRESS,
                              WM_JOB_TYPE_OBJECT_SIM_FLUID);

  WM_jobs_customdata_set(wm_job, job, fluid_bake_free);
  WM_jobs_timer(wm_job, 0.01, NC_OBJECT | ND_MODIFIER, NC_OBJECT | ND_MODIFIER);
  WM_jobs_callbacks(wm_job, fluid_bake_startjob, nullptr, nullptr, fluid_bake_endjob);

  WM_locked_interface_set_with_flags(CTX_wm_manager(C), REGION_DRAW_LOCK_BAKING);

  WM_jobs_start(CTX_wm_manager(C), wm_job);
  WM_event_add_modal_handler(C, op);

  return OPERATOR_RUNNING_MODAL;
}

static wmOperatorStatus fluid_bake_modal(bContext *C, wmOperator * /*op*/, const wmEvent *event)
{
  /* no running blender, remove handler and pass through */
  if (0 == WM_jobs_test(CTX_wm_manager(C), CTX_data_scene(C), WM_JOB_TYPE_OBJECT_SIM_FLUID)) {
    return OPERATOR_FINISHED | OPERATOR_PASS_THROUGH;
  }

  switch (event->type) {
    case EVT_ESCKEY:
      return OPERATOR_RUNNING_MODAL;
    default: {
      break;
    }
  }
  return OPERATOR_PASS_THROUGH;
}

static wmOperatorStatus fluid_free_exec(bContext *C, wmOperator *op)
{
  FluidModifierData *fmd = nullptr;
  FluidDomainSettings *fds;
  Object *ob = blender::ed::object::context_active_object(C);
  Scene *scene = CTX_data_scene(C);

  /*
   * Get modifier data
   */
  fmd = (FluidModifierData *)BKE_modifiers_findby_type(ob, eModifierType_Fluid);
  if (!fmd) {
    BKE_report(op->reports, RPT_ERROR, "Bake free failed: no Fluid modifier found");
    return OPERATOR_CANCELLED;
  }
  fds = fmd->domain;
  if (!fds) {
    BKE_report(op->reports, RPT_ERROR, "Bake free failed: invalid domain");
    return OPERATOR_CANCELLED;
  }

  /* Cannot free data if other bakes currently working */
  if (fmd->domain->cache_flag & (FLUID_DOMAIN_BAKING_DATA | FLUID_DOMAIN_BAKING_NOISE |
                                 FLUID_DOMAIN_BAKING_MESH | FLUID_DOMAIN_BAKING_PARTICLES))
  {
    BKE_report(op->reports, RPT_ERROR, "Bake free failed: pending bake jobs found");
    return OPERATOR_CANCELLED;
  }

  FluidJob *job = MEM_mallocN<FluidJob>("FluidJob");
  job->bmain = CTX_data_main(C);
  job->scene = scene;
  job->depsgraph = CTX_data_depsgraph_pointer(C);
  job->ob = ob;
  job->fmd = fmd;
  job->type = op->type->idname;
  job->name = op->type->name;

  if (!fluid_validatepaths(job, op->reports)) {
    fluid_bake_free(job);
    return OPERATOR_CANCELLED;
  }

  /* Clear existing banners so that the upcoming progress bar from this job has more room. */
  WM_report_banners_cancel(job->bmain);

  wmJob *wm_job = WM_jobs_get(CTX_wm_manager(C),
                              CTX_wm_window(C),
                              scene,
                              "Freeing fluid...",
                              WM_JOB_PROGRESS,
                              WM_JOB_TYPE_OBJECT_SIM_FLUID);

  WM_jobs_customdata_set(wm_job, job, fluid_bake_free);
  WM_jobs_timer(wm_job, 0.01, NC_OBJECT | ND_MODIFIER, NC_OBJECT | ND_MODIFIER);
  WM_jobs_callbacks(wm_job, fluid_free_startjob, nullptr, nullptr, fluid_free_endjob);

  WM_locked_interface_set_with_flags(CTX_wm_manager(C), REGION_DRAW_LOCK_BAKING);

  /* Free Fluid Geometry. */
  WM_jobs_start(CTX_wm_manager(C), wm_job);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus fluid_pause_exec(bContext *C, wmOperator *op)
{
  FluidModifierData *fmd = nullptr;
  FluidDomainSettings *fds;
  Object *ob = blender::ed::object::context_active_object(C);

  /*
   * Get modifier data
   */
  fmd = (FluidModifierData *)BKE_modifiers_findby_type(ob, eModifierType_Fluid);
  if (!fmd) {
    BKE_report(op->reports, RPT_ERROR, "Bake free failed: no Fluid modifier found");
    return OPERATOR_CANCELLED;
  }
  fds = fmd->domain;
  if (!fds) {
    BKE_report(op->reports, RPT_ERROR, "Bake free failed: invalid domain");
    return OPERATOR_CANCELLED;
  }

  G.is_break = true;

  return OPERATOR_FINISHED;
}

void FLUID_OT_bake_all(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Bake All";
  ot->description = "Bake Entire Fluid Simulation";
  ot->idname = FLUID_JOB_BAKE_ALL;

  /* API callbacks. */
  ot->exec = fluid_bake_exec;
  ot->invoke = fluid_bake_invoke;
  ot->modal = fluid_bake_modal;
  ot->poll = ED_operator_object_active_editable;
}

void FLUID_OT_free_all(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Free All";
  ot->description = "Free Entire Fluid Simulation";
  ot->idname = FLUID_JOB_FREE_ALL;

  /* API callbacks. */
  ot->exec = fluid_free_exec;
  ot->poll = ED_operator_object_active_editable;
}

void FLUID_OT_bake_data(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Bake Data";
  ot->description = "Bake Fluid Data";
  ot->idname = FLUID_JOB_BAKE_DATA;

  /* API callbacks. */
  ot->exec = fluid_bake_exec;
  ot->invoke = fluid_bake_invoke;
  ot->modal = fluid_bake_modal;
  ot->poll = ED_operator_object_active_editable;
}

void FLUID_OT_free_data(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Free Data";
  ot->description = "Free Fluid Data";
  ot->idname = FLUID_JOB_FREE_DATA;

  /* API callbacks. */
  ot->exec = fluid_free_exec;
  ot->poll = ED_operator_object_active_editable;
}

void FLUID_OT_bake_noise(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Bake Noise";
  ot->description = "Bake Fluid Noise";
  ot->idname = FLUID_JOB_BAKE_NOISE;

  /* API callbacks. */
  ot->exec = fluid_bake_exec;
  ot->invoke = fluid_bake_invoke;
  ot->modal = fluid_bake_modal;
  ot->poll = ED_operator_object_active_editable;
}

void FLUID_OT_free_noise(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Free Noise";
  ot->description = "Free Fluid Noise";
  ot->idname = FLUID_JOB_FREE_NOISE;

  /* API callbacks. */
  ot->exec = fluid_free_exec;
  ot->poll = ED_operator_object_active_editable;
}

void FLUID_OT_bake_mesh(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Bake Mesh";
  ot->description = "Bake Fluid Mesh";
  ot->idname = FLUID_JOB_BAKE_MESH;

  /* API callbacks. */
  ot->exec = fluid_bake_exec;
  ot->invoke = fluid_bake_invoke;
  ot->modal = fluid_bake_modal;
  ot->poll = ED_operator_object_active_editable;
}

void FLUID_OT_free_mesh(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Free Mesh";
  ot->description = "Free Fluid Mesh";
  ot->idname = FLUID_JOB_FREE_MESH;

  /* API callbacks. */
  ot->exec = fluid_free_exec;
  ot->poll = ED_operator_object_active_editable;
}

void FLUID_OT_bake_particles(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Bake Particles";
  ot->description = "Bake Fluid Particles";
  ot->idname = FLUID_JOB_BAKE_PARTICLES;

  /* API callbacks. */
  ot->exec = fluid_bake_exec;
  ot->invoke = fluid_bake_invoke;
  ot->modal = fluid_bake_modal;
  ot->poll = ED_operator_object_active_editable;
}

void FLUID_OT_free_particles(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Free Particles";
  ot->description = "Free Fluid Particles";
  ot->idname = FLUID_JOB_FREE_PARTICLES;

  /* API callbacks. */
  ot->exec = fluid_free_exec;
  ot->poll = ED_operator_object_active_editable;
}

void FLUID_OT_bake_guides(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Bake Guides";
  ot->description = "Bake Fluid Guiding";
  ot->idname = FLUID_JOB_BAKE_GUIDES;

  /* API callbacks. */
  ot->exec = fluid_bake_exec;
  ot->invoke = fluid_bake_invoke;
  ot->modal = fluid_bake_modal;
  ot->poll = ED_operator_object_active_editable;
}

void FLUID_OT_free_guides(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Free Guides";
  ot->description = "Free Fluid Guiding";
  ot->idname = FLUID_JOB_FREE_GUIDES;

  /* API callbacks. */
  ot->exec = fluid_free_exec;
  ot->poll = ED_operator_object_active_editable;
}

void FLUID_OT_pause_bake(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Pause Bake";
  ot->description = "Pause Bake";
  ot->idname = FLUID_JOB_BAKE_PAUSE;

  /* API callbacks. */
  ot->exec = fluid_pause_exec;
  ot->poll = ED_operator_object_active_editable;
}
