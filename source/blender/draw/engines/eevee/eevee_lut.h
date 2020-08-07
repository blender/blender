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
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup gpu
 */

#pragma once

extern const float ltc_mat_ggx[64 * 64 * 4];
extern const float ltc_mag_ggx[64 * 64 * 2];
extern const float bsdf_split_sum_ggx[64 * 64 * 2];
extern const float ltc_disk_integral[64 * 64];
extern const float btdf_split_sum_ggx[32][64 * 64];
extern const float blue_noise[64 * 64][4];
