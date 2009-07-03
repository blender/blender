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

#include "BLI_arithb.h"
#include "BLI_blenlib.h"
#include "BLI_voxel.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_main.h"

#include "DNA_texture_types.h"
#include "render_types.h"
#include "renderdatabase.h"
#include "texture.h"
#include "voxeldata.h"

void load_frame_blendervoxel(FILE *fp, float *F, int size, int frame, int offset)
{	
	fseek(fp,frame*size*sizeof(float)+offset,0);
	fread(F,sizeof(float),size,fp);
}

void load_frame_raw8(FILE *fp, float *F, int size, int frame)
{
	char *tmp;
	int i;
	
	tmp = (char *)MEM_mallocN(sizeof(char)*size, "temporary voxel file reading storage");
	
	fseek(fp,(frame-1)*size*sizeof(char),0);
	fread(tmp, sizeof(char), size, fp);
	
	for (i=0; i<size; i++) {
		F[i] = (float)tmp[i] / 256.f;
	}
	MEM_freeN(tmp);
}

void load_frame_image_sequence(Render *re, VoxelData *vd, Tex *tex)
{
	ImBuf *ibuf;
	Image *ima = tex->ima;
	ImageUser *iuser = &tex->iuser;
	int x=0, y=0, z=0;
	float r, g, b;
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
	
	vd->still = 1;
	vd->resolX = ibuf->x;
	vd->resolY = ibuf->y;
	vd->resolZ = iuser->frames;
	vd->dataset = MEM_mapallocN(sizeof(float)*(vd->resolX)*(vd->resolY)*(vd->resolZ), "voxel dataset");
	
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
				vd->dataset[ V_I(x, y, z, &vd->resolX) ] = (rf[0] + rf[1] + rf[2])*0.333f;
				rf +=4;
			}
		}
		
		BKE_image_free_anim_ibufs(ima, iuser->framenr);
	}
}

void write_voxeldata_header(struct VoxelDataHeader *h, FILE *fp)
{
	fwrite(h,sizeof(struct VoxelDataHeader),1,fp);
}

void read_voxeldata_header(FILE *fp, struct VoxelData *vd)
{
	VoxelDataHeader *h=(VoxelDataHeader *)MEM_mallocN(sizeof(VoxelDataHeader), "voxel data header");
	
	rewind(fp);
	fread(h,sizeof(VoxelDataHeader),1,fp);
	
	vd->resolX=h->resolX;
	vd->resolY=h->resolY;
	vd->resolZ=h->resolZ;

	MEM_freeN(h);
}

void cache_voxeldata(struct Render *re,Tex *tex)
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
	}

	if (!BLI_exists(vd->source_path)) return;
	fp = fopen(vd->source_path,"rb");
	if (!fp) return;

	if (vd->file_format == TEX_VD_BLENDERVOXEL)
		read_voxeldata_header(fp, vd);
	
	size = (vd->resolX)*(vd->resolY)*(vd->resolZ);
	vd->dataset = MEM_mapallocN(sizeof(float)*size, "voxel dataset");
		
	if (vd->still) curframe = vd->still_frame;
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
	
	if(re->scene->r.scemode & R_PREVIEWBUTS)
		return;
	
	re->i.infostr= "Loading voxel datasets";
	re->stats_draw(&re->i);
	
	/* XXX: should be doing only textures used in this render */
	for (tex= G.main->tex.first; tex; tex= tex->id.next) {
		if(tex->id.us && tex->type==TEX_VOXELDATA) {
			cache_voxeldata(re, tex);
		}
	}
	
	re->i.infostr= NULL;
	re->stats_draw(&re->i);
	
}

static void free_voxeldata_one(Render *re, Tex *tex)
{
	VoxelData *vd = tex->vd;
	
	if (vd->dataset) {
		MEM_freeN(vd->dataset);
		vd->dataset = NULL;
	}
}


void free_voxeldata(Render *re)
{
	Tex *tex;
	
	if(re->scene->r.scemode & R_PREVIEWBUTS)
		return;
	
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
	VecCopyf(co, texvec);
	VecMulf(co, 0.5f);
	VecAddf(co, co, offset);

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
			texres->tin = voxel_sample_nearest(vd->dataset, &vd->resolX, co);
			break;  
		case TEX_VD_LINEAR:
			texres->tin = voxel_sample_trilinear(vd->dataset, &vd->resolX, co);
			break;					
		case TEX_VD_TRICUBIC:
			texres->tin = voxel_sample_tricubic(vd->dataset, &vd->resolX, co);
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


