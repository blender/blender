/*
 * Copyright 2016, Blender Foundation.
 *
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
 * Contributor(s): Blender Institute
 *
 */

/** \file workbench_studiolight.c
 *  \ingroup draw_engine
 */
#include "BKE_studiolight.h"

#include "DRW_engine.h"
#include "workbench_private.h"

#include "BLI_math.h"

void studiolight_update_world(StudioLight *sl, WORKBENCH_UBO_World *wd)
{
	copy_v3_v3(wd->diffuse_light_x_pos, sl->diffuse_light[STUDIOLIGHT_X_POS]);
	copy_v3_v3(wd->diffuse_light_x_neg, sl->diffuse_light[STUDIOLIGHT_X_NEG]);
	copy_v3_v3(wd->diffuse_light_y_pos, sl->diffuse_light[STUDIOLIGHT_Y_POS]);
	copy_v3_v3(wd->diffuse_light_y_neg, sl->diffuse_light[STUDIOLIGHT_Y_NEG]);
	copy_v3_v3(wd->diffuse_light_z_pos, sl->diffuse_light[STUDIOLIGHT_Z_POS]);
	copy_v3_v3(wd->diffuse_light_z_neg, sl->diffuse_light[STUDIOLIGHT_Z_NEG]);
}
