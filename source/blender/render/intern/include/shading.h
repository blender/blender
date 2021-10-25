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
 * The Original Code is Copyright (C) 2006 Blender Foundation
 * All rights reserved.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/render/intern/include/shading.h
 *  \ingroup render
 */


struct ShadeInput;
struct ShadeResult;
struct RenderPart;
struct RenderLayer;
struct PixStr;
struct LampRen;
struct VlakRen;
struct StrandPoint;
struct ObjectInstanceRen;
struct Isect;

/* shadeinput.c */

#define RE_MAX_OSA 16

/* needed to calculate shadow and AO for an entire pixel */
typedef struct ShadeSample {
	int tot;						/* amount of shi in use, can be 1 for not FULL_OSA */
	
	RenderLayer *rlpp[RE_MAX_OSA];	/* fast lookup from sample to renderlayer (fullsample buf) */
	
	/* could be malloced once */
	ShadeInput shi[RE_MAX_OSA];
	ShadeResult shr[RE_MAX_OSA];
} ShadeSample;


	/* also the node shader callback */
void shade_material_loop(struct ShadeInput *shi, struct ShadeResult *shr);

void shade_input_set_triangle_i(struct ShadeInput *shi, struct ObjectInstanceRen *obi, struct VlakRen *vlr, short i1, short i2, short i3);
void shade_input_set_triangle(struct ShadeInput *shi, volatile int obi, volatile int facenr, int normal_flip);
void shade_input_copy_triangle(struct ShadeInput *shi, struct ShadeInput *from);
void shade_input_calc_viewco(struct ShadeInput *shi, float x, float y, float z, float view[3], float dxyview[2], float co[3], float dxco[3], float dyco[3]);
void shade_input_set_viewco(struct ShadeInput *shi, float x, float y, float sx, float sy, float z);
void shade_input_set_uv(struct ShadeInput *shi);
void shade_input_set_normals(struct ShadeInput *shi);
void shade_input_set_vertex_normals(struct ShadeInput *shi);
void shade_input_flip_normals(struct ShadeInput *shi);
void shade_input_set_shade_texco(struct ShadeInput *shi);
void shade_input_set_strand(struct ShadeInput *shi, struct StrandRen *strand, struct StrandPoint *spoint);
void shade_input_set_strand_texco(struct ShadeInput *shi, struct StrandRen *strand, struct StrandVert *svert, struct StrandPoint *spoint);
void shade_input_do_shade(struct ShadeInput *shi, struct ShadeResult *shr);

void shade_input_init_material(struct ShadeInput *shi);
void shade_input_initialize(struct ShadeInput *shi, struct RenderPart *pa, struct RenderLayer *rl, int sample);

void shade_sample_initialize(struct ShadeSample *ssamp, struct RenderPart *pa, struct RenderLayer *rl);
void shade_samples_do_AO(struct ShadeSample *ssamp);
void shade_samples_fill_with_ps(struct ShadeSample *ssamp, struct PixStr *ps, int x, int y);
int shade_samples(struct ShadeSample *ssamp, struct PixStr *ps, int x, int y);

void vlr_set_uv_indices(struct VlakRen *vlr, int *i1, int *i2, int *i3);

void	calc_R_ref(struct ShadeInput *shi);

void barycentric_differentials_from_position(
	const float co[3], const float v1[3], const float v2[3], const float v3[3],
	const float dxco[3], const float dyco[3], const float facenor[3], const bool differentials,
	float *u, float *v, float *dx_u, float *dx_v, float *dy_u, float *dy_v);

/* shadeoutput. */
void shade_lamp_loop(struct ShadeInput *shi, struct ShadeResult *shr);

void shade_color(struct ShadeInput *shi, ShadeResult *shr);

void ambient_occlusion(struct ShadeInput *shi);
void environment_lighting_apply(struct ShadeInput *shi, struct ShadeResult *shr);

ListBase *get_lights(struct ShadeInput *shi);
float lamp_get_visibility(struct LampRen *lar, const float co[3], float lv[3], float *dist);
void lamp_get_shadow(struct LampRen *lar, ShadeInput *shi, float inp, float shadfac[4], int do_real);

float fresnel_fac(const float view[3], const float vn[3], float fresnel, float fac);

/* rayshade.c */
extern void shade_ray(struct Isect *is, struct ShadeInput *shi, struct ShadeResult *shr);
