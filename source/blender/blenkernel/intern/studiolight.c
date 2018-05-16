/*
 * ***** BEGIN GPL LICENSE BLOCK *****
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
 * The Original Code is Copyright (C) 2006-2007 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

/** \file blender/blenkernel/intern/studiolight.c
 *  \ingroup bke
 */

#include "BKE_studiolight.h"

#include "BKE_appdir.h"
#include "BKE_icons.h"

#include "BLI_fileops.h"
#include "BLI_fileops_types.h"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_path_util.h"
#include "BLI_string.h"

#include "DNA_listBase.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "MEM_guardedalloc.h"


/* Statics */
static ListBase studiolights;
#define STUDIO_LIGHT_EXTENSIONS ".jpg", ".hdr"
#define STUDIO_LIGHT_DIFFUSE_SAMPLE_STEP 64
static const char *STUDIO_LIGHT_FOLDER = "studiolights/";

/* FUNCTIONS */
static void studiolight_free(struct StudioLight *sl)
{
	MEM_freeN(sl);
}

static struct StudioLight *studiolight_create(void)
{
	struct StudioLight *sl = MEM_callocN(sizeof(*sl), __func__);
	sl->path[0] = 0x00;
	sl->name[0] = 0x00;
	sl->flag = 0;
	sl->index = BLI_listbase_count(&studiolights);
	sl->icon_id = BKE_icon_ensure_studio_light(sl);
	return sl;
}

static void direction_to_equirectangular(float r[2], const float dir[3])
{
	r[0] = (atan2f(dir[1], dir[0]) - M_PI) / -(M_PI * 2);
	r[1] = (acosf(dir[2] / 1.0) - M_PI) / -M_PI;
}

static void studiolight_calculate_directional_diffuse_light(ImBuf *ibuf, float color[4], const float start[3], const float v1[3], const float v2[3])
{
	const int steps = STUDIO_LIGHT_DIFFUSE_SAMPLE_STEP;
	float uv[2];
	float dir[3];
	float col[4];
	float v11[3];
	float v12[3];
	float totcol[4];

	zero_v4(totcol);
	for (int x = 0; x < steps ; x++) {
		float xf = (float)x / (float)steps;
		mul_v3_v3fl(v11, v1, xf);
		for (int y = 0; y < steps; y++) {
			float yf = (float)y / (float)steps;
			/* start + x/steps*v1 + y/steps*v2 */
			mul_v3_v3fl(v12, v2, yf);
			add_v3_v3v3(dir, start, v11);
			add_v3_v3(dir, v12);
			/* normalize */
			normalize_v3(dir);

			/* sample */
			direction_to_equirectangular(uv, dir);
			nearest_interpolation_color_wrap(ibuf, NULL, col, uv[0] * ibuf->x, uv[1] * ibuf->y);
			add_v3_v3(totcol, col);
		}
	}
	mul_v3_v3fl(color, totcol, 1.0/(steps*steps));
}

static void studiolight_calculate_diffuse_light(StudioLight *sl)
{
	float start[3];
	float v1[3];
	float v2[3];

	/* init light to black */
	copy_v3_fl(sl->diffuse_light[STUDIOLIGHT_X_POS], 0.0f);
	copy_v3_fl(sl->diffuse_light[STUDIOLIGHT_X_NEG], 0.0f);
	copy_v3_fl(sl->diffuse_light[STUDIOLIGHT_Y_POS], 0.0f);
	copy_v3_fl(sl->diffuse_light[STUDIOLIGHT_Y_NEG], 0.0f);
	copy_v3_fl(sl->diffuse_light[STUDIOLIGHT_Z_POS], 0.0f);
	copy_v3_fl(sl->diffuse_light[STUDIOLIGHT_Z_NEG], 0.0f);

	if (sl->flag &= STUDIOLIGHT_EXTERNAL_FILE) {
		ImBuf* ibuf = NULL;
		ibuf = IMB_loadiffname(sl->path, 0, NULL);
		if (ibuf) {
			IMB_float_from_rect(ibuf);

			copy_v3_fl3(start, -1.0f, -1.0f, -1.0f);
			copy_v3_fl3(v1, 0.0f, 2.0f, 0.0f);
			copy_v3_fl3(v2, 0.0f, 0.0f, 2.0f);
			studiolight_calculate_directional_diffuse_light(ibuf, sl->diffuse_light[STUDIOLIGHT_Z_NEG], start, v1, v2);
			copy_v3_fl3(start, 1.0f, -1.0f, -1.0f);
			studiolight_calculate_directional_diffuse_light(ibuf, sl->diffuse_light[STUDIOLIGHT_Z_POS], start, v1, v2);

			copy_v3_fl3(start, -1.0f, -1.0f, -1.0f);
			copy_v3_fl3(v1, 2.0f, 0.0f, 0.0f);
			copy_v3_fl3(v2, 0.0f, 0.0f, 2.0f);
			studiolight_calculate_directional_diffuse_light(ibuf, sl->diffuse_light[STUDIOLIGHT_X_POS], start, v1, v2);
			copy_v3_fl3(start, -1.0f, 1.0f, -1.0f);
			studiolight_calculate_directional_diffuse_light(ibuf, sl->diffuse_light[STUDIOLIGHT_X_NEG], start, v1, v2);

			copy_v3_fl3(start, -1.0f, -1.0f, -1.0f);
			copy_v3_fl3(v1, 2.0f, 0.0f, 0.0f);
			copy_v3_fl3(v2, 0.0f, 2.0f, 0.0f);
			studiolight_calculate_directional_diffuse_light(ibuf, sl->diffuse_light[STUDIOLIGHT_Y_NEG], start, v1, v2);
			copy_v3_fl3(start, -1.0f, -1.0f, 1.0f);
			studiolight_calculate_directional_diffuse_light(ibuf, sl->diffuse_light[STUDIOLIGHT_Y_POS], start, v1, v2);

			IMB_freeImBuf(ibuf);
		}
	}
	sl->flag |= STUDIOLIGHT_DIFFUSE_LIGHT_CALCULATED;
}

static void studiolight_add_files_from_datafolder(const int folder_id, const char* subfolder, int flag)
{
	StudioLight *sl;
	struct direntry *dir;
	const char *folder = BKE_appdir_folder_id(folder_id, subfolder);
	if (folder) {
		unsigned int totfile = BLI_filelist_dir_contents(folder, &dir);
		int i;
		for (i = 0; i < totfile; i++) {
			if ((dir[i].type & S_IFREG)) {
				const char *filename = dir[i].relname;
				const char *path = dir[i].path;
				if (BLI_testextensie_n(filename, STUDIO_LIGHT_EXTENSIONS, NULL)) {
					sl = studiolight_create();
					sl->flag = STUDIOLIGHT_EXTERNAL_FILE | flag;
					BLI_strncpy(sl->name, filename, FILE_MAXFILE);
					BLI_strncpy(sl->path, path, FILE_MAXFILE);
					BLI_addtail(&studiolights, sl);
				}
			}
		}
		BLI_filelist_free(dir, totfile);
		dir = NULL;
	}

}

/* API */
void BKE_studiolight_init(void)
{
	StudioLight *sl;
	/* go over the preset folder and add a studiolight for every image with its path */
	/* order studio lights by name */
	/* Also reserve icon space for it. */
	/* Add default studio light */
	sl = studiolight_create();
	BLI_strncpy(sl->name, "INTERNAL_01\0", FILE_MAXFILE);
	sl->flag = STUDIOLIGHT_DIFFUSE_LIGHT_CALCULATED;
	copy_v3_fl(sl->diffuse_light[STUDIOLIGHT_X_POS], 0.0f);
	copy_v3_fl(sl->diffuse_light[STUDIOLIGHT_X_NEG], 0.0f);
	copy_v3_fl(sl->diffuse_light[STUDIOLIGHT_Y_POS], 0.0f);
	copy_v3_fl(sl->diffuse_light[STUDIOLIGHT_Y_NEG], 0.0f);
	copy_v3_fl(sl->diffuse_light[STUDIOLIGHT_Z_POS], 1.0f);
	copy_v3_fl(sl->diffuse_light[STUDIOLIGHT_Z_NEG], 0.0f);
	BLI_addtail(&studiolights, sl);

	studiolight_add_files_from_datafolder(BLENDER_SYSTEM_DATAFILES, STUDIO_LIGHT_FOLDER, 0);
	studiolight_add_files_from_datafolder(BLENDER_USER_DATAFILES,   STUDIO_LIGHT_FOLDER, 0);
}

void BKE_studiolight_free(void)
{
	struct StudioLight *sl;
	while((sl = (StudioLight*)BLI_pophead(&studiolights)))
	{
		studiolight_free(sl);
	}
}

struct StudioLight *BKE_studiolight_find(const char* name)
{
	LISTBASE_FOREACH(StudioLight *, sl, &studiolights) {
		if (STREQLEN(sl->name, name, FILE_MAXFILE)) {
			return sl;
		}
	}
	/* When not found, use the default studio light */
	return (StudioLight*)studiolights.first;
}

struct StudioLight *BKE_studiolight_findindex(int index)
{
	LISTBASE_FOREACH(StudioLight *, sl, &studiolights) {
		if (sl->index == index) {
			return sl;
		}
	}
	/* When not found, use the default studio light */
	return (StudioLight*)studiolights.first;
}

const struct ListBase *BKE_studiolight_listbase(void)
{
	return &studiolights;
}

unsigned int *BKE_studiolight_preview(StudioLight *sl, int icon_size)
{
	BKE_studiolight_ensure_flag(sl, STUDIOLIGHT_DIFFUSE_LIGHT_CALCULATED);

	uint *rect = MEM_mallocN(icon_size * icon_size * sizeof(uint), __func__);
	int icon_center = icon_size / 2;
	float sphere_radius = icon_center * 0.9;

	int offset = 0;
	for (int y = 0; y < icon_size; y++) {
		float dy = y - icon_center;
		for (int x = 0; x < icon_size; x++) {
			float dx = x - icon_center;
			/* calculate aliasing */
			float alias = 0;
			const float alias_step = 0.333;
			for (float ay = dy - 0.5; ay < dy + 0.5; ay += alias_step) {
				for (float ax = dx - 0.5; ax < dx + 0.5; ax += alias_step) {
					if (sqrt(ay * ay + ax * ax) < sphere_radius) {
						alias += alias_step * alias_step;
					}
				}
			}
			uint pixelresult = 0x0;
			uint alias_i = clamp_i(alias * 256, 0, 255);
			if (alias_i != 0) {
				/* calculate normal */
				uint alias_mask = alias_i << 24;
				float normal[3];
				normal[0] = dx / sphere_radius;
				normal[1] = dy / sphere_radius;
				normal[2] = sqrt(-(normal[0] * normal[0]) - (normal[1] * normal[1]) + 1);
				normalize_v3(normal);

				float color[3];
				mul_v3_v3fl(color, sl->diffuse_light[STUDIOLIGHT_X_POS], clamp_f(normal[0], 0.0, 1.0));
				interp_v3_v3v3(color, color, sl->diffuse_light[STUDIOLIGHT_X_NEG], clamp_f(-normal[0], 0.0, 1.0));
				interp_v3_v3v3(color, color, sl->diffuse_light[STUDIOLIGHT_Y_POS], clamp_f(normal[1], 0.0, 1.0));
				interp_v3_v3v3(color, color, sl->diffuse_light[STUDIOLIGHT_Y_NEG], clamp_f(-normal[1], 0.0, 1.0));
				interp_v3_v3v3(color, color, sl->diffuse_light[STUDIOLIGHT_Z_POS], clamp_f(normal[2], 0.0, 1.0));

				pixelresult = rgb_to_cpack(
				        linearrgb_to_srgb(color[0]),
				        linearrgb_to_srgb(color[1]),
				        linearrgb_to_srgb(color[2])) | alias_mask;
			}
			rect[offset++] = pixelresult;
		}
	}
	return rect;
}

void BKE_studiolight_ensure_flag(StudioLight *sl, int flag)
{
	if (sl->flag & flag){
		return;
	}

	if ((flag & STUDIOLIGHT_DIFFUSE_LIGHT_CALCULATED)) {
		studiolight_calculate_diffuse_light(sl);
	}
}
