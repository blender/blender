/*
 * $Id$
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
 * The Original Code is Copyright (C) 2007 by Janne Karhu.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_scene_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_force.h"
#include "DNA_object_types.h"
#include "DNA_vec_types.h"
#include "DNA_userdef_types.h"
#include "DNA_view3d_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_windowmanager_types.h"

#include "BKE_DerivedMesh.h"
#include "BKE_depsgraph.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_object.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_particle.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_utildefines.h"
#include "BKE_pointcache.h"

#include "BLI_arithb.h"
#include "BLI_blenlib.h"
#include "BLI_dynstr.h"
#include "BLI_kdtree.h"
#include "BLI_rand.h"

#include "PIL_time.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "ED_mesh.h"
#include "ED_particle.h"
#include "ED_view3d.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "physics_intern.h"

static void PE_create_particle_edit(Scene *scene, Object *ob, PointCache *cache, ParticleSystem *psys);
static void PTCacheUndo_clear(PTCacheEdit *edit);

#define KEY_K					PTCacheEditKey *key; int k
#define POINT_P					PTCacheEditPoint *point; int p
#define LOOP_POINTS				for(p=0, point=edit->points; p<edit->totpoint; p++, point++)
#define LOOP_VISIBLE_POINTS		for(p=0, point=edit->points; p<edit->totpoint; p++, point++) if(!(point->flag & PEP_HIDE))
#define LOOP_SELECTED_POINTS	for(p=0, point=edit->points; p<edit->totpoint; p++, point++) if(point_is_selected(point))
#define LOOP_UNSELECTED_POINTS	for(p=0, point=edit->points; p<edit->totpoint; p++, point++) if(!point_is_selected(point))
#define LOOP_EDITED_POINTS		for(p=0, point=edit->points; p<edit->totpoint; p++, point++) if(point->flag & PEP_EDIT_RECALC)
#define LOOP_TAGGED_POINTS		for(p=0, point=edit->points; p<edit->totpoint; p++, point++) if(point->flag & PEP_TAG)
#define LOOP_KEYS				for(k=0, key=point->keys; k<point->totkey; k++, key++)
#define LOOP_VISIBLE_KEYS		for(k=0, key=point->keys; k<point->totkey; k++, key++) if(!(key->flag & PEK_HIDE))
#define LOOP_SELECTED_KEYS		for(k=0, key=point->keys; k<point->totkey; k++, key++) if((key->flag & PEK_SELECT) && !(key->flag & PEK_HIDE))
#define LOOP_TAGGED_KEYS		for(k=0, key=point->keys; k<point->totkey; k++, key++) if(key->flag & PEK_TAG)

#define KEY_WCO					(key->flag & PEK_USE_WCO ? key->world_co : key->co)

/**************************** utilities *******************************/

int PE_poll(bContext *C)
{
	Scene *scene= CTX_data_scene(C);
	Object *ob= CTX_data_active_object(C);

	if(!scene || !ob || !(ob->mode & OB_MODE_PARTICLE_EDIT))
		return 0;
	
	return (PE_get_current(scene, ob) != NULL);
}

int PE_hair_poll(bContext *C)
{
	Scene *scene= CTX_data_scene(C);
	Object *ob= CTX_data_active_object(C);
	PTCacheEdit *edit;

	if(!scene || !ob || !(ob->mode & OB_MODE_PARTICLE_EDIT))
		return 0;
	
	edit= PE_get_current(scene, ob);

	return (edit && edit->psys);
}

int PE_poll_3dview(bContext *C)
{
	return PE_poll(C) && CTX_wm_area(C)->spacetype == SPACE_VIEW3D &&
		CTX_wm_region(C)->regiontype == RGN_TYPE_WINDOW;
}

void PE_free_ptcache_edit(PTCacheEdit *edit)
{
	POINT_P;

	if(edit==0) return;

	PTCacheUndo_clear(edit);

	if(edit->points) {
		LOOP_POINTS {
			if(point->keys) 
				MEM_freeN(point->keys);
		}

		MEM_freeN(edit->points);
	}

	if(edit->mirror_cache)
		MEM_freeN(edit->mirror_cache);

	if(edit->emitter_cosnos) {
		MEM_freeN(edit->emitter_cosnos);
		edit->emitter_cosnos= 0;
	}

	if(edit->emitter_field) {
		BLI_kdtree_free(edit->emitter_field);
		edit->emitter_field= 0;
	}

	psys_free_path_cache(NULL, edit);

	MEM_freeN(edit);
}

/************************************************/
/*			Edit Mode Helpers					*/
/************************************************/

int PE_start_edit(PTCacheEdit *edit)
{
	if(edit) {
		edit->edited = 1;
		if(edit->psys)
			edit->psys->flag |= PSYS_EDITED;
		return 1;
	}

	return 0;
}

ParticleEditSettings *PE_settings(Scene *scene)
{
	return &scene->toolsettings->particle;
}

/* always gets atleast the first particlesystem even if PSYS_CURRENT flag is not set */
static PTCacheEdit *pe_get_current(Scene *scene, Object *ob, int create)
{
	ParticleEditSettings *pset= PE_settings(scene);
	PTCacheEdit *edit = NULL;
	ListBase pidlist;
	PTCacheID *pid;

	pset->scene = scene;
	pset->object = ob;

	if(ob==NULL)
		return NULL;

	BKE_ptcache_ids_from_object(&pidlist, ob);

	/* in the case of only one editable thing, set pset->edittype accordingly */
	if(pidlist.first && pidlist.first == pidlist.last) {
		pid = pidlist.first;
		switch(pid->type) {
			case PTCACHE_TYPE_PARTICLES:
				pset->edittype = PE_TYPE_PARTICLES;
				break;
			case PTCACHE_TYPE_SOFTBODY:
				pset->edittype = PE_TYPE_SOFTBODY;
				break;
			case PTCACHE_TYPE_CLOTH:
				pset->edittype = PE_TYPE_CLOTH;
				break;
		}
	}

	for(pid=pidlist.first; pid; pid=pid->next) {
		if(pset->edittype == PE_TYPE_PARTICLES && pid->type == PTCACHE_TYPE_PARTICLES) {
			ParticleSystem *psys = pid->calldata;

			if(psys->flag & PSYS_CURRENT) {
				if(psys->part && psys->part->type == PART_HAIR) {
					if(psys->flag & PSYS_HAIR_DYNAMICS && psys->pointcache->flag & PTCACHE_BAKED) {
						if(create && !psys->pointcache->edit)
							PE_create_particle_edit(scene, ob, pid->cache, NULL);
						edit = pid->cache->edit;
					}
					else {
						if(create && !psys->edit && psys->flag & PSYS_HAIR_DONE)
							PE_create_particle_edit(scene, ob, NULL, psys);
						edit = psys->edit;
					}
				}
				else {
					if(create && pid->cache->flag & PTCACHE_BAKED && !pid->cache->edit)
						PE_create_particle_edit(scene, ob, pid->cache, psys);
					edit = pid->cache->edit;
				}

				break;
			}
		}
		else if(pset->edittype == PE_TYPE_SOFTBODY && pid->type == PTCACHE_TYPE_SOFTBODY) {
			if(create && pid->cache->flag & PTCACHE_BAKED && !pid->cache->edit)
				PE_create_particle_edit(scene, ob, pid->cache, NULL);
			edit = pid->cache->edit;
			break;
		}
		else if(pset->edittype == PE_TYPE_CLOTH && pid->type == PTCACHE_TYPE_CLOTH) {
			if(create && pid->cache->flag & PTCACHE_BAKED && !pid->cache->edit)
				PE_create_particle_edit(scene, ob, pid->cache, NULL);
			edit = pid->cache->edit;
			break;
		}
	}

	if(edit)
		edit->pid = *pid;

	BLI_freelistN(&pidlist);

	return edit;
}

PTCacheEdit *PE_get_current(Scene *scene, Object *ob)
{
	return pe_get_current(scene, ob, 0);
}

PTCacheEdit *PE_create_current(Scene *scene, Object *ob)
{
	return pe_get_current(scene, ob, 1);
}

void PE_current_changed(Scene *scene, Object *ob)
{
	if(ob->mode == OB_MODE_PARTICLE_EDIT)
		PE_create_current(scene, ob);
}

void PE_hide_keys_time(Scene *scene, PTCacheEdit *edit, float cfra)
{
	ParticleEditSettings *pset=PE_settings(scene);
	POINT_P; KEY_K;


	if(pset->flag & PE_FADE_TIME && pset->selectmode==SCE_SELECT_POINT) {
		LOOP_POINTS {
			LOOP_KEYS {
				if(fabs(cfra-*key->time) < pset->fade_frames)
					key->flag &= ~PEK_HIDE;
				else {
					key->flag |= PEK_HIDE;
					//key->flag &= ~PEK_SELECT;
				}
			}
		}
	}
	else {
		LOOP_POINTS {
			LOOP_KEYS {
				key->flag &= ~PEK_HIDE;
			}
		}
	}
}

/****************** common struct passed to callbacks ******************/

typedef struct PEData {
	ViewContext vc;
	bglMats mats;
	
	Scene *scene;
	Object *ob;
	DerivedMesh *dm;
	PTCacheEdit *edit;

	short *mval;
	rcti *rect;
	float rad;
	float dist;
	float dval;
	int select;

	float *dvec;
	float combfac;
	float pufffac;
	float cutfac;
	float smoothfac;
	float weightfac;
	float growfac;
	int totrekey;

	int invert;
	int tot;
	float vec[3];
} PEData;

static void PE_set_data(bContext *C, PEData *data)
{
	memset(data, 0, sizeof(*data));

	data->scene= CTX_data_scene(C);
	data->ob= CTX_data_active_object(C);
	data->edit= PE_get_current(data->scene, data->ob);
}

static void PE_set_view3d_data(bContext *C, PEData *data)
{
	PE_set_data(C, data);

	view3d_set_viewcontext(C, &data->vc);
	view3d_get_transformation(&data->vc, data->ob, &data->mats);

	if((data->vc.v3d->drawtype>OB_WIRE) && (data->vc.v3d->flag & V3D_ZBUF_SELECT))
		view3d_validate_backbuf(&data->vc);
}

/*************************** selection utilities *******************************/

static int key_test_depth(PEData *data, float co[3])
{
	View3D *v3d= data->vc.v3d;
	RegionView3D *rv3d= data->vc.rv3d;
	double ux, uy, uz;
	float depth;
	short wco[3], x,y;

	/* nothing to do */
	if((v3d->drawtype<=OB_WIRE) || (v3d->flag & V3D_ZBUF_SELECT)==0)
		return 1;

	project_short(data->vc.ar, co, wco);
	
	if(wco[0] == IS_CLIPPED)
		return 0;

	gluProject(co[0],co[1],co[2], data->mats.modelview, data->mats.projection,
		(GLint *)data->mats.viewport, &ux, &uy, &uz);

	x=wco[0];
	y=wco[1];

	// XXX verify ..

	if(rv3d->depths && x<rv3d->depths->w && y<rv3d->depths->h) {
		/* the 0.0001 is an experimental threshold to make selecting keys right next to a surface work better */
		if((float)uz - 0.0001 > rv3d->depths->depths[y*rv3d->depths->w+x])
			return 0;
		else
			return 1;
	}
	else {
		x+= (short)data->vc.ar->winrct.xmin;
		y+= (short)data->vc.ar->winrct.ymin;

		glReadPixels(x, y, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth);

		if((float)uz - 0.0001 > depth)
			return 0;
		else
			return 1;
	}
}

static int key_inside_circle(PEData *data, float rad, float co[3], float *distance)
{
	float dx, dy, dist;
	short sco[2];

	project_short(data->vc.ar, co, sco);
	
	if(sco[0] == IS_CLIPPED)
		return 0;
	
	dx= data->mval[0] - sco[0];
	dy= data->mval[1] - sco[1];
	dist= sqrt(dx*dx + dy*dy);

	if(dist > rad)
		return 0;

	if(key_test_depth(data, co)) {
		if(distance)
			*distance=dist;

		return 1;
	}
	
	return 0;
}

static int key_inside_rect(PEData *data, float co[3])
{
	short sco[2];

	project_short(data->vc.ar, co,sco);

	if(sco[0] == IS_CLIPPED)
		return 0;
	
	if(sco[0] > data->rect->xmin && sco[0] < data->rect->xmax &&
	   sco[1] > data->rect->ymin && sco[1] < data->rect->ymax)
		return key_test_depth(data, co);

	return 0;
}

static int key_inside_test(PEData *data, float co[3])
{
	if(data->mval)
		return key_inside_circle(data, data->rad, co, NULL);
	else
		return key_inside_rect(data, co);
}

static int point_is_selected(PTCacheEditPoint *point)
{
	KEY_K;
	int sel;

	if(point->flag & PEP_HIDE)
		return 0;

	sel= 0;

	LOOP_SELECTED_KEYS {
		return 1;
	}
	
	return 0;
}

/*************************** iterators *******************************/

typedef void (*ForPointFunc)(PEData *data, int point_index);
typedef void (*ForKeyFunc)(PEData *data, int point_index, int key_index);
typedef void (*ForKeyMatFunc)(PEData *data, float mat[][4], float imat[][4], int point_index, int key_index, PTCacheEditKey *key);

static void for_mouse_hit_keys(PEData *data, ForKeyFunc func, int nearest)
{
	ParticleEditSettings *pset= PE_settings(data->scene);
	PTCacheEdit *edit= data->edit;
	POINT_P; KEY_K;
	int nearest_point, nearest_key;
	float dist= data->rad;

	/* in path select mode we have no keys */
	if(pset->selectmode==SCE_SELECT_PATH)
		return;

	nearest_point= -1;
	nearest_key= -1;

	LOOP_VISIBLE_POINTS {
		if(pset->selectmode == SCE_SELECT_END) {
			/* only do end keys */
			key= point->keys + point->totkey-1;

			if(nearest) {
				if(key_inside_circle(data, dist, KEY_WCO, &dist)) {
					nearest_point= p;
					nearest_key= point->totkey-1;
				}
			}
			else if(key_inside_test(data, KEY_WCO))
				func(data, p, point->totkey-1);
		}
		else {
			/* do all keys */
			LOOP_VISIBLE_KEYS {
				if(nearest) {
					if(key_inside_circle(data, dist, KEY_WCO, &dist)) {
						nearest_point= p;
						nearest_key= k;
					}
				}
				else if(key_inside_test(data, KEY_WCO))
					func(data, p, k);
			}
		}
	}

	/* do nearest only */
	if(nearest && nearest_point > -1)
		func(data, nearest_point, nearest_key);
}

static void foreach_mouse_hit_point(PEData *data, ForPointFunc func, int selected)
{
	ParticleEditSettings *pset= PE_settings(data->scene);
	PTCacheEdit *edit= data->edit;
	POINT_P; KEY_K;

	/* all is selected in path mode */
	if(pset->selectmode==SCE_SELECT_PATH)
		selected=0;

	LOOP_VISIBLE_POINTS {
		if(pset->selectmode==SCE_SELECT_END) {
			/* only do end keys */
			key= point->keys + point->totkey - 1;

			if(selected==0 || key->flag & PEK_SELECT)
				if(key_inside_circle(data, data->rad, KEY_WCO, &data->dist))
					func(data, p);
		}
		else {
			/* do all keys */
			LOOP_VISIBLE_KEYS {
				if(selected==0 || key->flag & PEK_SELECT) {
					if(key_inside_circle(data, data->rad, KEY_WCO, &data->dist)) {
						func(data, p);
						break;
					}
				}
			}
		}
	}
}

static void foreach_mouse_hit_key(PEData *data, ForKeyMatFunc func, int selected)
{
	PTCacheEdit *edit = data->edit;
	ParticleSystem *psys = edit->psys;
	ParticleSystemModifierData *psmd = NULL;
	ParticleEditSettings *pset= PE_settings(data->scene);
	POINT_P; KEY_K;
	float mat[4][4], imat[4][4];

	if(edit->psys)
		psmd= psys_get_modifier(data->ob, edit->psys);

	/* all is selected in path mode */
	if(pset->selectmode==SCE_SELECT_PATH)
		selected= 0;

	Mat4One(imat);
	Mat4One(mat);

	LOOP_VISIBLE_POINTS {
		if(edit->psys && !(edit->psys->flag & PSYS_GLOBAL_HAIR)) {
			psys_mat_hair_to_global(data->ob, psmd->dm, psys->part->from, psys->particles + p, mat);
			Mat4Invert(imat,mat);
		}

		if(pset->selectmode==SCE_SELECT_END) {
			/* only do end keys */
			key= point->keys + point->totkey-1;

			if(selected==0 || key->flag & PEK_SELECT)
				if(key_inside_circle(data, data->rad, KEY_WCO, &data->dist))
					func(data, mat, imat, p, point->totkey-1, key);
		}
		else {
			/* do all keys */
			LOOP_VISIBLE_KEYS {
				if(selected==0 || key->flag & PEK_SELECT)
					if(key_inside_circle(data, data->rad, KEY_WCO, &data->dist))
						func(data, mat, imat, p, k, key);
			}
		}
	}
}

static void foreach_selected_point(PEData *data, ForPointFunc func)
{
	PTCacheEdit *edit = data->edit;
	POINT_P;

	LOOP_SELECTED_POINTS {
		func(data, p);
	}
}

static void foreach_selected_key(PEData *data, ForKeyFunc func)
{
	PTCacheEdit *edit = data->edit;
	POINT_P; KEY_K;

	LOOP_VISIBLE_POINTS {
		LOOP_SELECTED_KEYS {
			func(data, p, k);
		}
	}
}

static void foreach_point(PEData *data, ForPointFunc func)
{
	PTCacheEdit *edit = data->edit;
	POINT_P;

	LOOP_POINTS { 
		func(data, p);
	}
}

static int count_selected_keys(Scene *scene, PTCacheEdit *edit)
{
	ParticleEditSettings *pset= PE_settings(scene);
	POINT_P; KEY_K;
	int sel= 0;

	LOOP_VISIBLE_POINTS {
		if(pset->selectmode==SCE_SELECT_POINT) {
			LOOP_SELECTED_KEYS {
				sel++;
			}
		}
		else if(pset->selectmode==SCE_SELECT_END) {
			key = point->keys + point->totkey - 1;
			if(key->flag & PEK_SELECT)
				sel++;
		}
	}

	return sel;
}

/************************************************/
/*			Particle Edit Mirroring				*/
/************************************************/

static void PE_update_mirror_cache(Object *ob, ParticleSystem *psys)
{
	PTCacheEdit *edit;
	ParticleSystemModifierData *psmd;
	KDTree *tree;
	KDTreeNearest nearest;
	HairKey *key;
	PARTICLE_P;
	float mat[4][4], co[3];
	int index, totpart;

	edit= psys->edit;
	psmd= psys_get_modifier(ob, psys);
	totpart= psys->totpart;

	if(!psmd->dm)
		return;

	tree= BLI_kdtree_new(totpart);

	/* insert particles into kd tree */
	LOOP_PARTICLES {
		key = pa->hair;
		psys_mat_hair_to_orco(ob, psmd->dm, psys->part->from, pa, mat);
		VECCOPY(co, key->co);
		Mat4MulVecfl(mat, co);
		BLI_kdtree_insert(tree, p, co, NULL);
	}

	BLI_kdtree_balance(tree);

	/* lookup particles and set in mirror cache */
	if(!edit->mirror_cache)
		edit->mirror_cache= MEM_callocN(sizeof(int)*totpart, "PE mirror cache");
	
	LOOP_PARTICLES {
		key = pa->hair;
		psys_mat_hair_to_orco(ob, psmd->dm, psys->part->from, pa, mat);
		VECCOPY(co, key->co);
		Mat4MulVecfl(mat, co);
		co[0]= -co[0];

		index= BLI_kdtree_find_nearest(tree, co, NULL, &nearest);

		/* this needs a custom threshold still, duplicated for editmode mirror */
		if(index != -1 && index != p && (nearest.dist <= 0.0002f))
			edit->mirror_cache[p]= index;
		else
			edit->mirror_cache[p]= -1;
	}

	/* make sure mirrors are in two directions */
	LOOP_PARTICLES {
		if(edit->mirror_cache[p]) {
			index= edit->mirror_cache[p];
			if(edit->mirror_cache[index] != p)
				edit->mirror_cache[p]= -1;
		}
	}

	BLI_kdtree_free(tree);
}

static void PE_mirror_particle(Object *ob, DerivedMesh *dm, ParticleSystem *psys, ParticleData *pa, ParticleData *mpa)
{
	HairKey *hkey, *mhkey;
	PTCacheEditPoint *point, *mpoint;
	PTCacheEditKey *key, *mkey;
	PTCacheEdit *edit;
	float mat[4][4], mmat[4][4], immat[4][4];
	int i, mi, k;

	edit= psys->edit;
	i= pa - psys->particles;

	/* find mirrored particle if needed */
	if(!mpa) {
		if(!edit->mirror_cache)
			PE_update_mirror_cache(ob, psys);

		mi= edit->mirror_cache[i];
		if(mi == -1)
			return;
		mpa= psys->particles + mi;
	}
	else
		mi= mpa - psys->particles;

	point = edit->points + i;
	mpoint = edit->points + mi;

	/* make sure they have the same amount of keys */
	if(pa->totkey != mpa->totkey) {
		if(mpa->hair) MEM_freeN(mpa->hair);
		if(mpoint->keys) MEM_freeN(mpoint->keys);

		mpa->hair= MEM_dupallocN(pa->hair);
		mpoint->keys= MEM_dupallocN(point->keys);
		mpoint->totkey= point->totkey;

		mhkey= mpa->hair;
		mkey= mpoint->keys;
		for(k=0; k<mpa->totkey; k++, mkey++, mhkey++) {
			mkey->co= mhkey->co;
			mkey->time= &mhkey->time;
			mkey->flag &= PEK_SELECT;
		}
	}

	/* mirror positions and tags */
	psys_mat_hair_to_orco(ob, dm, psys->part->from, pa, mat);
	psys_mat_hair_to_orco(ob, dm, psys->part->from, mpa, mmat);
	Mat4Invert(immat, mmat);

	hkey=pa->hair;
	mhkey=mpa->hair;
	key= point->keys;
	mkey= mpoint->keys;
	for(k=0; k<pa->totkey; k++, hkey++, mhkey++, key++, mkey++) {
		VECCOPY(mhkey->co, hkey->co);
		Mat4MulVecfl(mat, mhkey->co);
		mhkey->co[0]= -mhkey->co[0];
		Mat4MulVecfl(immat, mhkey->co);

		if(key->flag & PEK_TAG)
			mkey->flag |= PEK_TAG;
	}

	if(point->flag & PEP_TAG)
		mpoint->flag |= PEP_TAG;
	if(point->flag & PEP_EDIT_RECALC)
		mpoint->flag |= PEP_EDIT_RECALC;
}

static void PE_apply_mirror(Object *ob, ParticleSystem *psys)
{
	PTCacheEdit *edit;
	ParticleSystemModifierData *psmd;
	POINT_P;

	if(!psys)
		return;

	edit= psys->edit;
	psmd= psys_get_modifier(ob, psys);

	if(!edit->mirror_cache || !psmd->dm)
		return;

	/* we delay settings the PARS_EDIT_RECALC for mirrored particles
	 * to avoid doing mirror twice */
	LOOP_POINTS {
		if(point->flag & PEP_EDIT_RECALC) {
			PE_mirror_particle(ob, psmd->dm, psys, psys->particles + p, NULL);

			if(edit->mirror_cache[p] != -1)
				edit->points[edit->mirror_cache[p]].flag &= ~PEP_EDIT_RECALC;
		}
	}

	LOOP_POINTS {
		if(point->flag & PEP_EDIT_RECALC)
			if(edit->mirror_cache[p] != -1)
				edit->points[edit->mirror_cache[p]].flag |= PEP_EDIT_RECALC;
	}
}

/************************************************/
/*			Edit Calculation					*/
/************************************************/
/* tries to stop edited particles from going through the emitter's surface */
static void pe_deflect_emitter(Scene *scene, Object *ob, PTCacheEdit *edit)
{
	ParticleEditSettings *pset= PE_settings(scene);
	ParticleSystem *psys;
	ParticleSystemModifierData *psmd;
	POINT_P; KEY_K;
	int index;
	float *vec, *nor, dvec[3], dot, dist_1st;
	float hairimat[4][4], hairmat[4][4];

	if(edit==NULL || edit->psys==NULL || (pset->flag & PE_DEFLECT_EMITTER)==0 || (edit->psys->flag & PSYS_GLOBAL_HAIR))
		return;

	psys = edit->psys;
	psmd = psys_get_modifier(ob,psys);

	if(!psmd->dm)
		return;

	LOOP_EDITED_POINTS {
		psys_mat_hair_to_object(ob, psmd->dm, psys->part->from, psys->particles + p, hairmat);
	
		LOOP_KEYS {
			Mat4MulVecfl(hairmat, key->co);
		}

		LOOP_KEYS {
			if(k==0) {
				dist_1st = VecLenf((key+1)->co, key->co);
				dist_1st *= 0.75f * pset->emitterdist;
			}
			else {
				index= BLI_kdtree_find_nearest(edit->emitter_field,key->co,NULL,NULL);
				
				vec=edit->emitter_cosnos +index*6;
				nor=vec+3;

				VecSubf(dvec, key->co, vec);

				dot=Inpf(dvec,nor);
				VECCOPY(dvec,nor);

				if(dot>0.0f) {
					if(dot<dist_1st) {
						Normalize(dvec);
						VecMulf(dvec,dist_1st-dot);
						VecAddf(key->co,key->co,dvec);
					}
				}
				else {
					Normalize(dvec);
					VecMulf(dvec,dist_1st-dot);
					VecAddf(key->co,key->co,dvec);
				}
				if(k==1)
					dist_1st*=1.3333f;
			}
		}
		
		Mat4Invert(hairimat,hairmat);

		LOOP_KEYS {
			Mat4MulVecfl(hairimat, key->co);
		}
	}
}
/* force set distances between neighbouring keys */
void PE_apply_lengths(Scene *scene, PTCacheEdit *edit)
{
	
	ParticleEditSettings *pset=PE_settings(scene);
	POINT_P; KEY_K;
	float dv1[3];

	if(edit==0 || (pset->flag & PE_KEEP_LENGTHS)==0)
		return;

	if(edit->psys && edit->psys->flag & PSYS_GLOBAL_HAIR)
		return;

	LOOP_EDITED_POINTS {
		LOOP_KEYS {
			if(k) {
				VecSubf(dv1, key->co, (key - 1)->co);
				Normalize(dv1);
				VecMulf(dv1, (key - 1)->length);
				VecAddf(key->co, (key - 1)->co, dv1);
			}
		}
	}
}
/* try to find a nice solution to keep distances between neighbouring keys */
static void pe_iterate_lengths(Scene *scene, PTCacheEdit *edit)
{
	ParticleEditSettings *pset=PE_settings(scene);
	POINT_P;
	PTCacheEditKey *key;
	int j, k;
	float tlen;
	float dv0[3]= {0.0f, 0.0f, 0.0f};
	float dv1[3]= {0.0f, 0.0f, 0.0f};
	float dv2[3]= {0.0f, 0.0f, 0.0f};

	if(edit==0 || (pset->flag & PE_KEEP_LENGTHS)==0)
		return;

	if(edit->psys && edit->psys->flag & PSYS_GLOBAL_HAIR)
		return;

	LOOP_EDITED_POINTS {
		for(j=1; j<point->totkey; j++) {
			float mul= 1.0f / (float)point->totkey;

			if(pset->flag & PE_LOCK_FIRST) {
				key= point->keys + 1;
				k= 1;
				dv1[0]= dv1[1]= dv1[2]= 0.0;
			}
			else {
				key= point->keys;
				k= 0;
				dv0[0]= dv0[1]= dv0[2]= 0.0;
			}

			for(; k<point->totkey; k++, key++) {
				if(k) {
					VecSubf(dv0, (key - 1)->co, key->co);
					tlen= Normalize(dv0);
					VecMulf(dv0, (mul * (tlen - (key - 1)->length)));
				}

				if(k < point->totkey - 1) {
					VecSubf(dv2, (key + 1)->co, key->co);
					tlen= Normalize(dv2);
					VecMulf(dv2, mul * (tlen - key->length));
				}

				if(k) {
					VecAddf((key-1)->co,(key-1)->co,dv1);
				}

				VECADD(dv1,dv0,dv2);
			}
		}
	}
}
/* set current distances to be kept between neighbouting keys */
static void recalc_lengths(PTCacheEdit *edit)
{
	POINT_P; KEY_K;

	if(edit==0)
		return;

	LOOP_EDITED_POINTS {
		key= point->keys;
		for(k=0; k<point->totkey-1; k++, key++) {
			key->length= VecLenf(key->co, (key + 1)->co);
		}
	}
}

/* calculate a tree for finding nearest emitter's vertice */
static void recalc_emitter_field(Object *ob, ParticleSystem *psys)
{
	DerivedMesh *dm=psys_get_modifier(ob,psys)->dm;
	PTCacheEdit *edit= psys->edit;
	MFace *mface;
	MVert *mvert;
	float *vec, *nor;
	int i, totface, totvert;

	if(!dm)
		return;

	if(edit->emitter_cosnos)
		MEM_freeN(edit->emitter_cosnos);

	BLI_kdtree_free(edit->emitter_field);

	totface=dm->getNumFaces(dm);
	totvert=dm->getNumVerts(dm);

	edit->emitter_cosnos=MEM_callocN(totface*6*sizeof(float),"emitter cosnos");

	edit->emitter_field= BLI_kdtree_new(totface);

	vec=edit->emitter_cosnos;
	nor=vec+3;

	mvert=dm->getVertDataArray(dm,CD_MVERT);
	for(i=0; i<totface; i++, vec+=6, nor+=6) {
		mface=dm->getFaceData(dm,i,CD_MFACE);

		mvert=dm->getVertData(dm,mface->v1,CD_MVERT);
		VECCOPY(vec,mvert->co);
		VECCOPY(nor,mvert->no);

		mvert=dm->getVertData(dm,mface->v2,CD_MVERT);
		VECADD(vec,vec,mvert->co);
		VECADD(nor,nor,mvert->no);

		mvert=dm->getVertData(dm,mface->v3,CD_MVERT);
		VECADD(vec,vec,mvert->co);
		VECADD(nor,nor,mvert->no);

		if(mface->v4) {
			mvert=dm->getVertData(dm,mface->v4,CD_MVERT);
			VECADD(vec,vec,mvert->co);
			VECADD(nor,nor,mvert->no);
			
			VecMulf(vec,0.25);
		}
		else
			VecMulf(vec,0.3333f);

		Normalize(nor);

		BLI_kdtree_insert(edit->emitter_field, i, vec, NULL);
	}

	BLI_kdtree_balance(edit->emitter_field);
}

static void PE_update_selection(Scene *scene, Object *ob, int useflag)
{
	PTCacheEdit *edit= PE_get_current(scene, ob);
	HairKey *hkey;
	POINT_P; KEY_K;

	/* flag all particles to be updated if not using flag */
	if(!useflag)
		LOOP_POINTS
			point->flag |= PEP_EDIT_RECALC;

	/* flush edit key flag to hair key flag to preserve selection 
	 * on save */
	if(edit->psys) LOOP_POINTS {
		hkey = edit->psys->particles[p].hair;
		LOOP_KEYS {
			hkey->editflag= key->flag;
			hkey++;
		}
	}

	psys_cache_edit_paths(scene, ob, edit, CFRA);


	/* disable update flag */
	LOOP_POINTS
		point->flag &= ~PEP_EDIT_RECALC;
}

static void update_world_cos(Object *ob, PTCacheEdit *edit)
{
	ParticleSystem *psys = edit->psys;
	ParticleSystemModifierData *psmd= psys_get_modifier(ob, psys);
	POINT_P; KEY_K;
	float hairmat[4][4];

	if(psys==0 || psys->edit==0 || psmd->dm==NULL)
		return;

	LOOP_POINTS {
		if(!(psys->flag & PSYS_GLOBAL_HAIR))
			psys_mat_hair_to_global(ob, psmd->dm, psys->part->from, psys->particles+p, hairmat);

		LOOP_KEYS {
			VECCOPY(key->world_co,key->co);
			if(!(psys->flag & PSYS_GLOBAL_HAIR))
				Mat4MulVecfl(hairmat, key->world_co);
		}
	}
}
static void update_velocities(Object *ob, PTCacheEdit *edit)
{
	/*TODO: get frs_sec properly */
	float vec1[3], vec2[3], frs_sec, dfra;
	POINT_P; KEY_K;

	/* hair doesn't use velocities */
	if(edit->psys || !edit->points || !edit->points->keys->vel)
		return;

	frs_sec = edit->pid.flag & PTCACHE_VEL_PER_SEC ? 25.0f : 1.0f;

	LOOP_EDITED_POINTS {
		LOOP_KEYS {
			if(k==0) {
				dfra = *(key+1)->time - *key->time;

				if(dfra <= 0.0f)
					continue;

				VECSUB(key->vel, (key+1)->co, key->co);

				if(point->totkey>2) {
					VECSUB(vec1, (key+1)->co, (key+2)->co);
					Projf(vec2, vec1, key->vel);
					VECSUB(vec2, vec1, vec2);
					VECADDFAC(key->vel, key->vel, vec2, 0.5f);
				}
			}
			else if(k==point->totkey-1) {
				dfra = *key->time - *(key-1)->time;

				if(dfra <= 0.0f)
					continue;

				VECSUB(key->vel, key->co, (key-1)->co);

				if(point->totkey>2) {
					VECSUB(vec1, (key-2)->co, (key-1)->co);
					Projf(vec2, vec1, key->vel);
					VECSUB(vec2, vec1, vec2);
					VECADDFAC(key->vel, key->vel, vec2, 0.5f);
				}
			}
			else {
				dfra = *(key+1)->time - *(key-1)->time;
				
				if(dfra <= 0.0f)
					continue;

				VECSUB(key->vel, (key+1)->co, (key-1)->co);
			}
			VecMulf(key->vel, frs_sec/dfra);
		}
	}
}
void PE_update_object(Scene *scene, Object *ob, int useflag)
{
	ParticleEditSettings *pset= PE_settings(scene);
	PTCacheEdit *edit = PE_get_current(scene, ob);
	POINT_P;

	if(!edit)
		return;

	/* flag all particles to be updated if not using flag */
	if(!useflag)
		LOOP_POINTS {
			point->flag |= PEP_EDIT_RECALC;
		}

	/* do post process on particle edit keys */
	pe_iterate_lengths(scene, edit);
	pe_deflect_emitter(scene, ob, edit);
	PE_apply_lengths(scene, edit);
	if(pset->flag & PE_X_MIRROR)
		PE_apply_mirror(ob,edit->psys);
	if(edit->psys)
		update_world_cos(ob, edit);
	if(pset->flag & PE_AUTO_VELOCITY)
		update_velocities(ob, edit);
	PE_hide_keys_time(scene, edit, CFRA);

	/* regenerate path caches */
	psys_cache_edit_paths(scene, ob, edit, CFRA);

	/* disable update flag */
	LOOP_POINTS {
		point->flag &= ~PEP_EDIT_RECALC;
	}

	if(edit->psys)
		edit->psys->flag &= ~PSYS_HAIR_UPDATED;
}

/************************************************/
/*			Edit Selections						*/
/************************************************/

/*-----selection callbacks-----*/

static void select_key(PEData *data, int point_index, int key_index)
{
	PTCacheEdit *edit = data->edit;
	PTCacheEditPoint *point = edit->points + point_index;
	PTCacheEditKey *key = point->keys + key_index;

	if(data->select)
		key->flag |= PEK_SELECT;
	else
		key->flag &= ~PEK_SELECT;

	point->flag |= PEP_EDIT_RECALC;
}

static void select_keys(PEData *data, int point_index, int key_index)
{
	PTCacheEdit *edit = data->edit;
	PTCacheEditPoint *point = edit->points + point_index;
	KEY_K;

	LOOP_KEYS {
		if(data->select)
			key->flag |= PEK_SELECT;
		else
			key->flag &= ~PEK_SELECT;
	}

	point->flag |= PEP_EDIT_RECALC;
}

static void toggle_key_select(PEData *data, int point_index, int key_index)
{
	PTCacheEdit *edit = data->edit;
	PTCacheEditPoint *point = edit->points + point_index;
	PTCacheEditKey *key = point->keys + key_index;

	key->flag ^= PEK_SELECT;
	point->flag |= PEP_EDIT_RECALC;
}

/************************ de select all operator ************************/

static int de_select_all_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Object *ob= CTX_data_active_object(C);
	PTCacheEdit *edit= PE_get_current(scene, ob);
	POINT_P; KEY_K;
	int sel= 0;
	
	LOOP_VISIBLE_POINTS {
		LOOP_SELECTED_KEYS {
			sel= 1;
			key->flag &= ~PEK_SELECT;
			point->flag |= PEP_EDIT_RECALC;
		}
	}

	if(sel==0) {
		LOOP_VISIBLE_POINTS {
			LOOP_KEYS {
				if(!(key->flag & PEK_SELECT)) {
					key->flag |= PEK_SELECT;
					point->flag |= PEP_EDIT_RECALC;
				}
			}
		}
	}

	PE_update_selection(scene, ob, 1);
	WM_event_add_notifier(C, NC_OBJECT|ND_PARTICLE_SELECT, ob);

	return OPERATOR_FINISHED;
}

void PARTICLE_OT_select_all_toggle(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Select or Deselect All";
	ot->idname= "PARTICLE_OT_select_all_toggle";
	
	/* api callbacks */
	ot->exec= de_select_all_exec;
	ot->poll= PE_poll;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/************************ pick select operator ************************/

int PE_mouse_particles(bContext *C, short *mval, int extend)
{
	PEData data;
	Scene *scene= CTX_data_scene(C);
	Object *ob= CTX_data_active_object(C);
	PTCacheEdit *edit= PE_get_current(scene, ob);
	POINT_P; KEY_K;
	
	if(!PE_start_edit(edit))
		return OPERATOR_CANCELLED;

	if(!extend) {
		LOOP_VISIBLE_POINTS {
			LOOP_SELECTED_KEYS {
				key->flag &= ~PEK_SELECT;
				point->flag |= PEP_EDIT_RECALC;
			}
		}
	}

	PE_set_view3d_data(C, &data);
	data.mval= mval;
	data.rad= 75.0f;

	for_mouse_hit_keys(&data, toggle_key_select, 1);  /* nearest only */

	PE_update_selection(scene, ob, 1);
	WM_event_add_notifier(C, NC_OBJECT|ND_PARTICLE_SELECT, data.ob);

	return OPERATOR_FINISHED;
}

/************************ select first operator ************************/

static void select_root(PEData *data, int point_index)
{
	data->edit->points[point_index].keys->flag |= PEK_SELECT;
}

static int select_first_exec(bContext *C, wmOperator *op)
{
	PEData data;

	PE_set_data(C, &data);
	foreach_point(&data, select_root);
	WM_event_add_notifier(C, NC_OBJECT|ND_PARTICLE_SELECT, data.ob);

	return OPERATOR_FINISHED;
}

void PARTICLE_OT_select_first(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Select First";
	ot->idname= "PARTICLE_OT_select_first";
	
	/* api callbacks */
	ot->exec= select_first_exec;
	ot->poll= PE_poll;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/************************ select last operator ************************/

static void select_tip(PEData *data, int point_index)
{
	PTCacheEditPoint *point = data->edit->points + point_index;
	point->keys[point->totkey - 1].flag |= PEK_SELECT;
}

static int select_last_exec(bContext *C, wmOperator *op)
{
	PEData data;

	PE_set_data(C, &data);
	foreach_point(&data, select_tip);
	WM_event_add_notifier(C, NC_OBJECT|ND_PARTICLE_SELECT, data.ob);

	return OPERATOR_FINISHED;
}

void PARTICLE_OT_select_last(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Select Last";
	ot->idname= "PARTICLE_OT_select_last";
	
	/* api callbacks */
	ot->exec= select_last_exec;
	ot->poll= PE_poll;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/************************ select linked operator ************************/

static int select_linked_exec(bContext *C, wmOperator *op)
{
	PEData data;
	short mval[2];
	int location[2];

	RNA_int_get_array(op->ptr, "location", location);
	mval[0]= location[0];
	mval[1]= location[1];

	view3d_operator_needs_opengl(C);

	PE_set_view3d_data(C, &data);
	data.mval= mval;
	data.rad=75.0f;
	data.select= !RNA_boolean_get(op->ptr, "deselect");

	for_mouse_hit_keys(&data, select_keys, 1);  /* nearest only */
	PE_update_selection(data.scene, data.ob, 1);
	WM_event_add_notifier(C, NC_OBJECT|ND_PARTICLE_SELECT, data.ob);

	return OPERATOR_FINISHED;
}

static int select_linked_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	ARegion *ar= CTX_wm_region(C);
	int location[2];

	location[0]= event->x - ar->winrct.xmin;
	location[1]= event->y - ar->winrct.ymin;
	RNA_int_set_array(op->ptr, "location", location);

	return select_linked_exec(C, op);
}

void PARTICLE_OT_select_linked(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Select Linked";
	ot->idname= "PARTICLE_OT_select_linked";
	
	/* api callbacks */
	ot->exec= select_linked_exec;
	ot->invoke= select_linked_invoke;
	ot->poll= PE_poll_3dview;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	/* properties */
	RNA_def_boolean(ot->srna, "deselect", 0, "Deselect", "Deselect linked keys rather than selecting them.");
	RNA_def_int_vector(ot->srna, "location", 2, NULL, 0, INT_MAX, "Location", "", 0, 16384);
}

/************************ border select operator ************************/

int PE_border_select(bContext *C, rcti *rect, int select)
{
	Scene *scene= CTX_data_scene(C);
	Object *ob= CTX_data_active_object(C);
	PTCacheEdit *edit= PE_get_current(scene, ob);
	PEData data;

	if(!PE_start_edit(edit))
		return OPERATOR_CANCELLED;

	PE_set_view3d_data(C, &data);
	data.rect= rect;
	data.select= select;

	for_mouse_hit_keys(&data, select_key, 0);

	PE_update_selection(scene, ob, 1);
	WM_event_add_notifier(C, NC_OBJECT|ND_PARTICLE_SELECT, ob);

	return OPERATOR_FINISHED;
}

/************************ circle select operator ************************/

int PE_circle_select(bContext *C, int selecting, short *mval, float rad)
{
	Scene *scene= CTX_data_scene(C);
	Object *ob= CTX_data_active_object(C);
	PTCacheEdit *edit= PE_get_current(scene, ob);
	PEData data;

	if(!PE_start_edit(edit))
		return OPERATOR_FINISHED;

	PE_set_view3d_data(C, &data);
	data.mval= mval;
	data.rad= rad;
	data.select= selecting;

	for_mouse_hit_keys(&data, select_key, 0);

	PE_update_selection(scene, ob, 1);
	WM_event_add_notifier(C, NC_OBJECT|ND_PARTICLE_SELECT, ob);

	return OPERATOR_FINISHED;
}

/************************ lasso select operator ************************/

int PE_lasso_select(bContext *C, short mcords[][2], short moves, short select)
{
	Scene *scene= CTX_data_scene(C);
	Object *ob= CTX_data_active_object(C);
	ARegion *ar= CTX_wm_region(C);
	ParticleEditSettings *pset= PE_settings(scene);
	PTCacheEdit *edit = PE_get_current(scene, ob);
	ParticleSystem *psys = edit->psys;
	ParticleSystemModifierData *psmd = psys_get_modifier(ob, psys);
	POINT_P; KEY_K;
	float co[3], mat[4][4];
	short vertco[2];

	if(!PE_start_edit(edit))
		return OPERATOR_CANCELLED;

	Mat4One(mat);

	LOOP_VISIBLE_POINTS {
		if(edit->psys && !(psys->flag & PSYS_GLOBAL_HAIR))
			psys_mat_hair_to_global(ob, psmd->dm, psys->part->from, psys->particles + p, mat);

		if(pset->selectmode==SCE_SELECT_POINT) {
			LOOP_KEYS {
				VECCOPY(co, key->co);
				Mat4MulVecfl(mat, co);
				project_short(ar, co, vertco);
				if((vertco[0] != IS_CLIPPED) && lasso_inside(mcords,moves,vertco[0],vertco[1])) {
					if(select && !(key->flag & PEK_SELECT)) {
						key->flag |= PEK_SELECT;
						point->flag |= PEP_EDIT_RECALC;
					}
					else if(key->flag & PEK_SELECT) {
						key->flag &= ~PEK_SELECT;
						point->flag |= PEP_EDIT_RECALC;
					}
				}
			}
		}
		else if(pset->selectmode==SCE_SELECT_END) {
			key= point->keys + point->totkey - 1;

			VECCOPY(co, key->co);
			Mat4MulVecfl(mat, co);
			project_short(ar, co,vertco);
			if((vertco[0] != IS_CLIPPED) && lasso_inside(mcords,moves,vertco[0],vertco[1])) {
				if(select && !(key->flag & PEK_SELECT)) {
					key->flag |= PEK_SELECT;
					point->flag |= PEP_EDIT_RECALC;
				}
				else if(key->flag & PEK_SELECT) {
					key->flag &= ~PEK_SELECT;
					point->flag |= PEP_EDIT_RECALC;
				}
			}
		}
	}

	PE_update_selection(scene, ob, 1);
	WM_event_add_notifier(C, NC_OBJECT|ND_PARTICLE_SELECT, ob);

	return OPERATOR_FINISHED;
}

/*************************** hide operator **************************/

static int hide_exec(bContext *C, wmOperator *op)
{
	Object *ob= CTX_data_active_object(C);
	Scene *scene= CTX_data_scene(C);
	PTCacheEdit *edit= PE_get_current(scene, ob);
	POINT_P; KEY_K;
	
	if(RNA_enum_get(op->ptr, "unselected")) {
		LOOP_UNSELECTED_POINTS {
			point->flag |= PEP_HIDE;
			point->flag |= PEP_EDIT_RECALC;

			LOOP_KEYS
				key->flag &= ~PEK_SELECT;
		}
	}
	else {
		LOOP_SELECTED_POINTS {
			point->flag |= PEP_HIDE;
			point->flag |= PEP_EDIT_RECALC;

			LOOP_KEYS
				key->flag &= ~PEK_SELECT;
		}
	}

	PE_update_selection(scene, ob, 1);
	WM_event_add_notifier(C, NC_OBJECT|ND_PARTICLE_SELECT, ob);

	return OPERATOR_FINISHED;
}

void PARTICLE_OT_hide(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Hide Selected";
	ot->idname= "PARTICLE_OT_hide";
	
	/* api callbacks */
	ot->exec= hide_exec;
	ot->poll= PE_poll;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	/* props */
	RNA_def_boolean(ot->srna, "unselected", 0, "Unselected", "Hide unselected rather than selected.");
}

/*************************** reveal operator **************************/

static int reveal_exec(bContext *C, wmOperator *op)
{
	Object *ob= CTX_data_active_object(C);
	Scene *scene= CTX_data_scene(C);
	PTCacheEdit *edit= PE_get_current(scene, ob);
	POINT_P; KEY_K;

	LOOP_POINTS {
		if(point->flag & PEP_HIDE) {
			point->flag &= ~PEP_HIDE;
			point->flag |= PEP_EDIT_RECALC;

			LOOP_KEYS
				key->flag |= PEK_SELECT;
		}
	}

	PE_update_selection(scene, ob, 1);
	WM_event_add_notifier(C, NC_OBJECT|ND_PARTICLE_SELECT, ob);

	return OPERATOR_FINISHED;
}

void PARTICLE_OT_reveal(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Reveal";
	ot->idname= "PARTICLE_OT_reveal";
	
	/* api callbacks */
	ot->exec= reveal_exec;
	ot->poll= PE_poll;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/************************ select less operator ************************/

static void select_less_keys(PEData *data, int point_index)
{
	PTCacheEdit *edit= data->edit;
	PTCacheEditPoint *point = edit->points + point_index;
	KEY_K;

	LOOP_SELECTED_KEYS {
		if(k==0) {
			if(((key+1)->flag&PEK_SELECT)==0)
				key->flag |= PEK_TAG;
		}
		else if(k==point->totkey-1) {
			if(((key-1)->flag&PEK_SELECT)==0)
				key->flag |= PEK_TAG;
		}
		else {
			if((((key-1)->flag & (key+1)->flag) & PEK_SELECT)==0)
				key->flag |= PEK_TAG;
		}
	}

	LOOP_KEYS {
		if(key->flag&PEK_TAG)
			key->flag &= ~(PEK_TAG|PEK_SELECT);
	}
}

static int select_less_exec(bContext *C, wmOperator *op)
{
	PEData data;

	PE_set_data(C, &data);
	foreach_point(&data, select_less_keys);
	WM_event_add_notifier(C, NC_OBJECT|ND_PARTICLE_SELECT, data.ob);

	return OPERATOR_FINISHED;
}

void PARTICLE_OT_select_less(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Select Less";
	ot->idname= "PARTICLE_OT_select_less";
	
	/* api callbacks */
	ot->exec= select_less_exec;
	ot->poll= PE_poll;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/************************ select more operator ************************/

static void select_more_keys(PEData *data, int point_index)
{
	PTCacheEdit *edit= data->edit;
	PTCacheEditPoint *point = edit->points + point_index;
	KEY_K;

	LOOP_KEYS {
		if(key->flag & PEK_SELECT) continue;

		if(k==0) {
			if((key+1)->flag&PEK_SELECT)
				key->flag |= PEK_TAG;
		}
		else if(k==point->totkey-1) {
			if((key-1)->flag&PEK_SELECT)
				key->flag |= PEK_TAG;
		}
		else {
			if(((key-1)->flag | (key+1)->flag) & PEK_SELECT)
				key->flag |= PEK_TAG;
		}
	}

	LOOP_KEYS {
		if(key->flag&PEK_TAG) {
			key->flag &= ~PEK_TAG;
			key->flag |= PEK_SELECT;
		}
	}
}

static int select_more_exec(bContext *C, wmOperator *op)
{
	PEData data;

	PE_set_data(C, &data);
	foreach_point(&data, select_more_keys);
	WM_event_add_notifier(C, NC_OBJECT|ND_PARTICLE_SELECT, data.ob);

	return OPERATOR_FINISHED;
}

void PARTICLE_OT_select_more(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Select More";
	ot->idname= "PARTICLE_OT_select_more";
	
	/* api callbacks */
	ot->exec= select_more_exec;
	ot->poll= PE_poll;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/************************ rekey operator ************************/

static void rekey_particle(PEData *data, int pa_index)
{
	PTCacheEdit *edit= data->edit;
	ParticleSystem *psys= edit->psys;
	ParticleSimulationData sim = {data->scene, data->ob, edit->psys, NULL};
	ParticleData *pa= psys->particles + pa_index;
	PTCacheEditPoint *point = edit->points + pa_index;
	ParticleKey state;
	HairKey *key, *new_keys, *okey;
	PTCacheEditKey *ekey;
	float dval, sta, end;
	int k;

	pa->flag |= PARS_REKEY;

	key= new_keys= MEM_callocN(data->totrekey * sizeof(HairKey),"Hair re-key keys");

	okey = pa->hair;
	/* root and tip stay the same */
	VECCOPY(key->co, okey->co);
	VECCOPY((key + data->totrekey - 1)->co, (okey + pa->totkey - 1)->co);

	sta= key->time= okey->time;
	end= (key + data->totrekey - 1)->time= (okey + pa->totkey - 1)->time;
	dval= (end - sta) / (float)(data->totrekey - 1);

	/* interpolate new keys from old ones */
	for(k=1,key++; k<data->totrekey-1; k++,key++) {
		state.time= (float)k / (float)(data->totrekey-1);
		psys_get_particle_on_path(&sim, pa_index, &state, 0);
		VECCOPY(key->co, state.co);
		key->time= sta + k * dval;
	}

	/* replace keys */
	if(pa->hair)
		MEM_freeN(pa->hair);
	pa->hair= new_keys;

	point->totkey=pa->totkey=data->totrekey;


	if(point->keys)
		MEM_freeN(point->keys);
	ekey= point->keys= MEM_callocN(pa->totkey * sizeof(PTCacheEditKey),"Hair re-key edit keys");
		
	for(k=0, key=pa->hair; k<pa->totkey; k++, key++, ekey++) {
		ekey->co= key->co;
		ekey->time= &key->time;
		if(!(psys->flag & PSYS_GLOBAL_HAIR))
			ekey->flag |= PEK_USE_WCO;
	}

	pa->flag &= ~PARS_REKEY;
	point->flag |= PEP_EDIT_RECALC;
}

static int rekey_exec(bContext *C, wmOperator *op)
{
	PEData data;

	PE_set_data(C, &data);

	data.dval= 1.0f / (float)(data.totrekey-1);
	data.totrekey= RNA_int_get(op->ptr, "keys");

	foreach_selected_point(&data, rekey_particle);
	
	recalc_lengths(data.edit);
	PE_update_object(data.scene, data.ob, 1);
	WM_event_add_notifier(C, NC_OBJECT|ND_PARTICLE_DATA, data.ob);

	return OPERATOR_FINISHED;
}

void PARTICLE_OT_rekey(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Rekey";
	ot->idname= "PARTICLE_OT_rekey";
	
	/* api callbacks */
	ot->exec= rekey_exec;
	ot->invoke= WM_operator_props_popup;
	ot->poll= PE_poll;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	/* properties */
	RNA_def_int(ot->srna, "keys", 2, 2, INT_MAX, "Number of Keys", "", 2, 100);
}

static void rekey_particle_to_time(Scene *scene, Object *ob, int pa_index, float path_time)
{
	PTCacheEdit *edit= PE_get_current(scene, ob);
	ParticleSystem *psys;
	ParticleSimulationData sim = {scene, ob, edit ? edit->psys : NULL, NULL};
	ParticleData *pa;
	ParticleKey state;
	HairKey *new_keys, *key;
	PTCacheEditKey *ekey;
	int k;

	if(!edit || !edit->psys) return;

	psys = edit->psys;

	pa= psys->particles + pa_index;

	pa->flag |= PARS_REKEY;

	key= new_keys= MEM_dupallocN(pa->hair);
	
	/* interpolate new keys from old ones (roots stay the same) */
	for(k=1, key++; k < pa->totkey; k++, key++) {
		state.time= path_time * (float)k / (float)(pa->totkey-1);
		psys_get_particle_on_path(&sim, pa_index, &state, 0);
		VECCOPY(key->co, state.co);
	}

	/* replace hair keys */
	if(pa->hair)
		MEM_freeN(pa->hair);
	pa->hair= new_keys;

	/* update edit pointers */
	for(k=0, key=pa->hair, ekey=edit->points[pa_index].keys; k<pa->totkey; k++, key++, ekey++) {
		ekey->co= key->co;
		ekey->time= &key->time;
	}

	pa->flag &= ~PARS_REKEY;
}

/************************* utilities **************************/

static int remove_tagged_particles(Scene *scene, Object *ob, ParticleSystem *psys)
{
	PTCacheEdit *edit = psys->edit;
	ParticleEditSettings *pset= PE_settings(scene);
	ParticleData *pa, *npa=0, *new_pars=0;
	POINT_P;
	PTCacheEditPoint *npoint=0, *new_points=0;
	ParticleSystemModifierData *psmd;
	int i, totpart, new_totpart= psys->totpart, removed= 0;

	if(pset->flag & PE_X_MIRROR) {
		/* mirror tags */
		psmd= psys_get_modifier(ob, psys);
		totpart= psys->totpart;

		LOOP_TAGGED_POINTS {
			PE_mirror_particle(ob, psmd->dm, psys, psys->particles + p, NULL);
		}
	}

	LOOP_TAGGED_POINTS {
		new_totpart--;
		removed++;
	}

	if(new_totpart != psys->totpart) {
		if(new_totpart) {
			npa= new_pars= MEM_callocN(new_totpart * sizeof(ParticleData), "ParticleData array");
			npoint= new_points= MEM_callocN(new_totpart * sizeof(PTCacheEditPoint), "PTCacheEditKey array");
		}

		pa= psys->particles;
		point= edit->points;
		for(i=0; i<psys->totpart; i++, pa++, point++) {
			if(point->flag & PEP_TAG) {
				if(point->keys)
					MEM_freeN(point->keys);
				if(pa->hair)
					MEM_freeN(pa->hair);
			}
			else {
				memcpy(npa, pa, sizeof(ParticleData));
				memcpy(npoint, point, sizeof(PTCacheEditPoint));
				npa++;
				npoint++;
			}
		}

		if(psys->particles) MEM_freeN(psys->particles);
		psys->particles= new_pars;

		if(edit->points) MEM_freeN(edit->points);
		edit->points= new_points;

		if(edit->mirror_cache) {
			MEM_freeN(edit->mirror_cache);
			edit->mirror_cache= NULL;
		}

		edit->totpoint= psys->totpart= new_totpart;
	}

	return removed;
}

static void remove_tagged_keys(Scene *scene, Object *ob, ParticleSystem *psys)
{
	PTCacheEdit *edit= psys->edit;
	ParticleEditSettings *pset= PE_settings(scene);
	ParticleData *pa;
	HairKey *hkey, *nhkey, *new_hkeys=0;
	POINT_P; KEY_K;
	ParticleSystemModifierData *psmd;
	short new_totkey;

	if(pset->flag & PE_X_MIRROR) {
		/* mirror key tags */
		psmd= psys_get_modifier(ob, psys);

		LOOP_POINTS {
			LOOP_TAGGED_KEYS {
				PE_mirror_particle(ob, psmd->dm, psys, psys->particles + p, NULL);
				break;
			}
		}
	}

	LOOP_POINTS {
		new_totkey= point->totkey;
		LOOP_TAGGED_KEYS {
			new_totkey--;
		}
		/* we can't have elements with less than two keys*/
		if(new_totkey < 2)
			point->flag |= PEP_TAG;
	}
	remove_tagged_particles(scene, ob, psys);

	LOOP_POINTS {
		pa = psys->particles + p;
		new_totkey= pa->totkey;

		LOOP_TAGGED_KEYS {
			new_totkey--;
		}

		if(new_totkey != pa->totkey) {
			hkey= pa->hair;
			nhkey= new_hkeys= MEM_callocN(new_totkey*sizeof(HairKey), "HairKeys");

			LOOP_KEYS {
				while(key->flag & PEK_TAG && hkey < pa->hair + pa->totkey) {
					key++;
					hkey++;
				}

				if(hkey < pa->hair + pa->totkey) {
					VECCOPY(nhkey->co, hkey->co);
					nhkey->time= hkey->time;
					nhkey->weight= hkey->weight;
				}
				hkey++;
				nhkey++;
			}
			if(pa->hair)
				MEM_freeN(pa->hair);
			
			pa->hair= new_hkeys;

			point->totkey= pa->totkey= new_totkey;

			if(point->keys)
				MEM_freeN(point->keys);
			key= point->keys= MEM_callocN(new_totkey*sizeof(PTCacheEditKey), "particle edit keys");

			hkey = pa->hair;
			LOOP_KEYS {
				key->co= hkey->co;
				key->time= &hkey->time;
				hkey++;
			}
		}
	}
}

/************************ subdivide opertor *********************/

/* works like normal edit mode subdivide, inserts keys between neighbouring selected keys */
static void subdivide_particle(PEData *data, int pa_index)
{
	PTCacheEdit *edit= data->edit;
	ParticleSystem *psys= edit->psys;
	ParticleSimulationData sim = {data->scene, data->ob, edit->psys, NULL};
	ParticleData *pa= psys->particles + pa_index;
	PTCacheEditPoint *point = edit->points + pa_index;
	ParticleKey state;
	HairKey *key, *nkey, *new_keys;
	PTCacheEditKey *ekey, *nekey, *new_ekeys;

	int k;
	short totnewkey=0;
	float endtime;

	for(k=0, ekey=point->keys; k<pa->totkey-1; k++,ekey++) {
		if(ekey->flag&PEK_SELECT && (ekey+1)->flag&PEK_SELECT)
			totnewkey++;
	}

	if(totnewkey==0) return;

	pa->flag |= PARS_REKEY;

	nkey= new_keys= MEM_callocN((pa->totkey+totnewkey)*(sizeof(HairKey)),"Hair subdivide keys");
	nekey= new_ekeys= MEM_callocN((pa->totkey+totnewkey)*(sizeof(PTCacheEditKey)),"Hair subdivide edit keys");
	
	key = pa->hair;
	endtime= key[pa->totkey-1].time;

	for(k=0, ekey=point->keys; k<pa->totkey-1; k++, key++, ekey++) {

		memcpy(nkey,key,sizeof(HairKey));
		memcpy(nekey,ekey,sizeof(PTCacheEditKey));

		nekey->co= nkey->co;
		nekey->time= &nkey->time;

		nkey++;
		nekey++;

		if(ekey->flag & PEK_SELECT && (ekey+1)->flag & PEK_SELECT) {
			nkey->time= (key->time + (key+1)->time)*0.5f;
			state.time= (endtime != 0.0f)? nkey->time/endtime: 0.0f;
			psys_get_particle_on_path(&sim, pa_index, &state, 0);
			VECCOPY(nkey->co, state.co);

			nekey->co= nkey->co;
			nekey->time= &nkey->time;
			nekey->flag |= PEK_SELECT;
			if(!(psys->flag & PSYS_GLOBAL_HAIR))
				nekey->flag |= PEK_USE_WCO;

			nekey++;
			nkey++;
		}
	}
	/*tip still not copied*/
	memcpy(nkey,key,sizeof(HairKey));
	memcpy(nekey,ekey,sizeof(PTCacheEditKey));

	nekey->co= nkey->co;
	nekey->time= &nkey->time;

	if(pa->hair)
		MEM_freeN(pa->hair);
	pa->hair= new_keys;

	if(point->keys)
		MEM_freeN(point->keys);
	point->keys= new_ekeys;

	point->totkey = pa->totkey = pa->totkey + totnewkey;
	point->flag |= PEP_EDIT_RECALC;
	pa->flag &= ~PARS_REKEY;
}

static int subdivide_exec(bContext *C, wmOperator *op)
{
	PEData data;

	PE_set_data(C, &data);
	foreach_point(&data, subdivide_particle);
	
	recalc_lengths(data.edit);
	PE_update_object(data.scene, data.ob, 1);
	WM_event_add_notifier(C, NC_OBJECT|ND_PARTICLE_DATA, data.ob);

	return OPERATOR_FINISHED;
}

void PARTICLE_OT_subdivide(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Subdivide";
	ot->idname= "PARTICLE_OT_subdivide";
	
	/* api callbacks */
	ot->exec= subdivide_exec;
	ot->poll= PE_poll;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/************************ remove doubles opertor *********************/

static int remove_doubles_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Object *ob= CTX_data_active_object(C);
	ParticleEditSettings *pset=PE_settings(scene);
	PTCacheEdit *edit= PE_get_current(scene, ob);
	ParticleSystem *psys = edit->psys;
	ParticleSystemModifierData *psmd;
	KDTree *tree;
	KDTreeNearest nearest[10];
	POINT_P;
	float mat[4][4], co[3], threshold= RNA_float_get(op->ptr, "threshold");
	int n, totn, removed, flag, totremoved;

	if(psys->flag & PSYS_GLOBAL_HAIR)
		return OPERATOR_CANCELLED;

	edit= psys->edit;
	psmd= psys_get_modifier(ob, psys);
	totremoved= 0;

	do {
		removed= 0;

		tree=BLI_kdtree_new(psys->totpart);
			
		/* insert particles into kd tree */
		LOOP_SELECTED_POINTS {
			psys_mat_hair_to_object(ob, psmd->dm, psys->part->from, psys->particles+p, mat);
			VECCOPY(co, point->keys->co);
			Mat4MulVecfl(mat, co);
			BLI_kdtree_insert(tree, p, co, NULL);
		}

		BLI_kdtree_balance(tree);

		/* tag particles to be removed */
		LOOP_SELECTED_POINTS {
			psys_mat_hair_to_object(ob, psmd->dm, psys->part->from, psys->particles+p, mat);
			VECCOPY(co, point->keys->co);
			Mat4MulVecfl(mat, co);

			totn= BLI_kdtree_find_n_nearest(tree,10,co,NULL,nearest);

			for(n=0; n<totn; n++) {
				/* this needs a custom threshold still */
				if(nearest[n].index > p && nearest[n].dist < threshold) {
					if(!(point->flag & PEP_TAG)) {
						point->flag |= PEP_TAG;
						removed++;
					}
				}
			}
		}

		BLI_kdtree_free(tree);

		/* remove tagged particles - don't do mirror here! */
		flag= pset->flag;
		pset->flag &= ~PE_X_MIRROR;
		remove_tagged_particles(scene, ob, psys);
		pset->flag= flag;
		totremoved += removed;
	} while(removed);

	if(totremoved == 0)
		return OPERATOR_CANCELLED;

	BKE_reportf(op->reports, RPT_INFO, "Remove %d double particles.", totremoved);

	PE_update_object(scene, ob, 0);
	DAG_id_flush_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT|ND_PARTICLE_DATA, ob);

	return OPERATOR_FINISHED;
}

void PARTICLE_OT_remove_doubles(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Remove Doubles";
	ot->idname= "PARTICLE_OT_remove_doubles";
	
	/* api callbacks */
	ot->exec= remove_doubles_exec;
	ot->poll= PE_poll;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	/* properties */
	RNA_def_float(ot->srna, "threshold", 0.0002f, 0.0f, FLT_MAX, "Threshold", "Threshold distance withing which particles are removed", 0.00001f, 0.1f);
}

/************************ cursor drawing *******************************/

static void brush_drawcursor(bContext *C, int x, int y, void *customdata)
{
	ParticleEditSettings *pset= PE_settings(CTX_data_scene(C));
	ParticleBrushData *brush;

	if(pset->brushtype < 0)
		return;

	brush= &pset->brush[pset->brushtype];

	if(brush) {
		glPushMatrix();

		glTranslatef((float)x, (float)y, 0.0f);

		glColor4ub(255, 255, 255, 128);
		glEnable(GL_LINE_SMOOTH );
		glEnable(GL_BLEND);
		glutil_draw_lined_arc(0.0, M_PI*2.0, brush->size, 40);
		glDisable(GL_BLEND);
		glDisable(GL_LINE_SMOOTH );
		
		glPopMatrix();
	}
}

static void toggle_particle_cursor(bContext *C, int enable)
{
	ParticleEditSettings *pset= PE_settings(CTX_data_scene(C));

	if(pset->paintcursor && !enable) {
		WM_paint_cursor_end(CTX_wm_manager(C), pset->paintcursor);
		pset->paintcursor = NULL;
	}
	else if(enable)
		pset->paintcursor= WM_paint_cursor_activate(CTX_wm_manager(C), PE_poll_3dview, brush_drawcursor, NULL);
}

/********************* radial control operator *********************/

static int brush_radial_control_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	ParticleEditSettings *pset= PE_settings(CTX_data_scene(C));
	ParticleBrushData *brush;
	int mode = RNA_enum_get(op->ptr, "mode");
	float original_value=1.0f;

	if(pset->brushtype < 0)
		return OPERATOR_CANCELLED;

	brush= &pset->brush[pset->brushtype];

	toggle_particle_cursor(C, 0);

	if(mode == WM_RADIALCONTROL_SIZE)
		original_value = brush->size;
	else if(mode == WM_RADIALCONTROL_STRENGTH)
		original_value = brush->strength;

	RNA_float_set(op->ptr, "initial_value", original_value);

	return WM_radial_control_invoke(C, op, event);
}

static int brush_radial_control_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	int ret = WM_radial_control_modal(C, op, event);

	if(ret != OPERATOR_RUNNING_MODAL)
		toggle_particle_cursor(C, 1);

	return ret;
}

static int brush_radial_control_exec(bContext *C, wmOperator *op)
{
	ParticleEditSettings *pset= PE_settings(CTX_data_scene(C));
	ParticleBrushData *brush;
	int mode = RNA_enum_get(op->ptr, "mode");
	float new_value = RNA_float_get(op->ptr, "new_value");

	if(pset->brushtype < 0)
		return OPERATOR_CANCELLED;

	brush= &pset->brush[pset->brushtype];

	if(mode == WM_RADIALCONTROL_SIZE)
		brush->size= new_value;
	else if(mode == WM_RADIALCONTROL_STRENGTH)
		brush->strength= new_value;

	return OPERATOR_FINISHED;
}

void PARTICLE_OT_brush_radial_control(wmOperatorType *ot)
{
	WM_OT_radial_control_partial(ot);

	ot->name= "Brush Radial Control";
	ot->idname= "PARTICLE_OT_brush_radial_control";

	ot->invoke= brush_radial_control_invoke;
	ot->modal= brush_radial_control_modal;
	ot->exec= brush_radial_control_exec;
	ot->poll= PE_poll;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO|OPTYPE_BLOCKING;
}

/*************************** delete operator **************************/

enum { DEL_PARTICLE, DEL_KEY };

static EnumPropertyItem delete_type_items[]= {
	{DEL_PARTICLE, "PARTICLE", 0, "Particle", ""},
	{DEL_KEY, "KEY", 0, "Key", ""},
	{0, NULL, 0, NULL, NULL}};

static void set_delete_particle(PEData *data, int pa_index)
{
	PTCacheEdit *edit= data->edit;

	edit->points[pa_index].flag |= PEP_TAG;
}

static void set_delete_particle_key(PEData *data, int pa_index, int key_index)
{
	PTCacheEdit *edit= data->edit;

	edit->points[pa_index].keys[key_index].flag |= PEK_TAG;
}

static int delete_exec(bContext *C, wmOperator *op)
{
	PEData data;
	int type= RNA_enum_get(op->ptr, "type");

	PE_set_data(C, &data);

	if(type == DEL_KEY) {
		foreach_selected_key(&data, set_delete_particle_key);
		remove_tagged_keys(data.scene, data.ob, data.edit->psys);
		recalc_lengths(data.edit);
	}
	else if(type == DEL_PARTICLE) {
		foreach_selected_point(&data, set_delete_particle);
		remove_tagged_particles(data.scene, data.ob, data.edit->psys);
		recalc_lengths(data.edit);
	}

	PE_update_object(data.scene, data.ob, 0);
	DAG_id_flush_update(&data.ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT|ND_PARTICLE_DATA, data.ob);

	return OPERATOR_FINISHED;
}

void PARTICLE_OT_delete(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Delete";
	ot->idname= "PARTICLE_OT_delete";
	
	/* api callbacks */
	ot->exec= delete_exec;
	ot->invoke= WM_menu_invoke;
	ot->poll= PE_hair_poll;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	/* properties */
	RNA_def_enum(ot->srna, "type", delete_type_items, DEL_PARTICLE, "Type", "Delete a full particle or only keys.");
}

/*************************** mirror operator **************************/

static void PE_mirror_x(Scene *scene, Object *ob, int tagged)
{
	Mesh *me= (Mesh*)(ob->data);
	ParticleSystemModifierData *psmd;
	PTCacheEdit *edit= PE_get_current(scene, ob);
	ParticleSystem *psys = edit->psys;
	ParticleData *pa, *newpa, *new_pars;
	PTCacheEditPoint *newpoint, *new_points;
	POINT_P; KEY_K;
	HairKey *hkey;
	int *mirrorfaces;
	int rotation, totpart, newtotpart;

	if(psys->flag & PSYS_GLOBAL_HAIR)
		return;

	psmd= psys_get_modifier(ob, psys);
	if(!psmd->dm)
		return;

	mirrorfaces= mesh_get_x_mirror_faces(ob, NULL);

	if(!edit->mirror_cache)
		PE_update_mirror_cache(ob, psys);

	totpart= psys->totpart;
	newtotpart= psys->totpart;
	LOOP_VISIBLE_POINTS {
		pa = psys->particles + p;
		if(!tagged) {
			if(point_is_selected(point)) {
				if(edit->mirror_cache[p] != -1) {
					/* already has a mirror, don't need to duplicate */
					PE_mirror_particle(ob, psmd->dm, psys, pa, NULL);
					continue;
				}
				else
					point->flag |= PEP_TAG;
			}
		}

		if((point->flag & PEP_TAG) && mirrorfaces[pa->num*2] != -1)
			newtotpart++;
	}

	if(newtotpart != psys->totpart) {
		/* allocate new arrays and copy existing */
		new_pars= MEM_callocN(newtotpart*sizeof(ParticleData), "ParticleData new");
		new_points= MEM_callocN(newtotpart*sizeof(PTCacheEditPoint), "PTCacheEditPoint new");

		if(psys->particles) {
			memcpy(new_pars, psys->particles, totpart*sizeof(ParticleData));
			MEM_freeN(psys->particles);
		}
		psys->particles= new_pars;

		if(edit->points) {
			memcpy(new_points, edit->points, totpart*sizeof(PTCacheEditPoint));
			MEM_freeN(edit->points);
		}
		edit->points= new_points;

		if(edit->mirror_cache) {
			MEM_freeN(edit->mirror_cache);
			edit->mirror_cache= NULL;
		}

		edit->totpoint= psys->totpart= newtotpart;
			
		/* create new elements */
		newpa= psys->particles + totpart;
		newpoint= edit->points + totpart;

		LOOP_VISIBLE_POINTS {
			pa = psys->particles + p;

			if(!(point->flag & PEP_TAG) || mirrorfaces[pa->num*2] == -1)
				continue;

			/* duplicate */
			*newpa= *pa;
			*newpoint= *point;
			if(pa->hair) newpa->hair= MEM_dupallocN(pa->hair);
			if(point->keys) newpoint->keys= MEM_dupallocN(point->keys);

			/* rotate weights according to vertex index rotation */
			rotation= mirrorfaces[pa->num*2+1];
			newpa->fuv[0]= pa->fuv[2];
			newpa->fuv[1]= pa->fuv[1];
			newpa->fuv[2]= pa->fuv[0];
			newpa->fuv[3]= pa->fuv[3];
			while(rotation-- > 0)
				if(me->mface[pa->num].v4)
					SHIFT4(float, newpa->fuv[0], newpa->fuv[1], newpa->fuv[2], newpa->fuv[3])
				else
					SHIFT3(float, newpa->fuv[0], newpa->fuv[1], newpa->fuv[2])

			/* assign face inddex */
			newpa->num= mirrorfaces[pa->num*2];
			newpa->num_dmcache= psys_particle_dm_face_lookup(ob,psmd->dm,newpa->num,newpa->fuv, NULL);

			/* update edit key pointers */
			key= newpoint->keys;
			for(k=0, hkey=newpa->hair; k<newpa->totkey; k++, hkey++, key++) {
				key->co= hkey->co;
				key->time= &hkey->time;
			}

			/* map key positions as mirror over x axis */
			PE_mirror_particle(ob, psmd->dm, psys, pa, newpa);

			newpa++;
			newpoint++;
		}
	}

	LOOP_POINTS {
		point->flag &= ~PEP_TAG;
	}

	MEM_freeN(mirrorfaces);
}

static int mirror_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Object *ob= CTX_data_active_object(C);
	PTCacheEdit *edit= PE_get_current(scene, ob);
	
	PE_mirror_x(scene, ob, 0);

	update_world_cos(ob, edit);
	WM_event_add_notifier(C, NC_OBJECT|ND_PARTICLE_DATA, ob);
	DAG_id_flush_update(&ob->id, OB_RECALC_DATA);

	return OPERATOR_FINISHED;
}

void PARTICLE_OT_mirror(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Mirror";
	ot->idname= "PARTICLE_OT_mirror";
	
	/* api callbacks */
	ot->exec= mirror_exec;
	ot->poll= PE_poll;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/*********************** set brush operator **********************/

static EnumPropertyItem brush_type_items[]= {
	{PE_BRUSH_NONE, "NONE", 0, "None", ""},
	{PE_BRUSH_COMB, "COMB", 0, "Comb", ""},
	{PE_BRUSH_SMOOTH, "SMOOTH", 0, "Smooth", ""},
	{PE_BRUSH_ADD, "ADD", 0, "Add", ""},
	{PE_BRUSH_LENGTH, "LENGTH", 0, "Length", ""},
	{PE_BRUSH_PUFF, "PUFF", 0, "Puff", ""},
	{PE_BRUSH_CUT, "CUT", 0, "Cut", ""},
	{0, NULL, 0, NULL, NULL}
};

static int set_brush_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	ParticleEditSettings *pset= PE_settings(scene);

	pset->brushtype= RNA_enum_get(op->ptr, "type");

	return OPERATOR_FINISHED;
}

void PARTICLE_OT_brush_set(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Set Brush";
	ot->idname= "PARTICLE_OT_brush_set";
	
	/* api callbacks */
	ot->exec= set_brush_exec;
	ot->invoke= WM_menu_invoke;
	ot->poll= PE_poll;

	/* properties */
	RNA_def_enum(ot->srna, "type", brush_type_items, PE_BRUSH_NONE, "Type", "Brush type to select for editing.");
}


/*********************** set mode operator **********************/

static EnumPropertyItem edit_type_items[]= {
	{PE_TYPE_PARTICLES, "PARTICLES", 0, "Particles", ""},
	{PE_TYPE_SOFTBODY, "SOFTBODY", 0, "Soft body", ""},
	{PE_TYPE_CLOTH, "CLOTH", 0, "Cloth", ""},
	{0, NULL, 0, NULL, NULL}
};

static int set_edit_mode_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	ParticleEditSettings *pset= PE_settings(scene);

	pset->edittype= RNA_enum_get(op->ptr, "type");

	return OPERATOR_FINISHED;
}

void PARTICLE_OT_edit_type_set(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Set Edit Type";
	ot->idname= "PARTICLE_OT_edit_type_set";
	
	/* api callbacks */
	ot->exec= set_edit_mode_exec;
	ot->invoke= WM_menu_invoke;
	ot->poll= PE_poll;

	/* properties */
	RNA_def_enum(ot->srna, "type", edit_type_items, PE_TYPE_PARTICLES, "Type", "Edit type to select for editing.");
}

/************************* brush edit callbacks ********************/

static void brush_comb(PEData *data, float mat[][4], float imat[][4], int point_index, int key_index, PTCacheEditKey *key)
{
	ParticleEditSettings *pset= PE_settings(data->scene);
	float cvec[3], fac;

	if(pset->flag & PE_LOCK_FIRST && key_index == 0) return;

	fac= (float)pow((double)(1.0f - data->dist / data->rad), (double)data->combfac);

	VECCOPY(cvec,data->dvec);
	Mat4Mul3Vecfl(imat,cvec);
	VecMulf(cvec, fac);
	VECADD(key->co, key->co, cvec);

	(data->edit->points + point_index)->flag |= PEP_EDIT_RECALC;
}

static void brush_cut(PEData *data, int pa_index)
{
	PTCacheEdit *edit = data->edit;
	ARegion *ar= data->vc.ar;
	Object *ob= data->ob;
	ParticleEditSettings *pset= PE_settings(data->scene);
	ParticleCacheKey *key= edit->pathcache[pa_index];
	float rad2, cut_time= 1.0;
	float x0, x1, v0, v1, o0, o1, xo0, xo1, d, dv;
	int k, cut, keys= (int)pow(2.0, (double)pset->draw_step);
	short vertco[2];

	/* blunt scissors */
	if(BLI_frand() > data->cutfac) return;

	rad2= data->rad * data->rad;

	cut=0;

	project_short_noclip(ar, key->co, vertco);
	x0= (float)vertco[0];
	x1= (float)vertco[1];

	o0= (float)data->mval[0];
	o1= (float)data->mval[1];
	
	xo0= x0 - o0;
	xo1= x1 - o1;

	/* check if root is inside circle */
	if(xo0*xo0 + xo1*xo1 < rad2 && key_test_depth(data, key->co)) {
		cut_time= -1.0f;
		cut= 1;
	}
	else {
		/* calculate path time closest to root that was inside the circle */
		for(k=1, key++; k<=keys; k++, key++) {
			project_short_noclip(ar, key->co, vertco);

			if(key_test_depth(data, key->co) == 0) {
				x0= (float)vertco[0];
				x1= (float)vertco[1];

				xo0= x0 - o0;
				xo1= x1 - o1;
				continue;
			}

			v0= (float)vertco[0] - x0;
			v1= (float)vertco[1] - x1;

			dv= v0*v0 + v1*v1;

			d= (v0*xo1 - v1*xo0);
			
			d= dv * rad2 - d*d;

			if(d > 0.0f) {
				d= sqrt(d);

				cut_time= -(v0*xo0 + v1*xo1 + d);

				if(cut_time > 0.0f) {
					cut_time /= dv;

					if(cut_time < 1.0f) {
						cut_time += (float)(k-1);
						cut_time /= (float)keys;
						cut= 1;
						break;
					}
				}
			}

			x0= (float)vertco[0];
			x1= (float)vertco[1];

			xo0= x0 - o0;
			xo1= x1 - o1;
		}
	}

	if(cut) {
		if(cut_time < 0.0f) {
			edit->points[pa_index].flag |= PEP_TAG;
		}
		else {
			rekey_particle_to_time(data->scene, ob, pa_index, cut_time);
			edit->points[pa_index].flag |= PEP_EDIT_RECALC;
		}
	}
}

static void brush_length(PEData *data, int point_index)
{
	PTCacheEdit *edit= data->edit;
	PTCacheEditPoint *point = edit->points + point_index;
	KEY_K;
	float dvec[3],pvec[3];

	LOOP_KEYS {
		if(k==0) {
			VECCOPY(pvec,key->co);
		}
		else {
			VECSUB(dvec,key->co,pvec);
			VECCOPY(pvec,key->co);
			VecMulf(dvec,data->growfac);
			VECADD(key->co,(key-1)->co,dvec);
		}
	}

	point->flag |= PEP_EDIT_RECALC;
}

static void brush_puff(PEData *data, int point_index)
{
	PTCacheEdit *edit = data->edit;
	ParticleSystem *psys = edit->psys;
	PTCacheEditPoint *point = edit->points + point_index;
	KEY_K;
	float mat[4][4], imat[4][4];
	float lastco[3], rootco[3], co[3], nor[3], kco[3], dco[3], fac, length;

	if(psys && !(psys->flag & PSYS_GLOBAL_HAIR)) {
		psys_mat_hair_to_global(data->ob, data->dm, psys->part->from, psys->particles + point_index, mat);
		Mat4Invert(imat,mat);
	}
	else {
		Mat4One(mat);
		Mat4One(imat);
	}

	LOOP_KEYS {
		if(k==0) {
			/* find root coordinate and normal on emitter */
			VECCOPY(co, key->co);
			Mat4MulVecfl(mat, co);

			point_index= BLI_kdtree_find_nearest(edit->emitter_field, co, NULL, NULL);
			if(point_index == -1) return;

			VECCOPY(rootco, co);
			VecCopyf(nor, &edit->emitter_cosnos[point_index*6+3]);
			Normalize(nor);
			length= 0.0f;

			fac= (float)pow((double)(1.0f - data->dist / data->rad), (double)data->pufffac);
			fac *= 0.025f;
			if(data->invert)
				fac= -fac;
		}
		else {
			/* compute position as if hair was standing up straight */
			VECCOPY(lastco, co);
			VECCOPY(co, key->co);
			Mat4MulVecfl(mat, co);
			length += VecLenf(lastco, co);

			VECADDFAC(kco, rootco, nor, length);

			/* blend between the current and straight position */
			VECSUB(dco, kco, co);
			VECADDFAC(co, co, dco, fac);

			VECCOPY(key->co, co);
			Mat4MulVecfl(imat, key->co);
		}
	}

	point->flag |= PEP_EDIT_RECALC;
}

static void brush_smooth_get(PEData *data, float mat[][4], float imat[][4], int point_index, int key_index, PTCacheEditKey *key)
{	
	if(key_index) {
		float dvec[3];

		VecSubf(dvec,key->co,(key-1)->co);
		Mat4Mul3Vecfl(mat,dvec);
		VECADD(data->vec,data->vec,dvec);
		data->tot++;
	}
}

static void brush_smooth_do(PEData *data, float mat[][4], float imat[][4], int point_index, int key_index, PTCacheEditKey *key)
{
	float vec[3], dvec[3];
	
	if(key_index) {
		VECCOPY(vec,data->vec);
		Mat4Mul3Vecfl(imat,vec);

		VecSubf(dvec,key->co,(key-1)->co);

		VECSUB(dvec,vec,dvec);
		VecMulf(dvec,data->smoothfac);
		
		VECADD(key->co,key->co,dvec);
	}

	(data->edit->points + point_index)->flag |= PEP_EDIT_RECALC;
}

static void brush_add(PEData *data, short number)
{
	Scene *scene= data->scene;
	Object *ob= data->ob;
	PTCacheEdit *edit = data->edit;
	ParticleSystem *psys= edit->psys;
	ParticleData *add_pars= MEM_callocN(number*sizeof(ParticleData),"ParticleData add");
	ParticleSystemModifierData *psmd= psys_get_modifier(ob,psys);
	ParticleSimulationData sim = {scene, ob, psys, psmd};
	ParticleEditSettings *pset= PE_settings(scene);
	int i, k, n= 0, totpart= psys->totpart;
	float mco[2];
	short dmx= 0, dmy= 0;
	float co1[3], co2[3], min_d, imat[4][4];
	float framestep, timestep= psys_get_timestep(&sim);
	short size= pset->brush[PE_BRUSH_ADD].size;
	short size2= size*size;
	DerivedMesh *dm=0;
	Mat4Invert(imat,ob->obmat);

	if(psys->flag & PSYS_GLOBAL_HAIR)
		return;

	BLI_srandom(psys->seed+data->mval[0]+data->mval[1]);
	
	/* painting onto the deformed mesh, could be an option? */
	if(psmd->dm->deformedOnly)
		dm= psmd->dm;
	else
		dm= mesh_get_derived_deform(scene, ob, CD_MASK_BAREMESH);

	for(i=0; i<number; i++) {
		if(number>1) {
			dmx=dmy=size;
			while(dmx*dmx+dmy*dmy>size2) {
				dmx=(short)((2.0f*BLI_frand()-1.0f)*size);
				dmy=(short)((2.0f*BLI_frand()-1.0f)*size);
			}
		}

		mco[0]= data->mval[0] + dmx;
		mco[1]= data->mval[1] + dmy;
		viewline(data->vc.ar, data->vc.v3d, mco, co1, co2);

		Mat4MulVecfl(imat,co1);
		Mat4MulVecfl(imat,co2);
		min_d=2.0;
		
		/* warning, returns the derived mesh face */
		if(psys_intersect_dm(scene, ob,dm,0,co1,co2,&min_d,&add_pars[n].num,add_pars[n].fuv,0,0,0,0)) {
			add_pars[n].num_dmcache= psys_particle_dm_face_lookup(ob,psmd->dm,add_pars[n].num,add_pars[n].fuv,NULL);
			n++;
		}
	}
	if(n) {
		int newtotpart=totpart+n;
		float hairmat[4][4], cur_co[3];
		KDTree *tree=0;
		ParticleData *pa, *new_pars= MEM_callocN(newtotpart*sizeof(ParticleData),"ParticleData new");
		PTCacheEditPoint *point, *new_points= MEM_callocN(newtotpart*sizeof(PTCacheEditPoint),"PTCacheEditPoint array new");
		PTCacheEditKey *key;
		HairKey *hkey;

		/* save existing elements */
		memcpy(new_pars, psys->particles, totpart * sizeof(ParticleData));
		memcpy(new_points, edit->points, totpart * sizeof(PTCacheEditPoint));

		/* change old arrays to new ones */
		if(psys->particles) MEM_freeN(psys->particles);
		psys->particles= new_pars;

		if(edit->points) MEM_freeN(edit->points);
		edit->points= new_points;

		if(edit->mirror_cache) {
			MEM_freeN(edit->mirror_cache);
			edit->mirror_cache= NULL;
		}

		/* create tree for interpolation */
		if(pset->flag & PE_INTERPOLATE_ADDED && psys->totpart) {
			tree=BLI_kdtree_new(psys->totpart);
			
			for(i=0, pa=psys->particles; i<totpart; i++, pa++) {
				psys_particle_on_dm(psmd->dm,psys->part->from,pa->num,pa->num_dmcache,pa->fuv,pa->foffset,cur_co,0,0,0,0,0);
				BLI_kdtree_insert(tree, i, cur_co, NULL);
			}

			BLI_kdtree_balance(tree);
		}

		edit->totpoint= psys->totpart= newtotpart;

		/* create new elements */
		pa= psys->particles + totpart;
		point= edit->points + totpart;

		for(i=totpart; i<newtotpart; i++, pa++, point++) {
			memcpy(pa, add_pars + i - totpart, sizeof(ParticleData));
			pa->hair= MEM_callocN(pset->totaddkey * sizeof(HairKey), "BakeKey key add");
			key= point->keys= MEM_callocN(pset->totaddkey * sizeof(PTCacheEditKey), "PTCacheEditKey add");
			point->totkey= pa->totkey= pset->totaddkey;

			for(k=0, hkey=pa->hair; k<pa->totkey; k++, hkey++, key++) {
				key->co= hkey->co;
				key->time= &hkey->time;

				if(!(psys->flag & PSYS_GLOBAL_HAIR))
					key->flag |= PEK_USE_WCO;
			}
			
			pa->size= 1.0f;
			initialize_particle(&sim, pa,i);
			reset_particle(&sim, pa, 0.0, 1.0);
			point->flag |= PEP_EDIT_RECALC;
			if(pset->flag & PE_X_MIRROR)
				point->flag |= PEP_TAG; /* signal for duplicate */
			
			framestep= pa->lifetime/(float)(pset->totaddkey-1);

			if(tree) {
				HairKey *hkey;
				ParticleKey key[3];
				KDTreeNearest ptn[3];
				int w, maxw;
				float maxd, mind, dd, totw=0.0, weight[3];

				psys_particle_on_dm(psmd->dm,psys->part->from,pa->num,pa->num_dmcache,pa->fuv,pa->foffset,co1,0,0,0,0,0);
				maxw= BLI_kdtree_find_n_nearest(tree,3,co1,NULL,ptn);

				maxd= ptn[maxw-1].dist;
				mind= ptn[0].dist;
				dd= maxd - mind;
				
				for(w=0; w<maxw; w++) {
					weight[w]= (float)pow(2.0, (double)(-6.0f * ptn[w].dist / maxd));
					totw += weight[w];
				}
				for(;w<3; w++) {
					weight[w]= 0.0f;
				}

				for(w=0; w<maxw; w++)
					weight[w] /= totw;

				for(k=0; k<pset->totaddkey; k++) {
					hkey= (HairKey*)pa->hair + k;
					hkey->time= pa->time + k * framestep;

					key[0].time= hkey->time/ 100.0f;
					psys_get_particle_on_path(&sim, ptn[0].index, key, 0);
					VecMulf(key[0].co, weight[0]);
					
					if(maxw>1) {
						key[1].time= key[0].time;
						psys_get_particle_on_path(&sim, ptn[1].index, key + 1, 0);
						VecMulf(key[1].co, weight[1]);
						VECADD(key[0].co, key[0].co, key[1].co);

						if(maxw>2) {						
							key[2].time= key[0].time;
							psys_get_particle_on_path(&sim, ptn[2].index, key + 2, 0);
							VecMulf(key[2].co, weight[2]);
							VECADD(key[0].co, key[0].co, key[2].co);
						}
					}

					if(k==0)
						VECSUB(co1, pa->state.co, key[0].co);

					VECADD(hkey->co, key[0].co, co1);

					hkey->time= key[0].time;
				}
			}
			else {
				for(k=0, hkey=pa->hair; k<pset->totaddkey; k++, hkey++) {
					VECADDFAC(hkey->co, pa->state.co, pa->state.vel, k * framestep * timestep);
					hkey->time += k * framestep;
				}
			}
			for(k=0, hkey=pa->hair; k<pset->totaddkey; k++, hkey++) {
				psys_mat_hair_to_global(ob, psmd->dm, psys->part->from, pa, hairmat);
				Mat4Invert(imat,hairmat);
				Mat4MulVecfl(imat, hkey->co);
			}
		}

		if(tree)
			BLI_kdtree_free(tree);
	}
	if(add_pars)
		MEM_freeN(add_pars);
	
	if(!psmd->dm->deformedOnly)
		dm->release(dm);
}

/************************* brush edit operator ********************/

typedef struct BrushEdit {
	Scene *scene;
	Object *ob;
	PTCacheEdit *edit;

	int first;
	int lastmouse[2];
} BrushEdit;

static int brush_edit_init(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Object *ob= CTX_data_active_object(C);
	ParticleEditSettings *pset= PE_settings(scene);
	PTCacheEdit *edit= PE_get_current(scene, ob);
	ARegion *ar= CTX_wm_region(C);
	BrushEdit *bedit;

	if(pset->brushtype < 0)
		return 0;

	initgrabz(ar->regiondata, ob->obmat[3][0], ob->obmat[3][1], ob->obmat[3][2]);

	bedit= MEM_callocN(sizeof(BrushEdit), "BrushEdit");
	bedit->first= 1;
	op->customdata= bedit;

	bedit->scene= scene;
	bedit->ob= ob;
	bedit->edit= edit;

	return 1;
}

static void brush_edit_apply(bContext *C, wmOperator *op, PointerRNA *itemptr)
{
	BrushEdit *bedit= op->customdata;
	Scene *scene= bedit->scene;
	Object *ob= bedit->ob;
	PTCacheEdit *edit= bedit->edit;
	ParticleEditSettings *pset= PE_settings(scene);
	ParticleSystemModifierData *psmd= edit->psys ? psys_get_modifier(ob, edit->psys) : NULL;
	ParticleBrushData *brush= &pset->brush[pset->brushtype];
	ARegion *ar= CTX_wm_region(C);
	float vec[3], mousef[2];
	short mval[2], mvalo[2];
	int flip, mouse[2], dx, dy, removed= 0, selected= 0;
	int lock_root = pset->flag & PE_LOCK_FIRST;

	if(!PE_start_edit(edit))
		return;

	RNA_float_get_array(itemptr, "mouse", mousef);
	mouse[0] = mousef[0];
	mouse[1] = mousef[1];
	flip= RNA_boolean_get(itemptr, "flip");

	if(bedit->first) {
		bedit->lastmouse[0]= mouse[0];
		bedit->lastmouse[1]= mouse[1];
	}

	dx= mouse[0] - bedit->lastmouse[0];
	dy= mouse[1] - bedit->lastmouse[1];

	mval[0]= mouse[0];
	mval[1]= mouse[1];

	mvalo[0]= bedit->lastmouse[0];
	mvalo[1]= bedit->lastmouse[1];

	/* disable locking temporatily for disconnected hair */
	if(edit->psys && edit->psys->flag & PSYS_GLOBAL_HAIR)
		pset->flag &= ~PE_LOCK_FIRST;

	if(((pset->brushtype == PE_BRUSH_ADD) ?
		(sqrt(dx * dx + dy * dy) > pset->brush[PE_BRUSH_ADD].step) : (dx != 0 || dy != 0))
		|| bedit->first) {

		view3d_operator_needs_opengl(C);
		selected= (short)count_selected_keys(scene, edit);

		switch(pset->brushtype) {
			case PE_BRUSH_COMB:
			{
				PEData data;

				PE_set_view3d_data(C, &data);
				data.mval= mval;
				data.rad= (float)brush->size;

				data.combfac= (float)(brush->strength - 50) / 50.0f;
				if(data.combfac < 0.0f)
					data.combfac= 1.0f - 9.0f * data.combfac;
				else
					data.combfac= 1.0f - data.combfac;

				Mat4Invert(ob->imat, ob->obmat);

				window_to_3d_delta(ar, vec, dx, dy);
				data.dvec= vec;

				foreach_mouse_hit_key(&data, brush_comb, selected);
				break;
			}
			case PE_BRUSH_CUT:
			{
				PEData data;
				
				if(edit->psys && edit->pathcache) {
					PE_set_view3d_data(C, &data);
					data.mval= mval;
					data.rad= (float)brush->size;
					data.cutfac= (float)(brush->strength / 100.0f);

					if(selected)
						foreach_selected_point(&data, brush_cut);
					else
						foreach_point(&data, brush_cut);

					removed= remove_tagged_particles(scene, ob, edit->psys);
					if(pset->flag & PE_KEEP_LENGTHS)
						recalc_lengths(edit);
				}
				break;
			}
			case PE_BRUSH_LENGTH:
			{
				PEData data;
				
				PE_set_view3d_data(C, &data);
				data.mval= mval;
				
				data.rad= (float)brush->size;
				data.growfac= (float)brush->strength / 5000.0f;

				if(brush->invert ^ flip)
					data.growfac= 1.0f - data.growfac;
				else
					data.growfac= 1.0f + data.growfac;

				foreach_mouse_hit_point(&data, brush_length, selected);

				if(pset->flag & PE_KEEP_LENGTHS)
					recalc_lengths(edit);
				break;
			}
			case PE_BRUSH_PUFF:
			{
				PEData data;

				if(edit->psys) {
					PE_set_view3d_data(C, &data);
					data.dm= psmd->dm;
					data.mval= mval;
					data.rad= (float)brush->size;

					data.pufffac= (float)(brush->strength - 50) / 50.0f;
					if(data.pufffac < 0.0f)
						data.pufffac= 1.0f - 9.0f * data.pufffac;
					else
						data.pufffac= 1.0f - data.pufffac;

					data.invert= (brush->invert ^ flip);
					Mat4Invert(ob->imat, ob->obmat);

					foreach_mouse_hit_point(&data, brush_puff, selected);
				}
				break;
			}
			case PE_BRUSH_ADD:
			{
				PEData data;

				if(edit->psys && edit->psys->part->from==PART_FROM_FACE) {
					PE_set_view3d_data(C, &data);
					data.mval= mval;

					brush_add(&data, brush->strength);

					if(pset->flag & PE_KEEP_LENGTHS)
						recalc_lengths(edit);
				}
				break;
			}
			case PE_BRUSH_SMOOTH:
			{
				PEData data;

				PE_set_view3d_data(C, &data);
				data.mval= mval;
				data.rad= (float)brush->size;

				data.vec[0]= data.vec[1]= data.vec[2]= 0.0f;
				data.tot= 0;

				data.smoothfac= (float)(brush->strength / 100.0f);

				Mat4Invert(ob->imat, ob->obmat);

				foreach_mouse_hit_key(&data, brush_smooth_get, selected);

				if(data.tot) {
					VecMulf(data.vec, 1.0f / (float)data.tot);
					foreach_mouse_hit_key(&data, brush_smooth_do, selected);
				}

				break;
			}
		}
		if((pset->flag & PE_KEEP_LENGTHS)==0)
			recalc_lengths(edit);

		if(pset->brushtype == PE_BRUSH_ADD || removed) {
			if(pset->brushtype == PE_BRUSH_ADD && (pset->flag & PE_X_MIRROR))
				PE_mirror_x(scene, ob, 1);

			update_world_cos(ob,edit);
			psys_free_path_cache(NULL, edit);
			DAG_id_flush_update(&ob->id, OB_RECALC_DATA);
		}
		else
			PE_update_object(scene, ob, 1);

		WM_event_add_notifier(C, NC_OBJECT|ND_PARTICLE_SELECT, ob);
		
		bedit->lastmouse[0]= mouse[0];
		bedit->lastmouse[1]= mouse[1];
		bedit->first= 0;
	}

	pset->flag |= lock_root;
}

static void brush_edit_exit(bContext *C, wmOperator *op)
{
	BrushEdit *bedit= op->customdata;

	MEM_freeN(bedit);
}

static int brush_edit_exec(bContext *C, wmOperator *op)
{
	if(!brush_edit_init(C, op))
		return OPERATOR_CANCELLED;

	RNA_BEGIN(op->ptr, itemptr, "stroke") {
		brush_edit_apply(C, op, &itemptr);
	}
	RNA_END;

	brush_edit_exit(C, op);

	return OPERATOR_FINISHED;
}

static void brush_edit_apply_event(bContext *C, wmOperator *op, wmEvent *event)
{
	ARegion *ar= CTX_wm_region(C);
	PointerRNA itemptr;
	float mouse[2];

	mouse[0]= event->x - ar->winrct.xmin;
	mouse[1]= event->y - ar->winrct.ymin;

	/* fill in stroke */
	RNA_collection_add(op->ptr, "stroke", &itemptr);

	RNA_float_set_array(&itemptr, "mouse", mouse);
	RNA_boolean_set(&itemptr, "flip", event->shift != 0); // XXX hardcoded

	/* apply */
	brush_edit_apply(C, op, &itemptr);
}

static int brush_edit_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	if(!brush_edit_init(C, op))
		return OPERATOR_CANCELLED;
	
	brush_edit_apply_event(C, op, event);

	WM_event_add_modal_handler(C, op);

	return OPERATOR_RUNNING_MODAL;
}

static int brush_edit_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	switch(event->type) {
		case LEFTMOUSE:
		case MIDDLEMOUSE:
		case RIGHTMOUSE: // XXX hardcoded
			brush_edit_exit(C, op);
			return OPERATOR_FINISHED;
		case MOUSEMOVE:
			brush_edit_apply_event(C, op, event);
			break;
	}

	return OPERATOR_RUNNING_MODAL;
}

static int brush_edit_cancel(bContext *C, wmOperator *op)
{
	brush_edit_exit(C, op);

	return OPERATOR_CANCELLED;
}

void PARTICLE_OT_brush_edit(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Brush Edit";
	ot->idname= "PARTICLE_OT_brush_edit";
	
	/* api callbacks */
	ot->exec= brush_edit_exec;
	ot->invoke= brush_edit_invoke;
	ot->modal= brush_edit_modal;
	ot->cancel= brush_edit_cancel;
	ot->poll= PE_poll_3dview;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO|OPTYPE_BLOCKING;

	/* properties */
	RNA_def_collection_runtime(ot->srna, "stroke", &RNA_OperatorStrokeElement, "Stroke", "");
}

/*********************** undo ***************************/

static void free_PTCacheUndo(PTCacheUndo *undo)
{
	PTCacheEditPoint *point;
	int i;

	for(i=0, point=undo->points; i<undo->totpoint; i++, point++) {
		if(undo->particles && (undo->particles + i)->hair)
			MEM_freeN((undo->particles + i)->hair);
		if(point->keys)
			MEM_freeN(point->keys);
	}
	if(undo->points)
		MEM_freeN(undo->points);

	if(undo->particles)
		MEM_freeN(undo->particles);

	BKE_ptcache_free_mem(&undo->mem_cache);
}

static void make_PTCacheUndo(PTCacheEdit *edit, PTCacheUndo *undo)
{
	PTCacheEditPoint *point;
	int i;

	undo->totpoint= edit->totpoint;

	if(edit->psys) {
		ParticleData *pa;

		pa= undo->particles= MEM_dupallocN(edit->psys->particles);

		for(i=0; i<edit->totpoint; i++, pa++)
			pa->hair= MEM_dupallocN(pa->hair);

		undo->psys_flag = edit->psys->flag;
	}
	else {
		PTCacheMem *pm;

		BLI_duplicatelist(&undo->mem_cache, &edit->pid.cache->mem_cache);
		pm = undo->mem_cache.first;

		for(; pm; pm=pm->next) {
			for(i=0; i<BPHYS_TOT_DATA; i++)
				pm->data[i] = MEM_dupallocN(pm->data[i]);
		}
	}

	point= undo->points = MEM_dupallocN(edit->points);
	undo->totpoint = edit->totpoint;

	for(i=0; i<edit->totpoint; i++, point++) {
		point->keys= MEM_dupallocN(point->keys);
		/* no need to update edit key->co & key->time pointers here */
	}
}

static void get_PTCacheUndo(PTCacheEdit *edit, PTCacheUndo *undo)
{
	ParticleSystem *psys = edit->psys;
	ParticleData *pa;
	HairKey *hkey;
	POINT_P; KEY_K;

	LOOP_POINTS {
		if(psys && psys->particles[p].hair)
			MEM_freeN(psys->particles[p].hair);

		if(point->keys)
			MEM_freeN(point->keys);
	}
	if(psys && psys->particles)
		MEM_freeN(psys->particles);
	if(edit->points)
		MEM_freeN(edit->points);
	if(edit->mirror_cache) {
		MEM_freeN(edit->mirror_cache);
		edit->mirror_cache= NULL;
	}

	edit->points= MEM_dupallocN(undo->points);
	edit->totpoint = undo->totpoint;

	LOOP_POINTS {
		point->keys= MEM_dupallocN(point->keys);
	}

	if(psys) {
		psys->particles= MEM_dupallocN(undo->particles);

		psys->totpart= undo->totpoint;

		LOOP_POINTS {
			pa = psys->particles + p;
			hkey= pa->hair = MEM_dupallocN(pa->hair);

			LOOP_KEYS {
				key->co= hkey->co;
				key->time= &hkey->time;
				hkey++;
			}
		}

		psys->flag = undo->psys_flag;
	}
	else {
		PTCacheMem *pm;
		int i;

		BKE_ptcache_free_mem(&edit->pid.cache->mem_cache);

		BLI_duplicatelist(&edit->pid.cache->mem_cache, &undo->mem_cache);

		pm = edit->pid.cache->mem_cache.first;

		for(; pm; pm=pm->next) {
			for(i=0; i<BPHYS_TOT_DATA; i++)
				pm->data[i] = MEM_dupallocN(pm->data[i]);

			BKE_ptcache_mem_init_pointers(pm);

			LOOP_POINTS {
				LOOP_KEYS {
					if((int)key->ftime == pm->frame) {
						key->co = pm->cur[BPHYS_DATA_LOCATION];
						key->vel = pm->cur[BPHYS_DATA_VELOCITY];
						key->rot = pm->cur[BPHYS_DATA_ROTATION];
						key->time = &key->ftime;
					}
				}
				BKE_ptcache_mem_incr_pointers(pm);
			}
		}
	}
}

void PE_undo_push(Scene *scene, char *str)
{
	PTCacheEdit *edit= PE_get_current(scene, OBACT);
	PTCacheUndo *undo;
	int nr;

	if(!edit) return;

	/* remove all undos after (also when curundo==NULL) */
	while(edit->undo.last != edit->curundo) {
		undo= edit->undo.last;
		BLI_remlink(&edit->undo, undo);
		free_PTCacheUndo(undo);
		MEM_freeN(undo);
	}

	/* make new */
	edit->curundo= undo= MEM_callocN(sizeof(PTCacheUndo), "particle undo file");
	strncpy(undo->name, str, 64-1);
	BLI_addtail(&edit->undo, undo);
	
	/* and limit amount to the maximum */
	nr= 0;
	undo= edit->undo.last;
	while(undo) {
		nr++;
		if(nr==U.undosteps) break;
		undo= undo->prev;
	}
	if(undo) {
		while(edit->undo.first!=undo) {
			PTCacheUndo *first= edit->undo.first;
			BLI_remlink(&edit->undo, first);
			free_PTCacheUndo(first);
			MEM_freeN(first);
		}
	}

	/* copy  */
	make_PTCacheUndo(edit,edit->curundo);
}

void PE_undo_step(Scene *scene, int step)
{	
	PTCacheEdit *edit= PE_get_current(scene, OBACT);

	if(!edit) return;

	if(step==0) {
		get_PTCacheUndo(edit,edit->curundo);
	}
	else if(step==1) {
		
		if(edit->curundo==NULL || edit->curundo->prev==NULL);
		else {
			if(G.f & G_DEBUG) printf("undo %s\n", edit->curundo->name);
			edit->curundo= edit->curundo->prev;
			get_PTCacheUndo(edit, edit->curundo);
		}
	}
	else {
		/* curundo has to remain current situation! */
		
		if(edit->curundo==NULL || edit->curundo->next==NULL);
		else {
			get_PTCacheUndo(edit, edit->curundo->next);
			edit->curundo= edit->curundo->next;
			if(G.f & G_DEBUG) printf("redo %s\n", edit->curundo->name);
		}
	}

	PE_update_object(scene, OBACT, 0);
	DAG_id_flush_update(&OBACT->id, OB_RECALC_DATA);
}

static void PTCacheUndo_number(Scene *scene, PTCacheEdit *edit, int nr)
{
	PTCacheUndo *undo;
	int a=1;
	
	for(undo= edit->undo.first; undo; undo= undo->next, a++) {
		if(a==nr) break;
	}
	edit->curundo= undo;
	PE_undo_step(scene, 0);
}

static void PTCacheUndo_clear(PTCacheEdit *edit)
{
	PTCacheUndo *undo;

	if(edit==0) return;
	
	undo= edit->undo.first;
	while(undo) {
		free_PTCacheUndo(undo);
		undo= undo->next;
	}
	BLI_freelistN(&edit->undo);
	edit->curundo= NULL;
}

void PE_undo(Scene *scene)
{
	PE_undo_step(scene, 1);
}

void PE_redo(Scene *scene)
{
	PE_undo_step(scene, -1);
}

void PE_undo_menu(Scene *scene, Object *ob)
{
	PTCacheEdit *edit= PE_get_current(scene, ob);
	PTCacheUndo *undo;
	DynStr *ds;
	short event=0;
	char *menu;

	if(!edit) return;
	
	ds= BLI_dynstr_new();

	BLI_dynstr_append(ds, "Particlemode Undo History %t");
	
	for(undo= edit->undo.first; undo; undo= undo->next) {
		BLI_dynstr_append(ds, "|");
		BLI_dynstr_append(ds, undo->name);
	}
	
	menu= BLI_dynstr_get_cstring(ds);
	BLI_dynstr_free(ds);
	
// XXX	event= pupmenu_col(menu, 20);
	MEM_freeN(menu);
	
	if(event>0) PTCacheUndo_number(scene, edit, event);
}

/************************ utilities ******************************/

int PE_minmax(Scene *scene, float *min, float *max)
{
	Object *ob= OBACT;
	PTCacheEdit *edit= PE_get_current(scene, ob);
	ParticleSystem *psys = edit->psys;
	ParticleSystemModifierData *psmd = NULL;
	POINT_P; KEY_K;
	float co[3], mat[4][4];
	int ok= 0;

	if(!edit) return ok;
	
	if(psys)
		psmd= psys_get_modifier(ob, psys);
	else
		Mat4One(mat);

	LOOP_VISIBLE_POINTS {
		if(psys)
			psys_mat_hair_to_global(ob, psmd->dm, psys->part->from, psys->particles+p, mat);

		LOOP_SELECTED_KEYS {
			VECCOPY(co, key->co);
			Mat4MulVecfl(mat, co);
			DO_MINMAX(co, min, max);		
			ok= 1;
		}
	}

	if(!ok) {
		minmax_object(ob, min, max);
		ok= 1;
	}
  
	return ok;
}

/************************ particle edit toggle operator ************************/

/* initialize needed data for bake edit */
static void PE_create_particle_edit(Scene *scene, Object *ob, PointCache *cache, ParticleSystem *psys)
{
	PTCacheEdit *edit= (psys)? psys->edit : cache->edit;
	ParticleSystemModifierData *psmd= (psys)? psys_get_modifier(ob, psys): NULL;
	POINT_P; KEY_K;
	ParticleData *pa = NULL;
	HairKey *hkey;
	int totpoint;

	/* no psmd->dm happens in case particle system modifier is not enabled */
	if(!(psys && psmd && psmd->dm) && !cache)
		return;

	if(cache && cache->flag & PTCACHE_DISK_CACHE)
		return;

	if(!edit) {
		totpoint = psys ? psys->totpart : ((PTCacheMem*)cache->mem_cache.first)->totpoint;

		edit= MEM_callocN(sizeof(PTCacheEdit), "PE_create_particle_edit");
		edit->points=MEM_callocN(totpoint*sizeof(PTCacheEditPoint),"PTCacheEditPoints");
		edit->totpoint = totpoint;

		if(psys && !cache) {
			psys->edit= edit;
			edit->psys = psys;

			psys->free_edit= PE_free_ptcache_edit;

			edit->pathcache = NULL;
			edit->pathcachebufs.first = edit->pathcachebufs.last = NULL;

			pa = psys->particles;
			LOOP_POINTS {
				point->totkey = pa->totkey;
				point->keys= MEM_callocN(point->totkey*sizeof(PTCacheEditKey),"ParticleEditKeys");
				point->flag |= PEP_EDIT_RECALC;

				hkey = pa->hair;
				LOOP_KEYS {
					key->co= hkey->co;
					key->time= &hkey->time;
					key->flag= hkey->editflag;
					if(!(psys->flag & PSYS_GLOBAL_HAIR))
						key->flag |= PEK_USE_WCO;
					hkey++;
				}
				pa++;
			}
		}
		else {
			PTCacheMem *pm;
			int totframe=0;

			cache->edit= edit;
			cache->free_edit= PE_free_ptcache_edit;
			edit->psys = NULL;

			for(pm=cache->mem_cache.first; pm; pm=pm->next)
				totframe++;

			for(pm=cache->mem_cache.first; pm; pm=pm->next) {
				BKE_ptcache_mem_init_pointers(pm);

				LOOP_POINTS {
					if(psys) {
						pa = psys->particles + p;
						if((pm->next && pm->next->frame < pa->time)
							|| (pm->prev && pm->prev->frame >= pa->dietime)) {
								BKE_ptcache_mem_incr_pointers(pm);
								continue;
							}
					}

					if(!point->totkey) {
						key = point->keys = MEM_callocN(totframe*sizeof(PTCacheEditKey),"ParticleEditKeys");
						point->flag |= PEP_EDIT_RECALC;
					}
					else
						key = point->keys + point->totkey;

					key->co = pm->cur[BPHYS_DATA_LOCATION];
					key->vel = pm->cur[BPHYS_DATA_VELOCITY];
					key->rot = pm->cur[BPHYS_DATA_ROTATION];
					key->ftime = (float)pm->frame;
					key->time = &key->ftime;
					BKE_ptcache_mem_incr_pointers(pm);

					point->totkey++;
				}
			}
			psys = NULL;
		}

		UI_GetThemeColor3ubv(TH_EDGE_SELECT, edit->sel_col);
		UI_GetThemeColor3ubv(TH_WIRE, edit->nosel_col);

		recalc_lengths(edit);
		if(psys && !cache)
			recalc_emitter_field(ob, psys);
		PE_update_object(scene, ob, 1);

		PTCacheUndo_clear(edit);
		PE_undo_push(scene, "Original");
	}
}

static int particle_edit_toggle_poll(bContext *C)
{
	Scene *scene= CTX_data_scene(C);
	Object *ob= CTX_data_active_object(C);

	if(!scene || !ob || ob->id.lib)
		return 0;
	
	return (ob->particlesystem.first || modifiers_findByType(ob, eModifierType_Cloth) || modifiers_findByType(ob, eModifierType_Softbody));
}

static int particle_edit_toggle_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Object *ob= CTX_data_active_object(C);

	if(!(ob->mode & OB_MODE_PARTICLE_EDIT)) {
		ob->mode |= OB_MODE_PARTICLE_EDIT;
		PE_create_current(scene, ob);
		toggle_particle_cursor(C, 1);
		WM_event_add_notifier(C, NC_SCENE|ND_MODE|NS_MODE_PARTICLE, NULL);
	}
	else {
		ob->mode &= ~OB_MODE_PARTICLE_EDIT;
		toggle_particle_cursor(C, 0);
		WM_event_add_notifier(C, NC_SCENE|ND_MODE|NS_MODE_OBJECT, NULL);
	}

	DAG_id_flush_update(&ob->id, OB_RECALC_DATA);

	return OPERATOR_FINISHED;
}

void PARTICLE_OT_particle_edit_toggle(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Particle Edit Toggle";
	ot->idname= "PARTICLE_OT_particle_edit_toggle";
	
	/* api callbacks */
	ot->exec= particle_edit_toggle_exec;
	ot->poll= particle_edit_toggle_poll;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}


/************************ set editable operator ************************/

static int clear_edited_exec(bContext *C, wmOperator *op)
{
	Object *ob= CTX_data_active_object(C);
	ParticleSystem *psys = psys_get_current(ob);
	
	if(psys->edit) {
		if(psys->edit->edited || 1) { // XXX okee("Lose changes done in particle mode?")) 
			PE_free_ptcache_edit(psys->edit);

			psys->edit = NULL;
			psys->free_edit = NULL;

			psys->recalc |= PSYS_RECALC_RESET;
			psys->flag &= ~PSYS_GLOBAL_HAIR;
			psys->flag &= ~PSYS_EDITED;

			psys_reset(psys, PSYS_RESET_DEPSGRAPH);
			DAG_id_flush_update(&ob->id, OB_RECALC_DATA);
		}
	}

	return OPERATOR_FINISHED;
}

void PARTICLE_OT_edited_clear(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Clear Edited";
	ot->idname= "PARTICLE_OT_edited_clear";
	
	/* api callbacks */
	ot->exec= clear_edited_exec;
	ot->poll= particle_edit_toggle_poll;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/*********************** specials menu **************************/

static int specials_menu_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	Scene *scene= CTX_data_scene(C);
	ParticleEditSettings *pset=PE_settings(scene);
	uiPopupMenu *pup;
	uiLayout *layout;

	pup= uiPupMenuBegin(C, "Specials", 0);
	layout= uiPupMenuLayout(pup);

	uiItemO(layout, NULL, 0, "PARTICLE_OT_rekey");
	if(pset->selectmode & SCE_SELECT_POINT) {
		uiItemO(layout, NULL, 0, "PARTICLE_OT_subdivide");
		uiItemO(layout, NULL, 0, "PARTICLE_OT_select_first");
		uiItemO(layout, NULL, 0, "PARTICLE_OT_select_last");
	}
	uiItemO(layout, NULL, 0, "PARTICLE_OT_remove_doubles");

	uiPupMenuEnd(C, pup);

	return OPERATOR_CANCELLED;
}

void PARTICLE_OT_specials_menu(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Specials Menu";
	ot->idname= "PARTICLE_OT_specials_menu";
	
	/* api callbacks */
	ot->invoke= specials_menu_invoke;
	ot->poll= PE_hair_poll;
}

