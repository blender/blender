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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2006 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Ben Batt <benbatt@gmail.com>
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 * Implementation of CustomData.
 *
 * BKE_customdata.h contains the function prototypes for this file.
 *
 */

/** \file blender/blenkernel/intern/customdata.c
 *  \ingroup bke
 */
 

#include <math.h>
#include <string.h>
#include <assert.h>

#include "MEM_guardedalloc.h"

#include "DNA_meshdata_types.h"
#include "DNA_ID.h"

#include "BLI_blenlib.h"
#include "BLI_linklist.h"
#include "BLI_math.h"
#include "BLI_mempool.h"
#include "BLI_utildefines.h"

#include "BKE_customdata.h"
#include "BKE_customdata_file.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_utildefines.h"
#include "BKE_multires.h"

/* number of layers to add when growing a CustomData object */
#define CUSTOMDATA_GROW 5

/********************* Layer type information **********************/
typedef struct LayerTypeInfo {
	int size;          /* the memory size of one element of this layer's data */
	const char *structname;  /* name of the struct used, for file writing */
	int structnum;     /* number of structs per element, for file writing */
	const char *defaultname; /* default layer name */

	/* a function to copy count elements of this layer's data
	 * (deep copy if appropriate)
	 * if NULL, memcpy is used
	 */
	void (*copy)(const void *source, void *dest, int count);

	/* a function to free any dynamically allocated components of this
	 * layer's data (note the data pointer itself should not be freed)
	 * size should be the size of one element of this layer's data (e.g.
	 * LayerTypeInfo.size)
	 */
	void (*free)(void *data, int count, int size);

	/* a function to interpolate between count source elements of this
	 * layer's data and store the result in dest
	 * if weights == NULL or sub_weights == NULL, they should default to 1
	 *
	 * weights gives the weight for each element in sources
	 * sub_weights gives the sub-element weights for each element in sources
	 *    (there should be (sub element count)^2 weights per element)
	 * count gives the number of elements in sources
	 */
	void (*interp)(void **sources, float *weights, float *sub_weights,
	               int count, void *dest);

	/* a function to swap the data in corners of the element */
	void (*swap)(void *data, const int *corner_indices);

	/* a function to set a layer's data to default values. if NULL, the
	   default is assumed to be all zeros */
	void (*set_default)(void *data, int count);

    /* functions necassary for geometry collapse*/
	int (*equal)(void *data1, void *data2);
	void (*multiply)(void *data, float fac);
	void (*initminmax)(void *min, void *max);
	void (*add)(void *data1, void *data2);
	void (*dominmax)(void *data1, void *min, void *max);
	void (*copyvalue)(void *source, void *dest);

	/* a function to read data from a cdf file */
	int (*read)(CDataFile *cdf, void *data, int count);

	/* a function to write data to a cdf file */
	int (*write)(CDataFile *cdf, void *data, int count);

	/* a function to determine file size */
	size_t (*filesize)(CDataFile *cdf, void *data, int count);

	/* a function to validate layer contents depending on
	 * sub-elements count
	 */
	void (*validate)(void *source, int sub_elements);
} LayerTypeInfo;

static void layerCopy_mdeformvert(const void *source, void *dest,
								  int count)
{
	int i, size = sizeof(MDeformVert);

	memcpy(dest, source, count * size);

	for(i = 0; i < count; ++i) {
		MDeformVert *dvert = (MDeformVert *)((char *)dest + i * size);

		if(dvert->totweight) {
			MDeformWeight *dw = MEM_callocN(dvert->totweight * sizeof(*dw),
											"layerCopy_mdeformvert dw");

			memcpy(dw, dvert->dw, dvert->totweight * sizeof(*dw));
			dvert->dw = dw;
		}
		else
			dvert->dw = NULL;
	}
}

static void layerFree_mdeformvert(void *data, int count, int size)
{
	int i;

	for(i = 0; i < count; ++i) {
		MDeformVert *dvert = (MDeformVert *)((char *)data + i * size);

		if(dvert->dw) {
			MEM_freeN(dvert->dw);
			dvert->dw = NULL;
			dvert->totweight = 0;
		}
	}
}

static void linklist_free_simple(void *link)
{
	MEM_freeN(link);
}

static void layerInterp_mdeformvert(void **sources, float *weights,
									float *UNUSED(sub_weights), int count, void *dest)
{
	MDeformVert *dvert = dest;
	LinkNode *dest_dw = NULL; /* a list of lists of MDeformWeight pointers */
	LinkNode *node;
	int i, j, totweight;

	if(count <= 0) return;

	/* build a list of unique def_nrs for dest */
	totweight = 0;
	for(i = 0; i < count; ++i) {
		MDeformVert *source = sources[i];
		float interp_weight = weights ? weights[i] : 1.0f;

		for(j = 0; j < source->totweight; ++j) {
			MDeformWeight *dw = &source->dw[j];

			for(node = dest_dw; node; node = node->next) {
				MDeformWeight *tmp_dw = (MDeformWeight *)node->link;

				if(tmp_dw->def_nr == dw->def_nr) {
					tmp_dw->weight += dw->weight * interp_weight;
					break;
				}
			}

			/* if this def_nr is not in the list, add it */
			if(!node) {
				MDeformWeight *tmp_dw = MEM_callocN(sizeof(*tmp_dw),
											"layerInterp_mdeformvert tmp_dw");
				tmp_dw->def_nr = dw->def_nr;
				tmp_dw->weight = dw->weight * interp_weight;
				BLI_linklist_prepend(&dest_dw, tmp_dw);
				totweight++;
			}
		}
	}

	/* now we know how many unique deform weights there are, so realloc */
	if(dvert->dw) MEM_freeN(dvert->dw);

	if(totweight) {
		dvert->dw = MEM_callocN(sizeof(*dvert->dw) * totweight,
								"layerInterp_mdeformvert dvert->dw");
		dvert->totweight = totweight;

		for(i = 0, node = dest_dw; node; node = node->next, ++i)
			dvert->dw[i] = *((MDeformWeight *)node->link);
	}
	else
		memset(dvert, 0, sizeof(*dvert));

	BLI_linklist_free(dest_dw, linklist_free_simple);
}


static void layerInterp_msticky(void **sources, float *weights,
								float *UNUSED(sub_weights), int count, void *dest)
{
	float co[2], w;
	MSticky *mst;
	int i;

	co[0] = co[1] = 0.0f;
	for(i = 0; i < count; i++) {
		w = weights ? weights[i] : 1.0f;
		mst = (MSticky*)sources[i];

		co[0] += w*mst->co[0];
		co[1] += w*mst->co[1];
	}

	mst = (MSticky*)dest;
	mst->co[0] = co[0];
	mst->co[1] = co[1];
}


static void layerCopy_tface(const void *source, void *dest, int count)
{
	const MTFace *source_tf = (const MTFace*)source;
	MTFace *dest_tf = (MTFace*)dest;
	int i;

	for(i = 0; i < count; ++i)
		dest_tf[i] = source_tf[i];
}

static void layerInterp_tface(void **sources, float *weights,
							  float *sub_weights, int count, void *dest)
{
	MTFace *tf = dest;
	int i, j, k;
	float uv[4][2];
	float *sub_weight;

	if(count <= 0) return;

	memset(uv, 0, sizeof(uv));

	sub_weight = sub_weights;
	for(i = 0; i < count; ++i) {
		float weight = weights ? weights[i] : 1;
		MTFace *src = sources[i];

		for(j = 0; j < 4; ++j) {
			if(sub_weights) {
				for(k = 0; k < 4; ++k, ++sub_weight) {
					float w = (*sub_weight) * weight;
					float *tmp_uv = src->uv[k];

					uv[j][0] += tmp_uv[0] * w;
					uv[j][1] += tmp_uv[1] * w;
				}
			} else {
				uv[j][0] += src->uv[j][0] * weight;
				uv[j][1] += src->uv[j][1] * weight;
			}
		}
	}

	*tf = *(MTFace *)sources[0];
	for(j = 0; j < 4; ++j) {
		tf->uv[j][0] = uv[j][0];
		tf->uv[j][1] = uv[j][1];
	}
}

static void layerSwap_tface(void *data, const int *corner_indices)
{
	MTFace *tf = data;
	float uv[4][2];
	static const short pin_flags[4] =
		{ TF_PIN1, TF_PIN2, TF_PIN3, TF_PIN4 };
	static const char sel_flags[4] =
		{ TF_SEL1, TF_SEL2, TF_SEL3, TF_SEL4 };
	short unwrap = tf->unwrap & ~(TF_PIN1 | TF_PIN2 | TF_PIN3 | TF_PIN4);
	char flag = tf->flag & ~(TF_SEL1 | TF_SEL2 | TF_SEL3 | TF_SEL4);
	int j;

	for(j = 0; j < 4; ++j) {
		int source_index = corner_indices[j];

		uv[j][0] = tf->uv[source_index][0];
		uv[j][1] = tf->uv[source_index][1];

		// swap pinning flags around
		if(tf->unwrap & pin_flags[source_index]) {
			unwrap |= pin_flags[j];
		}

		// swap selection flags around
		if(tf->flag & sel_flags[source_index]) {
			flag |= sel_flags[j];
		}
	}

	memcpy(tf->uv, uv, sizeof(tf->uv));
	tf->unwrap = unwrap;
	tf->flag = flag;
}

static void layerDefault_tface(void *data, int count)
{
	static MTFace default_tf = {{{0, 0}, {1, 0}, {1, 1}, {0, 1}}, NULL,
							   0, 0, TF_DYNAMIC|TF_CONVERTED, 0, 0};
	MTFace *tf = (MTFace*)data;
	int i;

	for(i = 0; i < count; i++)
		tf[i] = default_tf;
}

static void layerCopy_propFloat(const void *source, void *dest,
								  int count)
{
	memcpy(dest, source, sizeof(MFloatProperty)*count);
}

static void layerCopy_propInt(const void *source, void *dest,
								  int count)
{
	memcpy(dest, source, sizeof(MIntProperty)*count);
}

static void layerCopy_propString(const void *source, void *dest,
								  int count)
{
	memcpy(dest, source, sizeof(MStringProperty)*count);
}

static void layerCopy_origspace_face(const void *source, void *dest, int count)
{
	const OrigSpaceFace *source_tf = (const OrigSpaceFace*)source;
	OrigSpaceFace *dest_tf = (OrigSpaceFace*)dest;
	int i;

	for(i = 0; i < count; ++i)
		dest_tf[i] = source_tf[i];
}

static void layerInterp_origspace_face(void **sources, float *weights,
							  float *sub_weights, int count, void *dest)
{
	OrigSpaceFace *osf = dest;
	int i, j, k;
	float uv[4][2];
	float *sub_weight;

	if(count <= 0) return;

	memset(uv, 0, sizeof(uv));

	sub_weight = sub_weights;
	for(i = 0; i < count; ++i) {
		float weight = weights ? weights[i] : 1;
		OrigSpaceFace *src = sources[i];

		for(j = 0; j < 4; ++j) {
			if(sub_weights) {
				for(k = 0; k < 4; ++k, ++sub_weight) {
					float w = (*sub_weight) * weight;
					float *tmp_uv = src->uv[k];

					uv[j][0] += tmp_uv[0] * w;
					uv[j][1] += tmp_uv[1] * w;
				}
			} else {
				uv[j][0] += src->uv[j][0] * weight;
				uv[j][1] += src->uv[j][1] * weight;
			}
		}
	}

	*osf = *(OrigSpaceFace *)sources[0];
	for(j = 0; j < 4; ++j) {
		osf->uv[j][0] = uv[j][0];
		osf->uv[j][1] = uv[j][1];
	}
}

static void layerSwap_origspace_face(void *data, const int *corner_indices)
{
	OrigSpaceFace *osf = data;
	float uv[4][2];
	int j;

	for(j = 0; j < 4; ++j) {
		uv[j][0] = osf->uv[corner_indices[j]][0];
		uv[j][1] = osf->uv[corner_indices[j]][1];
	}
	memcpy(osf->uv, uv, sizeof(osf->uv));
}

static void layerDefault_origspace_face(void *data, int count)
{
	static OrigSpaceFace default_osf = {{{0, 0}, {1, 0}, {1, 1}, {0, 1}}};
	OrigSpaceFace *osf = (OrigSpaceFace*)data;
	int i;

	for(i = 0; i < count; i++)
		osf[i] = default_osf;
}

static void layerSwap_mdisps(void *data, const int *ci)
{
	MDisps *s = data;
	float (*d)[3] = NULL;
	int corners, cornersize, S;

	if(s->disps) {
		int nverts= (ci[1] == 3) ? 4 : 3; /* silly way to know vertex count of face */
		corners= multires_mdisp_corners(s);
		cornersize= s->totdisp/corners;

		if(corners!=nverts) {
			/* happens when face changed vertex count in edit mode
			   if it happened, just forgot displacement */

			MEM_freeN(s->disps);
			s->totdisp= (s->totdisp/corners)*nverts;
			s->disps= MEM_callocN(s->totdisp*sizeof(float)*3, "mdisp swap");
			return;
		}

		d= MEM_callocN(sizeof(float) * 3 * s->totdisp, "mdisps swap");

		for(S = 0; S < corners; S++)
			memcpy(d + cornersize*S, s->disps + cornersize*ci[S], cornersize*3*sizeof(float));
		
		MEM_freeN(s->disps);
		s->disps= d;
	}
}

static void layerInterp_mdisps(void **sources, float *UNUSED(weights),
				float *sub_weights, int count, void *dest)
{
	MDisps *d = dest;
	MDisps *s = NULL;
	int st, stl;
	int i, x, y;
	int side, S, dst_corners, src_corners;
	float crn_weight[4][2];
	float (*sw)[4] = (void*)sub_weights;
	float (*disps)[3], (*out)[3];

	/* happens when flipping normals of newly created mesh */
	if(!d->totdisp)
		return;

	s = sources[0];
	dst_corners = multires_mdisp_corners(d);
	src_corners = multires_mdisp_corners(s);

	if(sub_weights && count == 2 && src_corners == 3) {
		src_corners = multires_mdisp_corners(sources[1]);

		/* special case -- converting two triangles to quad */
		if(src_corners == 3 && dst_corners == 4) {
			MDisps tris[2];
			int vindex[4] = {0};

			for(i = 0; i < 2; i++)
				for(y = 0; y < 4; y++)
					for(x = 0; x < 4; x++)
						if(sw[x+i*4][y])
							vindex[x] = y;

			for(i = 0; i < 2; i++) {
				float sw_m4[4][4] = {{0}};
				int a = 7 & ~(1 << vindex[i*2] | 1 << vindex[i*2+1]);

				sw_m4[0][vindex[i*2+1]] = 1;
				sw_m4[1][vindex[i*2]] = 1;

				for(x = 0; x < 3; x++)
					if(a & (1 << x))
						sw_m4[2][x] = 1;

				tris[i] = *((MDisps*)sources[i]);
				tris[i].disps = MEM_dupallocN(tris[i].disps);
				layerInterp_mdisps(&sources[i], NULL, (float*)sw_m4, 1, &tris[i]);
			}

			mdisp_join_tris(d, &tris[0], &tris[1]);

			for(i = 0; i < 2; i++)
				MEM_freeN(tris[i].disps);

			return;
		}
	}

	/* For now, some restrictions on the input */
	if(count != 1 || !sub_weights) {
		for(i = 0; i < d->totdisp; ++i)
			zero_v3(d->disps[i]);

		return;
	}

	/* Initialize the destination */
	disps = MEM_callocN(3*d->totdisp*sizeof(float), "iterp disps");

	side = sqrt(d->totdisp / dst_corners);
	st = (side<<1)-1;
	stl = st - 1;

	sw= (void*)sub_weights;
	for(i = 0; i < 4; ++i) {
		crn_weight[i][0] = 0 * sw[i][0] + stl * sw[i][1] + stl * sw[i][2] + 0 * sw[i][3];
		crn_weight[i][1] = 0 * sw[i][0] + 0 * sw[i][1] + stl * sw[i][2] + stl * sw[i][3];
	}

	multires_mdisp_smooth_bounds(s);

	out = disps;
	for(S = 0; S < dst_corners; S++) {
		float base[2], axis_x[2], axis_y[2];

		mdisp_apply_weight(S, dst_corners, 0, 0, st, crn_weight, &base[0], &base[1]);
		mdisp_apply_weight(S, dst_corners, side-1, 0, st, crn_weight, &axis_x[0], &axis_x[1]);
		mdisp_apply_weight(S, dst_corners, 0, side-1, st, crn_weight, &axis_y[0], &axis_y[1]);

		sub_v2_v2(axis_x, base);
		sub_v2_v2(axis_y, base);
		normalize_v2(axis_x);
		normalize_v2(axis_y);

		for(y = 0; y < side; ++y) {
			for(x = 0; x < side; ++x, ++out) {
				int crn;
				float face_u, face_v, crn_u, crn_v;

				mdisp_apply_weight(S, dst_corners, x, y, st, crn_weight, &face_u, &face_v);
				crn = mdisp_rot_face_to_quad_crn(src_corners, st, face_u, face_v, &crn_u, &crn_v);

				old_mdisps_bilinear((*out), &s->disps[crn*side*side], side, crn_u, crn_v);
				mdisp_flip_disp(crn, dst_corners, axis_x, axis_y, *out);
			}
		}
	}

	MEM_freeN(d->disps);
	d->disps = disps;
}

static void layerCopy_mdisps(const void *source, void *dest, int count)
{
	int i;
	const MDisps *s = source;
	MDisps *d = dest;

	for(i = 0; i < count; ++i) {
		if(s[i].disps) {
			d[i].disps = MEM_dupallocN(s[i].disps);
			d[i].totdisp = s[i].totdisp;
		}
		else {
			d[i].disps = NULL;
			d[i].totdisp = 0;
		}
		
	}
}

static void layerValidate_mdisps(void *data, int sub_elements)
{
	MDisps *disps = data;
	if(disps->disps) {
		int corners = multires_mdisp_corners(disps);

		if(corners != sub_elements) {
			MEM_freeN(disps->disps);
			disps->totdisp = disps->totdisp / corners * sub_elements;
			disps->disps = MEM_callocN(3*disps->totdisp*sizeof(float), "layerValidate_mdisps");
		}
	}
}

static void layerFree_mdisps(void *data, int count, int UNUSED(size))
{
	int i;
	MDisps *d = data;

	for(i = 0; i < count; ++i) {
		if(d[i].disps)
			MEM_freeN(d[i].disps);
		d[i].disps = NULL;
		d[i].totdisp = 0;
	}
}

static int layerRead_mdisps(CDataFile *cdf, void *data, int count)
{
	MDisps *d = data;
	int i;

	for(i = 0; i < count; ++i) {
		if(!d[i].disps)
			d[i].disps = MEM_callocN(sizeof(float)*3*d[i].totdisp, "mdisps read");

		if(!cdf_read_data(cdf, d[i].totdisp*3*sizeof(float), d[i].disps)) {
			printf("failed to read multires displacement %d/%d %d\n", i, count, d[i].totdisp);
			return 0;
		}
	}

	return 1;
}

static int layerWrite_mdisps(CDataFile *cdf, void *data, int count)
{
	MDisps *d = data;
	int i;

	for(i = 0; i < count; ++i) {
		if(!cdf_write_data(cdf, d[i].totdisp*3*sizeof(float), d[i].disps)) {
			printf("failed to write multires displacement %d/%d %d\n", i, count, d[i].totdisp);
			return 0;
		}
	}

	return 1;
}

static size_t layerFilesize_mdisps(CDataFile *UNUSED(cdf), void *data, int count)
{
	MDisps *d = data;
	size_t size = 0;
	int i;

	for(i = 0; i < count; ++i)
		size += d[i].totdisp*3*sizeof(float);

	return size;
}

/* --------- */
static void layerCopyValue_mloopcol(void *source, void *dest)
{
	MLoopCol *m1 = source, *m2 = dest;
	
	m2->r = m1->r;
	m2->g = m1->g;
	m2->b = m1->b;
	m2->a = m1->a;
}

static int layerEqual_mloopcol(void *data1, void *data2)
{
	MLoopCol *m1 = data1, *m2 = data2;
	float r, g, b, a;

	r = m1->r - m2->r;
	g = m1->g - m2->g;
	b = m1->b - m2->b;
	a = m1->a - m2->a;

	return r*r + g*g + b*b + a*a < 0.001;
}

static void layerMultiply_mloopcol(void *data, float fac)
{
	MLoopCol *m = data;

	m->r = (float)m->r * fac;
	m->g = (float)m->g * fac;
	m->b = (float)m->b * fac;
	m->a = (float)m->a * fac;
}

static void layerAdd_mloopcol(void *data1, void *data2)
{
	MLoopCol *m = data1, *m2 = data2;

	m->r += m2->r;
	m->g += m2->g;
	m->b += m2->b;
	m->a += m2->a;
}

static void layerDoMinMax_mloopcol(void *data, void *vmin, void *vmax)
{
	MLoopCol *m = data;
	MLoopCol *min = vmin, *max = vmax;

	if (m->r < min->r) min->r = m->r;
	if (m->g < min->g) min->g = m->g;
	if (m->b < min->b) min->b = m->b;
	if (m->a < min->a) min->a = m->a;
	
	if (m->r > max->r) max->r = m->r;
	if (m->g > max->g) max->g = m->g;
	if (m->b > max->b) max->b = m->b;
	if (m->a > max->a) max->a = m->a;
}

static void layerInitMinMax_mloopcol(void *vmin, void *vmax)
{
	MLoopCol *min = vmin, *max = vmax;

	min->r = 255;
	min->g = 255;
	min->b = 255;
	min->a = 255;

	max->r = 0;
	max->g = 0;
	max->b = 0;
	max->a = 0;
}

static void layerDefault_mloopcol(void *data, int count)
{
	MLoopCol default_mloopcol = {255,255,255,255};
	MLoopCol *mlcol = (MLoopCol*)data;
	int i;
	for(i = 0; i < count; i++)
		mlcol[i] = default_mloopcol;

}

static void layerInterp_mloopcol(void **sources, float *weights,
				float *sub_weights, int count, void *dest)
{
	MLoopCol *mc = dest;
	int i;
	float *sub_weight;
	struct {
		float a;
		float r;
		float g;
		float b;
	} col;
	col.a = col.r = col.g = col.b = 0;

	sub_weight = sub_weights;
	for(i = 0; i < count; ++i){
		float weight = weights ? weights[i] : 1;
		MLoopCol *src = sources[i];
		if(sub_weights){
			col.a += src->a * (*sub_weight) * weight;
			col.r += src->r * (*sub_weight) * weight;
			col.g += src->g * (*sub_weight) * weight;
			col.b += src->b * (*sub_weight) * weight;
			sub_weight++;		
		} else {
			col.a += src->a * weight;
			col.r += src->r * weight;
			col.g += src->g * weight;
			col.b += src->b * weight;
		}
	}
	
	/* Subdivide smooth or fractal can cause problems without clamping
	 * although weights should also not cause this situation */
	CLAMP(col.a, 0.0f, 255.0f);
	CLAMP(col.r, 0.0f, 255.0f);
	CLAMP(col.g, 0.0f, 255.0f);
	CLAMP(col.b, 0.0f, 255.0f);
	
	mc->a = (int)col.a;
	mc->r = (int)col.r;
	mc->g = (int)col.g;
	mc->b = (int)col.b;
}

static void layerCopyValue_mloopuv(void *source, void *dest)
{
	MLoopUV *luv1 = source, *luv2 = dest;
	
	luv2->uv[0] = luv1->uv[0];
	luv2->uv[1] = luv1->uv[1];
}

static int layerEqual_mloopuv(void *data1, void *data2)
{
	MLoopUV *luv1 = data1, *luv2 = data2;
	float u, v;

	u = luv1->uv[0] - luv2->uv[0];
	v = luv1->uv[1] - luv2->uv[1];

	return u*u + v*v < 0.00001;
}

static void layerMultiply_mloopuv(void *data, float fac)
{
	MLoopUV *luv = data;

	luv->uv[0] *= fac;
	luv->uv[1] *= fac;
}

static void layerInitMinMax_mloopuv(void *vmin, void *vmax)
{
	MLoopUV *min = vmin, *max = vmax;

	INIT_MINMAX2(min->uv, max->uv);
}

static void layerDoMinMax_mloopuv(void *data, void *vmin, void *vmax)
{
	MLoopUV *min = vmin, *max = vmax, *luv = data;

	DO_MINMAX2(luv->uv, min->uv, max->uv);
}

static void layerAdd_mloopuv(void *data1, void *data2)
{
	MLoopUV *l1 = data1, *l2 = data2;

	l1->uv[0] += l2->uv[0];
	l1->uv[1] += l2->uv[1];
}

static void layerInterp_mloopuv(void **sources, float *weights,
				float *sub_weights, int count, void *dest)
{
	MLoopUV *mluv = dest;
	int i;
	float *sub_weight;
	struct {
		float u;
		float v;
	}uv;
	uv.u = uv.v = 0.0;

	sub_weight = sub_weights;
	for(i = 0; i < count; ++i){
		float weight = weights ? weights[i] : 1;
		MLoopUV *src = sources[i];
		if(sub_weights){
			uv.u += src->uv[0] * (*sub_weight) * weight;
			uv.v += src->uv[1] * (*sub_weight) * weight;
			sub_weight++;		
		} else {
			uv.u += src->uv[0] * weight;
			uv.v += src->uv[1] * weight;
		}
	}
	mluv->uv[0] = uv.u;
	mluv->uv[1] = uv.v;
}

static void layerInterp_mcol(void **sources, float *weights,
							 float *sub_weights, int count, void *dest)
{
	MCol *mc = dest;
	int i, j, k;
	struct {
		float a;
		float r;
		float g;
		float b;
	} col[4];
	float *sub_weight;

	if(count <= 0) return;

	memset(col, 0, sizeof(col));
	
	sub_weight = sub_weights;
	for(i = 0; i < count; ++i) {
		float weight = weights ? weights[i] : 1;

		for(j = 0; j < 4; ++j) {
			if(sub_weights) {
				MCol *src = sources[i];
				for(k = 0; k < 4; ++k, ++sub_weight, ++src) {
					col[j].a += src->a * (*sub_weight) * weight;
					col[j].r += src->r * (*sub_weight) * weight;
					col[j].g += src->g * (*sub_weight) * weight;
					col[j].b += src->b * (*sub_weight) * weight;
				}
			} else {
				MCol *src = sources[i];
				col[j].a += src[j].a * weight;
				col[j].r += src[j].r * weight;
				col[j].g += src[j].g * weight;
				col[j].b += src[j].b * weight;
			}
		}
	}

	for(j = 0; j < 4; ++j) {
		
		/* Subdivide smooth or fractal can cause problems without clamping
		 * although weights should also not cause this situation */
		CLAMP(col[j].a, 0.0f, 255.0f);
		CLAMP(col[j].r, 0.0f, 255.0f);
		CLAMP(col[j].g, 0.0f, 255.0f);
		CLAMP(col[j].b, 0.0f, 255.0f);
		
		mc[j].a = (int)col[j].a;
		mc[j].r = (int)col[j].r;
		mc[j].g = (int)col[j].g;
		mc[j].b = (int)col[j].b;
	}
}

static void layerSwap_mcol(void *data, const int *corner_indices)
{
	MCol *mcol = data;
	MCol col[4];
	int j;

	for(j = 0; j < 4; ++j)
		col[j] = mcol[corner_indices[j]];

	memcpy(mcol, col, sizeof(col));
}

static void layerDefault_mcol(void *data, int count)
{
	static MCol default_mcol = {255, 255, 255, 255};
	MCol *mcol = (MCol*)data;
	int i;

	for(i = 0; i < 4*count; i++) {
		mcol[i] = default_mcol;
	}
}

static void layerInterp_bweight(void **sources, float *weights,
                                float *UNUSED(sub_weights), int count, void *dest)
{
	float *f = dest;
	float **in = (float **)sources;
	int i;
	
	if(count <= 0) return;

	*f = 0.0f;

	if (weights) {
		for(i = 0; i < count; ++i) {
			*f += *in[i] * weights[i];
		}
	}
	else {
		for(i = 0; i < count; ++i) {
			*f += *in[i];
		}
	}
}

static void layerInterp_shapekey(void **sources, float *weights,
                                 float *UNUSED(sub_weights), int count, void *dest)
{
	float *co = dest;
	float **in = (float **)sources;
	int i;

	if(count <= 0) return;

	zero_v3(co);

	if (weights) {
		for(i = 0; i < count; ++i) {
			madd_v3_v3fl(co, in[i], weights[i]);
		}
	}
	else {
		for(i = 0; i < count; ++i) {
			add_v3_v3(co, in[i]);
		}
	}
}

static const LayerTypeInfo LAYERTYPEINFO[CD_NUMTYPES] = {
	/* 0: CD_MVERT */
	{sizeof(MVert), "MVert", 1, NULL, NULL, NULL, NULL, NULL, NULL},
	/* 1: CD_MSTICKY */
	{sizeof(MSticky), "MSticky", 1, NULL, NULL, NULL, layerInterp_msticky, NULL,
	 NULL},
	/* 2: CD_MDEFORMVERT */
	{sizeof(MDeformVert), "MDeformVert", 1, NULL, layerCopy_mdeformvert,
	 layerFree_mdeformvert, layerInterp_mdeformvert, NULL, NULL},
	/* 3: CD_MEDGE */
	{sizeof(MEdge), "MEdge", 1, NULL, NULL, NULL, NULL, NULL, NULL},
	/* 4: CD_MFACE */
	{sizeof(MFace), "MFace", 1, NULL, NULL, NULL, NULL, NULL, NULL},
	/* 5: CD_MTFACE */
	{sizeof(MTFace), "MTFace", 1, "UVMap", layerCopy_tface, NULL,
	 layerInterp_tface, layerSwap_tface, layerDefault_tface},
	/* 6: CD_MCOL */
	/* 4 MCol structs per face */
	{sizeof(MCol)*4, "MCol", 4, "Col", NULL, NULL, layerInterp_mcol,
	 layerSwap_mcol, layerDefault_mcol},
	/* 7: CD_ORIGINDEX */
	{sizeof(int), "", 0, NULL, NULL, NULL, NULL, NULL, NULL},
	/* 8: CD_NORMAL */
	/* 3 floats per normal vector */
	{sizeof(float)*3, "", 0, NULL, NULL, NULL, NULL, NULL, NULL},
	/* 9: CD_POLYINDEX */
	{sizeof(int), "MIntProperty", 1, NULL, NULL, NULL, NULL, NULL, NULL},
	/* 10: CD_PROP_FLT */
	{sizeof(MFloatProperty), "MFloatProperty",1,"Float", layerCopy_propFloat,NULL,NULL,NULL},
	/* 11: CD_PROP_INT */
	{sizeof(MIntProperty), "MIntProperty",1,"Int",layerCopy_propInt,NULL,NULL,NULL},
	/* 12: CD_PROP_STR */
	{sizeof(MStringProperty), "MStringProperty",1,"String",layerCopy_propString,NULL,NULL,NULL},
	/* 13: CD_ORIGSPACE */
	{sizeof(OrigSpaceFace), "OrigSpaceFace", 1, "UVMap", layerCopy_origspace_face, NULL,
	 layerInterp_origspace_face, layerSwap_origspace_face, layerDefault_origspace_face},
	/* 14: CD_ORCO */
	{sizeof(float)*3, "", 0, NULL, NULL, NULL, NULL, NULL, NULL},
	/* 15: CD_MTEXPOLY */
	{sizeof(MTexPoly), "MTexPoly", 1, "Face Texture", NULL, NULL, NULL, NULL, NULL},
	/* 16: CD_MLOOPUV */
	{sizeof(MLoopUV), "MLoopUV", 1, "UV coord", NULL, NULL, layerInterp_mloopuv, NULL, NULL,
	 layerEqual_mloopuv, layerMultiply_mloopuv, layerInitMinMax_mloopuv, 
	 layerAdd_mloopuv, layerDoMinMax_mloopuv, layerCopyValue_mloopuv},
	/* 17: CD_MLOOPCOL */
	{sizeof(MLoopCol), "MLoopCol", 1, "Col", NULL, NULL, layerInterp_mloopcol, NULL, 
	 layerDefault_mloopcol, layerEqual_mloopcol, layerMultiply_mloopcol, layerInitMinMax_mloopcol, 
	 layerAdd_mloopcol, layerDoMinMax_mloopcol, layerCopyValue_mloopcol},
	/* 18: CD_TANGENT */
	{sizeof(float)*4*4, "", 0, NULL, NULL, NULL, NULL, NULL, NULL},
	/* 19: CD_MDISPS */
	{sizeof(MDisps), "MDisps", 1, NULL, layerCopy_mdisps,
	 layerFree_mdisps, layerInterp_mdisps, layerSwap_mdisps, NULL,
	 NULL, NULL, NULL, NULL, NULL, NULL, 
	 layerRead_mdisps, layerWrite_mdisps, layerFilesize_mdisps, layerValidate_mdisps},
	/* 20: CD_WEIGHT_MCOL */
	{sizeof(MCol)*4, "MCol", 4, "WeightCol", NULL, NULL, layerInterp_mcol,
	 layerSwap_mcol, layerDefault_mcol},
	/* 21: CD_ID_MCOL */
	{sizeof(MCol)*4, "MCol", 4, "IDCol", NULL, NULL, layerInterp_mcol,
	 layerSwap_mcol, layerDefault_mcol},
	/* 22: CD_TEXTURE_MCOL */
	{sizeof(MCol)*4, "MCol", 4, "TexturedCol", NULL, NULL, layerInterp_mcol,
	 layerSwap_mcol, layerDefault_mcol},
	/* 23: CD_CLOTH_ORCO */
	{sizeof(float)*3, "", 0, NULL, NULL, NULL, NULL, NULL, NULL},
	/* 24: CD_RECAST */
	{sizeof(MRecast), "MRecast", 1,"Recast",NULL,NULL,NULL,NULL}

#ifdef USE_BMESH_FORWARD_COMPAT
	,
/* BMESH ONLY */
	/* 25: CD_MPOLY */
	{sizeof(MPoly), "MPoly", 1, "NGon Face", NULL, NULL, NULL, NULL, NULL},
	/* 26: CD_MLOOP */
	{sizeof(MLoop), "MLoop", 1, "NGon Face-Vertex", NULL, NULL, NULL, NULL, NULL},
	/* 27: CD_SHAPE_KEYINDEX */
	{sizeof(int), "", 0, NULL, NULL, NULL, NULL, NULL, NULL},
	/* 28: CD_SHAPEKEY */
	{sizeof(float)*3, "", 0, "ShapeKey", NULL, NULL, layerInterp_shapekey},
	/* 29: CD_BWEIGHT */
	{sizeof(float), "", 0, "BevelWeight", NULL, NULL, layerInterp_bweight},
	/* 30: CD_CREASE */
	{sizeof(float), "", 0, "SubSurfCrease", NULL, NULL, layerInterp_bweight},
	/* 31: CD_WEIGHT_MLOOPCOL */
	{sizeof(MLoopCol), "MLoopCol", 1, "WeightLoopCol", NULL, NULL, layerInterp_mloopcol, NULL,
	 layerDefault_mloopcol, layerEqual_mloopcol, layerMultiply_mloopcol, layerInitMinMax_mloopcol,
	 layerAdd_mloopcol, layerDoMinMax_mloopcol, layerCopyValue_mloopcol},
/* END BMESH ONLY */

#endif /* USE_BMESH_FORWARD_COMPAT */

};

static const char *LAYERTYPENAMES[CD_NUMTYPES] = {
	/*   0-4 */ "CDMVert", "CDMSticky", "CDMDeformVert", "CDMEdge", "CDMFace",
	/*   5-9 */ "CDMTFace", "CDMCol", "CDOrigIndex", "CDNormal", "CDFlags",
	/* 10-14 */ "CDMFloatProperty", "CDMIntProperty","CDMStringProperty", "CDOrigSpace", "CDOrco",
	/* 15-19 */ "CDMTexPoly", "CDMLoopUV", "CDMloopCol", "CDTangent", "CDMDisps",
	/* 20-24 */"CDWeightMCol", "CDIDMCol", "CDTextureMCol", "CDClothOrco", "CDMRecast"

#ifdef USE_BMESH_FORWARD_COMPAT
	,
	/* 25-29 */ "CDMPoly", "CDMLoop", "CDShapeKeyIndex", "CDShapeKey", "CDBevelWeight",
	/* 30-31 */ "CDSubSurfCrease", "CDWeightLoopCol"

#endif /* USE_BMESH_FORWARD_COMPAT */
};

const CustomDataMask CD_MASK_BAREMESH =
	CD_MASK_MVERT | CD_MASK_MEDGE | CD_MASK_MFACE;
const CustomDataMask CD_MASK_MESH =
	CD_MASK_MVERT | CD_MASK_MEDGE | CD_MASK_MFACE |
	CD_MASK_MSTICKY | CD_MASK_MDEFORMVERT | CD_MASK_MTFACE | CD_MASK_MCOL |
	CD_MASK_PROP_FLT | CD_MASK_PROP_INT | CD_MASK_PROP_STR | CD_MASK_MDISPS | CD_MASK_RECAST;
const CustomDataMask CD_MASK_EDITMESH =
	CD_MASK_MSTICKY | CD_MASK_MDEFORMVERT | CD_MASK_MTFACE |
	CD_MASK_MCOL|CD_MASK_PROP_FLT | CD_MASK_PROP_INT | CD_MASK_PROP_STR | CD_MASK_MDISPS | CD_MASK_RECAST;
const CustomDataMask CD_MASK_DERIVEDMESH =
	CD_MASK_MSTICKY | CD_MASK_MDEFORMVERT | CD_MASK_MTFACE |
	CD_MASK_MCOL | CD_MASK_ORIGINDEX | CD_MASK_PROP_FLT | CD_MASK_PROP_INT | CD_MASK_CLOTH_ORCO |
	CD_MASK_PROP_STR | CD_MASK_ORIGSPACE | CD_MASK_ORCO | CD_MASK_TANGENT | CD_MASK_WEIGHT_MCOL | CD_MASK_RECAST;
const CustomDataMask CD_MASK_BMESH = 
	CD_MASK_MSTICKY | CD_MASK_MDEFORMVERT | CD_MASK_PROP_FLT | CD_MASK_PROP_INT | CD_MASK_PROP_STR;
const CustomDataMask CD_MASK_FACECORNERS =
	CD_MASK_MTFACE | CD_MASK_MCOL | CD_MASK_MTEXPOLY | CD_MASK_MLOOPUV |
	CD_MASK_MLOOPCOL;

static const LayerTypeInfo *layerType_getInfo(int type)
{
	if(type < 0 || type >= CD_NUMTYPES) return NULL;

	return &LAYERTYPEINFO[type];
}

static const char *layerType_getName(int type)
{
	if(type < 0 || type >= CD_NUMTYPES) return NULL;

	return LAYERTYPENAMES[type];
}

/********************* CustomData functions *********************/
static void customData_update_offsets(CustomData *data);

static CustomDataLayer *customData_add_layer__internal(CustomData *data,
	int type, int alloctype, void *layerdata, int totelem, const char *name);

void CustomData_update_typemap(CustomData *data)
{
	int i, lasttype = -1;

	/* since we cant do in a pre-processor do here as an assert */
	BLI_assert(sizeof(data->typemap) / sizeof(int) >= CD_NUMTYPES);

	for (i=0; i<CD_NUMTYPES; i++) {
		data->typemap[i] = -1;
	}

	for (i=0; i<data->totlayer; i++) {
		if (data->layers[i].type != lasttype) {
			data->typemap[data->layers[i].type] = i;
		}
		lasttype = data->layers[i].type;
	}
}

void CustomData_merge(const struct CustomData *source, struct CustomData *dest,
					  CustomDataMask mask, int alloctype, int totelem)
{
	/*const LayerTypeInfo *typeInfo;*/
	CustomDataLayer *layer, *newlayer;
	int i, type, number = 0, lasttype = -1, lastactive = 0, lastrender = 0, lastclone = 0, lastmask = 0, lastflag = 0;

	for(i = 0; i < source->totlayer; ++i) {
		layer = &source->layers[i];
		/*typeInfo = layerType_getInfo(layer->type);*/ /*UNUSED*/

		type = layer->type;

		if (type != lasttype) {
			number = 0;
			lastactive = layer->active;
			lastrender = layer->active_rnd;
			lastclone = layer->active_clone;
			lastmask = layer->active_mask;
			lasttype = type;
			lastflag = layer->flag;
		}
		else
			number++;

		if(lastflag & CD_FLAG_NOCOPY) continue;
		else if(!(mask & CD_TYPE_AS_MASK(type))) continue;
		else if(number < CustomData_number_of_layers(dest, type)) continue;

		if((alloctype == CD_ASSIGN) && (lastflag & CD_FLAG_NOFREE))
			newlayer = customData_add_layer__internal(dest, type, CD_REFERENCE,
				layer->data, totelem, layer->name);
		else
			newlayer = customData_add_layer__internal(dest, type, alloctype,
				layer->data, totelem, layer->name);
		
		if(newlayer) {
			newlayer->active = lastactive;
			newlayer->active_rnd = lastrender;
			newlayer->active_clone = lastclone;
			newlayer->active_mask = lastmask;
			newlayer->flag |= lastflag & (CD_FLAG_EXTERNAL|CD_FLAG_IN_MEMORY);
		}
	}

	CustomData_update_typemap(dest);
}

void CustomData_copy(const struct CustomData *source, struct CustomData *dest,
					 CustomDataMask mask, int alloctype, int totelem)
{
	memset(dest, 0, sizeof(*dest));

	if(source->external)
		dest->external= MEM_dupallocN(source->external);

	CustomData_merge(source, dest, mask, alloctype, totelem);
}

static void customData_free_layer__internal(CustomDataLayer *layer, int totelem)
{
	const LayerTypeInfo *typeInfo;

	if(!(layer->flag & CD_FLAG_NOFREE) && layer->data) {
		typeInfo = layerType_getInfo(layer->type);

		if(typeInfo->free)
			typeInfo->free(layer->data, totelem, typeInfo->size);

		if(layer->data)
			MEM_freeN(layer->data);
	}
}

static void CustomData_external_free(CustomData *data)
{
	if(data->external) {
		MEM_freeN(data->external);
		data->external= NULL;
	}
}

void CustomData_free(CustomData *data, int totelem)
{
	int i;

	for(i = 0; i < data->totlayer; ++i)
		customData_free_layer__internal(&data->layers[i], totelem);

	if(data->layers)
		MEM_freeN(data->layers);
	
	CustomData_external_free(data);
	
	memset(data, 0, sizeof(*data));
}

static void customData_update_offsets(CustomData *data)
{
	const LayerTypeInfo *typeInfo;
	int i, offset = 0;

	for(i = 0; i < data->totlayer; ++i) {
		typeInfo = layerType_getInfo(data->layers[i].type);

		data->layers[i].offset = offset;
		offset += typeInfo->size;
	}

	data->totsize = offset;
	CustomData_update_typemap(data);
}

int CustomData_get_layer_index(const CustomData *data, int type)
{
	int i; 

	for(i=0; i < data->totlayer; ++i)
		if(data->layers[i].type == type)
			return i;

	return -1;
}

int CustomData_get_layer_index_n(const struct CustomData *data, int type, int n)
{
	int i = CustomData_get_layer_index(data, type);

	if (i != -1) {
		i = (data->layers[i + n].type == type) ? (i + n) : (-1);
	}

	return i;
}

int CustomData_get_named_layer_index(const CustomData *data, int type, const char *name)
{
	int i;

	for(i=0; i < data->totlayer; ++i)
		if(data->layers[i].type == type && strcmp(data->layers[i].name, name)==0)
			return i;

	return -1;
}

int CustomData_get_active_layer_index(const CustomData *data, int type)
{
	if (!data->totlayer)
		return -1;

	if (data->typemap[type] != -1) {
		return data->typemap[type] + data->layers[data->typemap[type]].active;
	}

	return -1;
}

int CustomData_get_render_layer_index(const CustomData *data, int type)
{
	int i;

	for(i=0; i < data->totlayer; ++i)
		if(data->layers[i].type == type)
			return i + data->layers[i].active_rnd;

	return -1;
}

int CustomData_get_clone_layer_index(const CustomData *data, int type)
{
	int i;

	for(i=0; i < data->totlayer; ++i)
		if(data->layers[i].type == type)
			return i + data->layers[i].active_clone;

	return -1;
}

int CustomData_get_stencil_layer_index(const CustomData *data, int type)
{
	int i;

	for(i=0; i < data->totlayer; ++i)
		if(data->layers[i].type == type)
			return i + data->layers[i].active_mask;

	return -1;
}

int CustomData_get_active_layer(const CustomData *data, int type)
{
	int i;

	for(i=0; i < data->totlayer; ++i)
		if(data->layers[i].type == type)
			return data->layers[i].active;

	return -1;
}

int CustomData_get_render_layer(const CustomData *data, int type)
{
	int i;

	for(i=0; i < data->totlayer; ++i)
		if(data->layers[i].type == type)
			return data->layers[i].active_rnd;

	return -1;
}

int CustomData_get_clone_layer(const CustomData *data, int type)
{
	int i;

	for(i=0; i < data->totlayer; ++i)
		if(data->layers[i].type == type)
			return data->layers[i].active_clone;

	return -1;
}

int CustomData_get_stencil_layer(const CustomData *data, int type)
{
	int i;

	for(i=0; i < data->totlayer; ++i)
		if(data->layers[i].type == type)
			return data->layers[i].active_mask;

	return -1;
}

void CustomData_set_layer_active(CustomData *data, int type, int n)
{
	int i;

	for(i=0; i < data->totlayer; ++i)
		if(data->layers[i].type == type)
			data->layers[i].active = n;
}

void CustomData_set_layer_render(CustomData *data, int type, int n)
{
	int i;

	for(i=0; i < data->totlayer; ++i)
		if(data->layers[i].type == type)
			data->layers[i].active_rnd = n;
}

void CustomData_set_layer_clone(CustomData *data, int type, int n)
{
	int i;

	for(i=0; i < data->totlayer; ++i)
		if(data->layers[i].type == type)
			data->layers[i].active_clone = n;
}

void CustomData_set_layer_stencil(CustomData *data, int type, int n)
{
	int i;

	for(i=0; i < data->totlayer; ++i)
		if(data->layers[i].type == type)
			data->layers[i].active_mask = n;
}

/* for using with an index from CustomData_get_active_layer_index and CustomData_get_render_layer_index */
void CustomData_set_layer_active_index(CustomData *data, int type, int n)
{
	int i;

	for(i=0; i < data->totlayer; ++i)
		if(data->layers[i].type == type)
			data->layers[i].active = n-i;
}

void CustomData_set_layer_render_index(CustomData *data, int type, int n)
{
	int i;

	for(i=0; i < data->totlayer; ++i)
		if(data->layers[i].type == type)
			data->layers[i].active_rnd = n-i;
}

void CustomData_set_layer_clone_index(CustomData *data, int type, int n)
{
	int i;

	for(i=0; i < data->totlayer; ++i)
		if(data->layers[i].type == type)
			data->layers[i].active_clone = n-i;
}

void CustomData_set_layer_stencil_index(CustomData *data, int type, int n)
{
	int i;

	for(i=0; i < data->totlayer; ++i)
		if(data->layers[i].type == type)
			data->layers[i].active_mask = n-i;
}

void CustomData_set_layer_flag(struct CustomData *data, int type, int flag)
{
	int i;

	for(i=0; i < data->totlayer; ++i)
		if(data->layers[i].type == type)
			data->layers[i].flag |= flag;
}

static int customData_resize(CustomData *data, int amount)
{
	CustomDataLayer *tmp = MEM_callocN(sizeof(*tmp)*(data->maxlayer + amount),
									   "CustomData->layers");
	if(!tmp) return 0;

	data->maxlayer += amount;
	if (data->layers) {
		memcpy(tmp, data->layers, sizeof(*tmp) * data->totlayer);
		MEM_freeN(data->layers);
	}
	data->layers = tmp;

	return 1;
}

static CustomDataLayer *customData_add_layer__internal(CustomData *data,
	int type, int alloctype, void *layerdata, int totelem, const char *name)
{
	const LayerTypeInfo *typeInfo= layerType_getInfo(type);
	int size = typeInfo->size * totelem, flag = 0, index = data->totlayer;
	void *newlayerdata;

	if (!typeInfo->defaultname && CustomData_has_layer(data, type))
		return &data->layers[CustomData_get_layer_index(data, type)];

	if((alloctype == CD_ASSIGN) || (alloctype == CD_REFERENCE)) {
		newlayerdata = layerdata;
	}
	else {
		newlayerdata = MEM_callocN(size, layerType_getName(type));
		if(!newlayerdata)
			return NULL;
	}

	if (alloctype == CD_DUPLICATE) {
		if(typeInfo->copy)
			typeInfo->copy(layerdata, newlayerdata, totelem);
		else
			memcpy(newlayerdata, layerdata, size);
	}
	else if (alloctype == CD_DEFAULT) {
		if(typeInfo->set_default)
			typeInfo->set_default((char*)newlayerdata, totelem);
	}
	else if (alloctype == CD_REFERENCE)
		flag |= CD_FLAG_NOFREE;

	if(index >= data->maxlayer) {
		if(!customData_resize(data, CUSTOMDATA_GROW)) {
			if(newlayerdata != layerdata)
				MEM_freeN(newlayerdata);
			return NULL;
		}
	}
	
	data->totlayer++;

	/* keep layers ordered by type */
	for( ; index > 0 && data->layers[index - 1].type > type; --index)
		data->layers[index] = data->layers[index - 1];

	data->layers[index].type = type;
	data->layers[index].flag = flag;
	data->layers[index].data = newlayerdata;

	if(name || (name=typeInfo->defaultname)) {
		BLI_strncpy(data->layers[index].name, name, 32);
		CustomData_set_layer_unique_name(data, index);
	}
	else
		data->layers[index].name[0] = '\0';

	if(index > 0 && data->layers[index-1].type == type) {
		data->layers[index].active = data->layers[index-1].active;
		data->layers[index].active_rnd = data->layers[index-1].active_rnd;
		data->layers[index].active_clone = data->layers[index-1].active_clone;
		data->layers[index].active_mask = data->layers[index-1].active_mask;
	} else {
		data->layers[index].active = 0;
		data->layers[index].active_rnd = 0;
		data->layers[index].active_clone = 0;
		data->layers[index].active_mask = 0;
	}
	
	customData_update_offsets(data);

	return &data->layers[index];
}

void *CustomData_add_layer(CustomData *data, int type, int alloctype,
						   void *layerdata, int totelem)
{
	CustomDataLayer *layer;
	const LayerTypeInfo *typeInfo= layerType_getInfo(type);
	
	layer = customData_add_layer__internal(data, type, alloctype, layerdata,
										   totelem, typeInfo->defaultname);
	CustomData_update_typemap(data);

	if(layer)
		return layer->data;

	return NULL;
}

/*same as above but accepts a name*/
void *CustomData_add_layer_named(CustomData *data, int type, int alloctype,
						   void *layerdata, int totelem, const char *name)
{
	CustomDataLayer *layer;
	
	layer = customData_add_layer__internal(data, type, alloctype, layerdata,
										   totelem, name);
	CustomData_update_typemap(data);

	if(layer)
		return layer->data;

	return NULL;
}


int CustomData_free_layer(CustomData *data, int type, int totelem, int index)
{
	int i;
	
	if (index < 0) return 0;

	customData_free_layer__internal(&data->layers[index], totelem);

	for (i=index+1; i < data->totlayer; ++i)
		data->layers[i-1] = data->layers[i];

	data->totlayer--;

	/* if layer was last of type in array, set new active layer */
	if ((index >= data->totlayer) || (data->layers[index].type != type)) {
		i = CustomData_get_layer_index(data, type);
		
		if (i >= 0)
			for (; i < data->totlayer && data->layers[i].type == type; i++) {
				data->layers[i].active--;
				data->layers[i].active_rnd--;
				data->layers[i].active_clone--;
				data->layers[i].active_mask--;
			}
	}

	if (data->totlayer <= data->maxlayer-CUSTOMDATA_GROW)
		customData_resize(data, -CUSTOMDATA_GROW);

	customData_update_offsets(data);
	CustomData_update_typemap(data);

	return 1;
}

int CustomData_free_layer_active(CustomData *data, int type, int totelem)
{
	int index = 0;
	index = CustomData_get_active_layer_index(data, type);
	if (index < 0) return 0;
	return CustomData_free_layer(data, type, totelem, index);
}


void CustomData_free_layers(CustomData *data, int type, int totelem)
{
	while (CustomData_has_layer(data, type))
		CustomData_free_layer_active(data, type, totelem);
}

int CustomData_has_layer(const CustomData *data, int type)
{
	return (CustomData_get_layer_index(data, type) != -1);
}

int CustomData_number_of_layers(const CustomData *data, int type)
{
	int i, number = 0;

	for(i = 0; i < data->totlayer; i++)
		if(data->layers[i].type == type)
			number++;
	
	return number;
}

void *CustomData_duplicate_referenced_layer(struct CustomData *data, const int type, const int totelem)
{
	CustomDataLayer *layer;
	int layer_index;

	/* get the layer index of the first layer of type */
	layer_index = CustomData_get_active_layer_index(data, type);
	if(layer_index < 0) return NULL;

	layer = &data->layers[layer_index];

	if (layer->flag & CD_FLAG_NOFREE) {
		/* MEM_dupallocN won’t work in case of complex layers, like e.g.
		 * CD_MDEFORMVERT, which has pointers to allocated data...
		 * So in case a custom copy function is defined, use it!
		 */
		const LayerTypeInfo *typeInfo = layerType_getInfo(layer->type);

		if(typeInfo->copy) {
			char *dest_data = MEM_mallocN(typeInfo->size * totelem, "CD duplicate ref layer");
			typeInfo->copy(layer->data, dest_data, totelem);
			layer->data = dest_data;
		}
		else
			layer->data = MEM_dupallocN(layer->data);

		layer->flag &= ~CD_FLAG_NOFREE;
	}

	return layer->data;
}

void *CustomData_duplicate_referenced_layer_named(struct CustomData *data,
												  const int type, const char *name, const int totelem)
{
	CustomDataLayer *layer;
	int layer_index;

	/* get the layer index of the desired layer */
	layer_index = CustomData_get_named_layer_index(data, type, name);
	if(layer_index < 0) return NULL;

	layer = &data->layers[layer_index];

	if (layer->flag & CD_FLAG_NOFREE) {
		/* MEM_dupallocN won’t work in case of complex layers, like e.g.
		 * CD_MDEFORMVERT, which has pointers to allocated data...
		 * So in case a custom copy function is defined, use it!
		 */
		const LayerTypeInfo *typeInfo = layerType_getInfo(layer->type);

		if(typeInfo->copy) {
			char *dest_data = MEM_mallocN(typeInfo->size * totelem, "CD duplicate ref layer");
			typeInfo->copy(layer->data, dest_data, totelem);
			layer->data = dest_data;
		}
		else
			layer->data = MEM_dupallocN(layer->data);

		layer->flag &= ~CD_FLAG_NOFREE;
	}

	return layer->data;
}

int CustomData_is_referenced_layer(struct CustomData *data, int type)
{
	CustomDataLayer *layer;
	int layer_index;

	/* get the layer index of the first layer of type */
	layer_index = CustomData_get_active_layer_index(data, type);
	if(layer_index < 0) return 0;

	layer = &data->layers[layer_index];

	return (layer->flag & CD_FLAG_NOFREE) != 0;
}

void CustomData_free_temporary(CustomData *data, int totelem)
{
	CustomDataLayer *layer;
	int i, j;

	for(i = 0, j = 0; i < data->totlayer; ++i) {
		layer = &data->layers[i];

		if (i != j)
			data->layers[j] = data->layers[i];

		if ((layer->flag & CD_FLAG_TEMPORARY) == CD_FLAG_TEMPORARY)
			customData_free_layer__internal(layer, totelem);
		else
			j++;
	}

	data->totlayer = j;

	if(data->totlayer <= data->maxlayer-CUSTOMDATA_GROW)
		customData_resize(data, -CUSTOMDATA_GROW);

	customData_update_offsets(data);
}

void CustomData_set_only_copy(const struct CustomData *data,
                              CustomDataMask mask)
{
	int i;

	for(i = 0; i < data->totlayer; ++i)
		if(!(mask & CD_TYPE_AS_MASK(data->layers[i].type)))
			data->layers[i].flag |= CD_FLAG_NOCOPY;
}

void CustomData_copy_elements(int type, void *source, void *dest, int count)
{
	const LayerTypeInfo *typeInfo = layerType_getInfo(type);

	if (typeInfo->copy)
		typeInfo->copy(source, dest, count);
	else
		memcpy(dest, source, typeInfo->size*count);
}

void CustomData_copy_data(const CustomData *source, CustomData *dest,
						  int source_index, int dest_index, int count)
{
	const LayerTypeInfo *typeInfo;
	int src_i, dest_i;
	int src_offset;
	int dest_offset;

	/* copies a layer at a time */
	dest_i = 0;
	for(src_i = 0; src_i < source->totlayer; ++src_i) {

		/* find the first dest layer with type >= the source type
		 * (this should work because layers are ordered by type)
		 */
		while(dest_i < dest->totlayer
			  && dest->layers[dest_i].type < source->layers[src_i].type)
			++dest_i;

		/* if there are no more dest layers, we're done */
		if(dest_i >= dest->totlayer) return;

		/* if we found a matching layer, copy the data */
		if(dest->layers[dest_i].type == source->layers[src_i].type) {
			char *src_data = source->layers[src_i].data;
			char *dest_data = dest->layers[dest_i].data;

			typeInfo = layerType_getInfo(source->layers[src_i].type);

			src_offset = source_index * typeInfo->size;
			dest_offset = dest_index * typeInfo->size;
			
			if (!src_data || !dest_data) {
				printf("%s: warning null data for %s type (%p --> %p), skipping\n",
				       __func__, layerType_getName(source->layers[src_i].type),
				       (void *)src_data, (void *)dest_data);
				continue;
			}
			
			if(typeInfo->copy)
				typeInfo->copy(src_data + src_offset,
								dest_data + dest_offset,
								count);
			else
				memcpy(dest_data + dest_offset,
					   src_data + src_offset,
					   count * typeInfo->size);

			/* if there are multiple source & dest layers of the same type,
			 * we don't want to copy all source layers to the same dest, so
			 * increment dest_i
			 */
			++dest_i;
		}
	}
}

void CustomData_free_elem(CustomData *data, int index, int count)
{
	int i;
	const LayerTypeInfo *typeInfo;

	for(i = 0; i < data->totlayer; ++i) {
		if(!(data->layers[i].flag & CD_FLAG_NOFREE)) {
			typeInfo = layerType_getInfo(data->layers[i].type);

			if(typeInfo->free) {
				int offset = typeInfo->size * index;

				typeInfo->free((char *)data->layers[i].data + offset,
							   count, typeInfo->size);
			}
		}
	}
}

#define SOURCE_BUF_SIZE 100

void CustomData_interp(const CustomData *source, CustomData *dest,
					   int *src_indices, float *weights, float *sub_weights,
					   int count, int dest_index)
{
	int src_i, dest_i;
	int dest_offset;
	int j;
	void *source_buf[SOURCE_BUF_SIZE];
	void **sources = source_buf;

	/* slow fallback in case we're interpolating a ridiculous number of
	 * elements
	 */
	if(count > SOURCE_BUF_SIZE)
		sources = MEM_callocN(sizeof(*sources) * count,
							  "CustomData_interp sources");

	/* interpolates a layer at a time */
	dest_i = 0;
	for(src_i = 0; src_i < source->totlayer; ++src_i) {
		const LayerTypeInfo *typeInfo= layerType_getInfo(source->layers[src_i].type);
		if(!typeInfo->interp) continue;

		/* find the first dest layer with type >= the source type
		 * (this should work because layers are ordered by type)
		 */
		while(dest_i < dest->totlayer
			  && dest->layers[dest_i].type < source->layers[src_i].type)
			++dest_i;

		/* if there are no more dest layers, we're done */
		if(dest_i >= dest->totlayer) return;

		/* if we found a matching layer, copy the data */
		if(dest->layers[dest_i].type == source->layers[src_i].type) {
			void *src_data = source->layers[src_i].data;

			for(j = 0; j < count; ++j)
				sources[j] = (char *)src_data
							 + typeInfo->size * src_indices[j];

			dest_offset = dest_index * typeInfo->size;

			typeInfo->interp(sources, weights, sub_weights, count,
						   (char *)dest->layers[dest_i].data + dest_offset);

			/* if there are multiple source & dest layers of the same type,
			 * we don't want to copy all source layers to the same dest, so
			 * increment dest_i
			 */
			++dest_i;
		}
	}

	if(count > SOURCE_BUF_SIZE) MEM_freeN(sources);
}

void CustomData_swap(struct CustomData *data, int index, const int *corner_indices)
{
	const LayerTypeInfo *typeInfo;
	int i;

	for(i = 0; i < data->totlayer; ++i) {
		typeInfo = layerType_getInfo(data->layers[i].type);

		if(typeInfo->swap) {
			int offset = typeInfo->size * index;

			typeInfo->swap((char *)data->layers[i].data + offset, corner_indices);
		}
	}
}

void *CustomData_get(const CustomData *data, int index, int type)
{
	int offset;
	int layer_index;
	
	/* get the layer index of the active layer of type */
	layer_index = CustomData_get_active_layer_index(data, type);
	if(layer_index < 0) return NULL;

	/* get the offset of the desired element */
	offset = layerType_getInfo(type)->size * index;

	return (char *)data->layers[layer_index].data + offset;
}

void *CustomData_get_n(const CustomData *data, int type, int index, int n)
{
	int layer_index;
	int offset;

	/* get the layer index of the first layer of type */
	layer_index = data->typemap[type];
	if(layer_index < 0) return NULL;

	offset = layerType_getInfo(type)->size * index;
	return (char *)data->layers[layer_index+n].data + offset;
}

void *CustomData_get_layer(const CustomData *data, int type)
{
	/* get the layer index of the active layer of type */
	int layer_index = CustomData_get_active_layer_index(data, type);
	if(layer_index < 0) return NULL;

	return data->layers[layer_index].data;
}

void *CustomData_get_layer_n(const CustomData *data, int type, int n)
{
	/* get the layer index of the active layer of type */
	int layer_index = CustomData_get_layer_index_n(data, type, n);
	if(layer_index < 0) return NULL;

	return data->layers[layer_index].data;
}

void *CustomData_get_layer_named(const struct CustomData *data, int type,
								 const char *name)
{
	int layer_index = CustomData_get_named_layer_index(data, type, name);
	if(layer_index < 0) return NULL;

	return data->layers[layer_index].data;
}


int CustomData_set_layer_name(const CustomData *data, int type, int n, const char *name)
{
	/* get the layer index of the first layer of type */
	int layer_index = CustomData_get_layer_index_n(data, type, n);

	if(layer_index < 0) return 0;
	if (!name) return 0;
	
	strcpy(data->layers[layer_index].name, name);
	
	return 1;
}

void *CustomData_set_layer(const CustomData *data, int type, void *ptr)
{
	/* get the layer index of the first layer of type */
	int layer_index = CustomData_get_active_layer_index(data, type);

	if(layer_index < 0) return NULL;

	data->layers[layer_index].data = ptr;

	return ptr;
}

void *CustomData_set_layer_n(const struct CustomData *data, int type, int n, void *ptr)
{
	/* get the layer index of the first layer of type */
	int layer_index = CustomData_get_layer_index_n(data, type, n);
	if(layer_index < 0) return NULL;

	data->layers[layer_index].data = ptr;

	return ptr;
}

void CustomData_set(const CustomData *data, int index, int type, void *source)
{
	void *dest = CustomData_get(data, index, type);
	const LayerTypeInfo *typeInfo = layerType_getInfo(type);

	if(!dest) return;

	if(typeInfo->copy)
		typeInfo->copy(source, dest, 1);
	else
		memcpy(dest, source, typeInfo->size);
}

/* EditMesh functions */

void CustomData_em_free_block(CustomData *data, void **block)
{
	const LayerTypeInfo *typeInfo;
	int i;

	if(!*block) return;

	for(i = 0; i < data->totlayer; ++i) {
		if(!(data->layers[i].flag & CD_FLAG_NOFREE)) {
			typeInfo = layerType_getInfo(data->layers[i].type);

			if(typeInfo->free) {
				int offset = data->layers[i].offset;
				typeInfo->free((char*)*block + offset, 1, typeInfo->size);
			}
		}
	}

	MEM_freeN(*block);
	*block = NULL;
}

static void CustomData_em_alloc_block(CustomData *data, void **block)
{
	/* TODO: optimize free/alloc */

	if (*block)
		CustomData_em_free_block(data, block);

	if (data->totsize > 0)
		*block = MEM_callocN(data->totsize, "CustomData EM block");
	else
		*block = NULL;
}

void CustomData_em_copy_data(const CustomData *source, CustomData *dest,
							void *src_block, void **dest_block)
{
	const LayerTypeInfo *typeInfo;
	int dest_i, src_i;

	if (!*dest_block)
		CustomData_em_alloc_block(dest, dest_block);
	
	/* copies a layer at a time */
	dest_i = 0;
	for(src_i = 0; src_i < source->totlayer; ++src_i) {

		/* find the first dest layer with type >= the source type
		 * (this should work because layers are ordered by type)
		 */
		while(dest_i < dest->totlayer
			  && dest->layers[dest_i].type < source->layers[src_i].type)
			++dest_i;

		/* if there are no more dest layers, we're done */
		if(dest_i >= dest->totlayer) return;

		/* if we found a matching layer, copy the data */
		if(dest->layers[dest_i].type == source->layers[src_i].type &&
			strcmp(dest->layers[dest_i].name, source->layers[src_i].name) == 0) {
			char *src_data = (char*)src_block + source->layers[src_i].offset;
			char *dest_data = (char*)*dest_block + dest->layers[dest_i].offset;

			typeInfo = layerType_getInfo(source->layers[src_i].type);

			if(typeInfo->copy)
				typeInfo->copy(src_data, dest_data, 1);
			else
				memcpy(dest_data, src_data, typeInfo->size);

			/* if there are multiple source & dest layers of the same type,
			 * we don't want to copy all source layers to the same dest, so
			 * increment dest_i
			 */
			++dest_i;
		}
	}
}

void CustomData_em_validate_data(CustomData *data, void *block, int sub_elements)
{
	int i;
	for(i = 0; i < data->totlayer; i++) {
		const LayerTypeInfo *typeInfo = layerType_getInfo(data->layers[i].type);
		char *leayer_data = (char*)block + data->layers[i].offset;

		if(typeInfo->validate)
			typeInfo->validate(leayer_data, sub_elements);
	}
}

void *CustomData_em_get(const CustomData *data, void *block, int type)
{
	int layer_index;
	
	/* get the layer index of the first layer of type */
	layer_index = CustomData_get_active_layer_index(data, type);
	if(layer_index < 0) return NULL;

	return (char *)block + data->layers[layer_index].offset;
}

void *CustomData_em_get_n(const CustomData *data, void *block, int type, int n)
{
	int layer_index;
	
	/* get the layer index of the first layer of type */
	layer_index = CustomData_get_layer_index_n(data, type, n);
	if(layer_index < 0) return NULL;

	return (char *)block + data->layers[layer_index].offset;
}

void CustomData_em_set(CustomData *data, void *block, int type, void *source)
{
	void *dest = CustomData_em_get(data, block, type);
	const LayerTypeInfo *typeInfo = layerType_getInfo(type);

	if(!dest) return;

	if(typeInfo->copy)
		typeInfo->copy(source, dest, 1);
	else
		memcpy(dest, source, typeInfo->size);
}

void CustomData_em_set_n(CustomData *data, void *block, int type, int n, void *source)
{
	void *dest = CustomData_em_get_n(data, block, type, n);
	const LayerTypeInfo *typeInfo = layerType_getInfo(type);

	if(!dest) return;

	if(typeInfo->copy)
		typeInfo->copy(source, dest, 1);
	else
		memcpy(dest, source, typeInfo->size);
}

void CustomData_em_interp(CustomData *data, void **src_blocks, float *weights,
						  float *sub_weights, int count, void *dest_block)
{
	int i, j;
	void *source_buf[SOURCE_BUF_SIZE];
	void **sources = source_buf;

	/* slow fallback in case we're interpolating a ridiculous number of
	 * elements
	 */
	if(count > SOURCE_BUF_SIZE)
		sources = MEM_callocN(sizeof(*sources) * count,
							  "CustomData_interp sources");

	/* interpolates a layer at a time */
	for(i = 0; i < data->totlayer; ++i) {
		CustomDataLayer *layer = &data->layers[i];
		const LayerTypeInfo *typeInfo = layerType_getInfo(layer->type);

		if(typeInfo->interp) {
			for(j = 0; j < count; ++j)
				sources[j] = (char *)src_blocks[j] + layer->offset;

			typeInfo->interp(sources, weights, sub_weights, count,
							  (char *)dest_block + layer->offset);
		}
	}

	if(count > SOURCE_BUF_SIZE) MEM_freeN(sources);
}

void CustomData_em_set_default(CustomData *data, void **block)
{
	const LayerTypeInfo *typeInfo;
	int i;

	if (!*block)
		CustomData_em_alloc_block(data, block);

	for(i = 0; i < data->totlayer; ++i) {
		int offset = data->layers[i].offset;

		typeInfo = layerType_getInfo(data->layers[i].type);

		if(typeInfo->set_default)
			typeInfo->set_default((char*)*block + offset, 1);
	}
}

void CustomData_to_em_block(const CustomData *source, CustomData *dest,
							int src_index, void **dest_block)
{
	const LayerTypeInfo *typeInfo;
	int dest_i, src_i, src_offset;

	if (!*dest_block)
		CustomData_em_alloc_block(dest, dest_block);
	
	/* copies a layer at a time */
	dest_i = 0;
	for(src_i = 0; src_i < source->totlayer; ++src_i) {

		/* find the first dest layer with type >= the source type
		 * (this should work because layers are ordered by type)
		 */
		while(dest_i < dest->totlayer
			  && dest->layers[dest_i].type < source->layers[src_i].type)
			++dest_i;

		/* if there are no more dest layers, we're done */
		if(dest_i >= dest->totlayer) return;

		/* if we found a matching layer, copy the data */
		if(dest->layers[dest_i].type == source->layers[src_i].type) {
			int offset = dest->layers[dest_i].offset;
			char *src_data = source->layers[src_i].data;
			char *dest_data = (char*)*dest_block + offset;

			typeInfo = layerType_getInfo(dest->layers[dest_i].type);
			src_offset = src_index * typeInfo->size;

			if(typeInfo->copy)
				typeInfo->copy(src_data + src_offset, dest_data, 1);
			else
				memcpy(dest_data, src_data + src_offset, typeInfo->size);

			/* if there are multiple source & dest layers of the same type,
			 * we don't want to copy all source layers to the same dest, so
			 * increment dest_i
			 */
			++dest_i;
		}
	}
}

void CustomData_from_em_block(const CustomData *source, CustomData *dest,
							  void *src_block, int dest_index)
{
	const LayerTypeInfo *typeInfo;
	int dest_i, src_i, dest_offset;

	/* copies a layer at a time */
	dest_i = 0;
	for(src_i = 0; src_i < source->totlayer; ++src_i) {

		/* find the first dest layer with type >= the source type
		 * (this should work because layers are ordered by type)
		 */
		while(dest_i < dest->totlayer
			  && dest->layers[dest_i].type < source->layers[src_i].type)
			++dest_i;

		/* if there are no more dest layers, we're done */
		if(dest_i >= dest->totlayer) return;

		/* if we found a matching layer, copy the data */
		if(dest->layers[dest_i].type == source->layers[src_i].type) {
			int offset = source->layers[src_i].offset;
			char *src_data = (char*)src_block + offset;
			char *dest_data = dest->layers[dest_i].data;

			typeInfo = layerType_getInfo(dest->layers[dest_i].type);
			dest_offset = dest_index * typeInfo->size;

			if(typeInfo->copy)
				typeInfo->copy(src_data, dest_data + dest_offset, 1);
			else
				memcpy(dest_data + dest_offset, src_data, typeInfo->size);

			/* if there are multiple source & dest layers of the same type,
			 * we don't want to copy all source layers to the same dest, so
			 * increment dest_i
			 */
			++dest_i;
		}
	}

}

/*Bmesh functions*/
/*needed to convert to/from different face reps*/
void CustomData_to_bmeshpoly(CustomData *fdata, CustomData *pdata, CustomData *ldata)
{
	int i;
	for(i=0; i < fdata->totlayer; i++){
		if(fdata->layers[i].type == CD_MTFACE){
			CustomData_add_layer(pdata, CD_MTEXPOLY, CD_CALLOC, &(fdata->layers[i].name), 0);
			CustomData_add_layer(ldata, CD_MLOOPUV, CD_CALLOC, &(fdata->layers[i].name), 0);
		}
		else if(fdata->layers[i].type == CD_MCOL)
			CustomData_add_layer(ldata, CD_MLOOPCOL, CD_CALLOC, &(fdata->layers[i].name), 0);
	}		
}
void CustomData_from_bmeshpoly(CustomData *fdata, CustomData *pdata, CustomData *ldata, int total)
{
	int i;
	for(i=0; i < pdata->totlayer; i++){
		if(pdata->layers[i].type == CD_MTEXPOLY)
			CustomData_add_layer(fdata, CD_MTFACE, CD_CALLOC, &(pdata->layers[i].name), total);
	}
	for(i=0; i < ldata->totlayer; i++){
		if(ldata->layers[i].type == CD_MLOOPCOL)
			CustomData_add_layer(fdata, CD_MCOL, CD_CALLOC, &(ldata->layers[i].name), total);
	}
}


void CustomData_bmesh_init_pool(CustomData *data, int allocsize)
{
	if(data->totlayer)data->pool = BLI_mempool_create(data->totsize, allocsize, allocsize, FALSE, FALSE);
}

void CustomData_bmesh_free_block(CustomData *data, void **block)
{
	const LayerTypeInfo *typeInfo;
	int i;

	if(!*block) return;
	for(i = 0; i < data->totlayer; ++i) {
		if(!(data->layers[i].flag & CD_FLAG_NOFREE)) {
			typeInfo = layerType_getInfo(data->layers[i].type);

			if(typeInfo->free) {
				int offset = data->layers[i].offset;
				typeInfo->free((char*)*block + offset, 1, typeInfo->size);
			}
		}
	}

	BLI_mempool_free(data->pool, *block);
	*block = NULL;
}

static void CustomData_bmesh_alloc_block(CustomData *data, void **block)
{

	if (*block)
		CustomData_bmesh_free_block(data, block);

	if (data->totsize > 0)
		*block = BLI_mempool_calloc(data->pool);
	else
		*block = NULL;
}

void CustomData_bmesh_copy_data(const CustomData *source, CustomData *dest,
							void *src_block, void **dest_block)
{
	const LayerTypeInfo *typeInfo;
	int dest_i, src_i;

	if (!*dest_block)
		CustomData_bmesh_alloc_block(dest, dest_block);
	
	/* copies a layer at a time */
	dest_i = 0;
	for(src_i = 0; src_i < source->totlayer; ++src_i) {

		/* find the first dest layer with type >= the source type
		 * (this should work because layers are ordered by type)
		 */
		while(dest_i < dest->totlayer
			  && dest->layers[dest_i].type < source->layers[src_i].type)
			++dest_i;

		/* if there are no more dest layers, we're done */
		if(dest_i >= dest->totlayer) return;

		/* if we found a matching layer, copy the data */
		if(dest->layers[dest_i].type == source->layers[src_i].type &&
			strcmp(dest->layers[dest_i].name, source->layers[src_i].name) == 0) {
			char *src_data = (char*)src_block + source->layers[src_i].offset;
			char *dest_data = (char*)*dest_block + dest->layers[dest_i].offset;

			typeInfo = layerType_getInfo(source->layers[src_i].type);

			if(typeInfo->copy)
				typeInfo->copy(src_data, dest_data, 1);
			else
				memcpy(dest_data, src_data, typeInfo->size);

			/* if there are multiple source & dest layers of the same type,
			 * we don't want to copy all source layers to the same dest, so
			 * increment dest_i
			 */
			++dest_i;
		}
	}
}

/*Bmesh Custom Data Functions. Should replace editmesh ones with these as well, due to more effecient memory alloc*/
void *CustomData_bmesh_get(const CustomData *data, void *block, int type)
{
	int layer_index;
	
	/* get the layer index of the first layer of type */
	layer_index = CustomData_get_active_layer_index(data, type);
	if(layer_index < 0) return NULL;

	return (char *)block + data->layers[layer_index].offset;
}

void *CustomData_bmesh_get_n(const CustomData *data, void *block, int type, int n)
{
	int layer_index;
	
	/* get the layer index of the first layer of type */
	layer_index = CustomData_get_layer_index(data, type);
	if(layer_index < 0) return NULL;

	return (char *)block + data->layers[layer_index+n].offset;
}

/*gets from the layer at physical index n, note: doesn't check type.*/
void *CustomData_bmesh_get_layer_n(const CustomData *data, void *block, int n)
{
	if(n < 0 || n >= data->totlayer) return NULL;

	return (char *)block + data->layers[n].offset;
}


void CustomData_bmesh_set(const CustomData *data, void *block, int type, void *source)
{
	void *dest = CustomData_bmesh_get(data, block, type);
	const LayerTypeInfo *typeInfo = layerType_getInfo(type);

	if(!dest) return;

	if(typeInfo->copy)
		typeInfo->copy(source, dest, 1);
	else
		memcpy(dest, source, typeInfo->size);
}

void CustomData_bmesh_set_n(CustomData *data, void *block, int type, int n, void *source)
{
	void *dest = CustomData_bmesh_get_n(data, block, type, n);
	const LayerTypeInfo *typeInfo = layerType_getInfo(type);

	if(!dest) return;

	if(typeInfo->copy)
		typeInfo->copy(source, dest, 1);
	else
		memcpy(dest, source, typeInfo->size);
}

void CustomData_bmesh_set_layer_n(CustomData *data, void *block, int n, void *source)
{
	void *dest = CustomData_bmesh_get_layer_n(data, block, n);
	const LayerTypeInfo *typeInfo = layerType_getInfo(data->layers[n].type);

	if(!dest) return;

	if(typeInfo->copy)
		typeInfo->copy(source, dest, 1);
	else
		memcpy(dest, source, typeInfo->size);
}

void CustomData_bmesh_interp(CustomData *data, void **src_blocks, float *weights,
						  float *sub_weights, int count, void *dest_block)
{
	int i, j;
	void *source_buf[SOURCE_BUF_SIZE];
	void **sources = source_buf;

	/* slow fallback in case we're interpolating a ridiculous number of
	 * elements
	 */
	if(count > SOURCE_BUF_SIZE)
		sources = MEM_callocN(sizeof(*sources) * count,
							  "CustomData_interp sources");

	/* interpolates a layer at a time */
	for(i = 0; i < data->totlayer; ++i) {
		CustomDataLayer *layer = &data->layers[i];
		const LayerTypeInfo *typeInfo = layerType_getInfo(layer->type);
		if(typeInfo->interp) {
			for(j = 0; j < count; ++j)
				sources[j] = (char *)src_blocks[j] + layer->offset;

			typeInfo->interp(sources, weights, sub_weights, count,
							  (char *)dest_block + layer->offset);
		}
	}

	if(count > SOURCE_BUF_SIZE) MEM_freeN(sources);
}

void CustomData_bmesh_set_default(CustomData *data, void **block)
{
	const LayerTypeInfo *typeInfo;
	int i;

	if (!*block)
		CustomData_bmesh_alloc_block(data, block);

	for(i = 0; i < data->totlayer; ++i) {
		int offset = data->layers[i].offset;

		typeInfo = layerType_getInfo(data->layers[i].type);

		if(typeInfo->set_default)
			typeInfo->set_default((char*)*block + offset, 1);
		else memset((char*)*block + offset, 0, typeInfo->size);
	}
}

void CustomData_to_bmesh_block(const CustomData *source, CustomData *dest,
							int src_index, void **dest_block)
{
	const LayerTypeInfo *typeInfo;
	int dest_i, src_i, src_offset;

	if (!*dest_block)
		CustomData_bmesh_alloc_block(dest, dest_block);
	
	/* copies a layer at a time */
	dest_i = 0;
	for(src_i = 0; src_i < source->totlayer; ++src_i) {

		/* find the first dest layer with type >= the source type
		 * (this should work because layers are ordered by type)
		 */
		while(dest_i < dest->totlayer
			  && dest->layers[dest_i].type < source->layers[src_i].type)
			++dest_i;

		/* if there are no more dest layers, we're done */
		if(dest_i >= dest->totlayer) return;

		/* if we found a matching layer, copy the data */
		if(dest->layers[dest_i].type == source->layers[src_i].type) {
			int offset = dest->layers[dest_i].offset;
			char *src_data = source->layers[src_i].data;
			char *dest_data = (char*)*dest_block + offset;

			typeInfo = layerType_getInfo(dest->layers[dest_i].type);
			src_offset = src_index * typeInfo->size;

			if(typeInfo->copy)
				typeInfo->copy(src_data + src_offset, dest_data, 1);
			else
				memcpy(dest_data, src_data + src_offset, typeInfo->size);

			/* if there are multiple source & dest layers of the same type,
			 * we don't want to copy all source layers to the same dest, so
			 * increment dest_i
			 */
			++dest_i;
		}
	}
}

void CustomData_from_bmesh_block(const CustomData *source, CustomData *dest,
							  void *src_block, int dest_index)
{
	const LayerTypeInfo *typeInfo;
	int dest_i, src_i, dest_offset;

	/* copies a layer at a time */
	dest_i = 0;
	for(src_i = 0; src_i < source->totlayer; ++src_i) {

		/* find the first dest layer with type >= the source type
		 * (this should work because layers are ordered by type)
		 */
		while(dest_i < dest->totlayer
			  && dest->layers[dest_i].type < source->layers[src_i].type)
			++dest_i;

		/* if there are no more dest layers, we're done */
		if(dest_i >= dest->totlayer) return;

		/* if we found a matching layer, copy the data */
		if(dest->layers[dest_i].type == source->layers[src_i].type) {
			int offset = source->layers[src_i].offset;
			char *src_data = (char*)src_block + offset;
			char *dest_data = dest->layers[dest_i].data;

			typeInfo = layerType_getInfo(dest->layers[dest_i].type);
			dest_offset = dest_index * typeInfo->size;

			if(typeInfo->copy)
				typeInfo->copy(src_data, dest_data + dest_offset, 1);
			else
				memcpy(dest_data + dest_offset, src_data, typeInfo->size);

			/* if there are multiple source & dest layers of the same type,
			 * we don't want to copy all source layers to the same dest, so
			 * increment dest_i
			 */
			++dest_i;
		}
	}

}

void CustomData_file_write_info(int type, const char **structname, int *structnum)
{
	const LayerTypeInfo *typeInfo = layerType_getInfo(type);

	*structname = typeInfo->structname;
	*structnum = typeInfo->structnum;
}

int CustomData_sizeof(int type)
{
	const LayerTypeInfo *typeInfo = layerType_getInfo(type);

	return typeInfo->size;
}

const char *CustomData_layertype_name(int type)
{
	return layerType_getName(type);
}

static int  CustomData_is_property_layer(int type)
{
	if((type == CD_PROP_FLT) || (type == CD_PROP_INT) || (type == CD_PROP_STR))
		return 1;
	return 0;
}

static int cd_layer_find_dupe(CustomData *data, const char *name, int type, int index)
{
	int i;
	/* see if there is a duplicate */
	for(i=0; i<data->totlayer; i++) {
		if(i != index) {
			CustomDataLayer *layer= &data->layers[i];
			
			if(CustomData_is_property_layer(type)) {
				if(CustomData_is_property_layer(layer->type) && strcmp(layer->name, name)==0) {
					return 1;
				}
			}
			else{
				if(i!=index && layer->type==type && strcmp(layer->name, name)==0) {
					return 1;
				}
			}
		}
	}
	
	return 0;
}

static int customdata_unique_check(void *arg, const char *name)
{
	struct {CustomData *data; int type; int index;} *data_arg= arg;
	return cd_layer_find_dupe(data_arg->data, name, data_arg->type, data_arg->index);
}

void CustomData_set_layer_unique_name(CustomData *data, int index)
{	
	CustomDataLayer *nlayer= &data->layers[index];
	const LayerTypeInfo *typeInfo= layerType_getInfo(nlayer->type);

	struct {CustomData *data; int type; int index;} data_arg;
	data_arg.data= data;
	data_arg.type= nlayer->type;
	data_arg.index= index;

	if (!typeInfo->defaultname)
		return;
	
	BLI_uniquename_cb(customdata_unique_check, &data_arg, typeInfo->defaultname, '.', nlayer->name, sizeof(nlayer->name));
}

void CustomData_validate_layer_name(const CustomData *data, int type, char *name, char *outname)
{
	int index = -1;

	/* if a layer name was given, try to find that layer */
	if(name[0])
		index = CustomData_get_named_layer_index(data, type, name);

	if(index < 0) {
		/* either no layer was specified, or the layer we want has been
		* deleted, so assign the active layer to name
		*/
		index = CustomData_get_active_layer_index(data, type);
		strcpy(outname, data->layers[index].name);
	}
	else
		strcpy(outname, name);
}

int CustomData_verify_versions(struct CustomData *data, int index)
{
	const LayerTypeInfo *typeInfo;
	CustomDataLayer *layer = &data->layers[index];
	int i, keeplayer = 1;

	if (layer->type >= CD_NUMTYPES) {
		keeplayer = 0; /* unknown layer type from future version */
	}
	else {
		typeInfo = layerType_getInfo(layer->type);

		if (!typeInfo->defaultname && (index > 0) &&
			data->layers[index-1].type == layer->type)
			keeplayer = 0; /* multiple layers of which we only support one */
	}

	if (!keeplayer) {
		for (i=index+1; i < data->totlayer; ++i)
			data->layers[i-1] = data->layers[i];
		data->totlayer--;
	}

	return keeplayer;
}

/****************************** External Files *******************************/

static void customdata_external_filename(char filename[FILE_MAX], ID *id, CustomDataExternal *external)
{
	BLI_strncpy(filename, external->filename, FILE_MAX);
	BLI_path_abs(filename, ID_BLEND_PATH(G.main, id));
}

void CustomData_external_reload(CustomData *data, ID *UNUSED(id), CustomDataMask mask, int totelem)
{
	CustomDataLayer *layer;
	const LayerTypeInfo *typeInfo;
	int i;

	for(i=0; i<data->totlayer; i++) {
		layer = &data->layers[i];
		typeInfo = layerType_getInfo(layer->type);

		if(!(mask & CD_TYPE_AS_MASK(layer->type)));
		else if((layer->flag & CD_FLAG_EXTERNAL) && (layer->flag & CD_FLAG_IN_MEMORY)) {
			if(typeInfo->free)
				typeInfo->free(layer->data, totelem, typeInfo->size);
			layer->flag &= ~CD_FLAG_IN_MEMORY;
		}
	}
}

void CustomData_external_read(CustomData *data, ID *id, CustomDataMask mask, int totelem)
{
	CustomDataExternal *external= data->external;
	CustomDataLayer *layer;
	CDataFile *cdf;
	CDataFileLayer *blay;
	char filename[FILE_MAX];
	const LayerTypeInfo *typeInfo;
	int i, update = 0;

	if(!external)
		return;
	
	for(i=0; i<data->totlayer; i++) {
		layer = &data->layers[i];
		typeInfo = layerType_getInfo(layer->type);

		if(!(mask & CD_TYPE_AS_MASK(layer->type)));
		else if(layer->flag & CD_FLAG_IN_MEMORY);
		else if((layer->flag & CD_FLAG_EXTERNAL) && typeInfo->read)
			update= 1;
	}

	if(!update)
		return;

	customdata_external_filename(filename, id, external);

	cdf= cdf_create(CDF_TYPE_MESH);
	if(!cdf_read_open(cdf, filename)) {
		fprintf(stderr, "Failed to read %s layer from %s.\n", layerType_getName(layer->type), filename);
		return;
	}

	for(i=0; i<data->totlayer; i++) {
		layer = &data->layers[i];
		typeInfo = layerType_getInfo(layer->type);

		if(!(mask & CD_TYPE_AS_MASK(layer->type)));
		else if(layer->flag & CD_FLAG_IN_MEMORY);
		else if((layer->flag & CD_FLAG_EXTERNAL) && typeInfo->read) {
			blay= cdf_layer_find(cdf, layer->type, layer->name);

			if(blay) {
				if(cdf_read_layer(cdf, blay)) {
					if(typeInfo->read(cdf, layer->data, totelem));
					else break;
					layer->flag |= CD_FLAG_IN_MEMORY;
				}
				else
					break;
			}
		}
	}

	cdf_read_close(cdf);
	cdf_free(cdf);
}

void CustomData_external_write(CustomData *data, ID *id, CustomDataMask mask, int totelem, int free)
{
	CustomDataExternal *external= data->external;
	CustomDataLayer *layer;
	CDataFile *cdf;
	CDataFileLayer *blay;
	const LayerTypeInfo *typeInfo;
	int i, update = 0;
	char filename[FILE_MAX];

	if(!external)
		return;

	/* test if there is anything to write */
	for(i=0; i<data->totlayer; i++) {
		layer = &data->layers[i];
		typeInfo = layerType_getInfo(layer->type);

		if(!(mask & CD_TYPE_AS_MASK(layer->type)));
		else if((layer->flag & CD_FLAG_EXTERNAL) && typeInfo->write)
			update= 1;
	}

	if(!update)
		return;

	/* make sure data is read before we try to write */
	CustomData_external_read(data, id, mask, totelem);
	customdata_external_filename(filename, id, external);

	cdf= cdf_create(CDF_TYPE_MESH);

	for(i=0; i<data->totlayer; i++) {
		layer = &data->layers[i];
		typeInfo = layerType_getInfo(layer->type);

		if((layer->flag & CD_FLAG_EXTERNAL) && typeInfo->filesize) {
			if(layer->flag & CD_FLAG_IN_MEMORY) {
				cdf_layer_add(cdf, layer->type, layer->name,
					typeInfo->filesize(cdf, layer->data, totelem));
			}
			else {
				cdf_free(cdf);
				return; /* read failed for a layer! */
			}
		}
	}

	if(!cdf_write_open(cdf, filename)) {
		fprintf(stderr, "Failed to open %s for writing.\n", filename);
		return;
	}

	for(i=0; i<data->totlayer; i++) {
		layer = &data->layers[i];
		typeInfo = layerType_getInfo(layer->type);

		if((layer->flag & CD_FLAG_EXTERNAL) && typeInfo->write) {
			blay= cdf_layer_find(cdf, layer->type, layer->name);

			if(cdf_write_layer(cdf, blay)) {
				if(typeInfo->write(cdf, layer->data, totelem));
				else break;
			}
			else
				break;
		}
	}

	if(i != data->totlayer) {
		fprintf(stderr, "Failed to write data to %s.\n", filename);
		cdf_free(cdf);
		return;
	}

	for(i=0; i<data->totlayer; i++) {
		layer = &data->layers[i];
		typeInfo = layerType_getInfo(layer->type);

		if((layer->flag & CD_FLAG_EXTERNAL) && typeInfo->write) {
			if(free) {
				if(typeInfo->free)
					typeInfo->free(layer->data, totelem, typeInfo->size);
				layer->flag &= ~CD_FLAG_IN_MEMORY;
			}
		}
	}

	cdf_write_close(cdf);
	cdf_free(cdf);
}

void CustomData_external_add(CustomData *data, ID *UNUSED(id), int type, int UNUSED(totelem), const char *filename)
{
	CustomDataExternal *external= data->external;
	CustomDataLayer *layer;
	int layer_index;

	layer_index = CustomData_get_active_layer_index(data, type);
	if(layer_index < 0) return;

	layer = &data->layers[layer_index];

	if(layer->flag & CD_FLAG_EXTERNAL)
		return;

	if(!external) {
		external= MEM_callocN(sizeof(CustomDataExternal), "CustomDataExternal");
		data->external= external;
	}
	BLI_strncpy(external->filename, filename, sizeof(external->filename));

	layer->flag |= CD_FLAG_EXTERNAL|CD_FLAG_IN_MEMORY;
}

void CustomData_external_remove(CustomData *data, ID *id, int type, int totelem)
{
	CustomDataExternal *external= data->external;
	CustomDataLayer *layer;
	//char filename[FILE_MAX];
	int layer_index; // i, remove_file;

	layer_index = CustomData_get_active_layer_index(data, type);
	if(layer_index < 0) return;

	layer = &data->layers[layer_index];

	if(!external)
		return;

	if(layer->flag & CD_FLAG_EXTERNAL) {
		if(!(layer->flag & CD_FLAG_IN_MEMORY))
			CustomData_external_read(data, id, CD_TYPE_AS_MASK(layer->type), totelem);

		layer->flag &= ~CD_FLAG_EXTERNAL;

#if 0
		remove_file= 1;
		for(i=0; i<data->totlayer; i++)
			if(data->layers[i].flag & CD_FLAG_EXTERNAL)
				remove_file= 0;

		if(remove_file) {
			customdata_external_filename(filename, id, external);
			cdf_remove(filename);
			CustomData_external_free(data);
		}
#endif
	}
}

int CustomData_external_test(CustomData *data, int type)
{
	CustomDataLayer *layer;
	int layer_index;

	layer_index = CustomData_get_active_layer_index(data, type);
	if(layer_index < 0) return 0;

	layer = &data->layers[layer_index];
	return (layer->flag & CD_FLAG_EXTERNAL);
}

#if 0
void CustomData_external_remove_object(CustomData *data, ID *id)
{
	CustomDataExternal *external= data->external;
	char filename[FILE_MAX];

	if(!external)
		return;

	customdata_external_filename(filename, id, external);
	cdf_remove(filename);
	CustomData_external_free(data);
}
#endif

