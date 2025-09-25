/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Clear AOVs for secondary deferred layers.
 * The first opaque layer will always have AOV buffers cleared.
 * However the subsequent layers (e.g. refraction) have to clear the result of the first layer for
 * all the pixels they touch. Doing it inside the material shader proved to be a bottleneck for
 * shader compilation. To avoid the overhead, we draw a fullscreen triangle that will clear the
 * AOVs for the pixel affected by the next layer using stencil test after the prepass.
 */

#include "infos/eevee_deferred_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(eevee_deferred_aov_clear)

#include "eevee_renderpass_lib.glsl"

void main()
{
  clear_aovs();
}
