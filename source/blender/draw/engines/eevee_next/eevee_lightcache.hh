/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup eevee
 */

#pragma once

#include "BLI_vector.hh"

struct wmWindowManager;
struct wmWindow;
struct Main;
struct ViewLayer;
struct Scene;
struct Object;
struct wmJob;

/** Opaque type hiding eevee::LightBake. */
struct EEVEE_NEXT_LightBake;

/* -------------------------------------------------------------------- */
/** \name Light Bake Job
 * \{ */

/**
 * Create the job description.
 * This is called for async (modal) bake operator.
 * The actual work will be done by `EEVEE_NEXT_lightbake_job()`.
 * IMPORTANT: Must run on the main thread because of potential GPUContext creation.
 */
wmJob *EEVEE_NEXT_lightbake_job_create(wmWindowManager *wm,
                                       wmWindow *win,
                                       Main *bmain,
                                       ViewLayer *view_layer,
                                       Scene *scene,
                                       blender::Vector<Object *> original_probes,
                                       int delay_ms,
                                       int frame);

/**
 * Allocate dependency graph and job description (EEVEE_NEXT_LightBake).
 * Dependency graph evaluation does *not* happen here. It is delayed until
 * `EEVEE_NEXT_lightbake_job` runs.
 * IMPORTANT: Must run on the main thread because of potential GPUContext creation.
 * Return `EEVEE_NEXT_LightBake *` but cast to `void *` because of compatibility with existing
 * EEVEE function.
 */
void *EEVEE_NEXT_lightbake_job_data_alloc(Main *bmain,
                                          ViewLayer *view_layer,
                                          Scene *scene,
                                          blender::Vector<Object *> original_probes,
                                          int frame);

/**
 * Free the job data.
 * NOTE: Does not free the GPUContext. This is the responsibility of `EEVEE_NEXT_lightbake_job()`
 */
void EEVEE_NEXT_lightbake_job_data_free(void *job_data /* EEVEE_NEXT_LightBake */);

/**
 * Callback for updating original scene light cache with bake result.
 * Run by the job system for each update step and the finish step.
 * This is called manually by `EEVEE_NEXT_lightbake_job()` if not run from a job.
 */
void EEVEE_NEXT_lightbake_update(void *job_data /* EEVEE_NEXT_LightBake */);

/**
 * Do the full light baking for all samples.
 * Will call `EEVEE_NEXT_lightbake_update()` on finish.
 */
void EEVEE_NEXT_lightbake_job(void *job_data /* EEVEE_NEXT_LightBake */,
                              bool *stop,
                              bool *do_update,
                              float *progress);

/** \} */
