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
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/texture.c
 *  \ingroup bke
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_kdopbvh.h"
#include "BLI_utildefines.h"
#include "BLI_math_color.h"

#include "DNA_key_types.h"
#include "DNA_object_types.h"
#include "DNA_lamp_types.h"
#include "DNA_material_types.h"
#include "DNA_world_types.h"
#include "DNA_brush_types.h"
#include "DNA_node_types.h"
#include "DNA_color_types.h"
#include "DNA_particle_types.h"
#include "DNA_linestyle_types.h"

#include "IMB_imbuf.h"

#include "BKE_global.h"
#include "BKE_main.h"

#include "BKE_colorband.h"
#include "BKE_library.h"
#include "BKE_library_query.h"
#include "BKE_library_remap.h"
#include "BKE_image.h"
#include "BKE_material.h"
#include "BKE_texture.h"
#include "BKE_key.h"
#include "BKE_icons.h"
#include "BKE_node.h"
#include "BKE_animsys.h"
#include "BKE_colortools.h"
#include "BKE_scene.h"

#include "RE_shader_ext.h"

/* ****************** Mapping ******************* */

TexMapping *BKE_texture_mapping_add(int type)
{
	TexMapping *texmap = MEM_callocN(sizeof(TexMapping), "TexMapping");
	
	BKE_texture_mapping_default(texmap, type);
	
	return texmap;
}

void BKE_texture_mapping_default(TexMapping *texmap, int type)
{
	memset(texmap, 0, sizeof(TexMapping));

	texmap->size[0] = texmap->size[1] = texmap->size[2] = 1.0f;
	texmap->max[0] = texmap->max[1] = texmap->max[2] = 1.0f;
	unit_m4(texmap->mat);

	texmap->projx = PROJ_X;
	texmap->projy = PROJ_Y;
	texmap->projz = PROJ_Z;
	texmap->mapping = MTEX_FLAT;
	texmap->type = type;
}

void BKE_texture_mapping_init(TexMapping *texmap)
{
	float smat[4][4], rmat[4][4], tmat[4][4], proj[4][4], size[3];

	if (texmap->projx == PROJ_X && texmap->projy == PROJ_Y && texmap->projz == PROJ_Z &&
	    is_zero_v3(texmap->loc) && is_zero_v3(texmap->rot) && is_one_v3(texmap->size))
	{
		unit_m4(texmap->mat);

		texmap->flag |= TEXMAP_UNIT_MATRIX;
	}
	else {
		/* axis projection */
		zero_m4(proj);
		proj[3][3] = 1.0f;

		if (texmap->projx != PROJ_N)
			proj[texmap->projx - 1][0] = 1.0f;
		if (texmap->projy != PROJ_N)
			proj[texmap->projy - 1][1] = 1.0f;
		if (texmap->projz != PROJ_N)
			proj[texmap->projz - 1][2] = 1.0f;

		/* scale */
		copy_v3_v3(size, texmap->size);

		if (ELEM(texmap->type, TEXMAP_TYPE_TEXTURE, TEXMAP_TYPE_NORMAL)) {
			/* keep matrix invertible */
			if (fabsf(size[0]) < 1e-5f)
				size[0] = signf(size[0]) * 1e-5f;
			if (fabsf(size[1]) < 1e-5f)
				size[1] = signf(size[1]) * 1e-5f;
			if (fabsf(size[2]) < 1e-5f)
				size[2] = signf(size[2]) * 1e-5f;
		}
		
		size_to_mat4(smat, texmap->size);

		/* rotation */
		eul_to_mat4(rmat, texmap->rot);

		/* translation */
		unit_m4(tmat);
		copy_v3_v3(tmat[3], texmap->loc);

		if (texmap->type == TEXMAP_TYPE_TEXTURE) {
			/* to transform a texture, the inverse transform needs
			 * to be applied to the texture coordinate */
			mul_m4_series(texmap->mat, tmat, rmat, smat);
			invert_m4(texmap->mat);
		}
		else if (texmap->type == TEXMAP_TYPE_POINT) {
			/* forward transform */
			mul_m4_series(texmap->mat, tmat, rmat, smat);
		}
		else if (texmap->type == TEXMAP_TYPE_VECTOR) {
			/* no translation for vectors */
			mul_m4_m4m4(texmap->mat, rmat, smat);
		}
		else if (texmap->type == TEXMAP_TYPE_NORMAL) {
			/* no translation for normals, and inverse transpose */
			mul_m4_m4m4(texmap->mat, rmat, smat);
			invert_m4(texmap->mat);
			transpose_m4(texmap->mat);
		}

		/* projection last */
		mul_m4_m4m4(texmap->mat, texmap->mat, proj);

		texmap->flag &= ~TEXMAP_UNIT_MATRIX;
	}
}

ColorMapping *BKE_texture_colormapping_add(void)
{
	ColorMapping *colormap = MEM_callocN(sizeof(ColorMapping), "ColorMapping");
	
	BKE_texture_colormapping_default(colormap);
	
	return colormap;
}

void BKE_texture_colormapping_default(ColorMapping *colormap)
{
	memset(colormap, 0, sizeof(ColorMapping));

	BKE_colorband_init(&colormap->coba, true);

	colormap->bright = 1.0;
	colormap->contrast = 1.0;
	colormap->saturation = 1.0;

	colormap->blend_color[0] = 0.8f;
	colormap->blend_color[1] = 0.8f;
	colormap->blend_color[2] = 0.8f;
	colormap->blend_type = MA_RAMP_BLEND;
	colormap->blend_factor = 0.0f;
}

/* ******************* TEX ************************ */

/** Free (or release) any data used by this texture (does not free the texure itself). */
void BKE_texture_free(Tex *tex)
{
	BKE_animdata_free((ID *)tex, false);

	/* is no lib link block, but texture extension */
	if (tex->nodetree) {
		ntreeFreeTree(tex->nodetree);
		MEM_freeN(tex->nodetree);
		tex->nodetree = NULL;
	}

	MEM_SAFE_FREE(tex->coba);
	if (tex->env) {
		BKE_texture_envmap_free(tex->env);
		tex->env = NULL;
	}
	if (tex->pd) {
		BKE_texture_pointdensity_free(tex->pd);
		tex->pd = NULL;
	}
	if (tex->vd) {
		BKE_texture_voxeldata_free(tex->vd);
		tex->vd = NULL;
	}
	if (tex->ot) {
		BKE_texture_ocean_free(tex->ot);
		tex->ot = NULL;
	}
	
	BKE_icon_id_delete((ID *)tex);
	BKE_previewimg_free(&tex->preview);
}

/* ------------------------------------------------------------------------- */

void BKE_texture_default(Tex *tex)
{
	/* BLI_assert(MEMCMP_STRUCT_OFS_IS_ZERO(tex, id)); */  /* Not here, can be called with some pointers set. :/ */

	tex->type = TEX_IMAGE;
	tex->ima = NULL;
	tex->stype = 0;
	tex->flag = TEX_CHECKER_ODD;
	tex->imaflag = TEX_INTERPOL | TEX_MIPMAP | TEX_USEALPHA;
	tex->extend = TEX_REPEAT;
	tex->cropxmin = tex->cropymin = 0.0;
	tex->cropxmax = tex->cropymax = 1.0;
	tex->texfilter = TXF_EWA;
	tex->afmax = 8;
	tex->xrepeat = tex->yrepeat = 1;
	tex->fie_ima = 2;
	tex->sfra = 1;
	tex->frames = 0;
	tex->offset = 0;
	tex->noisesize = 0.25;
	tex->noisedepth = 2;
	tex->turbul = 5.0;
	tex->nabla = 0.025;  // also in do_versions
	tex->bright = 1.0;
	tex->contrast = 1.0;
	tex->saturation = 1.0;
	tex->filtersize = 1.0;
	tex->rfac = 1.0;
	tex->gfac = 1.0;
	tex->bfac = 1.0;
	/* newnoise: init. */
	tex->noisebasis = 0;
	tex->noisebasis2 = 0;
	/* musgrave */
	tex->mg_H = 1.0;
	tex->mg_lacunarity = 2.0;
	tex->mg_octaves = 2.0;
	tex->mg_offset = 1.0;
	tex->mg_gain = 1.0;
	tex->ns_outscale = 1.0;
	/* distnoise */
	tex->dist_amount = 1.0;
	/* voronoi */
	tex->vn_w1 = 1.0;
	tex->vn_w2 = tex->vn_w3 = tex->vn_w4 = 0.0;
	tex->vn_mexp = 2.5;
	tex->vn_distm = 0;
	tex->vn_coltype = 0;

	if (tex->env) {
		tex->env->stype = ENV_ANIM;
		tex->env->clipsta = 0.1;
		tex->env->clipend = 100;
		tex->env->cuberes = 512;
		tex->env->depth = 0;
	}

	if (tex->pd) {
		tex->pd->radius = 0.3f;
		tex->pd->falloff_type = TEX_PD_FALLOFF_STD;
	}
	
	if (tex->vd) {
		tex->vd->resol[0] = tex->vd->resol[1] = tex->vd->resol[2] = 0;
		tex->vd->interp_type = TEX_VD_LINEAR;
		tex->vd->file_format = TEX_VD_SMOKE;
	}
	
	if (tex->ot) {
		tex->ot->output = TEX_OCN_DISPLACEMENT;
		tex->ot->object = NULL;
	}
	
	tex->iuser.fie_ima = 2;
	tex->iuser.ok = 1;
	tex->iuser.frames = 100;
	tex->iuser.sfra = 1;
	
	tex->preview = NULL;
}

void BKE_texture_type_set(Tex *tex, int type)
{
	switch (type) {
			
		case TEX_VOXELDATA:
			if (tex->vd == NULL)
				tex->vd = BKE_texture_voxeldata_add();
			break;
		case TEX_POINTDENSITY:
			if (tex->pd == NULL)
				tex->pd = BKE_texture_pointdensity_add();
			break;
		case TEX_ENVMAP:
			if (tex->env == NULL)
				tex->env = BKE_texture_envmap_add();
			break;
		case TEX_OCEAN:
			if (tex->ot == NULL)
				tex->ot = BKE_texture_ocean_add();
			break;
	}
	
	tex->type = type;
}

/* ------------------------------------------------------------------------- */

Tex *BKE_texture_add(Main *bmain, const char *name)
{
	Tex *tex;

	tex = BKE_libblock_alloc(bmain, ID_TE, name, 0);
	
	BKE_texture_default(tex);
	
	return tex;
}

/* ------------------------------------------------------------------------- */

void BKE_texture_mtex_default(MTex *mtex)
{
	mtex->texco = TEXCO_UV;
	mtex->mapto = MAP_COL;
	mtex->object = NULL;
	mtex->projx = PROJ_X;
	mtex->projy = PROJ_Y;
	mtex->projz = PROJ_Z;
	mtex->mapping = MTEX_FLAT;
	mtex->ofs[0] = 0.0;
	mtex->ofs[1] = 0.0;
	mtex->ofs[2] = 0.0;
	mtex->size[0] = 1.0;
	mtex->size[1] = 1.0;
	mtex->size[2] = 1.0;
	mtex->tex = NULL;
	mtex->texflag = MTEX_3TAP_BUMP | MTEX_BUMP_OBJECTSPACE | MTEX_MAPTO_BOUNDS;
	mtex->colormodel = 0;
	mtex->r = 1.0;
	mtex->g = 0.0;
	mtex->b = 1.0;
	mtex->k = 1.0;
	mtex->def_var = 1.0;
	mtex->blendtype = MTEX_BLEND;
	mtex->colfac = 1.0;
	mtex->norfac = 1.0;
	mtex->varfac = 1.0;
	mtex->dispfac = 0.2;
	mtex->colspecfac = 1.0f;
	mtex->mirrfac = 1.0f;
	mtex->alphafac = 1.0f;
	mtex->difffac = 1.0f;
	mtex->specfac = 1.0f;
	mtex->emitfac = 1.0f;
	mtex->hardfac = 1.0f;
	mtex->raymirrfac = 1.0f;
	mtex->translfac = 1.0f;
	mtex->ambfac = 1.0f;
	mtex->colemitfac = 1.0f;
	mtex->colreflfac = 1.0f;
	mtex->coltransfac = 1.0f;
	mtex->densfac = 1.0f;
	mtex->scatterfac = 1.0f;
	mtex->reflfac = 1.0f;
	mtex->shadowfac = 1.0f;
	mtex->zenupfac = 1.0f;
	mtex->zendownfac = 1.0f;
	mtex->blendfac = 1.0f;
	mtex->timefac = 1.0f;
	mtex->lengthfac = 1.0f;
	mtex->clumpfac = 1.0f;
	mtex->kinkfac = 1.0f;
	mtex->kinkampfac = 1.0f;
	mtex->roughfac = 1.0f;
	mtex->twistfac = 1.0f;
	mtex->padensfac = 1.0f;
	mtex->lifefac = 1.0f;
	mtex->sizefac = 1.0f;
	mtex->ivelfac = 1.0f;
	mtex->dampfac = 1.0f;
	mtex->gravityfac = 1.0f;
	mtex->fieldfac = 1.0f;
	mtex->normapspace = MTEX_NSPACE_TANGENT;
	mtex->brush_map_mode = MTEX_MAP_MODE_TILED;
	mtex->random_angle = 2.0f * (float)M_PI;
	mtex->brush_angle_mode = 0;
}


/* ------------------------------------------------------------------------- */

MTex *BKE_texture_mtex_add(void)
{
	MTex *mtex;
	
	mtex = MEM_callocN(sizeof(MTex), "BKE_texture_mtex_add");
	
	BKE_texture_mtex_default(mtex);
	
	return mtex;
}

/* slot -1 for first free ID */
MTex *BKE_texture_mtex_add_id(ID *id, int slot)
{
	MTex **mtex_ar;
	short act;

	give_active_mtex(id, &mtex_ar, &act);

	if (mtex_ar == NULL) {
		return NULL;
	}
	
	if (slot == -1) {
		/* find first free */
		int i;
		for (i = 0; i < MAX_MTEX; i++) {
			if (!mtex_ar[i]) {
				slot = i;
				break;
			}
		}
		if (slot == -1) {
			return NULL;
		}
	}
	else {
		/* make sure slot is valid */
		if (slot < 0 || slot >= MAX_MTEX) {
			return NULL;
		}
	}

	if (mtex_ar[slot]) {
		id_us_min((ID *)mtex_ar[slot]->tex);
		MEM_freeN(mtex_ar[slot]);
		mtex_ar[slot] = NULL;
	}
	else if (GS(id->name) == ID_MA) {
		/* Reset this slot's ON/OFF toggle, for materials, when slot was empty. */
		((Material *)id)->septex &= ~(1 << slot);
	}

	mtex_ar[slot] = BKE_texture_mtex_add();

	return mtex_ar[slot];
}

/* ------------------------------------------------------------------------- */

/**
 * Only copy internal data of Texture ID from source to already allocated/initialized destination.
 * You probably nerver want to use that directly, use id_copy or BKE_id_copy_ex for typical needs.
 *
 * WARNING! This function will not handle ID user count!
 *
 * \param flag  Copying options (see BKE_library.h's LIB_ID_COPY_... flags for more).
 */
void BKE_texture_copy_data(Main *bmain, Tex *tex_dst, const Tex *tex_src, const int flag)
{
	/* We never handle usercount here for own data. */
	const int flag_subdata = flag | LIB_ID_CREATE_NO_USER_REFCOUNT;

	if (!BKE_texture_is_image_user(tex_src)) {
		tex_dst->ima = NULL;
	}

	if (tex_dst->coba) {
		tex_dst->coba = MEM_dupallocN(tex_dst->coba);
	}
	if (tex_dst->env) {
		tex_dst->env = BKE_texture_envmap_copy(tex_dst->env, flag_subdata);
	}
	if (tex_dst->pd) {
		tex_dst->pd = BKE_texture_pointdensity_copy(tex_dst->pd, flag_subdata);
	}
	if (tex_dst->vd) {
		tex_dst->vd = MEM_dupallocN(tex_dst->vd);
	}
	if (tex_dst->ot) {
		tex_dst->ot = BKE_texture_ocean_copy(tex_dst->ot, flag_subdata);
	}

	if (tex_src->nodetree) {
		if (tex_src->nodetree->execdata) {
			ntreeTexEndExecTree(tex_src->nodetree->execdata);
		}
		/* Note: nodetree is *not* in bmain, however this specific case is handled at lower level
		 *       (see BKE_libblock_copy_ex()). */
		BKE_id_copy_ex(bmain, (ID *)tex_src->nodetree, (ID **)&tex_dst->nodetree, flag, false);
	}

	if ((flag & LIB_ID_COPY_NO_PREVIEW) == 0) {
		BKE_previewimg_id_copy(&tex_dst->id, &tex_src->id);
	}
	else {
		tex_dst->preview = NULL;
	}
}

Tex *BKE_texture_copy(Main *bmain, const Tex *tex)
{
	Tex *tex_copy;
	BKE_id_copy_ex(bmain, &tex->id, (ID **)&tex_copy, 0, false);
	return tex_copy;
}

/* texture copy without adding to main dbase */
Tex *BKE_texture_localize(Tex *tex)
{
	/* TODO replace with something like
	 * 	Tex *tex_copy;
	 * 	BKE_id_copy_ex(bmain, &tex->id, (ID **)&tex_copy, LIB_ID_COPY_NO_MAIN | LIB_ID_COPY_NO_PREVIEW | LIB_ID_COPY_NO_USER_REFCOUNT, false);
	 * 	return tex_copy;
	 *
	 * ... Once f*** nodes are fully converted to that too :( */

	Tex *texn;
	
	texn = BKE_libblock_copy_nolib(&tex->id, false);
	
	/* image texture: BKE_texture_free also doesn't decrease */
	
	if (texn->coba) texn->coba = MEM_dupallocN(texn->coba);
	if (texn->env) {
		texn->env = BKE_texture_envmap_copy(texn->env, LIB_ID_CREATE_NO_USER_REFCOUNT);
		id_us_min(&texn->env->ima->id);
	}
	if (texn->pd) texn->pd = BKE_texture_pointdensity_copy(texn->pd, LIB_ID_CREATE_NO_USER_REFCOUNT);
	if (texn->vd) {
		texn->vd = MEM_dupallocN(texn->vd);
		if (texn->vd->dataset)
			texn->vd->dataset = MEM_dupallocN(texn->vd->dataset);
	}
	if (texn->ot) {
		texn->ot = BKE_texture_ocean_copy(tex->ot, LIB_ID_CREATE_NO_USER_REFCOUNT);
	}
	
	texn->preview = NULL;
	
	if (tex->nodetree) {
		texn->nodetree = ntreeLocalize(tex->nodetree);
	}
	
	return texn;
}


/* ------------------------------------------------------------------------- */

void BKE_texture_make_local(Main *bmain, Tex *tex, const bool lib_local)
{
	BKE_id_make_local_generic(bmain, &tex->id, true, lib_local);
}

Tex *give_current_object_texture(Object *ob)
{
	Material *ma, *node_ma;
	Tex *tex = NULL;
	
	if (ob == NULL) return NULL;
	if (ob->totcol == 0 && !(ob->type == OB_LAMP)) return NULL;
	
	if (ob->type == OB_LAMP) {
		tex = give_current_lamp_texture(ob->data);
	}
	else {
		ma = give_current_material(ob, ob->actcol);

		if ((node_ma = give_node_material(ma)))
			ma = node_ma;

		tex = give_current_material_texture(ma);
	}
	
	return tex;
}

Tex *give_current_lamp_texture(Lamp *la)
{
	MTex *mtex = NULL;
	Tex *tex = NULL;

	if (la) {
		mtex = la->mtex[(int)(la->texact)];
		if (mtex) tex = mtex->tex;
	}

	return tex;
}

void set_current_lamp_texture(Lamp *la, Tex *newtex)
{
	int act = la->texact;

	if (la->mtex[act] && la->mtex[act]->tex)
		id_us_min(&la->mtex[act]->tex->id);

	if (newtex) {
		if (!la->mtex[act]) {
			la->mtex[act] = BKE_texture_mtex_add();
			la->mtex[act]->texco = TEXCO_GLOB;
		}
		
		la->mtex[act]->tex = newtex;
		id_us_plus(&newtex->id);
	}
	else if (la->mtex[act]) {
		MEM_freeN(la->mtex[act]);
		la->mtex[act] = NULL;
	}
}

Tex *give_current_linestyle_texture(FreestyleLineStyle *linestyle)
{
	MTex *mtex = NULL;
	Tex *tex = NULL;

	if (linestyle) {
		mtex = linestyle->mtex[(int)(linestyle->texact)];
		if (mtex) tex = mtex->tex;
	}

	return tex;
}

void set_current_linestyle_texture(FreestyleLineStyle *linestyle, Tex *newtex)
{
	int act = linestyle->texact;

	if (linestyle->mtex[act] && linestyle->mtex[act]->tex)
		id_us_min(&linestyle->mtex[act]->tex->id);

	if (newtex) {
		if (!linestyle->mtex[act]) {
			linestyle->mtex[act] = BKE_texture_mtex_add();
			linestyle->mtex[act]->texco = TEXCO_STROKE;
		}

		linestyle->mtex[act]->tex = newtex;
		id_us_plus(&newtex->id);
	}
	else if (linestyle->mtex[act]) {
		MEM_freeN(linestyle->mtex[act]);
		linestyle->mtex[act] = NULL;
	}
}

bNode *give_current_material_texture_node(Material *ma)
{
	if (ma && ma->use_nodes && ma->nodetree)
		return nodeGetActiveID(ma->nodetree, ID_TE);
	
	return NULL;
}

Tex *give_current_material_texture(Material *ma)
{
	MTex *mtex = NULL;
	Tex *tex = NULL;
	bNode *node;
	
	if (ma && ma->use_nodes && ma->nodetree) {
		/* first check texture, then material, this works together
		 * with a hack that clears the active ID flag for textures on
		 * making a material node active */
		node = nodeGetActiveID(ma->nodetree, ID_TE);

		if (node) {
			tex = (Tex *)node->id;
			ma = NULL;
		}
	}

	if (ma) {
		mtex = ma->mtex[(int)(ma->texact)];
		if (mtex) tex = mtex->tex;
	}
	
	return tex;
}

bool give_active_mtex(ID *id, MTex ***mtex_ar, short *act)
{
	switch (GS(id->name)) {
		case ID_MA:
			*mtex_ar =       ((Material *)id)->mtex;
			if (act) *act =  (((Material *)id)->texact);
			break;
		case ID_WO:
			*mtex_ar =       ((World *)id)->mtex;
			if (act) *act =  (((World *)id)->texact);
			break;
		case ID_LA:
			*mtex_ar =       ((Lamp *)id)->mtex;
			if (act) *act =  (((Lamp *)id)->texact);
			break;
		case ID_LS:
			*mtex_ar =       ((FreestyleLineStyle *)id)->mtex;
			if (act) *act =  (((FreestyleLineStyle *)id)->texact);
			break;
		case ID_PA:
			*mtex_ar =       ((ParticleSettings *)id)->mtex;
			if (act) *act =  (((ParticleSettings *)id)->texact);
			break;
		default:
			*mtex_ar = NULL;
			if (act) *act =  0;
			return false;
	}

	return true;
}

void set_active_mtex(ID *id, short act)
{
	if (act < 0) act = 0;
	else if (act >= MAX_MTEX) act = MAX_MTEX - 1;

	switch (GS(id->name)) {
		case ID_MA:
			((Material *)id)->texact = act;
			break;
		case ID_WO:
			((World *)id)->texact = act;
			break;
		case ID_LA:
			((Lamp *)id)->texact = act;
			break;
		case ID_LS:
			((FreestyleLineStyle *)id)->texact = act;
			break;
		case ID_PA:
			((ParticleSettings *)id)->texact = act;
			break;
		default:
			break;
	}
}

void set_current_material_texture(Material *ma, Tex *newtex)
{
	Tex *tex = NULL;
	bNode *node;

	if ((ma->use_nodes && ma->nodetree) &&
	    (node = nodeGetActiveID(ma->nodetree, ID_TE)))
	{
		tex = (Tex *)node->id;
		id_us_min(&tex->id);
		if (newtex) {
			node->id = &newtex->id;
			id_us_plus(&newtex->id);
		}
		else {
			node->id = NULL;
		}
	}
	else {
		int act = (int)ma->texact;

		tex = (ma->mtex[act]) ? ma->mtex[act]->tex : NULL;
		id_us_min(&tex->id);

		if (newtex) {
			if (!ma->mtex[act]) {
				ma->mtex[act] = BKE_texture_mtex_add();
				/* Reset this slot's ON/OFF toggle, for materials, when slot was empty. */
				ma->septex &= ~(1 << act);
				/* For volumes the default UV texture coordinates are not available. */
				if (ma->material_type == MA_TYPE_VOLUME) {
					ma->mtex[act]->texco = TEXCO_ORCO;
				}
			}
			
			ma->mtex[act]->tex = newtex;
			id_us_plus(&newtex->id);
		}
		else if (ma->mtex[act]) {
			MEM_freeN(ma->mtex[act]);
			ma->mtex[act] = NULL;
		}
	}
}

bool has_current_material_texture(Material *ma)
{
	bNode *node;

	if (ma && ma->use_nodes && ma->nodetree) {
		node = nodeGetActiveID(ma->nodetree, ID_TE);

		if (node)
			return true;
	}

	return (ma != NULL);
}

Tex *give_current_world_texture(World *world)
{
	MTex *mtex = NULL;
	Tex *tex = NULL;
	
	if (!world) return NULL;
	
	mtex = world->mtex[(int)(world->texact)];
	if (mtex) tex = mtex->tex;
	
	return tex;
}

void set_current_world_texture(World *wo, Tex *newtex)
{
	int act = wo->texact;

	if (wo->mtex[act] && wo->mtex[act]->tex)
		id_us_min(&wo->mtex[act]->tex->id);

	if (newtex) {
		if (!wo->mtex[act]) {
			wo->mtex[act] = BKE_texture_mtex_add();
			wo->mtex[act]->texco = TEXCO_VIEW;
		}
		
		wo->mtex[act]->tex = newtex;
		id_us_plus(&newtex->id);
	}
	else if (wo->mtex[act]) {
		MEM_freeN(wo->mtex[act]);
		wo->mtex[act] = NULL;
	}
}

Tex *give_current_brush_texture(Brush *br)
{
	return br->mtex.tex;
}

void set_current_brush_texture(Brush *br, Tex *newtex)
{
	if (br->mtex.tex)
		id_us_min(&br->mtex.tex->id);

	if (newtex) {
		br->mtex.tex = newtex;
		id_us_plus(&newtex->id);
	}
}

Tex *give_current_particle_texture(ParticleSettings *part)
{
	MTex *mtex = NULL;
	Tex *tex = NULL;
	
	if (!part) return NULL;
	
	mtex = part->mtex[(int)(part->texact)];
	if (mtex) tex = mtex->tex;
	
	return tex;
}

void set_current_particle_texture(ParticleSettings *part, Tex *newtex)
{
	int act = part->texact;

	if (part->mtex[act] && part->mtex[act]->tex)
		id_us_min(&part->mtex[act]->tex->id);

	if (newtex) {
		if (!part->mtex[act]) {
			part->mtex[act] = BKE_texture_mtex_add();
			part->mtex[act]->texco = TEXCO_ORCO;
			part->mtex[act]->blendtype = MTEX_MUL;
		}
		
		part->mtex[act]->tex = newtex;
		id_us_plus(&newtex->id);
	}
	else if (part->mtex[act]) {
		MEM_freeN(part->mtex[act]);
		part->mtex[act] = NULL;
	}
}

/* ------------------------------------------------------------------------- */

EnvMap *BKE_texture_envmap_add(void)
{
	EnvMap *env;
	
	env = MEM_callocN(sizeof(EnvMap), "envmap");
	env->type = ENV_CUBE;
	env->stype = ENV_ANIM;
	env->clipsta = 0.1;
	env->clipend = 100.0;
	env->cuberes = 512;
	env->viewscale = 0.5;
	
	return env;
} 

/* ------------------------------------------------------------------------- */

EnvMap *BKE_texture_envmap_copy(const EnvMap *env, const int flag)
{
	EnvMap *envn;
	int a;
	
	envn = MEM_dupallocN(env);
	envn->ok = 0;
	for (a = 0; a < 6; a++) {
		envn->cube[a] = NULL;
	}
	if ((flag & LIB_ID_CREATE_NO_USER_REFCOUNT) == 0) {
		id_us_plus((ID *)envn->ima);
	}

	return envn;
}

/* ------------------------------------------------------------------------- */

void BKE_texture_envmap_free_data(EnvMap *env)
{
	unsigned int part;
	
	for (part = 0; part < 6; part++) {
		if (env->cube[part])
			IMB_freeImBuf(env->cube[part]);
		env->cube[part] = NULL;
	}
	env->ok = 0;
}

/* ------------------------------------------------------------------------- */

void BKE_texture_envmap_free(EnvMap *env)
{
	
	BKE_texture_envmap_free_data(env);
	MEM_freeN(env);
	
}

/* ------------------------------------------------------------------------- */

void BKE_texture_pointdensity_init_data(PointDensity *pd)
{
	pd->flag = 0;
	pd->radius = 0.3f;
	pd->falloff_type = TEX_PD_FALLOFF_STD;
	pd->falloff_softness = 2.0;
	pd->source = TEX_PD_PSYS;
	pd->point_tree = NULL;
	pd->point_data = NULL;
	pd->noise_size = 0.5f;
	pd->noise_depth = 1;
	pd->noise_fac = 1.0f;
	pd->noise_influence = TEX_PD_NOISE_STATIC;
	pd->coba = BKE_colorband_add(true);
	pd->speed_scale = 1.0f;
	pd->totpoints = 0;
	pd->object = NULL;
	pd->psys = 0;
	pd->psys_cache_space = TEX_PD_WORLDSPACE;
	pd->falloff_curve = curvemapping_add(1, 0, 0, 1, 1);

	pd->falloff_curve->preset = CURVE_PRESET_LINE;
	pd->falloff_curve->cm->flag &= ~CUMA_EXTEND_EXTRAPOLATE;
	curvemap_reset(pd->falloff_curve->cm, &pd->falloff_curve->clipr, pd->falloff_curve->preset, CURVEMAP_SLOPE_POSITIVE);
	curvemapping_changed(pd->falloff_curve, false);
}

PointDensity *BKE_texture_pointdensity_add(void)
{
	PointDensity *pd = MEM_callocN(sizeof(PointDensity), "pointdensity");
	BKE_texture_pointdensity_init_data(pd);
	return pd;
} 

PointDensity *BKE_texture_pointdensity_copy(const PointDensity *pd, const int UNUSED(flag))
{
	PointDensity *pdn;

	pdn = MEM_dupallocN(pd);
	pdn->point_tree = NULL;
	pdn->point_data = NULL;
	if (pdn->coba) {
		pdn->coba = MEM_dupallocN(pdn->coba);
	}
	pdn->falloff_curve = curvemapping_copy(pdn->falloff_curve); /* can be NULL */
	return pdn;
}

void BKE_texture_pointdensity_free_data(PointDensity *pd)
{
	if (pd->point_tree) {
		BLI_bvhtree_free(pd->point_tree);
		pd->point_tree = NULL;
	}
	if (pd->point_data) {
		MEM_freeN(pd->point_data);
		pd->point_data = NULL;
	}
	if (pd->coba) {
		MEM_freeN(pd->coba);
		pd->coba = NULL;
	}

	curvemapping_free(pd->falloff_curve); /* can be NULL */
}

void BKE_texture_pointdensity_free(PointDensity *pd)
{
	BKE_texture_pointdensity_free_data(pd);
	MEM_freeN(pd);
}

/* ------------------------------------------------------------------------- */

void BKE_texture_voxeldata_free_data(VoxelData *vd)
{
	if (vd->dataset) {
		MEM_freeN(vd->dataset);
		vd->dataset = NULL;
	}

}
 
void BKE_texture_voxeldata_free(VoxelData *vd)
{
	BKE_texture_voxeldata_free_data(vd);
	MEM_freeN(vd);
}
 
VoxelData *BKE_texture_voxeldata_add(void)
{
	VoxelData *vd;

	vd = MEM_callocN(sizeof(VoxelData), "voxeldata");
	vd->dataset = NULL;
	vd->resol[0] = vd->resol[1] = vd->resol[2] = 1;
	vd->interp_type = TEX_VD_LINEAR;
	vd->file_format = TEX_VD_SMOKE;
	vd->int_multiplier = 1.0;
	vd->extend = TEX_CLIP;
	vd->object = NULL;
	vd->cachedframe = -1;
	vd->ok = 0;
	
	return vd;
}
 
VoxelData *BKE_texture_voxeldata_copy(VoxelData *vd)
{
	VoxelData *vdn;

	vdn = MEM_dupallocN(vd);
	vdn->dataset = NULL;

	return vdn;
}

/* ------------------------------------------------------------------------- */

OceanTex *BKE_texture_ocean_add(void)
{
	OceanTex *ot;
	
	ot = MEM_callocN(sizeof(struct OceanTex), "ocean texture");
	ot->output = TEX_OCN_DISPLACEMENT;
	ot->object = NULL;
	
	return ot;
}

OceanTex *BKE_texture_ocean_copy(const OceanTex *ot, const int UNUSED(flag))
{
	OceanTex *otn = MEM_dupallocN(ot);
	
	return otn;
}

void BKE_texture_ocean_free(struct OceanTex *ot)
{
	MEM_freeN(ot);
}

/**
 * \returns true if this texture can use its #Texture.ima (even if its NULL)
 */
bool BKE_texture_is_image_user(const struct Tex *tex)
{
	switch (tex->type) {
		case TEX_IMAGE:
		{
			return true;
		}
		case TEX_ENVMAP:
		{
			if (tex->env) {
				if (tex->env->stype == ENV_LOAD) {
					return true;
				}
			}
			break;
		}
	}

	return false;
}

/* ------------------------------------------------------------------------- */
bool BKE_texture_dependsOnTime(const struct Tex *texture)
{
	if (texture->ima && BKE_image_is_animated(texture->ima)) {
		return true;
	}
	else if (texture->adt) {
		/* assume anything in adt means the texture is animated */
		return true;
	}
	else if (texture->type == TEX_NOISE) {
		/* noise always varies with time */
		return true;
	}
	return false;
}

/* ------------------------------------------------------------------------- */

void BKE_texture_get_value_ex(
        const Scene *scene, Tex *texture,
        float *tex_co, TexResult *texres,
        struct ImagePool *pool,
        bool use_color_management)
{
	int result_type;
	bool do_color_manage = false;

	if (scene && use_color_management) {
		do_color_manage = BKE_scene_check_color_management_enabled(scene);
	}

	/* no node textures for now */
	result_type = multitex_ext_safe(texture, tex_co, texres, pool, do_color_manage, false);

	/* if the texture gave an RGB value, we assume it didn't give a valid
	 * intensity, since this is in the context of modifiers don't use perceptual color conversion.
	 * if the texture didn't give an RGB value, copy the intensity across
	 */
	if (result_type & TEX_RGB) {
		texres->tin = (1.0f / 3.0f) * (texres->tr + texres->tg + texres->tb);
	}
	else {
		copy_v3_fl(&texres->tr, texres->tin);
	}
}

void BKE_texture_get_value(
        const Scene *scene, Tex *texture,
        float *tex_co, TexResult *texres, bool use_color_management)
{
	BKE_texture_get_value_ex(scene, texture, tex_co, texres, NULL, use_color_management);
}

static void texture_nodes_fetch_images_for_pool(bNodeTree *ntree, struct ImagePool *pool)
{
	for (bNode *node = ntree->nodes.first; node; node = node->next) {
		if (node->type == SH_NODE_TEX_IMAGE && node->id != NULL) {
			Image *image = (Image *)node->id;
			BKE_image_pool_acquire_ibuf(image, NULL, pool);
		}
		else if (node->type == NODE_GROUP && node->id != NULL) {
			/* TODO(sergey): Do we need to control recursion here? */
			bNodeTree *nested_tree = (bNodeTree *)node->id;
			texture_nodes_fetch_images_for_pool(nested_tree, pool);
		}
	}
}

/* Make sure all images used by texture are loaded into pool. */
void BKE_texture_fetch_images_for_pool(Tex *texture, struct ImagePool *pool)
{
	if (texture->nodetree != NULL) {
		texture_nodes_fetch_images_for_pool(texture->nodetree, pool);
	}
	else {
		if (texture->type == TEX_IMAGE) {
			if (texture->ima != NULL) {
				BKE_image_pool_acquire_ibuf(texture->ima, NULL, pool);
			}
		}
	}
}
