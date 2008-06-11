/* editparticle.c
 *
 *
 * $Id: editparticle.c $
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

#include "BKE_bad_level_calls.h"
#include "BKE_DerivedMesh.h"
#include "BKE_depsgraph.h"

#include "BKE_global.h"
#include "BKE_object.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_particle.h"
#include "BKE_scene.h"
#include "BKE_utildefines.h" 

#include "BSE_edit.h"

#include "BLI_arithb.h"
#include "BLI_blenlib.h"
#include "BLI_dynstr.h"
#include "BLI_kdtree.h"
#include "BLI_rand.h"

#include "PIL_time.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"
#include "BIF_graphics.h"
#include "BIF_editparticle.h"
#include "BIF_editview.h"
#include "BIF_interface.h"
#include "BIF_meshtools.h"
#include "BIF_mywindow.h"
#include "BIF_radialcontrol.h"
#include "BIF_resources.h"
#include "BIF_screen.h"
#include "BIF_space.h"
#include "BIF_toolbox.h"

#include "BSE_view.h"

#include "BDR_editobject.h" //rightmouse_transform()
#include "BDR_drawobject.h"

#include "blendef.h"
#include "mydevice.h"

static void ParticleUndo_clear(ParticleSystem *psys);

#define LOOP_PARTICLES(i,pa) for(i=0, pa=psys->particles; i<totpart; i++, pa++)
#define LOOP_KEYS(k,key) if(psys->edit)for(k=0, key=psys->edit->keys[i]; k<pa->totkey; k++, key++)

void PE_free_particle_edit(ParticleSystem *psys)
{
	ParticleEdit *edit=psys->edit;
	int i, totpart=psys->totpart;

	if(edit==0) return;

	ParticleUndo_clear(psys);

	if(edit->keys){
		for(i=0; i<totpart; i++){
			if(edit->keys[i])
				MEM_freeN(edit->keys[i]);
		}
		MEM_freeN(edit->keys);
	}

	if(edit->mirror_cache)
		MEM_freeN(edit->mirror_cache);

	if(edit->emitter_cosnos){
		MEM_freeN(edit->emitter_cosnos);
		edit->emitter_cosnos=0;
	}

	if(edit->emitter_field){
		BLI_kdtree_free(edit->emitter_field);
		edit->emitter_field=0;
	}

	MEM_freeN(edit);

	psys->edit=NULL;
}
/************************************************/
/*			Edit Mode Helpers					*/
/************************************************/
int PE_can_edit(ParticleSystem *psys)
{
	return (psys && psys->edit && (G.f & G_PARTICLEEDIT));
}

ParticleEditSettings *PE_settings()
{
	return &G.scene->toolsettings->particle;
}

void PE_change_act(void *ob_v, void *act_v)
{
	Object *ob = ob_v;
	ParticleSystem *psys;
	short act = *((short*)act_v) - 1;

	if((psys=psys_get_current(ob)))
		psys->flag &= ~PSYS_CURRENT;

	if(act>=0){
		if((psys=BLI_findlink(&ob->particlesystem,act))) {
			psys->flag |= PSYS_CURRENT;

			if(psys_check_enabled(ob, psys)) {
				if(G.f & G_PARTICLEEDIT && !psys->edit)
					PE_create_particle_edit(ob, psys);
				PE_recalc_world_cos(ob, psys);
			}
		}
	}
}

/* always gets atleast the first particlesystem even if PSYS_CURRENT flag is not set */
ParticleSystem *PE_get_current(Object *ob)
{
	ParticleSystem *psys;

	if(ob==NULL)
		return NULL;

	psys= ob->particlesystem.first;
	while(psys){
		if(psys->flag & PSYS_CURRENT)
			break;
		psys=psys->next;
	}

	if(psys==NULL && ob->particlesystem.first){
		psys=ob->particlesystem.first;
		psys->flag |= PSYS_CURRENT;
	}

	if(psys && psys_check_enabled(ob, psys) && ob == OBACT && (G.f & G_PARTICLEEDIT))
		if(psys->part->type == PART_HAIR && psys->flag & PSYS_EDITED)
			if(psys->edit == NULL)
				PE_create_particle_edit(ob, psys);

	return psys;
}
/* returns -1 if no system has PSYS_CURRENT flag */
short PE_get_current_num(Object *ob)
{
	short num=0;
	ParticleSystem *psys = ob->particlesystem.first;

	while(psys){
		if(psys->flag & PSYS_CURRENT)
			return num;
		num++;
		psys=psys->next;
	}

	return -1;
}

void PE_hide_keys_time(ParticleSystem *psys, float cfra)
{
	ParticleData *pa;
	ParticleEditKey *key;
	ParticleEditSettings *pset=PE_settings();
	int i,k,totpart=psys->totpart;

	if(pset->draw_timed && G.scene->selectmode==SCE_SELECT_POINT){
		LOOP_PARTICLES(i,pa){
			LOOP_KEYS(k,key){
				if(fabs(cfra-*key->time) < pset->draw_timed)
					key->flag &= ~PEK_HIDE;
				else{
					key->flag |= PEK_HIDE;
					key->flag &= ~PEK_SELECT;
				}
			}
		}
	}
	else{
		LOOP_PARTICLES(i,pa){
			LOOP_KEYS(k,key){
				key->flag &= ~PEK_HIDE;
			}
		}
	}
}

static int key_inside_circle(short mco[2], float rad, float co[3], float *distance)
{
	float dx,dy,dist;
	short vertco[2];

	project_short(co,vertco);
	
	if (vertco[0]==IS_CLIPPED)
		return 0;
	
	dx=(float)(mco[0]-vertco[0]);
	dy=(float)(mco[1]-vertco[1]);
	dist=(float)sqrt((double)(dx*dx + dy*dy));

	if(dist<=rad){
		if(distance) *distance=dist;
		return 1;
	}
	else
		return 0;
}
static int key_inside_rect(rcti *rect, float co[3])
{
	short vertco[2];

	project_short(co,vertco);

	if (vertco[0]==IS_CLIPPED)
		return 0;
	
	if(vertco[0] > rect->xmin && vertco[0] < rect->xmax &&
			vertco[1] > rect->ymin && vertco[1] < rect->ymax)
		return 1;
	else
		return 0;
}
static int test_key_depth(float *co, bglMats *mats){
	double ux, uy, uz;
	float depth;
	short wco[3], x,y;

	if((G.vd->drawtype<=OB_WIRE) || (G.vd->flag & V3D_ZBUF_SELECT)==0) return 1;

	gluProject(co[0],co[1],co[2], mats->modelview, mats->projection,
			(GLint *)mats->viewport, &ux, &uy, &uz );

	project_short(co,wco);
	
	if (wco[0]==IS_CLIPPED)
		return 0;
	
	x=wco[0];
	y=wco[1];

	if(G.vd->depths && x<G.vd->depths->w && y<G.vd->depths->h){
		/* the 0.0001 is an experimental threshold to make selecting keys right next to a surface work better */
		if((float)uz - 0.0001 > G.vd->depths->depths[y*G.vd->depths->w+x])
			return 0;
		else
			return 1;
	}
	else{
		x+= (short)curarea->winrct.xmin;
		y+= (short)curarea->winrct.ymin;

		glReadPixels(x, y, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth);

		if((float)uz - 0.0001 > depth)
			return 0;
		else
			return 1;
	}
}

static int particle_is_selected(ParticleSystem *psys, ParticleData *pa)
{
	ParticleEditKey *key;
	int sel, i, k;

	if(pa->flag&PARS_HIDE) return 0;

	sel=0;
	i= pa - psys->particles;
	LOOP_KEYS(k,key)
		if(key->flag&PEK_SELECT)
			return 1;
	
	return 0;
}

/*-----iterators over editable particles-----*/
static void for_mouse_hit_keys(int nearest, ParticleSystem *psys, void (*func)(ParticleSystem *psys, int pa_index, int key_index, void *userData), void *userData){
	/* these are allways the first in this userData */
	struct { short *mval; float rad; rcti *rect;} *data = userData;
	ParticleData *pa;
	ParticleEditKey *key;
	bglMats mats;
	int i,k, totpart, nearest_pa=-1, nearest_key=-1;
	float dist=data->rad;

	if(psys==0 || G.scene->selectmode==SCE_SELECT_PATH) return;

	totpart=psys->totpart;

	bgl_get_mats(&mats);

	LOOP_PARTICLES(i,pa){
		if(pa->flag & PARS_HIDE) continue;

		if(G.scene->selectmode==SCE_SELECT_END){
			key=psys->edit->keys[i]+pa->totkey-1;

			if(nearest){
				if(key_inside_circle(data->mval,dist,key->world_co,&dist) && test_key_depth(key->world_co,&mats)){
					nearest_pa=i;
					nearest_key=pa->totkey-1;
				}
			}
			else if(((data->mval)?
						key_inside_circle(data->mval,data->rad,key->world_co,0):
						key_inside_rect(data->rect,key->world_co)) && test_key_depth(key->world_co,&mats))
				func(psys,i,pa->totkey-1,userData);
		}
		else{
			key=psys->edit->keys[i];

			LOOP_KEYS(k,key){
				if(key->flag&PEK_HIDE) continue;

				if(nearest){
					if(key_inside_circle(data->mval,dist,key->world_co,&dist) && test_key_depth(key->world_co,&mats)){
						nearest_pa=i;
						nearest_key=k;
					}
				}
				else if(((data->mval)?
							key_inside_circle(data->mval,data->rad,key->world_co,0):
							key_inside_rect(data->rect,key->world_co)) && test_key_depth(key->world_co,&mats))
					func(psys,i,k,userData);
			}
		}
	}
	if(nearest && nearest_pa>-1){
		func(psys,nearest_pa,nearest_key,userData);
	}
}
static void foreach_mouse_hit_element(int selected, ParticleSystem *psys,void (*func)(ParticleSystem *psys, int index, void *userData), void *userData){
	/* these are allways the first in this userData */
	struct { short *mval; float rad; rcti* rect; float dist;} *data = userData;
	ParticleData *pa;
	ParticleEditKey *key;
	bglMats mats;
	int i,k, totpart;

	if(psys==0) return;

	totpart=psys->totpart;

	bgl_get_mats(&mats);

	if(G.scene->selectmode==SCE_SELECT_PATH)
		selected=0;

	LOOP_PARTICLES(i,pa){
		if(pa->flag & PARS_HIDE) continue;

		if(G.scene->selectmode==SCE_SELECT_END){
			key=psys->edit->keys[i]+pa->totkey-1;
			if(key_inside_circle(data->mval,data->rad,key->world_co,&data->dist) && (selected==0 || key->flag&PEK_SELECT) && test_key_depth(key->world_co,&mats))
				func(psys,i,userData);
		}
		else{
			LOOP_KEYS(k,key){
				if(key->flag&PEK_HIDE) continue;

				if(key_inside_circle(data->mval,data->rad,key->world_co,&data->dist) && (selected==0 || key->flag&PEK_SELECT) && test_key_depth(key->world_co,&mats)){
					func(psys,i,userData);
					break;
				}
			}
		}
	}
}
static void foreach_mouse_hit_key(int selected, ParticleSystem *psys,void (*func)(ParticleSystem *psys, float mat[][4], float imat[][4], int bel_index, int key_index, void *userData), void *userData){
	/* these are allways the first in this userData */
	struct { Object *ob; short *mval; float rad; rcti* rect; float dist;} *data = userData;
	ParticleData *pa;
	ParticleEditKey *key;
	ParticleSystemModifierData *psmd=0;
	bglMats mats;
	int i,k, totpart;
	float mat[4][4], imat[4][4];

	if(psys==0) return;

	psmd=psys_get_modifier(data->ob,psys);

	totpart=psys->totpart;

	bgl_get_mats(&mats);

	if(G.scene->selectmode==SCE_SELECT_PATH)
		selected=0;

	Mat4One(imat);
	Mat4One(mat);

	LOOP_PARTICLES(i,pa){
		if(pa->flag & PARS_HIDE) continue;

		psys_mat_hair_to_global(data->ob, psmd->dm, psys->part->from, pa, mat);
		//psys_geometry_mat(psmd->dm,pa,tmat);
		//Mat4MulMat4(mat,tmat,data->ob->obmat);
		Mat4Invert(imat,mat);

		if(G.scene->selectmode==SCE_SELECT_END){
			key=psys->edit->keys[i]+pa->totkey-1;
			if(key_inside_circle(data->mval,data->rad,key->world_co,&data->dist) && (selected==0 || key->flag&PEK_SELECT) && test_key_depth(key->world_co,&mats))
				func(psys,mat,imat,i,pa->totkey-1,userData);
		}
		else{
			LOOP_KEYS(k,key){
				if(key->flag&PEK_HIDE) continue;

				if(key_inside_circle(data->mval,data->rad,key->world_co,&data->dist) && (selected==0 || key->flag&PEK_SELECT) && test_key_depth(key->world_co,&mats)){
					func(psys,mat,imat,i,k,userData);
				}
			}
		}
	}
}
static void foreach_selected_element(ParticleSystem *psys, void (*func)(ParticleSystem *psys, int index, void *userData), void *userData){
	ParticleData *pa;
	int i,totpart;

	if(psys==0) return;

	totpart=psys->totpart;

	LOOP_PARTICLES(i,pa)
		if(particle_is_selected(psys, pa))
			func(psys,i,userData);
}
static void foreach_selected_key(ParticleSystem *psys, void (*func)(ParticleSystem *psys, int pa_index, int key_index, void *userData), void *userData){
	ParticleData *pa;
	ParticleEditKey *key;
	int i,k,totpart;

	if(psys==0) return;

	totpart=psys->totpart;

	LOOP_PARTICLES(i,pa){
		if(pa->flag&PARS_HIDE) continue;

		key=psys->edit->keys[i];
		LOOP_KEYS(k,key){
			if(key->flag&PEK_SELECT)
				func(psys,i,k,userData);
		}
	}
}
void PE_foreach_element(ParticleSystem *psys, void (*func)(ParticleSystem *psys, int index, void *userData), void *userData)
{
	int i,totpart;

	if(psys==0) return;

	totpart=psys->totpart;

	for(i=0; i<totpart; i++)
		func(psys,i,userData);
}
static int count_selected_keys(ParticleSystem *psys)
{
	ParticleData *pa;
	ParticleEditKey *key;
	int i,k,totpart,sel=0;

	if(psys==0) return 0;

	totpart=psys->totpart;

	LOOP_PARTICLES(i,pa){
		if(pa->flag&PARS_HIDE) continue;

		key=psys->edit->keys[i];
		if(G.scene->selectmode==SCE_SELECT_POINT){
			for(k=0; k<pa->totkey; k++,key++){
				if(key->flag&PEK_SELECT)
					sel++;
			}
		}
		else if(G.scene->selectmode==SCE_SELECT_END){
			key+=pa->totkey-1;
			if(key->flag&PEK_SELECT)
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
	ParticleEdit *edit;
	ParticleData *pa;
	ParticleSystemModifierData *psmd;
	KDTree *tree;
	KDTreeNearest nearest;
	float mat[4][4], co[3];
	int i, index, totpart;

	edit= psys->edit;
	psmd= psys_get_modifier(ob, psys);
	totpart= psys->totpart;

	tree= BLI_kdtree_new(totpart);

	/* insert particles into kd tree */
	LOOP_PARTICLES(i,pa) {
		psys_mat_hair_to_orco(ob, psmd->dm, psys->part->from, pa, mat);
		VECCOPY(co, pa->hair[0].co);
		Mat4MulVecfl(mat, co);
		BLI_kdtree_insert(tree, i, co, NULL);
	}

	BLI_kdtree_balance(tree);

	/* lookup particles and set in mirror cache */
	if(!edit->mirror_cache)
		edit->mirror_cache= MEM_callocN(sizeof(int)*totpart, "PE mirror cache");
	
	LOOP_PARTICLES(i,pa) {
		psys_mat_hair_to_orco(ob, psmd->dm, psys->part->from, pa, mat);
		VECCOPY(co, pa->hair[0].co);
		Mat4MulVecfl(mat, co);
		co[0]= -co[0];

		index= BLI_kdtree_find_nearest(tree, co, NULL, &nearest);

		/* this needs a custom threshold still, duplicated for editmode mirror */
		if(index != -1 && index != i && (nearest.dist <= 0.0002f))
			edit->mirror_cache[i]= index;
		else
			edit->mirror_cache[i]= -1;
	}

	/* make sure mirrors are in two directions */
	LOOP_PARTICLES(i,pa) {
		if(edit->mirror_cache[i]) {
			index= edit->mirror_cache[i];
			if(edit->mirror_cache[index] != i)
				edit->mirror_cache[i]= -1;
		}
	}

	BLI_kdtree_free(tree);
}

static void PE_mirror_particle(Object *ob, DerivedMesh *dm, ParticleSystem *psys, ParticleData *pa, ParticleData *mpa)
{
	HairKey *hkey, *mhkey;
	ParticleEditKey *key, *mkey;
	ParticleEdit *edit;
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

	/* make sure they have the same amount of keys */
	if(pa->totkey != mpa->totkey) {
		if(mpa->hair) MEM_freeN(mpa->hair);
		if(edit->keys[mi]) MEM_freeN(edit->keys[mi]);

		mpa->hair= MEM_dupallocN(pa->hair);
		edit->keys[mi]= MEM_dupallocN(edit->keys[i]);
		mpa->totkey= pa->totkey;

		mhkey= mpa->hair;
		mkey= edit->keys[mi];
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
	key= edit->keys[i];
	mkey= edit->keys[mi];
	for(k=0; k<pa->totkey; k++, hkey++, mhkey++, key++, mkey++) {
		VECCOPY(mhkey->co, hkey->co);
		Mat4MulVecfl(mat, mhkey->co);
		mhkey->co[0]= -mhkey->co[0];
		Mat4MulVecfl(immat, mhkey->co);

		if(key->flag & PEK_TAG)
			mkey->flag |= PEK_TAG;
	}

	if(pa->flag & PARS_TAG)
		mpa->flag |= PARS_TAG;
	if(pa->flag & PARS_EDIT_RECALC)
		mpa->flag |= PARS_EDIT_RECALC;
}

static void PE_apply_mirror(Object *ob, ParticleSystem *psys)
{
	ParticleEdit *edit;
	ParticleData *pa;
	ParticleSystemModifierData *psmd;
	int i, totpart;

	edit= psys->edit;
	psmd= psys_get_modifier(ob, psys);
	totpart= psys->totpart;

	/* we delay settings the PARS_EDIT_RECALC for mirrored particles
	 * to avoid doing mirror twice */
	LOOP_PARTICLES(i,pa) {
		if(pa->flag & PARS_EDIT_RECALC) {
			PE_mirror_particle(ob, psmd->dm, psys, pa, NULL);

			if(edit->mirror_cache[i] != -1)
				psys->particles[edit->mirror_cache[i]].flag &= ~PARS_EDIT_RECALC;
		}
	}

	LOOP_PARTICLES(i,pa)
		if(pa->flag & PARS_EDIT_RECALC)
			if(edit->mirror_cache[i] != -1)
				psys->particles[edit->mirror_cache[i]].flag |= PARS_EDIT_RECALC;

	edit->totkeys= psys_count_keys(psys);
}

/************************************************/
/*			Edit Calculation					*/
/************************************************/
/* tries to stop edited particles from going through the emitter's surface */
static void PE_deflect_emitter(Object *ob, ParticleSystem *psys)
{
	ParticleEdit *edit;
	ParticleData *pa;
	ParticleEditKey *key;
	ParticleEditSettings *pset = PE_settings();
	ParticleSystemModifierData *psmd = psys_get_modifier(ob,psys);
	int i,k,totpart,index;
	float *vec, *nor, dvec[3], dot, dist_1st;
	float hairimat[4][4], hairmat[4][4];

	if(psys==0)
		return;

	if((pset->flag & PE_DEFLECT_EMITTER)==0)
		return;

	edit=psys->edit;
	totpart=psys->totpart;

	LOOP_PARTICLES(i,pa){
		if(!(pa->flag & PARS_EDIT_RECALC))
			continue;
		
		psys_mat_hair_to_object(ob, psmd->dm, psys->part->from, pa, hairmat);
		
		LOOP_KEYS(k,key){
			Mat4MulVecfl(hairmat, key->co);
		}
	//}

	//LOOP_PARTICLES(i,pa){
		key=psys->edit->keys[i]+1;

		dist_1st=VecLenf((key-1)->co,key->co);
		dist_1st*=0.75f*pset->emitterdist;

		for(k=1; k<pa->totkey; k++, key++){
			index= BLI_kdtree_find_nearest(edit->emitter_field,key->co,NULL,NULL);
			
			vec=edit->emitter_cosnos +index*6;
			nor=vec+3;

			VecSubf(dvec, key->co, vec);

			dot=Inpf(dvec,nor);
			VECCOPY(dvec,nor);

			if(dot>0.0f){
				if(dot<dist_1st){
					Normalize(dvec);
					VecMulf(dvec,dist_1st-dot);
					VecAddf(key->co,key->co,dvec);
				}
			}
			else{
				Normalize(dvec);
				VecMulf(dvec,dist_1st-dot);
				VecAddf(key->co,key->co,dvec);
			}
			if(k==1)
				dist_1st*=1.3333f;
		}
	//}

	//LOOP_PARTICLES(i,pa){
		
		Mat4Invert(hairimat,hairmat);

		LOOP_KEYS(k,key){
			Mat4MulVecfl(hairimat, key->co);
		}
	}
}
/* force set distances between neighbouring keys */
void PE_apply_lengths(ParticleSystem *psys)
{
	ParticleEdit *edit;
	ParticleData *pa;
	ParticleEditKey *key;
	ParticleEditSettings *pset=PE_settings();
	int i,k,totpart;
	float dv1[3];

	if(psys==0)
		return;

	if((pset->flag & PE_KEEP_LENGTHS)==0)
		return;

	edit=psys->edit;
	totpart=psys->totpart;

	LOOP_PARTICLES(i,pa){
		if(!(pa->flag & PARS_EDIT_RECALC))
			continue;
		
		for(k=1, key=edit->keys[i] + 1; k<pa->totkey; k++, key++){
			VecSubf(dv1, key->co, (key - 1)->co);
			Normalize(dv1);
			VecMulf(dv1, (key - 1)->length);
			VecAddf(key->co, (key - 1)->co, dv1);
		}
	}
}
/* try to find a nice solution to keep distances between neighbouring keys */
static void PE_iterate_lengths(ParticleSystem *psys)
{
	ParticleEdit *edit;
	ParticleData *pa;
	ParticleEditKey *key;
	ParticleEditSettings *pset=PE_settings();
	int i, j, k,totpart;
	float tlen;
	float dv0[3] = {0.0f, 0.0f, 0.0f};
	float dv1[3] = {0.0f, 0.0f, 0.0f};
	float dv2[3] = {0.0f, 0.0f, 0.0f};

	if(psys==0)
		return;

	if((pset->flag & PE_KEEP_LENGTHS)==0)
		return;

	edit=psys->edit;
	totpart=psys->totpart;

	LOOP_PARTICLES(i,pa){
		if(!(pa->flag & PARS_EDIT_RECALC))
			continue;

		for(j=1; j<pa->totkey; j++){
			float mul = 1.0f / (float)pa->totkey;

			if(pset->flag & PE_LOCK_FIRST){
				key = edit->keys[i] + 1;
				k = 1;
				dv1[0] = dv1[1] = dv1[2] = 0.0;
			}
			else{
				key = edit->keys[i];
				k = 0;
				dv0[0] = dv0[1] = dv0[2] = 0.0;
			}

			for(; k<pa->totkey; k++, key++){
				if(k){
					VecSubf(dv0, (key - 1)->co, key->co);
					tlen = Normalize(dv0);
					VecMulf(dv0, (mul * (tlen - (key - 1)->length)));
				}

				if(k < pa->totkey - 1){
					VecSubf(dv2, (key + 1)->co, key->co);
					tlen = Normalize(dv2);
					VecMulf(dv2, mul * (tlen - key->length));
				}

				if(k){
					VecAddf((key-1)->co,(key-1)->co,dv1);
				}

				VECADD(dv1,dv0,dv2);
			}
		}
	}
}
/* set current distances to be kept between neighbouting keys */
static void recalc_lengths(ParticleSystem *psys)
{
	ParticleData *pa;
	ParticleEditKey *key;
	int i, k, totpart;

	if(psys==0)
		return;

	totpart = psys->totpart;

	LOOP_PARTICLES(i,pa){
		key = psys->edit->keys[i];
		for(k=0; k<pa->totkey-1; k++, key++){
			key->length = VecLenf(key->co, (key + 1)->co);
		}
	}
}
/* calculate and store key locations in world coordinates */
void PE_recalc_world_cos(Object *ob, ParticleSystem *psys)
{
	ParticleSystemModifierData *psmd = psys_get_modifier(ob, psys);
	ParticleData *pa;
	ParticleEditKey *key;
	int i, k, totpart;
	float hairmat[4][4];

	if(psys==0)
		return;

	totpart = psys->totpart;

	LOOP_PARTICLES(i,pa){
		psys_mat_hair_to_global(ob, psmd->dm, psys->part->from, pa, hairmat);

		LOOP_KEYS(k,key){
			VECCOPY(key->world_co,key->co);
			Mat4MulVecfl(hairmat, key->world_co);
		}
	}
}
/* calculate a tree for finding nearest emitter's vertice */
static void recalc_emitter_field(Object *ob, ParticleSystem *psys)
{
	DerivedMesh *dm=psys_get_modifier(ob,psys)->dm;
	ParticleEdit *edit = psys->edit;
	MFace *mface;
	MVert *mvert;
	float *vec, *nor;
	int i, totface, totvert;

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
	for(i=0; i<totface; i++, vec+=6, nor+=6){
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

		if (mface->v4){
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

void PE_update_selection(Object *ob, int useflag)
{
	ParticleSystem *psys= PE_get_current(ob);
	ParticleEdit *edit= psys->edit;
	ParticleEditSettings *pset= PE_settings();
	ParticleSettings *part= psys->part;
	ParticleData *pa;
	HairKey *hkey;
	ParticleEditKey *key;
	float cfra= CFRA;
	int i, k, totpart;

	totpart= psys->totpart;

	/* flag all particles to be updated if not using flag */
	if(!useflag)
		LOOP_PARTICLES(i,pa)
			pa->flag |= PARS_EDIT_RECALC;

	/* flush edit key flag to hair key flag to preserve selection 
	 * on save */
	LOOP_PARTICLES(i,pa) {
		key = edit->keys[i];

		for(k=0, hkey=pa->hair; k<pa->totkey; k++, hkey++, key++)
			hkey->editflag= key->flag;
	}

	psys_cache_paths(ob, psys, CFRA, 1);

	if(part->childtype && (pset->flag & PE_SHOW_CHILD))
		psys_cache_child_paths(ob, psys, cfra, 1);

	/* disable update flag */
	LOOP_PARTICLES(i,pa)
		pa->flag &= ~PARS_EDIT_RECALC;
}

void PE_update_object(Object *ob, int useflag)
{
	ParticleSystem *psys= PE_get_current(ob);
	ParticleEditSettings *pset= PE_settings();
	ParticleSettings *part= psys->part;
	ParticleData *pa;
	float cfra= CFRA;
	int i, totpart= psys->totpart;

	/* flag all particles to be updated if not using flag */
	if(!useflag)
		LOOP_PARTICLES(i,pa)
			pa->flag |= PARS_EDIT_RECALC;

	/* do post process on particle edit keys */
	PE_iterate_lengths(psys);
	PE_deflect_emitter(ob,psys);
	PE_apply_lengths(psys);
	if(pset->flag & PE_X_MIRROR)
		PE_apply_mirror(ob,psys);
	PE_recalc_world_cos(ob,psys);
	PE_hide_keys_time(psys,cfra);

	/* regenerate path caches */
	psys_cache_paths(ob, psys, cfra, 1);

	if(part->childtype && (pset->flag & PE_SHOW_CHILD))
		psys_cache_child_paths(ob, psys, cfra, 1);

	/* disable update flag */
	LOOP_PARTICLES(i,pa)
		pa->flag &= ~PARS_EDIT_RECALC;
}

/* initialize needed data for bake edit */
void PE_create_particle_edit(Object *ob, ParticleSystem *psys)
{
	ParticleEdit *edit=psys->edit;
	ParticleData *pa;
	ParticleEditKey *key;
	HairKey *hkey;
	int i,k, totpart=psys->totpart, alloc=1;

	if((psys->flag & PSYS_EDITED)==0)
		return;

	if(edit){
		int newtotkeys = psys_count_keys(psys);
		if(newtotkeys == edit->totkeys)
			alloc=0;
	}

	if(alloc){
		if(edit){
			error("ParticleEdit exists allready! Poke jahka!");
			PE_free_particle_edit(psys);
		}

		edit=psys->edit=MEM_callocN(sizeof(ParticleEdit), "PE_create_particle_edit");

		edit->keys=MEM_callocN(totpart*sizeof(ParticleEditKey*),"ParticleEditKey array");

		LOOP_PARTICLES(i,pa){
			key = edit->keys[i] = MEM_callocN(pa->totkey*sizeof(ParticleEditKey),"ParticleEditKeys");
			for(k=0, hkey=pa->hair; k<pa->totkey; k++, hkey++, key++){
				key->co = hkey->co;
				key->time = &hkey->time;
				key->flag= hkey->editflag;
			}
		}

		edit->totkeys = psys_count_keys(psys);
	}

	recalc_lengths(psys);
	recalc_emitter_field(ob, psys);
	PE_recalc_world_cos(ob, psys);

	if(alloc) {
		ParticleUndo_clear(psys);
		PE_undo_push("Original");
	}
}

/* toggle particle mode on & off */
void PE_set_particle_edit(void)
{
	Object *ob= OBACT;
	ParticleSystem *psys = PE_get_current(ob);

	scrarea_queue_headredraw(curarea);
	
	//if(!ob || ob->id.lib) return; /* is the id.lib test needed? -jahka*/
	if(ob==0 || psys==0) return;
	
	if(psys==0){
		if(ob->particlesystem.first){
			psys=ob->particlesystem.first;
			psys->flag |= PSYS_CURRENT;
		}
		else
			return;
	}

	if((G.f & G_PARTICLEEDIT)==0){
		if(psys && psys->part->type == PART_HAIR && psys->flag & PSYS_EDITED) {
			if(psys_check_enabled(ob, psys)) {
				if(psys->edit==0)
					PE_create_particle_edit(ob, psys);
				PE_recalc_world_cos(ob, psys);
			}
		}

		G.f |= G_PARTICLEEDIT;
	}
	else{
		G.f &= ~G_PARTICLEEDIT;
	}

	DAG_object_flush_update(G.scene, OBACT, OB_RECALC_DATA);

	allqueue(REDRAWVIEW3D, 1);	/* including header */
	allqueue(REDRAWBUTSOBJECT, 0);
}
/************************************************/
/*			Edit Selections						*/
/************************************************/
/*-----selection callbacks-----*/
static void select_key(ParticleSystem *psys, int pa_index, int key_index, void *userData)
{
	struct { short *mval; float rad; rcti* rect; int select; } *data = userData;
	ParticleData *pa = psys->particles + pa_index;
	ParticleEditKey *key = psys->edit->keys[pa_index] + key_index;

	if(data->select)
		key->flag|=PEK_SELECT;
	else
		key->flag&=~PEK_SELECT;

	pa->flag |= PARS_EDIT_RECALC;
}
static void select_keys(ParticleSystem *psys, int pa_index, int key_index, void *userData)
{
	struct { short *mval; float rad; rcti* rect; int select; } *data = userData;
	ParticleData *pa = psys->particles + pa_index;
	ParticleEditKey *key = psys->edit->keys[pa_index];
	int k;

	for(k=0; k<pa->totkey; k++,key++){
		if(data->select)
			key->flag|=PEK_SELECT;
		else
			key->flag&=~PEK_SELECT;
	}

	pa->flag |= PARS_EDIT_RECALC;
}
static void toggle_key_select(ParticleSystem *psys, int pa_index, int key_index, void *userData)
{
	ParticleData *pa = psys->particles + pa_index;

	if(psys->edit->keys[pa_index][key_index].flag&PEK_SELECT)
		psys->edit->keys[pa_index][key_index].flag&=~PEK_SELECT;
	else
		psys->edit->keys[pa_index][key_index].flag|=PEK_SELECT;
	
	pa->flag |= PARS_EDIT_RECALC;
}
static void select_root(ParticleSystem *psys, int index, void *userData)
{
	psys->edit->keys[index]->flag |= PEK_SELECT;
}

static void select_tip(ParticleSystem *psys, int index, void *userData)
{
	ParticleData *pa = psys->particles + index;
	ParticleEditKey *key = psys->edit->keys[index] + pa->totkey-1;

	key->flag |= PEK_SELECT;
}
static void select_more_keys(ParticleSystem *psys, int index, void *userData)
{
	ParticleEdit *edit = psys->edit;
	ParticleData *pa = psys->particles+index;
	ParticleEditKey *key;
	int k;

	for(k=0,key=edit->keys[index]; k<pa->totkey; k++,key++){
		if(key->flag&PEK_SELECT) continue;

		if(k==0){
			if((key+1)->flag&PEK_SELECT)
				key->flag |= PEK_TO_SELECT;
		}
		else if(k==pa->totkey-1){
			if((key-1)->flag&PEK_SELECT)
				key->flag |= PEK_TO_SELECT;
		}
		else{
			if(((key-1)->flag | (key+1)->flag) & PEK_SELECT)
				key->flag |= PEK_TO_SELECT;
		}
	}

	for(k=0,key=edit->keys[index]; k<pa->totkey; k++,key++){
		if(key->flag&PEK_TO_SELECT){
			key->flag &= ~PEK_TO_SELECT;
			key->flag |= PEK_SELECT;
		}
	}
}

static void select_less_keys(ParticleSystem *psys, int index, void *userData)
{
	ParticleEdit *edit = psys->edit;
	ParticleData *pa = psys->particles+index;
	ParticleEditKey *key;
	int k;

	for(k=0,key=edit->keys[index]; k<pa->totkey; k++,key++){
		if((key->flag&PEK_SELECT)==0) continue;

		if(k==0){
			if(((key+1)->flag&PEK_SELECT)==0)
				key->flag |= PEK_TO_SELECT;
		}
		else if(k==pa->totkey-1){
			if(((key-1)->flag&PEK_SELECT)==0)
				key->flag |= PEK_TO_SELECT;
		}
		else{
			if((((key-1)->flag & (key+1)->flag) & PEK_SELECT)==0)
				key->flag |= PEK_TO_SELECT;
		}
	}

	for(k=0,key=edit->keys[index]; k<pa->totkey; k++,key++){
		if(key->flag&PEK_TO_SELECT)
			key->flag &= ~(PEK_TO_SELECT|PEK_SELECT);
	}
}

/*-----using above callbacks-----*/
void PE_deselectall(void)
{
	Object *ob = OBACT;
	ParticleSystem *psys = PE_get_current(ob);
	ParticleEdit *edit = 0;
	ParticleData *pa;
	ParticleEditKey *key;
	int i,k,totpart, sel = 0;
		
	if(!PE_can_edit(psys)) return;
	
	edit = psys->edit;

	totpart = psys->totpart;
	
	LOOP_PARTICLES(i,pa){
		if(pa->flag & PARS_HIDE) continue;
		LOOP_KEYS(k,key){
			if(key->flag&PEK_SELECT){
				sel = 1;
				key->flag &= ~PEK_SELECT;
				pa->flag |= PARS_EDIT_RECALC;
			}
		}
	}

	if(sel==0){
		LOOP_PARTICLES(i,pa){
			if(pa->flag & PARS_HIDE) continue;
			LOOP_KEYS(k,key){
				if(!(key->flag & PEK_SELECT)) {
					key->flag |= PEK_SELECT;
					pa->flag |= PARS_EDIT_RECALC;
				}
			}
		}
	}

	PE_update_selection(ob, 1);

	BIF_undo_push("(De)select all keys");
	allqueue(REDRAWVIEW3D, 1);
}
void PE_mouse_particles(void)
{
	struct { short *mval; float rad; rcti* rect; int select; } data;
	Object *ob = OBACT;
	ParticleSystem *psys = PE_get_current(ob);
	ParticleEdit *edit = 0;
	ParticleData *pa;
	ParticleEditKey *key;
	short mval[2];
	int i,k,totpart;

	if(!PE_can_edit(psys)) return;

	edit = psys->edit;

	totpart = psys->totpart;

	bglFlush();
	glReadBuffer(GL_BACK);
	glDrawBuffer(GL_BACK);
	persp(PERSP_VIEW);

	if(G.qual != LR_SHIFTKEY)
		LOOP_PARTICLES(i,pa){
			if(pa->flag & PARS_HIDE) continue;
			LOOP_KEYS(k,key){
				if(key->flag & PEK_SELECT) {
					key->flag &= ~PEK_SELECT;
					pa->flag |= PARS_EDIT_RECALC;
				}
			}
		}

	getmouseco_areawin(mval);

	data.mval=mval;
	data.rad=75.0f;
	data.rect=0;
	data.select=0;

	for_mouse_hit_keys(1,psys,toggle_key_select,&data);

	PE_update_selection(ob, 1);

	rightmouse_transform();

	allqueue(REDRAWVIEW3D, 1);
}
void PE_select_root()
{
	Object *ob=OBACT;
	ParticleSystem *psys = PE_get_current(ob);

	if(!PE_can_edit(psys)) return;

	PE_foreach_element(psys,select_root,NULL);
	BIF_undo_push("Select first");
}
void PE_select_tip()
{
	Object *ob=OBACT;
	ParticleSystem *psys = PE_get_current(ob);

	if(!PE_can_edit(psys)) return;

	PE_foreach_element(psys,select_tip,NULL);
	BIF_undo_push("Select last");
}
void PE_select_linked(void)
{
	struct { short *mval; float rad; rcti* rect; int select; } data;
	Object *ob = OBACT;
	ParticleSystem *psys = PE_get_current(ob);
	short mval[2];

	if(!PE_can_edit(psys)) return;

	getmouseco_areawin(mval);

	data.mval=mval;
	data.rad=75.0f;
	data.rect=0;
	data.select=(G.qual != LR_SHIFTKEY);

	for_mouse_hit_keys(1,psys,select_keys,&data);

	PE_update_selection(ob, 1);

	BIF_undo_push("Select linked keys");

	allqueue(REDRAWVIEW3D, 1);
	return;
}
void PE_borderselect(void)
{
	struct { short *mval; float rad; rcti* rect; int select; } data;
	Object *ob = OBACT;
	ParticleSystem *psys = PE_get_current(ob);
	rcti rect;
	int val;

	if(!PE_can_edit(psys)) return;

	setlinestyle(2);
	val= get_border(&rect, 3);
	setlinestyle(0);
	
	if(val==0) return;

	data.mval=0;
	data.rect=&rect;
	data.select=(val==LEFTMOUSE);

	for_mouse_hit_keys(0,psys,select_key,&data);

	PE_update_selection(ob, 1);

	BIF_undo_push("Select keys");

	allqueue(REDRAWVIEW3D, 1);
	return;
}
void PE_selectionCB(short selecting, Object *editobj, short *mval, float rad)
{
	struct { short *mval; float rad; rcti* rect; int select; } data;
	ParticleSystem *psys = PE_get_current(OBACT);

	if(!PE_can_edit(psys)) return;

	data.mval=mval;
	data.rad=rad;
	data.rect=0;
	data.select=(selecting==LEFTMOUSE);

	for_mouse_hit_keys(0,psys,select_key,&data);

	draw_sel_circle(0, 0, 0, 0, 0);	/* signal */
	force_draw(0);
}
void PE_do_lasso_select(short mcords[][2], short moves, short select)
{
	Object *ob = OBACT;
	ParticleSystem *psys = PE_get_current(ob);
	ParticleSystemModifierData *psmd;
	ParticleEdit *edit;
	ParticleData *pa;
	ParticleEditKey *key;
	float co[3], mat[4][4];
	short vertco[2];
	int i, k, totpart;

	if(!PE_can_edit(psys)) return;

	psmd= psys_get_modifier(ob, psys);
	edit=psys->edit;
	totpart=psys->totpart;

	LOOP_PARTICLES(i,pa){
		if(pa->flag & PARS_HIDE) continue;

		psys_mat_hair_to_global(ob, psmd->dm, psys->part->from, pa, mat);

		if(G.scene->selectmode==SCE_SELECT_POINT){
			LOOP_KEYS(k,key){
				VECCOPY(co, key->co);
				Mat4MulVecfl(mat, co);
				project_short(co,vertco);
				if((vertco[0] != IS_CLIPPED) && lasso_inside(mcords,moves,vertco[0],vertco[1])){
					if(select && !(key->flag & PEK_SELECT)) {
						key->flag|=PEK_SELECT;
						pa->flag |= PARS_EDIT_RECALC;
					}
					else if(key->flag & PEK_SELECT) {
						key->flag&=~PEK_SELECT;
						pa->flag |= PARS_EDIT_RECALC;
					}
				}
			}
		}
		else if(G.scene->selectmode==SCE_SELECT_END){
			key = edit->keys[i] + pa->totkey - 1;

			VECCOPY(co, key->co);
			Mat4MulVecfl(mat, co);
			project_short(co,vertco);
			if((vertco[0] != IS_CLIPPED) && lasso_inside(mcords,moves,vertco[0],vertco[1])){
				if(select && !(key->flag & PEK_SELECT)) {
					key->flag|=PEK_SELECT;
					pa->flag |= PARS_EDIT_RECALC;
				}
				else if(key->flag & PEK_SELECT) {
					key->flag&=~PEK_SELECT;
					pa->flag |= PARS_EDIT_RECALC;
				}
			}
		}
	}

	PE_update_selection(ob, 1);

	BIF_undo_push("Lasso select particles");

	allqueue(REDRAWVIEW3D, 1);
}
void PE_hide(int mode)
{
	Object *ob = OBACT;
	ParticleSystem *psys = PE_get_current(ob);
	ParticleEdit *edit;
	ParticleEditKey *key;
	ParticleData *pa;
	int i, k, totpart;

	if(!PE_can_edit(psys)) return;

	edit = psys->edit;
	totpart = psys->totpart;
	
	if(mode == 0){ /* reveal all particles */
		LOOP_PARTICLES(i, pa){
			if(pa->flag & PARS_HIDE) {
				pa->flag &= ~PARS_HIDE;
				pa->flag |= PARS_EDIT_RECALC;

				LOOP_KEYS(k, key)
					key->flag |= PEK_SELECT;
			}
		}
	}
	else if(mode == 1){ /* hide unselected particles */
		LOOP_PARTICLES(i, pa) {
			if(!particle_is_selected(psys, pa)) {
				pa->flag |= PARS_HIDE;
				pa->flag |= PARS_EDIT_RECALC;

				LOOP_KEYS(k, key)
					key->flag &= ~PEK_SELECT;
			}
		}
	}
	else{ /* hide selected particles */
		LOOP_PARTICLES(i, pa) {
			if(particle_is_selected(psys, pa)) {
				pa->flag |= PARS_HIDE;
				pa->flag |= PARS_EDIT_RECALC;

				LOOP_KEYS(k, key)
					key->flag &= ~PEK_SELECT;
			}
		}
	}

	PE_update_selection(ob, 1);
	BIF_undo_push("(Un)hide elements");
	
	allqueue(REDRAWVIEW3D, 1);
}
void PE_select_less(void)
{
	ParticleSystem *psys = PE_get_current(OBACT);

	if(!PE_can_edit(psys)) return;

	PE_foreach_element(psys,select_less_keys,NULL);
	
	BIF_undo_push("Select less");
	allqueue(REDRAWVIEW3D, 1);
}
void PE_select_more(void)
{
	ParticleSystem *psys = PE_get_current(OBACT);

	if(!PE_can_edit(psys)) return;

	PE_foreach_element(psys,select_more_keys,NULL);
	
	BIF_undo_push("Select more");
	allqueue(REDRAWVIEW3D, 1);
}
/************************************************/
/*			Edit Rekey							*/
/************************************************/
static void rekey_element(ParticleSystem *psys, int index, void *userData)
{
	struct { Object *ob; float dval; } *data = userData;
	ParticleData *pa = psys->particles + index;
	ParticleEdit *edit = psys->edit;
	ParticleEditSettings *pset = PE_settings();
	ParticleKey state;
	HairKey *key, *new_keys;
	ParticleEditKey *ekey;
	float dval, sta, end;
	int k;

	pa->flag |= PARS_REKEY;

	key = new_keys = MEM_callocN(pset->totrekey * sizeof(HairKey),"Hair re-key keys");

	/* root and tip stay the same */
	VECCOPY(key->co, pa->hair->co);
	VECCOPY((key + pset->totrekey - 1)->co, (pa->hair + pa->totkey - 1)->co);

	sta = key->time = pa->hair->time;
	end = (key + pset->totrekey - 1)->time = (pa->hair + pa->totkey - 1)->time;
	dval = (end - sta) / (float)(pset->totrekey - 1);

	/* interpolate new keys from old ones */
	for(k=1,key++; k<pset->totrekey-1; k++,key++) {
		state.time = (float)k / (float)(pset->totrekey-1);
		psys_get_particle_on_path(data->ob, psys, index, &state, 0);
		VECCOPY(key->co, state.co);
		key->time = sta + k * dval;
	}

	/* replace keys */
	if(pa->hair)
		MEM_freeN(pa->hair);
	pa->hair = new_keys;

	pa->totkey=pset->totrekey;

	if(edit->keys[index])
		MEM_freeN(edit->keys[index]);
	ekey = edit->keys[index] = MEM_callocN(pa->totkey * sizeof(ParticleEditKey),"Hair re-key edit keys");
		
	for(k=0, key=pa->hair; k<pa->totkey; k++, key++, ekey++) {
		ekey->co = key->co;
		ekey->time = &key->time;
	}

	pa->flag &= ~PARS_REKEY;
	pa->flag |= PARS_EDIT_RECALC;
}
void PE_rekey(void)
{
	Object *ob=OBACT;
	ParticleSystem *psys = PE_get_current(ob);
	ParticleEditSettings *pset = PE_settings();
	struct { Object *ob; float dval; } data;

	if(!PE_can_edit(psys)) return;

	data.ob = ob;
	data.dval = 1.0f / (float)(pset->totrekey-1);

	foreach_selected_element(psys,rekey_element,&data);
	
	psys->edit->totkeys = psys_count_keys(psys);

	recalc_lengths(psys);
	
	PE_update_object(ob, 1);

	BIF_undo_push("Re-key particles");
}
static void rekey_element_to_time(int index, float path_time)
{
	Object *ob = OBACT;
	ParticleSystem *psys = PE_get_current(ob);
	ParticleEdit *edit=0;
	ParticleData *pa;
	ParticleKey state;
	HairKey *new_keys, *key;
	ParticleEditKey *ekey;
	int k;

	if(psys==0) return;

	edit = psys->edit;

	pa = psys->particles + index;

	pa->flag |= PARS_REKEY;

	key = new_keys = MEM_dupallocN(pa->hair);
	
	/* interpolate new keys from old ones (roots stay the same) */
	for(k=1, key++; k < pa->totkey; k++, key++) {
		state.time = path_time * (float)k / (float)(pa->totkey-1);
		psys_get_particle_on_path(ob, psys, index, &state, 0);
		VECCOPY(key->co, state.co);
	}

	/* replace hair keys */
	if(pa->hair)
		MEM_freeN(pa->hair);
	pa->hair = new_keys;

	/* update edit pointers */
	for(k=0, key=pa->hair, ekey=edit->keys[index]; k<pa->totkey; k++, key++, ekey++) {
		ekey->co = key->co;
		ekey->time = &key->time;
	}

	pa->flag &= ~PARS_REKEY;
}
static int remove_tagged_elements(Object *ob, ParticleSystem *psys)
{
	ParticleEdit *edit = psys->edit;
	ParticleEditSettings *pset = PE_settings();
	ParticleData *pa, *npa=0, *new_pars=0;
	ParticleEditKey **key, **nkey=0, **new_keys=0;
	ParticleSystemModifierData *psmd;
	int i, totpart, new_totpart = psys->totpart, removed = 0;

	if(pset->flag & PE_X_MIRROR) {
		/* mirror tags */
		psmd = psys_get_modifier(ob, psys);
		totpart = psys->totpart;

		LOOP_PARTICLES(i,pa)
			if(pa->flag & PARS_TAG)
				PE_mirror_particle(ob, psmd->dm, psys, pa, NULL);
	}

	for(i=0, pa=psys->particles; i<psys->totpart; i++, pa++) {
		if(pa->flag & PARS_TAG) {
			new_totpart--;
			removed++;
		}
	}

	if(new_totpart != psys->totpart) {
		if(new_totpart) {
			npa = new_pars = MEM_callocN(new_totpart * sizeof(ParticleData), "ParticleData array");
			nkey = new_keys = MEM_callocN(new_totpart * sizeof(ParticleEditKey *), "ParticleEditKey array");
		}

		pa = psys->particles;
		key = edit->keys;
		for(i=0; i<psys->totpart; i++, pa++, key++) {
			if(pa->flag & PARS_TAG) {
				if(*key)
					MEM_freeN(*key);
				if(pa->hair)
					MEM_freeN(pa->hair);
			}
			else {
				memcpy(npa, pa, sizeof(ParticleData));
				memcpy(nkey, key, sizeof(ParticleEditKey*));
				npa++;
				nkey++;
			}
		}

		if(psys->particles) MEM_freeN(psys->particles);
		psys->particles = new_pars;

		if(edit->keys) MEM_freeN(edit->keys);
		edit->keys = new_keys;

		if(edit->mirror_cache) {
			MEM_freeN(edit->mirror_cache);
			edit->mirror_cache = NULL;
		}

		psys->totpart = new_totpart;

		edit->totkeys = psys_count_keys(psys);
	}

	return removed;
}
static void remove_tagged_keys(Object *ob, ParticleSystem *psys)
{
	ParticleEdit *edit = psys->edit;
	ParticleEditSettings *pset = PE_settings();
	ParticleData *pa;
	HairKey *key, *nkey, *new_keys=0;
	ParticleEditKey *ekey;
	ParticleSystemModifierData *psmd;
	int i, k, totpart = psys->totpart;
	short new_totkey;

	if(pset->flag & PE_X_MIRROR) {
		/* mirror key tags */
		psmd = psys_get_modifier(ob, psys);

		LOOP_PARTICLES(i,pa) {
			LOOP_KEYS(k,ekey) {
				if(ekey->flag & PEK_TAG) {
					PE_mirror_particle(ob, psmd->dm, psys, pa, NULL);
					break;
				}
			}
		}
	}

	LOOP_PARTICLES(i,pa) {
		new_totkey = pa->totkey;
		LOOP_KEYS(k,ekey) {
			if(ekey->flag & PEK_TAG)
				new_totkey--;
		}
		/* we can't have elements with less than two keys*/
		if(new_totkey < 2)
			pa->flag |= PARS_TAG;
	}
	remove_tagged_elements(ob, psys);

	totpart = psys->totpart;

	LOOP_PARTICLES(i,pa) {
		new_totkey = pa->totkey;
		LOOP_KEYS(k,ekey) {
			if(ekey->flag & PEK_TAG)
				new_totkey--;
		}
		if(new_totkey != pa->totkey) {
			key = pa->hair;
			nkey = new_keys = MEM_callocN(new_totkey*sizeof(HairKey), "HairKeys");

			for(k=0, ekey=edit->keys[i]; k<new_totkey; k++, key++, nkey++, ekey++) {
				while(ekey->flag & PEK_TAG && key < pa->hair + pa->totkey) {
					key++;
					ekey++;
				}

				if(key < pa->hair + pa->totkey) {
					VECCOPY(nkey->co, key->co);
					nkey->time = key->time;
					nkey->weight = key->weight;
				}
			}
			if(pa->hair)
				MEM_freeN(pa->hair);
			
			pa->hair = new_keys;

			pa->totkey=new_totkey;

			if(edit->keys[i])
				MEM_freeN(edit->keys[i]);
			ekey = edit->keys[i] = MEM_callocN(new_totkey*sizeof(ParticleEditKey), "particle edit keys");

			for(k=0, key=pa->hair; k<pa->totkey; k++, key++, ekey++) {
				ekey->co = key->co;
				ekey->time = &key->time;
			}
		}
	}

	edit->totkeys = psys_count_keys(psys);
}
/* works like normal edit mode subdivide, inserts keys between neighbouring selected keys */
static void subdivide_element(ParticleSystem *psys, int index, void *userData)
{
	struct { Object *ob; } *data = userData;
	ParticleEdit *edit = psys->edit;
	ParticleData *pa = psys->particles + index;
	
	ParticleKey state;
	HairKey *key, *nkey, *new_keys;
	ParticleEditKey *ekey, *nekey, *new_ekeys;

	int k;
	short totnewkey=0;
	float endtime;

	for(k=0, ekey=edit->keys[index]; k<pa->totkey-1; k++,ekey++){
		if(ekey->flag&PEK_SELECT && (ekey+1)->flag&PEK_SELECT)
			totnewkey++;
	}

	if(totnewkey==0) return;

	pa->flag |= PARS_REKEY;

	nkey = new_keys = MEM_callocN((pa->totkey+totnewkey)*(sizeof(HairKey)),"Hair subdivide keys");
	nekey = new_ekeys = MEM_callocN((pa->totkey+totnewkey)*(sizeof(ParticleEditKey)),"Hair subdivide edit keys");
	endtime = pa->hair[pa->totkey-1].time;

	for(k=0, key=pa->hair, ekey=edit->keys[index]; k<pa->totkey-1; k++, key++, ekey++){

		memcpy(nkey,key,sizeof(HairKey));
		memcpy(nekey,ekey,sizeof(ParticleEditKey));

		nekey->co = nkey->co;
		nekey->time = &nkey->time;

		nkey++;
		nekey++;

		if(ekey->flag & PEK_SELECT && (ekey+1)->flag & PEK_SELECT){
			nkey->time= (key->time + (key+1)->time)*0.5f;
			state.time = (endtime != 0.0f)? nkey->time/endtime: 0.0f;
			psys_get_particle_on_path(data->ob, psys, index, &state, 0);
			VECCOPY(nkey->co, state.co);

			nekey->co= nkey->co;
			nekey->time= &nkey->time;
			nekey->flag |= PEK_SELECT;

			nekey++;
			nkey++;
		}
	}
	/*tip still not copied*/
	memcpy(nkey,key,sizeof(HairKey));
	memcpy(nekey,ekey,sizeof(ParticleEditKey));

	nekey->co = nkey->co;
	nekey->time = &nkey->time;

	if(pa->hair)
		MEM_freeN(pa->hair);
	pa->hair = new_keys;

	if(edit->keys[index])
		MEM_freeN(edit->keys[index]);

	edit->keys[index] = new_ekeys;

	pa->totkey += totnewkey;
	pa->flag |= PARS_EDIT_RECALC;
	pa->flag &= ~PARS_REKEY;
}
void PE_subdivide(void)
{
	Object *ob = OBACT;
	ParticleSystem *psys = PE_get_current(ob);
	struct { Object *ob; } data;

	if(!PE_can_edit(psys)) return;

	data.ob= ob;
	PE_foreach_element(psys,subdivide_element,&data);
	
	psys->edit->totkeys = psys_count_keys(psys);
	
	recalc_lengths(psys);
	PE_recalc_world_cos(ob, psys);

	PE_update_object(ob, 1);
	
	BIF_undo_push("Subdivide hair(s)");
}
void PE_remove_doubles(void)
{
	Object *ob=OBACT;
	ParticleSystem *psys=PE_get_current(ob);
	ParticleEditSettings *pset=PE_settings();
	ParticleData *pa;
	ParticleEdit *edit;
	ParticleSystemModifierData *psmd;
	KDTree *tree;
	KDTreeNearest nearest[10];
	float mat[4][4], co[3];
	int i, n, totn, removed, totpart, flag, totremoved;

	if(!PE_can_edit(psys)) return;

	edit= psys->edit;
	psmd= psys_get_modifier(ob, psys);
	totremoved= 0;

	do {
		removed= 0;

		totpart= psys->totpart;
		tree=BLI_kdtree_new(totpart);
			
		/* insert particles into kd tree */
		LOOP_PARTICLES(i,pa) {
			if(particle_is_selected(psys, pa)) {
				psys_mat_hair_to_object(ob, psmd->dm, psys->part->from, pa, mat);
				VECCOPY(co, pa->hair[0].co);
				Mat4MulVecfl(mat, co);
				BLI_kdtree_insert(tree, i, co, NULL);
			}
		}

		BLI_kdtree_balance(tree);

		/* tag particles to be removed */
		LOOP_PARTICLES(i,pa) {
			if(particle_is_selected(psys, pa)) {
				psys_mat_hair_to_object(ob, psmd->dm, psys->part->from, pa, mat);
				VECCOPY(co, pa->hair[0].co);
				Mat4MulVecfl(mat, co);

				totn= BLI_kdtree_find_n_nearest(tree,10,co,NULL,nearest);

				for(n=0; n<totn; n++) {
					/* this needs a custom threshold still */
					if(nearest[n].index > i && nearest[n].dist < 0.0002f) {
						if(!(pa->flag & PARS_TAG)) {
							pa->flag |= PARS_TAG;
							removed++;
						}
					}
				}
			}
		}

		BLI_kdtree_free(tree);

		/* remove tagged particles - don't do mirror here! */
		flag= pset->flag;
		pset->flag &= ~PE_X_MIRROR;
		remove_tagged_elements(ob, psys);
		pset->flag= flag;
		totremoved += removed;
	} while(removed);

	if(totremoved)
		notice("Removed: %d", totremoved);

	PE_recalc_world_cos(ob, psys);
	DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
	allqueue(REDRAWVIEW3D, 1);
	BIF_undo_push("Remove double particles");
}

static void PE_radialcontrol_callback(const int mode, const int val)
{
	ParticleEditSettings *pset = PE_settings();

	if(pset->brushtype>=0) {
		ParticleBrushData *brush= &pset->brush[pset->brushtype];

		if(mode == RADIALCONTROL_SIZE)
			brush->size = val;
		else if(mode == RADIALCONTROL_STRENGTH)
			brush->strength = val;
	}

	(*PE_radialcontrol()) = NULL;
}

RadialControl **PE_radialcontrol()
{
	static RadialControl *rc = NULL;
	return &rc;
}

void PE_radialcontrol_start(const int mode)
{
	ParticleEditSettings *pset = PE_settings();
	int orig= 1;

	if(pset->brushtype>=0) {
		ParticleBrushData *brush= &pset->brush[pset->brushtype];
		
		if(mode == RADIALCONTROL_SIZE)
			orig = brush->size;
		else if(mode == RADIALCONTROL_STRENGTH)
			orig = brush->strength;
		
		if(mode != RADIALCONTROL_NONE)
			(*PE_radialcontrol())= radialcontrol_start(mode, PE_radialcontrol_callback, orig, 100, 0);
	}
}

/************************************************/
/*			Edit Brushes						*/
/************************************************/
static void brush_comb(ParticleSystem *psys, float mat[][4], float imat[][4], int pa_index, int key_index, void *userData)
{
	struct {Object *ob; short *mval; float rad; rcti* rect; float dist; float *dvec; float combfac;} *data = userData;
	ParticleData *pa= &psys->particles[pa_index];
	ParticleEditSettings *pset= PE_settings();
	HairKey *key = pa->hair + key_index;
	float cvec[3], fac;

	if(pset->flag & PE_LOCK_FIRST && key_index == 0) return;

	fac = (float)pow((double)(1.0f - data->dist / data->rad), (double)data->combfac);

	VECCOPY(cvec,data->dvec);
	Mat4Mul3Vecfl(imat,cvec);
	VecMulf(cvec, fac);
	VECADD(key->co, key->co, cvec);

	pa->flag |= PARS_EDIT_RECALC;
}
static void brush_cut(ParticleSystem *psys, int index, void *userData)
{
	struct { short *mval; float rad; rcti* rect; int selected; float cutfac; bglMats mats;} *data = userData;
	ParticleData *pa= &psys->particles[index];
	ParticleCacheKey *key = psys->pathcache[index];
	float rad2, cut_time = 1.0;
	float x0, x1, v0, v1, o0, o1, xo0, xo1, d, dv;
	int k, cut, keys = (int)pow(2.0, (double)psys->part->draw_step);
	short vertco[2];

	/* blunt scissors */
	if(BLI_frand() > data->cutfac) return;

	rad2 = data->rad * data->rad;

	cut=0;

	project_short_noclip(key->co, vertco);
	x0 = (float)vertco[0];
	x1 = (float)vertco[1];

	o0 = (float)data->mval[0];
	o1 = (float)data->mval[1];
	
	xo0 = x0 - o0;
	xo1 = x1 - o1;

	/* check if root is inside circle */
	if(xo0*xo0 + xo1*xo1 < rad2 && test_key_depth(key->co,&(data->mats))) {
		cut_time = -1.0f;
		cut = 1;
	}
	else {
		/* calculate path time closest to root that was inside the circle */
		for(k=1, key++; k<=keys; k++, key++){
			project_short_noclip(key->co, vertco);

			if(test_key_depth(key->co,&(data->mats)) == 0) {
				x0 = (float)vertco[0];
				x1 = (float)vertco[1];

				xo0 = x0 - o0;
				xo1 = x1 - o1;
				continue;
			}

			v0 = (float)vertco[0] - x0;
			v1 = (float)vertco[1] - x1;

			dv = v0*v0 + v1*v1;

			d = (v0*xo1 - v1*xo0);
			
			d = dv * rad2 - d*d;

			if(d > 0.0f) {
				d = sqrt(d);

				cut_time = -(v0*xo0 + v1*xo1 + d);

				if(cut_time > 0.0f) {
					cut_time /= dv;

					if(cut_time < 1.0f) {
						cut_time += (float)(k-1);
						cut_time /= (float)keys;
						cut = 1;
						break;
					}
				}
			}

			x0 = (float)vertco[0];
			x1 = (float)vertco[1];

			xo0 = x0 - o0;
			xo1 = x1 - o1;
		}
	}

	if(cut) {
		if(cut_time < 0.0f) {
			pa->flag |= PARS_TAG;
		}
		else {
			rekey_element_to_time(index, cut_time);
			pa->flag |= PARS_EDIT_RECALC;
		}
	}
}
static void brush_length(ParticleSystem *psys, int index, void *userData)
{
	struct { short *mval; float rad; rcti* rect; float dist; float growfac; } *data = userData;
	ParticleData *pa = &psys->particles[index];
	HairKey *key;
	float dvec[3],pvec[3];
	int k;

	key = pa->hair;
	VECCOPY(pvec,key->co);

	for(k=1, key++; k<pa->totkey; k++,key++){
		VECSUB(dvec,key->co,pvec);
		VECCOPY(pvec,key->co);
		VecMulf(dvec,data->growfac);
		VECADD(key->co,(key-1)->co,dvec);
	}

	pa->flag |= PARS_EDIT_RECALC;
}
static void brush_puff(ParticleSystem *psys, int index, void *userData)
{
	struct { short *mval; float rad; rcti* rect; float dist;
		Object *ob; DerivedMesh *dm; float pufffac; int invert; } *data = userData;
	ParticleData *pa = &psys->particles[index];
	ParticleEdit *edit = psys->edit;
	HairKey *key;
	float mat[4][4], imat[4][4];
	float lastco[3], rootco[3], co[3], nor[3], kco[3], dco[3], fac, length;
	int k;

	psys_mat_hair_to_global(data->ob, data->dm, psys->part->from, pa, mat);
	Mat4Invert(imat,mat);

	/* find root coordinate and normal on emitter */
	key = pa->hair;
	VECCOPY(co, key->co);
	Mat4MulVecfl(mat, co);

	index= BLI_kdtree_find_nearest(edit->emitter_field, co, NULL, NULL);
	if(index == -1) return;

	VECCOPY(rootco, co);
	VecCopyf(nor, &psys->edit->emitter_cosnos[index*6+3]);
	Normalize(nor);
	length= 0.0f;

	fac= (float)pow((double)(1.0f - data->dist / data->rad), (double)data->pufffac);
	fac *= 0.025f;
	if(data->invert)
		fac= -fac;

	for(k=1, key++; k<pa->totkey; k++, key++){
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

	pa->flag |= PARS_EDIT_RECALC;
}
static void brush_smooth_get(ParticleSystem *psys, float mat[][4], float imat[][4], int pa_index, int key_index, void *userData)
{
	struct { Object *ob; short *mval; float rad; rcti* rect; float dist; float vec[3]; int tot; float smoothfac;} *data = userData;
	ParticleData *pa= &psys->particles[pa_index];
	HairKey *key = pa->hair + key_index;
	
	if(key_index){
		float dvec[3];

		VecSubf(dvec,key->co,(key-1)->co);
		Mat4Mul3Vecfl(mat,dvec);
		VECADD(data->vec,data->vec,dvec);
		data->tot++;
	}
}
static void brush_smooth_do(ParticleSystem *psys, float mat[][4], float imat[][4], int pa_index, int key_index, void *userData)
{
	struct { Object *ob; short *mval; float rad; rcti* rect; float dist; float vec[3]; int tot; float smoothfac;} *data = userData;
	ParticleData *pa= &psys->particles[pa_index];
	HairKey *key = pa->hair + key_index;
	float vec[3], dvec[3];
	
	if(key_index){
		VECCOPY(vec,data->vec);
		Mat4Mul3Vecfl(imat,vec);

		VecSubf(dvec,key->co,(key-1)->co);

		VECSUB(dvec,vec,dvec);
		VecMulf(dvec,data->smoothfac);
		
		VECADD(key->co,key->co,dvec);
	}

	pa->flag |= PARS_EDIT_RECALC;
}
#define EXPERIMENTAL_DEFORM_ONLY_PAINTING 1
static void brush_add(Object *ob, ParticleSystem *psys, short *mval, short number)
{
	ParticleData *add_pars = MEM_callocN(number*sizeof(ParticleData),"ParticleData add");
	ParticleSystemModifierData *psmd = psys_get_modifier(ob,psys);
	ParticleEditSettings *pset= PE_settings();
	ParticleEdit *edit = psys->edit;
	int i, k, n = 0, totpart = psys->totpart;
	short mco[2];
	short dmx = 0, dmy = 0;
	float co1[3], co2[3], min_d, imat[4][4];
	float framestep, timestep = psys_get_timestep(psys->part);
	short size = pset->brush[PE_BRUSH_ADD].size;
	short size2 = size*size;
#if EXPERIMENTAL_DEFORM_ONLY_PAINTING
	DerivedMesh *dm=0;
#endif
	Mat4Invert(imat,ob->obmat);

	BLI_srandom(psys->seed+mval[0]+mval[1]);
	
	/* painting onto the deformed mesh, could be an option? */
#if EXPERIMENTAL_DEFORM_ONLY_PAINTING
	if (psmd->dm->deformedOnly)
		dm = psmd->dm;
	else
		dm = mesh_get_derived_deform(ob, CD_MASK_BAREMESH);
#endif
	for(i=0; i<number; i++){
		if(number>1){
			dmx=dmy=size;
			while(dmx*dmx+dmy*dmy>size2){
				dmx=(short)((2.0f*BLI_frand()-1.0f)*size);
				dmy=(short)((2.0f*BLI_frand()-1.0f)*size);
			}
		}

		mco[0] = mval[0] + dmx;
		mco[1] = mval[1] + dmy;
		viewline(mco, co1, co2);

		Mat4MulVecfl(imat,co1);
		Mat4MulVecfl(imat,co2);
		min_d=2.0;
		
		/* warning, returns the derived mesh face */
#if EXPERIMENTAL_DEFORM_ONLY_PAINTING
		if(psys_intersect_dm(ob,dm,0,co1,co2,&min_d,&add_pars[n].num,add_pars[n].fuv,0,0,0,0)) {
			add_pars[n].num_dmcache= psys_particle_dm_face_lookup(ob,psmd->dm,add_pars[n].num,add_pars[n].fuv,NULL);
			n++;
		}
#else
#if 0
		if (psmd->dm->deformedOnly) {
			if(psys_intersect_dm(ob,psmd->dm,0,co1,co2,&min_d,&add_pars[n].num,add_pars[n].fuv,0,0,0,0)){
				n++;
			}
		} else {
			/* we need to test against the cage mesh, because 1) its faster and 2) then we can avoid converting the fuv back which is not simple */
			if(psys_intersect_dm(ob,psmd->dm,0,co1,co2,&min_d,&add_pars[n].num,add_pars[n].fuv,0,0,0,0)){
				MFace *mface;
				float fuv_mod[3] = {0.0, 0.0, 0.0};
				OrigSpaceFace *osface;
				
				mface= psmd->dm->getFaceData(psmd->dm,add_pars[n].num,CD_MFACE);
				osface= psmd->dm->getFaceData(psmd->dm, add_pars[n].num, CD_ORIGSPACE);
				
				add_pars[n].fuv[2]=0.0;
				
				/* use the original index for num and the derived index for num_dmcache */
				add_pars[n].num_dmcache = add_pars[n].num;
				add_pars[n].num = *(int *)psmd->dm->getFaceData(psmd->dm, add_pars[n].num, CD_ORIGINDEX);
				
				/* This is totally unaceptable code (fakeing mesh dara) but changing the target function isnt really nice either, do this temporarily */
				if (1) { /* Evilness*/
					MFace mface_fake;
					MVert mvert_fake[4];
					//int test1,test2;
					//test1 = add_pars[n].num_dmcache;
					//test2 = add_pars[n].num;
					
					mvert_fake[0].co[2] = mvert_fake[1].co[2] = mvert_fake[2].co[2] = mvert_fake[3].co[2] = 0.0;
					
					mface_fake.v1 = 0;
					mface_fake.v2 = 1;
					mface_fake.v3 = 2;
					
					if (mface->v4) {
						mface_fake.v4 = 3;
					} else {
						mface_fake.v4 = 0;
					}
					
					Vec2Copyf(mvert_fake[0].co, osface->uv[0]);
					Vec2Copyf(mvert_fake[1].co, osface->uv[1]);
					Vec2Copyf(mvert_fake[2].co, osface->uv[2]);
					Vec2Copyf(mvert_fake[3].co, osface->uv[3]);
					//printf("before %f %f %i %i\n", add_pars[n].fuv[0], add_pars[n].fuv[1], test1, test2);
					psys_interpolate_face(&mvert_fake, &mface_fake, NULL, &add_pars[n].fuv, &fuv_mod, NULL, NULL, NULL);
					
					/* Apply as the UV */
					Vec2Copyf(add_pars[n].fuv, fuv_mod);
					//printf("after %f %f\n", add_pars[n].fuv[0], add_pars[n].fuv[1]);
				}
				/* Make a fake face, for calculating the derived face's fuv on the original face */
				//PointInFace2DUV(mface->v4, osface->uv[0], osface->uv[1], osface->uv[2], osface->uv[3], add_pars[n].fuv, fuv_mod);
				//Vec2Copyf(add_pars[n].fuv, fuv_mod);
				
				n++;
			}
		}
#endif
#endif
	}
	if(n){
		int newtotpart=totpart+n;
		float hairmat[4][4], cur_co[3];
		KDTree *tree=0;
		ParticleData *pa, *new_pars = MEM_callocN(newtotpart*sizeof(ParticleData),"ParticleData new");
		ParticleEditKey *ekey, **key, **new_keys = MEM_callocN(newtotpart*sizeof(ParticleEditKey *),"ParticleEditKey array new");
		HairKey *hkey;

		/* save existing elements */
		memcpy(new_pars, psys->particles, totpart * sizeof(ParticleData));
		memcpy(new_keys, edit->keys, totpart * sizeof(ParticleEditKey*));

		/* change old arrays to new ones */
		if(psys->particles) MEM_freeN(psys->particles);
		psys->particles = new_pars;

		if(edit->keys) MEM_freeN(edit->keys);
		edit->keys = new_keys;

		if(edit->mirror_cache) {
			MEM_freeN(edit->mirror_cache);
			edit->mirror_cache = NULL;
		}

		/* create tree for interpolation */
		if(pset->flag & PE_INTERPOLATE_ADDED && psys->totpart){
			tree=BLI_kdtree_new(psys->totpart);
			
			for(i=0, pa=psys->particles; i<totpart; i++, pa++) {
				psys_particle_on_dm(ob,psmd->dm,psys->part->from,pa->num,pa->num_dmcache,pa->fuv,pa->foffset,cur_co,0,0,0,0,0);
				BLI_kdtree_insert(tree, i, cur_co, NULL);
			}

			BLI_kdtree_balance(tree);
		}

		psys->totpart = newtotpart;

		/* create new elements */
		pa = psys->particles + totpart;
		key = edit->keys + totpart;

		for(i=totpart; i<newtotpart; i++, pa++, key++){
			memcpy(pa, add_pars + i - totpart, sizeof(ParticleData));
			pa->hair = MEM_callocN(pset->totaddkey * sizeof(HairKey), "BakeKey key add");
			ekey = *key = MEM_callocN(pset->totaddkey * sizeof(ParticleEditKey), "ParticleEditKey add");
			pa->totkey = pset->totaddkey;

			for(k=0, hkey=pa->hair; k<pa->totkey; k++, hkey++, ekey++) {
				ekey->co = hkey->co;
				ekey->time = &hkey->time;
			}
			
			pa->size= 1.0f;
			initialize_particle(pa,i,ob,psys,psmd);
			reset_particle(pa,psys,psmd,ob,0.0,1.0,0,0,0);
			pa->flag |= PARS_EDIT_RECALC;
			if(pset->flag & PE_X_MIRROR)
				pa->flag |= PARS_TAG; /* signal for duplicate */
			
			framestep = pa->lifetime/(float)(pset->totaddkey-1);

			if(tree){
				HairKey *hkey;
				ParticleKey key[3];
				KDTreeNearest ptn[3];
				int w, maxw;
				float maxd, mind, dd, totw=0.0, weight[3];

				psys_particle_on_dm(ob,psmd->dm,psys->part->from,pa->num,pa->num_dmcache,pa->fuv,pa->foffset,co1,0,0,0,0,0);
				maxw = BLI_kdtree_find_n_nearest(tree,3,co1,NULL,ptn);

				maxd = ptn[maxw-1].dist;
				mind = ptn[0].dist;
				dd = maxd - mind;
				
				for(w=0; w<maxw; w++){
					weight[w] = (float)pow(2.0, (double)(-6.0f * ptn[w].dist / maxd));
					totw += weight[w];
				}
				for(;w<3; w++){
					weight[w] = 0.0f;
				}

				for(w=0; w<maxw; w++)
					weight[w] /= totw;

				for(k=0; k<pset->totaddkey; k++) {
					hkey = pa->hair + k;
					hkey->time = pa->time + k * framestep;

					key[0].time = hkey->time/ 100.0f;
					psys_get_particle_on_path(ob, psys, ptn[0].index, key, 0);
					VecMulf(key[0].co, weight[0]);
					
					if(maxw>1) {
						key[1].time = key[0].time;
						psys_get_particle_on_path(ob, psys, ptn[1].index, key + 1, 0);
						VecMulf(key[1].co, weight[1]);
						VECADD(key[0].co, key[0].co, key[1].co);

						if(maxw>2) {						
							key[2].time = key[0].time;
							psys_get_particle_on_path(ob, psys, ptn[2].index, key + 2, 0);
							VecMulf(key[2].co, weight[2]);
							VECADD(key[0].co, key[0].co, key[2].co);
						}
					}

					if(k==0)
						VECSUB(co1, pa->state.co, key[0].co);

					VECADD(pa->hair[k].co, key[0].co, co1);

					pa->hair[k].time = key[0].time;
				}
			}
			else{
				for(k=0, hkey=pa->hair; k<pset->totaddkey; k++, hkey++) {
					VECADDFAC(hkey->co, pa->state.co, pa->state.vel, k * framestep * timestep);
					pa->hair[k].time += k * framestep;
				}
			}
			for(k=0, hkey=pa->hair; k<pset->totaddkey; k++, hkey++) {
				psys_mat_hair_to_global(ob, psmd->dm, psys->part->from, pa, hairmat);
				Mat4Invert(imat,hairmat);
				Mat4MulVecfl(imat, hkey->co);
			}
		}
		edit->totkeys = psys_count_keys(psys);

		if(tree)
			BLI_kdtree_free(tree);
	}
	if(add_pars)
		MEM_freeN(add_pars);
	
/* painting onto the deformed mesh, could be an option? */
#if EXPERIMENTAL_DEFORM_ONLY_PAINTING
	if (!psmd->dm->deformedOnly)
		dm->release(dm);
#endif
}
static void brush_weight(ParticleSystem *psys, float mat[][4], float imat[][4], int pa_index, int key_index, void *userData)
{
	struct { Object *ob; short *mval; float rad; rcti* rect; float dist; float weightfac;} *data = userData;
	ParticleData *pa;

	/* roots have full weight allways */
	if(key_index) {
		pa= &psys->particles[pa_index];
		pa->hair[key_index].weight = data->weightfac;
		pa->flag |= PARS_EDIT_RECALC;
	}
}

/* returns 0 if no brush was used */
int PE_brush_particles(void)
{
	Object *ob = OBACT;
	ParticleSystem *psys = PE_get_current(ob);
	ParticleEdit *edit;
	ParticleEditSettings *pset = PE_settings();
	ParticleSystemModifierData *psmd;
	ParticleBrushData *brush;
	float vec1[3], vec2[3];
	short mval[2], mvalo[2], firsttime = 1, dx, dy;
	int selected = 0, flip, removed = 0;

	if(!PE_can_edit(psys)) return 0;

	edit = psys->edit;
	psmd= psys_get_modifier(ob, psys);

	flip= (get_qual() == LR_SHIFTKEY);

	if(pset->brushtype<0) return 0;
	brush= &pset->brush[pset->brushtype];

	initgrabz(ob->obmat[3][0], ob->obmat[3][1], ob->obmat[3][2]);

	getmouseco_areawin(mvalo);

	mval[0] = mvalo[0]; mval[1] = mvalo[1];

	while(get_mbut() & L_MOUSE){
		bglFlush();
		glReadBuffer(GL_BACK);
		glDrawBuffer(GL_BACK);
		persp(PERSP_VIEW);

		dx=mval[0]-mvalo[0];
		dy=mval[1]-mvalo[1];
		if(((pset->brushtype == PE_BRUSH_ADD) ?
			(sqrt(dx * dx + dy * dy) > pset->brush[PE_BRUSH_ADD].step) : (dx != 0 || dy != 0))
			|| firsttime){
			firsttime = 0;

			selected = (short)count_selected_keys(psys);

			switch(pset->brushtype){
				case PE_BRUSH_COMB:
				{
					struct { Object *ob; short *mval; float rad; rcti* rect; float dist; float *dvec; float combfac;} data;

					data.ob = ob;
					data.mval = mval;
					data.rad = (float)brush->size;

					data.combfac = (float)(brush->strength - 50) / 50.0f;
					if(data.combfac < 0.0f)
						data.combfac = 1.0f - 9.0f * data.combfac;
					else
						data.combfac = 1.0f - data.combfac;

					Mat4Invert(ob->imat, ob->obmat);

					window_to_3d(vec1, mvalo[0], mvalo[1]);
					window_to_3d(vec2, mval[0], mval[1]);
					VECSUB(vec1, vec2, vec1);
					data.dvec = vec1;

					foreach_mouse_hit_key(selected, psys,brush_comb, &data);
					break;
				}
				case PE_BRUSH_CUT:
				{
					struct { short *mval; float rad; rcti* rect; int selected; float cutfac; bglMats mats;} data;

					data.mval = mval;
					data.rad = (float)brush->size;

					data.selected = selected;

					data.cutfac = (float)(brush->strength / 100.0f);

					bgl_get_mats(&(data.mats));

					if(selected)
						foreach_selected_element(psys, brush_cut, &data);
					else
						PE_foreach_element(psys, brush_cut, &data);

					removed= remove_tagged_elements(ob, psys);
					if(pset->flag & PE_KEEP_LENGTHS)
						recalc_lengths(psys);
					break;
				}
				case PE_BRUSH_LENGTH:
				{
					struct { short *mval; float rad; rcti* rect; float dist; float growfac; } data;
					
					data.mval = mval;
					
					data.rad = (float)brush->size;
					data.growfac = (float)brush->strength / 5000.0f;

					if(brush->invert ^ flip)
						data.growfac = 1.0f - data.growfac;
					else
						data.growfac = 1.0f + data.growfac;

					foreach_mouse_hit_element(selected, psys, brush_length, &data);

					if(pset->flag & PE_KEEP_LENGTHS)
						recalc_lengths(psys);
					break;
				}
				case PE_BRUSH_PUFF:
				{
					struct { short *mval; float rad; rcti* rect; float dist;
						Object *ob; DerivedMesh *dm; float pufffac; int invert; } data;

					data.ob = ob;
					data.dm = psmd->dm;
					data.mval = mval;
					data.rad = (float)brush->size;

					data.pufffac = (float)(brush->strength - 50) / 50.0f;
					if(data.pufffac < 0.0f)
						data.pufffac = 1.0f - 9.0f * data.pufffac;
					else
						data.pufffac = 1.0f - data.pufffac;

					data.invert= (brush->invert ^ flip);
					Mat4Invert(ob->imat, ob->obmat);

					foreach_mouse_hit_element(selected, psys, brush_puff, &data);
					break;
				}
				case PE_BRUSH_ADD:
					if(psys->part->from==PART_FROM_FACE){
						brush_add(ob, psys, mval, brush->strength);
						if(pset->flag & PE_KEEP_LENGTHS)
							recalc_lengths(psys);
					}
					break;
				case PE_BRUSH_WEIGHT:
				{
					struct { Object *ob; short *mval; float rad; rcti* rect; float dist; float weightfac;} data;

					data.ob = ob;
					data.mval = mval;
					data.rad = (float)brush->size;

					data.weightfac = (float)(brush->strength / 100.0f);

					foreach_mouse_hit_key(selected, psys, brush_weight, &data);
					break;
				}
				case PE_BRUSH_SMOOTH:
				{
					struct { Object *ob; short *mval; float rad; rcti* rect; float dist; float vec[3]; int tot; float smoothfac;} data;

					data.ob = ob;
					data.mval = mval;
					data.rad = (float)brush->size;

					data.vec[0] = data.vec[1] = data.vec[2] = 0.0f;
					data.tot = 0;

					data.smoothfac = (float)(brush->strength / 100.0f);

					Mat4Invert(ob->imat, ob->obmat);

					foreach_mouse_hit_key(selected, psys, brush_smooth_get, &data);

					if(data.tot){
						VecMulf(data.vec, 1.0f / (float)data.tot);
						foreach_mouse_hit_key(selected, psys, brush_smooth_do, &data);
					}

					break;
				}
			}
			if((pset->flag & PE_KEEP_LENGTHS)==0)
				recalc_lengths(psys);

			if(pset->brushtype == PE_BRUSH_ADD || removed) {
				if(pset->brushtype == PE_BRUSH_ADD && (pset->flag & PE_X_MIRROR))
					PE_mirror_x(1);
				PE_recalc_world_cos(ob,psys);
				psys_free_path_cache(psys);
				DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
			}
			else
				PE_update_object(ob, 1);
			
			mvalo[0] = mval[0];
			mvalo[1] = mval[1];
		}

		force_draw(0);

		PIL_sleep_ms(10);
		
		getmouseco_areawin(mval);
	}
	allqueue(REDRAWVIEW3D, 1);

	BIF_undo_push("Brush edit particles");

	return 1;
}
static void set_delete_particle(ParticleSystem *psys, int index, void *userData)
{
	psys->particles[index].flag |= PARS_TAG;
}
static void set_delete_particle_key(ParticleSystem *psys, int pa_index, int key_index, void *userData)
{
	psys->edit->keys[pa_index][key_index].flag |= PEK_TAG;
}
void PE_delete_particle(void)
{
	Object *ob=OBACT;
	ParticleSystem *psys = PE_get_current(ob);
	short event=0;

	if(!PE_can_edit(psys)) return;

	event= pupmenu("Erase %t|Particle%x2|Key%x1");

	if(event<1) return;

	if(event==1){
		foreach_selected_key(psys, set_delete_particle_key, 0);
		remove_tagged_keys(ob, psys);
		recalc_lengths(psys);
	}
	else if(event==2){
		foreach_selected_element(psys, set_delete_particle, 0);
		remove_tagged_elements(ob, psys);
		recalc_lengths(psys);
	}

	DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
	allqueue(REDRAWVIEW3D, 1);
	BIF_undo_push("Delete particles/keys");
}

void PE_mirror_x(int tagged)
{
	Object *ob=OBACT;
	Mesh *me= (Mesh*)(ob->data);
	ParticleSystemModifierData *psmd;
	ParticleSystem *psys = PE_get_current(ob);
	ParticleEdit *edit;
	ParticleData *pa, *newpa, *new_pars;
	ParticleEditKey *ekey, **newkey, **key, **new_keys;
	HairKey *hkey;
	int *mirrorfaces;
	int i, k, rotation, totpart, newtotpart;

	if(!PE_can_edit(psys)) return;

	edit= psys->edit;
	psmd= psys_get_modifier(ob, psys);

	mirrorfaces= mesh_get_x_mirror_faces(ob);

	if(!edit->mirror_cache)
		PE_update_mirror_cache(ob, psys);

	totpart= psys->totpart;
	newtotpart= psys->totpart;
	LOOP_PARTICLES(i,pa) {
		if(pa->flag&PARS_HIDE) continue;

		if(!tagged) {
			if(particle_is_selected(psys, pa)) {
				if(edit->mirror_cache[i] != -1) {
					/* already has a mirror, don't need to duplicate */
					PE_mirror_particle(ob, psmd->dm, psys, pa, NULL);
					continue;
				}
				else
					pa->flag |= PARS_TAG;
			}
		}

		if((pa->flag & PARS_TAG) && mirrorfaces[pa->num*2] != -1)
			newtotpart++;
	}

	if(newtotpart != psys->totpart) {
		/* allocate new arrays and copy existing */
		new_pars= MEM_callocN(newtotpart*sizeof(ParticleData), "ParticleData new");
		new_keys= MEM_callocN(newtotpart*sizeof(ParticleEditKey*), "ParticleEditKey new");

		memcpy(new_pars, psys->particles, totpart*sizeof(ParticleData));
		memcpy(new_keys, edit->keys, totpart*sizeof(ParticleEditKey*));

		if(psys->particles) MEM_freeN(psys->particles);
		psys->particles= new_pars;

		if(edit->keys) MEM_freeN(edit->keys);
		edit->keys= new_keys;

		if(edit->mirror_cache) {
			MEM_freeN(edit->mirror_cache);
			edit->mirror_cache= NULL;
		}

		psys->totpart= newtotpart;
			
		/* create new elements */
		pa= psys->particles;
		newpa= psys->particles + totpart;
		key= edit->keys;
		newkey= edit->keys + totpart;

		for(i=0; i<totpart; i++, pa++, key++) {
			if(pa->flag&PARS_HIDE) continue;

			if(!(pa->flag & PARS_TAG) || mirrorfaces[pa->num*2] == -1)
				continue;

			/* duplicate */
			*newpa= *pa;
			if(pa->hair) newpa->hair= MEM_dupallocN(pa->hair);
			if(pa->keys) newpa->keys= MEM_dupallocN(pa->keys);
			if(*key) *newkey= MEM_dupallocN(*key);

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
			ekey= *newkey;
			for(k=0, hkey=newpa->hair; k<newpa->totkey; k++, hkey++, ekey++) {
				ekey->co= hkey->co;
				ekey->time= &hkey->time;
			}

			/* map key positions as mirror over x axis */
			PE_mirror_particle(ob, psmd->dm, psys, pa, newpa);

			newpa++;
			newkey++;
		}

		edit->totkeys = psys_count_keys(psys);
	}

	for(pa=psys->particles, i=0; i<psys->totpart; i++, pa++)
		pa->flag &= ~PARS_TAG;

	MEM_freeN(mirrorfaces);

	if(!tagged) {
		PE_recalc_world_cos(ob,psys);
		DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
		allqueue(REDRAWVIEW3D, 1);
		BIF_undo_push("Mirror particles");
	}
}

void PE_selectbrush_menu(void)
{
	ParticleEditSettings *pset= PE_settings();
	int val;
	
	pupmenu_set_active(pset->brushtype);
	
	val= pupmenu("Select Brush%t|None %x0|Comb %x1|Smooth %x7|Weight %x6|Add %x5|Length %x3|Puff %x4|Cut %x2");

	if(val>=0) {
		pset->brushtype= val-1;
		allqueue(REDRAWVIEW3D, 1);
	}
}

/************************************************/
/*			Particle Edit Undo					*/
/************************************************/
static void free_ParticleUndo(ParticleUndo *undo)
{
	ParticleData *pa;
	int i;

	for(i=0, pa=undo->particles; i<undo->totpart; i++, pa++) {
		if(pa->hair)
			MEM_freeN(pa->hair);
		if(undo->keys[i])
			MEM_freeN(undo->keys[i]);
	}
	if(undo->keys)
		MEM_freeN(undo->keys);

	if(undo->particles)
		MEM_freeN(undo->particles);

	//if(undo->emitter_cosnos)
	//	MEM_freeN(undo->emitter_cosnos);
}
static void make_ParticleUndo(ParticleSystem *psys, ParticleUndo *undo)
{
	ParticleData *pa,*upa;
	int i;

	undo->totpart = psys->totpart;
	undo->totkeys = psys->edit->totkeys;

	upa = undo->particles = MEM_dupallocN(psys->particles);
	undo->keys = MEM_dupallocN(psys->edit->keys);
	
	for(i=0, pa=psys->particles; i<undo->totpart; i++, pa++, upa++) {
		upa->hair = MEM_dupallocN(pa->hair);
		undo->keys[i] = MEM_dupallocN(psys->edit->keys[i]);
		/* no need to update edit key->co & key->time pointers here */
	}
}
static void get_ParticleUndo(ParticleSystem *psys, ParticleUndo *undo)
{
	ParticleData *pa, *upa;
	ParticleEditKey *key;
	HairKey *hkey;
	int i, k, totpart = psys->totpart;

	LOOP_PARTICLES(i,pa) {
		if(pa->hair)
			MEM_freeN(pa->hair);

		if(psys->edit->keys[i])
			MEM_freeN(psys->edit->keys[i]);
	}
	if(psys->particles)
		MEM_freeN(psys->particles);
	if(psys->edit->keys)
		MEM_freeN(psys->edit->keys);
	if(psys->edit->mirror_cache) {
		MEM_freeN(psys->edit->mirror_cache);
		psys->edit->mirror_cache= NULL;
	}

	pa = psys->particles = MEM_dupallocN(undo->particles);
	psys->edit->keys = MEM_dupallocN(undo->keys);

	for(i=0,upa=undo->particles; i<undo->totpart; i++, upa++, pa++){
		hkey = pa->hair = MEM_dupallocN(upa->hair);
		key = psys->edit->keys[i] = MEM_dupallocN(undo->keys[i]);
		for(k=0; k<pa->totkey; k++, hkey++, key++) {
			key->co = hkey->co;
			key->time = &hkey->time;
		}
	}

	psys->totpart = undo->totpart;
	psys->edit->totkeys = undo->totkeys;
}
void PE_undo_push(char *str)
{
	ParticleSystem *psys = PE_get_current(OBACT);
	ParticleEdit *edit = 0;
	ParticleUndo *undo;
	int nr;

	if(!PE_can_edit(psys)) return;
	edit = psys->edit;

	/* remove all undos after (also when curundo==NULL) */
	while(edit->undo.last != edit->curundo) {
		undo= edit->undo.last;
		BLI_remlink(&edit->undo, undo);
		free_ParticleUndo(undo);
		MEM_freeN(undo);
	}

	/* make new */
	edit->curundo= undo= MEM_callocN(sizeof(ParticleUndo), "particle undo file");
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
			ParticleUndo *first= edit->undo.first;
			BLI_remlink(&edit->undo, first);
			free_ParticleUndo(first);
			MEM_freeN(first);
		}
	}

	/* copy  */
	make_ParticleUndo(psys,edit->curundo);
}
void PE_undo_step(int step)
{	
	ParticleSystem *psys = PE_get_current(OBACT);
	ParticleEdit *edit = 0;

	if(!PE_can_edit(psys)) return;
	edit=psys->edit;

	if(step==0) {
		get_ParticleUndo(psys,edit->curundo);
	}
	else if(step==1) {
		
		if(edit->curundo==NULL || edit->curundo->prev==NULL) error("No more steps to undo");
		else {
			if(G.f & G_DEBUG) printf("undo %s\n", edit->curundo->name);
			edit->curundo= edit->curundo->prev;
			get_ParticleUndo(psys, edit->curundo);
		}
	}
	else {
		/* curundo has to remain current situation! */
		
		if(edit->curundo==NULL || edit->curundo->next==NULL) error("No more steps to redo");
		else {
			get_ParticleUndo(psys, edit->curundo->next);
			edit->curundo= edit->curundo->next;
			if(G.f & G_DEBUG) printf("redo %s\n", edit->curundo->name);
		}
	}

	DAG_object_flush_update(G.scene, OBACT, OB_RECALC_DATA);
	allqueue(REDRAWVIEW3D, 1);
	allqueue(REDRAWBUTSEDIT, 0);
	allqueue(REDRAWIMAGE, 0);
}
static void ParticleUndo_number(ParticleEdit *edit, int nr)
{
	ParticleUndo *undo;
	int a=1;
	
	for(undo= edit->undo.first; undo; undo= undo->next, a++) {
		if(a==nr) break;
	}
	edit->curundo= undo;
	PE_undo_step(0);
}
static void ParticleUndo_clear(ParticleSystem *psys)
{
	ParticleUndo *undo;
	ParticleEdit *edit;

	if(psys==0) return;

	edit = psys->edit;

	if(edit==0) return;
	
	undo= edit->undo.first;
	while(undo) {
		free_ParticleUndo(undo);
		undo= undo->next;
	}
	BLI_freelistN(&edit->undo);
	edit->curundo= NULL;
}
void PE_undo(void)
{
	PE_undo_step(1);
}
void PE_redo(void)
{
	PE_undo_step(-1);
}
void PE_undo_menu(void)
{
	ParticleSystem *psys = PE_get_current(OBACT);
	ParticleEdit *edit = 0;
	ParticleUndo *undo;
	DynStr *ds;
	short event;
	char *menu;

	if(!PE_can_edit(psys)) return;
	edit = psys->edit;
	
	ds= BLI_dynstr_new();

	BLI_dynstr_append(ds, "Particlemode Undo History %t");
	
	for(undo= edit->undo.first; undo; undo= undo->next) {
		BLI_dynstr_append(ds, "|");
		BLI_dynstr_append(ds, undo->name);
	}
	
	menu= BLI_dynstr_get_cstring(ds);
	BLI_dynstr_free(ds);
	
	event= pupmenu_col(menu, 20);
	MEM_freeN(menu);
	
	if(event>0) ParticleUndo_number(edit,event);
}

void PE_get_colors(char sel[4], char nosel[4])
{
	BIF_GetThemeColor3ubv(TH_EDGE_SELECT, sel);
	BIF_GetThemeColor3ubv(TH_WIRE, nosel);
}

int PE_minmax(float *min, float *max)
{
	Object *ob = OBACT;
	ParticleSystem *psys = PE_get_current(ob);
	ParticleSystemModifierData *psmd;
	ParticleData *pa;
	ParticleEditKey *key;
	float co[3], mat[4][4];
	int i, k, totpart, ok = 0;

	if(!PE_can_edit(psys)) return ok;
	
	psmd= psys_get_modifier(ob, psys);
	totpart= psys->totpart;

	LOOP_PARTICLES(i,pa){
		if(pa->flag&PARS_HIDE) continue;

		psys_mat_hair_to_global(ob, psmd->dm, psys->part->from, pa, mat);

		LOOP_KEYS(k,key){
			if(key->flag&PEK_SELECT) {
				VECCOPY(co, key->co);
				Mat4MulVecfl(mat, co);
				DO_MINMAX(co, min, max);		
				ok= 1;
			}
		}
	}

	if(!ok) {
		minmax_object(ob, min, max);
		ok= 1;
	}
  
	return ok;
}

