/**
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
 * Contributor(s): Blender Foundation, 2005. Full recode.
 * Roland Hess, 2007. Visual Key refactor.
 *
 * ***** END GPL LICENSE BLOCK *****
 */


/* this code feels over-complex, mostly because I choose in the past to devise a system
  that converts the Ipo blocks (linked to Object, Material, etc), into a copy of that
  data which is being worked on;  the 'editipo'.
  The editipo then can have 'ipokey' data, which is optimized for editing curves as if
  it were key positions. This is still a great feature to work with, which makes ipo editing
  in Blender still valuable. However, getting this beast under control was hard, even
  for me... (ton) */

#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef WIN32
#include <unistd.h>
#else
#include <io.h>
#endif   
#include "MEM_guardedalloc.h"
#include "PIL_time.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"

#include "DNA_constraint_types.h"
#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_camera_types.h"
#include "DNA_curve_types.h"
#include "DNA_group_types.h"
#include "DNA_ipo_types.h"
#include "DNA_key_types.h"
#include "DNA_lamp_types.h"
#include "DNA_material_types.h"
#include "DNA_object_types.h"
#include "DNA_object_fluidsim.h"
#include "DNA_particle_types.h"
#include "DNA_screen_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"
#include "DNA_sequence_types.h"
#include "DNA_sound_types.h"
#include "DNA_texture_types.h"
#include "DNA_userdef_types.h"
#include "DNA_view3d_types.h"
#include "DNA_world_types.h"

#include "BKE_action.h"
#include "BKE_armature.h"
#include "BKE_anim.h"
#include "BKE_constraint.h"
#include "BKE_depsgraph.h"
#include "BKE_global.h"
#include "BKE_group.h"
#include "BKE_ipo.h"
#include "BKE_key.h"
#include "BKE_material.h"
#include "BKE_particle.h"
#include "BKE_texture.h"
#include "BKE_utildefines.h"
#include "BKE_object.h"

#include "BIF_butspace.h"
#include "BIF_editaction.h"
#include "BIF_editconstraint.h"
#include "BIF_editkey.h"
#include "BIF_editnla.h"
#include "BIF_editseq.h"
#include "BIF_editview.h"
#include "BIF_interface.h"
#include "BIF_mywindow.h"
#include "BIF_poseobject.h"
#include "BIF_screen.h"
#include "BIF_space.h"
#include "BIF_toolbox.h"
#include "BIF_poseobject.h"

#include "BDR_drawobject.h"
#include "BDR_editobject.h"
#include "BDR_editcurve.h" 	// for bezt_compare 

#include "BSE_trans_types.h"
#include "BSE_editipo_types.h"
#include "BSE_drawipo.h"
#include "BSE_editipo.h"
#include "BSE_edit.h"
#include "BSE_drawview.h"
#include "BSE_headerbuttons.h"
#include "BSE_node.h"
#include "BSE_sequence.h"
#include "BSE_seqaudio.h"
#include "BSE_time.h"

#include "blendef.h"
#include "mydevice.h"
#include "transform.h"

extern int ob_ar[];
extern int ma_ar[];
extern int seq_ar[];
extern int cu_ar[];
extern int wo_ar[];
extern int la_ar[];
extern int cam_ar[];
extern int snd_ar[];
extern int ac_ar[];
extern int co_ar[];
extern int te_ar[];
extern int fluidsim_ar[]; // NT
extern int part_ar[];

/* forwards */
#define IPOTHRESH	0.9

/* tests if only one editipo is active */
static void check_active_editipo(void)
{
	EditIpo *ei, *actei;
	int a;
	
	actei= G.sipo->editipo;
	if(actei) {
		for(a=0; a<G.sipo->totipo; a++, actei++) {
			if(actei->flag & IPO_ACTIVE) 
				break;
		}
		if(actei==NULL) {
			/* set visible active */
			for(a=0, ei=G.sipo->editipo; a<G.sipo->totipo; a++, ei++) {
				if(ei->flag & IPO_VISIBLE)
					break;
			}
			if(ei==NULL) ei=G.sipo->editipo;
			ei->flag |= IPO_ACTIVE;
			if(ei->icu) ei->icu->flag |= IPO_ACTIVE;
		}
		else {
			/* make sure no others are active */
			for(a=0, ei=G.sipo->editipo; a<G.sipo->totipo; a++, ei++) {
				if(ei!=actei) {
					ei->flag &= ~IPO_ACTIVE;
					if(ei->icu) ei->icu->flag &= ~IPO_ACTIVE;
				}
			}
		}
	}
}

/* sets this ei channel active */
static void set_active_editipo(EditIpo *actei)
{
	EditIpo *ei;
	int a;
	
	for(a=0, ei=G.sipo->editipo; a<G.sipo->totipo; a++, ei++) {
		ei->flag &= ~IPO_ACTIVE;
		if(ei->icu) ei->icu->flag &= ~IPO_ACTIVE;
	}
	actei->flag |= IPO_ACTIVE;
	if(actei->icu) actei->icu->flag |= IPO_ACTIVE;
}

EditIpo *get_active_editipo(void)
{
	EditIpo *ei;
	int a;
	
	if(G.sipo==NULL)
		return NULL;
	
	/* prevent confusing situations, like for sequencer */
	if(G.sipo->totipo==1) {
		ei= G.sipo->editipo;
		ei->flag |= IPO_ACTIVE;
		return ei;
	}
	for(a=0, ei=G.sipo->editipo; a<G.sipo->totipo; a++, ei++)
		if(ei->flag & IPO_ACTIVE)
			return ei;
	
	return NULL;
}

static void set_active_key(int index)
{
	if(G.sipo->blocktype==ID_KE && G.sipo->from) {
		Object *ob= (Object *)G.sipo->from;
		Key *key= ob_get_key(ob);
		
		if(key) {
			KeyBlock *curkb;
			
			curkb= BLI_findlink(&key->block, index-1);
			if(curkb) {
				ob->shapenr= index;
				ob->shapeflag |= OB_SHAPE_TEMPLOCK;
				
				/* calc keypos */
				DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
				allqueue(REDRAWVIEW3D, 0);
				allqueue(REDRAWBUTSEDIT, 0);
			}
		}
	}			
}

void editipo_changed(SpaceIpo *si, int doredraw)
{
	EditIpo *ei;
	View2D *v2d;
	Key *key;
	KeyBlock *kb;
	int a, first=1;

	ei= si->editipo;
	if(ei==0)
		return;
	
	for(a=0; a<si->totipo; a++, ei++) {
		
		if(ei->icu) {
			
				/* twice because of ittererating new autohandle */
			calchandles_ipocurve(ei->icu);
			calchandles_ipocurve(ei->icu);
			
			if(ei->flag & IPO_VISIBLE) {
		
				boundbox_ipocurve(ei->icu, 0);
				sort_time_ipocurve(ei->icu);
				if(first) {
					si->v2d.tot= ei->icu->totrct;
					first= 0;
				}
				else BLI_union_rctf(&(si->v2d.tot), &(ei->icu->totrct));
			}
		}
	}
	

	v2d= &(si->v2d);	

	/* keylines? */
	if(si->blocktype==ID_KE) {
		key= ob_get_key((Object *)G.sipo->from);
		if(key && key->block.first) {
			kb= key->block.first;
			if(kb->pos < v2d->tot.ymin) v2d->tot.ymin= kb->pos;
			kb= key->block.last;
			if(kb->pos > v2d->tot.ymax) v2d->tot.ymax= kb->pos;
		}
	}
	
	/* is there no curve? */
	if(first) {
		v2d->tot.xmin= 0.0;
		v2d->tot.xmax= EFRA;
		v2d->tot.ymin= (float)-0.1;
		v2d->tot.ymax= (float)1.1;
	
		if(si->blocktype==ID_SEQ) {
			v2d->tot.xmin= -5.0;
			v2d->tot.xmax= 105.0;
			v2d->tot.ymin= (float)-0.1;
			v2d->tot.ymax= (float)1.1;
		}
	}
	
	si->tot= v2d->tot;	
	
	if(doredraw) {
		/* if you always call do_ipo: you get problems with insertkey, for example
		 * when inserting only a 'loc' the 'ob->rot' value then is changed.
		 */

		if(si->blocktype==ID_OB) { 			
				/* clear delta loc,rot,size (when free/delete ipo) */
			clear_delta_obipo(si->ipo);
			
		}
	
		do_ipo(si->ipo);

		allqueue(REDRAWIPO, 0);
		allqueue(REDRAWACTION, 0);
		allqueue(REDRAWTIME, 0);
		allqueue(REDRAWNLA, 0);
		allqueue(REDRAWBUTSOBJECT, 0);
		
		if(si->blocktype==ID_OB) {
			Object *ob= (Object *)si->from;			
			if(ob) DAG_object_flush_update(G.scene, ob, OB_RECALC_OB);
			allqueue(REDRAWVIEW3D, 0);
			allqueue(REDRAWNLA, 0);
		}

		else if(si->blocktype==ID_MA) allqueue(REDRAWBUTSSHADING, 0);
		else if(si->blocktype==ID_TE) allqueue(REDRAWBUTSSHADING, 0);
		else if(si->blocktype==ID_WO) allqueue(REDRAWBUTSSHADING, 0);
		else if(si->blocktype==ID_LA) allqueue(REDRAWBUTSSHADING, 0);
//		else if(si->blocktype==ID_SO) allqueue(REDRAWBUTSSOUND, 0);
		else if(si->blocktype==ID_CA) {
			allqueue(REDRAWBUTSEDIT, 0);
			allqueue(REDRAWVIEW3D, 0);
		}
		else if(si->blocktype==ID_SEQ) free_imbuf_seq_with_ipo(si->ipo);
		else if(si->blocktype==ID_PO) {
			Object *ob= OBACT;
			if(ob && ob->pose) {
				DAG_object_flush_update(G.scene, OBACT, OB_RECALC_DATA);
			}
			allqueue(REDRAWVIEW3D, 0);
			allqueue(REDRAWACTION, 0);
			allqueue(REDRAWNLA, 0);
		}
		else if(si->blocktype==ID_KE) {
			DAG_object_flush_update(G.scene, OBACT, OB_RECALC_DATA);
			allqueue(REDRAWVIEW3D, 0);
		}
		else if(si->blocktype==ID_CU) {
			DAG_object_flush_update(G.scene, OBACT, OB_RECALC_DATA);
			allqueue(REDRAWVIEW3D, 0);
		}
		else if(si->blocktype==ID_PA){
			DAG_object_flush_update(G.scene, OBACT, OB_RECALC_DATA);
			allqueue(REDRAWVIEW3D, 0);
		}
	}

	if(si->showkey) make_ipokey();
	
	if(si->actname[0])
		synchronize_action_strips();
}

void scale_editipo(void)
{
	/* comes from buttons, scale with G.sipo->tot rect */
	
	EditIpo *ei;
	BezTriple *bezt;
	float facx, facy;
	int a, b;	
	
	facx= (G.sipo->tot.xmax-G.sipo->tot.xmin)/(G.sipo->v2d.tot.xmax-G.sipo->v2d.tot.xmin);
	facy= (G.sipo->tot.ymax-G.sipo->tot.ymin)/(G.sipo->v2d.tot.ymax-G.sipo->v2d.tot.ymin);

	ei= G.sipo->editipo;
	if(ei==0) return;
	for(a=0; a<G.sipo->totipo; a++, ei++) {
		if (ISPOIN(ei, flag & IPO_VISIBLE, icu)) {
			bezt= ei->icu->bezt;
			b= ei->icu->totvert;
			while(b--) {
				
				bezt->vec[0][0]= facx*(bezt->vec[0][0] - G.sipo->v2d.tot.xmin) + G.sipo->tot.xmin;
				bezt->vec[1][0]= facx*(bezt->vec[1][0] - G.sipo->v2d.tot.xmin) + G.sipo->tot.xmin;
				bezt->vec[2][0]= facx*(bezt->vec[2][0] - G.sipo->v2d.tot.xmin) + G.sipo->tot.xmin;
			
				bezt->vec[0][1]= facy*(bezt->vec[0][1] - G.sipo->v2d.tot.ymin) + G.sipo->tot.ymin;
				bezt->vec[1][1]= facy*(bezt->vec[1][1] - G.sipo->v2d.tot.ymin) + G.sipo->tot.ymin;
				bezt->vec[2][1]= facy*(bezt->vec[2][1] - G.sipo->v2d.tot.ymin) + G.sipo->tot.ymin;

				bezt++;
			}
		}
	}

	editipo_changed(G.sipo, 1);

	BIF_undo_push("Scale Edit Ipo");
	allqueue(REDRAWNLA, 0);
	allqueue (REDRAWACTION, 0);
	allqueue(REDRAWIPO, 0);
}

static void make_ob_editipo(Object *ob, SpaceIpo *si)
{
	EditIpo *ei;
	int a, len, colipo=0;
	char *name;
	
	if(ob->type==OB_MESH) colipo= 1;

	ei= si->editipo= MEM_callocN(OB_TOTIPO*sizeof(EditIpo), "editipo");
	
	si->totipo= OB_TOTIPO;
	
	for(a=0; a<OB_TOTIPO; a++) {
		name = getname_ob_ei(ob_ar[a], colipo);
		strcpy(ei->name, name);
		ei->adrcode= ob_ar[a];
		
		if ELEM6(ei->adrcode, OB_ROT_X, OB_ROT_Y, OB_ROT_Z, OB_DROT_X, OB_DROT_Y, OB_DROT_Z) ei->disptype= IPO_DISPDEGR;
		else if(ei->adrcode==OB_LAY) ei->disptype= IPO_DISPBITS;
		else if(ei->adrcode==OB_TIME) ei->disptype= IPO_DISPTIME;

		ei->col= ipo_rainbow(a, OB_TOTIPO);

		if(colipo) {
			len= strlen(ei->name);
			if(len) {
				if( ei->name[ len-1 ]=='R') ei->col= 0x5050FF;
				else if( ei->name[ len-1 ]=='G') ei->col= 0x50FF50;
				else if( ei->name[ len-1 ]=='B') ei->col= 0xFF7050;
			}
		}
		
		ei->icu= find_ipocurve(si->ipo, ei->adrcode);
		if(ei->icu) {
			ei->flag= ei->icu->flag;
		}
		
		
		ei++;
	}
	//fprintf(stderr,"FSIMAKE_OPBJ call %d \n", si->totipo);
}

static void make_part_editipo(SpaceIpo *si)
{
	EditIpo *ei;
	int a;
	char *name;
	
	if(si->from==0) return;
	
	ei= si->editipo= MEM_callocN(PART_TOTIPO*sizeof(EditIpo), "editipo");
	
	si->totipo= PART_TOTIPO;
	
	for(a=0; a<PART_TOTIPO; a++) {
		name = getname_part_ei(part_ar[a]);
		strcpy(ei->name, name);
		ei->adrcode= part_ar[a];
		
		//if(ei->adrcode & MA_MAP1) {
		//	ei->adrcode-= MA_MAP1;
		//	ei->adrcode |= texchannel_to_adrcode(si->channel);
		//}
		//else {
		//	if(ei->adrcode==MA_MODE) ei->disptype= IPO_DISPBITS;
		//}
		
		ei->col= ipo_rainbow(a, PART_TOTIPO);
		
		//len= strlen(ei->name);
		//if(len) {
		//	if( ei->name[ len-1 ]=='R') ei->col= 0x5050FF;
		//	else if( ei->name[ len-1 ]=='G') ei->col= 0x50FF50;
		//	else if( ei->name[ len-1 ]=='B') ei->col= 0xFF7050;
		//}
		
		ei->icu= find_ipocurve(si->ipo, ei->adrcode);
		if(ei->icu) {
			ei->flag= ei->icu->flag;
		}
		
		ei++;
	}
}

// copied from make_seq_editipo
static void make_fluidsim_editipo(SpaceIpo *si) // NT
{
	EditIpo *ei;
	int a;
	char *name;
	ei= si->editipo= MEM_callocN(FLUIDSIM_TOTIPO*sizeof(EditIpo), "fluidsim_editipo");
	si->totipo = FLUIDSIM_TOTIPO;
	for(a=0; a<FLUIDSIM_TOTIPO; a++) {
		//fprintf(stderr,"FSINAME %d %d \n",a,fluidsim_ar[a], (int)(getname_fluidsim_ei(fluidsim_ar[a]))  );
		name = getname_fluidsim_ei(fluidsim_ar[a]);
		strcpy(ei->name, name);
		ei->adrcode= fluidsim_ar[a];
		ei->col= ipo_rainbow(a, FLUIDSIM_TOTIPO);
		ei->icu= find_ipocurve(si->ipo, ei->adrcode);
		if(ei->icu) {
			ei->flag = ei->icu->flag;
		} 
		//else { ei->flag |= IPO_VISIBLE; }
		//fprintf(stderr,"FSIMAKE eif%d,icuf%d icu%d %d|%d\n", ei->flag,ei->icu->flag, (int)ei->icu, IPO_VISIBLE,IPO_SELECT);
		//fprintf(stderr,"FSIMAKE eif%d icu%d %d|%d\n", ei->flag, (int)ei->icu, IPO_VISIBLE,IPO_SELECT);
		ei++;
	}
}

static void make_seq_editipo(SpaceIpo *si)
{
	EditIpo *ei;
	int a;
	char *name;
	
	ei= si->editipo= MEM_callocN(SEQ_TOTIPO*sizeof(EditIpo), "editipo");
	
	si->totipo= SEQ_TOTIPO;
	
	
	for(a=0; a<SEQ_TOTIPO; a++) {
		name = getname_seq_ei(seq_ar[a]);
		strcpy(ei->name, name);
		ei->adrcode= seq_ar[a];
		
		ei->col= ipo_rainbow(a, SEQ_TOTIPO);
		
		ei->icu= find_ipocurve(si->ipo, ei->adrcode);
		if(ei->icu) {
			ei->flag= ei->icu->flag;
		}
		else ei->flag |= IPO_VISIBLE;
		
		ei++;
	}
}

static void make_cu_editipo(SpaceIpo *si)
{
	EditIpo *ei;
	int a;
	char *name;
	
	ei= si->editipo= MEM_callocN(CU_TOTIPO*sizeof(EditIpo), "editipo");
	
	si->totipo= CU_TOTIPO;
	
	
	for(a=0; a<CU_TOTIPO; a++) {
		name = getname_cu_ei(cu_ar[a]);
		strcpy(ei->name, name);
		ei->adrcode= cu_ar[a];
		
		ei->col= ipo_rainbow(a, CU_TOTIPO);
		
		ei->icu= find_ipocurve(si->ipo, ei->adrcode);
		if(ei->icu) {
			ei->flag= ei->icu->flag;
		}
		else ei->flag |= IPO_VISIBLE;
		 
		ei++;
	}
}

static void make_key_editipo(SpaceIpo *si)
{
	Key *key;
	KeyBlock *kb=NULL;
	EditIpo *ei;
	int a;
	
	key= ob_get_key((Object *)G.sipo->from);
	if(key==NULL) return;
	
	si->totipo= BLI_countlist(&key->block);
	ei= si->editipo= MEM_callocN(si->totipo*sizeof(EditIpo), "editipo");
	
	for(a=0, kb= key->block.first; a<si->totipo; a++, ei++, kb= kb->next) {
		
		if(kb->name[0] != 0) strncpy(ei->name, kb->name, 31);	// length both same
		ei->adrcode= kb->adrcode;
		
		ei->col= ipo_rainbow(a, KEY_TOTIPO);
		
		ei->icu= find_ipocurve(si->ipo, ei->adrcode);
		if(ei->icu) {
			ei->flag= ei->icu->flag;
		}
		else if(a==0) 
			if(key && key->type==KEY_NORMAL)
				ei->flag |= IPO_VISIBLE;
		
		/* active ipo is tied to active shape  */
		{
			Object *ob= OBACT;
			if(a==ob->shapenr-1)
				set_active_editipo(ei);
		}
	}
	
	ei= si->editipo;
	if(key && key->type==KEY_RELATIVE) {
		strcpy(ei->name, "----");
	}
	else {
		ei->flag |= IPO_VISIBLE;
	}
}

static void make_mat_editipo(SpaceIpo *si)
{
	EditIpo *ei;
	int a, len;
	char *name;
	
	if(si->from==0) return;
	
	ei= si->editipo= MEM_callocN(MA_TOTIPO*sizeof(EditIpo), "editipo");
	
	si->totipo= MA_TOTIPO;
	
	for(a=0; a<MA_TOTIPO; a++) {
		name = getname_mat_ei(ma_ar[a]);
		strcpy(ei->name, name);
		ei->adrcode= ma_ar[a];
		
		if(ei->adrcode & MA_MAP1) {
			ei->adrcode-= MA_MAP1;
			ei->adrcode |= texchannel_to_adrcode(si->channel);
		}
		else {
			if(ei->adrcode==MA_MODE) ei->disptype= IPO_DISPBITS;
		}
		
		ei->col= ipo_rainbow(a, MA_TOTIPO);
		
		len= strlen(ei->name);
		if(len) {
			if( ei->name[ len-1 ]=='R') ei->col= 0x5050FF;
			else if( ei->name[ len-1 ]=='G') ei->col= 0x50FF50;
			else if( ei->name[ len-1 ]=='B') ei->col= 0xFF7050;
		}
		
		ei->icu= find_ipocurve(si->ipo, ei->adrcode);
		if(ei->icu) {
			ei->flag= ei->icu->flag;
		}
		
		ei++;
	}
}

static void make_texture_editipo(SpaceIpo *si)
{
	EditIpo *ei;
	int a, len;
	char *name;
	
	if(si->from==0) return;    
	
	ei= si->editipo= MEM_callocN(TE_TOTIPO*sizeof(EditIpo), "editipo");
	
	si->totipo= TE_TOTIPO;
	
	for(a=0; a<TE_TOTIPO; a++) {
		name = getname_tex_ei(te_ar[a]);
		strcpy(ei->name, name);
			ei->adrcode= te_ar[a];

		ei->col= ipo_rainbow(a, TE_TOTIPO);
	
		len= strlen(ei->name);
		if(len) {
			if( ei->name[ len-1 ]=='R') ei->col= 0x5050FF;
			else if( ei->name[ len-1 ]=='G') ei->col= 0x50FF50;
			else if( ei->name[ len-1 ]=='B') ei->col= 0xFF7050;
		}	
		ei->icu= find_ipocurve(si->ipo, ei->adrcode);
		if(ei->icu) {
			ei->flag= ei->icu->flag;
		}
		
		ei++;
	}
}

static void make_world_editipo(SpaceIpo *si)
{
	EditIpo *ei;
	int a, len;
	char *name;
	
	if(si->from==0) return;
	
	ei= si->editipo= MEM_callocN(WO_TOTIPO*sizeof(EditIpo), "editipo");
	
	si->totipo= WO_TOTIPO;
	
	for(a=0; a<WO_TOTIPO; a++) {
		name = getname_world_ei(wo_ar[a]);
		
		strcpy(ei->name, name); 
		ei->adrcode= wo_ar[a];
		
		if(ei->adrcode & MA_MAP1) {
			ei->adrcode-= MA_MAP1;
			ei->adrcode |= texchannel_to_adrcode(si->channel);
		}
		else {
			if(ei->adrcode==MA_MODE) ei->disptype= IPO_DISPBITS;
		}
		
		ei->col= ipo_rainbow(a, WO_TOTIPO);
		
		len= strlen(ei->name);
		if(len) {
			if( ei->name[ len-1 ]=='R') ei->col= 0x5050FF;
			else if( ei->name[ len-1 ]=='G') ei->col= 0x50FF50;
			else if( ei->name[ len-1 ]=='B') ei->col= 0xFF7050;
		}
		
		ei->icu= find_ipocurve(si->ipo, ei->adrcode);
		if(ei->icu) {
			ei->flag= ei->icu->flag;
		}
		
		ei++;
	}
}

static void make_lamp_editipo(SpaceIpo *si)
{
	EditIpo *ei;
	int a;
	char *name;
	
	ei= si->editipo= MEM_callocN(LA_TOTIPO*sizeof(EditIpo), "editipo");
	
	si->totipo= LA_TOTIPO;
	
	for(a=0; a<LA_TOTIPO; a++) {
		name = getname_la_ei(la_ar[a]);
		strcpy(ei->name, name);
		ei->adrcode= la_ar[a];

		if(ei->adrcode & MA_MAP1) {
			ei->adrcode-= MA_MAP1;
			ei->adrcode |= texchannel_to_adrcode(si->channel);
		}

		ei->col= ipo_rainbow(a, LA_TOTIPO);
		
		ei->icu= find_ipocurve(si->ipo, ei->adrcode);
		if(ei->icu) {
			ei->flag= ei->icu->flag;
		}
		
		ei++;
	}
}

static void make_camera_editipo(SpaceIpo *si)
{
	EditIpo *ei;
	int a;
	char *name;
	
	ei= si->editipo= MEM_callocN(CAM_TOTIPO*sizeof(EditIpo), "editipo");
	
	si->totipo= CAM_TOTIPO;
	
	
	for(a=0; a<CAM_TOTIPO; a++) {
		name = getname_cam_ei(cam_ar[a]);
		strcpy(ei->name, name);
		ei->adrcode= cam_ar[a];

		ei->col= ipo_rainbow(a, CAM_TOTIPO);
		
		ei->icu= find_ipocurve(si->ipo, ei->adrcode);
		if(ei->icu) {
			ei->flag= ei->icu->flag;
		}
		
		ei++;
	}
}

static int make_constraint_editipo(Ipo *ipo, EditIpo **si)
{
	EditIpo *ei;
	int a;
	char *name;
	
	ei= *si= MEM_callocN(CO_TOTIPO*sizeof(EditIpo), "editipo");
	
	for(a=0; a<CO_TOTIPO; a++) {
		name = getname_co_ei(co_ar[a]);
		strcpy(ei->name, name);
		ei->adrcode= co_ar[a];

		ei->col= ipo_rainbow(a, CO_TOTIPO);
		
		ei->icu= find_ipocurve(ipo, ei->adrcode);
		if(ei->icu) {
			ei->flag= ei->icu->flag;
		}
		
		ei++;
	}

	return CO_TOTIPO;
}

static int make_bone_editipo(Ipo *ipo, EditIpo **si)
{
	EditIpo *ei;
	int a;
	char *name;
	
	ei= *si= MEM_callocN(AC_TOTIPO*sizeof(EditIpo), "editipo");
	
	for(a=0; a<AC_TOTIPO; a++) {
		name = getname_ac_ei(ac_ar[a]);
		strcpy(ei->name, name);
		ei->adrcode= ac_ar[a];

		ei->col= ipo_rainbow(a, AC_TOTIPO);
		
		ei->icu= find_ipocurve(ipo, ei->adrcode);
		if(ei->icu) {
			ei->flag= ei->icu->flag;
		}
		
		ei++;
	}

	return AC_TOTIPO;
}

static void make_sound_editipo(SpaceIpo *si)
{
	EditIpo *ei;
	int a;
	char *name;
	
	ei= si->editipo= MEM_callocN(SND_TOTIPO*sizeof(EditIpo), "editipo");
	
	si->totipo= SND_TOTIPO;
	
	
	for(a=0; a<SND_TOTIPO; a++) {
		name = getname_snd_ei(snd_ar[a]);
		strcpy(ei->name, name);
		ei->adrcode= snd_ar[a];

		ei->col= ipo_rainbow(a, SND_TOTIPO);
		
		ei->icu= find_ipocurve(si->ipo, ei->adrcode);
		if(ei->icu) {
			ei->flag= ei->icu->flag;
		}
		
		ei++;
	}
}

/* only called in test_editipo() below */
static void make_editipo(void)
{
	EditIpo *ei;
	Object *ob;
	rctf *rf;
	int a;

	if(G.sipo->editipo)
		MEM_freeN(G.sipo->editipo);
	
	G.sipo->editipo= NULL;
	G.sipo->totipo= 0;
	
	if(G.sipo->from==NULL) return;
	
	ob= OBACT;

	if(G.sipo->ipo) G.sipo->showkey= G.sipo->ipo->showkey;

	if(G.sipo->blocktype==ID_SEQ) {
		make_seq_editipo(G.sipo);
	}
	else if(G.sipo->blocktype==ID_WO) {
		make_world_editipo(G.sipo);
	} 
	else if(G.sipo->blocktype==ID_OB) {
		if (ob) {
			ob->ipowin= ID_OB;
			make_ob_editipo(ob, G.sipo);
		}
	}
	else if(G.sipo->blocktype==ID_MA) {
		if (ob) {
			ob->ipowin= ID_MA;
			make_mat_editipo(G.sipo);
		}
	}
	else if(G.sipo->blocktype==ID_CU) {
		if (ob) {
			ob->ipowin= ID_CU;
			make_cu_editipo(G.sipo);
		}
	}
	else if(G.sipo->blocktype==ID_KE) {
		if (ob) {
			ob->ipowin= ID_KE;
			make_key_editipo(G.sipo);
		}
	}
	else if(G.sipo->blocktype==ID_LA) {
		if (ob) {
			ob->ipowin= ID_LA;
			make_lamp_editipo(G.sipo);
		}
	}
	else if(G.sipo->blocktype==ID_TE) {
		if (ob) {
			ob->ipowin= ID_TE;
			make_texture_editipo(G.sipo);
		}
	}
	else if(G.sipo->blocktype==ID_CA) {
		if (ob) {
			ob->ipowin= ID_CA;
			make_camera_editipo(G.sipo);
		}
	}
	else if(G.sipo->blocktype==ID_SO) {
		if (ob) {
			ob->ipowin= ID_SO;
			make_sound_editipo(G.sipo);
		}
	}
	else if(G.sipo->blocktype==ID_CO){
		G.sipo->totipo = make_constraint_editipo(G.sipo->ipo, (EditIpo**)&G.sipo->editipo);
		if (ob) {
			ob->ipowin= ID_CO;
		}
	}
	else if(G.sipo->blocktype==ID_PO) {

		G.sipo->totipo = make_bone_editipo(G.sipo->ipo, (EditIpo**)&G.sipo->editipo);
		if (ob) {
			ob->ipowin= ID_PO;
		}
	}
	else if(G.sipo->blocktype==ID_FLUIDSIM) {
		if (ob) { // NT
			ob->ipowin= ID_FLUIDSIM;
			make_fluidsim_editipo(G.sipo);
		}
	}
	else if(G.sipo->blocktype==ID_PA) {
		if (ob) {
			ob->ipowin= ID_PA;
			make_part_editipo(G.sipo);
		}
	}

	if(G.sipo->editipo==0) return;
	
	ei= G.sipo->editipo;
	for(a=0; a<G.sipo->totipo; a++, ei++) {
		if(ei->icu) ei->icu->flag= ei->flag;
	}
	editipo_changed(G.sipo, 0);
	
	/* sets globals, bad stuff but we need these variables in other parts of code */
	get_status_editipo();
	
	
	if (G.sipo->flag & SIPO_LOCK_VIEW) {
		rf= &(G.v2d->cur); /* view is locked, dont move it, just to sanity check */
		if(rf->xmin>=rf->xmax || rf->ymin>=rf->ymax) ipo_default_v2d_cur(G.sipo->blocktype, &G.v2d->cur);
	} else {
		if(G.sipo->ipo) {
			if (G.sipo->pin)
				rf= &(G.sipo->v2d.cur);
			else
				rf= &(G.sipo->ipo->cur);
			
			if(rf->xmin<rf->xmax && rf->ymin<rf->ymax) G.v2d->cur= *rf;
			else ipo_default_v2d_cur(G.sipo->blocktype, &G.v2d->cur);
		}
		else {
			ipo_default_v2d_cur(G.sipo->blocktype, &G.v2d->cur);
		}
	}
	view2d_do_locks(curarea, V2D_LOCK_COPY);
}

/* evaluates context in the current UI */
/* blocktype is type of ipo */
/* from is the base pointer to find data to change (ob in case of action or pose) */
/* bonename is for local bone ipos (constraint only now) */
static void get_ipo_context(short blocktype, ID **from, Ipo **ipo, char *actname, char *constname, char *bonename)
{
	Object *ob= OBACT;
	
	*from= NULL;
	*ipo= NULL;
	
	if(blocktype==ID_CO) {
		if (ob) {
			bConstraintChannel *chan;
			bConstraint *con= get_active_constraint(ob);
			
			if(con) {
				*from= &ob->id;
				
				BLI_strncpy(constname, con->name, 32);
				
				/* a bit hackish, but we want con->ipo to work */
				if(con->flag & CONSTRAINT_OWN_IPO) {
					if(ob->flag & OB_POSEMODE) {
						bPoseChannel *pchan= get_active_posechannel(ob);
						if(pchan) {
							BLI_strncpy(bonename, pchan->name, 32);
							*ipo= con->ipo;
						}
					}
				}
				else {
					chan= get_active_constraint_channel(ob);
					if(chan) {
						*ipo= chan->ipo;
						BLI_strncpy(constname, con->name, 32);
					}
					
					/* set actname if in posemode */
					if (ob->action) {
						if (ob->flag & OB_POSEMODE) {
							bPoseChannel *pchan= get_active_posechannel(ob);
							if (pchan) {
								BLI_strncpy(actname, pchan->name, 32);
								BLI_strncpy(bonename, pchan->name, 32);
							}
						}
						else if (ob->ipoflag & OB_ACTION_OB)
							strcpy(actname, "Object");
					}
					else {
						if (ob->flag & OB_POSEMODE) {
							bPoseChannel *pchan= get_active_posechannel(ob);
							if (pchan) {
								BLI_strncpy(actname, pchan->name, 32);
								BLI_strncpy(bonename, pchan->name, 32);
							}
						}
					}
				}
			}
		}
	}
	else if(blocktype==ID_PO) {
		if (ob && ob->action && ob->type==OB_ARMATURE) {
			bPoseChannel *pchan= get_active_posechannel(ob);
			
			*from= (ID *)ob;
			if (pchan) {
				bActionChannel *achan;
				
				BLI_strncpy(actname, pchan->name, 32);	/* also set when no channel yet */
				
				achan= get_action_channel(ob->action, pchan->name);
				if(achan)
					*ipo= achan->ipo;
			}
		} 
		
	}
	else if(blocktype==ID_OB) {
		if(ob) {
			*from= (ID *)ob;
			if(ob->ipoflag & OB_ACTION_OB) {
				if (ob->action) {
					bActionChannel *achan= get_action_channel(ob->action, "Object");
					if(achan) {
						*ipo= achan->ipo;
						BLI_strncpy(actname, achan->name, 32);
					}
				}
			}
			else {
				*ipo= ob->ipo;
			}
		}
	}
	else if(blocktype==ID_SEQ) {
		Sequence *last_seq = get_last_seq();
		
		if(last_seq) {
			*from= (ID *)last_seq;
			*ipo= last_seq->ipo;
		}
	}
	else if(blocktype==ID_WO) {
		World *wo= G.scene->world;
		*from= (ID *)wo;
		if(wo) *ipo= wo->ipo;
	}
	else if(blocktype==ID_TE) {
		if(ob) {
			Tex *tex= give_current_texture(ob, ob->actcol);
			*from= (ID *)tex;
			if(tex) *ipo= tex->ipo;
		}
	}
	else if(blocktype==ID_MA) {
		if(ob) {
			Material *ma= give_current_material(ob, ob->actcol);
			ma= editnode_get_active_material(ma);
			*from= (ID *)ma;
			if(ma) *ipo= ma->ipo;
		}
	}
	else if(blocktype==ID_KE) {
		if(ob) {
			Key *key= ob_get_key(ob);
			
			if(ob->ipoflag & OB_ACTION_KEY) {
				if (ob->action) {
					bActionChannel *achan= get_action_channel(ob->action, "Shape");
					if(achan) {
						*ipo= achan->ipo;
						BLI_strncpy(actname, achan->name, 32);
					}
				}
			}
			else if(key) *ipo= key->ipo;
			
			*from= (ID *)ob;
		}
	}
	else if(blocktype==ID_CU) {
		if(ob && ob->type==OB_CURVE) {
			Curve *cu= ob->data;
			*from= (ID *)cu;
			*ipo= cu->ipo;
		}
	}
	else if(blocktype==ID_LA) {
		if(ob && ob->type==OB_LAMP) {
			Lamp *la= ob->data;
			*from= (ID *)la;
			*ipo= la->ipo;
		}
	}
	else if(blocktype==ID_CA) {
		if(ob && ob->type==OB_CAMERA) {
			Camera *ca= ob->data;
			*from= (ID *)ca;
			if(ca) *ipo= ca->ipo;
		}
	}
	else if(blocktype==ID_SO) {
		
		//		if (G.buts && G.buts->mainb == BUTS_SOUND) {
		//			bSound *sound = G.buts->lockpoin;
		//			*from= (ID *)sound;
		//			if(sound) *ipo= sound->ipo;
		//		}
	}
	else if(blocktype==ID_FLUIDSIM) {
		if(ob && ( ob->fluidsimFlag & OB_FLUIDSIM_ENABLE)) {
			FluidsimSettings *fss= ob->fluidsimSettings;
			*from= (ID *)ob;
			if(fss) *ipo= fss->ipo;
		}
	}
	else if(blocktype==ID_PA) {
		ParticleSystem *psys = psys_get_current(ob);
		if(psys){
			*from= (ID *)ob;
			*ipo= psys->part->ipo;
		}
	}
}

/* called on each redraw, check if editipo data has to be remade */
/* if doit already set, it always makes (in case no ipo exists, we need to see the channels */
void test_editipo(int doit)
{
	
	if(G.sipo->pin==0) {
		Ipo *ipo;
		ID *from;
		char actname[32]="", constname[32]="", bonename[32]="";
		
		get_ipo_context(G.sipo->blocktype, &from, &ipo, actname, constname, bonename);
		
		if(G.sipo->ipo != ipo) {
			G.sipo->ipo= ipo;
			/* if lock we don't copy from ipo, this makes the UI jump around confusingly */
			if(G.v2d->flag & V2D_VIEWLOCK || G.sipo->flag & SIPO_LOCK_VIEW);
			else if(ipo) G.v2d->cur= ipo->cur;
			doit= 1;
		}
		if(G.sipo->from != from) {
			G.sipo->from= from;
			doit= 1;
		}
		if( strcmp(G.sipo->actname, actname)) {
			BLI_strncpy(G.sipo->actname, actname, 32);
			doit= 1;
		}
		if( strcmp(G.sipo->constname, constname)) {
			BLI_strncpy(G.sipo->constname, constname, 32);
			doit= 1;
		}
		if( strcmp(G.sipo->bonename, bonename)) {
			BLI_strncpy(G.sipo->bonename, bonename, 32);
			/* urmf; if bonename, then no action */
			if(bonename[0]) G.sipo->actname[0]= 0;
			doit= 1;
		}
		
		if(G.sipo->ipo)
			G.sipo->ipo->cur = G.v2d->cur;
		
	}
		
	if(G.sipo->editipo==NULL || doit) {
		make_editipo();
	}
}

/* ****************** EditIpo ************************ */

int totipo_edit=0, totipo_sel=0, totipo_curve=0, totipo_vis=0, totipo_vert=0, totipo_vertsel=0, totipo_key=0, totipo_keysel=0;

void get_status_editipo(void)
{
	EditIpo *ei;
	IpoKey *ik;
	BezTriple *bezt;
	int a, b;
	
	totipo_vis= 0;
	totipo_curve= 0;
	totipo_sel= 0;
	totipo_edit= 0;
	totipo_vert= 0;
	totipo_vertsel= 0;
	totipo_key= 0;
	totipo_keysel= 0;
	
	if(G.sipo->ipo && G.sipo->ipo->id.lib) return;
	
	ei= G.sipo->editipo;
	if(ei==0) return;
	for(a=0; a<G.sipo->totipo; a++) {
		if( ei->flag & IPO_VISIBLE ) {
			totipo_vis++;
			if(ei->flag & IPO_SELECT) totipo_sel++;
			if(ei->icu && ei->icu->totvert) totipo_curve++;
			if(G.sipo->showkey || (ei->flag & IPO_EDIT)) {
				
				/* if showkey: do count the vertices (for grab) */
				if(G.sipo->showkey==0) totipo_edit++;
				
				if(ei->icu) {
					if(ei->icu->bezt) {
						bezt= ei->icu->bezt;
						b= ei->icu->totvert;
						while(b--) {
							if(ei->icu->ipo==IPO_BEZ) {
								if(bezt->f1 & SELECT) totipo_vertsel++;
								if(bezt->f3 & SELECT) totipo_vertsel++;
								totipo_vert+= 2;
							}
							if(bezt->f2 & SELECT) totipo_vertsel++;
							
							totipo_vert++;
							bezt++;
						}
					}
				}
			}
		}
		ei++;
	}
	
	if(G.sipo->showkey) {
		ik= G.sipo->ipokey.first;
		while(ik) {
			totipo_key++;
			if(ik->flag & 1) totipo_keysel++;
			ik= ik->next;
		}
	}
}

/* synchronize editipo flag with icu flag and ipokey flags */
void update_editipo_flags(void)
{
	EditIpo *ei;
	IpoKey *ik;
	int a;
	
	ei= G.sipo->editipo;
	if(ei) {
		for(a=0; a<G.sipo->totipo; a++, ei++) {
			if(ei->icu) ei->icu->flag= ei->flag;
		}
	}
	if(G.sipo->showkey) {
		ik= G.sipo->ipokey.first;
		while(ik) {
			for(a=0; a<G.sipo->totipo; a++) {
				if(ik->data[a]) {
					if(ik->flag & 1) {
						ik->data[a]->f1 |= SELECT;
						ik->data[a]->f2 |= SELECT;
						ik->data[a]->f3 |= SELECT;
					}
					else {
						ik->data[a]->f1 &= ~SELECT;
						ik->data[a]->f2 &= ~SELECT;
						ik->data[a]->f3 &= ~SELECT;
					}
				}
			}
			ik= ik->next;
		}
	}
}

/* sort of enter/leave editmode for curves */
void set_editflag_editipo(void)
{
	EditIpo *ei;
	int a; /*  , tot= 0, ok= 0; */
	
	/* after showkey immediately go to editing of selected points */
	if(G.sipo->showkey) {
		G.sipo->showkey= 0;
		if(G.sipo->ipo) G.sipo->ipo->showkey= 0;
		ei= G.sipo->editipo;
		for(a=0; a<G.sipo->totipo; a++, ei++) ei->flag |= IPO_SELECT;
		scrarea_queue_headredraw(curarea);
		allqueue(REDRAWVIEW3D, 0);
	}
	
	get_status_editipo();
	
	if(G.sipo->ipo && G.sipo->ipo->id.lib) return;
	
	ei= G.sipo->editipo;
	for(a=0; a<G.sipo->totipo; a++, ei++) {		
		if(ei->icu) {
			if(ei->flag & IPO_VISIBLE) {
				
				if(totipo_edit==0 && (ei->flag & IPO_SELECT)) {
					ei->flag |= IPO_EDIT;
					ei->icu->flag= ei->flag;
				}
				else if(totipo_edit && (ei->flag & IPO_EDIT)) {
					ei->flag -= IPO_EDIT;
					ei->icu->flag= ei->flag;
				}
				else if(totipo_vis==1) {
					if(ei->flag & IPO_EDIT) ei->flag -= IPO_EDIT;
					else ei->flag |= IPO_EDIT;
					ei->icu->flag= ei->flag;
				}
			}
		}
	}
	scrarea_queue_headredraw(curarea);
	scrarea_queue_winredraw(curarea);
}

static short findnearest_ipovert(IpoCurve **icu, BezTriple **bezt)
{
	/* selected verts get a disadvantage */
	/* in icu and (bezt or bp) the nearest is written */
	/* return 0 1 2: handlepunt */
	EditIpo *ei;
	BezTriple *bezt1;
	int dist= 100, temp, a, b;
	short mval[2], hpoint=0, sco[3][2];

	*icu= 0;
	*bezt= 0;

	getmouseco_areawin(mval);

	ei= G.sipo->editipo;
	for(a=0; a<G.sipo->totipo; a++, ei++) {
		if (ISPOIN3(ei, flag & IPO_VISIBLE, flag & IPO_EDIT, icu)) {
			
			if(ei->icu->bezt) {
				bezt1= ei->icu->bezt;
				b= ei->icu->totvert;
				while(b--) {

					ipoco_to_areaco_noclip(G.v2d, bezt1->vec[0], sco[0]);
					ipoco_to_areaco_noclip(G.v2d, bezt1->vec[1], sco[1]);
					ipoco_to_areaco_noclip(G.v2d, bezt1->vec[2], sco[2]);
										
					if(ei->disptype==IPO_DISPBITS) {
						temp= abs(mval[0]- sco[1][0]);
					}
					else temp= abs(mval[0]- sco[1][0])+ abs(mval[1]- sco[1][1]);

					if( bezt1->f2 & SELECT) temp+=5;
					if(temp<dist) { 
						hpoint= 1; 
						*bezt= bezt1; 
						dist= temp; 
						*icu= ei->icu; 
					}
					
					if(ei->disptype!=IPO_DISPBITS && ei->icu->ipo==IPO_BEZ) {
						/* middle points get an advantage */
						temp= -3+abs(mval[0]- sco[0][0])+ abs(mval[1]- sco[0][1]);
						if( bezt1->f1 & 1) temp+=5;
						if(temp<dist) { 
							hpoint= 0; 
							*bezt= bezt1; 
							dist= temp; 
							*icu= ei->icu; 
						}
		
						temp= abs(mval[0]- sco[2][0])+ abs(mval[1]- sco[2][1]);
						if( bezt1->f3 & 1) temp+=5;
						if(temp<dist) { 
							hpoint= 2; 
							*bezt=bezt1; 
							dist= temp; 
							*icu= ei->icu; 
						}
					}
					bezt1++;
				}
			}
		}
	}

	return hpoint;
}

void mouse_select_ipo(void)
{
	Object *ob;
	KeyBlock *actkb=NULL;
	EditIpo *ei, *actei= 0;
	IpoCurve *icu;
	IpoKey *ik, *actik;
	BezTriple *bezt;
	TimeMarker *marker;
	float x, y, dist, mindist;
	int a, oldflag = 0, hand, ok;
	short mval[2], xo, yo;
	
	if(G.sipo->editipo==0) return;
	
	get_status_editipo();
	marker=find_nearest_marker(SCE_MARKERS, 1);
	
	/* map ipo-points for editing if scaled ipo */
	if (NLA_IPO_SCALED) {
		actstrip_map_ipo_keys(OBACT, G.sipo->ipo, 0, 0);
	}
	
	if(G.sipo->showkey) {
		float pixelwidth;
		
		view2d_getscale(G.v2d, &pixelwidth, NULL);
		
		getmouseco_areawin(mval);
		areamouseco_to_ipoco(G.v2d, mval, &x, &y);
		actik= 0;
		mindist= 1000.0;
		ik= G.sipo->ipokey.first;
		while(ik) {
			dist= (float)(fabs(ik->val-x));
			if(ik->flag & SELECT) dist+= pixelwidth;
			if(dist < mindist) {
				actik= ik;
				mindist= dist;
			}
			ik= ik->next;
		}
		if(actik) {
			oldflag= actik->flag;
			
			if(G.qual & LR_SHIFTKEY);
			else deselectall_editipo();
			
			if(G.qual & LR_SHIFTKEY) {
				if(oldflag & 1) actik->flag &= ~1;
				else actik->flag |= 1;
			}
			else {
				actik->flag |= 1;
			}
		}
	}
	else if(totipo_edit) {
		
		hand= findnearest_ipovert(&icu, &bezt);
		
		if(G.qual & LR_SHIFTKEY) {
			if(bezt) {
				if(hand==1) {
					if(BEZSELECTED(bezt)) {
						bezt->f1= bezt->f2= bezt->f3= 0;
					}
					else {
						bezt->f1= bezt->f2= bezt->f3= SELECT;
					}
				}
				else if(hand==0) {
					if(bezt->f1 & SELECT) bezt->f1= 0;
					else bezt->f1= SELECT;
				}
				else {
					if(bezt->f3 & SELECT) bezt->f3= 0;
					else bezt->f3= SELECT;
				}
			}				
		}
		else {
			deselectall_editipo();
			
			if(bezt) {
				if(hand==1) {
					bezt->f1|= SELECT; bezt->f2|= SELECT; bezt->f3|= SELECT;
				}
				else if(hand==0) bezt->f1 |= SELECT;
				else bezt->f3 |= SELECT;
			}
		}
	}
	else if (marker) {
		/* select timeline marker */
		if ((G.qual & LR_SHIFTKEY)==0) {
			oldflag= marker->flag;
			deselect_markers(0, 0);
			
			if (oldflag & SELECT)
				marker->flag &= ~SELECT;
			else
				marker->flag |= SELECT;
		}
		else {
			marker->flag |= SELECT;				
		}		
	}
	else {
		
		/* vertex keys ? */
		if(G.sipo->blocktype==ID_KE && G.sipo->from) {
			Key *key;
			KeyBlock *kb, *curkb;
			int i, index= 1;
			
			ob= (Object *)G.sipo->from;
			key= ob_get_key(ob);
			curkb= BLI_findlink(&key->block, ob->shapenr-1);
			
			ei= G.sipo->editipo;
			if(key->type==KEY_NORMAL || (ei->flag & IPO_VISIBLE)) {
				getmouseco_areawin(mval);
				
				areamouseco_to_ipoco(G.v2d, mval, &x, &y);
				/* how much is 20 pixels? */
				mindist= (float)(20.0*(G.v2d->cur.ymax-G.v2d->cur.ymin)/(float)curarea->winy);
				
				for(i=1, kb= key->block.first; kb; kb= kb->next, i++) {
					dist= (float)(fabs(kb->pos-y));
					if(kb==curkb) dist+= (float)0.01;
					if(dist < mindist) {
						actkb= kb;
						mindist= dist;
						index= i;
					}
				}
				if(actkb) {
					ok= TRUE;
					if(G.obedit && actkb!=curkb) {
						ok= okee("Copy key after leaving Edit Mode");
					}
					if(ok) {
						/* also does all keypos */
						deselectall_editipo();
						set_active_key(index);
						set_active_editipo(ei+index-1);
					}
				}
			}
		}
		
		/* select curve */
		if(actkb==NULL) {
			if(totipo_vis==1) {
				ei= G.sipo->editipo;
				for(a=0; a<G.sipo->totipo; a++, ei++) {
					if(ei->icu) {
						if(ei->flag & IPO_VISIBLE) actei= ei;
					}
				}
			}
			else if(totipo_vis>1) {
				actei= select_proj_ipo(0, 0);
			}
			
			if(actei) oldflag= actei->flag;
			
			if(G.qual & LR_SHIFTKEY);
			else deselectall_editipo();
			
			if(actei) {
				if(G.qual & LR_SHIFTKEY) {
					if(oldflag & IPO_SELECT) actei->flag &= ~IPO_SELECT;
					else actei->flag |= IPO_SELECT;
				}
				else {
					actei->flag |= IPO_SELECT;
				}
				set_active_editipo(actei);
			}
		}
	}
	
	/* undo mapping of ipo-points for editing if scaled ipo */
	if (NLA_IPO_SCALED) {
		actstrip_map_ipo_keys(OBACT, G.sipo->ipo, 1, 0);
	}
	
	update_editipo_flags();
	
	force_draw(0);
	BIF_undo_push("Select Ipo");
	
	if(G.sipo->showkey && G.sipo->blocktype==ID_OB) {
		ob= OBACT;
		if(ob && (ob->ipoflag & OB_DRAWKEY)) allqueue(REDRAWVIEW3D, 0);
	}
	/* points inside of curve are drawn selected too */
	if(G.sipo->blocktype==ID_CU)
		allqueue(REDRAWVIEW3D, 0);
	
	getmouseco_areawin(mval);
	xo= mval[0]; 
	yo= mval[1];
	
	while (get_mbut() & ((U.flag & USER_LMOUSESELECT)?L_MOUSE:R_MOUSE)) {		
		getmouseco_areawin(mval);
		if(abs(mval[0]-xo)+abs(mval[1]-yo) > 4) {
			
			if (marker) {
				transform_markers('g', 0);
			}
			else {
				if(actkb) move_keys(OBACT);
				else transform_ipo('g');
			}
			
			return;
		}
		BIF_wait_for_statechange();
	}
}


/* *********************************** */

/* handling of right-hand channel/curve buttons in ipo window */
void do_ipowin_buts(short event)
{
	EditIpo *ei = NULL;
	int a;

	/* without shift, all other channels are made invisible */
	if((G.qual & LR_SHIFTKEY)==0) {
		if(event>G.sipo->totipo) return;
		ei = G.sipo->editipo;
		for(a=0; a<G.sipo->totipo; a++) {
			if(a!=event) ei->flag &= ~IPO_VISIBLE;
			else ei->flag |= IPO_VISIBLE;
			ei++;
		}
	}
	
	/* set active */
	if(event>=0 && event<G.sipo->totipo) {
		ei= G.sipo->editipo;	// void pointer...
		set_active_editipo(ei+event);
		set_active_key(event+1);	// only if there's a key, of course
	}
	scrarea_queue_winredraw(curarea);
	
	update_editipo_flags();
	get_status_editipo();

	if(G.sipo->showkey) {
		make_ipokey();
		if(G.sipo->blocktype==ID_OB) allqueue(REDRAWVIEW3D, 0);
	}

}

/* the fake buttons to the left of channel names, for select/deselect curves */
void do_ipo_selectbuttons(void)
{
	EditIpo *ei, *ei1;
	int a, nr;
	short mval[2];
	
	if(G.sipo->showkey) return;
	
	/* do not allow editipo here: convert editipos to selected */
	get_status_editipo();
	if(totipo_edit) {
		set_editflag_editipo();
	}
	
	/* which */
	getmouseco_areawin(mval);

	nr= -(mval[1]-curarea->winy+30-G.sipo->butofs-IPOBUTY)/IPOBUTY;
	if(G.sipo->blocktype==ID_KE) nr--;		/* keys show something else in first channel */
	
	if(nr>=0 && nr<G.sipo->totipo) {
		ei= G.sipo->editipo;
		ei+= nr;
		
		set_active_editipo(ei);
		set_active_key(nr+1);

		if(ei->icu) {
			if((ei->flag & IPO_VISIBLE)==0) {
				ei->flag |= IPO_VISIBLE|IPO_SELECT;
			}
	
			if((G.qual & LR_SHIFTKEY)==0) {
				ei1= G.sipo->editipo;
				for(a=0; a<G.sipo->totipo; a++) {
					ei1->flag &= ~IPO_SELECT;
					ei1++;
				}
			}

			if(ei->flag & IPO_SELECT) {
				ei->flag &= ~IPO_SELECT;
			}
			else {
				ei->flag |= IPO_SELECT;
			}
			
			update_editipo_flags();
			scrarea_queue_winredraw(curarea);
		}
	}
	BIF_undo_push("Select Ipo curve");
}

/* ********************************* Inserting keys ********************************************* */

/* depending type, it returns ipo, if needed it creates one */
/* returns either action ipo or "real" ipo */
/* arguments define full context;
   - *from has to be set always, to Object in case of Actions
   - blocktype defines available channels of Ipo struct (blocktype ID_OB can be in action too)
   - if actname, use this to locate actionchannel, and optional constname 
   - if bonename, the constname is the ipo to the constraint
*/

/* note; check header_ipo.c, spaceipo_assign_ipo() too */
Ipo *verify_ipo(ID *from, short blocktype, char *actname, char *constname, char *bonename)
{

	if(from==NULL || from->lib) return NULL;
	
	/* first check action ipos */
	if(actname && actname[0]) {
		Object *ob= (Object *)from;
		bActionChannel *achan;
		
		if(GS(from->name)!=ID_OB) {
			printf("called ipo system for action with wrong base pointer\n");
			return NULL;
		}
		
		if(ob->action==NULL)
			ob->action= add_empty_action("Action");
		
		achan= verify_action_channel(ob->action, actname);
		
		if(achan) {
			/* automatically assign achan to act-group based on pchan's grouping */
			if (blocktype == ID_PO)
				verify_pchan2achan_grouping(ob->action, ob->pose, actname);
			
			/* constraint exception */
			if(blocktype==ID_CO) {
				bConstraintChannel *conchan= verify_constraint_channel(&achan->constraintChannels, constname);
				if(conchan->ipo==NULL) {
					conchan->ipo= add_ipo("CoIpo", ID_CO);	
				}
				return conchan->ipo;
			}
			else {
				if(achan->ipo==NULL) {
					achan->ipo= add_ipo("ActIpo", blocktype);
				}
				
				return achan->ipo;
			}
		}
	}
	else {
		
		switch(GS(from->name)) {
		case ID_OB:
			{
				Object *ob= (Object *)from;
				
				/* constraint exception */
				if(blocktype==ID_CO) {
					/* check the local constraint ipo */
					if(bonename && bonename[0] && ob->pose) {
						bPoseChannel *pchan= get_pose_channel(ob->pose, bonename);
						bConstraint *con;
						for(con= pchan->constraints.first; con; con= con->next)
							if(strcmp(con->name, constname)==0)
								break;
						if(con) {
							if(con->ipo==NULL) {
								con->ipo= add_ipo("CoIpo", ID_CO);
							}
							return con->ipo;
						}
					}
					else { /* the actionchannel */
						bConstraintChannel *conchan= verify_constraint_channel(&ob->constraintChannels, constname);
						if(conchan->ipo==NULL) {
							conchan->ipo= add_ipo("CoIpo", ID_CO);	
						}
						return conchan->ipo;
					}
				}
				else if(blocktype==ID_OB) {
					if(ob->ipo==NULL) {
						ob->ipo= add_ipo("ObIpo", ID_OB);
					}
					return ob->ipo;
				}
				else if(blocktype==ID_KE) {
					Key *key= ob_get_key((Object *)from);
					
					if(key) {
						if(key->ipo==NULL) {
							key->ipo= add_ipo("KeyIpo", ID_KE);
						}
						return key->ipo;
					}
					return NULL;
				}
				else if(blocktype== ID_FLUIDSIM) {
					Object *ob= (Object *)from;
					if(ob->fluidsimFlag & OB_FLUIDSIM_ENABLE) {
						FluidsimSettings *fss= ob->fluidsimSettings;
						if(fss->ipo==NULL) {
							fss->ipo= add_ipo("FluidsimIpo", ID_FLUIDSIM);
							//fprintf(stderr,"FSIPO NEW!\n");
						}
						return fss->ipo;
					}
				}
				else if(blocktype== ID_PA){
					Object *ob= (Object *)from;
					ParticleSystem *psys= psys_get_current(ob);
					if(psys){
						if(psys->part->ipo==0)
							psys->part->ipo= add_ipo("ParticleIpo", ID_PA);
						return psys->part->ipo;
					}
					return NULL;
				}
			}
			break;
		case ID_MA:
			{
				Material *ma= (Material *)from;

				if(ma->ipo==NULL) {
					ma->ipo= add_ipo("MatIpo", ID_MA);
				}
				return ma->ipo;
			}
			break;
		case ID_TE:
			{
				Tex *tex= (Tex *)from;

				if(tex->ipo==NULL) {
					tex->ipo= add_ipo("TexIpo", ID_TE);
				}
				return tex->ipo;
			}
			break;
		case ID_SEQ:
			{
				Sequence *seq= (Sequence *)from;	/* note, sequence is mimicing Id */

				if(seq->ipo==NULL) {
					seq->ipo= add_ipo("SeqIpo", ID_SEQ);
				}
				update_seq_ipo_rect(seq);
				return seq->ipo;
			}
			break;
		case ID_CU:
			{
				Curve *cu= (Curve *)from;
				
				if(cu->ipo==NULL) {
					cu->ipo= add_ipo("CuIpo", ID_CU);
				}
				return cu->ipo;
			}
			break;
		case ID_WO:
			{
				World *wo= (World *)from;

				if(wo->ipo==NULL) {
					wo->ipo= add_ipo("WoIpo", ID_WO);
				}
				return wo->ipo;
			}
			break;
		case ID_LA:
			{
				Lamp *la= (Lamp *)from;
				
				if(la->ipo==NULL) {
					la->ipo= add_ipo("LaIpo", ID_LA);
				}
				return la->ipo;
			}
			break;
		case ID_CA:
			{
				Camera *ca= (Camera *)from;

				if(ca->ipo==NULL) {
					ca->ipo= add_ipo("CaIpo", ID_CA);
				}
				return ca->ipo;
			}
			break;
		case ID_SO:
			{
				bSound *snd= (bSound *)from;

				if(snd->ipo==NULL) {
					snd->ipo= add_ipo("SndIpo", ID_SO);
				}
				return snd->ipo;
			}
		}
	}
	
	return NULL;	
}

/* returns and creates
 * Make sure functions check for NULL or they will crash!
 *  */
IpoCurve *verify_ipocurve(ID *from, short blocktype, char *actname, char *constname, char *bonename, int adrcode)
{
	Ipo *ipo;
	IpoCurve *icu= NULL;
	
	/* return 0 if lib */
	/* creates ipo too */
	ipo= verify_ipo(from, blocktype, actname, constname, bonename);
	
	if(ipo && ipo->id.lib==NULL && from->lib==NULL) {
		
		for(icu= ipo->curve.first; icu; icu= icu->next) {
			if(icu->adrcode==adrcode) break;
		}
		if(icu==NULL) {
			icu= MEM_callocN(sizeof(IpoCurve), "ipocurve");
			
			icu->flag |= IPO_VISIBLE|IPO_AUTO_HORIZ;
			if(ipo->curve.first==NULL) icu->flag |= IPO_ACTIVE;	/* first one added active */
			
			icu->blocktype= blocktype;
			icu->adrcode= adrcode;
			
			set_icu_vars(icu);
			
			BLI_addtail( &(ipo->curve), icu);
			
			switch (GS(from->name)) {
				case ID_SEQ: {
					Sequence *seq= (Sequence *)from;
					
					update_seq_icu_rects(seq);
					break;
				}
			}
		}
	}

	return icu;
}


/* threshold for inserting keyframes - threshold here should be good enough for now, but should become userpref */
#define BEZT_INSERT_THRESH 	0.00001

/* Binary search algorithm for finding where to insert BezTriple. (for use by insert_bezt_icu)
 * Returns the index to insert at (data already at that index will be offset if replace is 0)
 */
static int binarysearch_bezt_index (BezTriple array[], BezTriple *item, int arraylen, short *replace)
{
	int start=0, end=arraylen;
	int loopbreaker= 0, maxloop= arraylen * 2;
	const float frame= (item)? item->vec[1][0] : 0.0f;
	
	/* initialise replace-flag first */
	*replace= 0;
	
	/* sneaky optimisations (don't go through searching process if...):
	 *	- keyframe to be added is to be added out of current bounds
	 *	- keyframe to be added would replace one of the existing ones on bounds
	 */
	if ((arraylen <= 0) || ELEM(NULL, array, item)) {
		printf("Warning: binarysearch_bezt_index encountered invalid array \n");
		return 0;
	}
	else {
		/* check whether to add before/after/on */
		float framenum;
		
		/* 'First' Keyframe (when only one keyframe, this case is used) */
		framenum= array[0].vec[1][0];
		if (IS_EQT(frame, framenum, BEZT_INSERT_THRESH)) {
			*replace = 1;
			return 0;
		}
		else if (frame < framenum)
			return 0;
			
		/* 'Last' Keyframe */
		framenum= array[(arraylen-1)].vec[1][0];
		if (IS_EQT(frame, framenum, BEZT_INSERT_THRESH)) {
			*replace= 1;
			return (arraylen - 1);
		}
		else if (frame > framenum)
			return arraylen;
	}
	
	
	/* most of the time, this loop is just to find where to put it
	 * 'loopbreaker' is just here to prevent infinite loops 
	 */
	for (loopbreaker=0; (start <= end) && (loopbreaker < maxloop); loopbreaker++) {
		/* compute and get midpoint */
		int mid = (start + end) / 2;
		float midfra= array[mid].vec[1][0];
		
		/* check if exactly equal to midpoint */
		if (IS_EQT(frame, midfra, BEZT_INSERT_THRESH)) {
			*replace = 1;
			return mid;
		}
		
		/* repeat in upper/lower half */
		if (frame > midfra)
			start= mid + 1;
		else if (frame < midfra)
			end= mid - 1;
	}
	
	/* print error if loop-limit exceeded */
	if (loopbreaker == (maxloop-1)) {
		printf("Error: binarysearch_bezt_index was taking too long \n");
		
		// include debug info 
		printf("\tround = %d: start = %d, end = %d, arraylen = %d \n", loopbreaker, start, end, arraylen);
	}
	
	/* not found, so return where to place it */
	return start;
}

/* This function adds a given BezTriple to an IPO-Curve. It will allocate 
 * memory for the array if needed, and will insert the BezTriple into a
 * suitable place in chronological order.
 * 
 * NOTE: any recalculate of the IPO-Curve that needs to be done will need to 
 * 		be done by the caller.
 */
int insert_bezt_icu (IpoCurve *icu, BezTriple *bezt)
{
	BezTriple *newb;
	int i= 0;
	
	if (icu->bezt == NULL) {
		icu->bezt= MEM_callocN(sizeof(BezTriple), "beztriple");
		*(icu->bezt)= *bezt;
		icu->totvert= 1;
	}
	else {
		short replace = -1;
		i = binarysearch_bezt_index(icu->bezt, bezt, icu->totvert, &replace);
		
		if (replace) {			
			/* sanity check: 'i' may in rare cases exceed arraylen */
			if ((i >= 0) && (i < icu->totvert))
				*(icu->bezt + i) = *bezt;
		}
		else {
			/* add new */
			newb= MEM_callocN((icu->totvert+1)*sizeof(BezTriple), "beztriple");
			
			/* add the beztriples that should occur before the beztriple to be pasted (originally in ei->icu) */
			if (i > 0)
				memcpy(newb, icu->bezt, i*sizeof(BezTriple));
			
			/* add beztriple to paste at index i */
			*(newb + i)= *bezt;
			
			/* add the beztriples that occur after the beztriple to be pasted (originally in icu) */
			if (i < icu->totvert) 
				memcpy(newb+i+1, icu->bezt+i, (icu->totvert-i)*sizeof(BezTriple));
			
			/* replace (+ free) old with new */
			MEM_freeN(icu->bezt);
			icu->bezt= newb;
			
			icu->totvert++;
		}
	}
	
	/* we need to return the index, so that some tools which do post-processing can 
	 * detect where we added the BezTriple in the array
	 */
	return i;
}

/* This function is a wrapper for insert_bezt_icu, and should be used when
 * adding a new keyframe to a curve, when the keyframe doesn't exist anywhere
 * else yet. 
 * 
 * 'fast' - is only for the python API where importing BVH's would take an extreamly long time.
 */
void insert_vert_icu (IpoCurve *icu, float x, float y, short fast)
{
	BezTriple beztr;
	int a, h1, h2;
	
	/* set all three points, for nicer start position */
	memset(&beztr, 0, sizeof(BezTriple));
	beztr.vec[0][0]= x; 
	beztr.vec[0][1]= y;
	beztr.vec[1][0]= x;
	beztr.vec[1][1]= y;
	beztr.vec[2][0]= x;
	beztr.vec[2][1]= y;
	beztr.hide= IPO_BEZ;
	beztr.f1= beztr.f2= beztr.f3= SELECT;
	beztr.h1= beztr.h2= HD_AUTO;
	
	/* add temp beztriple to keyframes */
	a= insert_bezt_icu(icu, &beztr);
	if (!fast) calchandles_ipocurve(icu);
	
	/* set handletype */
	if (icu->totvert > 2) {
		BezTriple *bezt;
		
		h1= h2= HD_AUTO;
		bezt= (icu->bezt + a);
		
		if (a > 0) h1= (bezt-1)->h2;
		if (a < icu->totvert-1) h2= (bezt+1)->h1;
		
		bezt->h1= h1;
		bezt->h2= h2;
		
		if (!fast) calchandles_ipocurve(icu);
	}
}

void add_vert_ipo(void)
{
	EditIpo *ei;
	float x, y;
	int val;
	short mval[2];

	if(G.sipo->ipo && G.sipo->ipo->id.lib) return;
	if(G.sipo->showkey) {
		G.sipo->showkey= 0;
		free_ipokey(&G.sipo->ipokey);
	}
	
	getmouseco_areawin(mval);
	
	if(mval[0]>G.v2d->mask.xmax) return;
	
	ei= get_active_editipo();
	if(ei==NULL) {
		error("No active Ipo curve");
		return;
	}
	ei->flag |= IPO_VISIBLE;	/* can happen it is active but not visible */
	
	areamouseco_to_ipoco(G.v2d, mval, &x, &y);
	
	/* convert click-time to ipo-time */
	if (NLA_IPO_SCALED) {
		x= get_action_frame(OBACT, x);
	}
	
	if(ei->icu==NULL) {
		if(G.sipo->from) {
			ei->icu= verify_ipocurve(G.sipo->from, G.sipo->blocktype, G.sipo->actname, G.sipo->constname, G.sipo->bonename, ei->adrcode);
			if (ei->icu)
				ei->flag |= ei->icu->flag & IPO_AUTO_HORIZ;	/* new curve could have been added, weak... */
			else
				error("Cannot create an IPO curve, you may be using libdata");
		}
	}
	if(ei->icu==NULL) return;

	if(ei->disptype==IPO_DISPBITS) {
		ei->icu->vartype= IPO_BITS;
		val= (int)floor(y-0.5);
		if(val<0) val= 0;
		y= (float)(1 << val);
	}
	
	insert_vert_icu(ei->icu, x, y, 0);

	/* to be sure: if icu was 0, or only 1 curve visible */
	ei->flag |= IPO_SELECT;
	ei->icu->flag= ei->flag;
	
	editipo_changed(G.sipo, 1);
	BIF_undo_push("Add Ipo vertex");
}

static void *get_context_ipo_poin(ID *id, int blocktype, char *actname, char *constname, IpoCurve *icu, int *vartype)
{
	if (blocktype==ID_PO) {
		if (GS(id->name)==ID_OB) {
			Object *ob= (Object *)id;
			bPoseChannel *pchan= get_pose_channel(ob->pose, actname);
			
			if (pchan) {
				*vartype= IPO_FLOAT;
				return get_pchan_ipo_poin(pchan, icu->adrcode);
			}
			else
				return NULL;
		}
		return NULL;
	}
	else if (blocktype==ID_CO) {
		if ((GS(id->name)==ID_OB) && (constname && constname[0])) {
			Object *ob= (Object *)id;
			bConstraint *con;
			
			/* assume that we only want the influence (as only used for Constraint Channels) */
			if ((ob->ipoflag & OB_ACTION_OB) && !strcmp(actname, "Object")) {
				for (con= ob->constraints.first; con; con= con->next) {
					if (strcmp(constname, con->name)==0) {
						*vartype= IPO_FLOAT;
						return &con->enforce;
					}
				}
			}
			else if (ob->pose) {
				bPoseChannel *pchan= get_pose_channel(ob->pose, actname);
				
				if (pchan) {
					for (con= pchan->constraints.first; con; con= con->next) {
						if (strcmp(constname, con->name)==0) {
							*vartype= IPO_FLOAT;
							return &con->enforce;
						}
					}
				}
			}
		}
		return NULL;
	}
	else
		return get_ipo_poin(id, icu, vartype);

}

#define KEYNEEDED_DONTADD	0
#define	KEYNEEDED_JUSTADD	1
#define KEYNEEDED_DELPREV	2
#define KEYNEEDED_DELNEXT 	3

static int new_key_needed(IpoCurve *icu, float cFrame, float nValue) 
{
	/* This function determines whether a new keyframe is needed */
	/* Cases where keyframes should not be added:
	 *	1. Keyframe to be added bewteen two keyframes with similar values
	 *	2. Keyframe to be added on frame where two keyframes are already situated
	 *	3. Keyframe lies at point that intersects the linear line between two keyframes
	 */
	
	BezTriple *bezt=NULL, *prev=NULL;
	int totCount, i;
	float valA = 0.0f, valB = 0.0f;
	
	/* safety checking */
	if (!icu) return KEYNEEDED_JUSTADD;
	totCount= icu->totvert;
	if (totCount==0) return KEYNEEDED_JUSTADD;
	
	/* loop through checking if any are the same */
	bezt= icu->bezt;
	for (i=0; i<totCount; i++) {
		float prevPosi=0.0f, prevVal=0.0f;
		float beztPosi=0.0f, beztVal=0.0f;
			
		/* get current time+value */	
		beztPosi= bezt->vec[1][0];
		beztVal= bezt->vec[1][1];
			
		if (prev) {
			/* there is a keyframe before the one currently being examined */		
			
			/* get previous time+value */
			prevPosi= prev->vec[1][0];
			prevVal= prev->vec[1][1];
			
			/* keyframe to be added at point where there are already two similar points? */
			if (IS_EQ(prevPosi, cFrame) && IS_EQ(beztPosi, cFrame) && IS_EQ(beztPosi, prevPosi)) {
				return KEYNEEDED_DONTADD;
			}
			
			/* keyframe between prev+current points ? */
			if ((prevPosi <= cFrame) && (cFrame <= beztPosi)) {
				/* is the value of keyframe to be added the same as keyframes on either side ? */
				if (IS_EQ(prevVal, nValue) && IS_EQ(beztVal, nValue) && IS_EQ(prevVal, beztVal)) {
					return KEYNEEDED_DONTADD;
				}
				else {
					float realVal;
					
					/* get real value of curve at that point */
					realVal= eval_icu(icu, cFrame);
				
					/* compare whether it's the same as proposed */
					if (IS_EQ(realVal, nValue)) 
						return KEYNEEDED_DONTADD;
					else 
						return KEYNEEDED_JUSTADD;
				}
			}
			
			/* new keyframe before prev beztriple? */
			if (cFrame < prevPosi) {
				/* A new keyframe will be added. However, whether the previous beztriple
				 * stays around or not depends on whether the values of previous/current
				 * beztriples and new keyframe are the same.
				 */
				if (IS_EQ(prevVal, nValue) && IS_EQ(beztVal, nValue) && IS_EQ(prevVal, beztVal))
					return KEYNEEDED_DELNEXT;
				else 
					return KEYNEEDED_JUSTADD;
			}
		}
		else {
			/* just add a keyframe if there's only one keyframe 
			 * and the new one occurs before the exisiting one does.
			 */
			if ((cFrame < beztPosi) && (totCount==1))
				return KEYNEEDED_JUSTADD;
		}
		
		/* continue. frame to do not yet passed (or other conditions not met) */
		if (i < (totCount-1)) {
			prev= bezt;
			bezt++;
		}
		else
			break;
	}
	
	/* Frame in which to add a new-keyframe occurs after all other keys
	 * -> If there are at least two existing keyframes, then if the values of the
	 *	 last two keyframes and the new-keyframe match, the last existing keyframe
	 *	 gets deleted as it is no longer required.
	 * -> Otherwise, a keyframe is just added. 1.0 is added so that fake-2nd-to-last
	 *	 keyframe is not equal to last keyframe.
	 */
	bezt= (icu->bezt + (icu->totvert - 1));
	valA= bezt->vec[1][1];
	
	if (prev)
		valB= prev->vec[1][1];
	else 
		valB= bezt->vec[1][1] + 1.0f; 
		
	if (IS_EQ(valA, nValue) && IS_EQ(valA, valB)) 
		return KEYNEEDED_DELPREV;
	else 
		return KEYNEEDED_JUSTADD;
}

/* a duplicate of insertkey that does not check for routing to insertmatrixkey 
	to avoid recursion problems */
static void insertkey_nonrecurs(ID *id, int blocktype, char *actname, char *constname, int adrcode)
{
	IpoCurve *icu;
	Object *ob;
	void *poin= NULL;
	float curval, cfra;
	int vartype;
	int matset=0;
	
	if (matset==0) {
		icu= verify_ipocurve(id, blocktype, actname, constname, NULL, adrcode);
		
		if(icu) {
			
			poin= get_context_ipo_poin(id, blocktype, actname, constname, icu, &vartype);
			
			if(poin) {
				curval= read_ipo_poin(poin, vartype);
				
				cfra= frame_to_float(CFRA);
				
				/* if action is mapped in NLA, it returns a correction */
				if(actname && actname[0] && GS(id->name)==ID_OB)
					cfra= get_action_frame((Object *)id, cfra);
				
				if( GS(id->name)==ID_OB ) {
					ob= (Object *)id;
					if((ob->ipoflag & OB_OFFS_OB) && (give_timeoffset(ob)!=0.0) ) {
						/* actually frametofloat calc again! */
						cfra-= give_timeoffset(ob)*G.scene->r.framelen;
					}
				}
				
				insert_vert_icu(icu, cfra, curval, 0);
			}
		}
	}
}

int insertmatrixkey(ID *id, int blocktype, char *actname, char *constname, int adrcode)
{
	int matindex=0;
	/* branch on adrcode and blocktype, generating the proper matrix-based
	values to send to insertfloatkey */
	if (GS(id->name)==ID_OB) {
		Object *ob= (Object *)id;

		if ( blocktype==ID_OB ){ //working with an object
			if ((ob)&&!(ob->parent)) {
				if ((adrcode==OB_ROT_X)||(adrcode==OB_ROT_Y)||(adrcode==OB_ROT_Z)) { //get a rotation
					float eul[3];
					switch (adrcode) {
						case OB_ROT_X:
							matindex=0;
							break;
						case OB_ROT_Y:
							matindex=1;
							break;
						case OB_ROT_Z:
							matindex=2;
							break;
					}
					Mat4ToEul(ob->obmat, eul);
					insertfloatkey(id, ID_OB, actname, NULL, adrcode, eul[matindex]*(5.72958));
					return 1;
				} else if ((adrcode==OB_LOC_X)||(adrcode==OB_LOC_Y)||(adrcode==OB_LOC_Z)) {//get a translation
					switch (adrcode) {
						case OB_LOC_X:
							matindex=0;
							break;
						case OB_LOC_Y:
							matindex=1;
							break;
						case OB_LOC_Z:
							matindex=2;
							break;
					}
					insertfloatkey(id, ID_OB, actname, NULL, adrcode, ob->obmat[3][matindex]);
					return 1;
				}
			}
		} else if ( blocktype==ID_PO) { //working with a pose channel
			bPoseChannel *pchan= get_pose_channel(ob->pose, actname);
			if (pchan) {
				if ((adrcode==AC_LOC_X)||(adrcode==AC_LOC_Y)||(adrcode==AC_LOC_Z)) {
					switch (adrcode) {
						case AC_LOC_X:
							matindex=0;
							break;
						case AC_LOC_Y:
							matindex=1;
							break;
						case AC_LOC_Z:
							matindex=2;
							break;
					}
					if (!(pchan->bone->parent)||((pchan->bone->parent)&&!(pchan->bone->flag&BONE_CONNECTED))) { /* don't use for non-connected child bones */
						float delta_mat[4][4]; 
						armature_mat_pose_to_delta(delta_mat, pchan->pose_mat, pchan->bone->arm_mat);
						insertfloatkey(id, ID_PO, pchan->name, NULL, adrcode, delta_mat[3][matindex]);
						return 1;
					}
				} else if ((adrcode==AC_QUAT_W)||(adrcode==AC_QUAT_X)||(adrcode==AC_QUAT_Y)||(adrcode==AC_QUAT_Z)) { 
					float tmat[4][4], trimat[3][3], localQuat[4];
					
					switch (adrcode) {
						case AC_QUAT_W:
							matindex=0;
							break;
						case AC_QUAT_X:
							matindex=1;
							break;
						case AC_QUAT_Y:
							matindex=2;
							break;
						case AC_QUAT_Z:
							matindex=3;
							break;
					}
					
					/* it should be reasonable to assume that we are keyframing on the active object, although it is not 
					 * strictly required for this particular space conversion, arg1 must not be null for this to work
					 */
					Mat4CpyMat4(tmat, pchan->pose_mat);
					constraint_mat_convertspace(OBACT, pchan, tmat, CONSTRAINT_SPACE_POSE, CONSTRAINT_SPACE_LOCAL);
					
					Mat3CpyMat4(trimat, tmat);
					Mat3ToQuat_is_ok(trimat, localQuat);
					insertfloatkey(id, ID_PO, pchan->name, NULL, adrcode, localQuat[matindex]);
					
					return 1;
				}
			}
		}
	}
	/* failed to set a matrix key -- use traditional, but the non-recursing version */
	insertkey_nonrecurs(id,blocktype,actname,constname,adrcode);
	return 0;
}

static int match_adr_constraint(ID * id, int blocktype, char *actname, int adrcode)
{	/* This function matches constraint blocks with adrcodes to see if the
		visual keying method should be used. For example, an object looking to key
		location and having a CopyLoc constraint would return true. */
		
	Object *ob=NULL;
	int foundmatch=0;
	int searchtype=0;
	bConstraint *conref=NULL, *con=NULL;
	
	/*Retrieve constraint list*/
	if( GS(id->name)==ID_OB ) 
		ob= (Object *)id;
	if (ob) {
		if (blocktype==ID_PO) {
			bPoseChannel *pchan= get_pose_channel(ob->pose, actname);
			conref=pchan->constraints.first;
		} else if (blocktype==ID_OB) {
			conref=ob->constraints.first;
		}
		
		if (conref) {
			/*Set search type: 1 is for translation contraints, 2 is for rotation*/
			if ((adrcode==OB_LOC_X)||(adrcode==OB_LOC_Y)||(adrcode==OB_LOC_Z)||(adrcode==AC_LOC_X)||(adrcode==AC_LOC_Y)||(adrcode==AC_LOC_Z)) {
				searchtype=1;
			} else if ((adrcode==OB_ROT_X)||(adrcode==OB_ROT_Y)||(adrcode==OB_ROT_Z)||(adrcode==AC_QUAT_W)||(adrcode==AC_QUAT_X)||(adrcode==AC_QUAT_Y)||(adrcode==AC_QUAT_Z)) {
				searchtype=2;
			}
			
			if (searchtype>0) {
				for (con=conref; (con)&&(foundmatch==0); con=con->next) {
					switch (con->type) {
					/* match constraint types to which kinds of keying they would affect */
						case CONSTRAINT_TYPE_CHILDOF:
							foundmatch=1;
							break;
						case CONSTRAINT_TYPE_TRACKTO:
							if (searchtype==2) foundmatch=1;
							break;
						case CONSTRAINT_TYPE_FOLLOWPATH:
							foundmatch=1;
							break;
						case CONSTRAINT_TYPE_ROTLIMIT:
							if (searchtype==2) foundmatch=1;
							break;
						case CONSTRAINT_TYPE_LOCLIMIT:
							if (searchtype==1) foundmatch=1;
							break;
						case CONSTRAINT_TYPE_ROTLIKE:
							if (searchtype==2) foundmatch=1;
							break;
						case CONSTRAINT_TYPE_LOCLIKE:
							if (searchtype==1) foundmatch=1;
							break;
						case CONSTRAINT_TYPE_LOCKTRACK:
							if (searchtype==2) foundmatch=1;
							break;
						case CONSTRAINT_TYPE_DISTLIMIT:
							if (searchtype==1) foundmatch=1;
							break;
						case CONSTRAINT_TYPE_MINMAX:
							if (searchtype==1) foundmatch=1;
							break;
						case CONSTRAINT_TYPE_TRANSFORM:
							foundmatch=1;
							break;
						default:
							break;
					}
				}
			}
		}
	}
	
	return foundmatch;
			
}

void insertkey(ID *id, int blocktype, char *actname, char *constname, int adrcode, short fast)
{
	IpoCurve *icu;
	Object *ob;
	void *poin= NULL;
	float curval, cfra;
	int vartype;
	int matset=0;
	
	if ((IS_AUTOKEY_FLAG(AUTOMATKEY))&&(match_adr_constraint(id, blocktype, actname, adrcode))) {
		matset=insertmatrixkey(id, blocktype, actname, constname, adrcode);
	} 
	if (matset==0) {
		icu= verify_ipocurve(id, blocktype, actname, constname, NULL, adrcode);
		
		if(icu) {
			
			poin= get_context_ipo_poin(id, blocktype, actname, constname, icu, &vartype);
			
			if(poin) {
				curval= read_ipo_poin(poin, vartype);
				
				cfra= frame_to_float(CFRA);
				
				/* if action is mapped in NLA, it returns a correction */
				if(actname && actname[0] && GS(id->name)==ID_OB)
					cfra= get_action_frame((Object *)id, cfra);
				
				if( GS(id->name)==ID_OB ) {
					ob= (Object *)id;
					if((ob->ipoflag & OB_OFFS_OB) && (give_timeoffset(ob)!=0.0) ) {
						/* actually frametofloat calc again! */
						cfra-= give_timeoffset(ob)*G.scene->r.framelen;
					}
				}
				
				insert_vert_icu(icu, cfra, curval, fast);
			}
		}
	}
}



/* This function is a 'smarter' version of the insert key code.
 * It uses an auxilliary function to check whether a keyframe is really needed */
void insertkey_smarter(ID *id, int blocktype, char *actname, char *constname, int adrcode)
{
	IpoCurve *icu;
	Object *ob;
	void *poin= NULL;
	float curval, cfra;
	int vartype;
	int insert_mode;
	
	icu= verify_ipocurve(id, blocktype, actname, constname, NULL, adrcode);
	
	if(icu) {
		
		poin= get_context_ipo_poin(id, blocktype, actname, constname, icu, &vartype);
		
		if(poin) {
			curval= read_ipo_poin(poin, vartype);
			
			cfra= frame_to_float(CFRA);
			
			/* if action is mapped in NLA, it returns a correction */
			if(actname && actname[0] && GS(id->name)==ID_OB)
				cfra= get_action_frame((Object *)id, cfra);
			
			if( GS(id->name)==ID_OB ) {
				ob= (Object *)id;
				if((ob->ipoflag & OB_OFFS_OB) && (give_timeoffset(ob)!=0.0) ) {
					/* actually frametofloat calc again! */
					cfra-= give_timeoffset(ob)*G.scene->r.framelen;
				}
			}
			
			/* check whether this curve really needs a new keyframe */
			insert_mode= new_key_needed(icu, cfra, curval);
			
			/* insert new keyframe at current frame */
			if (insert_mode) 
				insert_vert_icu(icu, cfra, curval, 0);
			
			/* delete keyframe immediately before/after newly added */
			switch (insert_mode) {
				case KEYNEEDED_DELPREV:
					delete_icu_key(icu, icu->totvert-2, 1);
					break;
				case KEYNEEDED_DELNEXT:
					delete_icu_key(icu, 1, 1);
					break;
			}
		}
	}
}

/* For inserting keys based on an arbitrary float value */
void insertfloatkey(ID *id, int blocktype, char *actname, char *constname, int adrcode, float floatkey)
{
	IpoCurve *icu;
	Object *ob;
	void *poin= NULL;
	float cfra;
	int vartype;
	
	icu= verify_ipocurve(id, blocktype, actname, constname, NULL, adrcode);
	
	if(icu) {
		
		poin= get_context_ipo_poin(id, blocktype, actname, constname, icu, &vartype);
		
		if(poin) {
			
			cfra= frame_to_float(CFRA);
			
			/* if action is mapped in NLA, it returns a correction */
			if(actname && actname[0] && GS(id->name)==ID_OB)
				cfra= get_action_frame((Object *)id, cfra);
 			
 			if( GS(id->name)==ID_OB ) {
 				ob= (Object *)id;
 				if((ob->ipoflag & OB_OFFS_OB) && (give_timeoffset(ob)!=0.0) ) {
 					/* actually frametofloat calc again! */
 					cfra-= give_timeoffset(ob)*G.scene->r.framelen;
 				}
 			}
			
			/* insert new keyframe at current frame */
			insert_vert_icu(icu, cfra, floatkey, 0);
 		}
 	}
}

void insertkey_editipo(void)
{
	EditIpo *ei;
	IpoKey *ik;
	ID *id;
	float *fp, cfra, *insertvals;
	int a, nr, ok, tot;
	short event;
	
	ei= get_active_editipo();
	if(ei && ei->icu && ei->icu->driver) 
		event= pupmenu("Insert Curve %t|Default one-to-one mapping %x3");
	else if(G.sipo->showkey)
		event= pupmenu("Insert Key Vertices %t|Current Frame %x1|Selected Keys %x2");
	else 
		event= pupmenu("Insert Key Vertices %t|Current Frame %x1");
	
	if(event<1) return;
	
	if(event==3) {
		IpoDriver *driver= ei->icu->driver;
		
		if(ei->icu->bezt) MEM_freeN(ei->icu->bezt);
		ei->icu->totvert= 0;
		ei->icu->bezt= NULL;
		
		insert_vert_icu(ei->icu, 0.0f, 0.0f, 0);
		
		if(ELEM3(driver->adrcode, OB_ROT_X, OB_ROT_Y, OB_ROT_Z)) {
			if(ei->disptype==IPO_DISPDEGR)
				insert_vert_icu(ei->icu, 18.0f, 18.0f, 0);
			else
				insert_vert_icu(ei->icu, 18.0f, 1.0f, 0);
		}
		else
			insert_vert_icu(ei->icu, 1.0f, 1.0f, 0);
		
		ei->flag |= IPO_SELECT|IPO_VISIBLE;
		ei->icu->flag= ei->flag;
		ei->icu->extrap= IPO_DIR;

		do_ipo_buttons(B_IPOHOME);
	}
	else {
		ei= G.sipo->editipo;
		for(nr=0; nr<G.sipo->totipo; nr++, ei++) {
			if (ISPOIN(ei, flag & IPO_VISIBLE, icu)) {
			
				ok= 0;
				if(G.sipo->showkey) ok= 1;
				else if(ei->flag & IPO_SELECT) ok= 1;

				if(ok) {
					/* count amount */
					if(event==1) tot= 1;
					else {
						ik= G.sipo->ipokey.first;
						tot= 0;
						while(ik) {
							if(ik->flag & 1) tot++;
							ik= ik->next;
						}
					}
					if(tot) {
					
						/* correction for ob timeoffs */
						cfra= frame_to_float(CFRA);
						id= G.sipo->from;	
						if(id && GS(id->name)==ID_OB ) {
							Object *ob= (Object *)id;
							if((ob->ipoflag & OB_OFFS_OB) && (give_timeoffset(ob)!=0.0) ) {
								cfra-= give_timeoffset(ob)*G.scene->r.framelen;
							}
						}
						else if(id && GS(id->name)==ID_SEQ) {
							Sequence *last_seq = get_last_seq();	/* editsequence.c */
							
							if(last_seq && (last_seq->flag & SEQ_IPO_FRAME_LOCKED) == 0) {
								cfra= (float)(100.0*(cfra-last_seq->startdisp)/((float)(last_seq->enddisp-last_seq->startdisp)));
							}
						}
						
						/* convert cfra to ipo-time */
						if (NLA_IPO_SCALED) {
							cfra= get_action_frame(OBACT, cfra);
						}
				
						insertvals= MEM_mallocN(sizeof(float)*2*tot, "insertkey_editipo");
						/* make sure icu->curval is correct */
						calc_ipo(G.sipo->ipo, cfra);
						
						if(event==1) {
							insertvals[0]= cfra;
							
							insertvals[1]= ei->icu->curval;
						}
						else {
							fp= insertvals;
							ik= G.sipo->ipokey.first;
							while(ik) {
								if(ik->flag & 1) {
									calc_ipo(G.sipo->ipo, ik->val);

									fp[0]= ik->val;
									fp[1]= ei->icu->curval;
									fp+= 2;
								}
								ik= ik->next;
							}
						}
						fp= insertvals;
						for(a=0; a<tot; a++, fp+=2) {
							insert_vert_icu(ei->icu, fp[0], fp[1], 0);
						}
						
						MEM_freeN(insertvals);
						calc_ipo(G.sipo->ipo, (float)CFRA);
					}
				}
			}
		}
	}
	BIF_undo_push("Insert Key Ipo");
	allqueue (REDRAWACTION, 0);
	allqueue(REDRAWNLA, 0);
	allqueue(REDRAWIPO, 0);
	allspace(REMAKEIPO, 0);
}


void common_insertkey(void)
{
	Base *base;
	Object *ob;
	Material *ma;
	ID *id;
	IpoCurve *icu;
	World *wo;
	Lamp *la;
	Tex *te;
	int tlay, map, event;
	char menustr[256];

	if(curarea->spacetype==SPACE_IPO) {
		insertkey_editipo();
	}
	else if(curarea->spacetype==SPACE_ACTION) {
		insertkey_action();
	}
	else if(curarea->spacetype==SPACE_BUTS) {
		if(G.buts->mainb==CONTEXT_SHADING) {
			int tab= G.buts->tab[CONTEXT_SHADING];

			if(tab==TAB_SHADING_MAT) {
				ma = G.buts->lockpoin;
				ma = editnode_get_active_material(ma);
				id = (ID *)ma;
				
				if(id) {
					event= pupmenu("Insert Key %t|RGB%x0|Alpha%x1|Halo Size%x2|Mode %x3|All Color%x10|All Mirror%x14|Ofs%x12|Size%x13|All Mapping%x11");
					if(event== -1) return;

					map= texchannel_to_adrcode(ma->texact);

					if(event==0 || event==10) {
						insertkey(id, ID_MA, NULL, NULL, MA_COL_R, 0);
						insertkey(id, ID_MA, NULL, NULL, MA_COL_G, 0);
						insertkey(id, ID_MA, NULL, NULL, MA_COL_B, 0);
					}
					if(event==1 || event==10) {
						insertkey(id, ID_MA, NULL, NULL, MA_ALPHA, 0);
					}
					if(event==2 || event==10) {
						insertkey(id, ID_MA, NULL, NULL, MA_HASIZE, 0);
					}
					if(event==3 || event==10) {
						insertkey(id, ID_MA, NULL, NULL, MA_MODE, 0);
					}
					if(event==10) {
						insertkey(id, ID_MA, NULL, NULL, MA_SPEC_R, 0);
						insertkey(id, ID_MA, NULL, NULL, MA_SPEC_G, 0);
						insertkey(id, ID_MA, NULL, NULL, MA_SPEC_B, 0);
						insertkey(id, ID_MA, NULL, NULL, MA_REF, 0);
						insertkey(id, ID_MA, NULL, NULL, MA_EMIT, 0);
						insertkey(id, ID_MA, NULL, NULL, MA_AMB, 0);
						insertkey(id, ID_MA, NULL, NULL, MA_SPEC, 0);
						insertkey(id, ID_MA, NULL, NULL, MA_HARD, 0);
						insertkey(id, ID_MA, NULL, NULL, MA_MODE, 0);
						insertkey(id, ID_MA, NULL, NULL, MA_TRANSLU, 0);
						insertkey(id, ID_MA, NULL, NULL, MA_ADD, 0);
					}
					if(event==14) {
						insertkey(id, ID_MA, NULL, NULL, MA_RAYM, 0);
						insertkey(id, ID_MA, NULL, NULL, MA_FRESMIR, 0);
						insertkey(id, ID_MA, NULL, NULL, MA_FRESMIRI, 0);
						insertkey(id, ID_MA, NULL, NULL, MA_FRESTRA, 0);
						insertkey(id, ID_MA, NULL, NULL, MA_FRESTRAI, 0);
					}
					if(event==12 || event==11) {
						insertkey(id, ID_MA, NULL, NULL, map+MAP_OFS_X, 0);
						insertkey(id, ID_MA, NULL, NULL, map+MAP_OFS_Y, 0);
						insertkey(id, ID_MA, NULL, NULL, map+MAP_OFS_Z, 0);
					}
					if(event==13 || event==11) {
						insertkey(id, ID_MA, NULL, NULL, map+MAP_SIZE_X, 0);
						insertkey(id, ID_MA, NULL, NULL, map+MAP_SIZE_Y, 0);
						insertkey(id, ID_MA, NULL, NULL, map+MAP_SIZE_Z, 0);
					}
					if(event==11) {
						insertkey(id, ID_MA, NULL, NULL, map+MAP_R, 0);
						insertkey(id, ID_MA, NULL, NULL, map+MAP_G, 0);
						insertkey(id, ID_MA, NULL, NULL, map+MAP_B, 0);
						insertkey(id, ID_MA, NULL, NULL, map+MAP_DVAR, 0);
						insertkey(id, ID_MA, NULL, NULL, map+MAP_COLF, 0);
						insertkey(id, ID_MA, NULL, NULL, map+MAP_NORF, 0);
						insertkey(id, ID_MA, NULL, NULL, map+MAP_VARF, 0);
						insertkey(id, ID_MA, NULL, NULL, map+MAP_DISP, 0);
					}
				}
			}
			else if(tab==TAB_SHADING_WORLD) {
				id= G.buts->lockpoin;
				wo= G.buts->lockpoin;
				if(id) {
					event= pupmenu("Insert Key %t|Zenith RGB%x0|Horizon RGB%x1|Mist%x2|Stars %x3|Offset%x12|Size%x13");
					if(event== -1) return;

					map= texchannel_to_adrcode(wo->texact);

					if(event==0) {
						insertkey(id, ID_WO, NULL, NULL, WO_ZEN_R, 0);
						insertkey(id, ID_WO, NULL, NULL, WO_ZEN_G, 0);
						insertkey(id, ID_WO, NULL, NULL, WO_ZEN_B, 0);
					}
					if(event==1) {
						insertkey(id, ID_WO, NULL, NULL, WO_HOR_R, 0);
						insertkey(id, ID_WO, NULL, NULL, WO_HOR_G, 0);
						insertkey(id, ID_WO, NULL, NULL, WO_HOR_B, 0);
					}
					if(event==2) {
						insertkey(id, ID_WO, NULL, NULL, WO_MISI, 0);
						insertkey(id, ID_WO, NULL, NULL, WO_MISTDI, 0);
						insertkey(id, ID_WO, NULL, NULL, WO_MISTSTA, 0);
						insertkey(id, ID_WO, NULL, NULL, WO_MISTHI, 0);
					}
					if(event==3) {
						insertkey(id, ID_WO, NULL, NULL, WO_STAR_R, 0);
						insertkey(id, ID_WO, NULL, NULL, WO_STAR_G, 0);
						insertkey(id, ID_WO, NULL, NULL, WO_STAR_B, 0);
						insertkey(id, ID_WO, NULL, NULL, WO_STARDIST, 0);
						insertkey(id, ID_WO, NULL, NULL, WO_STARSIZE, 0);
					}
					if(event==12) {
						insertkey(id, ID_WO, NULL, NULL, map+MAP_OFS_X, 0);
						insertkey(id, ID_WO, NULL, NULL, map+MAP_OFS_Y, 0);
						insertkey(id, ID_WO, NULL, NULL, map+MAP_OFS_Z, 0);
					}
					if(event==13) {
						insertkey(id, ID_WO, NULL, NULL, map+MAP_SIZE_X, 0);
						insertkey(id, ID_WO, NULL, NULL, map+MAP_SIZE_Y, 0);
						insertkey(id, ID_WO, NULL, NULL, map+MAP_SIZE_Z, 0);
					}
				}
			}
			else if(tab==TAB_SHADING_LAMP) {
				id= G.buts->lockpoin;
				la= G.buts->lockpoin;
				if(id) {
					event= pupmenu("Insert Key %t|RGB%x0|Energy%x1|Spot Size%x2|Offset%x12|Size%x13");
					if(event== -1) return;

					map= texchannel_to_adrcode(la->texact);

					if(event==0) {
						insertkey(id, ID_LA, NULL, NULL, LA_COL_R, 0);
						insertkey(id, ID_LA, NULL, NULL, LA_COL_G, 0);
						insertkey(id, ID_LA, NULL, NULL, LA_COL_B, 0);
					}
					if(event==1) {
						insertkey(id, ID_LA, NULL, NULL, LA_ENERGY, 0);
					}
					if(event==2) {
						insertkey(id, ID_LA, NULL, NULL, LA_SPOTSI, 0);
					}
					if(event==12) {
						insertkey(id, ID_LA, NULL, NULL, map+MAP_OFS_X, 0);
						insertkey(id, ID_LA, NULL, NULL, map+MAP_OFS_Y, 0);
						insertkey(id, ID_LA, NULL, NULL, map+MAP_OFS_Z, 0);
					}
					if(event==13) {
						insertkey(id, ID_LA, NULL, NULL, map+MAP_SIZE_X, 0);
						insertkey(id, ID_LA, NULL, NULL, map+MAP_SIZE_Y, 0);
						insertkey(id, ID_LA, NULL, NULL, map+MAP_SIZE_Z, 0);
					}

				}
			}
			else if(tab==TAB_SHADING_TEX) {
				id= G.buts->lockpoin;
				te= G.buts->lockpoin;
				if(id) {
					event= pupmenu("Insert Key %t|Clouds%x0|Marble%x1|Stucci%x2|Wood%x3|Magic%x4|Blend%x5|Musgrave%x6|Voronoi%x7|DistortedNoise%x8|ColorFilter%x9");
					if(event== -1) return;

					if(event==0) {
						insertkey(id, ID_TE, NULL, NULL, TE_NSIZE, 0);
						insertkey(id, ID_TE, NULL, NULL, TE_NDEPTH, 0);
						insertkey(id, ID_TE, NULL, NULL, TE_NTYPE, 0);
						insertkey(id, ID_TE, NULL, NULL, TE_MG_TYP, 0);
						insertkey(id, ID_TE, NULL, NULL, TE_N_BAS1, 0);
					}
					if(event==1) {
						insertkey(id, ID_TE, NULL, NULL, TE_NSIZE, 0);
						insertkey(id, ID_TE, NULL, NULL, TE_NDEPTH, 0);
						insertkey(id, ID_TE, NULL, NULL, TE_NTYPE, 0);
						insertkey(id, ID_TE, NULL, NULL, TE_TURB, 0);
						insertkey(id, ID_TE, NULL, NULL, TE_MG_TYP, 0);
						insertkey(id, ID_TE, NULL, NULL, TE_N_BAS1, 0);
						insertkey(id, ID_TE, NULL, NULL, TE_N_BAS2, 0);
					}
					if(event==2) {
						insertkey(id, ID_TE, NULL, NULL, TE_NSIZE, 0);
						insertkey(id, ID_TE, NULL, NULL, TE_NTYPE, 0);
						insertkey(id, ID_TE, NULL, NULL, TE_TURB, 0);
						insertkey(id, ID_TE, NULL, NULL, TE_MG_TYP, 0);
						insertkey(id, ID_TE, NULL, NULL, TE_N_BAS1, 0);
					}
					if(event==3) {
						insertkey(id, ID_TE, NULL, NULL, TE_NSIZE, 0);
						insertkey(id, ID_TE, NULL, NULL, TE_NTYPE, 0);
						insertkey(id, ID_TE, NULL, NULL, TE_TURB, 0);
						insertkey(id, ID_TE, NULL, NULL, TE_MG_TYP, 0);
						insertkey(id, ID_TE, NULL, NULL, TE_N_BAS1, 0);
						insertkey(id, ID_TE, NULL, NULL, TE_N_BAS2, 0);
					}
					if(event==4) {
						insertkey(id, ID_TE, NULL, NULL, TE_NDEPTH, 0);
						insertkey(id, ID_TE, NULL, NULL, TE_TURB, 0);
					}
					if(event==5) {
						insertkey(id, ID_TE, NULL, NULL, TE_MG_TYP, 0);
					} 
					if(event==6) {
						insertkey(id, ID_TE, NULL, NULL, TE_MG_TYP, 0);
						insertkey(id, ID_TE, NULL, NULL, TE_MGH, 0);
						insertkey(id, ID_TE, NULL, NULL, TE_MG_LAC, 0);
						insertkey(id, ID_TE, NULL, NULL, TE_MG_OCT, 0);
						insertkey(id, ID_TE, NULL, NULL, TE_MG_OFF, 0);
						insertkey(id, ID_TE, NULL, NULL, TE_MG_GAIN, 0);
					}
					if(event==7) {
						insertkey(id, ID_TE, NULL, NULL, TE_VNW1, 0);
						insertkey(id, ID_TE, NULL, NULL, TE_VNW2, 0);
						insertkey(id, ID_TE, NULL, NULL, TE_VNW3, 0);
						insertkey(id, ID_TE, NULL, NULL, TE_VNW4, 0);
						insertkey(id, ID_TE, NULL, NULL, TE_VNMEXP, 0);
						insertkey(id, ID_TE, NULL, NULL, TE_VN_DISTM, 0);
						insertkey(id, ID_TE, NULL, NULL, TE_VN_COLT, 0);
						insertkey(id, ID_TE, NULL, NULL, TE_ISCA, 0);
						insertkey(id, ID_TE, NULL, NULL, TE_NSIZE, 0);
					}
					if(event==8) {
						insertkey(id, ID_TE, NULL, NULL, TE_MG_OCT, 0);
						insertkey(id, ID_TE, NULL, NULL, TE_MG_OFF, 0);
						insertkey(id, ID_TE, NULL, NULL, TE_MG_GAIN, 0);
						insertkey(id, ID_TE, NULL, NULL, TE_DISTA, 0);
					}
					if(event==9) {
						insertkey(id, ID_TE, NULL, NULL, TE_COL_R, 0);
						insertkey(id, ID_TE, NULL, NULL, TE_COL_G, 0);
						insertkey(id, ID_TE, NULL, NULL, TE_COL_B, 0);
						insertkey(id, ID_TE, NULL, NULL, TE_BRIGHT, 0);
						insertkey(id, ID_TE, NULL, NULL, TE_CONTRA, 0);
					}
				}
			}
		}
		else if(G.buts->mainb==CONTEXT_OBJECT) {
			ob= OBACT;
			if(ob) {
				id= (ID *) (ob);
				if(id) {
					if(ob->type==OB_MESH) 
						event= pupmenu("Insert Key %t|Surface Damping%x0|Random Damping%x1|Permeability%x2|Force Strength%x3|Force Falloff%x4");
					else
						event= pupmenu("Insert Key %t|Force Strength%x3|Force Falloff%x4");
					if(event == -1) return;

					if(event==0) {
						insertkey(id, ID_OB, NULL, NULL, OB_PD_SDAMP, 0);
					}
					if(event==1) {
						insertkey(id, ID_OB, NULL, NULL, OB_PD_RDAMP, 0);
					}
					if(event==2) {
						insertkey(id, ID_OB, NULL, NULL, OB_PD_PERM, 0);
					}
					if(event==3) {
						insertkey(id, ID_OB, NULL, NULL, OB_PD_FSTR, 0);
					}
					if(event==4) {
						insertkey(id, ID_OB, NULL, NULL, OB_PD_FFALL, 0);
					}
					if(event==5) {
						insertkey(id, ID_OB, NULL, NULL, OB_PD_FMAXD, 0);
					}
				}
			}
		}
		else if(G.buts->mainb==CONTEXT_EDITING) {
			ob= OBACT;
			if(ob && ob->type==OB_CAMERA) {
				id= G.buts->lockpoin;
				if(id) {
					/* yafray: insert key extended with aperture and focal distance */
					/* qdn: FocalDistance now enabled for Blender as wel, for use with defocus node */
					if (G.scene->r.renderer==R_INTERN)
						event= pupmenu("Insert Key %t|Lens%x0|Clipping%x1|FocalDistance%x3|Viewplane Shift%x4");
					else
						event= pupmenu("Insert Key %t|Lens%x0|Clipping%x1|Aperture%x2|FocalDistance%x3");
					if(event== -1) return;

					if(event==0) {
						insertkey(id, ID_CA, NULL, NULL, CAM_LENS, 0);
					}
					else if(event==1) {
						insertkey(id, ID_CA, NULL, NULL, CAM_STA, 0);
						insertkey(id, ID_CA, NULL, NULL, CAM_END, 0);
					}
					else if(event==2) {
						insertkey(id, ID_CA, NULL, NULL, CAM_YF_APERT, 0);
					}
					else if(event==3) {
						insertkey(id, ID_CA, NULL, NULL, CAM_YF_FDIST, 0);
					}
					else if(event==4) {
						insertkey(id, ID_CA, NULL, NULL, CAM_SHIFT_X, 0);
						insertkey(id, ID_CA, NULL, NULL, CAM_SHIFT_Y, 0);
					}
				}
			}
		}
		else if(FALSE /* && G.buts->mainb==BUTS_SOUND */) {
			if(G.ssound) {
				id= G.buts->lockpoin;
				if(id) {
					event= pupmenu("Insert Key %t|Volume%x0|Pitch%x1|Panning%x2|Attennuation%x3");
					if(event== -1) return;

					if(event==0) {
						insertkey(id, ID_SO, NULL, NULL, SND_VOLUME, 0);
					}
					if(event==1) {
						insertkey(id, ID_SO, NULL, NULL, SND_PITCH, 0);
					}
					if(event==2) {
						insertkey(id, ID_SO, NULL, NULL, SND_PANNING, 0);
					}
					if(event==3) {
						insertkey(id, ID_SO, NULL, NULL, SND_ATTEN, 0);
					}
				}
			}
		}

		BIF_undo_push("Insert Key Buttons");

		allqueue(REDRAWACTION, 0);
		allqueue(REDRAWNLA, 0);
		allqueue(REDRAWIPO, 0);
		allspace(REMAKEIPO, 0);

	}
	else if(curarea->spacetype==SPACE_VIEW3D) {
		ob= OBACT;

		if (ob && (ob->flag & OB_POSEMODE)) {
			bPoseChannel *pchan;
			
			set_pose_keys(ob);  /* sets pchan->flag to POSE_KEY if bone selected, and clears if not */
			for (pchan=ob->pose->chanbase.first; pchan; pchan=pchan->next)
				if (pchan->flag & POSE_KEY)
					break;
			if(pchan==NULL) return;
			strcpy(menustr, "Insert Key%t|Loc%x0|Rot%x1|Scale%x2|LocRot%x3|LocRotScale%x4|Avail%x9|Needed%x15|VisualLoc%x11|VisualRot%x12|VisualLocRot%x13");
		}
		else {
			base= FIRSTBASE;
			while(base) {
				if (TESTBASELIB(base)) break;
				base= base->next;
			}
			if(base==NULL) return;
			strcpy(menustr, "Insert Key%t|Loc%x0|Rot%x1|Scale%x2|LocRot%x3|LocRotScale%x4|Layer%x5|Avail%x9|Needed%x15|VisualLoc%x11|VisualRot%x12|VisualLocRot%x13");
		}

		if(ob) {
			if(ob->type==OB_MESH) strcat(menustr, "| %x6|Mesh%x7");
			else if(ob->type==OB_LATTICE) strcat(menustr, "| %x6|Lattice%x7");
			else if(ob->type==OB_CURVE) strcat(menustr, "| %x6|Curve%x7");
			else if(ob->type==OB_SURF) strcat(menustr, "| %x6|Surface%x7");
		}

		event= pupmenu(menustr);
		if(event== -1) return;

		if(event==7) { // ob != NULL
			insert_shapekey(ob);
			return;
		}

		if (ob && (ob->flag & OB_POSEMODE)){
			bPoseChannel *pchan;
			short recalc_bonepaths= 0;
			
			if (ob->action && ob->action->id.lib) {
				error ("Can't key libactions");
				return;
			}
			
			id= &ob->id;
			for (pchan=ob->pose->chanbase.first; pchan; pchan=pchan->next) {
				if (pchan->flag & POSE_KEY) {
					/* insert relevant keyframes */
					if(event==0 || event==3 ||event==4) {
						insertkey(id, ID_PO, pchan->name, NULL, AC_LOC_X, 0);
						insertkey(id, ID_PO, pchan->name, NULL, AC_LOC_Y, 0);
						insertkey(id, ID_PO, pchan->name, NULL, AC_LOC_Z, 0);
					}
					if(event==1 || event==3 || event==4) {
						insertkey(id, ID_PO, pchan->name, NULL, AC_QUAT_X, 0);
						insertkey(id, ID_PO, pchan->name, NULL, AC_QUAT_Y, 0);
						insertkey(id, ID_PO, pchan->name, NULL, AC_QUAT_Z, 0);
						insertkey(id, ID_PO, pchan->name, NULL, AC_QUAT_W, 0);
					}
					if(event==2 || event==4) {
						insertkey(id, ID_PO, pchan->name, NULL, AC_SIZE_X, 0);
						insertkey(id, ID_PO, pchan->name, NULL, AC_SIZE_Y, 0);
						insertkey(id, ID_PO, pchan->name, NULL, AC_SIZE_Z, 0);
					}
					if (event==9 && ob->action) {
						bActionChannel *achan;
						
						for (achan = ob->action->chanbase.first; achan; achan=achan->next){
							if (achan->ipo && !strcmp (achan->name, pchan->name)){
								for (icu = achan->ipo->curve.first; icu; icu=icu->next){
									insertkey(id, ID_PO, achan->name, NULL, icu->adrcode, 0);
								}
								break;
							}
						}
					}
 					if(event==11 || event==13) {
						int matok=0; 
						/* check one to make sure we're not trying to set visual loc keys on
							bones inside of a chain, which only leads to tears. */
						matok=  insertmatrixkey(id, ID_PO, pchan->name, NULL, AC_LOC_X);
								insertmatrixkey(id, ID_PO, pchan->name, NULL, AC_LOC_Y);
								insertmatrixkey(id, ID_PO, pchan->name, NULL, AC_LOC_Z);
						
						if (matok == 0) {
							insertkey(id, ID_PO, pchan->name, NULL, AC_LOC_X, 0);
							insertkey(id, ID_PO, pchan->name, NULL, AC_LOC_Y, 0);
							insertkey(id, ID_PO, pchan->name, NULL, AC_LOC_Z, 0);
						}
 					}
 					if(event==12 || event==13) {
						int matok=0; 
						/* check one to make sure we're not trying to set visual rot keys on
							bones inside of a chain, which only leads to tears. */
						matok=  insertmatrixkey(id, ID_PO, pchan->name, NULL, AC_QUAT_W);
								insertmatrixkey(id, ID_PO, pchan->name, NULL, AC_QUAT_X);
								insertmatrixkey(id, ID_PO, pchan->name, NULL, AC_QUAT_Y);
								insertmatrixkey(id, ID_PO, pchan->name, NULL, AC_QUAT_Z);
						
						if (matok == 0) {
							insertkey(id, ID_PO, pchan->name, NULL, AC_QUAT_W, 0);
							insertkey(id, ID_PO, pchan->name, NULL, AC_QUAT_X, 0);
							insertkey(id, ID_PO, pchan->name, NULL, AC_QUAT_Y, 0);
							insertkey(id, ID_PO, pchan->name, NULL, AC_QUAT_Z, 0);
						}
 					}
					if (event==15 && ob->action) {
						bActionChannel *achan;
						
						for (achan = ob->action->chanbase.first; achan; achan=achan->next){
							if (achan->ipo && !strcmp (achan->name, pchan->name)){
								for (icu = achan->ipo->curve.first; icu; icu=icu->next){
									insertkey_smarter(id, ID_PO, achan->name, NULL, icu->adrcode);
								}
								break;
							}
						}
					}
					
					/* clear unkeyed flag (it doesn't matter if it's set or not) */
					if (pchan->bone)
						pchan->bone->flag &= ~BONE_UNKEYED;
						
					/* check if bone has a path */
					if (pchan->path)
						recalc_bonepaths = 1;
				}
			}
			
			/* recalculate ipo handles, etc. */
			if(ob->action)
				remake_action_ipos(ob->action);
				
			/* recalculate bone-paths on adding new keyframe? */
			// TODO: currently, there is no setting to turn this on/off globally
			if (recalc_bonepaths)
				pose_recalculate_paths(ob);
			
			
			allqueue(REDRAWIPO, 0);
			allqueue(REDRAWACTION, 0);
			allqueue(REDRAWNLA, 0);
		}
		else {
			base= FIRSTBASE;
			while(base) {
				if (TESTBASELIB(base)) {
					char *actname= NULL;

					id= (ID *)(base->object);

					if(base->object->ipoflag & OB_ACTION_OB)
						actname= "Object";

					/* all curves in ipo deselect */
					if(base->object->ipo || base->object->action) {
						if (base->object->ipo) {
							icu= base->object->ipo->curve.first;
						}
						else {
							bActionChannel *achan;
							achan= get_action_channel(base->object->action, actname);
							
							if (achan && achan->ipo)
								icu= achan->ipo->curve.first;
							else
								icu= NULL;
						}
						
						while(icu) {
							icu->flag &= ~IPO_SELECT;
							
							switch (event) {
								case 9: 
									insertkey(id, ID_OB, actname, NULL, icu->adrcode, 0);
									break;
								case 15:
									insertkey_smarter(id, ID_OB, actname, NULL, icu->adrcode);
									break;
							}
							icu= icu->next;
						}
					}

					if(event==0 || event==3 ||event==4) {
						insertkey(id, ID_OB, actname, NULL, OB_LOC_X, 0);
						insertkey(id, ID_OB, actname, NULL, OB_LOC_Y, 0);
						insertkey(id, ID_OB, actname, NULL, OB_LOC_Z, 0);
					}
					if(event==1 || event==3 ||event==4) {
						insertkey(id, ID_OB, actname, NULL, OB_ROT_X, 0);
						insertkey(id, ID_OB, actname, NULL, OB_ROT_Y, 0);
						insertkey(id, ID_OB, actname, NULL, OB_ROT_Z, 0);
					}
					if(event==2 || event==4) {
						insertkey(id, ID_OB, actname, NULL, OB_SIZE_X, 0);
						insertkey(id, ID_OB, actname, NULL, OB_SIZE_Y, 0);
						insertkey(id, ID_OB, actname, NULL, OB_SIZE_Z, 0);
					}
					if(event==5) {
						/* remove localview  */
						tlay= base->object->lay;
						base->object->lay &= 0xFFFFFF;
						insertkey(id, ID_OB, actname, NULL, OB_LAY, 0);
						base->object->lay= tlay;
					}
 					if(event==11 || event==13) {
						insertmatrixkey(id, ID_OB, actname, NULL, OB_LOC_X);
						insertmatrixkey(id, ID_OB, actname, NULL, OB_LOC_Y);
						insertmatrixkey(id, ID_OB, actname, NULL, OB_LOC_Z);
 					}
 					if(event==12 || event==13) {
						insertmatrixkey(id, ID_OB, actname, NULL, OB_ROT_X);
						insertmatrixkey(id, ID_OB, actname, NULL, OB_ROT_Y);
						insertmatrixkey(id, ID_OB, actname, NULL, OB_ROT_Z);
 					}
					base->object->recalc |= OB_RECALC_OB;
				}
				base= base->next;
			}
		}

		if(event==0) BIF_undo_push("Insert Loc Key");
		else if(event==1) BIF_undo_push("Insert Rot Key");
		else if(event==2) BIF_undo_push("Insert Scale Key");
		else if(event==3) BIF_undo_push("Insert LocRot Key");
		else if(event==4) BIF_undo_push("Insert LocRotScale Key");
		else if(event==5) BIF_undo_push("Insert Layer Key");
		else if(event==7) BIF_undo_push("Insert Vertex Key");
		else if(event==9) BIF_undo_push("Insert Avail Key");
		else if(event==11) BIF_undo_push("Insert VisualLoc Key");
		else if(event==12) BIF_undo_push("Insert VisualRot Key");
		else if(event==13) BIF_undo_push("Insert VisualLocRot Key");
		else if(event==15) BIF_undo_push("Insert Needed Key");
		
		DAG_scene_flush_update(G.scene, screen_view3d_layers(), 0);
		
		allspace(REMAKEIPO, 0);
		allqueue(REDRAWIPO, 0);
		allqueue(REDRAWVIEW3D, 0);
		allqueue(REDRAWACTION, 0);
		allqueue(REDRAWNLA, 0);
	}

}

/* ****************************************************************************** */

void add_duplicate_editipo(void)
{
	Object *ob;
	EditIpo *ei;
	IpoCurve *icu;
	BezTriple *bezt, *beztn, *newb;
	int tot, a, b;
	
	get_status_editipo();
	if(totipo_vertsel==0) return;
	
	ei= G.sipo->editipo;
	for(a=0; a<G.sipo->totipo; a++, ei++) {
		if (ISPOIN3(ei, flag & IPO_VISIBLE, icu, icu->bezt)) {
			if(G.sipo->showkey || (ei->flag & IPO_EDIT)) {
				icu= ei->icu;
				
				/* how many points */
				tot= 0;
				b= icu->totvert;
				bezt= icu->bezt;
				while(b--) {
					if(bezt->f2 & SELECT) tot++;
					bezt++;
				}
				
				if(tot) {
					icu->totvert+= tot;
					newb= beztn= MEM_mallocN(icu->totvert*sizeof(BezTriple), "bezt");
					bezt= icu->bezt;
					b= icu->totvert-tot;
					while(b--) {
						*beztn= *bezt;
						if(bezt->f2 & SELECT) {
							beztn->f1= beztn->f2= beztn->f3= 0;
							beztn++;
							*beztn= *bezt;
						}
						beztn++;
						bezt++;
					}
					MEM_freeN(icu->bezt);
					icu->bezt= newb;
					
					calchandles_ipocurve(icu);
				}
			}
		}
	}
	
	if(G.sipo->showkey) {
		make_ipokey();
		if(G.sipo->blocktype==ID_OB) {
			ob= OBACT;
			if(ob && (ob->ipoflag & OB_DRAWKEY)) allqueue(REDRAWVIEW3D, 0);
		}
	}
	BIF_undo_push("Duplicate Ipo");
	transform_ipo('g');
}

void remove_doubles_ipo(void)
{
	EditIpo *ei;
	IpoKey *ik, *ikn;
	BezTriple *bezt, *newb, *new1;
	float val;
	int mode, a, b;
	
	ei= G.sipo->editipo;
	for(a=0; a<G.sipo->totipo; a++, ei++) {
		if (ISPOIN3(ei, flag & IPO_VISIBLE, icu, icu->bezt)) {
			
			/* OR the curve is selected OR in editmode OR in keymode */
			mode= 0;
			if(G.sipo->showkey || (ei->flag & IPO_EDIT)) mode= 1;
			else if(ei->flag & IPO_SELECT) mode= 2;
			
			if(mode) {
				bezt= ei->icu->bezt;
				newb= new1= MEM_mallocN(ei->icu->totvert*sizeof(BezTriple), "newbezt");
				*newb= *bezt;
				b= ei->icu->totvert-1;
				bezt++;
				while(b--) {
					
					/* can we remove? */
					if(mode==2 || (bezt->f2 & SELECT)) {
					
						/* are the points different? */
						if( fabs( bezt->vec[1][0]-newb->vec[1][0] ) > 0.9 ) {
							newb++;
							*newb= *bezt;
						}
						else {
							/* median */
							VecMidf(newb->vec[0], newb->vec[0], bezt->vec[0]);
							VecMidf(newb->vec[1], newb->vec[1], bezt->vec[1]);
							VecMidf(newb->vec[2], newb->vec[2], bezt->vec[2]);
							
							newb->h1= newb->h2= HD_FREE;
							
							ei->icu->totvert--;
						}
						
					}
					else {
						newb++;
						*newb= *bezt;
					}
					bezt++;
				}
				
				MEM_freeN(ei->icu->bezt);
				ei->icu->bezt= new1;
				
				calchandles_ipocurve(ei->icu);				
			}
		}
	}
	
	editipo_changed(G.sipo, 1);	/* makes ipokeys again! */

	/* remove double keys */
	if(G.sipo->showkey) {
		ik= G.sipo->ipokey.first;
		ikn= ik->next;
		
		while(ik && ikn) {
			if( (ik->flag & 1) && (ikn->flag & 1) ) {
				if( fabs(ik->val-ikn->val) < 0.9 ) {
					val= (float)((ik->val + ikn->val)/2.0);
					
					for(a=0; a<G.sipo->totipo; a++) {
						if(ik->data[a]) ik->data[a]->vec[1][0]= val;
						if(ikn->data[a]) ikn->data[a]->vec[1][0]= val;						
					}
				}
			}
			ik= ikn;
			ikn= ikn->next;

		}
		
		editipo_changed(G.sipo, 1);	/* makes ipokeys agian! */

	}
	deselectall_editipo();
}


void clean_ipo(void) 
{
	EditIpo *ei;
	short ok;
	int b;
	
	ok= fbutton(&G.scene->toolsettings->clean_thresh, 
				0.0000001f, 1.0, 0.001, 0.1,
				"Threshold");
	if (!ok) return;
	
	get_status_editipo();

	ei= G.sipo->editipo;
	for(b=0; b<G.sipo->totipo; b++, ei++) {
		if (ISPOIN3(ei, flag & IPO_VISIBLE, icu, icu->bezt)) {
		
			ok= 0;
			if(G.sipo->showkey) ok= 1;
			else if(totipo_vert && (ei->flag & IPO_EDIT)) ok= 2;
			else if(totipo_vert==0 && (ei->flag & IPO_SELECT)) ok= 3;
			
			if(ok) {
				/* only clean if ok */
				clean_ipo_curve(ei->icu);
			}
		}
	}
	
	editipo_changed(G.sipo, 1);
	BIF_undo_push("Clean IPO");
}

void clean_ipo_curve(IpoCurve *icu)
{
	BezTriple *old_bezts, *bezt, *beztn;
	BezTriple *lastb;
	int totCount, i;
	float thresh;
	
	/* check if any points  */
	if (icu == NULL || icu->totvert <= 1) 
		return;
	
	/* get threshold for match-testing */
	thresh= G.scene->toolsettings->clean_thresh;
	
	/* make a copy of the old BezTriples, and clear IPO curve */
	old_bezts = icu->bezt;
	totCount = icu->totvert;	
	icu->bezt = NULL;
	icu->totvert = 0;
	
	/* now insert first keyframe, as it should be ok */
	bezt = old_bezts;
	insert_vert_icu(icu, bezt->vec[1][0], bezt->vec[1][1], 0);
	
	/* Loop through BezTriples, comparing them. Skip any that do 
	 * not fit the criteria for "ok" points.
	 */
	for (i=1; i<totCount; i++) {	
		float prev[2], cur[2], next[2];
		
		/* get BezTriples and their values */
		if (i < (totCount - 1)) {
			beztn = (old_bezts + (i+1));
			next[0]= beztn->vec[1][0]; next[1]= beztn->vec[1][1];
		}
		else {
			beztn = NULL;
			next[0] = next[1] = 0.0f;
		}
		lastb= (icu->bezt + (icu->totvert - 1));
		bezt= (old_bezts + i);
		
		/* get references for quicker access */
		prev[0] = lastb->vec[1][0]; prev[1] = lastb->vec[1][1];
		cur[0] = bezt->vec[1][0]; cur[1] = bezt->vec[1][1];
		
		/* check if current bezt occurs at same time as last ok */
		if (IS_EQT(cur[0], prev[0], thresh)) {
			/* If there is a next beztriple, and if occurs at the same time, only insert 
			 * if there is a considerable distance between the points, and also if the 
			 * current is further away than the next one is to the previous.
			 */
			if (beztn && (IS_EQT(cur[0], next[0], thresh)) && 
				(IS_EQT(next[1], prev[1], thresh)==0)) 
			{
				/* only add if current is further away from previous */
				if (cur[1] > next[1]) {
					if (IS_EQT(cur[1], prev[1], thresh) == 0) {
						/* add new keyframe */
						insert_vert_icu(icu, cur[0], cur[1], 0);
					}
				}
			}
			else {
				/* only add if values are a considerable distance apart */
				if (IS_EQT(cur[1], prev[1], thresh) == 0) {
					/* add new keyframe */
					insert_vert_icu(icu, cur[0], cur[1], 0);
				}
			}
		}
		else {
			/* checks required are dependent on whether this is last keyframe or not */
			if (beztn) {
				/* does current have same value as previous and next? */
				if (IS_EQT(cur[1], prev[1], thresh) == 0) {
					/* add new keyframe*/
					insert_vert_icu(icu, cur[0], cur[1], 0);
				}
				else if (IS_EQT(cur[1], next[1], thresh) == 0) {
					/* add new keyframe */
					insert_vert_icu(icu, cur[0], cur[1], 0);
				}
			}
			else {	
				/* add if value doesn't equal that of previous */
				if (IS_EQT(cur[1], prev[1], thresh) == 0) {
					/* add new keyframe */
					insert_vert_icu(icu, cur[0], cur[1], 0);
				}
			}
		}
	}
	
	/* now free the memory used by the old BezTriples */
	if (old_bezts)
		MEM_freeN(old_bezts);
}


/* temp struct used for smooth_ipo */
typedef struct tSmooth_Bezt {
	float *h1, *h2, *h3;	/* bezt->vec[0,1,2][1] */
} tSmooth_Bezt;

void smooth_ipo(void)
{
	EditIpo *ei;
	short ok;
	int b;
	
	get_status_editipo();

	ei= G.sipo->editipo;
	for(b=0; b<G.sipo->totipo; b++, ei++) {
		if (ISPOIN3(ei, flag & IPO_VISIBLE, icu, icu->bezt)) {
			ok= 0;
			if(G.sipo->showkey) ok= 1;
			else if(totipo_vert && (ei->flag & IPO_EDIT)) ok= 2;
			else if(totipo_vert==0 && (ei->flag & IPO_SELECT)) ok= 3;
			
			if(ok) {
				IpoCurve *icu= ei->icu;
				BezTriple *bezt;
				int i, x, totSel = 0;
				
				/* check if enough points */
				if (icu->totvert >= 3) {
					/* first loop through - count how many verts are selected, and fix up handles */
					bezt= icu->bezt;
					for (i=0; i < icu->totvert; i++, bezt++) {						
						if (BEZSELECTED(bezt)) {							
							/* line point's handles up with point's vertical position */
							bezt->vec[0][1]= bezt->vec[2][1]= bezt->vec[1][1];
							if ((bezt->h1==HD_AUTO) || (bezt->h1==HD_VECT)) bezt->h1= HD_ALIGN;
							if ((bezt->h2==HD_AUTO) || (bezt->h2==HD_VECT)) bezt->h2= HD_ALIGN;
							
							/* add value to total */
							totSel++;
						}
					}
					
					/* if any points were selected, allocate tSmooth_Bezt points to work on */
					if (totSel >= 3) {
						tSmooth_Bezt *tarray, *tsb;
						
						/* allocate memory in one go */
						tsb= tarray= MEM_callocN(totSel*sizeof(tSmooth_Bezt), "tSmooth_Bezt Array");
						
						/* populate tarray with data of selected points */
						bezt= icu->bezt;
						for (i=0, x=0; (i < icu->totvert) && (x < totSel); i++, bezt++) {
							if (BEZSELECTED(bezt)) {
								/* tsb simply needs pointer to vec, and index */
								tsb->h1 = &bezt->vec[0][1];
								tsb->h2 = &bezt->vec[1][1];
								tsb->h3 = &bezt->vec[2][1];
								
								/* advance to the next tsb to populate */
								if (x < totSel- 1) 
									tsb++;
								else
									break;
							}
						}
						
						/* calculate the new smoothed ipo's with weighted averages:
						 *	- this is done with two passes
						 *	- uses 5 points for each operation (which stores in the relevant handles)
						 *	-	previous: w/a ratio = 3:5:2:1:1
						 *	- 	next: w/a ratio = 1:1:2:5:3
						 */
						
						/* round 1: calculate previous and next */ 
						tsb= tarray;
						for (i=0; i < totSel; i++, tsb++) {
							/* don't touch end points (otherwise, curves slowly explode) */
							if (ELEM(i, 0, (totSel-1)) == 0) {
								tSmooth_Bezt *tP1 = tsb - 1;
								tSmooth_Bezt *tP2 = (i-2 > 0) ? (tsb - 2) : (NULL);
								tSmooth_Bezt *tN1 = tsb + 1;
								tSmooth_Bezt *tN2 = (i+2 < totSel) ? (tsb + 2) : (NULL);
								
								float p1 = *tP1->h2;
								float p2 = (tP2) ? (*tP2->h2) : (*tP1->h2);
								float c1 = *tsb->h2;
								float n1 = *tN1->h2;
								float n2 = (tN2) ? (*tN2->h2) : (*tN1->h2);
								
								/* calculate previous and next */
								*tsb->h1= (3*p2 + 5*p1 + 2*c1 + n1 + n2) / 12;
								*tsb->h3= (p2 + p1 + 2*c1 + 5*n1 + 3*n2) / 12;
							}
						}
						
						/* round 2: calculate new values and reset handles */
						tsb= tarray;
						for (i=0; i < totSel; i++, tsb++) {
							/* calculate new position by averaging handles */
							*tsb->h2 = (*tsb->h1 + *tsb->h3) / 2;
							
							/* reset handles now */
							*tsb->h1 = *tsb->h2;
							*tsb->h3 = *tsb->h2;
						}
						
						/* free memory required for tarray */
						MEM_freeN(tarray);
					}
				}
				
				/* recalculate handles */
				calchandles_ipocurve(icu);
			}
		}
	}
	
	editipo_changed(G.sipo, 1);
	BIF_undo_push("Smooth IPO");
}

void join_ipo_menu(void)
{
	int mode = 0;
	mode= pupmenu("Join %t|All Selected %x1|Selected Doubles %x2");
	
	if (mode == -1) return;
	
	join_ipo(mode);
}

void join_ipo(int mode)
{
	EditIpo *ei;
	IpoKey *ik;
	IpoCurve *icu;
	BezTriple *bezt, *beztn, *newb;
	float val;
	int tot, a, b;
	
	get_status_editipo();
	
	/* Mode events:
	 * All Selected: 1
	 * Selected Doubles: 2
	 */
	
	if( mode==2 ) {
		remove_doubles_ipo();
		return;
	}
	
	/* first: multiple selected verts in 1 curve */
	ei= G.sipo->editipo;
	for(a=0; a<G.sipo->totipo; a++, ei++) {
		if (ISPOIN3(ei, flag & IPO_VISIBLE, icu, icu->bezt)) {
			if(G.sipo->showkey || (ei->flag & IPO_EDIT)) {
				icu= ei->icu;
				
				/* how many points */
				tot= 0;
				b= icu->totvert;
				bezt= icu->bezt;
				while(b--) {
					if(bezt->f2 & SELECT) tot++;
					bezt++;
				}
				
				if(tot>1) {
					tot--;
					icu->totvert-= tot;
					
					newb= MEM_mallocN(icu->totvert*sizeof(BezTriple), "bezt");
					/* the first point is the new one */
					beztn= newb+1;
					tot= 0;
					
					bezt= icu->bezt;
					b= icu->totvert+tot+1;
					while(b--) {
						
						if(bezt->f2 & SELECT) {
							if(tot==0) *newb= *bezt;
							else {
								VecAddf(newb->vec[0], newb->vec[0], bezt->vec[0]);
								VecAddf(newb->vec[1], newb->vec[1], bezt->vec[1]);
								VecAddf(newb->vec[2], newb->vec[2], bezt->vec[2]);
							}
							tot++;
						}
						else {
							*beztn= *bezt;
							beztn++;
						}
						bezt++;
					}
					
					VecMulf(newb->vec[0], (float)(1.0/((float)tot)));
					VecMulf(newb->vec[1], (float)(1.0/((float)tot)));
					VecMulf(newb->vec[2], (float)(1.0/((float)tot)));
					
					MEM_freeN(icu->bezt);
					icu->bezt= newb;
					
					sort_time_ipocurve(icu);
					calchandles_ipocurve(icu);
				}
			}
		}
	}
	
	/* next: in keymode: join multiple selected keys */
	
	editipo_changed(G.sipo, 1);	/* makes ipokeys again! */
	
	if(G.sipo->showkey) {
		ik= G.sipo->ipokey.first;
		val= 0.0;
		tot= 0;
		while(ik) {
			if(ik->flag & 1) {
				for(a=0; a<G.sipo->totipo; a++) {
					if(ik->data[a]) {
						val+= ik->data[a]->vec[1][0];
						break;
					}
				}
				tot++;
			}
			ik= ik->next;
		}
		if(tot>1) {
			val/= (float)tot;
			
			ik= G.sipo->ipokey.first;
			while(ik) {
				if(ik->flag & 1) {
					for(a=0; a<G.sipo->totipo; a++) {
						if(ik->data[a]) {
							ik->data[a]->vec[1][0]= val;
						}
					}
				}
				ik= ik->next;
			}
			editipo_changed(G.sipo, 0);
		}
	}
	deselectall_editipo();
	BIF_undo_push("Join Ipo");
}

void ipo_snap_menu(void)
{
	short event;
	
	event= pupmenu("Snap %t|Horizontal %x1|To Next %x2|To Frame %x3|To Current Frame%x4");
	if(event < 1) return;

	ipo_snap(event);
}

void ipo_snap(short event)
{
	EditIpo *ei;
	BezTriple *bezt;
	float dx = 0.0;
	int a, b;
	short ok, ok2;
	
	/* events:
	 * Horizontal : 1
	 * To Next: 2
	 * To Frame: 3
	 * To Current Frame: 4
	 */
	 
	get_status_editipo();
	
	/* map ipo-points for editing if scaled ipo */
	if (NLA_IPO_SCALED) {
		actstrip_map_ipo_keys(OBACT, G.sipo->ipo, 0, 0);
	}

	ei= G.sipo->editipo;
	for(b=0; b<G.sipo->totipo; b++, ei++) {
		if (ISPOIN3(ei, flag & IPO_VISIBLE, icu, icu->bezt)) {
		
			ok2= 0;
			if(G.sipo->showkey) ok2= 1;
			else if(totipo_vert && (ei->flag & IPO_EDIT)) ok2= 2;
			else if(totipo_vert==0 && (ei->flag & IPO_SELECT)) ok2= 3;
			
			if(ok2) {
				bezt= ei->icu->bezt;
				a= ei->icu->totvert;
				while(a--) {
					ok= 0;
					if(totipo_vert) {
						if(bezt->f2 & SELECT) ok= 1;
					}
					else ok= 1;
					
					if(ok) {
						if(event==1) {
							bezt->vec[0][1]= bezt->vec[2][1]= bezt->vec[1][1];
							if(bezt->h1==HD_AUTO || bezt->h1==HD_VECT) bezt->h1= HD_ALIGN;
							if(bezt->h2==HD_AUTO || bezt->h2==HD_VECT) bezt->h2= HD_ALIGN;
						}
						else if(event==2) {
							if(a) {
								bezt->vec[0][1]= bezt->vec[1][1]= bezt->vec[2][1]= (bezt+1)->vec[1][1];
								if(bezt->h1==HD_AUTO || bezt->h1==HD_VECT) bezt->h1= HD_ALIGN;
								if(bezt->h2==HD_AUTO || bezt->h2==HD_VECT) bezt->h2= HD_ALIGN;
							}
						}
						else if(event==3) {
							bezt->vec[1][0]= (float)(floor(bezt->vec[1][0]+0.5));
						}
						else if(event==4) {	/* to current frame */
							
							if(ok2==1 || ok2==2) {
								
								if(G.sipo->blocktype==ID_SEQ) {
									Sequence *seq;
							
									seq= (Sequence *)G.sipo->from;
									if(seq && (seq->flag & SEQ_IPO_FRAME_LOCKED) == 0) {
										dx= (float)(CFRA-seq->startdisp);
										dx= (float)(100.0*dx/((float)(seq->enddisp-seq->startdisp)));
										
										dx-= bezt->vec[1][0];
									} else {
										dx= G.scene->r.framelen*CFRA - bezt->vec[1][0];
									}
								}
								else dx= G.scene->r.framelen*CFRA - bezt->vec[1][0];
								
								bezt->vec[0][0]+= dx;
								bezt->vec[1][0]+= dx;
								bezt->vec[2][0]+= dx;
							}
						}
					}
					
					bezt++;
				}
				calchandles_ipocurve(ei->icu);
			}
		}
	}
	
	/* undo mapping of ipo-points for editing if scaled ipo */
	if (NLA_IPO_SCALED) {
		actstrip_map_ipo_keys(OBACT, G.sipo->ipo, 1, 0);
	}
	
	editipo_changed(G.sipo, 1);
	BIF_undo_push("Snap Ipo");
}

void ipo_mirror_menu(void)
{
	int mode = 0;
	mode= pupmenu("Mirror Over%t|Current Frame%x1|Vertical Axis%x2|Horizontal Axis%x3");
	
	if (mode == -1) return;
	
	ipo_mirror(mode);
}

void ipo_mirror(short mode)
{
	EditIpo *ei;
	BezTriple *bezt;
	
	int a, b;
	short ok, ok2, i;
	float diff;
	
	/* what's this for? */
	get_status_editipo();

	/* get edit ipo */
	ei= G.sipo->editipo;
	if (!ei) return;
	
	/* map ipo-points for editing if scaled ipo */
	if (NLA_IPO_SCALED) {
		actstrip_map_ipo_keys(OBACT, G.sipo->ipo, 0, 0);
	}
	
	/* look throught ipo curves */
	for(b=0; b<G.sipo->totipo; b++, ei++) {
		if (ISPOIN3(ei, flag & IPO_VISIBLE, icu, icu->bezt)) {
		
			ok2= 0;
			if(G.sipo->showkey) ok2= 1;
			else if(totipo_vert && (ei->flag & IPO_EDIT)) ok2= 2;
			else if(totipo_vert==0 && (ei->flag & IPO_SELECT)) ok2= 3;
			
			if(ok2) {
				bezt= ei->icu->bezt;
				a= ei->icu->totvert;
				
				/* loop through beztriples, mirroring them */
				while(a--) {
					ok= 0;
					if(totipo_vert) {
						if(bezt->f2 & SELECT) ok= 1;
					}
					else ok= 1;
					
					if(ok) {
						switch (mode) {
							case 1: /* mirror over current frame */
							{
								for (i=0; i<3; i++) {
									diff= ((float)CFRA - bezt->vec[i][0]);
									bezt->vec[i][0]= ((float)CFRA + diff);
								}
							}
								break;
							case 2: /* mirror over vertical axis (frame 0) */
							{
								for (i=0; i<3; i++) {
									diff= (0.0f - bezt->vec[i][0]);
									bezt->vec[i][0]= (0.0f + diff);
								}
							}
								break;
							case 3: /* mirror over horizontal axis */
							{
								for (i=0; i<3; i++) {
									diff= (0.0f - bezt->vec[i][1]);
									bezt->vec[i][1]= (0.0f + diff);
								}
							}
								break;
						}
					}
					
					bezt++;
				}
				
				/* sort out order and handles */
				sort_time_ipocurve(ei->icu);
				calchandles_ipocurve(ei->icu);
			}
		}
	}
	
	/* undo mapping of ipo-points for editing if scaled ipo */
	if (NLA_IPO_SCALED) {
		actstrip_map_ipo_keys(OBACT, G.sipo->ipo, 1, 0);
	}
	
	/* cleanup and undo push */
	editipo_changed(G.sipo, 1);
	BIF_undo_push("Mirror Ipo");
}

/*
 * When deleting an IPO curve from Python, check if the Ipo is being
 * edited and if so clear the pointer to the old curve.
 */

void del_ipoCurve ( IpoCurve * icu )
{
	int i;
	EditIpo *ei= G.sipo->editipo;
	if (!ei) return;

	for(i=0; i<G.sipo->totipo; i++, ei++) {
                if ( ei->icu == icu ) {
			ei->flag &= ~(IPO_SELECT | IPO_EDIT);
			ei->icu= NULL;
			return;
		}
	}
}

void del_ipo(int need_check)
{
	EditIpo *ei;
	BezTriple *bezt, *bezt1;
	int a, b;
	int del, event;

	get_status_editipo();
	if(G.sipo->ipo && G.sipo->ipo->id.lib) return;
	
	if(totipo_edit==0 && totipo_sel==0 && totipo_vertsel==0) {
		if (need_check) {
			if(okee("Erase selected keys"))
			   delete_key(OBACT);
		}
		else 
			delete_key(OBACT);
		return;
	}
	
	if (need_check)
		if( okee("Erase selected")==0 ) return;

	// first round, can we delete entire parts? 
	ei= G.sipo->editipo;
	for(a=0; a<G.sipo->totipo; a++, ei++) {
	
		del= 0;
		
		if(G.sipo->showkey==0 && totipo_edit==0) {
			if (ISPOIN3(ei, flag & IPO_VISIBLE, flag & IPO_SELECT, icu)) {
				del= 1;
			}
		}
		else {
			if (ISPOIN(ei, flag & IPO_VISIBLE, icu)) {
				if(G.sipo->showkey || (ei->flag & IPO_EDIT)) {
					if(ei->icu->bezt) {
						bezt= ei->icu->bezt;
						b= ei->icu->totvert;
						if(b) {
							while(b) {
								if( BEZSELECTED(bezt) );
								else break;
								b--;
								bezt++;
							}
							if(b==0) del= 1;
						}
					}
				}
			}
		}
		
		if(del) {
			if(ei->icu->driver==NULL) {
				BLI_remlink( &(G.sipo->ipo->curve), ei->icu);
				
				free_ipo_curve(ei->icu);
				
				ei->flag &= ~IPO_SELECT;
				ei->flag &= ~IPO_EDIT;
				ei->icu= NULL;	
			}
			else {
				if(ei->icu->bezt) MEM_freeN(ei->icu->bezt);
				ei->icu->bezt= NULL;
				ei->icu->totvert= 0;
				ei->flag &= ~IPO_EDIT;
			}
		}
	}
	
	// 2nd round, small parts: just curves 
	ei= G.sipo->editipo;
	for(b=0; b<G.sipo->totipo; b++, ei++) {
		if (ISPOIN(ei, flag & IPO_VISIBLE, icu)) {
			if(G.sipo->showkey || (ei->flag & IPO_EDIT)) {
			
				event= 0;
				if(ei->icu->bezt) {
				
					bezt= ei->icu->bezt;
					for(a=0; a<ei->icu->totvert; a++) {
						if( BEZSELECTED(bezt) ) {
							memmove(bezt, bezt+1, (ei->icu->totvert-a-1)*sizeof(BezTriple));
							ei->icu->totvert--;
							a--;
							event= 1;
						}
						else bezt++;
					}
					if(event) {
						if(ei->icu->totvert) {
							bezt1 = (BezTriple*) MEM_mallocN(ei->icu->totvert * sizeof(BezTriple), "delNurb");
							memcpy(bezt1, ei->icu->bezt, (ei->icu->totvert)*sizeof(BezTriple) );
							MEM_freeN(ei->icu->bezt);
							ei->icu->bezt= bezt1;
						}
						else {
							MEM_freeN(ei->icu->bezt);
							ei->icu->bezt= NULL;
						}
					}
				}
			}
		}
	}
	
	get_status_editipo();	/* count again */
	check_active_editipo();
	editipo_changed(G.sipo, 1);
	
	BIF_undo_push("Delete Ipo");
	allqueue(REDRAWNLA, 0);
	allqueue (REDRAWACTION, 0);
	allqueue(REDRAWIPO, 0);
	allspace(REMAKEIPO, 0);
}

/* ******************** copy paste buffer ******************** */
ListBase ipocopybuf={0, 0};
int totipocopybuf=0;

void free_ipocopybuf(void)
{
	IpoCurve *icu;
	
	while( (icu= ipocopybuf.first) ) {
		BLI_remlink(&ipocopybuf, icu);
		free_ipo_curve(icu);
	}
	totipocopybuf= 0;
}

void copy_editipo(void)
{
	EditIpo *ei;
	IpoCurve *icu;
	int a;
	
	if(G.sipo->showkey) {
		error("cannot copy\n");
		return;
	}
	
	free_ipocopybuf();
	
	ei= G.sipo->editipo;
	for(a=0; a<G.sipo->totipo; a++, ei++) {
		if (ISPOIN(ei, flag & IPO_VISIBLE, icu)) {
			if( (ei->flag & IPO_EDIT) || (ei->flag & IPO_SELECT) ) {
				icu= MEM_callocN(sizeof(IpoCurve), "ipocopybuf");
				*icu= *(ei->icu);
				BLI_addtail(&ipocopybuf, icu);
				icu->bezt= MEM_dupallocN(icu->bezt);
				icu->driver= MEM_dupallocN(icu->driver);

				totipocopybuf++;
			}
		}
	}
	
	if(totipocopybuf==0) error("Copy buffer is empty");
}

void paste_editipo(void)
{
	EditIpo *ei;
	IpoCurve *icu;
	int a;
	
	if (G.sipo->showkey) return;
	
	if (totipocopybuf==0) return;
	if (G.sipo->ipo==0) return;
	if (G.sipo->ipo && G.sipo->ipo->id.lib) return;

	get_status_editipo();
	
	if (totipo_vis==0) {
		error("No visible channels");
		return;
	}
	else if (totipo_vis!=totipocopybuf && totipo_sel!=totipocopybuf) {
		error("Incompatible paste");
		return;
	}
	
	icu= ipocopybuf.first;
	
	for (a=0, ei=G.sipo->editipo; a<G.sipo->totipo; a++, ei++) {
		if (ei->flag & IPO_VISIBLE) {
			/* don't attempt pasting if no valid buffer-curve to paste from anymore */
			if (icu == 0) return;
			
			/* if in editmode, paste keyframes */ 
			if (ei->flag & IPO_EDIT) {
				BezTriple *bezt;
				float offset= 0.0f;
				short offsetInit= 0;
				int i;
				
				/* make sure an ipo-curve exists (it may not, as this is an editipo) */
				ei->icu= verify_ipocurve(G.sipo->from, G.sipo->blocktype, G.sipo->actname, G.sipo->constname, G.sipo->bonename, ei->adrcode);
				if (ei->icu == NULL) return;
				
				/* Copy selected beztriples from source icu onto this edit-icu,
				 * with all added keyframes being offsetted by the difference between
				 * the first source keyframe and the current frame.
				 */
				for (i=0, bezt=icu->bezt; i < icu->totvert; i++, bezt++) {
					/* skip if not selected */
					if (BEZSELECTED(bezt) == 0) continue;
					
					/* initialise offset (if not already done) */
					if (offsetInit==0) {
						offset= CFRA - bezt->vec[1][0];
						offsetInit= 1;
					}
					/* temporarily apply offset to src beztriple while copying */
					bezt->vec[0][0] += offset;
					bezt->vec[1][0] += offset;
					bezt->vec[2][0] += offset;
						
					/* insert the keyframe */
					insert_bezt_icu(ei->icu, bezt);
					
					/* un-apply offset from src beztriple after copying */
					bezt->vec[0][0] -= offset;
					bezt->vec[1][0] -= offset;
					bezt->vec[2][0] -= offset;
				}
				
				/* recalculate handles of curve that data was pasted into */
				calchandles_ipocurve(ei->icu);
				
				/* advance to next copy/paste buffer ipo-curve */
				icu= icu->next;
			}
			
			/* otherwise paste entire curve data */
			else  {
				
				/* make sure an ipo-curve exists (it may not, as this is an editipo) */
				ei->icu= verify_ipocurve(G.sipo->from, G.sipo->blocktype, G.sipo->actname, G.sipo->constname, G.sipo->bonename, ei->adrcode);
				if (ei->icu==NULL) return;
				
				/* clear exisiting dynamic memory (keyframes, driver) */
				if (ei->icu->bezt) MEM_freeN(ei->icu->bezt);
				ei->icu->bezt= NULL;
				if (ei->icu->driver) MEM_freeN(ei->icu->driver);
				ei->icu->driver= NULL;
				
				ei->icu->totvert= icu->totvert;
				ei->icu->flag= ei->flag= icu->flag;
				ei->icu->extrap= icu->extrap;
				ei->icu->ipo= icu->ipo;
				
				/* make a copy of the source icu's data */
				if (icu->bezt)
					ei->icu->bezt= MEM_dupallocN(icu->bezt);
				if (icu->driver)
					ei->icu->driver= MEM_dupallocN(icu->driver);
				
				/* advance to next copy/paste buffer ipo-curve */
				icu= icu->next;
			}
		}
	}
	
	editipo_changed(G.sipo, 1);
	BIF_undo_push("Paste Ipo curves");
}

/* *********************** */


static int find_other_handles(EditIpo *eicur, float ctime, BezTriple **beztar)
{
	EditIpo *ei;
	BezTriple *bezt;
	int a, b, c= 1, totvert;
	
	ei= G.sipo->editipo;
	for(a=0; a<G.sipo->totipo; a++, ei++) {
		if(ei!=eicur && ei->icu && (ei->flag & IPO_VISIBLE)) {
			
			bezt= ei->icu->bezt;
			totvert= ei->icu->totvert;
			
			for(b=0; b<totvert; b++, bezt++) {
				if( bezt->vec[1][0] < ctime+IPOTHRESH &&  bezt->vec[1][0] > ctime-IPOTHRESH) {
					if(c>2) return 0;
					beztar[c]= bezt;
					c++;
				}
			}
		}
	}
	
	if(c==3) return 1;
	return 0;
}

void set_speed_editipo(float speed)
{
	EditIpo *ei;
	BezTriple *bezt, *beztar[3];
	float vec1[3], vec2[3];
	int a, b, totvert, didit=0, done_error = 0;
		
	if(G.sipo->ipo && G.sipo->ipo->id.lib) return;

	/* starting with 1 visible curve, selected point, associated points: do lencorr! */

	ei= G.sipo->editipo;
	for(a=0; a<G.sipo->totipo; a++, ei++) {
		if (ISPOIN(ei, flag & IPO_VISIBLE, icu)) {
			bezt= ei->icu->bezt;
			totvert= ei->icu->totvert;
			
			for(b=0; b<totvert; b++, bezt++) {
				if(BEZSELECTED(bezt)) {
					
					beztar[0]= bezt;
					
					if( find_other_handles(ei, bezt->vec[1][0], beztar) ) {
						beztar[0]->h1= beztar[0]->h2= HD_ALIGN;
						beztar[1]->h1= beztar[1]->h2= HD_ALIGN;
						beztar[2]->h1= beztar[2]->h2= HD_ALIGN;
						
						vec1[0]= (beztar[0]->vec[1][1] - beztar[0]->vec[0][1]) / (beztar[0]->vec[1][0] - beztar[0]->vec[0][0]) ;
						vec2[0]= (beztar[0]->vec[1][1] - beztar[0]->vec[2][1]) / (beztar[0]->vec[2][0] - beztar[0]->vec[1][0]) ;
						
						vec1[1]= (beztar[1]->vec[1][1] - beztar[1]->vec[0][1]) / (beztar[1]->vec[1][0] - beztar[1]->vec[0][0]) ;
						vec2[1]= (beztar[1]->vec[1][1] - beztar[1]->vec[2][1]) / (beztar[1]->vec[2][0] - beztar[1]->vec[1][0]) ;
						
						vec1[2]= (beztar[2]->vec[1][1] - beztar[2]->vec[0][1]) / (beztar[2]->vec[1][0] - beztar[2]->vec[0][0]) ;
						vec2[2]= (beztar[2]->vec[1][1] - beztar[2]->vec[2][1]) / (beztar[2]->vec[2][0] - beztar[2]->vec[1][0]) ;
						
						Normalize(vec1);
						Normalize(vec2);
						
						VecMulf(vec1, speed);
						VecMulf(vec2, speed);
						
						beztar[0]->vec[0][1]= beztar[0]->vec[1][1] - vec1[0]*(beztar[0]->vec[1][0] - beztar[0]->vec[0][0]) ;
						beztar[0]->vec[2][1]= beztar[0]->vec[1][1] - vec2[0]*(beztar[0]->vec[2][0] - beztar[0]->vec[1][0]) ;
						
						beztar[1]->vec[0][1]= beztar[1]->vec[1][1] - vec1[1]*(beztar[1]->vec[1][0] - beztar[1]->vec[0][0]) ;
						beztar[1]->vec[2][1]= beztar[1]->vec[1][1] - vec2[1]*(beztar[1]->vec[2][0] - beztar[1]->vec[1][0]) ;
						
						beztar[2]->vec[0][1]= beztar[2]->vec[1][1] - vec1[2]*(beztar[2]->vec[1][0] - beztar[2]->vec[0][0]) ;
						beztar[2]->vec[2][1]= beztar[2]->vec[1][1] - vec2[2]*(beztar[2]->vec[2][0] - beztar[2]->vec[1][0]) ;
						
						didit= 1;
					}
					else {
						if (done_error==0) {
							error("Only works for 3 visible curves with handles");
						}
						done_error = 1;
					}
				}
			}
			break;	
		}
	}
	
	if(didit==0) error("Did not set speed");
	
	editipo_changed(G.sipo, 1);
	BIF_undo_push("Set speed IPO");
	allqueue(REDRAWNLA, 0);
	allqueue (REDRAWACTION, 0);
	allqueue(REDRAWIPO, 0);

}


/* **************************************************** */

/* IPOKEY:
 * 
 *   there are three ways to use this system:
 *   1. below: for drawing and editing in Ipo window
 *   2. for drawing key positions in View3D (see ipo.c and drawobject.c)
 *   3. editing keys in View3D (below and in editobject.c)
 * 
 */


void free_ipokey(ListBase *lb)
{
	IpoKey *ik;
	
	ik= lb->first;
	while(ik) {
		if(ik->data) MEM_freeN(ik->data);
		ik= ik->next;
	}
	BLI_freelistN(lb);
}


void add_to_ipokey(ListBase *lb, BezTriple *bezt, int nr, int len)
{
	IpoKey *ik, *ikn;
	
	ik= lb->first;
	while(ik) {
		
		if( ik->val==bezt->vec[1][0] ) {
			if(ik->data[nr]==0) {	/* double points! */
				ik->data[nr]= bezt;
				if(bezt->f2 & SELECT) ik->flag= 1;
				return;
			}
		}
		else if(ik->val > bezt->vec[1][0]) break;
		
		ik= ik->next;
	}	
	
	ikn= MEM_callocN(sizeof(IpoKey), "add_to_ipokey");	
	if(ik) BLI_insertlinkbefore(lb, ik, ikn);
	else BLI_addtail(lb, ikn);

	ikn->data= MEM_callocN(sizeof(float *)*len, "add_to_ipokey");
	ikn->data[nr]= bezt;
	ikn->val= bezt->vec[1][0];

	if(bezt->f2 & SELECT) ikn->flag= 1;
}

void make_ipokey(void)
{
	EditIpo *ei;
	IpoKey *ik;
	ListBase *lb;
	BezTriple *bezt;
	int a, b, sel, desel, totvert;
	
	lb= &G.sipo->ipokey;
	free_ipokey(lb);
	
	ei= G.sipo->editipo;
	if(ei==0) return;
	for(a=0; a<G.sipo->totipo; a++, ei++) {
		if (ISPOIN(ei, flag & IPO_VISIBLE, icu)) {
			bezt= ei->icu->bezt;
			totvert= ei->icu->totvert;
			
			for(b=0; b<totvert; b++, bezt++) {
				add_to_ipokey(lb, bezt, a, G.sipo->totipo);
			}
			
			ei->flag &= ~IPO_SELECT;
			ei->flag &= ~IPO_EDIT;
			ei->icu->flag= ei->flag;
		}
	}
	
	/* test selectflags & scaling */
	ik= lb->first;
	while(ik) {
		sel= desel= 0;
		for(a=0; a<G.sipo->totipo; a++) {
			if(ik->data[a]) {
				bezt= ik->data[a];
				if(bezt->f2 & SELECT) sel++;
				else desel++;
			}
		}
		if(sel && desel) sel= 0;
		for(a=0; a<G.sipo->totipo; a++) {
			if(ik->data[a]) {
				bezt= ik->data[a];
				if(sel) {
					bezt->f1 |= SELECT;
					bezt->f2 |= SELECT;
					bezt->f3 |= SELECT;
				}
				else {
					bezt->f1 &= ~SELECT;
					bezt->f2 &= ~SELECT;
					bezt->f3 &= ~SELECT;
				}
			}
		}
		if(sel) ik->flag = 1;
		else ik->flag= 0;
		
		/* map ipo-keys for drawing/editing if scaled ipo */
		if (NLA_IPO_SCALED) {
			ik->val= get_action_frame_inv(OBACT, ik->val);
		}
		
		ik= ik->next;
	}
	
	get_status_editipo();
}

void make_ipokey_transform(Object *ob, ListBase *lb, int sel)
{
	IpoCurve *icu;
	BezTriple *bezt;
	IpoKey *ik;
	int a, adrcode = 0, ok, dloc=0, drot=0, dsize=0;
	
	if(ob->ipo==NULL) return;
	if(ob->ipo->showkey==0) return;
	
	/* test: are there delta curves? */
	icu= ob->ipo->curve.first;
	while(icu) {
		if(icu->flag & IPO_VISIBLE) {
			switch(icu->adrcode) {
			case OB_DLOC_X:
			case OB_DLOC_Y:
			case OB_DLOC_Z:
				dloc= 1;
				break;
			case OB_DROT_X:
			case OB_DROT_Y:
			case OB_DROT_Z:
				drot= 1;
				break;
			case OB_DSIZE_X:
			case OB_DSIZE_Y:
			case OB_DSIZE_Z:
				dsize= 1;
				break;
			}
		}
		icu= icu->next;
	}

	icu= ob->ipo->curve.first;
	while(icu) {
		if(icu->flag & IPO_VISIBLE) {
			ok= 0;
			
			switch(icu->adrcode) {
			case OB_DLOC_X:
			case OB_DLOC_Y:
			case OB_DLOC_Z:
			case OB_DROT_X:
			case OB_DROT_Y:
			case OB_DROT_Z:
			case OB_DSIZE_X:
			case OB_DSIZE_Y:
			case OB_DSIZE_Z:
				ok= 1;
				break;
				
			case OB_LOC_X:
			case OB_LOC_Y:
			case OB_LOC_Z:
				if(dloc==0) ok= 1;
				break;
			case OB_ROT_X:
			case OB_ROT_Y:
			case OB_ROT_Z:
				if(drot==0) ok= 1;
				break;
			case OB_SIZE_X:
			case OB_SIZE_Y:
			case OB_SIZE_Z:
				if(dsize==0) ok= 1;
				break;
			}
			if(ok) {
				for(a=0; a<OB_TOTIPO; a++) {
					if(icu->adrcode==ob_ar[a]) {
						adrcode= a;
						break;
					}
				}
			
				bezt= icu->bezt;
				a= icu->totvert;
				while(a--) {
					if(sel==0 || (bezt->f2 & SELECT)) {
						add_to_ipokey(lb, bezt, adrcode, OB_TOTIPO);
					}
					bezt++;
				}
			}
		}
		icu= icu->next;
	}
	
	
	ik= lb->first;
	while(ik) {
		/* map ipo-keys for drawing/editing if scaled ipo */
		if (NLA_IPO_SCALED) {
			ik->val= get_action_frame_inv(OBACT, ik->val);
		}
		
		ik= ik->next;
	}
}

void update_ipokey_val(void)	/* after moving vertices */
{
	IpoKey *ik;
	int a;
	
	ik= G.sipo->ipokey.first;
	while(ik) {
		for(a=0; a<G.sipo->totipo; a++) {
			if(ik->data[a]) {
				ik->val= ik->data[a]->vec[1][0];
				
				/* map ipo-keys for drawing/editing if scaled ipo */
				if (NLA_IPO_SCALED) {
					ik->val= get_action_frame_inv(OBACT, ik->val);
				}
				break;
			}
		}
		ik= ik->next;
	}
}

void set_tob_old(float *old, float *poin)
{
	old[0]= *(poin);
	old[3]= *(poin-3);
	old[6]= *(poin+3);
}

void set_ipo_pointers_transob(IpoKey *ik, TransOb *tob)
{
	BezTriple *bezt;
	int a, delta= 0;

	tob->locx= tob->locy= tob->locz= 0;
	tob->rotx= tob->roty= tob->rotz= 0;
	tob->sizex= tob->sizey= tob->sizez= 0;
	
	for(a=0; a<OB_TOTIPO; a++) {
		if(ik->data[a]) {
			bezt= ik->data[a];
		
			switch( ob_ar[a] ) {
			case OB_LOC_X:
			case OB_DLOC_X:
				tob->locx= &(bezt->vec[1][1]); break;
			case OB_LOC_Y:
			case OB_DLOC_Y:
				tob->locy= &(bezt->vec[1][1]); break;
			case OB_LOC_Z:
			case OB_DLOC_Z:
				tob->locz= &(bezt->vec[1][1]); break;
		
			case OB_DROT_X:
				delta= 1;
			case OB_ROT_X:
				tob->rotx= &(bezt->vec[1][1]); break;
			case OB_DROT_Y:
				delta= 1;
			case OB_ROT_Y:
				tob->roty= &(bezt->vec[1][1]); break;
			case OB_DROT_Z:
				delta= 1;
			case OB_ROT_Z:
				tob->rotz= &(bezt->vec[1][1]); break;
				
			case OB_SIZE_X:
			case OB_DSIZE_X:
				tob->sizex= &(bezt->vec[1][1]); break;
			case OB_SIZE_Y:
			case OB_DSIZE_Y:
				tob->sizey= &(bezt->vec[1][1]); break;
			case OB_SIZE_Z:
			case OB_DSIZE_Z:
				tob->sizez= &(bezt->vec[1][1]); break;		
			}	
		}
	}
	
	/* oldvals for e.g. undo */
	if(tob->locx) set_tob_old(tob->oldloc, tob->locx);
	if(tob->locy) set_tob_old(tob->oldloc+1, tob->locy);
	if(tob->locz) set_tob_old(tob->oldloc+2, tob->locz);
		
		/* store first oldrot, for mapping curves ('1'=10 degrees) and correct calculation */
	if(tob->rotx) set_tob_old(tob->oldrot+3, tob->rotx);
	if(tob->roty) set_tob_old(tob->oldrot+4, tob->roty);
	if(tob->rotz) set_tob_old(tob->oldrot+5, tob->rotz);
	
		/* store the first oldsize, this is not allowed to be dsize! */
	if(tob->sizex) set_tob_old(tob->oldsize+3, tob->sizex);
	if(tob->sizey) set_tob_old(tob->oldsize+4, tob->sizey);
	if(tob->sizez) set_tob_old(tob->oldsize+5, tob->sizez);

	tob->flag= TOB_IPO;
	if(delta) tob->flag |= TOB_IPODROT;
}



static int float_to_frame (float frame) 
{
	int to= (int) floor(0.5 + frame/G.scene->r.framelen );
	
	return to;	
}

void movekey_ipo(int dir)		/* only call external from view3d queue */
{
	IpoKey *ik;
	float toframe = 0.0;
	int a;
	
	if(G.sipo->showkey==0) return;

	ik= G.sipo->ipokey.first;
	if (dir==-1) {
		while (ik && float_to_frame(ik->val)<CFRA) {
			toframe= ik->val;
			ik= ik->next;
		}
	} else {
		while (ik && float_to_frame(ik->val)<=CFRA) {
			ik= ik->next;
		}
		if (ik) toframe= ik->val;
	}
	
	a= float_to_frame(toframe);
	
	if (a!=CFRA && a>0) {
		CFRA= a;
		
		update_for_newframe();
	}
	
	BIF_undo_push("Move Key");
	allqueue(REDRAWNLA, 0);
	allqueue(REDRAWACTION, 0);
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWIPO, 0);
	allspace(REMAKEIPO, 0);

}

void movekey_obipo(int dir)		/* only call external from view3d queue */
{
	Base *base;
	Object *ob;
	ListBase elems;
	IpoKey *ik;
	int a;
	float toframe= CFRA;
	
	if (!G.vd)
		return;
	
	base= FIRSTBASE;
	while(base) {
		if TESTBASELIB(base) {
			ob= base->object;
			if(ob->ipo && ob->ipo->showkey) {
				elems.first= elems.last= 0;
				make_ipokey_transform(ob, &elems, 0);

				if(elems.first) {
					ik= elems.first;
					if (dir==-1) {
						while (ik && float_to_frame(ik->val)<CFRA) {
							toframe= ik->val;
							ik= ik->next;
						}
					} else {
						while (ik && float_to_frame(ik->val)<=CFRA) {
							ik= ik->next;
						}
						if (ik) toframe= ik->val;
					}
										
					free_ipokey(&elems);
				}
			}
		}
		
		base= base->next;
	}
	
	a= float_to_frame(toframe);
	
	if (a!=CFRA && a>0) {
		CFRA= a;
		
		update_for_newframe();
	}
	
	BIF_undo_push("Move Key");
	allqueue(REDRAWNLA, 0);
	allqueue(REDRAWACTION, 0);
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWIPO, 0);
	allspace(REMAKEIPO, 0);

}
/* **************************************************** */
/* IPO TRANSFORM TOOLS 
 * 
 * Only the helper functions are stored here these days. They are here as
 * there are heaps of ugly globals which the IPO editor relies on. 
 * However, the actual transforms go through the transform system these days.
 */

/* Helper function for make_ipo_transdata, which is reponsible for associating
 * source data with transform data
 */
static void bezt_to_transdata (TransData *td, TransData2D *td2d, float *loc, float *cent, short selected, short onlytime)
{
	/* New location from td gets dumped onto the old-location of td2d, which then
	 * gets copied to the actual data at td2d->loc2d (bezt->vec[n])
	 *
	 * Due to NLA scaling, we apply NLA scaling to some of the verts here,
	 * and then that scaling will be undone after transform is done.
	 */
	
	if (NLA_IPO_SCALED) {
		td2d->loc[0] = get_action_frame_inv(OBACT, loc[0]);
		td2d->loc[1] = loc[1];
		td2d->loc[2] = 0.0f;
		td2d->loc2d = loc;
		
		/*td->flag = 0;*/ /* can be set beforehand, else make sure its set to 0 */
		td->loc = td2d->loc;
		td->center[0] = get_action_frame_inv(OBACT, cent[0]);
		td->center[1] = cent[1];
		td->center[2] = 0.0f;
		
		VECCOPY(td->iloc, td->loc);
	}
	else {
		td2d->loc[0] = loc[0];
		td2d->loc[1] = loc[1];
		td2d->loc[2] = 0.0f;
		td2d->loc2d = loc;
		
		/*td->flag = 0;*/ /* can be set beforehand, else make sure its set to 0 */
		td->loc = td2d->loc;
		VECCOPY(td->center, cent);
		VECCOPY(td->iloc, td->loc);
	}

	memset(td->axismtx, 0, sizeof(td->axismtx));
	td->axismtx[2][2] = 1.0f;

	td->ext= NULL; td->tdi= NULL; td->val= NULL;

	if (selected) {
		td->flag |= TD_SELECTED;
		td->dist= 0.0;
	}
	else
		td->dist= MAXFLOAT;
	
	if (onlytime)
		td->flag |= TD_TIMEONLY;
	
	Mat3One(td->mtx);
	Mat3One(td->smtx);
}	
 
/* This function is called by createTransIpoData and remake_ipo_transdata to
 * create the TransData and TransData2D arrays for transform. The costly counting
 * stage is only performed for createTransIpoData case, and is indicated by t->total==-1;
 */
void make_ipo_transdata (TransInfo *t)
{
	TransData *td = NULL;
	TransData2D *td2d = NULL;
	
	EditIpo *ei;
	BezTriple *bezt;
	int a, b;
	
	/* countsel and propmode are used for proportional edit, which is not yet available */
	int count=0/*, countsel=0*/;
	/*int propmode = t->flag & T_PROP_EDIT;*/
	
	/* count data and allocate memory (if needed) */
	if (t->total == 0) {
		/* count data first */
		if (totipo_vertsel) {
			/* we're probably in editmode, so only selected verts */
			count= totipo_vertsel;
		}
		else if (totipo_edit==0 && totipo_sel!=0) {
			/* we're not in editmode, so entire curves get moved */
			ei= G.sipo->editipo;
			for (a=0; a<G.sipo->totipo; a++, ei++) {
				if (ISPOIN3(ei, flag & IPO_VISIBLE, flag & IPO_SELECT, icu)) {
					if (ei->icu->bezt && ei->icu->ipo==IPO_BEZ)
						count+= 3*ei->icu->totvert;
					else 
						count+= ei->icu->totvert;
				}
			}
			if (count==0) return;
		}
		else {
			/* this case should not happen */
			return;
		}
		
		/* memory allocation */
		/*t->total= (propmode)? count: countsel;*/
		t->total= count;
		t->data= MEM_callocN(t->total*sizeof(TransData), "TransData (IPO Editor)");
			/* for each 2d vert a 3d vector is allocated, so that they can be treated just as if they were 3d verts */
		t->data2d= MEM_callocN(t->total*sizeof(TransData2D), "TransData2D (IPO Editor)");
	}
	
	td= t->data;
	td2d= t->data2d;
	
	/* add verts */
	if (totipo_vertsel) {
		/* we're probably in editmode, so only selected verts */
		ei= G.sipo->editipo;
		for (a=0; a<G.sipo->totipo; a++, ei++) {
			/* only consider those curves that are visible and are being edited/used for showkeys */
			if (ISPOIN(ei, flag & IPO_VISIBLE, icu)) {
				if ( (ei->flag & IPO_EDIT) || G.sipo->showkey) {
					if (ei->icu->bezt) {
						short onlytime= (ei->disptype==IPO_DISPBITS) ? 1 : (G.sipo->showkey) ? 1 : 0;
						bezt= ei->icu->bezt;
						
						for (b=0; b < ei->icu->totvert; b++, bezt++) {
							TransDataCurveHandleFlags *hdata = NULL;
							/* only include handles if selected, and interpolaton mode uses beztriples */
							if (ei->icu->ipo==IPO_BEZ) {
								if (bezt->f1 & SELECT) {
									hdata = initTransDataCurveHandes(td, bezt);
									bezt_to_transdata(td++, td2d++, bezt->vec[0], bezt->vec[1], 1, onlytime);
								}
								if (bezt->f3 & SELECT) {
									if (hdata==NULL) {
										hdata = initTransDataCurveHandes(td, bezt);
									}
									bezt_to_transdata(td++, td2d++, bezt->vec[2], bezt->vec[1], 1, onlytime);
								}
							}
							
							/* only include main vert if selected */
							if (bezt->f2 & SELECT) {
								
								if ((bezt->f1&SELECT)==0 && (bezt->f3&SELECT)==0) {
									if (hdata==NULL) {
										hdata = initTransDataCurveHandes(td, bezt);
									}
								}
								
								bezt_to_transdata(td++, td2d++, bezt->vec[1], bezt->vec[1], 1, onlytime);
							}
						}
						/* Sets handles based on the selection */
						testhandles_ipocurve(ei->icu);
					}
				}
			}
		}
	}
	else if (totipo_edit==0 && totipo_sel!=0) {
		/* we're not in editmode, so entire curves get moved */
		ei= G.sipo->editipo;
		for (a=0; a<G.sipo->totipo; a++, ei++) {
			/* only include curves that are visible and selected */
			if (ISPOIN3(ei, flag & IPO_VISIBLE, flag & IPO_SELECT, icu)) {
				if (ei->icu->bezt) {
					short onlytime= (ei->disptype==IPO_DISPBITS) ? 1 : (G.sipo->showkey) ? 1 : 0;
					bezt= ei->icu->bezt;
					b= ei->icu->totvert;
					
					for (b=0; b < ei->icu->totvert; b++, bezt++) {
						/* only include handles if interpolation mode is bezier not bpoint */
						if (ei->icu->ipo==IPO_BEZ) {
							bezt_to_transdata(td++, td2d++, bezt->vec[0], bezt->vec[1], 1, onlytime);
							bezt_to_transdata(td++, td2d++, bezt->vec[2], bezt->vec[1], 1, onlytime);
						}
						
						/* always include the main handle */
						bezt_to_transdata(td++, td2d++, bezt->vec[1], bezt->vec[1], 1, onlytime);
					}
				}
			}
		}
	}
}

/* ------------------------ */

/* struct for use in re-sorting BezTriples during IPO transform */
typedef struct BeztMap {
	BezTriple *bezt;
	int oldIndex; 		/* index of bezt in icu->bezt array before sorting */
	int newIndex;		/* index of bezt in icu->bezt array after sorting */
	short swapHs; 		/* swap order of handles (-1=clear; 0=not checked, 1=swap) */
} BeztMap;


/* This function converts an IpoCurve's BezTriple array to a BeztMap array
 * NOTE: this allocates memory that will need to get freed later
 */
static BeztMap *bezt_to_beztmaps (BezTriple *bezts, int totvert)
{
	BezTriple *bezt= bezts;
	BeztMap *bezm, *bezms;
	int i;
	
	/* allocate memory for this array */
	if (totvert==0 || bezts==NULL)
		return NULL;
	bezm= bezms= MEM_callocN(sizeof(BeztMap)*totvert, "BeztMaps");
	
	/* assign beztriples to beztmaps */
	for (i=0; i < totvert; i++, bezm++, bezt++) {
		bezm->bezt= bezt;
		bezm->oldIndex= i;
		bezm->newIndex= i;
	}
	
	return bezms;
}

/* This function copies the code of sort_time_ipocurve, but acts on BeztMap structs instead */
static void sort_time_beztmaps (BeztMap *bezms, int totvert)
{
	BeztMap *bezm;
	int i, ok= 1;
	
	/* keep repeating the process until nothing is out of place anymore */
	while (ok) {
		ok= 0;
		
		bezm= bezms;
		i= totvert;
		while (i--) {
			/* is current bezm out of order (i.e. occurs later than next)? */
			if (i > 0) {
				if (bezm->bezt->vec[1][0] > (bezm+1)->bezt->vec[1][0]) {
					bezm->newIndex++;
					(bezm+1)->newIndex--;
					
					SWAP(BeztMap, *bezm, *(bezm+1));
					
					ok= 1;
				}
			}
			
			/* do we need to check if the handles need to be swapped?
			 * optimisation: this only needs to be performed in the first loop
			 */
			if (bezm->swapHs == 0) {
				if ( (bezm->bezt->vec[0][0] > bezm->bezt->vec[1][0]) && 
					 (bezm->bezt->vec[2][0] < bezm->bezt->vec[1][0]) )
				{
					/* handles need to be swapped */
					bezm->swapHs = 1;
				}
				else {
					/* handles need to be cleared */
					bezm->swapHs = -1;
				}
			}
			
			bezm++;
		}	
	}
}

/* This function firstly adjusts the pointers that the transdata has to each BezTriple*/
static void beztmap_to_data (TransInfo *t, EditIpo *ei, BeztMap *bezms, int totvert)
{
	BezTriple *bezts = ei->icu->bezt;
	BeztMap *bezm;
	TransData2D *td;
	int i, j;
	char *adjusted;
	
	/* dynamically allocate an array of chars to mark whether an TransData's 
	 * pointers have been fixed already, so that we don't override ones that are
	 * already done
 	 */
	adjusted= MEM_callocN(t->total, "beztmap_adjusted_map");
	
	/* for each beztmap item, find if it is used anywhere */
	bezm= bezms;
	for (i= 0; i < totvert; i++, bezm++) {
		/* loop through transdata, testing if we have a hit 
		 * for the handles (vec[0]/vec[2]), we must also check if they need to be swapped...
		 */
		td= t->data2d;
		for (j= 0; j < t->total; j++, td++) {
			/* skip item if already marked */
			if (adjusted[j] != 0) continue;
			
			if (totipo_vertsel) {
				/* only selected verts */
				if (ei->icu->ipo==IPO_BEZ) {
					if (bezm->bezt->f1 & SELECT) {
						if (td->loc2d == bezm->bezt->vec[0]) {
							if (bezm->swapHs == 1)
								td->loc2d= (bezts + bezm->newIndex)->vec[2];
							else
								td->loc2d= (bezts + bezm->newIndex)->vec[0];
							adjusted[j] = 1;
						}
					}
					if (bezm->bezt->f3 & SELECT) {
						if (td->loc2d == bezm->bezt->vec[2]) {
							if (bezm->swapHs == 1)
								td->loc2d= (bezts + bezm->newIndex)->vec[0];
							else
								td->loc2d= (bezts + bezm->newIndex)->vec[2];
							adjusted[j] = 1;
						}
					}
				}
				if (bezm->bezt->f2 & SELECT) {
					if (td->loc2d == bezm->bezt->vec[1]) {
						td->loc2d= (bezts + bezm->newIndex)->vec[1];
						adjusted[j] = 1;
					}
				}
			}
			else {
				/* whole curve */
				if (ei->icu->ipo==IPO_BEZ) {
					if (td->loc2d == bezm->bezt->vec[0]) {
						if (bezm->swapHs == 1)
							td->loc2d= (bezts + bezm->newIndex)->vec[2];
						else
							td->loc2d= (bezts + bezm->newIndex)->vec[0];
						adjusted[j] = 1;
					}
					
					if (td->loc2d == bezm->bezt->vec[2]) {
						if (bezm->swapHs == 1)
							td->loc2d= (bezts + bezm->newIndex)->vec[0];
						else
							td->loc2d= (bezts + bezm->newIndex)->vec[2];
						adjusted[j] = 1;
					}
				}
				if (td->loc2d == bezm->bezt->vec[1]) {
					td->loc2d= (bezts + bezm->newIndex)->vec[1];
					adjusted[j] = 1;
				}
			}
		}
		
	}
	
	/* free temp memory used for 'adjusted' array */
	MEM_freeN(adjusted);
}

/* This function is called by recalcData during the Transform loop to recalculate 
 * the handles of curves and sort the keyframes so that the curves draw correctly.
 * It is only called if some keyframes have moved out of order.
 */
void remake_ipo_transdata (TransInfo *t)
{
	EditIpo *ei;
	int a;
	
	/* sort and reassign verts */
	ei= G.sipo->editipo;
	for (a=0; a<G.sipo->totipo; a++, ei++) {
		if (ISPOIN(ei, flag & IPO_VISIBLE, icu)) {
			if (ei->icu->bezt) {
				BeztMap *bezm;
				
				/* adjust transform-data pointers */
				bezm= bezt_to_beztmaps(ei->icu->bezt, ei->icu->totvert);
				sort_time_beztmaps(bezm, ei->icu->totvert);
				beztmap_to_data(t, ei, bezm, ei->icu->totvert);
				
				/* re-sort actual beztriples (perhaps this could be done using the beztmaps to save time?) */
				sort_time_ipocurve(ei->icu);
				
				/* free mapping stuff */
				MEM_freeN(bezm);
				
				/* make sure handles are all set correctly */
				testhandles_ipocurve(ei->icu);
			}
		}
	}
		
	/* remake ipokeys */
	if (G.sipo->showkey) make_ipokey();
}

/* This function acts as the entrypoint for transforms in the IPO editor (as for
 * the Action and NLA editors). The actual transform loop is not here anymore.
 */
void transform_ipo (int mode)
{
	short tmode;
	short context = (U.flag & USER_DRAGIMMEDIATE)?CTX_TWEAK:CTX_NONE;
	
	/* data-validation */
	if (G.sipo->ipo && G.sipo->ipo->id.lib) return;
	if (G.sipo->editipo==0) return;
	
	/* convert ascii-based mode to transform system constants (mode) */
	switch (mode) {
		case 'g':	
			tmode= TFM_TRANSLATION;
			break;
		case 'r':
			tmode= TFM_ROTATION;
			break;
		case 's':
			tmode= TFM_RESIZE;
			break;
		default:
			tmode= 0;
			return;
	}
	
	/* the transform system method involved depends on the selection */
	get_status_editipo();
	if (totipo_vertsel) {
		/* we're probably in editmode, so only selected verts - transform system */
		initTransform(tmode, context);
		Transform();
	}
	else if (totipo_edit==0 && totipo_sel!=0) {
		/* we're not in editmode, so entire curves get moved - transform system*/
		initTransform(tmode, context);
		Transform();
	}
	else {
		/* shapekey mode? special transform code */
		if (totipo_edit==0) 
			move_keys(OBACT);
		return;
	}
	
	/* cleanup */
	editipo_changed(G.sipo, 1);
}

/**************************************************/

void filter_sampledata(float *data, int sfra, int efra)
{
	float *da;
	int a;
	
	da= data+1;
	for(a=sfra+1; a<efra; a++, da++) {
		da[0]=(float)( 0.25*da[-1] + 0.5*da[0] + 0.25*da[1]);
	}
	
}

void sampledata_to_ipocurve(float *data, int sfra, int efra, IpoCurve *icu)
{
	BezTriple *bezt;
	float *da;
	int a, tot;
	
	filter_sampledata(data, sfra, efra);
	filter_sampledata(data, sfra, efra);
	
	icu->ipo= IPO_LIN;
	
	if(icu->bezt) MEM_freeN(icu->bezt);
	icu->bezt= NULL;
	
	tot= 1;	/* first point */
	da= data+1;
	for(a=sfra+1; a<efra; a++, da++) {
		if( IS_EQ(da[0], da[1])==0 && IS_EQ(da[1], da[2])==0 ) tot++;
	}
	
	icu->totvert= tot;
	bezt= icu->bezt= MEM_callocN(tot*sizeof(BezTriple), "samplebezt");
	bezt->vec[1][0]= (float)sfra;
	bezt->vec[1][1]= data[0];
	bezt++;
	da= data+1;
	for(a=sfra+1; a<efra; a++, da++) {
		if( IS_EQ(da[0], da[1])==0 && IS_EQ(da[1], da[2])==0 ) {
			bezt->vec[1][0]= (float)a;
			bezt->vec[1][1]= da[0];
			bezt++;
		}
	}	
}

void ipo_record(void)
{
	/* only 1 or 2 active curves
	 * make a copy (ESC) 
	 *
	 * reference point is the current situation (or 0)
	 * dx (dy) is the height correction factor
	 * CTRL: start record
	 */
	extern double tottime;
	EditIpo *ei, *ei1=0, *ei2=0;
	ScrArea *sa, *oldarea;
//	Ipo *ipo;
	Object *ob;
	void *poin;
	double swaptime;
	float or1, or2 = 0.0, fac, *data1, *data2;
	int type, a, afbreek=0, firsttime=1, cfrao, cfra, sfra, efra;
	unsigned short event = 0;
	short anim, val, xn, yn, mvalo[2], mval[2];
	char str[128];
	
	if(G.sipo->from==NULL) return;
	if(SFRA>=EFRA) return;
	
	anim= pupmenu("Record Mouse %t|Still %x1|Play Animation %x2");
	if(anim < 1) return;
	if(anim!=2) anim= 0;

	ob= OBACT;
	/* find the curves... */
	
	ei= G.sipo->editipo;
	for(a=0; a<G.sipo->totipo; a++) {
		if(ei->flag & IPO_VISIBLE) {
			
			if(ei1==0) ei1= ei;
			else if(ei2==0) ei2= ei;
			else {
				error("Maximum 2 visible curves");
				return;
			}
		}
		ei++;
	}

	if(ei1==0) {
		error("Select 1 or 2 channels");
		return;
	}

	/* make curves ready, start values */
	if(ei1->icu==NULL) 
		ei1->icu= verify_ipocurve(G.sipo->from, G.sipo->blocktype, G.sipo->actname, G.sipo->constname, G.sipo->bonename, ei1->adrcode);
	if(ei1->icu==NULL) return;
	
	poin= get_ipo_poin(G.sipo->from, ei1->icu, &type);
	if(poin) ei1->icu->curval= read_ipo_poin(poin, type);
	or1= ei1->icu->curval;
	ei1->icu->flag |= IPO_LOCK;

	if(ei2) {
		if(ei2->icu==NULL)
			ei2->icu= verify_ipocurve(G.sipo->from, G.sipo->blocktype, G.sipo->actname, G.sipo->constname, G.sipo->bonename, ei2->adrcode);
		if(ei2->icu==NULL) return;
		
		poin= get_ipo_poin(G.sipo->from, ei2->icu, &type);
		if(poin) ei2->icu->curval= read_ipo_poin(poin, type);
		or2= ei2->icu->curval;
		ei2->icu->flag |= IPO_LOCK;
	}
	fac= G.v2d->cur.ymax - G.v2d->cur.ymin;
	fac/= (float)curarea->winy;

	/* which area */
	oldarea= curarea;
	sa= G.curscreen->areabase.first;
	while(sa) {
		if(sa->win) {
			if(G.sipo->blocktype==ID_MA || G.sipo->blocktype==ID_LA) {
				if(sa->spacetype==SPACE_BUTS) break;
			}
			else {
				if(sa->spacetype==SPACE_VIEW3D) break;
			}
		}		
		sa= sa->next;	
	}

	if(sa) areawinset(sa->win);

	/* can we? */
	while(get_mbut()&L_MOUSE) BIF_wait_for_statechange();
	data1= MEM_callocN(sizeof(float)*(EFRA-SFRA+1), "data1");
	data2= MEM_callocN(sizeof(float)*(EFRA-SFRA+1), "data2");
	
	getmouseco_areawin(mvalo);
	xn= mvalo[0]; yn= mvalo[1];
	waitcursor(1);

	tottime= 0.0;
	swaptime= 1.0/FPS;

	cfrao= CFRA;
	cfra=efra= SFRA;
	sfra= EFRA;

	if (G.scene->audio.flag & AUDIO_SYNC) {
		audiostream_start(cfra);
	}

	while(afbreek==0) {
		
		getmouseco_areawin(mval);
		
		if(mval[0]!= mvalo[0] || mval[1]!=mvalo[1] || firsttime || (G.qual & LR_CTRLKEY)) {
			if(anim) CFRA= cfra;
			else firsttime= 0;

			set_timecursor(cfra);
	
			/* do ipo: first all, then the specific ones */
			if(anim==2) {
				do_ob_ipo(ob);
				do_ob_key(ob);
			}

			ei1->icu->curval= or1 + fac*(mval[0]-xn);
			if(ei2) ei2->icu->curval= or2 + fac*(mval[1]-yn);

			do_ipo_nocalc(G.sipo->ipo);
			
			if(G.qual & LR_CTRLKEY) {
				sprintf(str, "Recording... %d\n", cfra);
				data1[ cfra-SFRA ]= ei1->icu->curval;
				if(ei2) data2[ cfra-SFRA ]= ei2->icu->curval;
				
				sfra= MIN2(sfra, cfra);
				efra= MAX2(efra, cfra);
			}
			else sprintf(str, "Mouse Recording. Use Ctrl to start. LeftMouse or Space to end");
			
			do_ob_key(ob);
			ob->recalc |= OB_RECALC;
			
			headerprint(str);

			if(sa) scrarea_do_windraw(sa);

			/* minimal wait swaptime */
			tottime -= swaptime;
			while (update_time(cfra)) PIL_sleep_ms(1);

			screen_swapbuffers();
			
			tottime= 0.0;
			
			mvalo[0]= mval[0];
			mvalo[1]= mval[1];
			
			if(anim || (G.qual & LR_CTRLKEY)) {
				if (G.scene->audio.flag & AUDIO_SYNC) {
					cfra = audiostream_pos();
				} else {
					cfra++;
				}
				if(cfra>EFRA) {
					cfra= SFRA;
					if (G.scene->audio.flag & AUDIO_SYNC) {
						audiostream_stop();
						audiostream_start( cfra );
					}
				}
			}
		}
		
		while(qtest()) {
			event= extern_qread(&val);
			if(val) {
				switch(event) {
				case LEFTMOUSE: case ESCKEY: case SPACEKEY: case RETKEY:
					afbreek= 1;
					break;
				}
			}
			if(afbreek) break;
		}
	}
	
	if(event!=ESCKEY) {
		sampledata_to_ipocurve(data1+sfra-SFRA, sfra, efra, ei1->icu);
		if(ei2) sampledata_to_ipocurve(data2+sfra-SFRA, sfra, efra, ei2->icu);

		/* not nice when this is on */
		if(G.sipo->showkey) {
			G.sipo->showkey= 0;
			free_ipokey(&G.sipo->ipokey);
		}
	}
	else {
		/* undo: start values */
		poin= get_ipo_poin(G.sipo->from, ei1->icu, &type);
		if(poin) write_ipo_poin(poin, type, or1);
		if(ei1->icu->bezt==NULL) {
			BLI_remlink( &(G.sipo->ipo->curve), ei1->icu);
			MEM_freeN(ei1->icu);
			ei1->icu= NULL;
		}
		if(ei2) {
			poin= get_ipo_poin(G.sipo->from, ei2->icu, &type);
			if(poin) write_ipo_poin(poin, type, or2);
			if(ei2->icu->bezt==NULL) {
				BLI_remlink( &(G.sipo->ipo->curve), ei2->icu);
				MEM_freeN(ei2->icu);
				ei2->icu= NULL;
			}
		}
	}
	
	if(ei1->icu) ei1->icu->flag &= ~IPO_LOCK;	
	if(ei2 && ei2->icu) ei2->icu->flag &= ~IPO_LOCK;	
	
	editipo_changed(G.sipo, 0);
	do_ipo(G.sipo->ipo);
	waitcursor(0);
	if (G.scene->audio.flag & AUDIO_SYNC) {
		audiostream_stop();
	}

	allqueue(REDRAWVIEW3D, 0);
	if(sa) scrarea_queue_headredraw(sa);	/* headerprint */
	scrarea_queue_redraw(oldarea);
	CFRA= cfrao;
	
	/* for the time being? */
	update_for_newframe();
	BIF_undo_push("Ipo Record");
	
	MEM_freeN(data1);
	MEM_freeN(data2);
}

/* while transform, update for curves */
void remake_object_ipos(Object *ob)
{
	IpoCurve		*icu;
	
	if (!ob)
		return;
	if (!ob->ipo)
		return;
	
	for (icu = ob->ipo->curve.first; icu; icu=icu->next){
		sort_time_ipocurve(icu);
		testhandles_ipocurve(icu);
	}
}

/* Only delete the nominated keyframe from provided ipo-curve. 
 * Not recommended to be used many times successively. For that
 * there is delete_ipo_keys(). 
 */
void delete_icu_key(IpoCurve *icu, int index, short do_recalc)
{
	/* firstly check that index is valid */
	if (index < 0) 
		index *= -1;
	if (icu == NULL) 
		return;
	if (index >= icu->totvert)
		return;
	
	/*	Delete this key */
	memmove(&icu->bezt[index], &icu->bezt[index+1], sizeof(BezTriple)*(icu->totvert-index-1));
	icu->totvert--;
	
	/* recalc handles - only if it won't cause problems */
	if (do_recalc)
		calchandles_ipocurve(icu);
}

void delete_ipo_keys(Ipo *ipo)
{
	IpoCurve *icu, *next;
	int i;
	
	if (ipo == NULL)
		return;
	
	for (icu= ipo->curve.first; icu; icu= next) {
		next = icu->next;
		
		/* Delete selected BezTriples */
		for (i=0; i<icu->totvert; i++) {
			if (icu->bezt[i].f2 & SELECT) {
				memmove(&icu->bezt[i], &icu->bezt[i+1], sizeof(BezTriple)*(icu->totvert-i-1));
				icu->totvert--;
				i--;
			}
		}
		
		/* Only delete if there isn't an ipo-driver still hanging around on an empty curve */
		if (icu->totvert==0 && icu->driver==NULL) {
			BLI_remlink(&ipo->curve, icu);
			free_ipo_curve(icu);
		}
	}
}


int add_trans_ipo_keys(Ipo *ipo, TransVert *tv, int tvtot)
{
	int i;
	IpoCurve *icu;
	
	if (!ipo)
		return tvtot;
	
	for (icu=ipo->curve.first; icu; icu=icu->next){
		for (i=0; i<icu->totvert; i++){
			if (icu->bezt[i].f2 & 1){
				tv[tvtot+0].loc=icu->bezt[i].vec[0];
				tv[tvtot+1].loc=icu->bezt[i].vec[1];
				tv[tvtot+2].loc=icu->bezt[i].vec[2];
				
				memcpy (&tv[tvtot+0].oldloc, icu->bezt[i].vec[0], sizeof (float)*3);
				memcpy (&tv[tvtot+1].oldloc, icu->bezt[i].vec[1], sizeof (float)*3);
				memcpy (&tv[tvtot+2].oldloc, icu->bezt[i].vec[2], sizeof (float)*3);
				tvtot+=3;
			}
		}
	}
	
	return tvtot;
}

void duplicate_ipo_keys(Ipo *ipo)
{
	IpoCurve *icu;
	int i;
	BezTriple *newbezt;

	if (!ipo)
		return;

	for (icu=ipo->curve.first; icu; icu=icu->next){
		for (i=0; i<icu->totvert; i++){
			/* If a key is selected */
			if (icu->bezt[i].f2 & 1){
				/* Expand the list */
				newbezt = MEM_callocN(sizeof(BezTriple) * (icu->totvert+1), "beztriple");
				memcpy (newbezt, icu->bezt, sizeof(BezTriple) * (i+1));
				memcpy (newbezt+i+1, icu->bezt+i, sizeof(BezTriple));
				memcpy (newbezt+i+2, icu->bezt+i+1, sizeof (BezTriple) *(icu->totvert-(i+1)));
				icu->totvert++;
				MEM_freeN (icu->bezt);
				icu->bezt=newbezt;
				/* Unselect the current key*/
				icu->bezt[i].f1 &= ~ 1;
				icu->bezt[i].f2 &= ~ 1;
				icu->bezt[i].f3 &= ~ 1;
				i++;
				/* Select the copied key */
				icu->bezt[i].f1 |= 1;
				icu->bezt[i].f2 |= 1;
				icu->bezt[i].f3 |= 1;
				
			}
		}
	}
}

void move_to_frame(void)
{
	EditIpo *ei;
	BezTriple *bezt;
	ID *id;
	float cfra;
	int a, b;
	
	if(G.sipo->editipo==0) return;
	
	ei= G.sipo->editipo;
	
	for(a=0; a<G.sipo->totipo; a++, ei++) {
		if (ISPOIN(ei, flag & IPO_VISIBLE, icu)) {
			if(G.sipo->showkey || (ei->flag & IPO_EDIT)) {
				
				if(ei->icu->bezt) {
					
					b= ei->icu->totvert;
					bezt= ei->icu->bezt;
					while(b--) {
						if(BEZSELECTED(bezt)) {
							
							cfra=  bezt->vec[1][0]/G.scene->r.framelen;
							
							id= G.sipo->from;
							if(id && GS(id->name)==ID_OB ) {
								Object *ob= (Object *)id;
								if((ob->ipoflag & OB_OFFS_OB) && (give_timeoffset(ob)!=0.0) ) {
									cfra+= give_timeoffset(ob)/G.scene->r.framelen;
								}
							}
							CFRA= (int)floor(cfra+0.5);
							
							if(CFRA < 1) CFRA= 1;
							update_for_newframe();
							
							break;
						}
						bezt++;
					}
				}
			}
		}
	}
	BIF_undo_push("Set frame to selected Ipo vertex");
}
