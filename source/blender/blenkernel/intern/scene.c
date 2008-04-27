/*  scene.c
 *  
 * 
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
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
#include "DNA_color_types.h"
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
#include "BKE_colortools.h"
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
#include "BKE_sculpt.h"
#include "BKE_world.h"
#include "BKE_utildefines.h"

#include "BIF_previewrender.h"

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
	BLI_freelistN(&sce->transform_spaces);
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

	sculptdata_free(sce);
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

	sce->r.simplify_subsurf= 6;
	sce->r.simplify_particles= 1.0f;
	sce->r.simplify_shadowsamples= 16;
	sce->r.simplify_aosss= 1.0f;

	sce->r.cineonblack= 95;
	sce->r.cineonwhite= 685;
	sce->r.cineongamma= 1.7f;
	
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

	BLI_init_rctf(&sce->r.safety, 0.1f, 0.9f, 0.1f, 0.9f);
	sce->r.osa= 8;

	sculptdata_init(sce);

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
	
	set_last_seq(NULL);
	
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

							if(!dupob)
								free_object_duplilist(duplilist);
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
	int a, totscene;
	
	if(sce->set==NULL) return 1;
	
	totscene= 0;
	for(scene= G.main->scene.first; scene; scene= scene->id.next)
		totscene++;
	
	for(a=0, scene=sce; scene->set; scene=scene->set, a++) {
		/* more iterations than scenes means we have a cycle */
		if(a > totscene) {
			/* the tested scene gets zero'ed, that's typically current scene */
			sce->set= NULL;
			return 0;
		}
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

/* Initialize 'permanent' sculpt data that is saved with file kept after
   switching out of sculptmode. */
void sculptdata_init(Scene *sce)
{
	SculptData *sd;

	if(!sce)
		return;

	sd= &sce->sculptdata;

	if(sd->cumap) {
		curvemapping_free(sd->cumap);
		sd->cumap = NULL;
	}

	memset(sd, 0, sizeof(SculptData));

	sd->drawbrush.size = sd->smoothbrush.size = sd->pinchbrush.size =
		sd->inflatebrush.size = sd->grabbrush.size =
		sd->layerbrush.size = sd->flattenbrush.size = 50;
	sd->drawbrush.strength = sd->smoothbrush.strength =
		sd->pinchbrush.strength = sd->inflatebrush.strength =
		sd->grabbrush.strength = sd->layerbrush.strength =
		sd->flattenbrush.strength = 25;
	sd->drawbrush.dir = sd->pinchbrush.dir = sd->inflatebrush.dir = sd->layerbrush.dir= 1;
	sd->drawbrush.flag = sd->smoothbrush.flag =
		sd->pinchbrush.flag = sd->inflatebrush.flag =
		sd->layerbrush.flag = sd->flattenbrush.flag = 0;
	sd->drawbrush.view= 0;
	sd->brush_type= DRAW_BRUSH;
	sd->texact= -1;
	sd->texfade= 1;
	sd->averaging= 1;
	sd->texsep= 0;
	sd->texrept= SCULPTREPT_DRAG;
	sd->flags= SCULPT_DRAW_BRUSH;
	sd->tablet_size=3;
	sd->tablet_strength=10;
	sd->rake=0;
	sculpt_reset_curve(sd);
}

void sculptdata_free(Scene *sce)
{
	SculptData *sd= &sce->sculptdata;
	int a;

	sculptsession_free(sce);

	for(a=0; a<MAX_MTEX; a++) {
		MTex *mtex= sd->mtex[a];
		if(mtex) {
			if(mtex->tex) mtex->tex->id.us--;
			MEM_freeN(mtex);
		}
	}

	curvemapping_free(sd->cumap);
	sd->cumap = NULL;
}

void sculpt_vertexusers_free(SculptSession *ss)
{
	if(ss && ss->vertex_users){
		MEM_freeN(ss->vertex_users);
		MEM_freeN(ss->vertex_users_mem);
		ss->vertex_users= NULL;
		ss->vertex_users_mem= NULL;
		ss->vertex_users_size= 0;
	}
}

void sculptsession_free(Scene *sce)
{
	SculptSession *ss= sce->sculptdata.session;
	if(ss) {
		if(ss->projverts)
			MEM_freeN(ss->projverts);
		if(ss->mats)
			MEM_freeN(ss->mats);

		if(ss->radialcontrol)
			MEM_freeN(ss->radialcontrol);

		sculpt_vertexusers_free(ss);
		if(ss->texcache)
			MEM_freeN(ss->texcache);
		MEM_freeN(ss);
		sce->sculptdata.session= NULL;
	}
}

/*  Default curve approximates 0.5 * (cos(pi * x) + 1), with 0 <= x <= 1 */
void sculpt_reset_curve(SculptData *sd)
{
	CurveMap *cm = NULL;

	if(!sd->cumap)
		sd->cumap = curvemapping_add(1, 0, 0, 1, 1);

	cm = sd->cumap->cm;

	if(cm->curve)
		MEM_freeN(cm->curve);
	cm->curve= MEM_callocN(6*sizeof(CurveMapPoint), "curve points");
	cm->flag &= ~CUMA_EXTEND_EXTRAPOLATE;
	cm->totpoint= 6;
	cm->curve[0].x= 0;
	cm->curve[0].y= 1;
	cm->curve[1].x= 0.1;
	cm->curve[1].y= 0.97553;
	cm->curve[2].x= 0.3;
	cm->curve[2].y= 0.79389;
	cm->curve[3].x= 0.9;
	cm->curve[3].y= 0.02447;
	cm->curve[4].x= 0.7;
	cm->curve[4].y= 0.20611;
	cm->curve[5].x= 1;
	cm->curve[5].y= 0;

	curvemapping_changed(sd->cumap, 0);
}

/* render simplification */

int get_render_subsurf_level(RenderData *r, int lvl)
{
	if(G.rt == 1 && (r->mode & R_SIMPLIFY))
		return MIN2(r->simplify_subsurf, lvl);
	else
		return lvl;
}

int get_render_child_particle_number(RenderData *r, int num)
{
	if(G.rt == 1 && (r->mode & R_SIMPLIFY))
		return (int)(r->simplify_particles*num);
	else
		return num;
}

int get_render_shadow_samples(RenderData *r, int samples)
{
	if(G.rt == 1 && (r->mode & R_SIMPLIFY) && samples > 0)
		return MIN2(r->simplify_shadowsamples, samples);
	else
		return samples;
}

float get_render_aosss_error(RenderData *r, float error)
{
	if(G.rt == 1 && (r->mode & R_SIMPLIFY))
		return ((1.0f-r->simplify_aosss)*10.0f + 1.0f)*error;
	else
		return error;
}

