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
#include "BLI_winstuff.h"
#include <io.h>
#endif
#include "MEM_guardedalloc.h"

#include "nla.h"				/* for __NLA : IMPORTANT Do not delete me yet! */
#ifdef __NLA					/* for __NLA : IMPORTANT Do not delete me yet! */
#include "DNA_armature_types.h"	/* for __NLA : IMPORTANT Do not delete me yet! */
#include "BKE_armature.h"		/* for __NLA : IMPORTANT Do not delete me yet! */
#include "BKE_action.h"			/* for __NLA : IMPORTANT Do not delete me yet! */
#endif							/* for __NLA : IMPORTANT Do not delete me yet! */

#include "DNA_constraint_types.h"
#include "DNA_scene_types.h"
#include "DNA_object_types.h"
#include "DNA_scriptlink_types.h"
#include "DNA_meta_types.h"
#include "DNA_ika_types.h"
#include "DNA_lamp_types.h"
#include "DNA_material_types.h"
#include "DNA_group_types.h"
#include "DNA_curve_types.h"
#include "DNA_userdef_types.h"

#include "BLI_blenlib.h"

#include "BKE_bad_level_calls.h"
#include "BKE_utildefines.h"

#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_anim.h"
#include "BKE_constraint.h"

#include "BKE_library.h"

#include "BKE_scene.h"
#include "BKE_world.h"
#include "BKE_ipo.h"
#include "BKE_ika.h"
#include "BKE_key.h"

#include "BPY_extern.h"

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
	if(sce->fcam) MEM_freeN(sce->fcam);
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
}

Scene *add_scene(char *name)
{
	Scene *sce;

	sce= alloc_libblock(&G.main->scene, ID_SCE, name);
	sce->lay= 1;

	sce->r.mode= R_GAMMA;
	sce->r.cfra= 1;
	sce->r.sfra= 1;
	sce->r.efra= 250;
	sce->r.xsch= 320;
	sce->r.ysch= 256;
	sce->r.xasp= 1;
	sce->r.yasp= 1;
	sce->r.xparts= 1;
	sce->r.yparts= 1;
	sce->r.size= 100;
	sce->r.planes= 24;
	sce->r.quality= 90;
	sce->r.framapto= 100;
	sce->r.images= 100;
	sce->r.framelen= 1.0;
	sce->r.frs_sec= 25;

	sce->r.xplay= 640;
	sce->r.yplay= 480;
	sce->r.freqplay= 60;
	sce->r.depth= 32;

	if (sce->r.avicodecdata) {
printf("this is not good\n");
	}
//	sce->r.imtype= R_TARGA;

	sce->r.stereomode = 1;  // no stereo

	strcpy(sce->r.backbuf, "//backbuf");
	strcpy(sce->r.pic, U.renderdir);
	strcpy(sce->r.ftype, "//ftype");
	
	BLI_init_rctf(&sce->r.safety, 0.1f, 0.9f, 0.1f, 0.9f);
	sce->r.osa= 8;
	
	return sce;
}

int object_in_scene(Object *ob, Scene *sce)
{
	Base *base;
	
	base= sce->base.first;
	while(base) {
		if(base->object == ob) return 1;
		base= base->next;
	}
	return 0;
}

void sort_baselist(Scene *sce)
{
	/* in order of parent and track */
	ListBase tempbase, noparentbase, notyetbase;
	Base *base, *test=NULL;
	Object *par;
	int doit, domore= 0, lastdomore=1;
	
	
	/* keep same order when nothing has changed! */
	
	while(domore!=lastdomore) {

		lastdomore= domore;
		domore= 0;
		tempbase.first= tempbase.last= 0;
		noparentbase.first= noparentbase.last= 0;
		notyetbase.first= notyetbase.last= 0;
		
		while( (base= sce->base.first) ) {
			BLI_remlink(&sce->base, base);
			
			par= 0;
			if(base->object->type==OB_IKA) {
				Ika *ika= base->object->data;
				par= ika->parent;
			}

			if(par || base->object->parent || base->object->track) {
				
				doit= 0;
				if(base->object->parent) doit++;
				if(base->object->track) doit++;
				
			/* Count constraints */
				{
					bConstraint *con;
					for (con = base->object->constraints.first; con; con=con->next){
						switch (con->type){
						case CONSTRAINT_TYPE_KINEMATIC:
							{
								bKinematicConstraint *data=con->data;
								if (data->tar) doit++;
							}
							break;
						case CONSTRAINT_TYPE_TRACKTO:
							{
								bTrackToConstraint *data=con->data;
								if (data->tar) doit++;
							}
							break;
						case CONSTRAINT_TYPE_NULL:
							break;
						case CONSTRAINT_TYPE_ROTLIKE:
							{
								bRotateLikeConstraint *data=con->data;
								if (data->tar) doit++;
								
							}
							break;
						case CONSTRAINT_TYPE_LOCLIKE:
							{
								bLocateLikeConstraint *data=con->data;
								if (data->tar) doit++;
							}
							break;
						case CONSTRAINT_TYPE_ACTION:
							{
								bActionConstraint *data=con->data;
								if (data->tar) doit++;
							}
							break;
						default:
							break;
						}
					}
				}
				
				if(par) doit++;
				
				test= tempbase.first;
				while(test) {
					
					if(test->object==base->object->parent) doit--;
					if(test->object==base->object->track) doit--;
					if(test->object==par) doit--;
					
					/* Decrement constraints */
					{
						bConstraint *con;
						for (con = base->object->constraints.first; con; con=con->next){
							switch (con->type){
							case CONSTRAINT_TYPE_KINEMATIC:
								{
									bKinematicConstraint *data=con->data;
									if (test->object == data->tar && test->object!=base->object) doit--;
								}
								break;
							case CONSTRAINT_TYPE_TRACKTO:
								{
									bTrackToConstraint *data=con->data;
									if (test->object == data->tar && test->object!=base->object) doit--;
								}
								break;
							case CONSTRAINT_TYPE_NULL:
								break;
							case CONSTRAINT_TYPE_ROTLIKE:
								{
									bRotateLikeConstraint *data=con->data;
									if (test->object == data->tar && test->object!=base->object) doit--;
									
								}
								break;
							case CONSTRAINT_TYPE_LOCLIKE:
								{
									bLocateLikeConstraint *data=con->data;
									if (test->object == data->tar && test->object!=base->object) doit--;
								}
								break;
							case CONSTRAINT_TYPE_ACTION:
								{
									bActionConstraint *data=con->data;
									if (test->object == data->tar && test->object!=base->object) doit--;
								}
								break;
							default:
								break;
							}
						}
					}
					
					if(doit==0) break;
					test= test->next;
				}
				
				if(test) BLI_insertlink(&tempbase, test, base);
				else {
					BLI_addhead(&tempbase, base);
					domore++;
				}
				
			}
			else BLI_addtail(&noparentbase, base);
			
		}
		sce->base= noparentbase;
		addlisttolist(&sce->base, &tempbase);
		addlisttolist(&sce->base, &notyetbase);

	}
}


void set_scene_bg(Scene *sce)
{
	Base *base;
	Object *ob;
	Group *group;
	GroupObject *go;
	int flag;
	
	G.scene= sce;
	
	/* deselect objects (for dataselect) */
	ob= G.main->object.first;
	while(ob) {
		ob->flag &= ~(SELECT|OB_FROMGROUP);
		ob= ob->id.next;
	}

	/* group flags again */
	group= G.main->group.first;
	while(group) {
		go= group->gobject.first;
		while(go) {
			if(go->ob) go->ob->flag |= OB_FROMGROUP;
			go= go->next;
		}
		group= group->id.next;
	}

	/* sort baselist */
	sort_baselist(sce);

	/* copy layers and flags from bases to objects */
	base= G.scene->base.first;
	while(base) {
		
		base->object->lay= base->lay;
		
		base->flag &= ~OB_FROMGROUP;
		flag= base->object->flag & OB_FROMGROUP;
		base->flag |= flag;
		
		base->object->ctime= -1234567.0;	/* force ipo to be calculated later */
		base= base->next;
	}

	do_all_ipos();	/* layers/materials */

	BPY_do_all_scripts(SCRIPT_FRAMECHANGED);
	do_all_keys();
#ifdef __NLA
	do_all_actions();
	rebuild_all_armature_displists();
#endif
	do_all_ikas();


}

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
	extern ListBase duplilist;
	static Object *dupob;
	static int fase;
	int run_again=1;
	
	/* init */
	if(val==0) {
		fase= F_START;
		dupob= 0;
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
			
			if(*base == 0) fase= F_START;
			else {
				if(fase!=F_DUPLI) {
					if( (*base)->object->transflag & OB_DUPLI) {
						
						make_duplilist(G.scene, (*base)->object);
						dupob= duplilist.first;
						
					}
				}
				/* handle dupli's */
				if(dupob) {
					
					*ob= dupob;
					fase= F_DUPLI;
					
					dupob= dupob->id.next;
				}
				else if(fase==F_DUPLI) {
					fase= F_SCENE;
					free_duplilist();
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
