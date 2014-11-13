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
 * Contributors: 2004/2005/2006 Blender Foundation, full recode
 * Contributors: Vertex color baking, Copyright 2011 AutoCRC
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/render/intern/source/bake.c
 *  \ingroup render
 */


/* system includes */
#include <stdio.h>
#include <string.h>

/* External modules: */
#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"

#include "DNA_image_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_customdata.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_main.h"
#include "BKE_node.h"
#include "BKE_scene.h"
#include "BKE_library.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"
#include "IMB_colormanagement.h"

#include "RE_bake.h"

/* local include */
#include "rayintersection.h"
#include "rayobject.h"
#include "render_types.h"
#include "renderdatabase.h"
#include "shading.h"
#include "zbuf.h"

#include "PIL_time.h"

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
/* defined in pipeline.c, is hardcopy of active dynamic allocated Render */
/* only to be used here in this file, it's for speed */
extern struct Render R;
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */


/* ************************* bake ************************ */


typedef struct BakeShade {
	ShadeSample ssamp;
	ObjectInstanceRen *obi;
	VlakRen *vlr;
	
	ZSpan *zspan;
	Image *ima;
	ImBuf *ibuf;
	
	int rectx, recty, quad, type, vdone;
	bool ready;

	float dir[3];
	Object *actob;

	/* Output: vertex color or image data. If vcol is not NULL, rect and
	 * rect_float should be NULL. */
	MPoly *mpoly;
	MLoop *mloop;
	MLoopCol *vcol;
	
	unsigned int *rect;
	float *rect_float;

	/* displacement buffer used for normalization with unknown maximal distance */
	bool use_displacement_buffer;
	float *displacement_buffer;
	float displacement_min, displacement_max;
	
	bool use_mask;
	char *rect_mask; /* bake pixel mask */

	float dxco[3], dyco[3];

	short *do_update;

	struct ColorSpace *rect_colorspace;
} BakeShade;

static void bake_set_shade_input(ObjectInstanceRen *obi, VlakRen *vlr, ShadeInput *shi, int quad, int UNUSED(isect), int x, int y, float u, float v)
{
	if (quad)
		shade_input_set_triangle_i(shi, obi, vlr, 0, 2, 3);
	else
		shade_input_set_triangle_i(shi, obi, vlr, 0, 1, 2);
		
	/* cache for shadow */
	shi->samplenr = R.shadowsamplenr[shi->thread]++;

	shi->mask = 0xFFFF; /* all samples */

	shi->u = -u;
	shi->v = -v;
	shi->xs = x;
	shi->ys = y;

	shade_input_set_uv(shi);
	shade_input_set_normals(shi);

	/* no normal flip */
	if (shi->flippednor)
		shade_input_flip_normals(shi);

	/* set up view vector to look right at the surface (note that the normal
	 * is negated in the renderer so it does not need to be done here) */
	shi->view[0] = shi->vn[0];
	shi->view[1] = shi->vn[1];
	shi->view[2] = shi->vn[2];
}

static void bake_shade(void *handle, Object *ob, ShadeInput *shi, int UNUSED(quad), int x, int y, float UNUSED(u), float UNUSED(v), float *tvn, float *ttang)
{
	BakeShade *bs = handle;
	ShadeSample *ssamp = &bs->ssamp;
	ShadeResult shr;
	VlakRen *vlr = shi->vlr;

	shade_input_init_material(shi);

	if (bs->type == RE_BAKE_AO) {
		ambient_occlusion(shi);

		if (R.r.bake_flag & R_BAKE_NORMALIZE) {
			copy_v3_v3(shr.combined, shi->ao);
		}
		else {
			zero_v3(shr.combined);
			environment_lighting_apply(shi, &shr);
		}
	}
	else {
		if (bs->type == RE_BAKE_SHADOW) /* Why do shadows set the color anyhow?, ignore material color for baking */
			shi->r = shi->g = shi->b = 1.0f;

		shade_input_set_shade_texco(shi);
		
		/* only do AO for a full bake (and obviously AO bakes)
		 * AO for light bakes is a leftover and might not be needed */
		if (ELEM(bs->type, RE_BAKE_ALL, RE_BAKE_AO, RE_BAKE_LIGHT))
			shade_samples_do_AO(ssamp);
		
		if (shi->mat->nodetree && shi->mat->use_nodes) {
			ntreeShaderExecTree(shi->mat->nodetree, shi, &shr);
			shi->mat = vlr->mat;  /* shi->mat is being set in nodetree */
		}
		else
			shade_material_loop(shi, &shr);

		if (bs->type == RE_BAKE_NORMALS) {
			float nor[3];

			copy_v3_v3(nor, shi->vn);

			if (R.r.bake_normal_space == R_BAKE_SPACE_CAMERA) {
				/* pass */
			}
			else if (R.r.bake_normal_space == R_BAKE_SPACE_TANGENT) {
				float mat[3][3], imat[3][3];

				/* bitangent */
				if (tvn && ttang) {
					copy_v3_v3(mat[0], ttang);
					cross_v3_v3v3(mat[1], tvn, ttang);
					mul_v3_fl(mat[1], ttang[3]);
					copy_v3_v3(mat[2], tvn);
				}
				else {
					copy_v3_v3(mat[0], shi->nmaptang);
					cross_v3_v3v3(mat[1], shi->nmapnorm, shi->nmaptang);
					mul_v3_fl(mat[1], shi->nmaptang[3]);
					copy_v3_v3(mat[2], shi->nmapnorm);
				}

				invert_m3_m3(imat, mat);
				mul_m3_v3(imat, nor);
			}
			else if (R.r.bake_normal_space == R_BAKE_SPACE_OBJECT)
				mul_mat3_m4_v3(ob->imat_ren, nor);  /* ob->imat_ren includes viewinv! */
			else if (R.r.bake_normal_space == R_BAKE_SPACE_WORLD)
				mul_mat3_m4_v3(R.viewinv, nor);

			normalize_v3(nor); /* in case object has scaling */

			/* The invert of the red channel is to make
			 * the normal map compliant with the outside world.
			 * It needs to be done because in Blender
			 * the normal used in the renderer points inward. It is generated
			 * this way in calc_vertexnormals(). Should this ever change
			 * this negate must be removed.
			 *
			 * there is also a small 1e-5f bias for precision issues. otherwise
			 * we randomly get 127 or 128 for neutral colors. we choose 128
			 * because it is the convention flat color. * */
			shr.combined[0] = (-nor[0]) / 2.0f + 0.5f + 1e-5f;
			shr.combined[1] = nor[1]    / 2.0f + 0.5f + 1e-5f;
			shr.combined[2] = nor[2]    / 2.0f + 0.5f + 1e-5f;
		}
		else if (bs->type == RE_BAKE_TEXTURE) {
			copy_v3_v3(shr.combined, &shi->r);
			shr.alpha = shi->alpha;
		}
		else if (bs->type == RE_BAKE_SHADOW) {
			copy_v3_v3(shr.combined, shr.shad);
			shr.alpha = shi->alpha;
		}
		else if (bs->type == RE_BAKE_SPEC_COLOR) {
			copy_v3_v3(shr.combined, &shi->specr);
			shr.alpha = 1.0f;
		}
		else if (bs->type == RE_BAKE_SPEC_INTENSITY) {
			copy_v3_fl(shr.combined, shi->spec);
			shr.alpha = 1.0f;
		}
		else if (bs->type == RE_BAKE_MIRROR_COLOR) {
			copy_v3_v3(shr.combined, &shi->mirr);
			shr.alpha = 1.0f;
		}
		else if (bs->type == RE_BAKE_MIRROR_INTENSITY) {
			copy_v3_fl(shr.combined, shi->ray_mirror);
			shr.alpha = 1.0f;
		}
		else if (bs->type == RE_BAKE_ALPHA) {
			copy_v3_fl(shr.combined, shi->alpha);
			shr.alpha = 1.0f;
		}
		else if (bs->type == RE_BAKE_EMIT) {
			copy_v3_fl(shr.combined, shi->emit);
			shr.alpha = 1.0f;
		}
		else if (bs->type == RE_BAKE_VERTEX_COLORS) {
			copy_v3_v3(shr.combined, shi->vcol);
			shr.alpha = shi->vcol[3];
		}
	}
	
	if (bs->rect_float && !bs->vcol) {
		float *col = bs->rect_float + 4 * (bs->rectx * y + x);
		copy_v3_v3(col, shr.combined);
		if (bs->type == RE_BAKE_ALL || bs->type == RE_BAKE_TEXTURE || bs->type == RE_BAKE_VERTEX_COLORS) {
			col[3] = shr.alpha;
		}
		else {
			col[3] = 1.0;
		}
	}
	else {
		/* Target is char (LDR). */
		unsigned char col[4];

		if (ELEM(bs->type, RE_BAKE_ALL, RE_BAKE_TEXTURE)) {
			float rgb[3];

			copy_v3_v3(rgb, shr.combined);
			if (R.scene_color_manage) {
				/* Vertex colors have no way to specify color space, so they
				 * default to sRGB. */
				if (!bs->vcol)
					IMB_colormanagement_scene_linear_to_colorspace_v3(rgb, bs->rect_colorspace);
				else
					linearrgb_to_srgb_v3_v3(rgb, rgb);
			}
			rgb_float_to_uchar(col, rgb);
		}
		else {
			rgb_float_to_uchar(col, shr.combined);
		}
		
		if (ELEM(bs->type, RE_BAKE_ALL, RE_BAKE_TEXTURE, RE_BAKE_VERTEX_COLORS)) {
			col[3] = FTOCHAR(shr.alpha);
		}
		else {
			col[3] = 255;
		}

		if (bs->vcol) {
			/* Vertex color baking. Vcol has no useful alpha channel (it exists
			 * but is used only for vertex painting). */
			bs->vcol->r = col[0];
			bs->vcol->g = col[1];
			bs->vcol->b = col[2];
		}
		else {
			unsigned char *imcol = (unsigned char *)(bs->rect + bs->rectx * y + x);
			copy_v4_v4_char((char *)imcol, (char *)col);
		}

	}
	
	if (bs->rect_mask) {
		bs->rect_mask[bs->rectx * y + x] = FILTER_MASK_USED;
	}

	if (bs->do_update) {
		*bs->do_update = true;
	}
}

static void bake_displacement(void *handle, ShadeInput *UNUSED(shi), float dist, int x, int y)
{
	BakeShade *bs = handle;
	float disp;

	if (R.r.bake_flag & R_BAKE_NORMALIZE) {
		if (R.r.bake_maxdist)
			disp = (dist + R.r.bake_maxdist) / (R.r.bake_maxdist * 2);  /* alter the range from [-bake_maxdist, bake_maxdist] to [0, 1]*/
		else
			disp = dist;
	}
	else {
		disp = 0.5f + dist; /* alter the range from [-0.5,0.5] to [0,1]*/
	}

	if (bs->displacement_buffer) {
		float *displacement = bs->displacement_buffer + (bs->rectx * y + x);
		*displacement = disp;
		bs->displacement_min = min_ff(bs->displacement_min, disp);
		bs->displacement_max = max_ff(bs->displacement_max, disp);
	}

	if (bs->rect_float && !bs->vcol) {
		float *col = bs->rect_float + 4 * (bs->rectx * y + x);
		col[0] = col[1] = col[2] = disp;
		col[3] = 1.0f;
	}
	else {
		/* Target is char (LDR). */
		unsigned char col[4];
		col[0] = col[1] = col[2] = FTOCHAR(disp);
		col[3] = 255;

		if (bs->vcol) {
			/* Vertex color baking. Vcol has no useful alpha channel (it exists
			 * but is used only for vertex painting). */
			bs->vcol->r = col[0];
			bs->vcol->g = col[1];
			bs->vcol->b = col[2];
		}
		else {
			const char *imcol = (char *)(bs->rect + bs->rectx * y + x);
			copy_v4_v4_char((char *)imcol, (char *)col);
		}
	}
	if (bs->rect_mask) {
		bs->rect_mask[bs->rectx * y + x] = FILTER_MASK_USED;
	}
}

static int bake_intersect_tree(RayObject *raytree, Isect *isect, float *start, float *dir, float sign, float *hitco, float *dist)
{
	float maxdist;
	int hit;

	/* might be useful to make a user setting for maxsize*/
	if (R.r.bake_maxdist > 0.0f)
		maxdist = R.r.bake_maxdist;
	else
		maxdist = RE_RAYTRACE_MAXDIST + R.r.bake_biasdist;

	/* 'dir' is always normalized */
	madd_v3_v3v3fl(isect->start, start, dir, -R.r.bake_biasdist);

	mul_v3_v3fl(isect->dir, dir, sign);

	isect->dist = maxdist;

	hit = RE_rayobject_raycast(raytree, isect);
	if (hit) {
		madd_v3_v3v3fl(hitco, isect->start, isect->dir, isect->dist);

		*dist = isect->dist;
	}

	return hit;
}

static void bake_set_vlr_dxyco(BakeShade *bs, float *uv1, float *uv2, float *uv3)
{
	VlakRen *vlr = bs->vlr;
	float A, d1, d2, d3, *v1, *v2, *v3;

	if (bs->quad) {
		v1 = vlr->v1->co;
		v2 = vlr->v3->co;
		v3 = vlr->v4->co;
	}
	else {
		v1 = vlr->v1->co;
		v2 = vlr->v2->co;
		v3 = vlr->v3->co;
	}

	/* formula derived from barycentric coordinates:
	 * (uvArea1*v1 + uvArea2*v2 + uvArea3*v3)/uvArea
	 * then taking u and v partial derivatives to get dxco and dyco */
	A = (uv2[0] - uv1[0]) * (uv3[1] - uv1[1]) - (uv3[0] - uv1[0]) * (uv2[1] - uv1[1]);

	if (fabsf(A) > FLT_EPSILON) {
		A = 0.5f / A;

		d1 = uv2[1] - uv3[1];
		d2 = uv3[1] - uv1[1];
		d3 = uv1[1] - uv2[1];
		bs->dxco[0] = (v1[0] * d1 + v2[0] * d2 + v3[0] * d3) * A;
		bs->dxco[1] = (v1[1] * d1 + v2[1] * d2 + v3[1] * d3) * A;
		bs->dxco[2] = (v1[2] * d1 + v2[2] * d2 + v3[2] * d3) * A;

		d1 = uv3[0] - uv2[0];
		d2 = uv1[0] - uv3[0];
		d3 = uv2[0] - uv1[0];
		bs->dyco[0] = (v1[0] * d1 + v2[0] * d2 + v3[0] * d3) * A;
		bs->dyco[1] = (v1[1] * d1 + v2[1] * d2 + v3[1] * d3) * A;
		bs->dyco[2] = (v1[2] * d1 + v2[2] * d2 + v3[2] * d3) * A;
	}
	else {
		bs->dxco[0] = bs->dxco[1] = bs->dxco[2] = 0.0f;
		bs->dyco[0] = bs->dyco[1] = bs->dyco[2] = 0.0f;
	}

	if (bs->obi->flag & R_TRANSFORMED) {
		mul_m3_v3(bs->obi->nmat, bs->dxco);
		mul_m3_v3(bs->obi->nmat, bs->dyco);
	}
}

static void do_bake_shade(void *handle, int x, int y, float u, float v)
{
	BakeShade *bs = handle;
	VlakRen *vlr = bs->vlr;
	ObjectInstanceRen *obi = bs->obi;
	Object *ob = obi->obr->ob;
	float l, *v1, *v2, *v3, tvn[3], ttang[4];
	int quad;
	ShadeSample *ssamp = &bs->ssamp;
	ShadeInput *shi = ssamp->shi;

	/* fast threadsafe break test */
	if (R.test_break(R.tbh))
		return;

	/* setup render coordinates */
	if (bs->quad) {
		v1 = vlr->v1->co;
		v2 = vlr->v3->co;
		v3 = vlr->v4->co;
	}
	else {
		v1 = vlr->v1->co;
		v2 = vlr->v2->co;
		v3 = vlr->v3->co;
	}

	l = 1.0f - u - v;

	/* shrink barycentric coordinates inwards slightly to avoid some issues
	 * where baking selected to active might just miss the other face at the
	 * near the edge of a face */
	if (bs->actob) {
		const float eps = 1.0f - 1e-4f;
		float invsum;

		u = (u - 0.5f) * eps + 0.5f;
		v = (v - 0.5f) * eps + 0.5f;
		l = (l - 0.5f) * eps + 0.5f;

		invsum = 1.0f / (u + v + l);

		u *= invsum;
		v *= invsum;
		l *= invsum;
	}

	/* renderco */
	shi->co[0] = l * v3[0] + u * v1[0] + v * v2[0];
	shi->co[1] = l * v3[1] + u * v1[1] + v * v2[1];
	shi->co[2] = l * v3[2] + u * v1[2] + v * v2[2];

	/* avoid self shadow with vertex bake from adjacent faces [#33729] */
	if ((bs->vcol != NULL) && (bs->actob == NULL)) {
		madd_v3_v3fl(shi->co, vlr->n, 0.0001f);
	}

	if (obi->flag & R_TRANSFORMED)
		mul_m4_v3(obi->mat, shi->co);

	copy_v3_v3(shi->dxco, bs->dxco);
	copy_v3_v3(shi->dyco, bs->dyco);

	quad = bs->quad;
	bake_set_shade_input(obi, vlr, shi, quad, 0, x, y, u, v);

	if (bs->type == RE_BAKE_NORMALS && R.r.bake_normal_space == R_BAKE_SPACE_TANGENT) {
		shade_input_set_shade_texco(shi);
		copy_v3_v3(tvn, shi->nmapnorm);
		copy_v4_v4(ttang, shi->nmaptang);
	}

	/* if we are doing selected to active baking, find point on other face */
	if (bs->actob) {
		Isect isec, minisec;
		float co[3], minco[3], dist, mindist = 0.0f;
		int hit, sign, dir = 1;

		/* intersect with ray going forward and backward*/
		hit = 0;
		memset(&minisec, 0, sizeof(minisec));
		minco[0] = minco[1] = minco[2] = 0.0f;

		copy_v3_v3(bs->dir, shi->vn);

		for (sign = -1; sign <= 1; sign += 2) {
			memset(&isec, 0, sizeof(isec));
			isec.mode = RE_RAY_MIRROR;

			isec.orig.ob   = obi;
			isec.orig.face = vlr;
			isec.userdata = bs->actob;
			isec.check = RE_CHECK_VLR_BAKE;
			isec.skip = RE_SKIP_VLR_NEIGHBOUR;

			if (bake_intersect_tree(R.raytree, &isec, shi->co, shi->vn, sign, co, &dist)) {
				if (!hit || len_squared_v3v3(shi->co, co) < len_squared_v3v3(shi->co, minco)) {
					minisec = isec;
					mindist = dist;
					copy_v3_v3(minco, co);
					hit = 1;
					dir = sign;
				}
			}
		}

		if (ELEM(bs->type, RE_BAKE_DISPLACEMENT, RE_BAKE_DERIVATIVE)) {
			if (hit)
				bake_displacement(handle, shi, (dir == -1) ? mindist : -mindist, x, y);
			else
				bake_displacement(handle, shi, 0.0f, x, y);
			return;
		}

		/* if hit, we shade from the new point, otherwise from point one starting face */
		if (hit) {
			obi = (ObjectInstanceRen *)minisec.hit.ob;
			vlr = (VlakRen *)minisec.hit.face;
			quad = (minisec.isect == 2);
			copy_v3_v3(shi->co, minco);

			u = -minisec.u;
			v = -minisec.v;
			bake_set_shade_input(obi, vlr, shi, quad, 1, x, y, u, v);
		}
	}

	if (bs->type == RE_BAKE_NORMALS && R.r.bake_normal_space == R_BAKE_SPACE_TANGENT)
		bake_shade(handle, ob, shi, quad, x, y, u, v, tvn, ttang);
	else
		bake_shade(handle, ob, shi, quad, x, y, u, v, NULL, NULL);
}

static int get_next_bake_face(BakeShade *bs)
{
	ObjectRen *obr;
	VlakRen *vlr;
	MTFace *tface;
	static int v = 0, vdone = false;
	static ObjectInstanceRen *obi = NULL;

	if (bs == NULL) {
		vlr = NULL;
		v = vdone = false;
		obi = R.instancetable.first;
		return 0;
	}
	
	BLI_lock_thread(LOCK_CUSTOM1);

	for (; obi; obi = obi->next, v = 0) {
		obr = obi->obr;

		/* only allow non instances here */
		if (obr->flag & R_INSTANCEABLE)
			continue;

		for (; v < obr->totvlak; v++) {
			vlr = RE_findOrAddVlak(obr, v);

			if ((bs->actob && bs->actob == obr->ob) || (!bs->actob && (obr->ob->flag & SELECT))) {
				if (R.r.bake_flag & R_BAKE_VCOL) {
					/* Gather face data for vertex color bake */
					Mesh *me;
					int *origindex, vcollayer;
					CustomDataLayer *cdl;

					if (obr->ob->type != OB_MESH)
						continue;
					me = obr->ob->data;

					origindex = RE_vlakren_get_origindex(obr, vlr, 0);
					if (origindex == NULL)
						continue;
					if (*origindex >= me->totpoly) {
						/* Small hack for Array modifier, which gives false
						 * original indices - z0r */
						continue;
					}
#if 0
					/* Only shade selected faces. */
					if ((me->mface[*origindex].flag & ME_FACE_SEL) == 0)
						continue;
#endif

					vcollayer = CustomData_get_render_layer_index(&me->ldata, CD_MLOOPCOL);
					if (vcollayer == -1)
						continue;

					cdl = &me->ldata.layers[vcollayer];
					bs->mpoly = me->mpoly + *origindex;
					bs->vcol = ((MLoopCol *)cdl->data) + bs->mpoly->loopstart;
					bs->mloop = me->mloop + bs->mpoly->loopstart;

					/* Tag mesh for reevaluation. */
					me->id.flag |= LIB_DOIT;
				}
				else {
					Image *ima = NULL;
					ImBuf *ibuf = NULL;
					const float vec_alpha[4] = {0.0f, 0.0f, 0.0f, 0.0f};
					const float vec_solid[4] = {0.0f, 0.0f, 0.0f, 1.0f};
					const float nor_alpha[4] = {0.5f, 0.5f, 1.0f, 0.0f};
					const float nor_solid[4] = {0.5f, 0.5f, 1.0f, 1.0f};
					const float disp_alpha[4] = {0.5f, 0.5f, 0.5f, 0.0f};
					const float disp_solid[4] = {0.5f, 0.5f, 0.5f, 1.0f};

					tface = RE_vlakren_get_tface(obr, vlr, obr->bakemtface, NULL, 0);

					if (!tface || !tface->tpage)
						continue;

					ima = tface->tpage;
					ibuf = BKE_image_acquire_ibuf(ima, NULL, NULL);

					if (ibuf == NULL)
						continue;

					if (ibuf->rect == NULL && ibuf->rect_float == NULL) {
						BKE_image_release_ibuf(ima, ibuf, NULL);
						continue;
					}

					if (ibuf->rect_float && !(ibuf->channels == 0 || ibuf->channels == 4)) {
						BKE_image_release_ibuf(ima, ibuf, NULL);
						continue;
					}
					
					if (ima->flag & IMA_USED_FOR_RENDER) {
						ima->id.flag &= ~LIB_DOIT;
						BKE_image_release_ibuf(ima, ibuf, NULL);
						continue;
					}
					
					/* find the image for the first time? */
					if (ima->id.flag & LIB_DOIT) {
						ima->id.flag &= ~LIB_DOIT;
						
						/* we either fill in float or char, this ensures things go fine */
						if (ibuf->rect_float)
							imb_freerectImBuf(ibuf);
						/* clear image */
						if (R.r.bake_flag & R_BAKE_CLEAR) {
							if (R.r.bake_mode == RE_BAKE_NORMALS && R.r.bake_normal_space == R_BAKE_SPACE_TANGENT)
								IMB_rectfill(ibuf, (ibuf->planes == R_IMF_PLANES_RGBA) ? nor_alpha : nor_solid);
							else if (ELEM(R.r.bake_mode, RE_BAKE_DISPLACEMENT, RE_BAKE_DERIVATIVE))
								IMB_rectfill(ibuf, (ibuf->planes == R_IMF_PLANES_RGBA) ? disp_alpha : disp_solid);
							else
								IMB_rectfill(ibuf, (ibuf->planes == R_IMF_PLANES_RGBA) ? vec_alpha : vec_solid);
						}
						/* might be read by UI to set active image for display */
						R.bakebuf = ima;
					}

					/* Tag image for redraw. */
					ibuf->userflags |= IB_DISPLAY_BUFFER_INVALID;
					BKE_image_release_ibuf(ima, ibuf, NULL);
				}

				bs->obi = obi;
				bs->vlr = vlr;
				bs->vdone++;  /* only for error message if nothing was rendered */
				v++;
				BLI_unlock_thread(LOCK_CUSTOM1);
				return 1;
			}
		}
	}
	
	BLI_unlock_thread(LOCK_CUSTOM1);
	return 0;
}

static void bake_single_vertex(BakeShade *bs, VertRen *vert, float u, float v)
{
	int *origindex, i;
	MLoopCol *basevcol;
	MLoop *mloop;

	origindex = RE_vertren_get_origindex(bs->obi->obr, vert, 0);
	if (!origindex || *origindex == ORIGINDEX_NONE)
		return;

	/* Search for matching vertex index and apply shading. */
	for (i = 0; i < bs->mpoly->totloop; i++) {
		mloop = bs->mloop + i;
		if (mloop->v != *origindex)
			continue;
		basevcol = bs->vcol;
		bs->vcol = basevcol + i;
		do_bake_shade(bs, 0, 0, u, v);
		bs->vcol = basevcol;
		break;
	}
}

/* Bake all vertices of a face. Actually, this still works on a face-by-face
 * basis, and each vertex on each face is shaded. Vertex colors are a property
 * of loops, not vertices. */
static void shade_verts(BakeShade *bs)
{
	VlakRen *vlr = bs->vlr;

	/* Disable baking to image; write to vcol instead. vcol pointer is set in
	 * bake_single_vertex. */
	bs->ima = NULL;
	bs->rect = NULL;
	bs->rect_float = NULL;
	bs->displacement_buffer = NULL;
	bs->displacement_min = FLT_MAX;
	bs->displacement_max = -FLT_MAX;

	bs->quad = 0;

	/* No anti-aliasing for vertices. */
	zero_v3(bs->dxco);
	zero_v3(bs->dyco);

	/* Shade each vertex of the face. u and v are barycentric coordinates; since
	 * we're only interested in vertices, these will be 0 or 1. */
	if ((vlr->flag & R_FACE_SPLIT) == 0) {
		/* Processing triangle face, whole quad, or first half of split quad. */

		bake_single_vertex(bs, bs->vlr->v1, 1.0f, 0.0f);
		bake_single_vertex(bs, bs->vlr->v2, 0.0f, 1.0f);
		bake_single_vertex(bs, bs->vlr->v3, 0.0f, 0.0f);

		if (vlr->v4) {
			bs->quad = 1;
			bake_single_vertex(bs, bs->vlr->v4, 0.0f, 0.0f);
		}
	}
	else {
		/* Processing second half of split quad. Only one vertex to go. */
		if (vlr->flag & R_DIVIDE_24) {
			bake_single_vertex(bs, bs->vlr->v2, 0.0f, 1.0f);
		}
		else {
			bake_single_vertex(bs, bs->vlr->v3, 0.0f, 0.0f);
		}
	}
}

/* already have tested for tface and ima and zspan */
static void shade_tface(BakeShade *bs)
{
	VlakRen *vlr = bs->vlr;
	ObjectInstanceRen *obi = bs->obi;
	ObjectRen *obr = obi->obr;
	MTFace *tface = RE_vlakren_get_tface(obr, vlr, obr->bakemtface, NULL, 0);
	Image *ima = tface->tpage;
	float vec[4][2];
	int a, i1, i2, i3;
	
	/* check valid zspan */
	if (ima != bs->ima) {
		BKE_image_release_ibuf(bs->ima, bs->ibuf, NULL);

		bs->ima = ima;
		bs->ibuf = BKE_image_acquire_ibuf(ima, NULL, NULL);
		/* note, these calls only free/fill contents of zspan struct, not zspan itself */
		zbuf_free_span(bs->zspan);
		zbuf_alloc_span(bs->zspan, bs->ibuf->x, bs->ibuf->y, R.clipcrop);
	}

	bs->rectx = bs->ibuf->x;
	bs->recty = bs->ibuf->y;
	bs->rect = bs->ibuf->rect;
	bs->rect_colorspace = bs->ibuf->rect_colorspace;
	bs->rect_float = bs->ibuf->rect_float;
	bs->vcol = NULL;
	bs->quad = 0;
	bs->rect_mask = NULL;
	bs->displacement_buffer = NULL;

	if (bs->use_mask || bs->use_displacement_buffer) {
		BakeImBufuserData *userdata = bs->ibuf->userdata;
		if (userdata == NULL) {
			BLI_lock_thread(LOCK_CUSTOM1);
			userdata = bs->ibuf->userdata;
			if (userdata == NULL) /* since the thread was locked, its possible another thread alloced the value */
				userdata = MEM_callocN(sizeof(BakeImBufuserData), "BakeImBufuserData");

			if (bs->use_mask) {
				if (userdata->mask_buffer == NULL) {
					userdata->mask_buffer = MEM_callocN(sizeof(char) * bs->rectx * bs->recty, "BakeMask");
				}
			}

			if (bs->use_displacement_buffer) {
				if (userdata->displacement_buffer == NULL) {
					userdata->displacement_buffer = MEM_callocN(sizeof(float) * bs->rectx * bs->recty, "BakeDisp");
				}
			}

			bs->ibuf->userdata = userdata;

			BLI_unlock_thread(LOCK_CUSTOM1);
		}

		bs->rect_mask = userdata->mask_buffer;
		bs->displacement_buffer = userdata->displacement_buffer;
	}
	
	/* get pixel level vertex coordinates */
	for (a = 0; a < 4; a++) {
		/* Note, workaround for pixel aligned UVs which are common and can screw up our intersection tests
		 * where a pixel gets in between 2 faces or the middle of a quad,
		 * camera aligned quads also have this problem but they are less common.
		 * Add a small offset to the UVs, fixes bug #18685 - Campbell */
		vec[a][0] = tface->uv[a][0] * (float)bs->rectx - (0.5f + 0.001f);
		vec[a][1] = tface->uv[a][1] * (float)bs->recty - (0.5f + 0.002f);
	}

	/* UV indices have to be corrected for possible quad->tria splits */
	i1 = 0; i2 = 1; i3 = 2;
	vlr_set_uv_indices(vlr, &i1, &i2, &i3);
	bake_set_vlr_dxyco(bs, vec[i1], vec[i2], vec[i3]);
	zspan_scanconvert(bs->zspan, bs, vec[i1], vec[i2], vec[i3], do_bake_shade);
	
	if (vlr->v4) {
		bs->quad = 1;
		bake_set_vlr_dxyco(bs, vec[0], vec[2], vec[3]);
		zspan_scanconvert(bs->zspan, bs, vec[0], vec[2], vec[3], do_bake_shade);
	}
}

static void *do_bake_thread(void *bs_v)
{
	BakeShade *bs = bs_v;

	while (get_next_bake_face(bs)) {
		if (R.r.bake_flag & R_BAKE_VCOL) {
			shade_verts(bs);
		}
		else {
			shade_tface(bs);
		}
		
		/* fast threadsafe break test */
		if (R.test_break(R.tbh))
			break;

		/* access is not threadsafe but since its just true/false probably ok
		 * only used for interactive baking */
		if (bs->do_update) {
			*bs->do_update = true;
		}
	}
	bs->ready = true;

	BKE_image_release_ibuf(bs->ima, bs->ibuf, NULL);

	return NULL;
}

void RE_bake_ibuf_filter(ImBuf *ibuf, char *mask, const int filter)
{
	/* must check before filtering */
	const short is_new_alpha = (ibuf->planes != R_IMF_PLANES_RGBA) && BKE_imbuf_alpha_test(ibuf);

	/* Margin */
	if (filter) {
		IMB_filter_extend(ibuf, mask, filter);
	}

	/* if the bake results in new alpha then change the image setting */
	if (is_new_alpha) {
		ibuf->planes = R_IMF_PLANES_RGBA;
	}
	else {
		if (filter && ibuf->planes != R_IMF_PLANES_RGBA) {
			/* clear alpha added by filtering */
			IMB_rectfill_alpha(ibuf, 1.0f);
		}
	}
}

void RE_bake_ibuf_normalize_displacement(ImBuf *ibuf, float *displacement, char *mask, float displacement_min, float displacement_max)
{
	int i;
	const float *current_displacement = displacement;
	const char *current_mask = mask;
	float max_distance;

	max_distance = max_ff(fabsf(displacement_min), fabsf(displacement_max));

	for (i = 0; i < ibuf->x * ibuf->y; i++) {
		if (*current_mask == FILTER_MASK_USED) {
			float normalized_displacement;

			if (max_distance > 1e-5f)
				normalized_displacement = (*current_displacement + max_distance) / (max_distance * 2);
			else
				normalized_displacement = 0.5f;

			if (ibuf->rect_float) {
				/* currently baking happens to RGBA only */
				float *fp = ibuf->rect_float + i * 4;
				fp[0] = fp[1] = fp[2] = normalized_displacement;
				fp[3] = 1.0f;
			}

			if (ibuf->rect) {
				unsigned char *cp = (unsigned char *) (ibuf->rect + i);
				cp[0] = cp[1] = cp[2] = FTOCHAR(normalized_displacement);
				cp[3] = 255;
			}
		}

		current_displacement++;
		current_mask++;
	}
}

/* using object selection tags, the faces with UV maps get baked */
/* render should have been setup */
/* returns 0 if nothing was handled */
int RE_bake_shade_all_selected(Render *re, int type, Object *actob, short *do_update, float *progress)
{
	BakeShade *handles;
	ListBase threads;
	Image *ima;
	int a, vdone = false, result = BAKE_RESULT_OK;
	bool use_mask = false;
	bool use_displacement_buffer = false;
	bool do_manage = BKE_scene_check_color_management_enabled(re->scene);
	
	re->scene_color_manage = BKE_scene_check_color_management_enabled(re->scene);
	
	/* initialize render global */
	R = *re;
	R.bakebuf = NULL;

	/* initialize static vars */
	get_next_bake_face(NULL);
	
	/* do we need a mask? */
	if (re->r.bake_filter)
		use_mask = true;

	/* do we need buffer to store displacements  */
	if (ELEM(type, RE_BAKE_DISPLACEMENT, RE_BAKE_DERIVATIVE)) {
		if (((R.r.bake_flag & R_BAKE_NORMALIZE) && R.r.bake_maxdist == 0.0f) ||
		    (type == RE_BAKE_DERIVATIVE))
		{
			use_displacement_buffer = true;
			use_mask = true;
		}
	}

	/* baker uses this flag to detect if image was initialized */
	if ((R.r.bake_flag & R_BAKE_VCOL) == 0) {
		for (ima = G.main->image.first; ima; ima = ima->id.next) {
			ImBuf *ibuf = BKE_image_acquire_ibuf(ima, NULL, NULL);
			ima->id.flag |= LIB_DOIT;
			ima->flag &= ~IMA_USED_FOR_RENDER;
			if (ibuf) {
				ibuf->userdata = NULL; /* use for masking if needed */
			}
			BKE_image_release_ibuf(ima, ibuf, NULL);
		}
	}

	if (R.r.bake_flag & R_BAKE_VCOL) {
		/* untag all meshes */
		BKE_main_id_tag_listbase(&G.main->mesh, false);
	}

	BLI_init_threads(&threads, do_bake_thread, re->r.threads);

	handles = MEM_callocN(sizeof(BakeShade) * re->r.threads, "BakeShade");

	/* get the threads running */
	for (a = 0; a < re->r.threads; a++) {
		/* set defaults in handles */
		handles[a].ssamp.shi[0].lay = re->lay;

		if (type == RE_BAKE_SHADOW) {
			handles[a].ssamp.shi[0].passflag = SCE_PASS_SHADOW;
		}
		else {
			handles[a].ssamp.shi[0].passflag = SCE_PASS_COMBINED;
		}
		handles[a].ssamp.shi[0].combinedflag = ~(SCE_PASS_SPEC);
		handles[a].ssamp.shi[0].thread = a;
		handles[a].ssamp.shi[0].do_manage = do_manage;
		handles[a].ssamp.tot = 1;

		handles[a].type = type;
		handles[a].actob = actob;
		if (R.r.bake_flag & R_BAKE_VCOL)
			handles[a].zspan = NULL;
		else
			handles[a].zspan = MEM_callocN(sizeof(ZSpan), "zspan for bake");
		
		handles[a].use_mask = use_mask;
		handles[a].use_displacement_buffer = use_displacement_buffer;

		handles[a].do_update = do_update; /* use to tell the view to update */
		
		handles[a].displacement_min = FLT_MAX;
		handles[a].displacement_max = -FLT_MAX;

		BLI_insert_thread(&threads, &handles[a]);
	}
	
	/* wait for everything to be done */
	a = 0;
	while (a != re->r.threads) {
		PIL_sleep_ms(50);

		/* calculate progress */
		for (vdone = false, a = 0; a < re->r.threads; a++)
			vdone += handles[a].vdone;
		if (progress)
			*progress = (float)(vdone / (float)re->totvlak);

		for (a = 0; a < re->r.threads; a++) {
			if (handles[a].ready == false) {
				break;
			}
		}
	}

	/* filter and refresh images */
	if ((R.r.bake_flag & R_BAKE_VCOL) == 0) {
		float displacement_min = FLT_MAX, displacement_max = -FLT_MAX;

		if (use_displacement_buffer) {
			for (a = 0; a < re->r.threads; a++) {
				displacement_min = min_ff(displacement_min, handles[a].displacement_min);
				displacement_max = max_ff(displacement_max, handles[a].displacement_max);
			}
		}

		for (ima = G.main->image.first; ima; ima = ima->id.next) {
			if ((ima->id.flag & LIB_DOIT) == 0) {
				ImBuf *ibuf = BKE_image_acquire_ibuf(ima, NULL, NULL);
				BakeImBufuserData *userdata;

				if (ima->flag & IMA_USED_FOR_RENDER)
					result = BAKE_RESULT_FEEDBACK_LOOP;

				if (!ibuf)
					continue;

				userdata = (BakeImBufuserData *)ibuf->userdata;
				if (userdata) {
					if (use_displacement_buffer) {
						if (type == RE_BAKE_DERIVATIVE) {
							float user_scale = (R.r.bake_flag & R_BAKE_USERSCALE) ? R.r.bake_user_scale : -1.0f;
							RE_bake_make_derivative(ibuf, userdata->displacement_buffer, userdata->mask_buffer,
							                        displacement_min, displacement_max, user_scale);
						}
						else {
							RE_bake_ibuf_normalize_displacement(ibuf, userdata->displacement_buffer, userdata->mask_buffer,
							                                    displacement_min, displacement_max);
						}
					}

					RE_bake_ibuf_filter(ibuf, userdata->mask_buffer, re->r.bake_filter);
				}

				ibuf->userflags |= IB_BITMAPDIRTY;
				BKE_image_release_ibuf(ima, ibuf, NULL);
			}
		}

		/* calculate return value */
		for (a = 0; a < re->r.threads; a++) {
			zbuf_free_span(handles[a].zspan);
			MEM_freeN(handles[a].zspan);
		}
	}

	MEM_freeN(handles);
	
	BLI_end_threads(&threads);

	if (vdone == 0) {
		result = BAKE_RESULT_NO_OBJECTS;
	}

	return result;
}

struct Image *RE_bake_shade_get_image(void)
{
	return R.bakebuf;
}

/* **************** Derivative Maps Baker **************** */

static void add_single_heights_margin(const ImBuf *ibuf, const char *mask, float *heights_buffer)
{
	int x, y;

	for (y = 0; y < ibuf->y; y++) {
		for (x = 0; x < ibuf->x; x++) {
			int index = ibuf->x * y + x;

			/* If unassigned pixel, look for neighbors. */
			if (mask[index] != FILTER_MASK_USED) {
				float height_acc = 0;
				int denom = 0;
				int i, j;

				for (j = -1; j <= 1; j++)
					for (i = -1; i <= 1; i++) {
						int w = (i == 0 ? 1 : 0) + (j == 0 ? 1 : 0) + 1;

						if (i != 0 || j != 0) {
							int index2 = 0;
							int x0 = x + i;
							int y0 = y + j;

							CLAMP(x0, 0, ibuf->x - 1);
							CLAMP(y0, 0, ibuf->y - 1);

							index2 = ibuf->x * y0 + x0;

							if (mask[index2] == FILTER_MASK_USED) {
								height_acc += w * heights_buffer[index2];
								denom += w;
							}
						}
					}

				/* Insert final value. */
				if (denom > 0) {
					heights_buffer[index] = height_acc / denom;
				}
			}
		}
	}
}

/* returns user-scale */
float RE_bake_make_derivative(ImBuf *ibuf, float *heights_buffer, const char *mask,
                              const float height_min, const float height_max,
                              const float fmult)
{
	const float delta_height = height_max - height_min;
	const float denom = delta_height > 0.0f ? (8 * delta_height) : 1.0f;
	bool auto_range_fit = fmult <= 0.0f;
	float max_num_deriv = -1.0f;
	int x, y, index;

	/* Need a single margin to calculate good derivatives. */
	add_single_heights_margin(ibuf, mask, heights_buffer);

	if (auto_range_fit) {
		/* If automatic range fitting is enabled. */
		for (y = 0; y < ibuf->y; y++) {
			const int Yu = y == (ibuf->y - 1) ? (ibuf->y - 1) : (y + 1);
			const int Yc = y;
			const int Yd = y == 0 ? 0 : (y - 1);

			for (x = 0; x < ibuf->x; x++) {
				const int Xl = x == 0 ? 0 : (x - 1);
				const int Xc = x;
				const int Xr = x == (ibuf->x - 1) ? (ibuf->x - 1) : (x + 1);

				const float Hcy = heights_buffer[Yc * ibuf->x + Xr] - heights_buffer[Yc * ibuf->x + Xl];
				const float Hu  = heights_buffer[Yu * ibuf->x + Xr] - heights_buffer[Yu * ibuf->x + Xl];
				const float Hd  = heights_buffer[Yd * ibuf->x + Xr] - heights_buffer[Yd * ibuf->x + Xl];

				const float Hl  = heights_buffer[Yu * ibuf->x + Xl] - heights_buffer[Yd * ibuf->x + Xl];
				const float Hcx = heights_buffer[Yu * ibuf->x + Xc] - heights_buffer[Yd * ibuf->x + Xc];
				const float Hr  = heights_buffer[Yu * ibuf->x + Xr] - heights_buffer[Yd * ibuf->x + Xr];

				/* This corresponds to using the sobel kernel on the heights buffer
				 * to obtain the derivative multiplied by 8.
				 */
				const float deriv_x = Hu + 2 * Hcy + Hd;
				const float deriv_y = Hr + 2 * Hcx + Hl;

				/* early out */
				index = ibuf->x * y + x;
				if (mask[index] != FILTER_MASK_USED) {
					continue;
				}

				/* Widen bound. */
				if (fabsf(deriv_x) > max_num_deriv) {
					max_num_deriv = fabsf(deriv_x);
				}

				if (fabsf(deriv_y) > max_num_deriv) {
					max_num_deriv = fabsf(deriv_y);
				}
			}
		}
	}

	/* Output derivatives. */
	auto_range_fit &= (max_num_deriv > 0);
	for (y = 0; y < ibuf->y; y++) {
		const int Yu = y == (ibuf->y - 1) ? (ibuf->y - 1) : (y + 1);
		const int Yc = y;
		const int Yd = y == 0 ? 0 : (y - 1);

		for (x = 0; x < ibuf->x; x++) {
			const int Xl = x == 0 ? 0 : (x - 1);
			const int Xc = x;
			const int Xr = x == (ibuf->x - 1) ? (ibuf->x - 1) : (x + 1);

			const float Hcy = heights_buffer[Yc * ibuf->x + Xr] - heights_buffer[Yc * ibuf->x + Xl];
			const float Hu  = heights_buffer[Yu * ibuf->x + Xr] - heights_buffer[Yu * ibuf->x + Xl];
			const float Hd  = heights_buffer[Yd * ibuf->x + Xr] - heights_buffer[Yd * ibuf->x + Xl];

			const float Hl  = heights_buffer[Yu * ibuf->x + Xl] - heights_buffer[Yd * ibuf->x + Xl];
			const float Hcx = heights_buffer[Yu * ibuf->x + Xc] - heights_buffer[Yd * ibuf->x + Xc];
			const float Hr  = heights_buffer[Yu * ibuf->x + Xr] - heights_buffer[Yd * ibuf->x + Xr];

			/* This corresponds to using the sobel kernel on the heights buffer
			 * to obtain the derivative multiplied by 8.
			 */
			float deriv_x = Hu + 2 * Hcy + Hd;
			float deriv_y = Hr + 2 * Hcx + Hl;

			/* Early out. */
			index = ibuf->x * y + x;
			if (mask[index] != FILTER_MASK_USED) {
				continue;
			}

			if (auto_range_fit) {
				deriv_x /= max_num_deriv;
				deriv_y /= max_num_deriv;
			}
			else {
				deriv_x *= (fmult / denom);
				deriv_y *= (fmult / denom);
			}

			deriv_x = deriv_x * 0.5f + 0.5f;
			deriv_y = deriv_y * 0.5f + 0.5f;

			/* Clamp. */
			CLAMP(deriv_x, 0.0f, 1.0f);
			CLAMP(deriv_y, 0.0f, 1.0f);

			/* Write out derivatives. */
			if (ibuf->rect_float) {
				float *rrgbf = ibuf->rect_float + index * 4;

				rrgbf[0] = deriv_x;
				rrgbf[1] = deriv_y;
				rrgbf[2] = 0.0f;
				rrgbf[3] = 1.0f;
			}
			else {
				char *rrgb = (char *)ibuf->rect + index * 4;

				rrgb[0] = FTOCHAR(deriv_x);
				rrgb[1] = FTOCHAR(deriv_y);
				rrgb[2] = 0;
				rrgb[3] = 255;
			}
		}
	}

	/* Eeturn user-scale (for rendering). */
	return auto_range_fit ? (max_num_deriv / denom) : (fmult > 0.0f ? (1.0f / fmult) : 0.0f);
}
