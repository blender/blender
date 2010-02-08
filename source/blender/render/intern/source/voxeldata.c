/**
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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

#include <math.h>
#include <stdlib.h>
#include <stdio.h>

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_voxel.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_main.h"
#include "BKE_modifier.h"

#include "smoke_API.h"

#include "DNA_texture_types.h"
#include "DNA_object_types.h"
#include "DNA_modifier_types.h"
#include "DNA_smoke_types.h"


#include "render_types.h"
#include "renderdatabase.h"
#include "texture.h"
#include "voxeldata.h"

static int load_frame_blendervoxel(FILE *fp, float *F, int size, int frame, int offset)
{	
	if(fseek(fp,frame*size*sizeof(float)+offset,0) == -1)
		return 0;
	if(fread(F,sizeof(float),size,fp) != size)
		return 0;
	
	return 1;
}

static int load_frame_raw8(FILE *fp, float *F, int size, int frame)
{
	char *tmp;
	int i;
	
	tmp = (char *)MEM_mallocN(sizeof(char)*size, "temporary voxel file reading storage");
	
	if(fseek(fp,(frame-1)*size*sizeof(char),0) == -1) {
		MEM_freeN(tmp);
		return 0;
	}
	if(fread(tmp, sizeof(char), size, fp) != size) {
		MEM_freeN(tmp);
		return 0;
	}
	
	for (i=0; i<size; i++) {
		F[i] = (float)tmp[i] / 256.f;
	}
	MEM_freeN(tmp);
	return 1;
}

static void load_frame_image_sequence(Render *re, VoxelData *vd, Tex *tex)
{
	ImBuf *ibuf;
	Image *ima = tex->ima;
	ImageUser *iuser = &tex->iuser;
	int x=0, y=0, z=0;
	float *rf;

	if (!ima || !iuser) return;
	
	ima->source = IMA_SRC_SEQUENCE;
	iuser->framenr = 1 + iuser->offset;

	/* find the first valid ibuf and use it to initialise the resolution of the data set */
	/* need to do this in advance so we know how much memory to allocate */
	ibuf= BKE_image_get_ibuf(ima, iuser);
	while (!ibuf && (iuser->framenr < iuser->frames)) {
		iuser->framenr++;
		ibuf= BKE_image_get_ibuf(ima, iuser);
	}
	if (!ibuf) return;
	if (!ibuf->rect_float) IMB_float_from_rect(ibuf);
	
	vd->flag |= TEX_VD_STILL;
	vd->resol[0] = ibuf->x;
	vd->resol[1] = ibuf->y;
	vd->resol[2] = iuser->frames;
	vd->dataset = MEM_mapallocN(sizeof(float)*(vd->resol[0])*(vd->resol[1])*(vd->resol[2]), "voxel dataset");
	
	for (z=0; z < iuser->frames; z++)
	{	
		/* get a new ibuf for each frame */
		if (z > 0) {
			iuser->framenr++;
			ibuf= BKE_image_get_ibuf(ima, iuser);
			if (!ibuf) break;
			if (!ibuf->rect_float) IMB_float_from_rect(ibuf);
		}
		rf = ibuf->rect_float;
		
		for (y=0; y < ibuf->y; y++)
		{
			for (x=0; x < ibuf->x; x++)
			{
				/* currently converted to monchrome */
				vd->dataset[ V_I(x, y, z, vd->resol) ] = (rf[0] + rf[1] + rf[2])*0.333f;
				rf +=4;
			}
		}
		
		BKE_image_free_anim_ibufs(ima, iuser->framenr);
	}
}

static int read_voxeldata_header(FILE *fp, struct VoxelData *vd)
{
	VoxelDataHeader *h=(VoxelDataHeader *)MEM_mallocN(sizeof(VoxelDataHeader), "voxel data header");
	
	rewind(fp);
	if(fread(h,sizeof(VoxelDataHeader),1,fp) != 1) {
		MEM_freeN(h);
		return 0;
	}
	
	vd->resol[0]=h->resolX;
	vd->resol[1]=h->resolY;
	vd->resol[2]=h->resolZ;

	MEM_freeN(h);
	return 1;
}

static void init_frame_smoke(Render *re, VoxelData *vd, Tex *tex)
{
	Object *ob;
	ModifierData *md;
	
	vd->dataset = NULL;
	if (vd->object == NULL)	return;	
	ob= vd->object;
	
	/* draw code for smoke */
	if( (md = (ModifierData *)modifiers_findByType(ob, eModifierType_Smoke)) )
	{
		SmokeModifierData *smd = (SmokeModifierData *)md;

		
		if(smd->domain && smd->domain->fluid) {
			
			if (vd->smoked_type == TEX_VD_SMOKEHEAT) {
				int totRes;
				float *heat;
				int i;

				VECCOPY(vd->resol, smd->domain->res);
				totRes = (vd->resol[0])*(vd->resol[1])*(vd->resol[2]);

				// scaling heat values from -2.0-2.0 to 0.0-1.0
				vd->dataset = MEM_mapallocN(sizeof(float)*(totRes), "smoke data");


				heat = smoke_get_heat(smd->domain->fluid);

				for (i=0; i<totRes; i++)
				{
					vd->dataset[i] = (heat[i]+2.0f)/4.0f;
				}

				//vd->dataset = smoke_get_heat(smd->domain->fluid);
			}
			else if (vd->smoked_type == TEX_VD_SMOKEVEL) {
				int totRes;
				float *xvel, *yvel, *zvel;
				int i;

				VECCOPY(vd->resol, smd->domain->res);
				totRes = (vd->resol[0])*(vd->resol[1])*(vd->resol[2]);

				// scaling heat values from -2.0-2.0 to 0.0-1.0
				vd->dataset = MEM_mapallocN(sizeof(float)*(totRes), "smoke data");

				xvel = smoke_get_velocity_x(smd->domain->fluid);
				yvel = smoke_get_velocity_y(smd->domain->fluid);
				zvel = smoke_get_velocity_z(smd->domain->fluid);

				for (i=0; i<totRes; i++)
				{
					vd->dataset[i] = sqrt(xvel[i]*xvel[i] + yvel[i]*yvel[i] + zvel[i]*zvel[i])*3.0f;
				}

			}
			else {
				if (smd->domain->flags & MOD_SMOKE_HIGHRES) {
					smoke_turbulence_get_res(smd->domain->wt, vd->resol);
					vd->dataset = smoke_turbulence_get_density(smd->domain->wt);
				} else {
					VECCOPY(vd->resol, smd->domain->res);
					vd->dataset = smoke_get_density(smd->domain->fluid);
				}
			} // end of fluid condition
		}
	}
}

static void cache_voxeldata(struct Render *re,Tex *tex)
{	
	VoxelData *vd = tex->vd;
	FILE *fp;
	int size;
	int curframe;
	
	if (!vd) return;
	
	/* image sequence gets special treatment */
	if (vd->file_format == TEX_VD_IMAGE_SEQUENCE) {
		load_frame_image_sequence(re, vd, tex);
		return;
	} else if (vd->file_format == TEX_VD_SMOKE) {
		init_frame_smoke(re, vd, tex);
		return;
	}

	if (!BLI_exists(vd->source_path)) return;
	fp = fopen(vd->source_path,"rb");
	if (!fp) return;

	if (vd->file_format == TEX_VD_BLENDERVOXEL) {
		if(!read_voxeldata_header(fp, vd)) {
			fclose(fp);
			return;
		}
	}
	
	size = (vd->resol[0])*(vd->resol[1])*(vd->resol[2]);
	vd->dataset = MEM_mapallocN(sizeof(float)*size, "voxel dataset");
		
	if (vd->flag & TEX_VD_STILL) curframe = vd->still_frame;
	else curframe = re->r.cfra;
	
	switch(vd->file_format) {
		case TEX_VD_BLENDERVOXEL:
			load_frame_blendervoxel(fp, vd->dataset, size, curframe-1, sizeof(VoxelDataHeader));
			break;
		case TEX_VD_RAW_8BIT:
			load_frame_raw8(fp, vd->dataset, size, curframe);
			break;
	}
	
	fclose(fp);
}

void make_voxeldata(struct Render *re)
{
    Tex *tex;
	
	re->i.infostr= "Loading voxel datasets";
	re->stats_draw(re->sdh, &re->i);
	
	/* XXX: should be doing only textures used in this render */
	for (tex= G.main->tex.first; tex; tex= tex->id.next) {
		if(tex->id.us && tex->type==TEX_VOXELDATA) {
			cache_voxeldata(re, tex);
		}
	}
	
	re->i.infostr= NULL;
	re->stats_draw(re->sdh, &re->i);
	
}

static void free_voxeldata_one(Render *re, Tex *tex)
{
	VoxelData *vd = tex->vd;
	
	if (vd->dataset) {
		if(vd->file_format != TEX_VD_SMOKE)
			MEM_freeN(vd->dataset);
		vd->dataset = NULL;
	}
}


void free_voxeldata(Render *re)
{
	Tex *tex;
	
	for (tex= G.main->tex.first; tex; tex= tex->id.next) {
		if(tex->id.us && tex->type==TEX_VOXELDATA) {
			free_voxeldata_one(re, tex);
		}
	}
}

int voxeldatatex(struct Tex *tex, float *texvec, struct TexResult *texres)
{	 
    int retval = TEX_INT;
	VoxelData *vd = tex->vd;	
	float co[3], offset[3] = {0.5, 0.5, 0.5};

	if ((!vd) || (vd->dataset==NULL)) {
		texres->tin = 0.0f;
		return 0;
	}
	
	/* scale lookup from 0.0-1.0 (original location) to -1.0, 1.0, consistent with image texture tex coords */
	/* in implementation this works backwards, bringing sample locations from -1.0, 1.0
	 * to the range 0.0, 1.0, before looking up in the voxel structure. */
	copy_v3_v3(co, texvec);
	mul_v3_fl(co, 0.5f);
	add_v3_v3v3(co, co, offset);

	/* co is now in the range 0.0, 1.0 */
	switch (tex->extend) {
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
			co[0] = co[0] - floor(co[0]);
			co[1] = co[1] - floor(co[1]);
			co[2] = co[2] - floor(co[2]);
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
	
	switch (vd->interp_type) {
		case TEX_VD_NEARESTNEIGHBOR:
			texres->tin = voxel_sample_nearest(vd->dataset, vd->resol, co);
			break;  
		case TEX_VD_LINEAR:
			texres->tin = voxel_sample_trilinear(vd->dataset, vd->resol, co);
			break;					
		case TEX_VD_QUADRATIC:
			texres->tin = voxel_sample_triquadratic(vd->dataset, vd->resol, co);
			break;
		case TEX_VD_TRICUBIC_CATROM:
		case TEX_VD_TRICUBIC_BSPLINE:
			texres->tin = voxel_sample_tricubic(vd->dataset, vd->resol, co, (vd->interp_type == TEX_VD_TRICUBIC_BSPLINE));
			break;
	}
	
	texres->tin *= vd->int_multiplier;
	BRICONT;
	
	texres->tr = texres->tin;
	texres->tg = texres->tin;
	texres->tb = texres->tin;
	texres->ta = texres->tin;
	BRICONTRGB;
	
	return retval;	
}


