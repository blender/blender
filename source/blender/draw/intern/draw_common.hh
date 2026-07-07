/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#pragma once

#include "draw_common_c.hh"
#include "draw_manager.hh"
#include "draw_pass.hh"

namespace blender::draw {

/** Hair. */

void hair_init();

/**
 * \note Only valid after #DRW_curves_update().
 */
gpu::VertBuf *hair_pos_buffer_get(Scene *scene,
                                  Object *object,
                                  ParticleSystem *psys,
                                  ModifierData *md);

gpu::Batch *hair_sub_pass_setup(PassMain::Sub &sub_ps,
                                const Scene *scene,
                                const ObjectRef &ob_ref,
                                ParticleSystem *psys,
                                ModifierData *md,
                                GPUMaterial *gpu_material = nullptr);

gpu::Batch *hair_sub_pass_setup(PassSimple::Sub &sub_ps,
                                const Scene *scene,
                                const ObjectRef &ob_ref,
                                ParticleSystem *psys,
                                ModifierData *md,
                                GPUMaterial *gpu_material = nullptr);

/** Curves. */

/**
 * \note Content of the vertex buf is only valid after #DRW_curves_update().
 */
gpu::VertBuf *curves_pos_buffer_get(Object *object);

gpu::Batch *curves_sub_pass_setup(PassMain::Sub &ps,
                                  const Scene *scene,
                                  Object *ob,
                                  const char *&r_error,
                                  GPUMaterial *gpu_material = nullptr);

gpu::Batch *curves_sub_pass_setup(PassSimple::Sub &ps,
                                  const Scene *scene,
                                  Object *ob,
                                  const char *&r_error,
                                  GPUMaterial *gpu_material = nullptr);

/* Point cloud. */

gpu::Batch *pointcloud_sub_pass_setup(PassMain::Sub &sub_ps,
                                      Object *object,
                                      GPUMaterial *gpu_material = nullptr);

gpu::Batch *pointcloud_sub_pass_setup(PassSimple::Sub &sub_ps,
                                      Object *object,
                                      GPUMaterial *gpu_material = nullptr);

/** Volume. */

/**
 * Add attribute bindings of volume grids to an existing pass.
 * No draw call is added so the caller can decide how to use the data.
 * \return nullptr if there is nothing to draw.
 */
PassMain::Sub *volume_sub_pass(PassMain::Sub &ps,
                               Scene *scene,
                               Object *ob,
                               GPUMaterial *gpu_material);
/**
 * Add attribute bindings of volume grids to an existing pass.
 * No draw call is added so the caller can decide how to use the data.
 * \return nullptr if there is nothing to draw.
 */
PassSimple::Sub *volume_sub_pass(PassSimple::Sub &ps,
                                 Scene *scene,
                                 Object *ob,
                                 GPUMaterial *gpu_material);

}  // namespace blender::draw
