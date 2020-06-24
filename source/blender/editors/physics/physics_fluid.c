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
 * The Original Code is Copyright (C) Blender Foundation
 * All rights reserved.
 */

/** \file
 * \ingroup edphys
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "MEM_guardedalloc.h"

/* types */
#include "DNA_action_types.h"
#include "DNA_object_types.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_path_util.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "BKE_context.h"
#include "BKE_customdata.h"
#include "BKE_fluid.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_screen.h"

#include "DEG_depsgraph.h"

#include "ED_object.h"
#include "ED_screen.h"
#include "PIL_time.h"

#include "WM_api.h"
#include "WM_types.h"

#include "manta_fluid_API.h"
#include "physics_intern.h"  // own include

#include "DNA_fluid_types.h"
#include "DNA_mesh_types.h"
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

typedef struct FluidJob {
  /* from wmJob */
  void *owner;
  short *stop, *do_update;
  float *progress;
  const char *type;
  const char *name;

  struct Main *bmain;
  Scene *scene;
  Depsgraph *depsgraph;
  Object *ob;

  FluidModifierData *mmd;

  int success;
  double start;

  int *pause_frame;
} FluidJob;

static inline bool fluid_is_bake_all(FluidJob *job)
{
  return (STREQ(job->type, FLUID_JOB_BAKE_ALL));
}
static inline bool fluid_is_bake_data(FluidJob *job)
{
  return (STREQ(job->type, FLUID_JOB_BAKE_DATA));
}
static inline bool fluid_is_bake_noise(FluidJob *job)
{
  return (STREQ(job->type, FLUID_JOB_BAKE_NOISE));
}
static inline bool fluid_is_bake_mesh(FluidJob *job)
{
  return (STREQ(job->type, FLUID_JOB_BAKE_MESH));
}
static inline bool fluid_is_bake_particle(FluidJob *job)
{
  return (STREQ(job->type, FLUID_JOB_BAKE_PARTICLES));
}
static inline bool fluid_is_bake_guiding(FluidJob *job)
{
  return (STREQ(job->type, FLUID_JOB_BAKE_GUIDES));
}
static inline bool fluid_is_free_all(FluidJob *job)
{
  return (STREQ(job->type, FLUID_JOB_FREE_ALL));
}
static inline bool fluid_is_free_data(FluidJob *job)
{
  return (STREQ(job->type, FLUID_JOB_FREE_DATA));
}
static inline bool fluid_is_free_noise(FluidJob *job)
{
  return (STREQ(job->type, FLUID_JOB_FREE_NOISE));
}
static inline bool fluid_is_free_mesh(FluidJob *job)
{
  return (STREQ(job->type, FLUID_JOB_FREE_MESH));
}
static inline bool fluid_is_free_particles(FluidJob *job)
{
  return (STREQ(job->type, FLUID_JOB_FREE_PARTICLES));
}
static inline bool fluid_is_free_guiding(FluidJob *job)
{
  return (STREQ(job->type, FLUID_JOB_FREE_GUIDES));
}

static bool fluid_initjob(
    bContext *C, FluidJob *job, wmOperator *op, char *error_msg, int error_size)
{
  FluidModifierData *mmd = NULL;
  FluidDomainSettings *mds;
  Object *ob = ED_object_active_context(C);

  mmd = (FluidModifierData *)BKE_modifiers_findby_type(ob, eModifierType_Fluid);
  if (!mmd) {
    BLI_strncpy(error_msg, N_("Bake failed: no Fluid modifier found"), error_size);
    return false;
  }
  mds = mmd->domain;
  if (!mds) {
    BLI_strncpy(error_msg, N_("Bake failed: invalid domain"), error_size);
    return false;
  }

  job->bmain = CTX_data_main(C);
  job->scene = CTX_data_scene(C);
  job->depsgraph = CTX_data_depsgraph_pointer(C);
  job->ob = ob;
  job->mmd = mmd;
  job->type = op->type->idname;
  job->name = op->type->name;

  return true;
}

static bool fluid_validatepaths(FluidJob *job, ReportList *reports)
{
  FluidDomainSettings *mds = job->mmd->domain;
  char temp_dir[FILE_MAX];
  temp_dir[0] = '\0';
  bool is_relative = false;

  const char *relbase = BKE_modifier_path_relbase(job->bmain, job->ob);

  /* We do not accept empty paths, they can end in random places silently, see T51176. */
  if (mds->cache_directory[0] == '\0') {
    char cache_name[64];
    BKE_fluid_cache_new_name_for_current_session(sizeof(cache_name), cache_name);
    BKE_modifier_path_init(mds->cache_directory, sizeof(mds->cache_directory), cache_name);
    BKE_reportf(reports,
                RPT_WARNING,
                "Fluid: Empty cache path, reset to default '%s'",
                mds->cache_directory);
  }

  BLI_strncpy(temp_dir, mds->cache_directory, FILE_MAXDIR);
  is_relative = BLI_path_abs(temp_dir, relbase);

  /* Ensure whole path exists */
  const bool dir_exists = BLI_dir_create_recursive(temp_dir);

  /* We change path to some presumably valid default value, but do not allow bake process to
   * continue, this gives user chance to set manually another path. */
  if (!dir_exists) {
    char cache_name[64];
    BKE_fluid_cache_new_name_for_current_session(sizeof(cache_name), cache_name);
    BKE_modifier_path_init(mds->cache_directory, sizeof(mds->cache_directory), cache_name);

    BKE_reportf(reports,
                RPT_ERROR,
                "Fluid: Could not create cache directory '%s', reset to default '%s'",
                temp_dir,
                mds->cache_directory);

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
    BLI_strncpy(mds->cache_directory, temp_dir, FILE_MAXDIR);

    return false;
  }

  /* Change path back to is original state (ie relative or absolute). */
  if (is_relative) {
    BLI_path_rel(temp_dir, relbase);
  }

  /* Copy final dir back into domain settings */
  BLI_strncpy(mds->cache_directory, temp_dir, FILE_MAXDIR);
  return true;
}

static void fluid_bake_free(void *customdata)
{
  FluidJob *job = customdata;
  MEM_freeN(job);
}

static void fluid_bake_sequence(FluidJob *job)
{
  FluidDomainSettings *mds = job->mmd->domain;
  Scene *scene = job->scene;
  int frame = 1, orig_frame;
  int frames;
  int *pause_frame = NULL;
  bool is_first_frame;

  frames = mds->cache_frame_end - mds->cache_frame_start + 1;

  if (frames <= 0) {
    BLI_strncpy(mds->error, N_("No frames to bake"), sizeof(mds->error));
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
  frame = is_first_frame ? mds->cache_frame_start : (*pause_frame);

  /* Save orig frame and update scene frame. */
  orig_frame = CFRA;
  CFRA = frame;

  /* Loop through selected frames. */
  for (; frame <= mds->cache_frame_end; frame++) {
    const float progress = (frame - mds->cache_frame_start) / (float)frames;

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

    CFRA = frame;

    /* Update animation system. */
    ED_update_for_newframe(job->bmain, job->depsgraph);

    /* If user requested stop, quit baking. */
    if (G.is_break) {
      job->success = 0;
      return;
    }
  }

  /* Restore frame position that we were on before bake. */
  CFRA = orig_frame;
}

static void fluid_bake_endjob(void *customdata)
{
  FluidJob *job = customdata;
  FluidDomainSettings *mds = job->mmd->domain;

  if (fluid_is_bake_noise(job) || fluid_is_bake_all(job)) {
    mds->cache_flag &= ~FLUID_DOMAIN_BAKING_NOISE;
    mds->cache_flag |= FLUID_DOMAIN_BAKED_NOISE;
    mds->cache_flag &= ~FLUID_DOMAIN_OUTDATED_NOISE;
  }
  if (fluid_is_bake_mesh(job) || fluid_is_bake_all(job)) {
    mds->cache_flag &= ~FLUID_DOMAIN_BAKING_MESH;
    mds->cache_flag |= FLUID_DOMAIN_BAKED_MESH;
    mds->cache_flag &= ~FLUID_DOMAIN_OUTDATED_MESH;
  }
  if (fluid_is_bake_particle(job) || fluid_is_bake_all(job)) {
    mds->cache_flag &= ~FLUID_DOMAIN_BAKING_PARTICLES;
    mds->cache_flag |= FLUID_DOMAIN_BAKED_PARTICLES;
    mds->cache_flag &= ~FLUID_DOMAIN_OUTDATED_PARTICLES;
  }
  if (fluid_is_bake_guiding(job) || fluid_is_bake_all(job)) {
    mds->cache_flag &= ~FLUID_DOMAIN_BAKING_GUIDE;
    mds->cache_flag |= FLUID_DOMAIN_BAKED_GUIDE;
    mds->cache_flag &= ~FLUID_DOMAIN_OUTDATED_GUIDE;
  }
  if (fluid_is_bake_data(job) || fluid_is_bake_all(job)) {
    mds->cache_flag &= ~FLUID_DOMAIN_BAKING_DATA;
    mds->cache_flag |= FLUID_DOMAIN_BAKED_DATA;
    mds->cache_flag &= ~FLUID_DOMAIN_OUTDATED_DATA;
  }
  DEG_id_tag_update(&job->ob->id, ID_RECALC_GEOMETRY);

  G.is_rendering = false;
  BKE_spacedata_draw_locks(false);
  WM_set_locked_interface(G_MAIN->wm.first, false);

  /* Bake was successful:
   * Report for ended bake and how long it took. */
  if (job->success) {
    /* Show bake info. */
    WM_reportf(
        RPT_INFO, "Fluid: %s complete! (%.2f)", job->name, PIL_check_seconds_timer() - job->start);
  }
  else {
    if (mds->error[0] != '\0') {
      WM_reportf(RPT_ERROR, "Fluid: %s failed: %s", job->name, mds->error);
    }
    else { /* User canceled the bake. */
      WM_reportf(RPT_WARNING, "Fluid: %s canceled!", job->name);
    }
  }
}

static void fluid_bake_startjob(void *customdata, short *stop, short *do_update, float *progress)
{
  FluidJob *job = customdata;
  FluidDomainSettings *mds = job->mmd->domain;

  char temp_dir[FILE_MAX];
  const char *relbase = BKE_modifier_path_relbase_from_global(job->ob);

  job->stop = stop;
  job->do_update = do_update;
  job->progress = progress;
  job->start = PIL_check_seconds_timer();
  job->success = 1;

  G.is_break = false;
  G.is_rendering = true;
  BKE_spacedata_draw_locks(true);

  if (fluid_is_bake_noise(job) || fluid_is_bake_all(job)) {
    BLI_path_join(temp_dir, sizeof(temp_dir), mds->cache_directory, FLUID_DOMAIN_DIR_NOISE, NULL);
    BLI_path_abs(temp_dir, relbase);
    BLI_dir_create_recursive(temp_dir); /* Create 'noise' subdir if it does not exist already */
    mds->cache_flag &= ~(FLUID_DOMAIN_BAKED_NOISE | FLUID_DOMAIN_OUTDATED_NOISE);
    mds->cache_flag |= FLUID_DOMAIN_BAKING_NOISE;
    job->pause_frame = &mds->cache_frame_pause_noise;
  }
  if (fluid_is_bake_mesh(job) || fluid_is_bake_all(job)) {
    BLI_path_join(temp_dir, sizeof(temp_dir), mds->cache_directory, FLUID_DOMAIN_DIR_MESH, NULL);
    BLI_path_abs(temp_dir, relbase);
    BLI_dir_create_recursive(temp_dir); /* Create 'mesh' subdir if it does not exist already */
    mds->cache_flag &= ~(FLUID_DOMAIN_BAKED_MESH | FLUID_DOMAIN_OUTDATED_MESH);
    mds->cache_flag |= FLUID_DOMAIN_BAKING_MESH;
    job->pause_frame = &mds->cache_frame_pause_mesh;
  }
  if (fluid_is_bake_particle(job) || fluid_is_bake_all(job)) {
    BLI_path_join(
        temp_dir, sizeof(temp_dir), mds->cache_directory, FLUID_DOMAIN_DIR_PARTICLES, NULL);
    BLI_path_abs(temp_dir, relbase);
    BLI_dir_create_recursive(
        temp_dir); /* Create 'particles' subdir if it does not exist already */
    mds->cache_flag &= ~(FLUID_DOMAIN_BAKED_PARTICLES | FLUID_DOMAIN_OUTDATED_PARTICLES);
    mds->cache_flag |= FLUID_DOMAIN_BAKING_PARTICLES;
    job->pause_frame = &mds->cache_frame_pause_particles;
  }
  if (fluid_is_bake_guiding(job) || fluid_is_bake_all(job)) {
    BLI_path_join(temp_dir, sizeof(temp_dir), mds->cache_directory, FLUID_DOMAIN_DIR_GUIDE, NULL);
    BLI_path_abs(temp_dir, relbase);
    BLI_dir_create_recursive(temp_dir); /* Create 'guiding' subdir if it does not exist already */
    mds->cache_flag &= ~(FLUID_DOMAIN_BAKED_GUIDE | FLUID_DOMAIN_OUTDATED_GUIDE);
    mds->cache_flag |= FLUID_DOMAIN_BAKING_GUIDE;
    job->pause_frame = &mds->cache_frame_pause_guide;
  }
  if (fluid_is_bake_data(job) || fluid_is_bake_all(job)) {
    BLI_path_join(temp_dir, sizeof(temp_dir), mds->cache_directory, FLUID_DOMAIN_DIR_CONFIG, NULL);
    BLI_path_abs(temp_dir, relbase);
    BLI_dir_create_recursive(temp_dir); /* Create 'config' subdir if it does not exist already */

    BLI_path_join(temp_dir, sizeof(temp_dir), mds->cache_directory, FLUID_DOMAIN_DIR_DATA, NULL);
    BLI_path_abs(temp_dir, relbase);
    BLI_dir_create_recursive(temp_dir); /* Create 'data' subdir if it does not exist already */
    mds->cache_flag &= ~(FLUID_DOMAIN_BAKED_DATA | FLUID_DOMAIN_OUTDATED_DATA);
    mds->cache_flag |= FLUID_DOMAIN_BAKING_DATA;
    job->pause_frame = &mds->cache_frame_pause_data;

    if (mds->flags & FLUID_DOMAIN_EXPORT_MANTA_SCRIPT) {
      BLI_path_join(
          temp_dir, sizeof(temp_dir), mds->cache_directory, FLUID_DOMAIN_DIR_SCRIPT, NULL);
      BLI_path_abs(temp_dir, relbase);
      BLI_dir_create_recursive(temp_dir); /* Create 'script' subdir if it does not exist already */
    }
  }
  DEG_id_tag_update(&job->ob->id, ID_RECALC_GEOMETRY);

  fluid_bake_sequence(job);

  if (do_update) {
    *do_update = true;
  }
  if (stop) {
    *stop = 0;
  }
}

static void fluid_free_endjob(void *customdata)
{
  FluidJob *job = customdata;
  FluidDomainSettings *mds = job->mmd->domain;

  G.is_rendering = false;
  BKE_spacedata_draw_locks(false);
  WM_set_locked_interface(G_MAIN->wm.first, false);

  /* Reflect the now empty cache in the viewport too. */
  DEG_id_tag_update(&job->ob->id, ID_RECALC_GEOMETRY);

  /* Free was successful:
   *  Report for ended free job and how long it took */
  if (job->success) {
    /* Show free job info */
    WM_reportf(
        RPT_INFO, "Fluid: %s complete! (%.2f)", job->name, PIL_check_seconds_timer() - job->start);
  }
  else {
    if (mds->error[0] != '\0') {
      WM_reportf(RPT_ERROR, "Fluid: %s failed: %s", job->name, mds->error);
    }
    else { /* User canceled the free job */
      WM_reportf(RPT_WARNING, "Fluid: %s canceled!", job->name);
    }
  }
}

static void fluid_free_startjob(void *customdata, short *stop, short *do_update, float *progress)
{
  FluidJob *job = customdata;
  FluidDomainSettings *mds = job->mmd->domain;

  job->stop = stop;
  job->do_update = do_update;
  job->progress = progress;
  job->start = PIL_check_seconds_timer();
  job->success = 1;

  G.is_break = false;
  G.is_rendering = true;
  BKE_spacedata_draw_locks(true);

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
  BKE_fluid_cache_free(mds, job->ob, cache_map);
#else
  UNUSED_VARS(mds);
#endif

  *do_update = true;
  *stop = 0;

  /* Update scene so that viewport shows freed up scene */
  ED_update_for_newframe(job->bmain, job->depsgraph);
}

/***************************** Operators ******************************/

static int fluid_bake_exec(struct bContext *C, struct wmOperator *op)
{
  FluidJob *job = MEM_mallocN(sizeof(FluidJob), "FluidJob");
  char error_msg[256] = "\0";

  if (!fluid_initjob(C, job, op, error_msg, sizeof(error_msg))) {
    if (error_msg[0]) {
      BKE_report(op->reports, RPT_ERROR, error_msg);
    }
    fluid_bake_free(job);
    return OPERATOR_CANCELLED;
  }
  if (!fluid_validatepaths(job, op->reports)) {
    return OPERATOR_CANCELLED;
  }
  WM_report_banners_cancel(job->bmain);

  fluid_bake_startjob(job, NULL, NULL, NULL);
  fluid_bake_endjob(job);
  fluid_bake_free(job);

  return OPERATOR_FINISHED;
}

static int fluid_bake_invoke(struct bContext *C,
                             struct wmOperator *op,
                             const wmEvent *UNUSED(_event))
{
  Scene *scene = CTX_data_scene(C);
  FluidJob *job = MEM_mallocN(sizeof(FluidJob), "FluidJob");
  char error_msg[256] = "\0";

  if (!fluid_initjob(C, job, op, error_msg, sizeof(error_msg))) {
    if (error_msg[0]) {
      BKE_report(op->reports, RPT_ERROR, error_msg);
    }
    fluid_bake_free(job);
    return OPERATOR_CANCELLED;
  }

  if (!fluid_validatepaths(job, op->reports)) {
    return OPERATOR_CANCELLED;
  }

  /* Clear existing banners so that the upcoming progress bar from this job has more room. */
  WM_report_banners_cancel(job->bmain);

  wmJob *wm_job = WM_jobs_get(CTX_wm_manager(C),
                              CTX_wm_window(C),
                              scene,
                              "Fluid Bake",
                              WM_JOB_PROGRESS,
                              WM_JOB_TYPE_OBJECT_SIM_FLUID);

  WM_jobs_customdata_set(wm_job, job, fluid_bake_free);
  WM_jobs_timer(wm_job, 0.01, NC_OBJECT | ND_MODIFIER, NC_OBJECT | ND_MODIFIER);
  WM_jobs_callbacks(wm_job, fluid_bake_startjob, NULL, NULL, fluid_bake_endjob);

  WM_set_locked_interface(CTX_wm_manager(C), true);

  WM_jobs_start(CTX_wm_manager(C), wm_job);
  WM_event_add_modal_handler(C, op);

  return OPERATOR_RUNNING_MODAL;
}

static int fluid_bake_modal(bContext *C, wmOperator *UNUSED(op), const wmEvent *event)
{
  /* no running blender, remove handler and pass through */
  if (0 == WM_jobs_test(CTX_wm_manager(C), CTX_data_scene(C), WM_JOB_TYPE_OBJECT_SIM_FLUID)) {
    return OPERATOR_FINISHED | OPERATOR_PASS_THROUGH;
  }

  switch (event->type) {
    case EVT_ESCKEY:
      return OPERATOR_RUNNING_MODAL;
  }
  return OPERATOR_PASS_THROUGH;
}

static int fluid_free_exec(struct bContext *C, struct wmOperator *op)
{
  FluidModifierData *mmd = NULL;
  FluidDomainSettings *mds;
  Object *ob = ED_object_active_context(C);
  Scene *scene = CTX_data_scene(C);

  /*
   * Get modifier data
   */
  mmd = (FluidModifierData *)BKE_modifiers_findby_type(ob, eModifierType_Fluid);
  if (!mmd) {
    BKE_report(op->reports, RPT_ERROR, "Bake free failed: no Fluid modifier found");
    return OPERATOR_CANCELLED;
  }
  mds = mmd->domain;
  if (!mds) {
    BKE_report(op->reports, RPT_ERROR, "Bake free failed: invalid domain");
    return OPERATOR_CANCELLED;
  }

  /* Cannot free data if other bakes currently working */
  if (mmd->domain->cache_flag & (FLUID_DOMAIN_BAKING_DATA | FLUID_DOMAIN_BAKING_NOISE |
                                 FLUID_DOMAIN_BAKING_MESH | FLUID_DOMAIN_BAKING_PARTICLES)) {
    BKE_report(op->reports, RPT_ERROR, "Bake free failed: pending bake jobs found");
    return OPERATOR_CANCELLED;
  }

  FluidJob *job = MEM_mallocN(sizeof(FluidJob), "FluidJob");
  job->bmain = CTX_data_main(C);
  job->scene = scene;
  job->depsgraph = CTX_data_depsgraph_pointer(C);
  job->ob = ob;
  job->mmd = mmd;
  job->type = op->type->idname;
  job->name = op->type->name;

  if (!fluid_validatepaths(job, op->reports)) {
    return OPERATOR_CANCELLED;
  }

  /* Clear existing banners so that the upcoming progress bar from this job has more room. */
  WM_report_banners_cancel(job->bmain);

  wmJob *wm_job = WM_jobs_get(CTX_wm_manager(C),
                              CTX_wm_window(C),
                              scene,
                              "Fluid Free",
                              WM_JOB_PROGRESS,
                              WM_JOB_TYPE_OBJECT_SIM_FLUID);

  WM_jobs_customdata_set(wm_job, job, fluid_bake_free);
  WM_jobs_timer(wm_job, 0.01, NC_OBJECT | ND_MODIFIER, NC_OBJECT | ND_MODIFIER);
  WM_jobs_callbacks(wm_job, fluid_free_startjob, NULL, NULL, fluid_free_endjob);

  WM_set_locked_interface(CTX_wm_manager(C), true);

  /*  Free Fluid Geometry */
  WM_jobs_start(CTX_wm_manager(C), wm_job);

  return OPERATOR_FINISHED;
}

static int fluid_pause_exec(struct bContext *C, struct wmOperator *op)
{
  FluidModifierData *mmd = NULL;
  FluidDomainSettings *mds;
  Object *ob = ED_object_active_context(C);

  /*
   * Get modifier data
   */
  mmd = (FluidModifierData *)BKE_modifiers_findby_type(ob, eModifierType_Fluid);
  if (!mmd) {
    BKE_report(op->reports, RPT_ERROR, "Bake free failed: no Fluid modifier found");
    return OPERATOR_CANCELLED;
  }
  mds = mmd->domain;
  if (!mds) {
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

  /* api callbacks */
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

  /* api callbacks */
  ot->exec = fluid_free_exec;
  ot->poll = ED_operator_object_active_editable;
}

void FLUID_OT_bake_data(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Bake Data";
  ot->description = "Bake Fluid Data";
  ot->idname = FLUID_JOB_BAKE_DATA;

  /* api callbacks */
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

  /* api callbacks */
  ot->exec = fluid_free_exec;
  ot->poll = ED_operator_object_active_editable;
}

void FLUID_OT_bake_noise(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Bake Noise";
  ot->description = "Bake Fluid Noise";
  ot->idname = FLUID_JOB_BAKE_NOISE;

  /* api callbacks */
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

  /* api callbacks */
  ot->exec = fluid_free_exec;
  ot->poll = ED_operator_object_active_editable;
}

void FLUID_OT_bake_mesh(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Bake Mesh";
  ot->description = "Bake Fluid Mesh";
  ot->idname = FLUID_JOB_BAKE_MESH;

  /* api callbacks */
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

  /* api callbacks */
  ot->exec = fluid_free_exec;
  ot->poll = ED_operator_object_active_editable;
}

void FLUID_OT_bake_particles(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Bake Particles";
  ot->description = "Bake Fluid Particles";
  ot->idname = FLUID_JOB_BAKE_PARTICLES;

  /* api callbacks */
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

  /* api callbacks */
  ot->exec = fluid_free_exec;
  ot->poll = ED_operator_object_active_editable;
}

void FLUID_OT_bake_guides(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Bake Guides";
  ot->description = "Bake Fluid Guiding";
  ot->idname = FLUID_JOB_BAKE_GUIDES;

  /* api callbacks */
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

  /* api callbacks */
  ot->exec = fluid_free_exec;
  ot->poll = ED_operator_object_active_editable;
}

void FLUID_OT_pause_bake(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Pause Bake";
  ot->description = "Pause Bake";
  ot->idname = FLUID_JOB_BAKE_PAUSE;

  /* api callbacks */
  ot->exec = fluid_pause_exec;
  ot->poll = ED_operator_object_active_editable;
}
