/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#pragma once

#include "BKE_subdiv.hh"

#include "BLI_sys_types.h"

/* Hardcoded for until GPU shaders are automatically generated, then we will have a more
 * programmatic way of detecting this. */
#define MAX_GPU_SUBDIV_SSBOS 12

struct Mesh;
struct Object;
struct Scene;
struct Subdiv;
struct SubdivSettings;
struct SubsurfModifierData;

/* Runtime subsurf modifier data, cached in modifier on evaluated meshes. */
struct SubsurfRuntimeData {
  /* Subdivision settings, exists before descriptor or mesh wrapper is created. */
  SubdivSettings settings;

  /* Cached subdivision surface descriptor, with topology and settings. */
  Subdiv *subdiv_cpu;
  Subdiv *subdiv_gpu;

  /* Recent usage markers for UI diagnostics. To avoid UI flicker due to races
   * between evaluation and UI redraw, they are set to 2 when an evaluator is used,
   * and count down every frame. */
  char used_cpu, used_gpu;

  /* Cached mesh wrapper data, to be used for GPU subdiv or lazy evaluation on CPU. */
  bool has_gpu_subdiv;
  int resolution;
  bool use_optimal_display;
  bool use_loop_normals;

  /* Cached from the draw code for stats display. */
  int stats_totvert;
  int stats_totedge;
  int stats_faces_num;
  int stats_totloop;
};

SubdivSettings BKE_subsurf_modifier_settings_init(const SubsurfModifierData *smd,
                                                  bool use_render_params);

bool BKE_subsurf_modifier_runtime_init(SubsurfModifierData *smd, bool use_render_params);

bool BKE_subsurf_modifier_use_custom_loop_normals(const SubsurfModifierData *smd,
                                                  const Mesh *mesh);
bool BKE_subsurf_modifier_has_split_normals(const SubsurfModifierData *smd, const Mesh *mesh);

/**
 * Return true if GPU subdivision evaluation is disabled by force due to incompatible mesh or
 * modifier settings. This will only return true if GPU subdivision is enabled in the preferences
 * and supported by the GPU. It is mainly useful for showing UI messages.
 */
bool BKE_subsurf_modifier_force_disable_gpu_evaluation_for_mesh(const SubsurfModifierData *smd,
                                                                const Mesh *mesh);
/**
 * \param skip_check_is_last: When true, we assume that the modifier passed is the last enabled
 * modifier in the stack.
 */
bool BKE_subsurf_modifier_can_do_gpu_subdiv(const Scene *scene,
                                            const Object *ob,
                                            const Mesh *mesh,
                                            const SubsurfModifierData *smd,
                                            int required_mode);

bool BKE_subsurf_modifier_has_gpu_subdiv(const Mesh *mesh);

extern void (*BKE_subsurf_modifier_free_gpu_cache_cb)(Subdiv *subdiv);

/**
 * Main goal of this function is to give usable subdivision surface descriptor
 * which matches settings and topology.
 */
Subdiv *BKE_subsurf_modifier_subdiv_descriptor_ensure(SubsurfRuntimeData *runtime_data,
                                                      const Mesh *mesh,
                                                      bool for_draw_code);

/**
 * Return the #ModifierMode required for the evaluation of the subsurf modifier,
 * which should be used to check if the modifier is enabled.
 */
int BKE_subsurf_modifier_eval_required_mode(bool is_final_render, bool is_edit_mode);
