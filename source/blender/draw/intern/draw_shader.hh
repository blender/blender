/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#pragma once

#include "draw_curves_private.hh"
#include "draw_hair_private.h"

struct GPUShader;

/* draw_shader.cc */

GPUShader *DRW_shader_hair_refine_get(ParticleRefineShader refinement);

GPUShader *DRW_shader_curves_refine_get(blender::draw::CurvesEvalShader type);

GPUShader *DRW_shader_debug_print_display_get();
GPUShader *DRW_shader_debug_draw_display_get();
GPUShader *DRW_shader_draw_visibility_compute_get();
GPUShader *DRW_shader_draw_view_finalize_get();
GPUShader *DRW_shader_draw_resource_finalize_get();
GPUShader *DRW_shader_draw_command_generate_get();

void DRW_shaders_free(void);
