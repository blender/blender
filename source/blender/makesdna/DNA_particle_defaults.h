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

/* Struct members on own line. */
/* clang-format off */

/* -------------------------------------------------------------------- */
/** \name ParticleSettings Struct
 * \{ */

#define _DNA_DEFAULT_ParticleSettings \
  { \
    .type = PART_EMITTER, \
    .distr = PART_DISTR_JIT, \
    .draw_as = PART_DRAW_REND, \
    .ren_as = PART_DRAW_HALO, \
    .bb_uv_split = 1, \
    .flag = PART_EDISTR | PART_TRAND | PART_HIDE_ADVANCED_HAIR, \
 \
    .sta = 1.0f, \
    .end = 200.0f, \
    .lifetime = 50.0f, \
    .jitfac = 1.0f, \
    .totpart = 1000, \
    .grid_res = 10, \
    .timetweak = 1.0f, \
    .courant_target = 0.2f, \
 \
    .integrator = PART_INT_MIDPOINT, \
    .phystype = PART_PHYS_NEWTON, \
    .hair_step = 5, \
    .keys_step = 5, \
    .draw_step = 2, \
    .ren_step = 3, \
    .adapt_angle = 5, \
    .adapt_pix = 3, \
    .kink_axis = 2, \
    .kink_amp_clump = 1.0f, \
    .kink_extra_steps = 4, \
    .clump_noise_size = 1.0f, \
    .reactevent = PART_EVENT_DEATH, \
    .disp = 100, \
    .from = PART_FROM_FACE, \
 \
    .normfac = 1.0f, \
 \
    .mass = 1.0f, \
    .size = 0.05f, \
    .childsize = 1.0f, \
 \
    .rotmode = PART_ROT_VEL, \
    .avemode = PART_AVE_VELOCITY, \
 \
    .child_nbr = 10, \
    .ren_child_nbr = 100, \
    .childrad = 0.2f, \
    .childflat = 0.0f, \
    .clumppow = 0.0f, \
    .kink_amp = 0.2f, \
    .kink_freq = 2.0f, \
 \
    .rough1_size = 1.0f, \
    .rough2_size = 1.0f, \
    .rough_end_shape = 1.0f, \
 \
    .clength = 1.0f, \
    .clength_thres = 0.0f, \
 \
    .draw = 0, \
    .draw_line = {0.5f,}, \
    .path_start = 0.0f, \
    .path_end = 1.0f, \
 \
    .bb_size = {1.0f, 1.0f}, \
 \
    .keyed_loops = 1, \
 \
    .color_vec_max = 1.0f, \
    .draw_col = PART_DRAW_COL_MAT, \
 \
    .omat = 1, \
    .use_modifier_stack = false, \
    .draw_size = 0.1f, \
 \
    .shape_flag = PART_SHAPE_CLOSE_TIP, \
    .shape = 0.0f, \
    .rad_root = 1.0f, \
    .rad_tip = 0.0f, \
    .rad_scale = 0.01f, \
  }

/** \} */

/* clang-format on */
