/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "BLI_enum_flags.hh"
#include "BLI_sys_types.h"

/** Opaque type hiding blender::gpu::Fence. */
struct GPUFence;

enum GPUWriteMask {
  GPU_WRITE_NONE = 0,
  GPU_WRITE_RED = (1 << 0),
  GPU_WRITE_GREEN = (1 << 1),
  GPU_WRITE_BLUE = (1 << 2),
  GPU_WRITE_ALPHA = (1 << 3),
  GPU_WRITE_DEPTH = (1 << 4),
  GPU_WRITE_STENCIL = (1 << 5),
  GPU_WRITE_COLOR = (GPU_WRITE_RED | GPU_WRITE_GREEN | GPU_WRITE_BLUE | GPU_WRITE_ALPHA),
};

ENUM_OPERATORS(GPUWriteMask)

enum GPUBarrier {
  /* Texture Barrier. */

  /** All written texture prior to this barrier can be bound as frame-buffer attachment. */
  GPU_BARRIER_FRAMEBUFFER = (1 << 0),
  /** All written texture prior to this barrier can be bound as image. */
  GPU_BARRIER_SHADER_IMAGE_ACCESS = (1 << 1),
  /** All written texture prior to this barrier can be bound as sampler. */
  GPU_BARRIER_TEXTURE_FETCH = (1 << 2),
  /** All written texture prior to this barrier can be read or updated with CPU memory. */
  GPU_BARRIER_TEXTURE_UPDATE = (1 << 3),
  /** All written texture prior to this barrier can be read or updated with PBO. */
  // GPU_BARRIER_PIXEL_BUFFER = (1 << 4), /* Not implemented yet. */

  /* Buffer Barrier. */

  /** All written buffer prior to this barrier can be bound as indirect command buffer. */
  GPU_BARRIER_COMMAND = (1 << 10),
  /** All written buffer prior to this barrier can be bound as SSBO. */
  GPU_BARRIER_SHADER_STORAGE = (1 << 11),
  /** All written buffer prior to this barrier can be bound as VBO. */
  GPU_BARRIER_VERTEX_ATTRIB_ARRAY = (1 << 12),
  /** All written buffer prior to this barrier can be bound as IBO. */
  GPU_BARRIER_ELEMENT_ARRAY = (1 << 13),
  /** All written buffer prior to this barrier can be bound as UBO. */
  GPU_BARRIER_UNIFORM = (1 << 14),
  /** All written buffer prior to this barrier can be read or updated with CPU memory. */
  GPU_BARRIER_BUFFER_UPDATE = (1 << 15),
  /** All written persistent mapped buffer prior to this barrier can be read or updated. */
  // GPU_BARRIER_CLIENT_MAPPED_BUFFER = (1 << 15), /* Not implemented yet. */
};

ENUM_OPERATORS(GPUBarrier)

/* NOTE: For Metal and Vulkan only.
 * TODO(Metal): Update barrier calls to use stage flags. */
enum GPUStageBarrierBits {
  GPU_BARRIER_STAGE_VERTEX = (1 << 0),
  GPU_BARRIER_STAGE_FRAGMENT = (1 << 1),
  GPU_BARRIER_STAGE_COMPUTE = (1 << 2),
  GPU_BARRIER_STAGE_ANY_GRAPHICS = (GPU_BARRIER_STAGE_VERTEX | GPU_BARRIER_STAGE_FRAGMENT),
  GPU_BARRIER_STAGE_ANY = (GPU_BARRIER_STAGE_VERTEX | GPU_BARRIER_STAGE_FRAGMENT |
                           GPU_BARRIER_STAGE_COMPUTE),
};

ENUM_OPERATORS(GPUStageBarrierBits)

/**
 * Defines the fixed pipeline blending equation.
 * SRC is the output color from the shader.
 * DST is the color from the frame-buffer.
 * The blending equation is:
 * `(SRC * A) + (DST * B)`.
 * The blend mode will modify the A and B parameters.
 */
enum GPUBlend {
  GPU_BLEND_NONE = 0,
  /** Pre-multiply variants will _NOT_ multiply rgb output by alpha. */
  GPU_BLEND_ALPHA,
  GPU_BLEND_ALPHA_PREMULT,
  GPU_BLEND_ADDITIVE,
  GPU_BLEND_ADDITIVE_PREMULT,
  GPU_BLEND_MULTIPLY,
  GPU_BLEND_SUBTRACT,
  /** Replace logic op: SRC * (1 - DST)
   * NOTE: Does not modify alpha. */
  GPU_BLEND_INVERT,
  /** Stores min(SRC, DST) per component. */
  GPU_BLEND_MIN,
  /** Stores max(SRC, DST) per component. */
  GPU_BLEND_MAX,
  /** Order independent transparency.
   * NOTE: Cannot be used as is. Needs special setup (frame-buffer, shader ...). */
  GPU_BLEND_OIT,
  /** Special blend to add color under and multiply DST color by SRC alpha. */
  GPU_BLEND_BACKGROUND,
  /** Custom blend parameters using dual source blending : SRC0 + SRC1 * DST
   * NOTE: Can only be used with _ONE_ Draw Buffer and shader needs to be specialized. */
  GPU_BLEND_CUSTOM,
  GPU_BLEND_ALPHA_UNDER_PREMUL,
  /** Multiplies every channel (alpha included) by `1 - SRC.a`. Used for piercing a hole using an
   * image alpha channel. */
  GPU_BLEND_OVERLAY_MASK_FROM_ALPHA,
};

enum GPUDepthTest {
  GPU_DEPTH_NONE = 0,
  GPU_DEPTH_ALWAYS, /* Used to draw to the depth buffer without really testing. */
  GPU_DEPTH_LESS,
  GPU_DEPTH_LESS_EQUAL, /* Default. */
  GPU_DEPTH_EQUAL,
  GPU_DEPTH_GREATER,
  GPU_DEPTH_GREATER_EQUAL,
};

enum GPUStencilTest {
  GPU_STENCIL_NONE = 0,
  GPU_STENCIL_ALWAYS,
  GPU_STENCIL_EQUAL,
  GPU_STENCIL_NEQUAL,
};

enum GPUStencilOp {
  GPU_STENCIL_OP_NONE = 0,
  GPU_STENCIL_OP_REPLACE,
  /** Special values for stencil shadows. */
  GPU_STENCIL_OP_COUNT_DEPTH_PASS,
  GPU_STENCIL_OP_COUNT_DEPTH_FAIL,
};

enum GPUFaceCullTest {
  GPU_CULL_NONE = 0, /* Culling disabled. */
  GPU_CULL_FRONT,
  GPU_CULL_BACK,
};

enum GPUProvokingVertex {
  GPU_VERTEX_LAST = 0,  /* Default. */
  GPU_VERTEX_FIRST = 1, /* Follow Blender loop order. */
};

void GPU_blend(GPUBlend blend);
void GPU_face_culling(GPUFaceCullTest culling);
void GPU_depth_test(GPUDepthTest test);
void GPU_stencil_test(GPUStencilTest test);
void GPU_provoking_vertex(GPUProvokingVertex vert);
void GPU_front_facing(bool invert);
void GPU_depth_range(float near, float far);
void GPU_scissor_test(bool enable);
void GPU_line_smooth(bool enable);
/**
 * \note By convention, this is set as needed and not reset back to 1.0.
 * This means code that draws lines must always set the line width beforehand,
 * but is not expected to restore its previous value.
 */
void GPU_line_width(float width);
void GPU_logic_op_xor_set(bool enable);
void GPU_point_size(float size);
void GPU_polygon_smooth(bool enable);

/**
 * Programmable point size:
 * - Shaders set their own point size when enabled
 * - Use GPU_point_size when disabled.
 *
 * TODO: remove and use program point size everywhere.
 */
void GPU_program_point_size(bool enable);
void GPU_scissor(int x, int y, int width, int height);
void GPU_scissor_get(int coords[4]);
void GPU_viewport(int x, int y, int width, int height);
void GPU_viewport_size_get_f(float coords[4]);
void GPU_viewport_size_get_i(int coords[4]);
void GPU_write_mask(GPUWriteMask mask);
void GPU_color_mask(bool r, bool g, bool b, bool a);
void GPU_depth_mask(bool depth);
bool GPU_depth_mask_get();
void GPU_shadow_offset(bool enable);
void GPU_clip_distances(int distances_enabled);
bool GPU_mipmap_enabled();
void GPU_state_set(GPUWriteMask write_mask,
                   GPUBlend blend,
                   GPUFaceCullTest culling_test,
                   GPUDepthTest depth_test,
                   GPUStencilTest stencil_test,
                   GPUStencilOp stencil_op,
                   GPUProvokingVertex provoking_vert);

void GPU_stencil_reference_set(uint reference);
void GPU_stencil_write_mask_set(uint write_mask);
void GPU_stencil_compare_mask_set(uint compare_mask);

/* Sets the depth range to be 0..1. Only have effect with the OpenGL backend. Have no effect if
 * glClipControl is not supported. Shaders used for drawing with this state must use
 * BuiltinBits::CLIP_CONTROL for their vertex shader to be patched. */
void GPU_clip_control_unit_range(bool enable);

GPUFaceCullTest GPU_face_culling_get();
GPUBlend GPU_blend_get();
GPUDepthTest GPU_depth_test_get();
GPUWriteMask GPU_write_mask_get();
uint GPU_stencil_mask_get();
GPUStencilTest GPU_stencil_test_get();
/**
 * \note Already pre-multiplied by `U.pixelsize`.
 */
float GPU_line_width_get();
bool GPU_line_smooth_get();

void GPU_flush();
void GPU_finish();
void GPU_apply_state();

/**
 * A barrier _must_ be issued _after_ a shader arbitrary write to a buffer or a
 * texture (i.e: using imageStore, imageAtomics, or SSBO). Otherwise, the written value may not
 * appear updated to the next user of this resource.
 *
 * The type of barrier must be chosen depending on the _future_ use of the memory that was written
 * by the shader.
 */
void GPU_memory_barrier(GPUBarrier barrier);

GPUFence *GPU_fence_create();
void GPU_fence_free(GPUFence *fence);
void GPU_fence_signal(GPUFence *fence);
void GPU_fence_wait(GPUFence *fence);
