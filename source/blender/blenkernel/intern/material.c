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
#include "BLI_bpath.h"
#include "BLI_string.h"

#include "BKE_animsys.h"
#include "BKE_displist.h"
#include "BKE_global.h"
#include "BKE_icons.h"
#include "BKE_image.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_mesh.h"
#include "BKE_node.h"
#include "BKE_curve.h"

#include "GPU_material.h"

/* used in UI and render */
Material defmaterial;

/* called on startup, creator.c */
void init_def_material(void)
{
	init_material(&defmaterial);
}

/* not material itself */
void BKE_material_free(Material *ma)
{
	MTex *mtex;
	int a;
	
	for (a = 0; a < MAX_MTEX; a++) {
		mtex = ma->mtex[a];
		if (mtex && mtex->tex) mtex->tex->id.us--;
		if (mtex) MEM_freeN(mtex);
	}
	
	if (ma->ramp_col) MEM_freeN(ma->ramp_col);
	if (ma->ramp_spec) MEM_freeN(ma->ramp_spec);
	
	BKE_free_animdata((ID *)ma);
	
	if (ma->preview)
		BKE_previewimg_free(&ma->preview);
	BKE_icon_delete((struct ID *)ma);
	ma->id.icon_id = 0;
	
	/* is no lib link block, but material extension */
	if (ma->nodetree) {
		ntreeFreeTree(ma->nodetree);
		MEM_freeN(ma->nodetree);
	}

	if (ma->gpumaterial.first)
		GPU_material_free(ma);
}

void init_material(Material *ma)
{
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
	ma->shade_flag = MA_APPROX_OCCLUSION;
	ma->preview = NULL;
}

Material *BKE_material_add(const char *name)
{
	Material *ma;

	ma = BKE_libblock_alloc(&G.main->mat, ID_MA, name);
	
	init_material(ma);
	
	return ma;	
}

/* XXX keep synced with next function */
Material *BKE_material_copy(Material *ma)
{
	Material *man;
	int a;
	
	man = BKE_libblock_copy(&ma->id);
	
	id_lib_extern((ID *)man->group);
	
	for (a = 0; a < MAX_MTEX; a++) {
		if (ma->mtex[a]) {
			man->mtex[a] = MEM_mallocN(sizeof(MTex), "copymaterial");
			memcpy(man->mtex[a], ma->mtex[a], sizeof(MTex));
			id_us_plus((ID *)man->mtex[a]->tex);
		}
	}
	
	if (ma->ramp_col) man->ramp_col = MEM_dupallocN(ma->ramp_col);
	if (ma->ramp_spec) man->ramp_spec = MEM_dupallocN(ma->ramp_spec);
	
	if (ma->preview) man->preview = BKE_previewimg_copy(ma->preview);

	if (ma->nodetree) {
		man->nodetree = ntreeCopyTree(ma->nodetree); /* 0 == full new tree */
	}

	man->gpumaterial.first = man->gpumaterial.last = NULL;
	
	return man;
}

/* XXX (see above) material copy without adding to main dbase */
Material *localize_material(Material *ma)
{
	Material *man;
	int a;
	
	man = BKE_libblock_copy(&ma->id);
	BLI_remlink(&G.main->mat, man);

	/* no increment for texture ID users, in previewrender.c it prevents decrement */
	for (a = 0; a < MAX_MTEX; a++) {
		if (ma->mtex[a]) {
			man->mtex[a] = MEM_mallocN(sizeof(MTex), "copymaterial");
			memcpy(man->mtex[a], ma->mtex[a], sizeof(MTex));
		}
	}
	
	if (ma->ramp_col) man->ramp_col = MEM_dupallocN(ma->ramp_col);
	if (ma->ramp_spec) man->ramp_spec = MEM_dupallocN(ma->ramp_spec);
	
	man->preview = NULL;
	
	if (ma->nodetree)
		man->nodetree = ntreeLocalize(ma->nodetree);
	
	man->gpumaterial.first = man->gpumaterial.last = NULL;
	
	return man;
}

static void extern_local_material(Material *ma)
{
	int i;
	for (i = 0; i < MAX_MTEX; i++) {
		if (ma->mtex[i]) id_lib_extern((ID *)ma->mtex[i]->tex);
	}
}

void BKE_material_make_local(Material *ma)
{
	Main *bmain = G.main;
	Object *ob;
	Mesh *me;
	Curve *cu;
	MetaBall *mb;
	int a, is_local = FALSE, is_lib = FALSE;

	/* - only lib users: do nothing
	 * - only local users: set flag
	 * - mixed: make copy
	 */
	
	if (ma->id.lib == NULL) return;

	/* One local user; set flag and return. */
	if (ma->id.us == 1) {
		id_clear_lib_data(bmain, &ma->id);
		extern_local_material(ma);
		return;
	}

	/* Check which other IDs reference this one to determine if it's used by
	 * lib or local */
	/* test objects */
	ob = bmain->object.first;
	while (ob) {
		if (ob->mat) {
			for (a = 0; a < ob->totcol; a++) {
				if (ob->mat[a] == ma) {
					if (ob->id.lib) is_lib = TRUE;
					else is_local = TRUE;
				}
			}
		}
		ob = ob->id.next;
	}
	/* test meshes */
	me = bmain->mesh.first;
	while (me) {
		if (me->mat) {
			for (a = 0; a < me->totcol; a++) {
				if (me->mat[a] == ma) {
					if (me->id.lib) is_lib = TRUE;
					else is_local = TRUE;
				}
			}
		}
		me = me->id.next;
	}
	/* test curves */
	cu = bmain->curve.first;
	while (cu) {
		if (cu->mat) {
			for (a = 0; a < cu->totcol; a++) {
				if (cu->mat[a] == ma) {
					if (cu->id.lib) is_lib = TRUE;
					else is_local = TRUE;
				}
			}
		}
		cu = cu->id.next;
	}
	/* test mballs */
	mb = bmain->mball.first;
	while (mb) {
		if (mb->mat) {
			for (a = 0; a < mb->totcol; a++) {
				if (mb->mat[a] == ma) {
					if (mb->id.lib) is_lib = TRUE;
					else is_local = TRUE;
				}
			}
		}
		mb = mb->id.next;
	}

	/* Only local users. */
	if (is_local && is_lib == FALSE) {
		id_clear_lib_data(bmain, &ma->id);
		extern_local_material(ma);
	}
	/* Both user and local, so copy. */
	else if (is_local && is_lib) {
		Material *ma_new = BKE_material_copy(ma);

		ma_new->id.us = 0;

		/* Remap paths of new ID using old library as base. */
		BKE_id_lib_local_paths(bmain, ma->id.lib, &ma_new->id);

		/* do objects */
		ob = bmain->object.first;
		while (ob) {
			if (ob->mat) {
				for (a = 0; a < ob->totcol; a++) {
					if (ob->mat[a] == ma) {
						if (ob->id.lib == NULL) {
							ob->mat[a] = ma_new;
							ma_new->id.us++;
							ma->id.us--;
						}
					}
				}
			}
			ob = ob->id.next;
		}
		/* do meshes */
		me = bmain->mesh.first;
		while (me) {
			if (me->mat) {
				for (a = 0; a < me->totcol; a++) {
					if (me->mat[a] == ma) {
						if (me->id.lib == NULL) {
							me->mat[a] = ma_new;
							ma_new->id.us++;
							ma->id.us--;
						}
					}
				}
			}
			me = me->id.next;
		}
		/* do curves */
		cu = bmain->curve.first;
		while (cu) {
			if (cu->mat) {
				for (a = 0; a < cu->totcol; a++) {
					if (cu->mat[a] == ma) {
						if (cu->id.lib == NULL) {
							cu->mat[a] = ma_new;
							ma_new->id.us++;
							ma->id.us--;
						}
					}
				}
			}
			cu = cu->id.next;
		}
		/* do mballs */
		mb = bmain->mball.first;
		while (mb) {
			if (mb->mat) {
				for (a = 0; a < mb->totcol; a++) {
					if (mb->mat[a] == ma) {
						if (mb->id.lib == NULL) {
							mb->mat[a] = ma_new;
							ma_new->id.us++;
							ma->id.us--;
						}
					}
				}
			}
			mb = mb->id.next;
		}
	}
}

/* for curve, mball, mesh types */
void extern_local_matarar(struct Material **matar, short totcol)
{
	short i;
	for (i = 0; i < totcol; i++) {
		id_lib_extern((ID *)matar[i]);
	}
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
	else if (ELEM3(ob->type, OB_CURVE, OB_FONT, OB_SURF)) {
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
	else if (ELEM3(ob->type, OB_CURVE, OB_FONT, OB_SURF)) {
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
	switch (GS(id->name)) {
		case ID_ME:
			return &(((Mesh *)id)->mat);
			break;
		case ID_CU:
			return &(((Curve *)id)->mat);
			break;
		case ID_MB:
			return &(((MetaBall *)id)->mat);
			break;
	}
	return NULL;
}

short *give_totcolp_id(ID *id)
{
	switch (GS(id->name)) {
		case ID_ME:
			return &(((Mesh *)id)->totcol);
			break;
		case ID_CU:
			return &(((Curve *)id)->totcol);
			break;
		case ID_MB:
			return &(((MetaBall *)id)->totcol);
			break;
	}
	return NULL;
}

static void data_delete_material_index_id(ID *id, short index)
{
	switch (GS(id->name)) {
		case ID_ME:
			BKE_mesh_delete_material_index((Mesh *)id, index);
			break;
		case ID_CU:
			BKE_curve_delete_material_index((Curve *)id, index);
			break;
		case ID_MB:
			/* meta-elems don't have materials atm */
			break;
	}
}

void material_append_id(ID *id, Material *ma)
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
		test_object_materials(id);
	}
}

Material *material_pop_id(ID *id, int index_i, int remove_material_slot)
{
	short index = (short)index_i;
	Material *ret = NULL;
	Material ***matar;
	if ((matar = give_matarar_id(id))) {
		short *totcol = give_totcolp_id(id);
		if (index >= 0 && index < (*totcol)) {
			ret = (*matar)[index];
			id_us_min((ID *)ret);

			if (remove_material_slot) {
				if (*totcol <= 1) {
					*totcol = 0;
					MEM_freeN(*matar);
					*matar = NULL;
				}
				else {
					Material **mat;
					if (index + 1 != (*totcol))
						memmove((*matar) + index, (*matar) + (index + 1), sizeof(void *) * ((*totcol) - (index + 1)));

					(*totcol)--;
					
					mat = MEM_callocN(sizeof(void *) * (*totcol), "newmatar");
					memcpy(mat, *matar, sizeof(void *) * (*totcol));
					MEM_freeN(*matar);

					*matar = mat;
					test_object_materials(id);
				}

				/* decrease mat_nr index */
				data_delete_material_index_id(id, index);
			}

			/* don't remove material slot, only clear it*/
			else
				(*matar)[index] = NULL;
		}
	}
	
	return ret;
}

Material *give_current_material(Object *ob, short act)
{
	Material ***matarar, *ma;
	short *totcolp;
	
	if (ob == NULL) return NULL;
	
	/* if object cannot have material, (totcolp == NULL) */
	totcolp = give_totcolp(ob);
	if (totcolp == NULL || ob->totcol == 0) return NULL;
	
	if (act < 0) {
		printf("no!\n");
	}
	
	if (act > ob->totcol) act = ob->totcol;
	else if (act <= 0) act = 1;

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

ID *material_from(Object *ob, short act)
{

	if (ob == NULL) return NULL;

	if (ob->totcol == 0) return ob->data;
	if (act == 0) act = 1;

	if (ob->matbits[act - 1]) return (ID *)ob;
	else return ob->data;
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

/* GS reads the memory pointed at in a specific ordering. There are,
 * however two definitions for it. I have jotted them down here, both,
 * but I think the first one is actually used. The thing is that
 * big-endian systems might read this the wrong way round. OTOH, we
 * constructed the IDs that are read out with this macro explicitly as
 * well. I expect we'll sort it out soon... */

/* from blendef: */
#define GS(a)   (*((short *)(a)))

/* from misc_util: flip the bytes from x  */
/*  #define GS(x) (((unsigned char *)(x))[0] << 8 | ((unsigned char *)(x))[1]) */

void resize_object_material(Object *ob, const short totcol)
{
	Material **newmatar;
	char *newmatbits;

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
	ob->totcol = totcol;
	if (ob->totcol && ob->actcol == 0) ob->actcol = 1;
	if (ob->actcol > ob->totcol) ob->actcol = ob->totcol;
}

void test_object_materials(ID *id)
{
	/* make the ob mat-array same size as 'ob->data' mat-array */
	Object *ob;
	short *totcol;

	if (id == NULL || (totcol = give_totcolp_id(id)) == NULL) {
		return;
	}

	for (ob = G.main->object.first; ob; ob = ob->id.next) {
		if (ob->data == id) {
			resize_object_material(ob, *totcol);
		}
	}
}

void assign_material_id(ID *id, Material *ma, short act)
{
	Material *mao, **matar, ***matarar;
	short *totcolp;

	if (act > MAXMAT) return;
	if (act < 1) act = 1;

	/* prevent crashing when using accidentally */
	BLI_assert(id->lib == NULL);
	if (id->lib) return;

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
	if (mao) mao->id.us--;
	(*matarar)[act - 1] = ma;

	if (ma)
		id_us_plus((ID *)ma);

	test_object_materials(id);
}

void assign_material(Object *ob, Material *ma, short act)
{
	Material *mao, **matar, ***matarar;
	char *matbits;
	short *totcolp;

	if (act > MAXMAT) return;
	if (act < 1) act = 1;
	
	/* prevent crashing when using accidentally */
	BLI_assert(ob->id.lib == NULL);
	if (ob->id.lib) return;
	
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
		matar = MEM_callocN(sizeof(void *) * act, "matarray2");
		matbits = MEM_callocN(sizeof(char) * act, "matbits1");
		if (ob->totcol) {
			memcpy(matar, ob->mat, sizeof(void *) * ob->totcol);
			memcpy(matbits, ob->matbits, sizeof(char) * (*totcolp));
			MEM_freeN(ob->mat);
			MEM_freeN(ob->matbits);
		}
		ob->mat = matar;
		ob->matbits = matbits;
		ob->totcol = act;

		/* copy object/mesh linking, or assign based on userpref */
		if (ob->actcol)
			ob->matbits[act - 1] = ob->matbits[ob->actcol - 1];
		else
			ob->matbits[act - 1] = (U.flag & USER_MAT_ON_OB) ? 1 : 0;
	}
	
	/* do it */

	if (ob->matbits[act - 1]) {   /* in object */
		mao = ob->mat[act - 1];
		if (mao) mao->id.us--;
		ob->mat[act - 1] = ma;
	}
	else {  /* in data */
		mao = (*matarar)[act - 1];
		if (mao) mao->id.us--;
		(*matarar)[act - 1] = ma;
	}

	if (ma)
		id_us_plus((ID *)ma);
	test_object_materials(ob->data);
}

/* XXX - this calls many more update calls per object then are needed, could be optimized */
void assign_matarar(struct Object *ob, struct Material ***matar, short totcol)
{
	int actcol_orig = ob->actcol;
	short i;

	while (object_remove_material_slot(ob)) {}

	/* now we have the right number of slots */
	for (i = 0; i < totcol; i++)
		assign_material(ob, (*matar)[i], i + 1);

	if (actcol_orig > ob->totcol)
		actcol_orig = ob->totcol;

	ob->actcol = actcol_orig;
}


short find_material_index(Object *ob, Material *ma)
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

int object_add_material_slot(Object *ob)
{
	if (ob == NULL) return FALSE;
	if (ob->totcol >= MAXMAT) return FALSE;
	
	assign_material(ob, NULL, ob->totcol + 1);
	ob->actcol = ob->totcol;
	return TRUE;
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
			else if (ma->texco & (TEXCO_LAVECTOR | TEXCO_VIEW | TEXCO_STICKY)) needuv = 1;

			if ((ma->mapto & MAP_NORM) && (mtex->normapspace == MTEX_NSPACE_TANGENT))
				needtang = 1;
		}
	}

	if (needtang) ma->mode |= MA_NORMAP_TANG;
	else ma->mode &= ~MA_NORMAP_TANG;
	
	if (ma->mode & (MA_VERTEXCOL | MA_VERTEXCOLP | MA_FACETEXTURE)) {
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
	/* will become or-ed result of all node modes */
	ma->mode_l = ma->mode;
	ma->mode_l &= ~MA_SHLESS;

	if (ma->strand_surfnor > 0.0f)
		ma->mode_l |= MA_STR_SURFDIFF;

	/* parses the geom+tex nodes */
	if (ma->nodetree && ma->use_nodes)
		ntreeShaderGetTexcoMode(ma->nodetree, r_mode, &ma->texco, &ma->mode_l);
}

static void init_render_nodetree(bNodeTree *ntree, Material *basemat, int r_mode, float *amb)
{
	bNode *node;
	
	for (node = ntree->nodes.first; node; node = node->next) {
		if (node->id) {
			if (GS(node->id->name) == ID_MA) {
				Material *ma = (Material *)node->id;
				if (ma != basemat) {
					do_init_render_material(ma, r_mode, amb);
					basemat->texco |= ma->texco;
					basemat->mode_l |= ma->mode_l & ~(MA_TRANSP | MA_ZTRANSP | MA_RAYTRANSP);
				}
			}
			else if (node->type == NODE_GROUP)
				init_render_nodetree((bNodeTree *)node->id, basemat, r_mode, amb);
		}
	}
}

void init_render_material(Material *mat, int r_mode, float *amb)
{
	
	do_init_render_material(mat, r_mode, amb);
	
	if (mat->nodetree && mat->use_nodes) {
		init_render_nodetree(mat->nodetree, mat, r_mode, amb);
		
		if (!mat->nodetree->execdata)
			mat->nodetree->execdata = ntreeShaderBeginExecTree(mat->nodetree, 1);
	}
}

void init_render_materials(Main *bmain, int r_mode, float *amb)
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
	
	do_init_render_material(&defmaterial, r_mode, amb);
}

/* only needed for nodes now */
void end_render_material(Material *mat)
{
	if (mat && mat->nodetree && mat->use_nodes) {
		if (mat->nodetree->execdata)
			ntreeShaderEndExecTree(mat->nodetree->execdata, 1);
	}
}

void end_render_materials(Main *bmain)
{
	Material *ma;
	for (ma = bmain->mat.first; ma; ma = ma->id.next)
		if (ma->id.us) 
			end_render_material(ma);
}

static int material_in_nodetree(bNodeTree *ntree, Material *mat)
{
	bNode *node;

	for (node = ntree->nodes.first; node; node = node->next) {
		if (node->id && GS(node->id->name) == ID_MA) {
			if (node->id == (ID *)mat)
				return 1;
		}
		else if (node->type == NODE_GROUP)
			if (material_in_nodetree((bNodeTree *)node->id, mat))
				return 1;
	}

	return 0;
}

int material_in_material(Material *parmat, Material *mat)
{
	if (parmat == mat)
		return 1;
	else if (parmat->nodetree && parmat->use_nodes)
		return material_in_nodetree(parmat->nodetree, mat);
	else
		return 0;
}


/* ****************** */

/* Update drivers for materials in a nodetree */
static void material_node_drivers_update(Scene *scene, bNodeTree *ntree, float ctime)
{
	bNode *node;
	
	/* nodetree itself */
	if (ntree->adt && ntree->adt->drivers.first) {
		BKE_animsys_evaluate_animdata(scene, &ntree->id, ntree->adt, ctime, ADT_RECALC_DRIVERS);
	}
	
	/* nodes... */
	for (node = ntree->nodes.first; node; node = node->next) {
		if (node->id && GS(node->id->name) == ID_MA) {
			/* TODO: prevent infinite recursion here... */
			material_drivers_update(scene, (Material *)node->id, ctime);
		}
		else if (node->type == NODE_GROUP && node->id) {
			material_node_drivers_update(scene, (bNodeTree *)node->id, ctime);
		}
	}
}

/* Calculate all drivers for materials 
 * FIXME: this is really a terrible method which may result in some things being calculated
 * multiple times. However, without proper despgraph support for these things, we are forced
 * into this sort of thing...
 */
void material_drivers_update(Scene *scene, Material *ma, float ctime)
{
	//if (G.f & G_DEBUG)
	//	printf("material_drivers_update(%s, %s)\n", scene->id.name, ma->id.name);
	
	/* material itself */
	if (ma->adt && ma->adt->drivers.first) {
		BKE_animsys_evaluate_animdata(scene, &ma->id, ma->adt, ctime, ADT_RECALC_DRIVERS);
	}
	
	/* nodes */
	if (ma->nodetree) {
		material_node_drivers_update(scene, ma->nodetree, ctime);
	}
}
	
/* ****************** */
#if 0 /* UNUSED */
static char colname_array[125][20]= {
"Black", "DarkRed", "HalfRed", "Red", "Red",
"DarkGreen", "DarkOlive", "Brown", "Chocolate", "OrangeRed",
"HalfGreen", "GreenOlive", "DryOlive", "Goldenrod", "DarkOrange",
"LightGreen", "Chartreuse", "YellowGreen", "Yellow", "Gold",
"Green", "LawnGreen", "GreenYellow", "LightOlive", "Yellow",
"DarkBlue", "DarkPurple", "HotPink", "VioletPink", "RedPink",
"SlateGray", "DarkGrey", "PalePurple", "IndianRed", "Tomato",
"SeaGreen", "PaleGreen", "GreenKhaki", "LightBrown", "LightSalmon",
"SpringGreen", "PaleGreen", "MediumOlive", "YellowBrown", "LightGold",
"LightGreen", "LightGreen", "LightGreen", "GreenYellow", "PaleYellow",
"HalfBlue", "DarkSky", "HalfMagenta", "VioletRed", "DeepPink",
"SteelBlue", "SkyBlue", "Orchid", "LightHotPink", "HotPink",
"SeaGreen", "SlateGray", "MediumGrey", "Burlywood", "LightPink",
"SpringGreen", "Aquamarine", "PaleGreen", "Khaki", "PaleOrange",
"SpringGreen", "SeaGreen", "PaleGreen", "PaleWhite", "YellowWhite",
"LightBlue", "Purple", "MediumOrchid", "Magenta", "Magenta",
"RoyalBlue", "SlateBlue", "MediumOrchid", "Orchid", "Magenta",
"DeepSkyBlue", "LightSteelBlue", "LightSkyBlue", "Violet", "LightPink",
"Cyan", "DarkTurquoise", "SkyBlue", "Grey", "Snow",
"Mint", "Mint", "Aquamarine", "MintCream", "Ivory",
"Blue", "Blue", "DarkMagenta", "DarkOrchid", "Magenta",
"SkyBlue", "RoyalBlue", "LightSlateBlue", "MediumOrchid", "Magenta",
"DodgerBlue", "SteelBlue", "MediumPurple", "PalePurple", "Plum",
"DeepSkyBlue", "PaleBlue", "LightSkyBlue", "PalePurple", "Thistle",
"Cyan", "ColdBlue", "PaleTurquoise", "GhostWhite", "White"
};

void automatname(Material *ma)
{
	int nr, r, g, b;
	float ref;
	
	if (ma == NULL) return;
	if (ma->mode & MA_SHLESS) ref = 1.0;
	else ref = ma->ref;

	r = (int)(4.99f * (ref * ma->r));
	g = (int)(4.99f * (ref * ma->g));
	b = (int)(4.99f * (ref * ma->b));
	nr = r + 5 * g + 25 * b;
	if (nr > 124) nr = 124;
	new_id(&G.main->mat, (ID *)ma, colname_array[nr]);
	
}
#endif

int object_remove_material_slot(Object *ob)
{
	Material *mao, ***matarar;
	Object *obt;
	short *totcolp;
	short a, actcol;
	
	if (ob == NULL || ob->totcol == 0) {
		return FALSE;
	}

	/* this should never happen and used to crash */
	if (ob->actcol <= 0) {
		printf("%s: invalid material index %d, report a bug!\n", __func__, ob->actcol);
		BLI_assert(0);
		return FALSE;
	}

	/* take a mesh/curve/mball as starting point, remove 1 index,
	 * AND with all objects that share the ob->data
	 * 
	 * after that check indices in mesh/curve/mball!!!
	 */
	
	totcolp = give_totcolp(ob);
	matarar = give_matarar(ob);

	if (*matarar == NULL) return FALSE;

	/* we delete the actcol */
	mao = (*matarar)[ob->actcol - 1];
	if (mao) mao->id.us--;
	
	for (a = ob->actcol; a < ob->totcol; a++)
		(*matarar)[a - 1] = (*matarar)[a];
	(*totcolp)--;
	
	if (*totcolp == 0) {
		MEM_freeN(*matarar);
		*matarar = NULL;
	}
	
	actcol = ob->actcol;
	obt = G.main->object.first;
	while (obt) {
	
		if (obt->data == ob->data) {
			
			/* WATCH IT: do not use actcol from ob or from obt (can become zero) */
			mao = obt->mat[actcol - 1];
			if (mao) mao->id.us--;
		
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
		obt = obt->id.next;
	}

	/* check indices from mesh */
	if (ELEM4(ob->type, OB_MESH, OB_CURVE, OB_SURF, OB_FONT)) {
		data_delete_material_index_id((ID *)ob->data, actcol - 1);
		BKE_displist_free(&ob->disp);
	}

	return TRUE;
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
			r_col[0] = facm * (r_col[0]) + fac *fabsf(r_col[0] - col[0]);
			r_col[1] = facm * (r_col[1]) + fac *fabsf(r_col[1] - col[1]);
			r_col[2] = facm * (r_col[2]) + fac *fabsf(r_col[2] - col[2]);
			break;
		case MA_RAMP_DARK:
			tmp = col[0] + ((1 - col[0]) * facm);
			if (tmp < r_col[0]) r_col[0] = tmp;
			tmp = col[1] + ((1 - col[1]) * facm);
			if (tmp < r_col[1]) r_col[1] = tmp;
			tmp = col[2] + ((1 - col[2]) * facm);
			if (tmp < r_col[2]) r_col[2] = tmp;
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
		}
		break;
		case MA_RAMP_SAT:
		{
			float rH, rS, rV;
			float colH, colS, colV;
			rgb_to_hsv(r_col[0], r_col[1], r_col[2], &rH, &rS, &rV);
			if (rS != 0) {
				rgb_to_hsv(col[0], col[1], col[2], &colH, &colS, &colV);
				hsv_to_rgb(rH, (facm * rS + fac * colS), rV, r_col + 0, r_col + 1, r_col + 2);
			}
		}
		break;
		case MA_RAMP_VAL:
		{
			float rH, rS, rV;
			float colH, colS, colV;
			rgb_to_hsv(r_col[0], r_col[1], r_col[2], &rH, &rS, &rV);
			rgb_to_hsv(col[0], col[1], col[2], &colH, &colS, &colV);
			hsv_to_rgb(rH, rS, (facm * rV + fac * colV), r_col + 0, r_col + 1, r_col + 2);
		}
		break;
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
		}
		break;
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
		}
		break;
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

/* copy/paste buffer, if we had a propper py api that would be better */
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
	matcopybuf.nodetree = ntreeCopyTree(ma->nodetree);
	matcopybuf.preview = NULL;
	matcopybuf.gpumaterial.first = matcopybuf.gpumaterial.last = NULL;
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
		if (mtex && mtex->tex) mtex->tex->id.us--;
		if (mtex) MEM_freeN(mtex);
	}

	if (ma->nodetree) {
		ntreeFreeTree(ma->nodetree);
		MEM_freeN(ma->nodetree);
	}

	GPU_material_free(ma);

	id = (ma->id);
	memcpy(ma, &matcopybuf, sizeof(Material));
	(ma->id) = id;

	if (matcopybuf.ramp_col) ma->ramp_col = MEM_dupallocN(matcopybuf.ramp_col);
	if (matcopybuf.ramp_spec) ma->ramp_spec = MEM_dupallocN(matcopybuf.ramp_spec);

	for (a = 0; a < MAX_MTEX; a++) {
		mtex = ma->mtex[a];
		if (mtex) {
			ma->mtex[a] = MEM_dupallocN(mtex);
			if (mtex->tex) id_us_plus((ID *)mtex->tex);
		}
	}

	ma->nodetree = ntreeCopyTree(matcopybuf.nodetree);
}


/*********************** texface to material convert functions **********************/
/* encode all the TF information into a single int */
static int encode_tfaceflag(MTFace *tf, int convertall)
{
	/* calculate the flag */
	int flag = tf->mode;

	/* options that change the material offline render */	
	if (!convertall) {
		flag &= ~TF_OBCOL;
	}

	/* clean flags that are not being converted */
	flag &= ~TF_TEX;
	flag &= ~TF_SHAREDVERT;
	flag &= ~TF_SHAREDCOL;
	flag &= ~TF_CONVERTED;

	/* light tface flag is ignored in GLSL mode */
	flag &= ~TF_LIGHT;
	
	/* 15 is how big the flag can be - hardcoded here and in decode_tfaceflag() */
	flag |= tf->transp << 15;
	
	/* increase 1 so flag 0 is different than no flag yet */
	return flag + 1;
}

/* set the material options based in the tface flag */
static void decode_tfaceflag(Material *ma, int flag, int convertall)
{
	int alphablend;	
	GameSettings *game = &ma->game;

	/* flag is shifted in 1 to make 0 != no flag yet (see encode_tfaceflag) */
	flag -= 1;

	alphablend = flag >> 15; //encoded in the encode_tfaceflag function
	(*game).flag = 0;
	
	/* General Material Options */
	if ((flag & TF_DYNAMIC) == 0) (*game).flag    |= GEMAT_NOPHYSICS;
	
	/* Material Offline Rendering Properties */
	if (convertall) {
		if (flag & TF_OBCOL) ma->shade_flag |= MA_OBCOLOR;
	}
	
	/* Special Face Properties */
	if ((flag & TF_TWOSIDE) == 0) (*game).flag |= GEMAT_BACKCULL;
	if (flag & TF_INVISIBLE) (*game).flag |= GEMAT_INVISIBLE;
	if (flag & TF_BMFONT) (*game).flag |= GEMAT_TEXT;
	
	/* Face Orientation */
	if (flag & TF_BILLBOARD) (*game).face_orientation |= GEMAT_HALO;
	else if (flag & TF_BILLBOARD2) (*game).face_orientation |= GEMAT_BILLBOARD;
	else if (flag & TF_SHADOW) (*game).face_orientation |= GEMAT_SHADOW;
	
	/* Alpha Blend */
	if (flag & TF_ALPHASORT && ELEM(alphablend, TF_ALPHA, TF_ADD)) (*game).alpha_blend = GEMAT_ALPHA_SORT;
	else if (alphablend & TF_ALPHA) (*game).alpha_blend = GEMAT_ALPHA;
	else if (alphablend & TF_ADD) (*game).alpha_blend = GEMAT_ADD;
	else if (alphablend & TF_CLIP) (*game).alpha_blend = GEMAT_CLIP;
}

/* boolean check to see if the mesh needs a material */
static int check_tfaceneedmaterial(int flag)
{
	// check if the flags we have are not deprecated != than default material options
	// also if only flags are visible and collision see if all objects using this mesh have this option in physics

	/* flag is shifted in 1 to make 0 != no flag yet (see encode_tfaceflag) */
	flag -= 1;

	// deprecated flags
	flag &= ~TF_OBCOL;
	flag &= ~TF_SHAREDVERT;
	flag &= ~TF_SHAREDCOL;

	/* light tface flag is ignored in GLSL mode */
	flag &= ~TF_LIGHT;
	
	// automatic detected if tex image has alpha
	flag &= ~(TF_ALPHA << 15);
	// automatic detected if using texture
	flag &= ~TF_TEX;

	// settings for the default NoMaterial
	if (flag == TF_DYNAMIC)
		return 0;

	else
		return 1;
}

/* return number of digits of an integer */
// XXX to be optmized or replaced by an equivalent blender internal function
static int integer_getdigits(int number)
{
	int i = 0;
	if (number == 0) return 1;

	while (number != 0) {
		number = (int)(number / 10);
		i++;
	}
	return i;
}

static void calculate_tface_materialname(char *matname, char *newname, int flag)
{
	// if flag has only light and collision and material matches those values
	// you can do strcpy(name, mat_name);
	// otherwise do:
	int digits = integer_getdigits(flag);
	/* clamp the old name, remove the MA prefix and add the .TF.flag suffix
	 * e.g. matname = "MALoooooooooooooongName"; newname = "Loooooooooooooon.TF.2" */
	BLI_snprintf(newname, MAX_ID_NAME, "%.*s.TF.%0*d", MAX_ID_NAME - (digits + 5), matname, digits, flag);
}

/* returns -1 if no match */
static short mesh_getmaterialnumber(Mesh *me, Material *ma)
{
	short a;

	for (a = 0; a < me->totcol; a++) {
		if (me->mat[a] == ma) {
			return a;
		}
	}

	return -1;
}

/* append material */
static short mesh_addmaterial(Mesh *me, Material *ma)
{
	material_append_id(&me->id, NULL);
	me->mat[me->totcol - 1] = ma;

	id_us_plus(&ma->id);

	return me->totcol - 1;
}

static void set_facetexture_flags(Material *ma, Image *image)
{
	if (image) {
		ma->mode |= MA_FACETEXTURE;
		/* we could check if the texture has alpha, but then more meshes sharing the same
		 * material may need it. Let's make it simple. */
		if (BKE_image_has_alpha(image))
			ma->mode |= MA_FACETEXTURE_ALPHA;
	}
}

/* returns material number */
static short convert_tfacenomaterial(Main *main, Mesh *me, MTFace *tf, int flag)
{
	Material *ma;
	char idname[MAX_ID_NAME];
	short mat_nr = -1;
	
	/* new material, the name uses the flag*/
	BLI_snprintf(idname, sizeof(idname), "MAMaterial.TF.%0*d", integer_getdigits(flag), flag);

	if ((ma = BLI_findstring(&main->mat, idname + 2, offsetof(ID, name) + 2))) {
		mat_nr = mesh_getmaterialnumber(me, ma);
		/* assign the material to the mesh */
		if (mat_nr == -1) mat_nr = mesh_addmaterial(me, ma);

		/* if needed set "Face Textures [Alpha]" Material options */
		set_facetexture_flags(ma, tf->tpage);
	}
	/* create a new material */
	else {
		ma = BKE_material_add(idname + 2);

		if (ma) {
			printf("TexFace Convert: Material \"%s\" created.\n", idname + 2);
			mat_nr = mesh_addmaterial(me, ma);
			
			/* if needed set "Face Textures [Alpha]" Material options */
			set_facetexture_flags(ma, tf->tpage);

			decode_tfaceflag(ma, flag, 1);
			// the final decoding will happen after, outside the main loop
			// for now store the flag into the material and change light/tex/collision 
			// store the flag as a negative number
			ma->game.flag = -flag;
			id_us_min((ID *)ma);	
		}
		else printf("Error: Unable to create Material \"%s\" for Mesh \"%s\".", idname + 2, me->id.name + 2);
	}

	/* set as converted, no need to go bad to this face */
	tf->mode |= TF_CONVERTED;	
	return mat_nr;
}

/* Function to fully convert materials */
static void convert_tfacematerial(Main *main, Material *ma)
{
	Mesh *me;
	Material *mat_new;
	MFace *mf;
	MTFace *tf;
	int flag, index;
	int a;
	short mat_nr;
	CustomDataLayer *cdl;
	char idname[MAX_ID_NAME];

	for (me = main->mesh.first; me; me = me->id.next) {
		/* check if this mesh uses this material */
		for (a = 0; a < me->totcol; a++)
			if (me->mat[a] == ma) break;
			
		/* no material found */
		if (a == me->totcol) continue;

		/* get the active tface layer */
		index = CustomData_get_active_layer_index(&me->fdata, CD_MTFACE);
		cdl = (index == -1) ? NULL : &me->fdata.layers[index];
		if (!cdl) continue;

		/* loop over all the faces and stop at the ones that use the material*/
		for (a = 0, mf = me->mface; a < me->totface; a++, mf++) {
			if (me->mat[mf->mat_nr] != ma) continue;

			/* texface data for this face */
			tf = ((MTFace *)cdl->data) + a;
			flag = encode_tfaceflag(tf, 1);

			/* the name of the new material */
			calculate_tface_materialname(ma->id.name, (char *)&idname, flag);

			if ((mat_new = BLI_findstring(&main->mat, idname + 2, offsetof(ID, name) + 2))) {
				/* material already existent, see if the mesh has it */
				mat_nr = mesh_getmaterialnumber(me, mat_new);
				/* material is not in the mesh, add it */
				if (mat_nr == -1) mat_nr = mesh_addmaterial(me, mat_new);
			}
			/* create a new material */
			else {
				mat_new = BKE_material_copy(ma);
				if (mat_new) {
					/* rename the material*/
					strcpy(mat_new->id.name, idname);
					id_us_min((ID *)mat_new);	

					mat_nr = mesh_addmaterial(me, mat_new);
					decode_tfaceflag(mat_new, flag, 1);
				}
				else {
					printf("Error: Unable to create Material \"%s\" for Mesh \"%s.", idname + 2, me->id.name + 2);
					mat_nr = mf->mat_nr;
					continue;
				}
			}
			
			/* if the material has a texture but no texture channel
			 * set "Face Textures [Alpha]" Material options 
			 * actually we need to run it always, because of old behavior
			 * of using face texture if any texture channel was present (multitex) */
			//if ((!mat_new->mtex[0]) && (!mat_new->mtex[0]->tex))
			set_facetexture_flags(mat_new, tf->tpage);

			/* set the material number to the face*/
			mf->mat_nr = mat_nr;
		}
		/* remove material from mesh */
		for (a = 0; a < me->totcol; )
			if (me->mat[a] == ma) material_pop_id(&me->id, a, 1); else a++;
	}
}


#define MAT_BGE_DISPUTED -99999

int do_version_tface(Main *main, int fileload)
{
	Mesh *me;
	Material *ma;
	MFace *mf;
	MTFace *tf;
	CustomDataLayer *cdl;
	int a;
	int flag;
	int index;

	/* sometimes mesh has no materials but will need a new one. In those
	 * cases we need to ignore the mf->mat_nr and only look at the face
	 * mode because it can be zero as uninitialized or the 1st created material
	 */
	int nomaterialslots;

	/* alert to user to check the console */
	int nowarning = 1;

	/* mark all the materials to conversion with a flag
	 * if there is tface create a complete flag for that storing in flag
	 * if there is tface and flag > 0: creates a new flag based on this face
	 * if flags are different set flag to -1  
	 */
	
	/* 1st part: marking mesh materials to update */
	for (me = main->mesh.first; me; me = me->id.next) {
		if (me->id.lib) continue;

		/* get the active tface layer */
		index = CustomData_get_active_layer_index(&me->fdata, CD_MTFACE);
		cdl = (index == -1) ? NULL : &me->fdata.layers[index];
		if (!cdl) continue;

		nomaterialslots = (me->totcol == 0 ? 1 : 0);
		
		/* loop over all the faces*/
		for (a = 0, mf = me->mface; a < me->totface; a++, mf++) {
			/* texface data for this face */
			tf = ((MTFace *)cdl->data) + a;

			/* conversion should happen only once */
			if (fileload)
				tf->mode &= ~TF_CONVERTED;
			else {
				if ((tf->mode & TF_CONVERTED)) continue;
				else tf->mode |= TF_CONVERTED;
			}
			
			/* no material slots */
			if (nomaterialslots) {
				flag = encode_tfaceflag(tf, 1);
				
				/* create/find a new material and assign to the face */
				if (check_tfaceneedmaterial(flag)) {
					mf->mat_nr = convert_tfacenomaterial(main, me, tf, flag);
				}
				/* else mark them as no-material to be reverted to 0 later */
				else {
					mf->mat_nr = -1;
				}
			}
			else if (mf->mat_nr < me->totcol) {
				ma = me->mat[mf->mat_nr];
				
				/* no material create one if necessary */
				if (!ma) {
					/* find a new material and assign to the face */
					flag = encode_tfaceflag(tf, 1);

					/* create/find a new material and assign to the face */
					if (check_tfaceneedmaterial(flag))
						mf->mat_nr = convert_tfacenomaterial(main, me, tf, flag);

					continue;
				}

				/* we can't read from this if it comes from a library,
				 * at doversion time: direct_link might not have happened on it,
				 * so ma->mtex is not pointing to valid memory yet.
				 * later we could, but it's better not */
				else if (ma->id.lib)
					continue;
				
				/* material already marked as disputed */
				else if (ma->game.flag == MAT_BGE_DISPUTED)
					continue;

				/* found a material */
				else {
					flag = encode_tfaceflag(tf, ((fileload) ? 0 : 1));

					/* first time changing this material */
					if (ma->game.flag == 0)
						ma->game.flag = -flag;
			
					/* mark material as disputed */
					else if (ma->game.flag != -flag) {
						ma->game.flag = MAT_BGE_DISPUTED;
						continue;
					}
			
					/* material ok so far */
					else {
						ma->game.flag = -flag;
						
						/* some people uses multitexture with TexFace by creating a texture
						 * channel which not necessarily the tf->tpage image. But the game engine
						 * was enabling it. Now it's required to set "Face Texture [Alpha] in the
						 * material settings. */
						if (!fileload)
							set_facetexture_flags(ma, tf->tpage);
					}
				}
			}
			else {
				continue;
			}
		}

		/* if we didn't have material slot and now we do, we need to
		 * make sure the materials are correct */
		if (nomaterialslots) {
			if (me->totcol > 0) {
				for (a = 0, mf = me->mface; a < me->totface; a++, mf++) {
					if (mf->mat_nr == -1) {
						/* texface data for this face */
						tf = ((MTFace *)cdl->data) + a;
						mf->mat_nr = convert_tfacenomaterial(main, me, tf, encode_tfaceflag(tf, 1));
					}
				}
			}
			else {
				for (a = 0, mf = me->mface; a < me->totface; a++, mf++) {
					mf->mat_nr = 0;
				}
			}
		}

	}
	
	/* 2nd part - conversion */
	/* skip library files */

	/* we shouldn't loop through the materials created in the loop. make the loop stop at its original length) */
	for (ma = main->mat.first, a = 0; ma; ma = ma->id.next, a++) {
		if (ma->id.lib) continue;

		/* disputed material */
		if (ma->game.flag == MAT_BGE_DISPUTED) {
			ma->game.flag = 0;
			if (fileload) {
				printf("Warning: material \"%s\" skipped - to convert old game texface to material go to the Help menu.\n", ma->id.name + 2);
				nowarning = 0;
			}
			else
				convert_tfacematerial(main, ma);
			continue;	
		}
	
		/* no conflicts in this material - 90% of cases
		 * convert from tface system to material */
		else if (ma->game.flag < 0) {
			decode_tfaceflag(ma, -(ma->game.flag), 1);

			/* material is good make sure all faces using
			 * this material are set to converted */
			if (fileload) {
				for (me = main->mesh.first; me; me = me->id.next) {
					/* check if this mesh uses this material */
					for (a = 0; a < me->totcol; a++)
						if (me->mat[a] == ma) break;
						
					/* no material found */
					if (a == me->totcol) continue;
			
					/* get the active tface layer */
					index = CustomData_get_active_layer_index(&me->fdata, CD_MTFACE);
					cdl = (index == -1) ? NULL : &me->fdata.layers[index];
					if (!cdl) continue;
			
					/* loop over all the faces and stop at the ones that use the material*/
					for (a = 0, mf = me->mface; a < me->totface; a++, mf++) {
						if (me->mat[mf->mat_nr] == ma) {
							/* texface data for this face */
							tf = ((MTFace *)cdl->data) + a;
							tf->mode |= TF_CONVERTED;
						}
					}
				}
			}
		}
		/* material is not used by faces with texface
		 * set the default flag - do it only once */
		else {
			if (fileload) {
				ma->game.flag = GEMAT_BACKCULL;
			}
		}
	}

	return nowarning;
}

