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

#include <stddef.h>
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

#include "DNA_anim_types.h"
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
#include "DNA_screen_types.h"
#include "DNA_texture_types.h"
#include "DNA_userdef_types.h"

#include "BKE_action.h"			
#include "BKE_anim.h"
#include "BKE_animsys.h"
#include "BKE_armature.h"		
#include "BKE_colortools.h"
#include "BKE_colortools.h"
#include "BKE_constraint.h"
#include "BKE_depsgraph.h"
#include "BKE_global.h"
#include "BKE_group.h"
#include "BKE_ipo.h"
#include "BKE_idprop.h"
#include "BKE_image.h"
#include "BKE_key.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_node.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_pointcache.h"
#include "BKE_scene.h"
#include "BKE_sequencer.h"
#include "BKE_world.h"
#include "BKE_utildefines.h"

//XXX #include "BIF_previewrender.h"
//XXX #include "BIF_editseq.h"

#ifndef DISABLE_PYTHON
#include "BPY_extern.h"
#endif

#include "BLI_math.h"
#include "BLI_blenlib.h"

//XXX #include "nla.h"

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

Scene *copy_scene(Main *bmain, Scene *sce, int type)
{
	Scene *scen;
	ToolSettings *ts;
	Base *base, *obase;
	
	if(type == SCE_COPY_EMPTY) {
		ListBase lb;
		scen= add_scene(sce->id.name+2);
		
		lb= scen->r.layers;
		scen->r= sce->r;
		scen->r.layers= lb;
	}
	else {
		scen= copy_libblock(sce);
		BLI_duplicatelist(&(scen->base), &(sce->base));
		
		clear_id_newpoins();
		
		id_us_plus((ID *)scen->world);
		id_us_plus((ID *)scen->set);
		id_us_plus((ID *)scen->ima);
		id_us_plus((ID *)scen->gm.dome.warptext);

		scen->ed= NULL;
		scen->theDag= NULL;
		scen->obedit= NULL;
		scen->toolsettings= MEM_dupallocN(sce->toolsettings);
		scen->stats= NULL;

		ts= scen->toolsettings;
		if(ts) {
			if(ts->vpaint) {
				ts->vpaint= MEM_dupallocN(ts->vpaint);
				ts->vpaint->paintcursor= NULL;
				ts->vpaint->vpaint_prev= NULL;
				ts->vpaint->wpaint_prev= NULL;
				copy_paint(&ts->vpaint->paint, &ts->vpaint->paint);
			}
			if(ts->wpaint) {
				ts->wpaint= MEM_dupallocN(ts->wpaint);
				ts->wpaint->paintcursor= NULL;
				ts->wpaint->vpaint_prev= NULL;
				ts->wpaint->wpaint_prev= NULL;
				copy_paint(&ts->wpaint->paint, &ts->wpaint->paint);
			}
			if(ts->sculpt) {
				ts->sculpt= MEM_dupallocN(ts->sculpt);
				copy_paint(&ts->sculpt->paint, &ts->sculpt->paint);
			}

			copy_paint(&ts->imapaint.paint, &ts->imapaint.paint);
			ts->imapaint.paintcursor= NULL;

			ts->particle.paintcursor= NULL;
		}
		
		BLI_duplicatelist(&(scen->markers), &(sce->markers));
		BLI_duplicatelist(&(scen->transform_spaces), &(sce->transform_spaces));
		BLI_duplicatelist(&(scen->r.layers), &(sce->r.layers));
		BKE_keyingsets_copy(&(scen->keyingsets), &(sce->keyingsets));
		
		scen->nodetree= ntreeCopyTree(sce->nodetree, 0);
		
		obase= sce->base.first;
		base= scen->base.first;
		while(base) {
			id_us_plus(&base->object->id);
			if(obase==sce->basact) scen->basact= base;
	
			obase= obase->next;
			base= base->next;
		}
	}
	
	/* make a private copy of the avicodecdata */
	if(sce->r.avicodecdata) {
		scen->r.avicodecdata = MEM_dupallocN(sce->r.avicodecdata);
		scen->r.avicodecdata->lpFormat = MEM_dupallocN(scen->r.avicodecdata->lpFormat);
		scen->r.avicodecdata->lpParms = MEM_dupallocN(scen->r.avicodecdata->lpParms);
	}
	
	/* make a private copy of the qtcodecdata */
	if(sce->r.qtcodecdata) {
		scen->r.qtcodecdata = MEM_dupallocN(sce->r.qtcodecdata);
		scen->r.qtcodecdata->cdParms = MEM_dupallocN(scen->r.qtcodecdata->cdParms);
	}
	
	/* NOTE: part of SCE_COPY_LINK_DATA and SCE_COPY_FULL operations
	 * are done outside of blenkernel with ED_objects_single_users! */

    /*  camera */
	if(type == SCE_COPY_LINK_DATA || type == SCE_COPY_FULL) {
	    ID_NEW(scen->camera);
	}

	/* world */
	if(type == SCE_COPY_FULL) {
        if(scen->world) {
            id_us_plus((ID *)scen->world);
            scen->world= copy_world(scen->world);
        }
	}

	return scen;
}

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
	
	if(sce->gpd) {
#if 0   // removed since this can be invalid memory when freeing everything
        // since the grease pencil data is free'd before the scene.
        // since grease pencil data is not (yet?), shared between objects
        // its probably safe not to do this, some save and reload will free this.
		sce->gpd->id.us--;
#endif
		sce->gpd= NULL;
	}

	BLI_freelistN(&sce->base);
	seq_free_editing(sce);

	BKE_free_animdata((ID *)sce);
	BKE_keyingsets_free(&sce->keyingsets);
	
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
	if (sce->r.ffcodecdata.properties) {
		IDP_FreeProperty(sce->r.ffcodecdata.properties);
		MEM_freeN(sce->r.ffcodecdata.properties);
		sce->r.ffcodecdata.properties = NULL;
	}
	
	BLI_freelistN(&sce->markers);
	BLI_freelistN(&sce->transform_spaces);
	BLI_freelistN(&sce->r.layers);
	
	if(sce->toolsettings) {
		if(sce->toolsettings->vpaint) {
			free_paint(&sce->toolsettings->vpaint->paint);
			MEM_freeN(sce->toolsettings->vpaint);
		}
		if(sce->toolsettings->wpaint) {
			free_paint(&sce->toolsettings->wpaint->paint);
			MEM_freeN(sce->toolsettings->wpaint);
		}
		if(sce->toolsettings->sculpt) {
			free_paint(&sce->toolsettings->sculpt->paint);
			MEM_freeN(sce->toolsettings->sculpt);
		}
		free_paint(&sce->toolsettings->imapaint.paint);

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

	if(sce->stats)
		MEM_freeN(sce->stats);
}

Scene *add_scene(char *name)
{
	Scene *sce;
	ParticleEditSettings *pset;
	int a;

	sce= alloc_libblock(&G.main->scene, ID_SCE, name);
	sce->lay= 1;
	
	sce->r.mode= R_GAMMA|R_OSA|R_SHADOW|R_SSS|R_ENVMAP|R_RAYTRACE;
	sce->r.cfra= 1;
	sce->r.sfra= 1;
	sce->r.efra= 250;
	sce->r.frame_step= 1;
	sce->r.xsch= 1920;
	sce->r.ysch= 1080;
	sce->r.xasp= 1;
	sce->r.yasp= 1;
	sce->r.xparts= 8;
	sce->r.yparts= 8;
	sce->r.size= 25;
	sce->r.planes= 24;
	sce->r.quality= 90;
	sce->r.framapto= 100;
	sce->r.images= 100;
	sce->r.framelen= 1.0;
	sce->r.frs_sec= 25;
	sce->r.frs_sec_base= 1;
	sce->r.ocres = 128;
	sce->r.color_mgt_flag |= R_COLOR_MANAGEMENT;
	
	sce->r.bake_mode= 1;	/* prevent to include render stuff here */
	sce->r.bake_filter= 8;
	sce->r.bake_osa= 5;
	sce->r.bake_flag= R_BAKE_CLEAR;
	sce->r.bake_normal_space= R_BAKE_SPACE_TANGENT;

	sce->r.scemode= R_DOCOMP|R_DOSEQ|R_EXTENSION;
	sce->r.stamp= R_STAMP_TIME|R_STAMP_FRAME|R_STAMP_DATE|R_STAMP_SCENE|R_STAMP_CAMERA|R_STAMP_RENDERTIME;
	
	sce->r.threads= 1;

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

	sce->toolsettings->selectmode= SCE_SELECT_VERTEX;
	sce->toolsettings->normalsize= 0.1;
	sce->toolsettings->autokey_mode= U.autokey_mode;

	sce->toolsettings->skgen_resolution = 100;
	sce->toolsettings->skgen_threshold_internal 	= 0.01f;
	sce->toolsettings->skgen_threshold_external 	= 0.01f;
	sce->toolsettings->skgen_angle_limit	 		= 45.0f;
	sce->toolsettings->skgen_length_ratio			= 1.3f;
	sce->toolsettings->skgen_length_limit			= 1.5f;
	sce->toolsettings->skgen_correlation_limit		= 0.98f;
	sce->toolsettings->skgen_symmetry_limit			= 0.1f;
	sce->toolsettings->skgen_postpro = SKGEN_SMOOTH;
	sce->toolsettings->skgen_postpro_passes = 1;
	sce->toolsettings->skgen_options = SKGEN_FILTER_INTERNAL|SKGEN_FILTER_EXTERNAL|SKGEN_FILTER_SMART|SKGEN_HARMONIC|SKGEN_SUB_CORRELATION|SKGEN_STICK_TO_EMBEDDING;
	sce->toolsettings->skgen_subdivisions[0] = SKGEN_SUB_CORRELATION;
	sce->toolsettings->skgen_subdivisions[1] = SKGEN_SUB_LENGTH;
	sce->toolsettings->skgen_subdivisions[2] = SKGEN_SUB_ANGLE;

	sce->toolsettings->proportional_size = 1.0f;

	sce->physics_settings.gravity[0] = 0.0f;
	sce->physics_settings.gravity[1] = 0.0f;
	sce->physics_settings.gravity[2] = -9.81f;
	sce->physics_settings.flag = PHYS_GLOBAL_GRAVITY;

	sce->unit.scale_length = 1.0f;

	pset= &sce->toolsettings->particle;
	pset->flag= PE_KEEP_LENGTHS|PE_LOCK_FIRST|PE_DEFLECT_EMITTER|PE_AUTO_VELOCITY;
	pset->emitterdist= 0.25f;
	pset->totrekey= 5;
	pset->totaddkey= 5;
	pset->brushtype= PE_BRUSH_NONE;
	pset->draw_step= 2;
	pset->fade_frames= 2;
	pset->selectmode= SCE_SELECT_PATH;
	for(a=0; a<PE_TOT_BRUSH; a++) {
		pset->brush[a].strength= 50;
		pset->brush[a].size= 50;
		pset->brush[a].step= 10;
	}
	pset->brush[PE_BRUSH_CUT].strength= 100;
	
	sce->jumpframe = 10;
	sce->r.ffcodecdata.audio_mixrate = 44100;

	sce->audio.distance_model = 2.0;
	sce->audio.doppler_factor = 1.0;
	sce->audio.speed_of_sound = 343.3;

	strcpy(sce->r.backbuf, "//backbuf");
	strcpy(sce->r.pic, U.renderdir);

	BLI_init_rctf(&sce->r.safety, 0.1f, 0.9f, 0.1f, 0.9f);
	sce->r.osa= 8;

	/* note; in header_info.c the scene copy happens..., if you add more to renderdata it has to be checked there */
	scene_add_render_layer(sce);
	
	/* game data */
	sce->gm.stereoflag = STEREO_NOSTEREO;
	sce->gm.stereomode = STEREO_ANAGLYPH;
	sce->gm.eyeseparation = 0.10;

	sce->gm.dome.angle = 180;
	sce->gm.dome.mode = DOME_FISHEYE;
	sce->gm.dome.res = 4;
	sce->gm.dome.resbuf = 1.0f;
	sce->gm.dome.tilt = 0;

	sce->gm.xplay= 800;
	sce->gm.yplay= 600;
	sce->gm.freqplay= 60;
	sce->gm.depth= 32;

	sce->gm.gravity= 9.8f;
	sce->gm.physicsEngine= WOPHY_BULLET;
	sce->gm.mode = 32; //XXX ugly harcoding, still not sure we should drop mode. 32 == 1 << 5 == use_occlusion_culling 
	sce->gm.occlusionRes = 128;
	sce->gm.ticrate = 60;
	sce->gm.maxlogicstep = 5;
	sce->gm.physubstep = 1;
	sce->gm.maxphystep = 5;

	sce->gm.flag = GAME_DISPLAY_LISTS;
	sce->gm.matmode = GAME_MAT_MULTITEX;

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

void set_scene_bg(Scene *scene)
{
	Scene *sce;
	Base *base;
	Object *ob;
	Group *group;
	GroupObject *go;
	int flag;
	
	/* check for cyclic sets, for reading old files but also for definite security (py?) */
	scene_check_setscene(scene);
	
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
	DAG_scene_sort(scene);
	
	/* ensure dags are built for sets */
	for(sce= scene->set; sce; sce= sce->set)
		if(sce->theDag==NULL)
			DAG_scene_sort(sce);

	/* copy layers and flags from bases to objects */
	for(base= scene->base.first; base; base= base->next) {
		ob= base->object;
		ob->lay= base->lay;
		
		/* group patch... */
		base->flag &= ~(OB_FROMGROUP);
		flag= ob->flag & (OB_FROMGROUP);
		base->flag |= flag;
		
		/* not too nice... for recovering objects with lost data */
		//if(ob->pose==NULL) base->flag &= ~OB_POSEMODE;
		ob->flag= base->flag;
		
		ob->ctime= -1234567.0;	/* force ipo to be calculated later */
	}
	/* no full animation update, this to enable render code to work (render code calls own animation updates) */
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
	
	//XXX error("Can't find scene: %s", name);
}

void unlink_scene(Main *bmain, Scene *sce, Scene *newsce)
{
	Scene *sce1;
	bScreen *sc;

	/* check all sets */
	for(sce1= bmain->scene.first; sce1; sce1= sce1->id.next)
		if(sce1->set == sce)
			sce1->set= NULL;
	
	/* check all sequences */
	clear_scene_in_allseqs(sce);

	/* check render layer nodes in other scenes */
	clear_scene_in_nodes(bmain, sce);
	
	/* al screens */
	for(sc= bmain->screen.first; sc; sc= sc->id.next)
		if(sc->scene == sce)
			sc->scene= newsce;

	free_libblock(&bmain->scene, sce);
}

/* used by metaballs
 * doesnt return the original duplicated object, only dupli's
 */
int next_object(Scene *scene, int val, Base **base, Object **ob)
{
	static ListBase *duplilist= NULL;
	static DupliObject *dupob;
	static int fase= F_START, in_next_object= 0;
	int run_again=1;
	
	/* init */
	if(val==0) {
		fase= F_START;
		dupob= NULL;
		
		/* XXX particle systems with metas+dupligroups call this recursively */
		/* see bug #18725 */
		if(in_next_object) {
			printf("ERROR: MetaBall generation called recursively, not supported\n");
			
			return F_ERROR;
		}
	}
	else {
		in_next_object= 1;
		
		/* run_again is set when a duplilist has been ended */
		while(run_again) {
			run_again= 0;

			/* the first base */
			if(fase==F_START) {
				*base= scene->base.first;
				if(*base) {
					*ob= (*base)->object;
					fase= F_SCENE;
				}
				else {
				    /* exception: empty scene */
					if(scene->set && scene->set->base.first) {
						*base= scene->set->base.first;
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
							if(scene->set && scene->set->base.first) {
								*base= scene->set->base.first;
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
							duplilist= object_duplilist(scene, (*base)->object);
							
							dupob= duplilist->first;

							if(!dupob)
								free_object_duplilist(duplilist);
						}
					}
				}
				/* handle dupli's */
				if(dupob) {
					
					copy_m4_m4(dupob->ob->obmat, dupob->mat);
					
					(*base)->flag |= OB_FROMDUPLI;
					*ob= dupob->ob;
					fase= F_DUPLI;
					
					dupob= dupob->next;
				}
				else if(fase==F_DUPLI) {
					fase= F_SCENE;
					(*base)->flag &= ~OB_FROMDUPLI;
					
					for(dupob= duplilist->first; dupob; dupob= dupob->next) {
						copy_m4_m4(dupob->ob->obmat, dupob->omat);
					}
					
					free_object_duplilist(duplilist);
					duplilist= NULL;
					run_again= 1;
				}
			}
		}
	}
	
	/* reset recursion test */
	in_next_object= 0;
	
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

#ifdef DURIAN_CAMERA_SWITCH
Object *scene_find_camera_switch(Scene *scene)
{
	TimeMarker *m;
	int cfra = scene->r.cfra;
	int frame = -(MAXFRAME + 1);
	Object *camera= NULL;

	for (m= scene->markers.first; m; m= m->next) {
		if(m->camera && (m->frame <= cfra) && (m->frame > frame)) {
			camera= m->camera;
			frame= m->frame;

			if(frame == cfra)
				break;

		}
	}
	return camera;
}
#endif

static char *get_cfra_marker_name(Scene *scene)
{
	ListBase *markers= &scene->markers;
	TimeMarker *m1, *m2;

	/* search through markers for match */
	for (m1=markers->first, m2=markers->last; m1 && m2; m1=m1->next, m2=m2->prev) {
		if (m1->frame==CFRA)
			return m1->name;

		if (m1 == m2)
			break;

		if (m2->frame==CFRA)
			return m2->name;
	}

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

/* This (evil) function is needed to cope with two legacy Blender rendering features
* mblur (motion blur that renders 'subframes' and blurs them together), and fields 
* rendering. Thus, the use of ugly globals from object.c
*/
// BAD... EVIL... JUJU...!!!!
// XXX moved here temporarily
float frame_to_float (Scene *scene, int cfra)		/* see also bsystem_time in object.c */
{
	extern float bluroffs;	/* bad stuff borrowed from object.c */
	extern float fieldoffs;
	float ctime;
	
	ctime= (float)cfra;
	ctime+= bluroffs+fieldoffs;
	ctime*= scene->r.framelen;
	
	return ctime;
}

static void scene_update_newframe(Scene *sce, unsigned int lay)
{
	Base *base;
	Object *ob;
	float ctime = frame_to_float(sce, sce->r.cfra); 
	
	if(sce->theDag==NULL)
		DAG_scene_sort(sce);
	
	DAG_scene_update_flags(sce, lay);   // only stuff that moves or needs display still
	
	/* All 'standard' (i.e. without any dependencies) animation is handled here,
	 * with an 'local' to 'macro' order of evaluation. This should ensure that
	 * settings stored nestled within a hierarchy (i.e. settings in a Texture block
	 * can be overridden by settings from Scene, which owns the Texture through a hierarchy 
	 * such as Scene->World->MTex/Texture) can still get correctly overridden.
	 */
	BKE_animsys_evaluate_all_animation(G.main, ctime);
	
	for(base= sce->base.first; base; base= base->next) {
		ob= base->object;
		
		object_handle_update(sce, ob);   // bke_object.h
		
		/* only update layer when an ipo */
			// XXX old animation system
		//if(ob->ipo && has_ipo_code(ob->ipo, OB_LAY) ) {
		//	base->lay= ob->lay;
		//}
	}
}

/* this is called in main loop, doing tagged updates before redraw */
void scene_update_tagged(Scene *scene)
{
	Scene *sce;
	Base *base;
	float ctime = frame_to_float(scene, scene->r.cfra); 

	/* update all objects: drivers, matrices, displists, etc. flags set
	   by depgraph or manual, no layer check here, gets correct flushed */

	/* sets first, we allow per definition current scene to have
	   dependencies on sets, but not the other way around. */
	if(scene->set) {
		for(SETLOOPER(scene->set, base))
			object_handle_update(scene, base->object);
	}
	
	for(base= scene->base.first; base; base= base->next) {
		object_handle_update(scene, base->object);
	}

	/* recalc scene animation data here (for sequencer) */
	{
		AnimData *adt= BKE_animdata_from_id(&scene->id);

		if(adt && (adt->recalc & ADT_RECALC_ANIM))
			BKE_animsys_evaluate_animdata(&scene->id, adt, ctime, 0);
	}

	BKE_ptcache_quick_cache_all(scene);

	/* in the future this should handle updates for all datablocks, not
	   only objects and scenes. - brecht */
}

/* applies changes right away, does all sets too */
void scene_update_for_newframe(Scene *sce, unsigned int lay)
{
	Scene *scene= sce;
	
	/* clear animation overrides */
	// XXX TODO...
	
	/* sets first, we allow per definition current scene to have dependencies on sets */
	for(sce= sce->set; sce; sce= sce->set)
		scene_update_newframe(sce, lay);

	scene_update_newframe(scene, lay);
}

/* return default layer, also used to patch old files */
void scene_add_render_layer(Scene *sce)
{
	SceneRenderLayer *srl;
//	int tot= 1 + BLI_countlist(&sce->r.layers);
	
	srl= MEM_callocN(sizeof(SceneRenderLayer), "new render layer");
	sprintf(srl->name, "RenderLayer");
	BLI_uniquename(&sce->r.layers, srl, "RenderLayer", '.', offsetof(SceneRenderLayer, name), 32);
	BLI_addtail(&sce->r.layers, srl);

	/* note, this is also in render, pipeline.c, to make layer when scenedata doesnt have it */
	srl->lay= (1<<20) -1;
	srl->layflag= 0x7FFF;	/* solid ztra halo edge strand */
	srl->passflag= SCE_PASS_COMBINED|SCE_PASS_Z;
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

