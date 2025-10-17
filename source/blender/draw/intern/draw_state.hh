/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "GPU_state.hh"

#include "BLI_enum_flags.hh"

/** \file
 * \ingroup draw
 *
 * Internal Pipeline State tracking. It is higher level than GPU state as everything fits a single
 * enum.
 */

/**
 * DRWState is a bit-mask that stores the current render state and the desired render state. Based
 * on the differences the minimum state changes can be invoked to setup the desired render state.
 *
 * The Write Stencil, Stencil test, Depth test and Blend state options are mutual exclusive
 * therefore they aren't ordered as a bit mask.
 */
typedef enum : uint32_t {
  /** To be used for compute passes. */
  DRW_STATE_NO_DRAW = 0,
  /** Write mask */
  DRW_STATE_WRITE_DEPTH = (1 << 0),
  DRW_STATE_WRITE_COLOR = (1 << 1),
  /* Write Stencil. These options are mutual exclusive and packed into 2 bits */
  DRW_STATE_WRITE_STENCIL = (1 << 2),
  DRW_STATE_WRITE_STENCIL_SHADOW_PASS = (2 << 2),
  DRW_STATE_WRITE_STENCIL_SHADOW_FAIL = (3 << 2),
  /** Depth test. These options are mutual exclusive and packed into 3 bits */
  DRW_STATE_DEPTH_ALWAYS = (1 << 4),
  DRW_STATE_DEPTH_LESS = (2 << 4),
  DRW_STATE_DEPTH_LESS_EQUAL = (3 << 4),
  DRW_STATE_DEPTH_EQUAL = (4 << 4),
  DRW_STATE_DEPTH_GREATER = (5 << 4),
  DRW_STATE_DEPTH_GREATER_EQUAL = (6 << 4),
  /** Culling test */
  DRW_STATE_CULL_BACK = (1 << 7),
  DRW_STATE_CULL_FRONT = (1 << 8),
  /** Stencil test. These options are mutually exclusive and packed into 2 bits. */
  DRW_STATE_STENCIL_ALWAYS = (1 << 9),
  DRW_STATE_STENCIL_EQUAL = (2 << 9),
  DRW_STATE_STENCIL_NEQUAL = (3 << 9),

  /** Blend state. These options are mutual exclusive and packed into 4 bits */
  DRW_STATE_BLEND_ADD = (1 << 11),
  /** Same as additive but let alpha accumulate without pre-multiply. */
  DRW_STATE_BLEND_ADD_FULL = (2 << 11),
  /** Standard alpha blending. */
  DRW_STATE_BLEND_ALPHA = (3 << 11),
  /** Use that if color is already pre-multiply by alpha. */
  DRW_STATE_BLEND_ALPHA_PREMUL = (4 << 11),
  DRW_STATE_BLEND_BACKGROUND = (5 << 11),
  DRW_STATE_BLEND_OIT = (6 << 11),
  DRW_STATE_BLEND_MUL = (7 << 11),
  DRW_STATE_BLEND_SUB = (8 << 11),
  /** Use dual source blending. WARNING: Only one color buffer allowed. */
  DRW_STATE_BLEND_CUSTOM = (9 << 11),
  DRW_STATE_LOGIC_INVERT = (10 << 11),
  DRW_STATE_BLEND_ALPHA_UNDER_PREMUL = (11 << 11),

  /* See GPU_clip_control_unit_range. */
  DRW_STATE_CLIP_CONTROL_UNIT_RANGE = (1 << 26),
  DRW_STATE_IN_FRONT_SELECT = (1 << 27),
  DRW_STATE_SHADOW_OFFSET = (1 << 28),
  DRW_STATE_CLIP_PLANES = (1 << 29),
  DRW_STATE_FIRST_VERTEX_CONVENTION = (1 << 30),
  /** DO NOT USE. Assumed always enabled. Only used internally. */
  DRW_STATE_PROGRAM_POINT_SIZE = (1u << 31),
} DRWState;

ENUM_OPERATORS(DRWState);

#define DRW_STATE_DEFAULT \
  (DRW_STATE_WRITE_DEPTH | DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_LESS_EQUAL)
#define DRW_STATE_BLEND_ENABLED \
  (DRW_STATE_BLEND_ADD | DRW_STATE_BLEND_ADD_FULL | DRW_STATE_BLEND_ALPHA | \
   DRW_STATE_BLEND_ALPHA_PREMUL | DRW_STATE_BLEND_BACKGROUND | DRW_STATE_BLEND_OIT | \
   DRW_STATE_BLEND_MUL | DRW_STATE_BLEND_SUB | DRW_STATE_BLEND_CUSTOM | DRW_STATE_LOGIC_INVERT)
#define DRW_STATE_RASTERIZER_ENABLED \
  (DRW_STATE_WRITE_DEPTH | DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_STENCIL | \
   DRW_STATE_WRITE_STENCIL_SHADOW_PASS | DRW_STATE_WRITE_STENCIL_SHADOW_FAIL)
#define DRW_STATE_DEPTH_TEST_ENABLED \
  (DRW_STATE_DEPTH_ALWAYS | DRW_STATE_DEPTH_LESS | DRW_STATE_DEPTH_LESS_EQUAL | \
   DRW_STATE_DEPTH_EQUAL | DRW_STATE_DEPTH_GREATER | DRW_STATE_DEPTH_GREATER_EQUAL)
#define DRW_STATE_STENCIL_TEST_ENABLED \
  (DRW_STATE_STENCIL_ALWAYS | DRW_STATE_STENCIL_EQUAL | DRW_STATE_STENCIL_NEQUAL)
#define DRW_STATE_WRITE_STENCIL_ENABLED \
  (DRW_STATE_WRITE_STENCIL | DRW_STATE_WRITE_STENCIL_SHADOW_PASS | \
   DRW_STATE_WRITE_STENCIL_SHADOW_FAIL)

namespace blender::draw {

/* -------------------------------------------------------------------- */
/** \name DRWState to GPU state conversion
 * \{ */

static inline GPUWriteMask to_write_mask(DRWState state)
{
  GPUWriteMask write_mask = GPU_WRITE_NONE;
  if (state & DRW_STATE_WRITE_DEPTH) {
    write_mask |= GPU_WRITE_DEPTH;
  }
  if (state & DRW_STATE_WRITE_COLOR) {
    write_mask |= GPU_WRITE_COLOR;
  }
  if (state & DRW_STATE_WRITE_STENCIL_ENABLED) {
    write_mask |= GPU_WRITE_STENCIL;
  }
  return write_mask;
}

static inline GPUFaceCullTest to_face_cull_test(DRWState state)
{
  switch (state & (DRW_STATE_CULL_BACK | DRW_STATE_CULL_FRONT)) {
    case DRW_STATE_CULL_BACK:
      return GPU_CULL_BACK;
    case DRW_STATE_CULL_FRONT:
      return GPU_CULL_FRONT;
    default:
      return GPU_CULL_NONE;
  }
}

static inline GPUDepthTest to_depth_test(DRWState state)
{
  switch (state & DRW_STATE_DEPTH_TEST_ENABLED) {
    case DRW_STATE_DEPTH_LESS:
      return GPU_DEPTH_LESS;
    case DRW_STATE_DEPTH_LESS_EQUAL:
      return GPU_DEPTH_LESS_EQUAL;
    case DRW_STATE_DEPTH_EQUAL:
      return GPU_DEPTH_EQUAL;
    case DRW_STATE_DEPTH_GREATER:
      return GPU_DEPTH_GREATER;
    case DRW_STATE_DEPTH_GREATER_EQUAL:
      return GPU_DEPTH_GREATER_EQUAL;
    case DRW_STATE_DEPTH_ALWAYS:
      return GPU_DEPTH_ALWAYS;
    default:
      return GPU_DEPTH_NONE;
  }
}

static inline GPUStencilOp to_stencil_op(DRWState state)
{
  switch (state & DRW_STATE_WRITE_STENCIL_ENABLED) {
    case DRW_STATE_WRITE_STENCIL:
      return GPU_STENCIL_OP_REPLACE;
    case DRW_STATE_WRITE_STENCIL_SHADOW_PASS:
      return GPU_STENCIL_OP_COUNT_DEPTH_PASS;
    case DRW_STATE_WRITE_STENCIL_SHADOW_FAIL:
      return GPU_STENCIL_OP_COUNT_DEPTH_FAIL;
    default:
      return GPU_STENCIL_OP_NONE;
  }
}

static inline GPUStencilTest to_stencil_test(DRWState state)
{
  switch (state & DRW_STATE_STENCIL_TEST_ENABLED) {
    case DRW_STATE_STENCIL_ALWAYS:
      return GPU_STENCIL_ALWAYS;
    case DRW_STATE_STENCIL_EQUAL:
      return GPU_STENCIL_EQUAL;
    case DRW_STATE_STENCIL_NEQUAL:
      return GPU_STENCIL_NEQUAL;
    default:
      return GPU_STENCIL_NONE;
  }
}

static inline GPUBlend to_blend(DRWState state)
{
  switch (state & DRW_STATE_BLEND_ENABLED) {
    case DRW_STATE_BLEND_ADD:
      return GPU_BLEND_ADDITIVE;
    case DRW_STATE_BLEND_ADD_FULL:
      return GPU_BLEND_ADDITIVE_PREMULT;
    case DRW_STATE_BLEND_ALPHA:
      return GPU_BLEND_ALPHA;
    case DRW_STATE_BLEND_ALPHA_PREMUL:
      return GPU_BLEND_ALPHA_PREMULT;
    case DRW_STATE_BLEND_BACKGROUND:
      return GPU_BLEND_BACKGROUND;
    case DRW_STATE_BLEND_OIT:
      return GPU_BLEND_OIT;
    case DRW_STATE_BLEND_MUL:
      return GPU_BLEND_MULTIPLY;
    case DRW_STATE_BLEND_SUB:
      return GPU_BLEND_SUBTRACT;
    case DRW_STATE_BLEND_CUSTOM:
      return GPU_BLEND_CUSTOM;
    case DRW_STATE_LOGIC_INVERT:
      return GPU_BLEND_INVERT;
    case DRW_STATE_BLEND_ALPHA_UNDER_PREMUL:
      return GPU_BLEND_ALPHA_UNDER_PREMUL;
    default:
      return GPU_BLEND_NONE;
  }
}

static inline GPUProvokingVertex to_provoking_vertex(DRWState state)
{
  switch (state & DRW_STATE_FIRST_VERTEX_CONVENTION) {
    case DRW_STATE_FIRST_VERTEX_CONVENTION:
      return GPU_VERTEX_FIRST;
    default:
      return GPU_VERTEX_LAST;
  }
}

/** \} */

};  // namespace blender::draw
