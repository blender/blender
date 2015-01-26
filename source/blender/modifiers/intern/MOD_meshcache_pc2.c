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
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/modifiers/intern/MOD_meshcache_pc2.c
 *  \ingroup modifiers
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "BLI_sys_types.h"
#include "BLI_utildefines.h"
#include "BLI_fileops.h"
#ifdef __BIG_ENDIAN__
#  include "BLI_endian_switch.h"
#endif

#include "MOD_meshcache_util.h"  /* own include */

#include "DNA_modifier_types.h"

typedef struct PC2Head {
	char    header[12];  /* 'POINTCACHE2\0' */
	int     file_version;  /* unused - should be 1 */
	int     verts_tot;
	float   start;
	float   sampling;
	int     frame_tot;
} PC2Head;  /* frames, verts */

static bool meshcache_read_pc2_head(FILE *fp, const int verts_tot,
                                    PC2Head *pc2_head,
                                    const char **err_str)
{
	if (!fread(pc2_head, sizeof(*pc2_head), 1, fp)) {
		*err_str = "Missing header";
		return false;
	}

	if (!STREQ(pc2_head->header, "POINTCACHE2")) {
		*err_str = "Invalid header";
		return false;
	}

#ifdef __BIG_ENDIAN__
	BLI_endian_switch_int32_array(&pc2_head->file_version, (sizeof(*pc2_head) - sizeof(pc2_head->header)) / sizeof(int));
#endif

	if (pc2_head->verts_tot != verts_tot) {
		*err_str = "Vertex count mismatch";
		return false;
	}

	if (pc2_head->frame_tot <= 0) {
		*err_str = "Invalid frame total";
		return false;
	}
	/* intentionally dont seek back */

	return true;
}


/**
 * Gets the index frange and factor
 *
 * currently same as for MDD
 */
static bool meshcache_read_pc2_range(FILE *fp,
                                     const int verts_tot,
                                     const float frame, const char interp,
                                     int r_index_range[2], float *r_factor,
                                     const char **err_str)
{
	PC2Head pc2_head;

	/* first check interpolation and get the vert locations */

	if (meshcache_read_pc2_head(fp, verts_tot, &pc2_head, err_str) == false) {
		return false;
	}

	MOD_meshcache_calc_range(frame, interp, pc2_head.frame_tot, r_index_range, r_factor);

	return true;
}

static bool meshcache_read_pc2_range_from_time(FILE *fp,
                                               const int verts_tot,
                                               const float time, const float fps,
                                               float *r_frame,
                                               const char **err_str)
{
	PC2Head pc2_head;
	float frame;

	if (meshcache_read_pc2_head(fp, verts_tot, &pc2_head, err_str) == false) {
		return false;
	}

	frame = ((time / fps) - pc2_head.start) / pc2_head.sampling;

	if (frame >= pc2_head.frame_tot) {
		frame = (float)(pc2_head.frame_tot - 1);
	}
	else if (frame < 0.0f) {
		frame = 0.0f;
	}

	*r_frame = frame;
	return true;
}

bool MOD_meshcache_read_pc2_index(FILE *fp,
                                  float (*vertexCos)[3], const int verts_tot,
                                  const int index, const float factor,
                                  const char **err_str)
{
	PC2Head pc2_head;

	if (meshcache_read_pc2_head(fp, verts_tot, &pc2_head, err_str) == false) {
		return false;
	}

	if (fseek(fp, index * pc2_head.verts_tot * sizeof(float) * 3, SEEK_CUR) != 0) {
		*err_str = "Failed to seek frame";
		return false;
	}

	if (factor >= 1.0f) {
		float *vco = *vertexCos;
		unsigned int i;
		for (i = pc2_head.verts_tot; i != 0 ; i--, vco += 3) {
			fread(vco, sizeof(float) * 3, 1, fp);

#  ifdef __BIG_ENDIAN__
			BLI_endian_switch_float(vco + 0);
			BLI_endian_switch_float(vco + 1);
			BLI_endian_switch_float(vco + 2);
#  endif  /* __BIG_ENDIAN__ */
		}
	}
	else {
		const float ifactor = 1.0f - factor;
		float *vco = *vertexCos;
		unsigned int i;
		for (i = pc2_head.verts_tot; i != 0 ; i--, vco += 3) {
			float tvec[3];
			fread(tvec, sizeof(float) * 3, 1, fp);

#ifdef __BIG_ENDIAN__
			BLI_endian_switch_float(tvec + 0);
			BLI_endian_switch_float(tvec + 1);
			BLI_endian_switch_float(tvec + 2);
#endif  /* __BIG_ENDIAN__ */

			vco[0] = (vco[0] * ifactor) + (tvec[0] * factor);
			vco[1] = (vco[1] * ifactor) + (tvec[1] * factor);
			vco[2] = (vco[2] * ifactor) + (tvec[2] * factor);
		}
	}

	return true;
}


bool MOD_meshcache_read_pc2_frame(FILE *fp,
                                  float (*vertexCos)[3], const int verts_tot, const char interp,
                                  const float frame,
                                  const char **err_str)
{
	int index_range[2];
	float factor;

	if (meshcache_read_pc2_range(fp, verts_tot, frame, interp,
	                             index_range, &factor,  /* read into these values */
	                             err_str) == false)
	{
		return false;
	}

	if (index_range[0] == index_range[1]) {
		/* read single */
		if ((fseek(fp, 0, SEEK_SET) == 0) &&
		    MOD_meshcache_read_pc2_index(fp, vertexCos, verts_tot, index_range[0], 1.0f, err_str))
		{
			return true;
		}
		else {
			return false;
		}
	}
	else {
		/* read both and interpolate */
		if ((fseek(fp, 0, SEEK_SET) == 0) &&
		    MOD_meshcache_read_pc2_index(fp, vertexCos, verts_tot, index_range[0], 1.0f, err_str) &&
		    (fseek(fp, 0, SEEK_SET) == 0) &&
		    MOD_meshcache_read_pc2_index(fp, vertexCos, verts_tot, index_range[1], factor, err_str))
		{
			return true;
		}
		else {
			return false;
		}
	}
}

bool MOD_meshcache_read_pc2_times(const char *filepath,
                                  float (*vertexCos)[3], const int verts_tot, const char interp,
                                  const float time, const float fps, const char time_mode,
                                  const char **err_str)
{
	float frame;

	FILE *fp = BLI_fopen(filepath, "rb");
	bool ok;

	if (fp == NULL) {
		*err_str = errno ? strerror(errno) : "Unknown error opening file";
		return false;
	}

	switch (time_mode) {
		case MOD_MESHCACHE_TIME_FRAME:
		{
			frame = time;
			break;
		}
		case MOD_MESHCACHE_TIME_SECONDS:
		{
			/* we need to find the closest time */
			if (meshcache_read_pc2_range_from_time(fp, verts_tot, time, fps, &frame, err_str) == false) {
				fclose(fp);
				return false;
			}
			rewind(fp);
			break;
		}
		case MOD_MESHCACHE_TIME_FACTOR:
		default:
		{
			PC2Head pc2_head;
			if (meshcache_read_pc2_head(fp, verts_tot, &pc2_head, err_str) == false) {
				fclose(fp);
				return false;
			}

			frame = CLAMPIS(time, 0.0f, 1.0f) * (float)pc2_head.frame_tot;
			rewind(fp);
			break;
		}
	}

	ok = MOD_meshcache_read_pc2_frame(fp, vertexCos, verts_tot, interp, frame, err_str);

	fclose(fp);
	return ok;
}
