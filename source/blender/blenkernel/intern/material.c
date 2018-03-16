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

/** \file blender/blenkernel/intern/material.c
 *  \ingroup bke
 */


#include <string.h>
#include <math.h>
#include <stddef.h>

#include "MEM_guardedalloc.h"

#include "DNA_anim_types.h"
#include "DNA_curve_types.h"
#include "DNA_group_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_customdata_types.h"
#include "DNA_ID.h"
#include "DNA_meta_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_math.h"		
#include "BLI_listbase.h"		
#include "BLI_utildefines.h"
#include "BLI_string.h"
#include "BLI_array_utils.h"

#include "BKE_animsys.h"
#include "BKE_displist.h"
#include "BKE_global.h"
#include "BKE_icons.h"
#include "BKE_image.h"
#include "BKE_library.h"
#include "BKE_library_query.h"
#include "BKE_library_remap.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_mesh.h"
#include "BKE_scene.h"
#include "BKE_node.h"
#include "BKE_curve.h"
#include "BKE_editmesh.h"
#include "BKE_font.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"

#include "GPU_material.h"

/* used in UI and render */
Material defmaterial;

/* called on startup, creator.c */
void init_def_material(void)
{
	BKE_material_init(&defmaterial);
}

/** Free (or release) any data used by this material (does not free the material itself). */
void BKE_material_free(Material *ma)
{
	int a;

	BKE_animdata_free((ID *)ma, false);
	
	for (a = 0; a < MAX_MTEX; a++) {
		MEM_SAFE_FREE(ma->mtex[a]);
	}
	
	MEM_SAFE_FREE(ma->ramp_col);
	MEM_SAFE_FREE(ma->ramp_spec);

	/* Free gpu material before the ntree */
	GPU_material_free(&ma->gpumaterial);
	
	/* is no lib link block, but material extension */
	if (ma->nodetree) {
		ntreeFreeTree(ma->nodetree);
		MEM_freeN(ma->nodetree);
		ma->nodetree = NULL;
	}

	MEM_SAFE_FREE(ma->texpaintslot);

	BKE_icon_id_delete((ID *)ma);
	BKE_previewimg_free(&ma->preview);
}

void BKE_material_init(Material *ma)
{
	BLI_assert(MEMCMP_STRUCT_OFS_IS_ZERO(ma, id));

	ma->r = ma->g = ma->b = ma->ref = 0.8;
	ma->specr = ma->specg = ma->specb = 1.0;
	ma->mirr = ma->mirg = ma->mirb = 1.0;
	ma->spectra = 1.0;
	ma->amb = 1.0;
	ma->alpha = 1.0;
	ma->spec = ma->hasize = 0.5;
	ma->har = 50;
	ma->starc = ma->ringc = 4;
	ma->linec = 12;
	ma->flarec = 1;
	ma->flaresize = ma->subsize = 1.0;
	ma->flareboost = 1;
	ma->seed2 = 6;
	ma->friction = 0.5;
	ma->refrac = 4.0;
	ma->roughness = 0.5;
	ma->param[0] = 0.5;
	ma->param[1] = 0.1;
	ma->param[2] = 0.5;
	ma->param[3] = 0.1;
	ma->rms = 0.1;
	ma->darkness = 1.0;

	ma->strand_sta = ma->strand_end = 1.0f;

	ma->ang = 1.0;
	ma->ray_depth = 2;
	ma->ray_depth_tra = 2;
	ma->fresnel_mir = 0.0;
	ma->fresnel_tra = 0.0;
	ma->fresnel_tra_i = 1.25;
	ma->fresnel_mir_i = 1.25;
	ma->tx_limit = 0.0;
	ma->tx_falloff = 1.0;
	ma->shad_alpha = 1.0f;
	ma->vcol_alpha = 0;
	
	ma->gloss_mir = ma->gloss_tra = 1.0;
	ma->samp_gloss_mir = ma->samp_gloss_tra = 18;
	ma->adapt_thresh_mir = ma->adapt_thresh_tra = 0.005;
	ma->dist_mir = 0.0;
	ma->fadeto_mir = MA_RAYMIR_FADETOSKY;
	
	ma->rampfac_col = 1.0;
	ma->rampfac_spec = 1.0;
	ma->pr_lamp = 3;         /* two lamps, is bits */
	ma->pr_type = MA_SPHERE;

	ma->sss_radius[0] = 1.0f;
	ma->sss_radius[1] = 1.0f;
	ma->sss_radius[2] = 1.0f;
	ma->sss_col[0] = 1.0f;
	ma->sss_col[1] = 1.0f;
	ma->sss_col[2] = 1.0f;
	ma->sss_error = 0.05f;
	ma->sss_scale = 0.1f;
	ma->sss_ior = 1.3f;
	ma->sss_colfac = 1.0f;
	ma->sss_texfac = 0.0f;
	ma->sss_front = 1.0f;
	ma->sss_back = 1.0f;

	ma->vol.density = 1.0f;
	ma->vol.emission = 0.0f;
	ma->vol.scattering = 1.0f;
	ma->vol.reflection = 1.0f;
	ma->vol.transmission_col[0] = ma->vol.transmission_col[1] = ma->vol.transmission_col[2] = 1.0f;
	ma->vol.reflection_col[0] = ma->vol.reflection_col[1] = ma->vol.reflection_col[2] = 1.0f;
	ma->vol.emission_col[0] = ma->vol.emission_col[1] = ma->vol.emission_col[2] = 1.0f;
	ma->vol.density_scale = 1.0f;
	ma->vol.depth_cutoff = 0.01f;
	ma->vol.stepsize_type = MA_VOL_STEP_RANDOMIZED;
	ma->vol.stepsize = 0.2f;
	ma->vol.shade_type = MA_VOL_SHADE_SHADED;
	ma->vol.shadeflag |= MA_VOL_PRECACHESHADING;
	ma->vol.precache_resolution = 50;
	ma->vol.ms_spread = 0.2f;
	ma->vol.ms_diff = 1.f;
	ma->vol.ms_intensity = 1.f;
	
	ma->game.flag = GEMAT_BACKCULL;
	ma->game.alpha_blend = 0;
	ma->game.face_orientation = 0;
	
	ma->mode = MA_TRACEBLE | MA_SHADBUF | MA_SHADOW | MA_RAYBIAS | MA_TANGENT_STR | MA_ZTRANSP;
	ma->mode2 = MA_CASTSHADOW;
	ma->shade_flag = MA_APPROX_OCCLUSION;
	ma->preview = NULL;

	ma->alpha_threshold = 0.5f;
}

Material *BKE_material_add(Main *bmain, const char *name)
{
	Material *ma;

	ma = BKE_libblock_alloc(bmain, ID_MA, name, 0);
	
	BKE_material_init(ma);
	
	return ma;
}

/**
 * Only copy internal data of Material ID from source to already allocated/initialized destination.
 * You probably nerver want to use that directly, use id_copy or BKE_id_copy_ex for typical needs.
 *
 * WARNING! This function will not handle ID user count!
 *
 * \param flag  Copying options (see BKE_library.h's LIB_ID_COPY_... flags for more).
 */
void BKE_material_copy_data(Main *bmain, Material *ma_dst, const Material *ma_src, const int flag)
{
	for (int a = 0; a < MAX_MTEX; a++) {
		if (ma_src->mtex[a]) {
			ma_dst->mtex[a] = MEM_mallocN(sizeof(*ma_dst->mtex[a]), __func__);
			*ma_dst->mtex[a] = *ma_src->mtex[a];
		}
	}

	if (ma_src->ramp_col) {
		ma_dst->ramp_col = MEM_dupallocN(ma_src->ramp_col);
	}
	if (ma_src->ramp_spec) {
		ma_dst->ramp_spec = MEM_dupallocN(ma_src->ramp_spec);
	}

	if (ma_src->nodetree) {
		/* Note: nodetree is *not* in bmain, however this specific case is handled at lower level
		 *       (see BKE_libblock_copy_ex()). */
		BKE_id_copy_ex(bmain, (ID *)ma_src->nodetree, (ID **)&ma_dst->nodetree, flag, false);
	}

	if ((flag & LIB_ID_COPY_NO_PREVIEW) == 0) {
		BKE_previewimg_id_copy(&ma_dst->id, &ma_src->id);
	}
	else {
		ma_dst->preview = NULL;
	}

	BLI_listbase_clear(&ma_dst->gpumaterial);

	/* TODO Duplicate Engine Settings and set runtime to NULL */
}

Material *BKE_material_copy(Main *bmain, const Material *ma)
{
	Material *ma_copy;
	BKE_id_copy_ex(bmain, &ma->id, (ID **)&ma_copy, 0, false);
	return ma_copy;
}

/* XXX (see above) material copy without adding to main dbase */
Material *BKE_material_localize(Material *ma)
{
	/* TODO replace with something like
	 * 	Material *ma_copy;
	 * 	BKE_id_copy_ex(bmain, &ma->id, (ID **)&ma_copy, LIB_ID_COPY_NO_MAIN | LIB_ID_COPY_NO_PREVIEW | LIB_ID_COPY_NO_USER_REFCOUNT, false);
	 * 	return ma_copy;
	 *
	 * ... Once f*** nodes are fully converted to that too :( */

	Material *man;
	int a;
	
	man = BKE_libblock_copy_nolib(&ma->id, false);

	/* no increment for texture ID users, in previewrender.c it prevents decrement */
	for (a = 0; a < MAX_MTEX; a++) {
		if (ma->mtex[a]) {
			man->mtex[a] = MEM_mallocN(sizeof(MTex), "copymaterial");
			memcpy(man->mtex[a], ma->mtex[a], sizeof(MTex));
		}
	}
	
	if (ma->ramp_col) man->ramp_col = MEM_dupallocN(ma->ramp_col);
	if (ma->ramp_spec) man->ramp_spec = MEM_dupallocN(ma->ramp_spec);

	man->texpaintslot = NULL;
	man->preview = NULL;
	
	if (ma->nodetree)
		man->nodetree = ntreeLocalize(ma->nodetree);
	
	BLI_listbase_clear(&man->gpumaterial);

	/* TODO Duplicate Engine Settings and set runtime to NULL */
	
	return man;
}

void BKE_material_make_local(Main *bmain, Material *ma, const bool lib_local)
{
	BKE_id_make_local_generic(bmain, &ma->id, true, lib_local);
}

Material ***give_matarar(Object *ob)
{
	Mesh *me;
	Curve *cu;
	MetaBall *mb;
	
	if (ob->type == OB_MESH) {
		me = ob->data;
		return &(me->mat);
	}
	else if (ELEM(ob->type, OB_CURVE, OB_FONT, OB_SURF)) {
		cu = ob->data;
		return &(cu->mat);
	}
	else if (ob->type == OB_MBALL) {
		mb = ob->data;
		return &(mb->mat);
	}
	return NULL;
}

short *give_totcolp(Object *ob)
{
	Mesh *me;
	Curve *cu;
	MetaBall *mb;
	
	if (ob->type == OB_MESH) {
		me = ob->data;
		return &(me->totcol);
	}
	else if (ELEM(ob->type, OB_CURVE, OB_FONT, OB_SURF)) {
		cu = ob->data;
		return &(cu->totcol);
	}
	else if (ob->type == OB_MBALL) {
		mb = ob->data;
		return &(mb->totcol);
	}
	return NULL;
}

/* same as above but for ID's */
Material ***give_matarar_id(ID *id)
{
	/* ensure we don't try get materials from non-obdata */
	BLI_assert(OB_DATA_SUPPORT_ID(GS(id->name)));

	switch (GS(id->name)) {
		case ID_ME:
			return &(((Mesh *)id)->mat);
		case ID_CU:
			return &(((Curve *)id)->mat);
		case ID_MB:
			return &(((MetaBall *)id)->mat);
		default:
			break;
	}
	return NULL;
}

short *give_totcolp_id(ID *id)
{
	/* ensure we don't try get materials from non-obdata */
	BLI_assert(OB_DATA_SUPPORT_ID(GS(id->name)));

	switch (GS(id->name)) {
		case ID_ME:
			return &(((Mesh *)id)->totcol);
		case ID_CU:
			return &(((Curve *)id)->totcol);
		case ID_MB:
			return &(((MetaBall *)id)->totcol);
		default:
			break;
	}
	return NULL;
}

static void material_data_index_remove_id(ID *id, short index)
{
	/* ensure we don't try get materials from non-obdata */
	BLI_assert(OB_DATA_SUPPORT_ID(GS(id->name)));

	switch (GS(id->name)) {
		case ID_ME:
			BKE_mesh_material_index_remove((Mesh *)id, index);
			break;
		case ID_CU:
			BKE_curve_material_index_remove((Curve *)id, index);
			break;
		case ID_MB:
			/* meta-elems don't have materials atm */
			break;
		default:
			break;
	}
}

static void material_data_index_clear_id(ID *id)
{
	/* ensure we don't try get materials from non-obdata */
	BLI_assert(OB_DATA_SUPPORT_ID(GS(id->name)));

	switch (GS(id->name)) {
		case ID_ME:
			BKE_mesh_material_index_clear((Mesh *)id);
			break;
		case ID_CU:
			BKE_curve_material_index_clear((Curve *)id);
			break;
		case ID_MB:
			/* meta-elems don't have materials atm */
			break;
		default:
			break;
	}
}

void BKE_material_resize_id(Main *bmain, ID *id, short totcol, bool do_id_user)
{
	Material ***matar = give_matarar_id(id);
	short *totcolp = give_totcolp_id(id);

	if (matar == NULL) {
		return;
	}

	if (do_id_user && totcol < (*totcolp)) {
		short i;
		for (i = totcol; i < (*totcolp); i++) {
			id_us_min((ID *)(*matar)[i]);
		}
	}

	if (totcol == 0) {
		if (*totcolp) {
			MEM_freeN(*matar);
			*matar = NULL;
		}
	}
	else {
		*matar = MEM_recallocN(*matar, sizeof(void *) * totcol);
	}
	*totcolp = totcol;

	DEG_relations_tag_update(bmain);
}

void BKE_material_append_id(Main *bmain, ID *id, Material *ma)
{
	Material ***matar;
	if ((matar = give_matarar_id(id))) {
		short *totcol = give_totcolp_id(id);
		Material **mat = MEM_callocN(sizeof(void *) * ((*totcol) + 1), "newmatar");
		if (*totcol) memcpy(mat, *matar, sizeof(void *) * (*totcol));
		if (*matar) MEM_freeN(*matar);

		*matar = mat;
		(*matar)[(*totcol)++] = ma;

		id_us_plus((ID *)ma);
		test_all_objects_materials(bmain, id);
		DEG_relations_tag_update(bmain);
	}
}

Material *BKE_material_pop_id(Main *bmain, ID *id, int index_i, bool update_data)
{
	short index = (short)index_i;
	Material *ret = NULL;
	Material ***matar;
	if ((matar = give_matarar_id(id))) {
		short *totcol = give_totcolp_id(id);
		if (index >= 0 && index < (*totcol)) {
			ret = (*matar)[index];
			id_us_min((ID *)ret);

			if (*totcol <= 1) {
				*totcol = 0;
				MEM_freeN(*matar);
				*matar = NULL;
			}
			else {
				if (index + 1 != (*totcol))
					memmove((*matar) + index, (*matar) + (index + 1), sizeof(void *) * ((*totcol) - (index + 1)));

				(*totcol)--;
				*matar = MEM_reallocN(*matar, sizeof(void *) * (*totcol));
				test_all_objects_materials(G.main, id);
			}

			if (update_data) {
				/* decrease mat_nr index */
				material_data_index_remove_id(id, index);
			}

			DEG_relations_tag_update(bmain);
		}
	}
	
	return ret;
}

void BKE_material_clear_id(Main *bmain, ID *id, bool update_data)
{
	Material ***matar;
	if ((matar = give_matarar_id(id))) {
		short *totcol = give_totcolp_id(id);

		while ((*totcol)--) {
			id_us_min((ID *)((*matar)[*totcol]));
		}
		*totcol = 0;
		if (*matar) {
			MEM_freeN(*matar);
			*matar = NULL;
		}

		if (update_data) {
			/* decrease mat_nr index */
			material_data_index_clear_id(id);
		}

		DEG_relations_tag_update(bmain);
	}
}

Material *give_current_material(Object *ob, short act)
{
	Material ***matarar, *ma;
	const short *totcolp;

	if (ob == NULL) return NULL;
	
	/* if object cannot have material, (totcolp == NULL) */
	totcolp = give_totcolp(ob);
	if (totcolp == NULL || ob->totcol == 0) return NULL;

	/* return NULL for invalid 'act', can happen for mesh face indices */
	if (act > ob->totcol)
		return NULL;
	else if (act <= 0) {
		if (act < 0) {
			printf("Negative material index!\n");
		}
		return NULL;
	}

	if (ob->matbits && ob->matbits[act - 1]) {    /* in object */
		ma = ob->mat[act - 1];
	}
	else {                              /* in data */

		/* check for inconsistency */
		if (*totcolp < ob->totcol)
			ob->totcol = *totcolp;
		if (act > ob->totcol) act = ob->totcol;

		matarar = give_matarar(ob);
		
		if (matarar && *matarar) ma = (*matarar)[act - 1];
		else ma = NULL;
		
	}
	
	return ma;
}

Material *give_node_material(Material *ma)
{
	if (ma && ma->use_nodes && ma->nodetree) {
		bNode *node = nodeGetActiveID(ma->nodetree, ID_MA);

		if (node)
			return (Material *)node->id;
	}

	return NULL;
}

void BKE_material_resize_object(Main *bmain, Object *ob, const short totcol, bool do_id_user)
{
	Material **newmatar;
	char *newmatbits;

	if (do_id_user && totcol < ob->totcol) {
		short i;
		for (i = totcol; i < ob->totcol; i++) {
			id_us_min((ID *)ob->mat[i]);
		}
	}

	if (totcol == 0) {
		if (ob->totcol) {
			MEM_freeN(ob->mat);
			MEM_freeN(ob->matbits);
			ob->mat = NULL;
			ob->matbits = NULL;
		}
	}
	else if (ob->totcol < totcol) {
		newmatar = MEM_callocN(sizeof(void *) * totcol, "newmatar");
		newmatbits = MEM_callocN(sizeof(char) * totcol, "newmatbits");
		if (ob->totcol) {
			memcpy(newmatar, ob->mat, sizeof(void *) * ob->totcol);
			memcpy(newmatbits, ob->matbits, sizeof(char) * ob->totcol);
			MEM_freeN(ob->mat);
			MEM_freeN(ob->matbits);
		}
		ob->mat = newmatar;
		ob->matbits = newmatbits;
	}
	/* XXX, why not realloc on shrink? - campbell */

	ob->totcol = totcol;
	if (ob->totcol && ob->actcol == 0) ob->actcol = 1;
	if (ob->actcol > ob->totcol) ob->actcol = ob->totcol;

	DEG_relations_tag_update(bmain);
}

void test_object_materials(Object *ob, ID *id)
{
	/* make the ob mat-array same size as 'ob->data' mat-array */
	const short *totcol;

	if (id == NULL || (totcol = give_totcolp_id(id)) == NULL) {
		return;
	}

	BKE_material_resize_object(G.main, ob, *totcol, false);
}

void test_all_objects_materials(Main *bmain, ID *id)
{
	/* make the ob mat-array same size as 'ob->data' mat-array */
	Object *ob;
	const short *totcol;

	if (id == NULL || (totcol = give_totcolp_id(id)) == NULL) {
		return;
	}

	BKE_main_lock(bmain);
	for (ob = bmain->object.first; ob; ob = ob->id.next) {
		if (ob->data == id) {
			BKE_material_resize_object(bmain, ob, *totcol, false);
		}
	}
	BKE_main_unlock(bmain);
}

void assign_material_id(ID *id, Material *ma, short act)
{
	Material *mao, **matar, ***matarar;
	short *totcolp;

	if (act > MAXMAT) return;
	if (act < 1) act = 1;

	/* this is needed for Python overrides,
	 * we just have to take care that the UI can't do this */
#if 0
	/* prevent crashing when using accidentally */
	BLI_assert(id->lib == NULL);
	if (id->lib) return;
#endif

	/* test arraylens */

	totcolp = give_totcolp_id(id);
	matarar = give_matarar_id(id);

	if (totcolp == NULL || matarar == NULL) return;

	if (act > *totcolp) {
		matar = MEM_callocN(sizeof(void *) * act, "matarray1");

		if (*totcolp) {
			memcpy(matar, *matarar, sizeof(void *) * (*totcolp));
			MEM_freeN(*matarar);
		}

		*matarar = matar;
		*totcolp = act;
	}

	/* in data */
	mao = (*matarar)[act - 1];
	if (mao)
		id_us_min(&mao->id);
	(*matarar)[act - 1] = ma;

	if (ma)
		id_us_plus(&ma->id);

	test_all_objects_materials(G.main, id);
}

void assign_material(Object *ob, Material *ma, short act, int assign_type)
{
	Material *mao, **matar, ***matarar;
	short *totcolp;
	char bit = 0;

	if (act > MAXMAT) return;
	if (act < 1) act = 1;
	
	/* prevent crashing when using accidentally */
	BLI_assert(!ID_IS_LINKED(ob));
	if (ID_IS_LINKED(ob)) return;
	
	/* test arraylens */
	
	totcolp = give_totcolp(ob);
	matarar = give_matarar(ob);
	
	if (totcolp == NULL || matarar == NULL) return;
	
	if (act > *totcolp) {
		matar = MEM_callocN(sizeof(void *) * act, "matarray1");

		if (*totcolp) {
			memcpy(matar, *matarar, sizeof(void *) * (*totcolp));
			MEM_freeN(*matarar);
		}

		*matarar = matar;
		*totcolp = act;
	}

	if (act > ob->totcol) {
		/* Need more space in the material arrays */
		ob->mat = MEM_recallocN_id(ob->mat, sizeof(void *) * act, "matarray2");
		ob->matbits = MEM_recallocN_id(ob->matbits, sizeof(char) * act, "matbits1");
		ob->totcol = act;
	}

	/* Determine the object/mesh linking */
	if (assign_type == BKE_MAT_ASSIGN_EXISTING) {
		/* keep existing option (avoid confusion in scripts),
		 * intentionally ignore userpref (default to obdata). */
		bit = ob->matbits[act - 1];
	}
	else if (assign_type == BKE_MAT_ASSIGN_USERPREF && ob->totcol && ob->actcol) {
		/* copy from previous material */
		bit = ob->matbits[ob->actcol - 1];
	}
	else {
		switch (assign_type) {
			case BKE_MAT_ASSIGN_OBDATA:
				bit = 0;
				break;
			case BKE_MAT_ASSIGN_OBJECT:
				bit = 1;
				break;
			case BKE_MAT_ASSIGN_USERPREF:
			default:
				bit = (U.flag & USER_MAT_ON_OB) ? 1 : 0;
				break;
		}
	}
	
	/* do it */

	ob->matbits[act - 1] = bit;
	if (bit == 1) {   /* in object */
		mao = ob->mat[act - 1];
		if (mao)
			id_us_min(&mao->id);
		ob->mat[act - 1] = ma;
		test_object_materials(ob, ob->data);
	}
	else {  /* in data */
		mao = (*matarar)[act - 1];
		if (mao)
			id_us_min(&mao->id);
		(*matarar)[act - 1] = ma;
		test_all_objects_materials(G.main, ob->data);  /* Data may be used by several objects... */
	}

	if (ma)
		id_us_plus(&ma->id);
}


void BKE_material_remap_object(Object *ob, const unsigned int *remap)
{
	Material ***matar = give_matarar(ob);
	const short *totcol_p = give_totcolp(ob);

	BLI_array_permute(ob->mat, ob->totcol, remap);

	if (ob->matbits) {
		BLI_array_permute(ob->matbits, ob->totcol, remap);
	}

	if (matar) {
		BLI_array_permute(*matar, *totcol_p, remap);
	}

	if (ob->type == OB_MESH) {
		BKE_mesh_material_remap(ob->data, remap, ob->totcol);
	}
	else if (ELEM(ob->type, OB_CURVE, OB_SURF, OB_FONT)) {
		BKE_curve_material_remap(ob->data, remap, ob->totcol);
	}
	else {
		/* add support for this object data! */
		BLI_assert(matar == NULL);
	}
}

/**
 * Calculate a material remapping from \a ob_src to \a ob_dst.
 *
 * \param remap_src_to_dst: An array the size of `ob_src->totcol`
 * where index values are filled in which map to \a ob_dst materials.
 */
void BKE_material_remap_object_calc(
        Object *ob_dst, Object *ob_src,
        short *remap_src_to_dst)
{
	if (ob_src->totcol == 0) {
		return;
	}

	GHash *gh_mat_map = BLI_ghash_ptr_new_ex(__func__, ob_src->totcol);

	for (int i = 0; i < ob_dst->totcol; i++) {
		Material *ma_src = give_current_material(ob_dst, i + 1);
		BLI_ghash_reinsert(gh_mat_map, ma_src, SET_INT_IN_POINTER(i), NULL, NULL);
	}

	/* setup default mapping (when materials don't match) */
	{
		int i = 0;
		if (ob_dst->totcol >= ob_src->totcol) {
			for (; i < ob_src->totcol; i++) {
				remap_src_to_dst[i] = i;
			}
		}
		else {
			for (; i < ob_dst->totcol; i++) {
				remap_src_to_dst[i] = i;
			}
			for (; i < ob_src->totcol; i++) {
				remap_src_to_dst[i] = 0;
			}
		}
	}

	for (int i = 0; i < ob_src->totcol; i++) {
		Material *ma_src = give_current_material(ob_src, i + 1);

		if ((i < ob_dst->totcol) && (ma_src == give_current_material(ob_dst, i + 1))) {
			/* when objects have exact matching materials - keep existing index */
		}
		else {
			void **index_src_p = BLI_ghash_lookup_p(gh_mat_map, ma_src);
			if (index_src_p) {
				remap_src_to_dst[i] = GET_INT_FROM_POINTER(*index_src_p);
			}
		}
	}

	BLI_ghash_free(gh_mat_map, NULL, NULL);
}


/* XXX - this calls many more update calls per object then are needed, could be optimized */
void assign_matarar(struct Object *ob, struct Material ***matar, short totcol)
{
	int actcol_orig = ob->actcol;
	short i;

	while ((ob->totcol > totcol) &&
	       BKE_object_material_slot_remove(ob))
	{
		/* pass */
	}

	/* now we have the right number of slots */
	for (i = 0; i < totcol; i++)
		assign_material(ob, (*matar)[i], i + 1, BKE_MAT_ASSIGN_USERPREF);

	if (actcol_orig > ob->totcol)
		actcol_orig = ob->totcol;

	ob->actcol = actcol_orig;
}


short BKE_object_material_slot_find_index(Object *ob, Material *ma)
{
	Material ***matarar;
	short a, *totcolp;
	
	if (ma == NULL) return 0;
	
	totcolp = give_totcolp(ob);
	matarar = give_matarar(ob);
	
	if (totcolp == NULL || matarar == NULL) return 0;
	
	for (a = 0; a < *totcolp; a++)
		if ((*matarar)[a] == ma)
			break;
	if (a < *totcolp)
		return a + 1;
	return 0;
}

bool BKE_object_material_slot_add(Object *ob)
{
	if (ob == NULL) return false;
	if (ob->totcol >= MAXMAT) return false;
	
	assign_material(ob, NULL, ob->totcol + 1, BKE_MAT_ASSIGN_USERPREF);
	ob->actcol = ob->totcol;
	return true;
}

static void do_init_render_material(Material *ma, int r_mode, float *amb)
{
	MTex *mtex;
	int a, needuv = 0, needtang = 0;
	
	if (ma->flarec == 0) ma->flarec = 1;

	/* add all texcoflags from mtex, texco and mapto were cleared in advance */
	for (a = 0; a < MAX_MTEX; a++) {
		
		/* separate tex switching */
		if (ma->septex & (1 << a)) continue;

		mtex = ma->mtex[a];
		if (mtex && mtex->tex && (mtex->tex->type | (mtex->tex->use_nodes && mtex->tex->nodetree) )) {
			
			ma->texco |= mtex->texco;
			ma->mapto |= mtex->mapto;

			/* always get derivatives for these textures */
			if (ELEM(mtex->tex->type, TEX_IMAGE, TEX_ENVMAP)) ma->texco |= TEXCO_OSA;
			else if (mtex->texflag & (MTEX_COMPAT_BUMP | MTEX_3TAP_BUMP | MTEX_5TAP_BUMP | MTEX_BICUBIC_BUMP)) ma->texco |= TEXCO_OSA;
			
			if (ma->texco & (TEXCO_ORCO | TEXCO_REFL | TEXCO_NORM | TEXCO_STRAND | TEXCO_STRESS)) needuv = 1;
			else if (ma->texco & (TEXCO_GLOB | TEXCO_UV | TEXCO_OBJECT | TEXCO_SPEED)) needuv = 1;
			else if (ma->texco & (TEXCO_LAVECTOR | TEXCO_VIEW)) needuv = 1;

			if ((ma->mapto & MAP_NORM) && (mtex->normapspace == MTEX_NSPACE_TANGENT))
				needtang = 1;
		}
	}

	if (needtang) ma->mode |= MA_NORMAP_TANG;
	else ma->mode &= ~MA_NORMAP_TANG;
	
	if (ma->mode & (MA_VERTEXCOL | MA_VERTEXCOLP)) {
		needuv = 1;
		if (r_mode & R_OSA) ma->texco |= TEXCO_OSA;     /* for texfaces */
	}
	if (needuv) ma->texco |= NEED_UV;
	
	/* since the raytracer doesnt recalc O structs for each ray, we have to preset them all */
	if (r_mode & R_RAYTRACE) {
		if ((ma->mode & (MA_RAYMIRROR | MA_SHADOW_TRA)) || ((ma->mode & MA_TRANSP) && (ma->mode & MA_RAYTRANSP))) {
			ma->texco |= NEED_UV | TEXCO_ORCO | TEXCO_REFL | TEXCO_NORM;
			if (r_mode & R_OSA) ma->texco |= TEXCO_OSA;
		}
	}
	
	if (amb) {
		ma->ambr = ma->amb * amb[0];
		ma->ambg = ma->amb * amb[1];
		ma->ambb = ma->amb * amb[2];
	}

	/* local group override */
	if ((ma->shade_flag & MA_GROUP_LOCAL) && ma->id.lib && ma->group && ma->group->id.lib) {
		Group *group;

		for (group = G.main->group.first; group; group = group->id.next) {
			if (!ID_IS_LINKED(group) && STREQ(group->id.name, ma->group->id.name)) {
				ma->group = group;
			}
		}
	}
}

static void init_render_nodetree(bNodeTree *ntree, Material *basemat, int r_mode, float *amb)
{
	bNode *node;

	/* parses the geom+tex nodes */
	ntreeShaderGetTexcoMode(ntree, r_mode, &basemat->texco, &basemat->mode_l);
	for (node = ntree->nodes.first; node; node = node->next) {
		if (node->id) {
			if (GS(node->id->name) == ID_MA) {
				Material *ma = (Material *)node->id;
				if (ma != basemat) {
					do_init_render_material(ma, r_mode, amb);
					basemat->texco |= ma->texco;
				}

				basemat->mode_l |= ma->mode & ~(MA_MODE_PIPELINE | MA_SHLESS);
				basemat->mode2_l |= ma->mode2 & ~MA_MODE2_PIPELINE;
				/* basemat only considered shadeless if all node materials are too */
				if (!(ma->mode & MA_SHLESS))
					basemat->mode_l &= ~MA_SHLESS;

				if (ma->strand_surfnor > 0.0f)
					basemat->mode_l |= MA_STR_SURFDIFF;
			}
			else if (node->type == NODE_GROUP)
				init_render_nodetree((bNodeTree *)node->id, basemat, r_mode, amb);
		}
		else if (node->typeinfo->type == SH_NODE_NORMAL_MAP) {
			basemat->mode2_l |= MA_TANGENT_CONCRETE;
			NodeShaderNormalMap *nm = node->storage;
			bool taken_into_account = false;
			for (int i = 0; i < basemat->nmap_tangent_names_count; i++) {
				if (STREQ(basemat->nmap_tangent_names[i], nm->uv_map)) {
					taken_into_account = true;
					break;
				}
			}
			if (!taken_into_account) {
				BLI_assert(basemat->nmap_tangent_names_count < MAX_MTFACE + 1);
				strcpy(basemat->nmap_tangent_names[basemat->nmap_tangent_names_count++], nm->uv_map);
			}
		}
	}
}

void init_render_material(Material *mat, int r_mode, float *amb)
{
	
	do_init_render_material(mat, r_mode, amb);
	
	if (mat->nodetree && mat->use_nodes) {
		/* mode_l will take the pipeline options from the main material, and the or-ed
		 * result of non-pipeline options from the nodes. shadeless is an exception,
		 * mode_l will have it set when all node materials are shadeless. */
		mat->mode_l = (mat->mode & MA_MODE_PIPELINE) | MA_SHLESS;
		mat->mode2_l = mat->mode2 & MA_MODE2_PIPELINE;
		mat->nmap_tangent_names_count = 0;
		init_render_nodetree(mat->nodetree, mat, r_mode, amb);
		
		if (!mat->nodetree->execdata)
			mat->nodetree->execdata = ntreeShaderBeginExecTree(mat->nodetree);
	}
	else {
		mat->mode_l = mat->mode;
		mat->mode2_l = mat->mode2;

		if (mat->strand_surfnor > 0.0f)
			mat->mode_l |= MA_STR_SURFDIFF;
	}
}

void init_render_materials(Main *bmain, int r_mode, float *amb, bool do_default_material)
{
	Material *ma;
	
	/* clear these flags before going over materials, to make sure they
	 * are cleared only once, otherwise node materials contained in other
	 * node materials can go wrong */
	for (ma = bmain->mat.first; ma; ma = ma->id.next) {
		if (ma->id.us) {
			ma->texco = 0;
			ma->mapto = 0;
		}
	}

	/* two steps, first initialize, then or the flags for layers */
	for (ma = bmain->mat.first; ma; ma = ma->id.next) {
		/* is_used flag comes back in convertblender.c */
		ma->flag &= ~MA_IS_USED;
		if (ma->id.us) 
			init_render_material(ma, r_mode, amb);
	}

	if (do_default_material) {
		init_render_material(&defmaterial, r_mode, amb);
	}
}

/* only needed for nodes now */
void end_render_material(Material *mat)
{
	if (mat && mat->nodetree && mat->use_nodes) {
		if (mat->nodetree->execdata)
			ntreeShaderEndExecTree(mat->nodetree->execdata);
	}
}

void end_render_materials(Main *bmain)
{
	Material *ma;
	for (ma = bmain->mat.first; ma; ma = ma->id.next)
		if (ma->id.us) 
			end_render_material(ma);
}

static bool material_in_nodetree(bNodeTree *ntree, Material *mat)
{
	bNode *node;

	for (node = ntree->nodes.first; node; node = node->next) {
		if (node->id) {
			if (GS(node->id->name) == ID_MA) {
				if (node->id == (ID *)mat) {
					return true;
				}
			}
			else if (node->type == NODE_GROUP) {
				if (material_in_nodetree((bNodeTree *)node->id, mat)) {
					return true;
				}
			}
		}
	}

	return false;
}

bool material_in_material(Material *parmat, Material *mat)
{
	if (parmat == mat)
		return true;
	else if (parmat->nodetree && parmat->use_nodes)
		return material_in_nodetree(parmat->nodetree, mat);
	else
		return false;
}


/* ****************** */

bool BKE_object_material_slot_remove(Object *ob)
{
	Material *mao, ***matarar;
	short *totcolp;
	short a, actcol;
	
	if (ob == NULL || ob->totcol == 0) {
		return false;
	}

	/* this should never happen and used to crash */
	if (ob->actcol <= 0) {
		printf("%s: invalid material index %d, report a bug!\n", __func__, ob->actcol);
		BLI_assert(0);
		return false;
	}

	/* take a mesh/curve/mball as starting point, remove 1 index,
	 * AND with all objects that share the ob->data
	 * 
	 * after that check indices in mesh/curve/mball!!!
	 */
	
	totcolp = give_totcolp(ob);
	matarar = give_matarar(ob);

	if (ELEM(NULL, matarar, *matarar)) {
		return false;
	}

	/* can happen on face selection in editmode */
	if (ob->actcol > ob->totcol) {
		ob->actcol = ob->totcol;
	}
	
	/* we delete the actcol */
	mao = (*matarar)[ob->actcol - 1];
	if (mao)
		id_us_min(&mao->id);
	
	for (a = ob->actcol; a < ob->totcol; a++)
		(*matarar)[a - 1] = (*matarar)[a];
	(*totcolp)--;
	
	if (*totcolp == 0) {
		MEM_freeN(*matarar);
		*matarar = NULL;
	}
	
	actcol = ob->actcol;

	for (Object *obt = G.main->object.first; obt; obt = obt->id.next) {
		if (obt->data == ob->data) {
			/* Can happen when object material lists are used, see: T52953 */
			if (actcol > obt->totcol) {
				continue;
			}
			/* WATCH IT: do not use actcol from ob or from obt (can become zero) */
			mao = obt->mat[actcol - 1];
			if (mao)
				id_us_min(&mao->id);
		
			for (a = actcol; a < obt->totcol; a++) {
				obt->mat[a - 1] = obt->mat[a];
				obt->matbits[a - 1] = obt->matbits[a];
			}
			obt->totcol--;
			if (obt->actcol > obt->totcol) obt->actcol = obt->totcol;
			
			if (obt->totcol == 0) {
				MEM_freeN(obt->mat);
				MEM_freeN(obt->matbits);
				obt->mat = NULL;
				obt->matbits = NULL;
			}
		}
	}

	/* check indices from mesh */
	if (ELEM(ob->type, OB_MESH, OB_CURVE, OB_SURF, OB_FONT)) {
		material_data_index_remove_id((ID *)ob->data, actcol - 1);
		if (ob->curve_cache) {
			BKE_displist_free(&ob->curve_cache->disp);
		}
	}

	return true;
}

static bool get_mtex_slot_valid_texpaint(struct MTex *mtex)
{
	return (mtex && (mtex->texco == TEXCO_UV) &&
	        mtex->tex && (mtex->tex->type == TEX_IMAGE) &&
	        mtex->tex->ima);
}

static bNode *nodetree_uv_node_recursive(bNode *node)
{
	bNode *inode;
	bNodeSocket *sock;
	
	for (sock = node->inputs.first; sock; sock = sock->next) {
		if (sock->link) {
			inode = sock->link->fromnode;
			if (inode->typeinfo->nclass == NODE_CLASS_INPUT && inode->typeinfo->type == SH_NODE_UVMAP) {
				return inode;
			}
			else {
				return nodetree_uv_node_recursive(inode);
			}
		}
	}
	
	return NULL;
}

void BKE_texpaint_slot_refresh_cache(Scene *scene, Material *ma)
{
	MTex **mtex;
	short count = 0;
	short index = 0, i;

	bool use_nodes = BKE_scene_use_new_shading_nodes(scene);
	bool is_bi = BKE_scene_uses_blender_internal(scene) || BKE_scene_uses_blender_game(scene);

	/* XXX, for 2.8 testing & development its useful to have non Cycles/BI engines use material nodes
	 * In the future we may have some way to check this which each engine can define.
	 * For now use material slots for Clay/Eevee.
	 * - Campbell */
	if (!(use_nodes || is_bi)) {
		is_bi = true;
	}
	
	if (!ma)
		return;

	if (ma->texpaintslot) {
		MEM_freeN(ma->texpaintslot);
		ma->tot_slots = 0;
		ma->texpaintslot = NULL;
	}

	if (scene->toolsettings->imapaint.mode == IMAGEPAINT_MODE_IMAGE) {
		ma->paint_active_slot = 0;
		ma->paint_clone_slot = 0;
		return;
	}
	
	if (use_nodes || ma->use_nodes) {
		bNode *node, *active_node;

		if (!(ma->nodetree)) {
			ma->paint_active_slot = 0;
			ma->paint_clone_slot = 0;
			return;
		}

		for (node = ma->nodetree->nodes.first; node; node = node->next) {
			if (node->typeinfo->nclass == NODE_CLASS_TEXTURE && node->typeinfo->type == SH_NODE_TEX_IMAGE && node->id)
				count++;
		}

		if (count == 0) {
			ma->paint_active_slot = 0;
			ma->paint_clone_slot = 0;
			return;
		}
		ma->texpaintslot = MEM_callocN(sizeof(*ma->texpaintslot) * count, "texpaint_slots");

		active_node = nodeGetActiveTexture(ma->nodetree);

		for (node = ma->nodetree->nodes.first; node; node = node->next) {
			if (node->typeinfo->nclass == NODE_CLASS_TEXTURE && node->typeinfo->type == SH_NODE_TEX_IMAGE && node->id) {
				if (active_node == node)
					ma->paint_active_slot = index;
				ma->texpaintslot[index].ima = (Image *)node->id;
				
				/* for new renderer, we need to traverse the treeback in search of a UV node */
				if (use_nodes) {
					bNode *uvnode = nodetree_uv_node_recursive(node);
					
					if (uvnode) {
						NodeShaderUVMap *storage = (NodeShaderUVMap *)uvnode->storage;
						ma->texpaintslot[index].uvname = storage->uv_map;
						/* set a value to index so UI knows that we have a valid pointer for the mesh */
						ma->texpaintslot[index].index = 0;
					}
					else {
						/* just invalidate the index here so UV map does not get displayed on the UI */
						ma->texpaintslot[index].index = -1;
					}
				}
				else {
					ma->texpaintslot[index].index = -1;
				}
				index++;
			}
		}
	}
	else if (is_bi) {
		for (mtex = ma->mtex, i = 0; i < MAX_MTEX; i++, mtex++) {
			if (get_mtex_slot_valid_texpaint(*mtex)) {
				count++;
			}
		}

		if (count == 0) {
			ma->paint_active_slot = 0;
			ma->paint_clone_slot = 0;
			return;
		}

		ma->texpaintslot = MEM_callocN(sizeof(*ma->texpaintslot) * count, "texpaint_slots");

		for (mtex = ma->mtex, i = 0; i < MAX_MTEX; i++, mtex++) {
			if (get_mtex_slot_valid_texpaint(*mtex)) {
				ma->texpaintslot[index].ima = (*mtex)->tex->ima;
				ma->texpaintslot[index].uvname = (*mtex)->uvname;
				ma->texpaintslot[index].index = i;
				
				index++;
			}
		}
	}
	else {
		ma->paint_active_slot = 0;
		ma->paint_clone_slot = 0;
		return;
	}	


	ma->tot_slots = count;
	
	
	if (ma->paint_active_slot >= count) {
		ma->paint_active_slot = count - 1;
	}

	if (ma->paint_clone_slot >= count) {
		ma->paint_clone_slot = count - 1;
	}

	return;
}

void BKE_texpaint_slots_refresh_object(Scene *scene, struct Object *ob)
{
	int i;

	for (i = 1; i < ob->totcol + 1; i++) {
		Material *ma = give_current_material(ob, i);
		BKE_texpaint_slot_refresh_cache(scene, ma);
	}
}


/* r_col = current value, col = new value, (fac == 0) is no change */
void ramp_blend(int type, float r_col[3], const float fac, const float col[3])
{
	float tmp, facm = 1.0f - fac;
	
	switch (type) {
		case MA_RAMP_BLEND:
			r_col[0] = facm * (r_col[0]) + fac * col[0];
			r_col[1] = facm * (r_col[1]) + fac * col[1];
			r_col[2] = facm * (r_col[2]) + fac * col[2];
			break;
		case MA_RAMP_ADD:
			r_col[0] += fac * col[0];
			r_col[1] += fac * col[1];
			r_col[2] += fac * col[2];
			break;
		case MA_RAMP_MULT:
			r_col[0] *= (facm + fac * col[0]);
			r_col[1] *= (facm + fac * col[1]);
			r_col[2] *= (facm + fac * col[2]);
			break;
		case MA_RAMP_SCREEN:
			r_col[0] = 1.0f - (facm + fac * (1.0f - col[0])) * (1.0f - r_col[0]);
			r_col[1] = 1.0f - (facm + fac * (1.0f - col[1])) * (1.0f - r_col[1]);
			r_col[2] = 1.0f - (facm + fac * (1.0f - col[2])) * (1.0f - r_col[2]);
			break;
		case MA_RAMP_OVERLAY:
			if (r_col[0] < 0.5f)
				r_col[0] *= (facm + 2.0f * fac * col[0]);
			else
				r_col[0] = 1.0f - (facm + 2.0f * fac * (1.0f - col[0])) * (1.0f - r_col[0]);
			if (r_col[1] < 0.5f)
				r_col[1] *= (facm + 2.0f * fac * col[1]);
			else
				r_col[1] = 1.0f - (facm + 2.0f * fac * (1.0f - col[1])) * (1.0f - r_col[1]);
			if (r_col[2] < 0.5f)
				r_col[2] *= (facm + 2.0f * fac * col[2]);
			else
				r_col[2] = 1.0f - (facm + 2.0f * fac * (1.0f - col[2])) * (1.0f - r_col[2]);
			break;
		case MA_RAMP_SUB:
			r_col[0] -= fac * col[0];
			r_col[1] -= fac * col[1];
			r_col[2] -= fac * col[2];
			break;
		case MA_RAMP_DIV:
			if (col[0] != 0.0f)
				r_col[0] = facm * (r_col[0]) + fac * (r_col[0]) / col[0];
			if (col[1] != 0.0f)
				r_col[1] = facm * (r_col[1]) + fac * (r_col[1]) / col[1];
			if (col[2] != 0.0f)
				r_col[2] = facm * (r_col[2]) + fac * (r_col[2]) / col[2];
			break;
		case MA_RAMP_DIFF:
			r_col[0] = facm * (r_col[0]) + fac * fabsf(r_col[0] - col[0]);
			r_col[1] = facm * (r_col[1]) + fac * fabsf(r_col[1] - col[1]);
			r_col[2] = facm * (r_col[2]) + fac * fabsf(r_col[2] - col[2]);
			break;
		case MA_RAMP_DARK:
			r_col[0] = min_ff(r_col[0], col[0]) * fac + r_col[0] * facm;
			r_col[1] = min_ff(r_col[1], col[1]) * fac + r_col[1] * facm;
			r_col[2] = min_ff(r_col[2], col[2]) * fac + r_col[2] * facm;
			break;
		case MA_RAMP_LIGHT:
			tmp = fac * col[0];
			if (tmp > r_col[0]) r_col[0] = tmp;
			tmp = fac * col[1];
			if (tmp > r_col[1]) r_col[1] = tmp;
			tmp = fac * col[2];
			if (tmp > r_col[2]) r_col[2] = tmp;
			break;
		case MA_RAMP_DODGE:
			if (r_col[0] != 0.0f) {
				tmp = 1.0f - fac * col[0];
				if (tmp <= 0.0f)
					r_col[0] = 1.0f;
				else if ((tmp = (r_col[0]) / tmp) > 1.0f)
					r_col[0] = 1.0f;
				else
					r_col[0] = tmp;
			}
			if (r_col[1] != 0.0f) {
				tmp = 1.0f - fac * col[1];
				if (tmp <= 0.0f)
					r_col[1] = 1.0f;
				else if ((tmp = (r_col[1]) / tmp) > 1.0f)
					r_col[1] = 1.0f;
				else
					r_col[1] = tmp;
			}
			if (r_col[2] != 0.0f) {
				tmp = 1.0f - fac * col[2];
				if (tmp <= 0.0f)
					r_col[2] = 1.0f;
				else if ((tmp = (r_col[2]) / tmp) > 1.0f)
					r_col[2] = 1.0f;
				else
					r_col[2] = tmp;
			}
			break;
		case MA_RAMP_BURN:
			tmp = facm + fac * col[0];

			if (tmp <= 0.0f)
				r_col[0] = 0.0f;
			else if ((tmp = (1.0f - (1.0f - (r_col[0])) / tmp)) < 0.0f)
				r_col[0] = 0.0f;
			else if (tmp > 1.0f)
				r_col[0] = 1.0f;
			else
				r_col[0] = tmp;

			tmp = facm + fac * col[1];
			if (tmp <= 0.0f)
				r_col[1] = 0.0f;
			else if ((tmp = (1.0f - (1.0f - (r_col[1])) / tmp)) < 0.0f)
				r_col[1] = 0.0f;
			else if (tmp > 1.0f)
				r_col[1] = 1.0f;
			else
				r_col[1] = tmp;

			tmp = facm + fac * col[2];
			if (tmp <= 0.0f)
				r_col[2] = 0.0f;
			else if ((tmp = (1.0f - (1.0f - (r_col[2])) / tmp)) < 0.0f)
				r_col[2] = 0.0f;
			else if (tmp > 1.0f)
				r_col[2] = 1.0f;
			else
				r_col[2] = tmp;
			break;
		case MA_RAMP_HUE:
		{
			float rH, rS, rV;
			float colH, colS, colV;
			float tmpr, tmpg, tmpb;
			rgb_to_hsv(col[0], col[1], col[2], &colH, &colS, &colV);
			if (colS != 0) {
				rgb_to_hsv(r_col[0], r_col[1], r_col[2], &rH, &rS, &rV);
				hsv_to_rgb(colH, rS, rV, &tmpr, &tmpg, &tmpb);
				r_col[0] = facm * (r_col[0]) + fac * tmpr;
				r_col[1] = facm * (r_col[1]) + fac * tmpg;
				r_col[2] = facm * (r_col[2]) + fac * tmpb;
			}
			break;
		}
		case MA_RAMP_SAT:
		{
			float rH, rS, rV;
			float colH, colS, colV;
			rgb_to_hsv(r_col[0], r_col[1], r_col[2], &rH, &rS, &rV);
			if (rS != 0) {
				rgb_to_hsv(col[0], col[1], col[2], &colH, &colS, &colV);
				hsv_to_rgb(rH, (facm * rS + fac * colS), rV, r_col + 0, r_col + 1, r_col + 2);
			}
			break;
		}
		case MA_RAMP_VAL:
		{
			float rH, rS, rV;
			float colH, colS, colV;
			rgb_to_hsv(r_col[0], r_col[1], r_col[2], &rH, &rS, &rV);
			rgb_to_hsv(col[0], col[1], col[2], &colH, &colS, &colV);
			hsv_to_rgb(rH, rS, (facm * rV + fac * colV), r_col + 0, r_col + 1, r_col + 2);
			break;
		}
		case MA_RAMP_COLOR:
		{
			float rH, rS, rV;
			float colH, colS, colV;
			float tmpr, tmpg, tmpb;
			rgb_to_hsv(col[0], col[1], col[2], &colH, &colS, &colV);
			if (colS != 0) {
				rgb_to_hsv(r_col[0], r_col[1], r_col[2], &rH, &rS, &rV);
				hsv_to_rgb(colH, colS, rV, &tmpr, &tmpg, &tmpb);
				r_col[0] = facm * (r_col[0]) + fac * tmpr;
				r_col[1] = facm * (r_col[1]) + fac * tmpg;
				r_col[2] = facm * (r_col[2]) + fac * tmpb;
			}
			break;
		}
		case MA_RAMP_SOFT:
		{
			float scr, scg, scb;

			/* first calculate non-fac based Screen mix */
			scr = 1.0f - (1.0f - col[0]) * (1.0f - r_col[0]);
			scg = 1.0f - (1.0f - col[1]) * (1.0f - r_col[1]);
			scb = 1.0f - (1.0f - col[2]) * (1.0f - r_col[2]);

			r_col[0] = facm * (r_col[0]) + fac * (((1.0f - r_col[0]) * col[0] * (r_col[0])) + (r_col[0] * scr));
			r_col[1] = facm * (r_col[1]) + fac * (((1.0f - r_col[1]) * col[1] * (r_col[1])) + (r_col[1] * scg));
			r_col[2] = facm * (r_col[2]) + fac * (((1.0f - r_col[2]) * col[2] * (r_col[2])) + (r_col[2] * scb));
			break;
		}
		case MA_RAMP_LINEAR:
			if (col[0] > 0.5f)
				r_col[0] = r_col[0] + fac * (2.0f * (col[0] - 0.5f));
			else
				r_col[0] = r_col[0] + fac * (2.0f * (col[0]) - 1.0f);
			if (col[1] > 0.5f)
				r_col[1] = r_col[1] + fac * (2.0f * (col[1] - 0.5f));
			else
				r_col[1] = r_col[1] + fac * (2.0f * (col[1]) - 1.0f);
			if (col[2] > 0.5f)
				r_col[2] = r_col[2] + fac * (2.0f * (col[2] - 0.5f));
			else
				r_col[2] = r_col[2] + fac * (2.0f * (col[2]) - 1.0f);
			break;
	}
}

/**
 * \brief copy/paste buffer, if we had a proper py api that would be better
 * \note matcopybuf.nodetree does _NOT_ use ID's
 * \todo matcopybuf.nodetree's  node->id's are NOT validated, this will crash!
 */
static Material matcopybuf;
static short matcopied = 0;

void clear_matcopybuf(void)
{
	memset(&matcopybuf, 0, sizeof(Material));
	matcopied = 0;
}

void free_matcopybuf(void)
{
	int a;

	for (a = 0; a < MAX_MTEX; a++) {
		if (matcopybuf.mtex[a]) {
			MEM_freeN(matcopybuf.mtex[a]);
			matcopybuf.mtex[a] = NULL;
		}
	}

	if (matcopybuf.ramp_col) MEM_freeN(matcopybuf.ramp_col);
	if (matcopybuf.ramp_spec) MEM_freeN(matcopybuf.ramp_spec);

	matcopybuf.ramp_col = NULL;
	matcopybuf.ramp_spec = NULL;

	if (matcopybuf.nodetree) {
		ntreeFreeTree(matcopybuf.nodetree);
		MEM_freeN(matcopybuf.nodetree);
		matcopybuf.nodetree = NULL;
	}

	matcopied = 0;
}

void copy_matcopybuf(Material *ma)
{
	int a;
	MTex *mtex;

	if (matcopied)
		free_matcopybuf();

	memcpy(&matcopybuf, ma, sizeof(Material));
	if (matcopybuf.ramp_col) matcopybuf.ramp_col = MEM_dupallocN(matcopybuf.ramp_col);
	if (matcopybuf.ramp_spec) matcopybuf.ramp_spec = MEM_dupallocN(matcopybuf.ramp_spec);

	for (a = 0; a < MAX_MTEX; a++) {
		mtex = matcopybuf.mtex[a];
		if (mtex) {
			matcopybuf.mtex[a] = MEM_dupallocN(mtex);
		}
	}
	matcopybuf.nodetree = ntreeCopyTree_ex(ma->nodetree, G.main, false);
	matcopybuf.preview = NULL;
	BLI_listbase_clear(&matcopybuf.gpumaterial);
	/* TODO Duplicate Engine Settings and set runtime to NULL */
	matcopied = 1;
}

void paste_matcopybuf(Material *ma)
{
	int a;
	MTex *mtex;
	ID id;

	if (matcopied == 0)
		return;
	/* free current mat */
	if (ma->ramp_col) MEM_freeN(ma->ramp_col);
	if (ma->ramp_spec) MEM_freeN(ma->ramp_spec);
	for (a = 0; a < MAX_MTEX; a++) {
		mtex = ma->mtex[a];
		if (mtex && mtex->tex)
			id_us_min(&mtex->tex->id);
		if (mtex)
			MEM_freeN(mtex);
	}

	/* Free gpu material before the ntree */
	GPU_material_free(&ma->gpumaterial);

	if (ma->nodetree) {
		ntreeFreeTree(ma->nodetree);
		MEM_freeN(ma->nodetree);
	}

	id = (ma->id);
	memcpy(ma, &matcopybuf, sizeof(Material));
	(ma->id) = id;

	if (matcopybuf.ramp_col) ma->ramp_col = MEM_dupallocN(matcopybuf.ramp_col);
	if (matcopybuf.ramp_spec) ma->ramp_spec = MEM_dupallocN(matcopybuf.ramp_spec);

	for (a = 0; a < MAX_MTEX; a++) {
		mtex = ma->mtex[a];
		if (mtex) {
			ma->mtex[a] = MEM_dupallocN(mtex);
			if (mtex->tex) {
				/* first check this is in main (we may have loaded another file) [#35500] */
				if (BLI_findindex(&G.main->tex, mtex->tex) != -1) {
					id_us_plus((ID *)mtex->tex);
				}
				else {
					ma->mtex[a]->tex = NULL;
				}
			}
		}
	}

	ma->nodetree = ntreeCopyTree_ex(matcopybuf.nodetree, G.main, false);
}

struct Image *BKE_object_material_edit_image_get(Object *ob, short mat_nr)
{
	Material *ma = give_current_material(ob, mat_nr + 1);
	return ma ? ma->edit_image : NULL;
}

struct Image **BKE_object_material_edit_image_get_array(Object *ob)
{
	Image **image_array = MEM_mallocN(sizeof(Material *) * ob->totcol, __func__);
	for (int i = 0; i < ob->totcol; i++) {
		image_array[i] = BKE_object_material_edit_image_get(ob, i);
	}
	return image_array;
}

bool BKE_object_material_edit_image_set(Object *ob, short mat_nr, Image *image)
{
	Material *ma = give_current_material(ob, mat_nr + 1);
	if (ma) {
		/* both may be NULL */
		id_us_min((ID *)ma->edit_image);
		ma->edit_image = image;
		id_us_plus((ID *)ma->edit_image);
		return true;
	}
	return false;
}

void BKE_material_eval(const struct EvaluationContext *UNUSED(eval_ctx), Material *material)
{
	DEG_debug_print_eval(__func__, material->id.name, material);
	if ((BLI_listbase_is_empty(&material->gpumaterial) == false)) {
		GPU_material_uniform_buffer_tag_dirty(&material->gpumaterial);
	}
}
