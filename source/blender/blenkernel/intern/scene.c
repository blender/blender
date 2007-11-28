/*  scene.c
 *  
 * 
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#include <stdio.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef WIN32 
#include <unistd.h>
#else
#include <io.h>
#endif
#include "MEM_guardedalloc.h"

#include "DNA_armature_types.h"	
#include "DNA_constraint_types.h"
#include "DNA_curve_types.h"
#include "DNA_group_types.h"
#include "DNA_lamp_types.h"
#include "DNA_material_types.h"
#include "DNA_meta_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_scriptlink_types.h"
#include "DNA_texture_types.h"
#include "DNA_userdef_types.h"

#include "BKE_action.h"			
#include "BKE_anim.h"
#include "BKE_armature.h"		
#include "BKE_bad_level_calls.h"
#include "BKE_constraint.h"
#include "BKE_depsgraph.h"
#include "BKE_global.h"
#include "BKE_group.h"
#include "BKE_ipo.h"
#include "BKE_image.h"
#include "BKE_key.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_node.h"
#include "BKE_object.h"
#include "BKE_scene.h"
#include "BKE_world.h"
#include "BKE_utildefines.h"

#include "BIF_previewrender.h"
#include "BDR_sculptmode.h"

#include "BPY_extern.h"
#include "BLI_arithb.h"
#include "BLI_blenlib.h"

#include "nla.h"

#ifdef WIN32
#else
#include <sys/time.h>
#endif

void free_avicodecdata(AviCodecData *acd)
{
	if (acd) {
		if (acd->lpFormat){
			MEM_freeN(acd->lpFormat);
			acd->lpFormat = NULL;
			acd->cbFormat = 0;
		}
		if (acd->lpParms){
			MEM_freeN(acd->lpParms);
			acd->lpParms = NULL;
			acd->cbParms = 0;
		}
	}
}

void free_qtcodecdata(QuicktimeCodecData *qcd)
{
	if (qcd) {
		if (qcd->cdParms){
			MEM_freeN(qcd->cdParms);
			qcd->cdParms = NULL;
			qcd->cdSize = 0;
		}
	}
}

/* copy_scene moved to src/header_info.c... should be back */

/* do not free scene itself */
void free_scene(Scene *sce)
{
	Base *base;

	base= sce->base.first;
	while(base) {
		base->object->id.us--;
		base= base->next;
	}
	/* do not free objects! */

	BLI_freelistN(&sce->base);
	free_editing(sce->ed);
	if(sce->radio) MEM_freeN(sce->radio);
	sce->radio= 0;
	
	BPY_free_scriptlink(&sce->scriptlink);
	if (sce->r.avicodecdata) {
		free_avicodecdata(sce->r.avicodecdata);
		MEM_freeN(sce->r.avicodecdata);
		sce->r.avicodecdata = NULL;
	}
	if (sce->r.qtcodecdata) {
		free_qtcodecdata(sce->r.qtcodecdata);
		MEM_freeN(sce->r.qtcodecdata);
		sce->r.qtcodecdata = NULL;
	}
	
	BLI_freelistN(&sce->markers);
	BLI_freelistN(&sce->r.layers);
	
	if(sce->toolsettings){
		MEM_freeN(sce->toolsettings);
		sce->toolsettings = NULL;	
	}
	
	if (sce->theDag) {
		free_forest(sce->theDag);
		MEM_freeN(sce->theDag);
	}
	
	if(sce->nodetree) {
		ntreeFreeTree(sce->nodetree);
		MEM_freeN(sce->nodetree);
	}

	sculptmode_free_all(sce);
}

Scene *add_scene(char *name)
{
	Scene *sce;
	ParticleEditSettings *pset;
	int a;

	sce= alloc_libblock(&G.main->scene, ID_SCE, name);
	sce->lay= 1;
	sce->selectmode= SCE_SELECT_VERTEX;
	sce->editbutsize= 0.1;
	
	sce->r.mode= R_GAMMA;
	sce->r.cfra= 1;
	sce->r.sfra= 1;
	sce->r.efra= 250;
	sce->r.xsch= 320;
	sce->r.ysch= 256;
	sce->r.xasp= 1;
	sce->r.yasp= 1;
	sce->r.xparts= 4;
	sce->r.yparts= 4;
	sce->r.size= 100;
	sce->r.planes= 24;
	sce->r.quality= 90;
	sce->r.framapto= 100;
	sce->r.images= 100;
	sce->r.framelen= 1.0;
	sce->r.frs_sec= 25;
	sce->r.frs_sec_base= 1;
	sce->r.ocres = 128;
	
	sce->r.bake_mode= 1;	/* prevent to include render stuff here */
	sce->r.bake_filter= 2;
	sce->r.bake_osa= 5;
	sce->r.bake_flag= R_BAKE_CLEAR;
	sce->r.bake_normal_space= R_BAKE_SPACE_TANGENT;
	
	sce->r.xplay= 640;
	sce->r.yplay= 480;
	sce->r.freqplay= 60;
	sce->r.depth= 32;
	
	sce->r.threads= 1;
	
	sce->r.stereomode = 1;  // no stereo

	sce->toolsettings = MEM_callocN(sizeof(struct ToolSettings),"Tool Settings Struct");
	sce->toolsettings->cornertype=1;
	sce->toolsettings->degr = 90; 
	sce->toolsettings->step = 9;
	sce->toolsettings->turn = 1; 				
	sce->toolsettings->extr_offs = 1; 
	sce->toolsettings->doublimit = 0.001;
	sce->toolsettings->segments = 32;
	sce->toolsettings->rings = 32;
	sce->toolsettings->vertices = 32;
	sce->toolsettings->editbutflag = 1;
	sce->toolsettings->uvcalc_radius = 1.0f;
	sce->toolsettings->uvcalc_cubesize = 1.0f;
	sce->toolsettings->uvcalc_mapdir = 1;
	sce->toolsettings->uvcalc_mapalign = 1;
	sce->toolsettings->unwrapper = 1;
	sce->toolsettings->select_thresh= 0.01f;
	sce->toolsettings->jointrilimit = 0.8f;

	pset= &sce->toolsettings->particle;
	pset->flag= PE_KEEP_LENGTHS|PE_LOCK_FIRST|PE_DEFLECT_EMITTER;
	pset->emitterdist= 0.25f;
	pset->totrekey= 5;
	pset->totaddkey= 5;
	pset->brushtype= PE_BRUSH_NONE;
	for(a=0; a<PE_TOT_BRUSH; a++) {
		pset->brush[a].strength= 50;
		pset->brush[a].size= 50;
		pset->brush[a].step= 10;
	}
	pset->brush[PE_BRUSH_CUT].strength= 100;
	
	sce->jumpframe = 10;
	sce->audio.mixrate = 44100;

	strcpy(sce->r.backbuf, "//backbuf");
	strcpy(sce->r.pic, U.renderdir);
	strcpy(sce->r.ftype, "//ftype");

	BLI_init_rctf(&sce->r.safety, 0.1f, 0.9f, 0.1f, 0.9f);
	sce->r.osa= 8;

	sculptmode_init(sce);
	
	/* note; in header_info.c the scene copy happens..., if you add more to renderdata it has to be checked there */
	scene_add_render_layer(sce);
	
	return sce;
}

Base *object_in_scene(Object *ob, Scene *sce)
{
	Base *base;
	
	base= sce->base.first;
	while(base) {
		if(base->object == ob) return base;
		base= base->next;
	}
	return NULL;
}

void set_scene_bg(Scene *sce)
{
	Base *base;
	Object *ob;
	Group *group;
	GroupObject *go;
	int flag;
	
	G.scene= sce;
	
	/* check for cyclic sets, for reading old files but also for definite security (py?) */
	scene_check_setscene(G.scene);
	
	/* deselect objects (for dataselect) */
	for(ob= G.main->object.first; ob; ob= ob->id.next)
		ob->flag &= ~(SELECT|OB_FROMGROUP);

	/* group flags again */
	for(group= G.main->group.first; group; group= group->id.next) {
		go= group->gobject.first;
		while(go) {
			if(go->ob) go->ob->flag |= OB_FROMGROUP;
			go= go->next;
		}
	}

	/* sort baselist */
	DAG_scene_sort(sce);
	
	/* ensure dags are built for sets */
	for(sce= sce->set; sce; sce= sce->set)
		if(sce->theDag==NULL)
			DAG_scene_sort(sce);

	/* copy layers and flags from bases to objects */
	for(base= G.scene->base.first; base; base= base->next) {
		ob= base->object;
		ob->lay= base->lay;
		
		/* group patch... */
		base->flag &= ~(OB_FROMGROUP);
		flag= ob->flag & (OB_FROMGROUP);
		base->flag |= flag;
		
		/* not too nice... for recovering objects with lost data */
		if(ob->pose==NULL) base->flag &= ~OB_POSEMODE;
		ob->flag= base->flag;
		
		ob->ctime= -1234567.0;	/* force ipo to be calculated later */
	}
	/* no full animation update, this to enable render code to work (render code calls own animation updates) */
	
	/* do we need FRAMECHANGED in set_scene? */
//	if (G.f & G_DOSCRIPTLINKS) BPY_do_all_scripts(SCRIPT_FRAMECHANGED);
}

/* called from creator.c */
void set_scene_name(char *name)
{
	Scene *sce;

	for (sce= G.main->scene.first; sce; sce= sce->id.next) {
		if (BLI_streq(name, sce->id.name+2)) {
			set_scene_bg(sce);
			return;
		}
	}
	
	error("Can't find scene: %s", name);
}

/* used by metaballs
 * doesnt return the original duplicated object, only dupli's
 */
int next_object(int val, Base **base, Object **ob)
{
	static ListBase *duplilist= NULL;
	static DupliObject *dupob;
	static int fase;
	int run_again=1;
	
	/* init */
	if(val==0) {
		fase= F_START;
		dupob= NULL;
	}
	else {

		/* run_again is set when a duplilist has been ended */
		while(run_again) {
			run_again= 0;

			/* the first base */
			if(fase==F_START) {
				*base= G.scene->base.first;
				if(*base) {
					*ob= (*base)->object;
					fase= F_SCENE;
				}
				else {
				    /* exception: empty scene */
					if(G.scene->set && G.scene->set->base.first) {
						*base= G.scene->set->base.first;
						*ob= (*base)->object;
						fase= F_SET;
					}
				}
			}
			else {
				if(*base && fase!=F_DUPLI) {
					*base= (*base)->next;
					if(*base) *ob= (*base)->object;
					else {
						if(fase==F_SCENE) {
							/* scene is finished, now do the set */
							if(G.scene->set && G.scene->set->base.first) {
								*base= G.scene->set->base.first;
								*ob= (*base)->object;
								fase= F_SET;
							}
						}
					}
				}
			}
			
			if(*base == NULL) fase= F_START;
			else {
				if(fase!=F_DUPLI) {
					if( (*base)->object->transflag & OB_DUPLI) {
						/* groups cannot be duplicated for mballs yet, 
						this enters eternal loop because of 
						makeDispListMBall getting called inside of group_duplilist */
						if((*base)->object->dup_group == NULL) {
							duplilist= object_duplilist(G.scene, (*base)->object);
							
							dupob= duplilist->first;
						}
					}
				}
				/* handle dupli's */
				if(dupob) {
					
					Mat4CpyMat4(dupob->ob->obmat, dupob->mat);
					
					(*base)->flag |= OB_FROMDUPLI;
					*ob= dupob->ob;
					fase= F_DUPLI;
					
					dupob= dupob->next;
				}
				else if(fase==F_DUPLI) {
					fase= F_SCENE;
					(*base)->flag &= ~OB_FROMDUPLI;
					
					for(dupob= duplilist->first; dupob; dupob= dupob->next) {
						Mat4CpyMat4(dupob->ob->obmat, dupob->omat);
					}
					
					free_object_duplilist(duplilist);
					duplilist= NULL;
					run_again= 1;
				}
			}
		}
	}
	
	return fase;
}

Object *scene_find_camera(Scene *sc)
{
	Base *base;
	
	for (base= sc->base.first; base; base= base->next)
		if (base->object->type==OB_CAMERA)
			return base->object;

	return NULL;
}


Base *scene_add_base(Scene *sce, Object *ob)
{
	Base *b= MEM_callocN(sizeof(*b), "scene_add_base");
	BLI_addhead(&sce->base, b);

	b->object= ob;
	b->flag= ob->flag;
	b->lay= ob->lay;

	return b;
}

void scene_deselect_all(Scene *sce)
{
	Base *b;

	for (b= sce->base.first; b; b= b->next) {
		b->flag&= ~SELECT;
		b->object->flag= b->flag;
	}
}

void scene_select_base(Scene *sce, Base *selbase)
{
	scene_deselect_all(sce);

	selbase->flag |= SELECT;
	selbase->object->flag= selbase->flag;

	sce->basact= selbase;
}

/* checks for cycle, returns 1 if it's all OK */
int scene_check_setscene(Scene *sce)
{
	Scene *scene;
	
	if(sce->set==NULL) return 1;
	
	/* LIB_DOIT is the free flag to tag library data */
	for(scene= G.main->scene.first; scene; scene= scene->id.next)
		scene->id.flag &= ~LIB_DOIT;
	
	scene= sce;
	while(scene->set) {
		scene->id.flag |= LIB_DOIT;
		/* when set has flag set, we got a cycle */
		if(scene->set->id.flag & LIB_DOIT)
			break;
		scene= scene->set;
	}
	
	if(scene->set) {
		/* the tested scene gets zero'ed, that's typically current scene */
		sce->set= NULL;
		return 0;
	}
	return 1;
}

static void scene_update(Scene *sce, unsigned int lay)
{
	Base *base;
	Object *ob;
	
	if(sce->theDag==NULL)
		DAG_scene_sort(sce);
	
	DAG_scene_update_flags(sce, lay);   // only stuff that moves or needs display still
	
	for(base= sce->base.first; base; base= base->next) {
		ob= base->object;
		
		object_handle_update(ob);   // bke_object.h
		
		/* only update layer when an ipo */
		if(ob->ipo && has_ipo_code(ob->ipo, OB_LAY) ) {
			base->lay= ob->lay;
		}
	}
}

/* applies changes right away, does all sets too */
void scene_update_for_newframe(Scene *sce, unsigned int lay)
{
	Scene *scene= sce;
	
	/* clears all BONE_UNKEYED flags for every pose's pchans */
	framechange_poses_clear_unkeyed();
	
	/* object ipos are calculated in where_is_object */
	do_all_data_ipos();
	
	if (G.f & G_DOSCRIPTLINKS) BPY_do_all_scripts(SCRIPT_FRAMECHANGED);
	
	/* sets first, we allow per definition current scene to have dependencies on sets */
	for(sce= sce->set; sce; sce= sce->set)
		scene_update(sce, lay);

	scene_update(scene, lay);
}

/* return default layer, also used to patch old files */
void scene_add_render_layer(Scene *sce)
{
	SceneRenderLayer *srl;
	int tot= 1 + BLI_countlist(&sce->r.layers);
	
	srl= MEM_callocN(sizeof(SceneRenderLayer), "new render layer");
	sprintf(srl->name, "%d RenderLayer", tot);
	BLI_addtail(&sce->r.layers, srl);

	/* note, this is also in render, pipeline.c, to make layer when scenedata doesnt have it */
	srl->lay= (1<<20) -1;
	srl->layflag= 0x7FFF;	/* solid ztra halo edge strand */
	srl->passflag= SCE_PASS_COMBINED|SCE_PASS_Z;
}

