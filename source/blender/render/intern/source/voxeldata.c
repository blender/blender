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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Raul Fernandez Hernandez (Farsthary), Matt Ebb.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/render/intern/source/voxeldata.c
 *  \ingroup render
 */


#include <math.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef WIN32
#include "BLI_winstuff.h"
#endif

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_threads.h"
#include "BLI_voxel.h"
#include "BLI_utildefines.h"

#include "BLF_translation.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_main.h"
#include "BKE_modifier.h"

#include "smoke_API.h"

#include "DNA_texture_types.h"
#include "DNA_object_force.h"
#include "DNA_object_types.h"
#include "DNA_modifier_types.h"
#include "DNA_smoke_types.h"


#include "render_types.h"
#include "texture.h"
#include "voxeldata.h"

static bool is_vd_res_ok(VoxelData *vd)
{
	/* arbitrary large value so corrupt headers don't break */
	const int min = 1, max = 100000;
	return (vd->resol[0] >= min && vd->resol[0] <= max) &&
	       (vd->resol[1] >= min && vd->resol[1] <= max) &&
	       (vd->resol[2] >= min && vd->resol[2] <= max);
}

/* use size_t because the result may exceed INT_MAX */
static size_t vd_resol_size(VoxelData *vd)
{
	return (size_t)vd->resol[0] * (size_t)vd->resol[1] * (size_t)vd->resol[2];
}

static int load_frame_blendervoxel(VoxelData *vd, FILE *fp, int frame)
{	
	const size_t size = vd_resol_size(vd);
	size_t offset = sizeof(VoxelDataHeader);
	
	if (is_vd_res_ok(vd) == false)
		return 0;

	vd->dataset = MEM_mapallocN(sizeof(float) * size, "voxel dataset");
	if (vd->dataset == NULL) return 0;

	if (fseek(fp, frame * size * sizeof(float) + offset, 0) == -1)
		return 0;
	if (fread(vd->dataset, sizeof(float), size, fp) != size)
		return 0;
	
	vd->cachedframe = frame;
	vd->ok = 1;
	return 1;
}

static int load_frame_raw8(VoxelData *vd, FILE *fp, int frame)
{
	const size_t size = vd_resol_size(vd);
	size_t i;
	char *data_c;

	if (is_vd_res_ok(vd) == false)
		return 0;

	vd->dataset = MEM_mapallocN(sizeof(float) * size, "voxel dataset");
	if (vd->dataset == NULL) return 0;
	data_c = (char *)MEM_mallocN(sizeof(char) * size, "temporary voxel file reading storage");
	if (data_c == NULL) {
		MEM_freeN(vd->dataset);
		vd->dataset = NULL;
		return 0;
	}

	if (fseek(fp, (frame - 1) * size * sizeof(char), 0) == -1) {
		MEM_freeN(data_c);
		MEM_freeN(vd->dataset);
		vd->dataset = NULL;
		return 0;
	}
	if (fread(data_c, sizeof(char), size, fp) != size) {
		MEM_freeN(data_c);
		MEM_freeN(vd->dataset);
		vd->dataset = NULL;
		return 0;
	}
	
	for (i = 0; i < size; i++) {
		vd->dataset[i] = (float)data_c[i] / 255.f;
	}
	MEM_freeN(data_c);
	
	vd->cachedframe = frame;
	vd->ok = 1;
	return 1;
}

static void load_frame_image_sequence(VoxelData *vd, Tex *tex)
{
	ImBuf *ibuf;
	Image *ima = tex->ima;
	ImageUser *tiuser = &tex->iuser;
	ImageUser iuser = *(tiuser);
	int x = 0, y = 0, z = 0;
	const float *rf;

	if (!ima) return;
	if (iuser.frames == 0) return;
	
	ima->source = IMA_SRC_SEQUENCE;
	iuser.framenr = 1 + iuser.offset;

	/* find the first valid ibuf and use it to initialize the resolution of the data set */
	/* need to do this in advance so we know how much memory to allocate */
	ibuf = BKE_image_acquire_ibuf(ima, &iuser, NULL);
	while (!ibuf && (iuser.framenr < iuser.frames)) {
		iuser.framenr++;
		ibuf = BKE_image_acquire_ibuf(ima, &iuser, NULL);
	}
	if (!ibuf) return;
	if (!ibuf->rect_float) IMB_float_from_rect(ibuf);
	
	vd->flag |= TEX_VD_STILL;
	vd->resol[0] = ibuf->x;
	vd->resol[1] = ibuf->y;
	vd->resol[2] = iuser.frames;
	vd->dataset = MEM_mapallocN(sizeof(float) * vd_resol_size(vd), "voxel dataset");
	
	for (z = 0; z < iuser.frames; z++) {
		/* get a new ibuf for each frame */
		if (z > 0) {
			iuser.framenr++;
			BKE_image_release_ibuf(ima, ibuf, NULL);
			ibuf = BKE_image_acquire_ibuf(ima, &iuser, NULL);
			if (!ibuf) break;
			if (!ibuf->rect_float) IMB_float_from_rect(ibuf);
		}
		rf = ibuf->rect_float;
		
		for (y = 0; y < ibuf->y; y++) {
			for (x = 0; x < ibuf->x; x++) {
				/* currently averaged to monchrome */
				vd->dataset[BLI_VOXEL_INDEX(x, y, z, vd->resol)] = (rf[0] + rf[1] + rf[2]) / 3.0f;
				rf += 4;
			}
		}
		
		BKE_image_free_anim_ibufs(ima, iuser.framenr);
	}

	BKE_image_release_ibuf(ima, ibuf, NULL);

	vd->ok = 1;
	return;
}

static int read_voxeldata_header(FILE *fp, struct VoxelData *vd)
{
	VoxelDataHeader *h = (VoxelDataHeader *)MEM_mallocN(sizeof(VoxelDataHeader), "voxel data header");
	
	rewind(fp);
	if (fread(h, sizeof(VoxelDataHeader), 1, fp) != 1) {
		MEM_freeN(h);
		return 0;
	}
	
	vd->resol[0] = h->resolX;
	vd->resol[1] = h->resolY;
	vd->resol[2] = h->resolZ;

	MEM_freeN(h);
	return 1;
}

static void init_frame_smoke(VoxelData *vd, int cfra)
{
#ifdef WITH_SMOKE
	Object *ob;
	ModifierData *md;
	
	vd->dataset = NULL;
	if (vd->object == NULL) return;
	ob = vd->object;
	
	/* draw code for smoke */
	if ((md = (ModifierData *)modifiers_findByType(ob, eModifierType_Smoke))) {
		SmokeModifierData *smd = (SmokeModifierData *)md;
		SmokeDomainSettings *sds = smd->domain;
		
		if (sds && sds->fluid) {
			BLI_rw_mutex_lock(sds->fluid_mutex, THREAD_LOCK_READ);

			if (!sds->fluid) {
				BLI_rw_mutex_unlock(sds->fluid_mutex);
				return;
			}

			if (cfra < sds->point_cache[0]->startframe)
				;  /* don't show smoke before simulation starts, this could be made an option in the future */
			else if (vd->smoked_type == TEX_VD_SMOKEHEAT) {
				size_t totRes;
				size_t i;
				float *heat;

				if (!smoke_has_heat(sds->fluid)) {
					BLI_rw_mutex_unlock(sds->fluid_mutex);
					return;
				}

				copy_v3_v3_int(vd->resol, sds->res);
				totRes = vd_resol_size(vd);
				vd->dataset = MEM_mapallocN(sizeof(float) * (totRes), "smoke data");
				/* get heat data */
				heat = smoke_get_heat(sds->fluid);

				/* scale heat values from -2.0-2.0 to 0.0-1.0 */
				for (i = 0; i < totRes; i++) {
					vd->dataset[i] = (heat[i] + 2.0f) / 4.0f;
				}
			}
			else if (vd->smoked_type == TEX_VD_SMOKEVEL) {
				size_t totRes;
				size_t i;
				float *xvel, *yvel, *zvel;

				copy_v3_v3_int(vd->resol, sds->res);
				totRes = vd_resol_size(vd);
				vd->dataset = MEM_mapallocN(sizeof(float) * (totRes), "smoke data");
				/* get velocity data */
				xvel = smoke_get_velocity_x(sds->fluid);
				yvel = smoke_get_velocity_y(sds->fluid);
				zvel = smoke_get_velocity_z(sds->fluid);

				/* map velocities between 0 and 0.3f */
				for (i = 0; i < totRes; i++) {
					vd->dataset[i] = sqrtf(xvel[i] * xvel[i] + yvel[i] * yvel[i] + zvel[i] * zvel[i]) * 3.0f;
				}

			}
			else if (vd->smoked_type == TEX_VD_SMOKEFLAME) {
				size_t totRes;
				float *flame;

				if (sds->flags & MOD_SMOKE_HIGHRES) {
					if (!smoke_turbulence_has_fuel(sds->wt)) {
						BLI_rw_mutex_unlock(sds->fluid_mutex);
						return;
					}
					smoke_turbulence_get_res(sds->wt, vd->resol);
					flame = smoke_turbulence_get_flame(sds->wt);
				}
				else {
					if (!smoke_has_fuel(sds->fluid)) {
						BLI_rw_mutex_unlock(sds->fluid_mutex);
						return;
					}
					copy_v3_v3_int(vd->resol, sds->res);
					flame = smoke_get_flame(sds->fluid);
				}

				/* always store copy, as smoke internal data can change */
				totRes = vd_resol_size(vd);
				vd->dataset = MEM_mapallocN(sizeof(float)*(totRes), "smoke data");
				memcpy(vd->dataset, flame, sizeof(float)*totRes);
			}
			else {
				size_t totCells;
				int depth = 4;
				vd->data_type = TEX_VD_RGBA_PREMUL;

				/* data resolution */
				if (sds->flags & MOD_SMOKE_HIGHRES) {
					smoke_turbulence_get_res(sds->wt, vd->resol);
				}
				else {
					copy_v3_v3_int(vd->resol, sds->res);
				}

				/* TODO: is_vd_res_ok(rvd) doesnt check this resolution */
				totCells = vd_resol_size(vd) * depth;
				/* always store copy, as smoke internal data can change */
				vd->dataset = MEM_mapallocN(sizeof(float) * totCells, "smoke data");

				if (sds->flags & MOD_SMOKE_HIGHRES) {
					if (smoke_turbulence_has_colors(sds->wt)) {
						smoke_turbulence_get_rgba(sds->wt, vd->dataset, 1);
					}
					else {
						smoke_turbulence_get_rgba_from_density(sds->wt, sds->active_color, vd->dataset, 1);
					}
				}
				else {
					if (smoke_has_colors(sds->fluid)) {
						smoke_get_rgba(sds->fluid, vd->dataset, 1);
					}
					else {
						smoke_get_rgba_from_density(sds->fluid, sds->active_color, vd->dataset, 1);
					}
				}
			}  /* end of fluid condition */

			BLI_rw_mutex_unlock(sds->fluid_mutex);
		}
	}
	
	vd->ok = 1;

#else // WITH_SMOKE
	(void)vd;
	(void)cfra;

	vd->dataset = NULL;
#endif
}

void cache_voxeldata(Tex *tex, int scene_frame)
{	
	VoxelData *vd = tex->vd;
	FILE *fp;
	int curframe;
	char path[sizeof(vd->source_path)];
	
	/* only re-cache if dataset needs updating */
	if ((vd->flag & TEX_VD_STILL) || (vd->cachedframe == scene_frame))
		if (vd->ok) return;
	
	/* clear out old cache, ready for new */
	if (vd->dataset) {
		MEM_freeN(vd->dataset);
		vd->dataset = NULL;
	}
	/* reset data_type */
	vd->data_type = TEX_VD_INTENSITY;

	if (vd->flag & TEX_VD_STILL)
		curframe = vd->still_frame;
	else
		curframe = scene_frame;
	
	BLI_strncpy(path, vd->source_path, sizeof(path));
	
	switch (vd->file_format) {
		case TEX_VD_IMAGE_SEQUENCE:
			load_frame_image_sequence(vd, tex);
			return;
		case TEX_VD_SMOKE:
			init_frame_smoke(vd, scene_frame);
			return;
		case TEX_VD_BLENDERVOXEL:
			BLI_path_abs(path, G.main->name);
			if (!BLI_exists(path)) return;
			fp = BLI_fopen(path, "rb");
			if (!fp) return;
			
			if (read_voxeldata_header(fp, vd))
				load_frame_blendervoxel(vd, fp, curframe - 1);

			fclose(fp);
			return;
		case TEX_VD_RAW_8BIT:
			BLI_path_abs(path, G.main->name);
			if (!BLI_exists(path)) return;
			fp = BLI_fopen(path, "rb");
			if (!fp) return;
			
			load_frame_raw8(vd, fp, curframe);
			fclose(fp);
			return;
	}
}

void make_voxeldata(struct Render *re)
{
	Tex *tex;
	
	re->i.infostr = IFACE_("Loading voxel datasets");
	re->stats_draw(re->sdh, &re->i);
	
	/* XXX: should be doing only textures used in this render */
	for (tex = re->main->tex.first; tex; tex = tex->id.next) {
		if (tex->id.us && tex->type == TEX_VOXELDATA) {
			cache_voxeldata(tex, re->r.cfra);
		}
	}
	
	re->i.infostr = NULL;
	re->stats_draw(re->sdh, &re->i);
	
}

int voxeldatatex(struct Tex *tex, const float texvec[3], struct TexResult *texres)
{	 
	VoxelData *vd = tex->vd;
	float co[3], offset[3] = {0.5, 0.5, 0.5}, a;
	int retval = (vd->data_type == TEX_VD_RGBA_PREMUL) ? TEX_RGB : TEX_INT;
	int depth = (vd->data_type == TEX_VD_RGBA_PREMUL) ? 4 : 1;
	int ch;

	if (vd->dataset == NULL) {
		texres->tin = 0.0f;
		return 0;
	}
	
	/* scale lookup from 0.0-1.0 (original location) to -1.0, 1.0, consistent with image texture tex coords */
	/* in implementation this works backwards, bringing sample locations from -1.0, 1.0
	 * to the range 0.0, 1.0, before looking up in the voxel structure. */
	copy_v3_v3(co, texvec);
	mul_v3_fl(co, 0.5f);
	add_v3_v3(co, offset);

	/* co is now in the range 0.0, 1.0 */
	switch (vd->extend) {
		case TEX_CLIP:
		{
			if ((co[0] < 0.f || co[0] > 1.f) || (co[1] < 0.f || co[1] > 1.f) || (co[2] < 0.f || co[2] > 1.f)) {
				texres->tin = 0.f;
				return retval;
			}
			break;
		}
		case TEX_REPEAT:
		{
			co[0] = co[0] - floorf(co[0]);
			co[1] = co[1] - floorf(co[1]);
			co[2] = co[2] - floorf(co[2]);
			break;
		}
		case TEX_EXTEND:
		{
			CLAMP(co[0], 0.f, 1.f);
			CLAMP(co[1], 0.f, 1.f);
			CLAMP(co[2], 0.f, 1.f);
			break;
		}
	}

	for (ch = 0; ch < depth; ch++) {
		float *dataset = vd->dataset + ch*vd->resol[0]*vd->resol[1]*vd->resol[2];
		float *result = &texres->tin;

		if (vd->data_type == TEX_VD_RGBA_PREMUL) {
			switch (ch) {
				case 0:
					result = &texres->tr;
					break;
				case 1:
					result = &texres->tg;
					break;
				case 2:
					result = &texres->tb;
					break;
			}
		}

		switch (vd->interp_type) {
			case TEX_VD_NEARESTNEIGHBOR:
				*result = BLI_voxel_sample_nearest(dataset, vd->resol, co);
				break;  
			case TEX_VD_LINEAR:
				*result = BLI_voxel_sample_trilinear(dataset, vd->resol, co);
				break;
			case TEX_VD_QUADRATIC:
				*result = BLI_voxel_sample_triquadratic(dataset, vd->resol, co);
				break;
			case TEX_VD_TRICUBIC_CATROM:
			case TEX_VD_TRICUBIC_BSPLINE:
				*result = BLI_voxel_sample_tricubic(dataset, vd->resol, co, (vd->interp_type == TEX_VD_TRICUBIC_BSPLINE));
				break;
		}
	}

	a = texres->tin;
	texres->tin *= vd->int_multiplier;
	BRICONT;
	
	if (vd->data_type == TEX_VD_RGBA_PREMUL) {
		/* unmultiply */
		if (a>0.001f) {
			texres->tr /= a;
			texres->tg /= a;
			texres->tb /= a;
		}
		texres->talpha = 1;
	}
	else {
		texres->tr = texres->tin;
		texres->tg = texres->tin;
		texres->tb = texres->tin;
	}

	texres->ta = texres->tin;
	BRICONTRGB;
	
	return retval;
}
