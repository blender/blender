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
 * Contributor(s): Campbell Barton, pkowal
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/modifiers/intern/MOD_meshcache_mdd.c
 *  \ingroup modifiers
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "BLI_sys_types.h"
#include "BLI_utildefines.h"
#include "BLI_fileops.h"
#include "BLI_math.h"
#ifdef __LITTLE_ENDIAN__
#  include "BLI_endian_switch.h"
#endif

#include "MOD_meshcache_util.h"  /* own include */

#include "DNA_modifier_types.h"

typedef struct MDDHead {
	int frame_tot;
	int verts_tot;
} MDDHead;  /* frames, verts */

static bool meshcache_read_mdd_head(FILE *fp, const int verts_tot,
                                    MDDHead *mdd_head,
                                    const char **err_str)
{
	if (!fread(mdd_head, sizeof(*mdd_head), 1, fp)) {
		*err_str = "Missing header";
		return false;
	}

#ifdef __LITTLE_ENDIAN__
	BLI_endian_switch_int32_array((int *)mdd_head, 2);
#endif

	if (mdd_head->verts_tot != verts_tot) {
		*err_str = "Vertex count mismatch";
		return false;
	}

	if (mdd_head->frame_tot <= 0) {
		*err_str = "Invalid frame total";
		return false;
	}
	/* intentionally dont seek back */

	return true;
}

/**
 * Gets the index frange and factor
 */
static bool meshcache_read_mdd_range(FILE *fp,
                                     const int verts_tot,
                                     const float frame, const char interp,
                                     int r_index_range[2], float *r_factor,
                                     const char **err_str)
{
	MDDHead mdd_head;

	/* first check interpolation and get the vert locations */

	if (meshcache_read_mdd_head(fp, verts_tot, &mdd_head, err_str) == false) {
		return false;
	}

	MOD_meshcache_calc_range(frame, interp, mdd_head.frame_tot, r_index_range, r_factor);

	return true;
}

static bool meshcache_read_mdd_range_from_time(FILE *fp,
                                               const int verts_tot,
                                               const float time, const float UNUSED(fps),
                                               float *r_frame,
                                               const char **err_str)
{
	MDDHead mdd_head;
	int i;
	float f_time, f_time_prev = FLT_MAX;
	float frame;

	if (meshcache_read_mdd_head(fp, verts_tot, &mdd_head, err_str) == false) {
		return false;
	}

	for (i = 0; i < mdd_head.frame_tot; i++) {
		fread(&f_time, sizeof(float), 1, fp);
#ifdef __LITTLE_ENDIAN__
		BLI_endian_switch_float(&f_time);
#endif
		if (f_time >= time) {
			break;
		}
		f_time_prev = f_time;
	}

	if (i == mdd_head.frame_tot) {
		frame = (float)(mdd_head.frame_tot - 1);
	}
	if (UNLIKELY(f_time_prev == FLT_MAX)) {
		frame = 0.0f;
	}
	else {
		const float range  = f_time - f_time_prev;

		if (range <= FRAME_SNAP_EPS) {
			frame = (float)i;
		}
		else {
			frame = (float)(i - 1) + ((time - f_time_prev) / range);
		}
	}

	*r_frame = frame;
	return true;
}

bool MOD_meshcache_read_mdd_index(FILE *fp,
                                  float (*vertexCos)[3], const int verts_tot,
                                  const int index, const float factor,
                                  const char **err_str)
{
	MDDHead mdd_head;

	if (meshcache_read_mdd_head(fp, verts_tot, &mdd_head, err_str) == false) {
		return false;
	}

	if (fseek(fp, mdd_head.frame_tot * sizeof(int), SEEK_CUR) != 0) {
		*err_str = "Header seek failed";
		return false;
	}

	if (fseek(fp, index * mdd_head.verts_tot * sizeof(float) * 3, SEEK_CUR) != 0) {
		*err_str = "Failed to seek frame";
		return false;
	}

	if (factor >= 1.0f) {
#if 1
		float *vco = *vertexCos;
		unsigned int i;
		for (i = mdd_head.verts_tot; i != 0 ; i--, vco += 3) {
			fread(vco, sizeof(float) * 3, 1, fp);

#  ifdef __LITTLE_ENDIAN__
			BLI_endian_switch_float(vco + 0);
			BLI_endian_switch_float(vco + 1);
			BLI_endian_switch_float(vco + 2);
#  endif  /* __LITTLE_ENDIAN__ */
		}
#else
		/* no blending */
		if (!fread(vertexCos, sizeof(float) * 3, mdd_head.verts_tot, f)) {
			*err_str = errno ? strerror(errno) : "Failed to read frame";
			return false;
		}
#  ifdef __LITTLE_ENDIAN__
		BLI_endian_switch_float_array(vertexCos[0], mdd_head.verts_tot * 3);
#  endif
#endif
	}
	else {
		const float ifactor = 1.0f - factor;
		float *vco = *vertexCos;
		unsigned int i;
		for (i = mdd_head.verts_tot; i != 0 ; i--, vco += 3) {
			float tvec[3];
			fread(tvec, sizeof(float) * 3, 1, fp);

#ifdef __LITTLE_ENDIAN__
			BLI_endian_switch_float(tvec + 0);
			BLI_endian_switch_float(tvec + 1);
			BLI_endian_switch_float(tvec + 2);
#endif

			vco[0] = (vco[0] * ifactor) + (tvec[0] * factor);
			vco[1] = (vco[1] * ifactor) + (tvec[1] * factor);
			vco[2] = (vco[2] * ifactor) + (tvec[2] * factor);
		}
	}

	return true;
}

bool MOD_meshcache_read_mdd_frame(FILE *fp,
                                  float (*vertexCos)[3], const int verts_tot, const char interp,
                                  const float frame,
                                  const char **err_str)
{
	int index_range[2];
	float factor;

	if (meshcache_read_mdd_range(fp, verts_tot, frame, interp,
	                             index_range, &factor,  /* read into these values */
	                             err_str) == false)
	{
		return false;
	}

	if (index_range[0] == index_range[1]) {
		/* read single */
		if ((fseek(fp, 0, SEEK_SET) == 0) &&
		    MOD_meshcache_read_mdd_index(fp, vertexCos, verts_tot, index_range[0], 1.0f, err_str))
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
		    MOD_meshcache_read_mdd_index(fp, vertexCos, verts_tot, index_range[0], 1.0f, err_str) &&
		    (fseek(fp, 0, SEEK_SET) == 0) &&
		    MOD_meshcache_read_mdd_index(fp, vertexCos, verts_tot, index_range[1], factor, err_str))
		{
			return true;
		}
		else {
			return false;
		}
	}
}

bool MOD_meshcache_read_mdd_times(const char *filepath,
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
			if (meshcache_read_mdd_range_from_time(fp, verts_tot, time, fps, &frame, err_str) == false) {
				fclose(fp);
				return false;
			}
			rewind(fp);
			break;
		}
		case MOD_MESHCACHE_TIME_FACTOR:
		default:
		{
			MDDHead mdd_head;
			if (meshcache_read_mdd_head(fp, verts_tot, &mdd_head, err_str) == false) {
				fclose(fp);
				return false;
			}

			frame = CLAMPIS(time, 0.0f, 1.0f) * (float)mdd_head.frame_tot;
			rewind(fp);
			break;
		}
	}

	ok = MOD_meshcache_read_mdd_frame(fp, vertexCos, verts_tot, interp, frame, err_str);

	fclose(fp);
	return ok;
}
