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
#include "DRW_engine.h"
#include "workbench_private.h"

#include "BLI_math.h"


#define STUDIOLIGHT_X_POS 0
#define STUDIOLIGHT_X_NEG 1
#define STUDIOLIGHT_Y_POS 2
#define STUDIOLIGHT_Y_NEG 3
#define STUDIOLIGHT_Z_POS 4
#define STUDIOLIGHT_Z_NEG 5

const float studiolights[][6][3] = {
	{
		{1.0,  0.8,   0.6},
		{1.0,  0.6,   0.6},
		{0.9,  0.9,   1.0},
		{0.05, 0.025, 0.025},
		{0.8,  0.8,   0.75},
		{1.0,  0.95,  0.8},
	},
	{
		{0.0, 0.0, 0.0},
		{0.0, 0.0, 0.0},
		{0.0, 0.0, 0.0},
		{0.0, 0.0, 0.0},
		{0.8, 0.8, 1.0},
		{0.0, 0.0, 0.0},
	},
	{
		{0.4, 0.3, 0.8},
		{0.4, 0.3, 0.8},
		{0.8, 0.8, 0.2},
		{0.0, 0.0, 0.0},
		{0.4, 0.4, 0.8},
		{0.0, 0.0, 0.0},
	},
	{
		{0.2, 0.2, 0.0},
		{0.8, 0.2, 0.0},
		{0.8, 0.2, 0.0},
		{0.2, 0.2, 0.0},
		{0.8, 0.6, 0.4},
		{0.0, 0.0, 0.0},
	},
	{
		{0.8, 0.2, 0.0},
		{0.8, 0.2, 0.0},
		{0.8, 0.6, 0.0},
		{0.2, 0.2, 0.0},
		{1.0, 0.5, 0.0},
		{0.0, 0.0, 0.0},
	},
};

void studiolight_update_world(int studio_light, WORKBENCH_UBO_World *wd)
{
	copy_v3_v3(wd->diffuse_light_x_pos, studiolights[studio_light][STUDIOLIGHT_X_POS]);
	copy_v3_v3(wd->diffuse_light_x_neg, studiolights[studio_light][STUDIOLIGHT_X_NEG]);
	copy_v3_v3(wd->diffuse_light_y_pos, studiolights[studio_light][STUDIOLIGHT_Y_POS]);
	copy_v3_v3(wd->diffuse_light_y_neg, studiolights[studio_light][STUDIOLIGHT_Y_NEG]);
	copy_v3_v3(wd->diffuse_light_z_pos, studiolights[studio_light][STUDIOLIGHT_Z_POS]);
	copy_v3_v3(wd->diffuse_light_z_neg, studiolights[studio_light][STUDIOLIGHT_Z_NEG]);
}

uint *WORKBENCH_generate_studiolight_preview(int studiolight_id, int icon_size)
{
	unsigned int* rect = MEM_mallocN(icon_size * icon_size * sizeof(unsigned int), __func__);
	int icon_center = icon_size / 2;
	float sphere_radius = icon_center * 0.9;

	int offset = 0;
	for (int y = 0 ; y < icon_size ; y ++) {
		float dy = y - icon_center;
		for (int x = 0 ; x < icon_size ; x ++) {
			float dx = x - icon_center;
			/* calculate aliasing */
			float alias = 0;
			const float alias_step = 0.2;
			for (float ay = dy - 0.5; ay < dy + 0.5 ; ay += alias_step) {
				for (float ax = dx - 0.5; ax < dx + 0.5 ; ax += alias_step) {
					if (sqrt(ay*ay + ax*ax) < sphere_radius) {
						alias += alias_step*alias_step;
					}
				}
			}
			unsigned int pixelresult = 0x0;
			unsigned int alias_i = clamp_i(alias * 256, 0, 255);
			if (alias_i != 0) {
				/* calculate normal */
				unsigned int alias_mask = alias_i << 24;
				float normal[3];
				normal[0] = dx / sphere_radius;
				normal[1] = dy / sphere_radius;
				normal[2] = sqrt(-(normal[0]*normal[0])-(normal[1]*normal[1]) + 1);
				normalize_v3(normal);

				float color[3];
				mul_v3_v3fl(color, studiolights[studiolight_id][STUDIOLIGHT_X_POS], clamp_f(normal[0], 0.0, 1.0));
				interp_v3_v3v3(color, color, studiolights[studiolight_id][STUDIOLIGHT_X_NEG], clamp_f(-normal[0], 0.0, 1.0));
				interp_v3_v3v3(color, color, studiolights[studiolight_id][STUDIOLIGHT_Y_POS], clamp_f(normal[1], 0.0, 1.0));
				interp_v3_v3v3(color, color, studiolights[studiolight_id][STUDIOLIGHT_Y_NEG], clamp_f(-normal[1], 0.0, 1.0));
				interp_v3_v3v3(color, color, studiolights[studiolight_id][STUDIOLIGHT_Z_POS], clamp_f(normal[2], 0.0, 1.0));

				pixelresult = rgb_to_cpack(
					linearrgb_to_srgb(color[0]),
					linearrgb_to_srgb(color[1]),
					linearrgb_to_srgb(color[2])
				) | alias_mask;
			}
			rect[offset++] = pixelresult;
		}
	}
	return rect;
}
