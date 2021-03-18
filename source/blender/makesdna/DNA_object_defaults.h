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
 */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "DNA_vec_defaults.h"

/* Struct members on own line. */
/* clang-format off */

/* -------------------------------------------------------------------- */
/** \name Object Struct
 * \{ */

#define _DNA_DEFAULT_Object \
  { \
    /* Type is not very meaningful as a default, normally changed. */ \
    .type = OB_EMPTY, \
    .color = {1, 1, 1, 1}, \
 \
    .constinv = _DNA_DEFAULT_UNIT_M4, \
    .parentinv = _DNA_DEFAULT_UNIT_M4, \
    .obmat = _DNA_DEFAULT_UNIT_M4, \
 \
    .scale = {1, 1, 1}, \
    .dscale = {1, 1, 1}, \
    /* Objects should default to having Euler XYZ rotations, \
     * but rotations default to quaternions. */ \
    .rotmode = ROT_MODE_EUL, \
    /** See #unit_axis_angle. */ \
    .rotAxis = {0, 1, 0}, \
    .rotAngle = 0, \
    .drotAxis = {0, 1, 0}, \
    .drotAngle = 0, \
    .quat = _DNA_DEFAULT_UNIT_QT, \
    .dquat = _DNA_DEFAULT_UNIT_QT, \
    .protectflag = OB_LOCK_ROT4D, \
 \
    .dt = OB_TEXTURE, \
 \
    .empty_drawtype = OB_PLAINAXES, \
    .empty_drawsize = 1.0, \
    .empty_image_depth = OB_EMPTY_IMAGE_DEPTH_DEFAULT, \
    .ima_ofs = {-0.5, -0.5}, \
 \
    .instance_faces_scale = 1, \
    .col_group = 0x01,  \
    .col_mask = 0xffff, \
    .preview = NULL, \
    .duplicator_visibility_flag = OB_DUPLI_FLAG_VIEWPORT | OB_DUPLI_FLAG_RENDER, \
    .pc_ids = {NULL, NULL}, \
    .lineart = { .crease_threshold = DEG2RAD(140.0f) }, \
  }

/** \} */

/* clang-format on */
