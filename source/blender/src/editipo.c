/**
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef WIN32
#include <unistd.h>
#else
#include <io.h>
#include "BLI_winstuff.h"
#endif   
#include "MEM_guardedalloc.h"
#include "PIL_time.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_editVert.h"

#include "DNA_constraint_types.h"
#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_object_types.h"
#include "DNA_lamp_types.h"
#include "DNA_sequence_types.h"
#include "DNA_sound_types.h"
#include "DNA_camera_types.h"
#include "DNA_material_types.h"
#include "DNA_key_types.h"
#include "DNA_screen_types.h"
#include "DNA_scene_types.h"
#include "DNA_ipo_types.h"
#include "DNA_curve_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"
#include "DNA_view3d_types.h"
#include "DNA_group_types.h"
#include "DNA_ika_types.h"

#include "BKE_utildefines.h"
#include "BKE_action.h"
#include "BKE_anim.h"
#include "BKE_material.h"
#include "BKE_ipo.h"
#include "BKE_key.h"
#include "BKE_ika.h"
#include "BKE_displist.h"
#include "BKE_global.h"
#include "BKE_group.h"

#include "BIF_buttons.h"
#include "BIF_editkey.h"
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

#include "BSE_trans_types.h"
#include "BSE_editipo_types.h"
#include "BSE_drawipo.h"
#include "BSE_editipo.h"
#include "BSE_editaction.h"
#include "BSE_edit.h"
#include "BSE_drawview.h"
#include "BSE_headerbuttons.h"

#include "blendef.h"
#include "mydevice.h"
#include "interface.h"
#include "render.h"

/* forwards */
#define BEZSELECTED(bezt)   (((bezt)->f1 & 1) || ((bezt)->f2 & 1) || ((bezt)->f3 & 1))

#define IPOTHRESH	0.9
#define ISPOIN(a, b, c)                       ( (a->b) && (a->c) )
#define ISPOIN3(a, b, c, d)           ( (a->b) && (a->c) && (a->d) )
#define ISPOIN4(a, b, c, d, e)        ( (a->b) && (a->c) && (a->d) && (a->e) )   

extern int ob_ar[];
extern int ma_ar[];
extern int seq_ar[];
extern int cu_ar[];
extern int key_ar[];
extern int wo_ar[];
extern int la_ar[];
extern int cam_ar[];
extern int snd_ar[];
extern int ac_ar[];
extern int co_ar[];

void getname_ac_ei(int nr, char *str)
{
	switch(nr) {
	case AC_LOC_X:
		strcpy(str, "LocX"); break;
	case AC_LOC_Y:
		strcpy(str, "LocY"); break;
	case AC_LOC_Z:
		strcpy(str, "LocZ"); break;
	case AC_SIZE_X:
		strcpy(str, "SizeX"); break;
	case AC_SIZE_Y:
		strcpy(str, "SizeY"); break;
	case AC_SIZE_Z:
		strcpy(str, "SizeZ"); break;
	case AC_QUAT_X:
		strcpy(str, "QuatX"); break;
	case AC_QUAT_Y:
		strcpy(str, "QuatY"); break;
	case AC_QUAT_Z:
		strcpy(str, "QuatZ"); break;
	case AC_QUAT_W:
		strcpy(str, "QuatW"); break;
	default:
		str[0]= 0;
	}	
}

void getname_co_ei(int nr, char *str)
{
	switch(nr){
	case CO_ENFORCE:
		strcpy(str, "Inf"); break;
	}
}

void getname_ob_ei(int nr, char *str, int colipo)
{
	switch(nr) {
	case OB_LOC_X:
		strcpy(str, "LocX"); break;
	case OB_LOC_Y:
		strcpy(str, "LocY"); break;
	case OB_LOC_Z:
		strcpy(str, "LocZ"); break;
	case OB_DLOC_X:
		strcpy(str, "dLocX"); break;
	case OB_DLOC_Y:
		strcpy(str, "dLocY"); break;
	case OB_DLOC_Z:
		strcpy(str, "dLocZ"); break;

	case OB_ROT_X:
		strcpy(str, "RotX"); break;
	case OB_ROT_Y:
		strcpy(str, "RotY"); break;
	case OB_ROT_Z:
		strcpy(str, "RotZ"); break;
	case OB_DROT_X:
		strcpy(str, "dRotX"); break;
	case OB_DROT_Y:
		strcpy(str, "dRotY"); break;
	case OB_DROT_Z:
		strcpy(str, "dRotZ"); break;
		
	case OB_SIZE_X:
		strcpy(str, "SizeX"); break;
	case OB_SIZE_Y:
		strcpy(str, "SizeY"); break;
	case OB_SIZE_Z:
		strcpy(str, "SizeZ"); break;
	case OB_DSIZE_X:
		strcpy(str, "dSizeX"); break;
	case OB_DSIZE_Y:
		strcpy(str, "dSizeY"); break;
	case OB_DSIZE_Z:
		strcpy(str, "dSizeZ"); break;
	
	case OB_LAY:
		strcpy(str, "Layer"); break;

	case OB_TIME:
		strcpy(str, "Time"); break;
	case OB_EFF_X:
		if(colipo) strcpy(str, "ColR");
		else strcpy(str, "EffX");
		break;
	case OB_EFF_Y:
		if(colipo) strcpy(str, "ColG");
		else strcpy(str, "EffY");
		break;
	case OB_EFF_Z:
		if(colipo) strcpy(str, "ColB");
		else strcpy(str, "EffZ");
		break;
	case OB_COL_A:
		strcpy(str, "ColA");
		break;
	default:
		str[0]= 0;
	}	
}

void getname_tex_ei(int nr, char *str)
{
	switch(nr) {
	case MAP_OFS_X:
		strcpy(str, "OfsX"); break;
	case MAP_OFS_Y:
		strcpy(str, "OfsY"); break;
	case MAP_OFS_Z:
		strcpy(str, "OfsZ"); break;
	case MAP_SIZE_X:
		strcpy(str, "SizeX"); break;
	case MAP_SIZE_Y:
		strcpy(str, "SizeY"); break;
	case MAP_SIZE_Z:
		strcpy(str, "SizeZ"); break;
	case MAP_R:
		strcpy(str, "texR"); break;
	case MAP_G:
		strcpy(str, "texG"); break;
	case MAP_B:
		strcpy(str, "texB"); break;
	case MAP_DVAR:
		strcpy(str, "DefVar"); break;
	case MAP_COLF:
		strcpy(str, "Col"); break;
	case MAP_NORF:
		strcpy(str, "Nor"); break;
	case MAP_VARF:
		strcpy(str, "Var"); break;
	default:
		str[0]= 0;
	}
}

void getname_mat_ei(int nr, char *str)
{
	if(nr>=MA_MAP1) getname_tex_ei((nr & (MA_MAP1-1)), str);
	else {
		switch(nr) {
		case MA_COL_R:
			strcpy(str, "R"); break;
		case MA_COL_G:
			strcpy(str, "G"); break;
		case MA_COL_B:
			strcpy(str, "B"); break;
		case MA_SPEC_R:
			strcpy(str, "SpecR"); break;
		case MA_SPEC_G:
			strcpy(str, "SpecG"); break;
		case MA_SPEC_B:
			strcpy(str, "SpecB"); break;
		case MA_MIR_R:
			strcpy(str, "MirR"); break;
		case MA_MIR_G:
			strcpy(str, "MirG"); break;
		case MA_MIR_B:
			strcpy(str, "MirB"); break;
		case MA_REF:
			strcpy(str, "Ref"); break;
		case MA_ALPHA:
			strcpy(str, "Alpha"); break;
		case MA_EMIT:
			strcpy(str, "Emit"); break;
		case MA_AMB:
			strcpy(str, "Amb"); break;
		case MA_SPEC:
			strcpy(str, "Spec"); break;
		case MA_HARD:
			strcpy(str, "Hard"); break;
		case MA_SPTR:
			strcpy(str, "SpTra"); break;
		case MA_ANG:
			strcpy(str, "Ang"); break;
		case MA_MODE:
			strcpy(str, "Mode"); break;
		case MA_HASIZE:
			strcpy(str, "HaSize"); break;
		default:
			str[0]= 0;
		}
	}
}

void getname_world_ei(int nr, char *str)
{
	if(nr>=MA_MAP1) getname_tex_ei((nr & (MA_MAP1-1)), str);
	else {
		switch(nr) {
		case WO_HOR_R:
			strcpy(str, "HorR"); break;
		case WO_HOR_G:
			strcpy(str, "HorG"); break;
		case WO_HOR_B:
			strcpy(str, "HorB"); break;
		case WO_ZEN_R:
			strcpy(str, "ZenR"); break;
		case WO_ZEN_G:
			strcpy(str, "ZenG"); break;
		case WO_ZEN_B:
			strcpy(str, "ZenB"); break;

		case WO_EXPOS:
			strcpy(str, "Expos"); break;

		case WO_MISI:
			strcpy(str, "Misi"); break;
		case WO_MISTDI:
			strcpy(str, "MisDi"); break;
		case WO_MISTSTA:
			strcpy(str, "MisSta"); break;
		case WO_MISTHI:
			strcpy(str, "MisHi"); break;

		case WO_STAR_R:
			strcpy(str, "StarR"); break;
		case WO_STAR_G:
			strcpy(str, "StarB"); break;
		case WO_STAR_B:
			strcpy(str, "StarG"); break;

		case WO_STARDIST:
			strcpy(str, "StarDi"); break;
		case WO_STARSIZE:
			strcpy(str, "StarSi"); break;
		default:
			str[0]= 0;
		}
	}
}

void getname_seq_ei(int nr, char *str)
{
	switch(nr) {
	case SEQ_FAC1:
		strcpy(str, "Fac"); break;
	default:
		str[0]= 0;
	}
}

void getname_cu_ei(int nr, char *str)
{

	switch(nr) {
	case CU_SPEED:
		strcpy(str, "Speed"); break;
	default:
		str[0]= 0;
	}
}

void getname_key_ei(int nr, char *str)
{
	if(nr==KEY_SPEED) strcpy(str, "Speed");
	else sprintf(str, "Key %d", nr);
}

void getname_la_ei(int nr, char *str)
{
	if(nr>=MA_MAP1) getname_tex_ei((nr & (MA_MAP1-1)), str);
	else {
		switch(nr) {
		case LA_ENERGY:
			strcpy(str, "Energ"); break;
		case LA_COL_R:
			strcpy(str, "R"); break;
		case LA_COL_G:
			strcpy(str, "G"); break;
		case LA_COL_B:
			strcpy(str, "B"); break;
		case LA_DIST:
			strcpy(str, "Dist"); break;
		case LA_SPOTSI:
			strcpy(str, "SpoSi"); break;
		case LA_SPOTBL:
			strcpy(str, "SpoBl"); break;
		case LA_QUAD1:
			strcpy(str, "Quad1"); break;
		case LA_QUAD2:
			strcpy(str, "Quad2"); break;
		case LA_HALOINT:
			strcpy(str, "HaInt"); break;
		default:
			str[0]= 0;
		}
	}
}

void getname_cam_ei(int nr, char *str)
{
	switch(nr) {
	case CAM_LENS:
		strcpy(str, "Lens"); break;
	case CAM_STA:
		strcpy(str, "ClSta"); break;
	case CAM_END:
		strcpy(str, "ClEnd"); break;
	default:
		str[0]= 0;
	}
}

void getname_snd_ei(int nr, char *str)
{
	switch(nr) {
	case SND_VOLUME:
		strcpy(str, "Vol"); break;
	case SND_PITCH:
		strcpy(str, "Pitch"); break;
	case SND_PANNING:
		strcpy(str, "Pan"); break;
	case SND_ATTEN:
		strcpy(str, "Atten"); break;
	default:
		str[0]= 0;
	}
}


IpoCurve *find_ipocurve(Ipo *ipo, int adrcode)
{
	if(ipo) {
		IpoCurve *icu= ipo->curve.first;
		while(icu) {
			if(icu->adrcode==adrcode) return icu;
			icu= icu->next;
		}
	}
	return NULL;
}

void boundbox_ipocurve(IpoCurve *icu)
{
	BezTriple *bezt;
	float vec[3]={0.0,0.0,0.0};
	float min[3], max[3];
	int a;
	
	if(icu->totvert) {
		INIT_MINMAX(min, max);
		
		if(icu->bezt ) {
			a= icu->totvert;
			bezt= icu->bezt;
			while(a--) {
				if(icu->vartype & IPO_BITS) {
					vec[0]= bezt->vec[1][0];
					vec[1]= 0.0;
					DO_MINMAX(vec, min, max);
					
					vec[1]= 16.0;
					DO_MINMAX(vec, min, max);
				}
				else {
					if(icu->ipo==IPO_BEZ && a!=icu->totvert-1) {
						DO_MINMAX(bezt->vec[0], min, max);
					}
					DO_MINMAX(bezt->vec[1], min, max);
					if(icu->ipo==IPO_BEZ && a!=0) {
						DO_MINMAX(bezt->vec[2], min, max);
					}
				}
				
				bezt++;
			}
		}
		if(min[0]==max[0]) max[0]= (float)(min[0]+1.0);
		if(min[1]==max[1]) max[1]= (float)(min[1]+0.1);
		
		icu->totrct.xmin= min[0];
		icu->totrct.ymin= min[1];
		icu->totrct.xmax= max[0];
		icu->totrct.ymax= max[1];
	}
	else {
		icu->totrct.xmin= icu->totrct.ymin= 0.0;
		icu->totrct.xmax= EFRA;
		icu->totrct.ymax= 1.0;
	}
}

void boundbox_ipo(Ipo *ipo, rctf *bb)
{
	IpoCurve *icu;
	int first= 1;
	
	icu= ipo->curve.first;
	while(icu) {
			
		boundbox_ipocurve(icu);
				
		if(first) {
			*bb= icu->totrct;
			first= 0;
		}
		else BLI_union_rctf(bb, &(icu->totrct));
		
		icu= icu->next;
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
		
				boundbox_ipocurve(ei->icu);
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
		key= (Key *)si->from;
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
		allqueue (REDRAWACTION, 0);
		allqueue(REDRAWNLA, 0);
		allqueue(REDRAWBUTSANIM, 0);
		
		if(si->blocktype==ID_OB) {
			Object *ob= (Object *)si->from;			
			if(ob && ob->type==OB_IKA) itterate_ika(ob);
			allqueue(REDRAWVIEW3D, 0);
			allqueue(REDRAWNLA, 0);
		}

		else if(si->blocktype==ID_MA) allqueue(REDRAWBUTSMAT, 0);
		else if(si->blocktype==ID_WO) allqueue(REDRAWBUTSWORLD, 0);
		else if(si->blocktype==ID_LA) allqueue(REDRAWBUTSLAMP, 0);
		else if(si->blocktype==ID_SO) allqueue(REDRAWBUTSSOUND, 0);
		else if(si->blocktype==ID_CA) {
			allqueue(REDRAWBUTSEDIT, 0);
			allqueue(REDRAWVIEW3D, 0);
		}
		else if(si->blocktype==ID_SEQ) clear_last_seq();
		else if(si->blocktype==ID_AC){
			do_all_actions();
			allqueue(REDRAWACTION, 0);
			allqueue(REDRAWNLA, 0);
		}
		else if(si->blocktype==ID_KE) {
			do_spec_key((Key *)si->from);
			allqueue(REDRAWVIEW3D, 0);
		}
		else if(si->blocktype==ID_CU) {
			calc_curvepath(OBACT);
			allqueue(REDRAWVIEW3D, 0);
		}
	}

	if(si->showkey) make_ipokey();
}

void scale_editipo()
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
		if ISPOIN(ei, flag & IPO_VISIBLE, icu) {
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
	allqueue(REDRAWNLA, 0);
	allqueue (REDRAWACTION, 0);
	allqueue(REDRAWIPO, 0);
}


Ipo *get_ipo_to_edit(ID **from)
{
	Object *ob= OBACT;
	
	*from= 0;
	
	
	if (G.sipo->pin) {
		*from = G.sipo->from;
		return G.sipo->ipo;
	}

	if(G.sipo->blocktype==ID_SEQ) {
		extern Sequence *last_seq;
		
		*from= (ID *)last_seq;
		if(last_seq) return last_seq->ipo;
	}
	else if(G.sipo->blocktype==IPO_CO){
		if (ob && ob->activecon){
			*from= (ID*) ob;
			return ob->activecon->ipo;
		}
	}
	else if(G.sipo->blocktype==ID_AC) {
		bActionChannel *chan;
		if (ob && ob->action){
			*from= (ID *) ob->action;
			chan= get_hilighted_action_channel(ob->action);
			if (chan)
				return chan->ipo;
			else{
				*from = NULL;
				return NULL;
			}
		}

	}
	else if(G.sipo->blocktype==ID_WO) {
		World *wo= G.scene->world;
		*from= (ID *)wo;
		if(wo) return wo->ipo;
	}
	else if(G.sipo->blocktype==ID_OB) {
		if(ob) {
			*from= (ID *)ob;
			return ob->ipo;
		}
	}
	else if(G.sipo->blocktype==ID_MA) {
		if(ob) {
			Material *ma= give_current_material(ob, ob->actcol);
			*from= (ID *)ma;
			if(ma) return ma->ipo;
		}
	}
	else if(G.sipo->blocktype==ID_KE) {
		if(ob) {
			Key *key= give_current_key(ob);
			*from= (ID *)key;
			if(key) return key->ipo;
		}
	}
	else if(G.sipo->blocktype==ID_CU) {
		if(ob && ob->type==OB_CURVE) {
			Curve *cu= ob->data;
			*from= (ID *)cu;
			return cu->ipo;
		}
	}
	else if(G.sipo->blocktype==ID_LA) {
		if(ob && ob->type==OB_LAMP) {
			Lamp *la= ob->data;
			*from= (ID *)la;
			return la->ipo;
		}
	}
	else if(G.sipo->blocktype==ID_CA) {
		if(ob && ob->type==OB_CAMERA) {
			Camera *ca= ob->data;
			*from= (ID *)ca;
			if(ca) return ca->ipo;
		}
	}
	else if(G.sipo->blocktype==ID_SO) {

		if (G.buts && G.buts->mainb == BUTS_SOUND) {

			bSound *sound = G.buts->lockpoin;

			*from= (ID *)sound;

			if(sound) return sound->ipo;

		}
	}

	return NULL;
}

unsigned int ipo_rainbow(int cur, int tot)
{
	float dfac, fac, sat;

	dfac= (float)(1.0/( (float)tot+1.0));

	/* this calculation makes 2 different cycles of rainbow colors */
	if(cur< tot/2) fac= (float)(cur*2.0*dfac);
	else fac= (float)((cur-tot/2)*2.0*dfac +dfac);
	
	if(fac>0.5 && fac<0.8) sat= (float)0.4;
	else sat= 0.5;
	
	return hsv_to_cpack(fac, sat, 1.0);
}		

void make_ob_editipo(Object *ob, SpaceIpo *si)
{
	EditIpo *ei;
	int a, len, colipo=0;
	
	if(ob->type==OB_MESH) colipo= 1;

	ei= si->editipo= MEM_callocN(OB_TOTIPO*sizeof(EditIpo), "editipo");
	
	si->totipo= OB_TOTIPO;
	
	for(a=0; a<OB_TOTIPO; a++) {
		getname_ob_ei(ob_ar[a], ei->name, colipo);
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
}


void make_seq_editipo(SpaceIpo *si)
{
	EditIpo *ei;
	int a;
	
	ei= si->editipo= MEM_callocN(SEQ_TOTIPO*sizeof(EditIpo), "editipo");
	
	si->totipo= SEQ_TOTIPO;
	
	
	for(a=0; a<SEQ_TOTIPO; a++) {
		getname_seq_ei(seq_ar[a], ei->name);
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

void make_cu_editipo(SpaceIpo *si)
{
	EditIpo *ei;
	int a;
	
	ei= si->editipo= MEM_callocN(CU_TOTIPO*sizeof(EditIpo), "editipo");
	
	si->totipo= CU_TOTIPO;
	
	
	for(a=0; a<CU_TOTIPO; a++) {
		getname_cu_ei(cu_ar[a], ei->name);
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

void make_key_editipo(SpaceIpo *si)
{
	Key *key;
	EditIpo *ei;
	int a;
	
	ei= si->editipo= MEM_callocN(KEY_TOTIPO*sizeof(EditIpo), "editipo");
	
	si->totipo= KEY_TOTIPO;
	
	for(a=0; a<KEY_TOTIPO; a++) {
		getname_key_ei(key_ar[a], ei->name);
		ei->adrcode= key_ar[a];
		
		ei->col= ipo_rainbow(a, KEY_TOTIPO);
		
		ei->icu= find_ipocurve(si->ipo, ei->adrcode);
		if(ei->icu) {
			ei->flag= ei->icu->flag;
		}
		else if(a==0) ei->flag |= IPO_VISIBLE;
		 
		ei++;
	}
	
	ei= si->editipo;
	key= (Key *)G.sipo->from;
	if(key && key->type==KEY_RELATIVE) {
		strcpy(ei->name, "----");
	}
	else {
		ei->flag |= IPO_VISIBLE;
	}
}

int texchannel_to_adrcode(int channel)
{
	switch(channel) {
		case 0: return MA_MAP1;
		case 1: return MA_MAP2; 
		case 2: return MA_MAP3; 
		case 3: return MA_MAP4; 
		case 4: return MA_MAP5; 
		case 5: return MA_MAP6; 
		case 6: return MA_MAP7; 
		case 7: return MA_MAP8; 
		default: return 0;
	}
}

void make_mat_editipo(SpaceIpo *si)
{
	EditIpo *ei;
	int a, len;
	
	if(si->from==0) return;
	
	ei= si->editipo= MEM_callocN(MA_TOTIPO*sizeof(EditIpo), "editipo");
	
	si->totipo= MA_TOTIPO;
	
	for(a=0; a<MA_TOTIPO; a++) {
		getname_mat_ei(ma_ar[a], ei->name);
		ei->adrcode= ma_ar[a];
		
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

void make_world_editipo(SpaceIpo *si)
{
	EditIpo *ei;
	int a, len;
	
	if(si->from==0) return;
	
	ei= si->editipo= MEM_callocN(WO_TOTIPO*sizeof(EditIpo), "editipo");
	
	si->totipo= WO_TOTIPO;
	
	for(a=0; a<WO_TOTIPO; a++) {
		getname_world_ei(wo_ar[a], ei->name);
		ei->adrcode= wo_ar[a];
		
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

void make_lamp_editipo(SpaceIpo *si)
{
	EditIpo *ei;
	int a;
	
	ei= si->editipo= MEM_callocN(LA_TOTIPO*sizeof(EditIpo), "editipo");
	
	si->totipo= LA_TOTIPO;
	
	
	for(a=0; a<LA_TOTIPO; a++) {
		getname_la_ei(la_ar[a], ei->name);
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

void make_camera_editipo(SpaceIpo *si)
{
	EditIpo *ei;
	int a;
	
	ei= si->editipo= MEM_callocN(CAM_TOTIPO*sizeof(EditIpo), "editipo");
	
	si->totipo= CAM_TOTIPO;
	
	
	for(a=0; a<CAM_TOTIPO; a++) {
		getname_cam_ei(cam_ar[a], ei->name);
		ei->adrcode= cam_ar[a];

		ei->col= ipo_rainbow(a, CAM_TOTIPO);
		
		ei->icu= find_ipocurve(si->ipo, ei->adrcode);
		if(ei->icu) {
			ei->flag= ei->icu->flag;
		}
		
		ei++;
	}
}

int make_constraint_editipo(Ipo *ipo, EditIpo **si)
{
	EditIpo *ei;
	int a;
	
	ei= *si= MEM_callocN(CO_TOTIPO*sizeof(EditIpo), "editipo");
	
	for(a=0; a<CO_TOTIPO; a++) {
		getname_co_ei(co_ar[a], ei->name);
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
int make_action_editipo(Ipo *ipo, EditIpo **si)
{
	EditIpo *ei;
	int a;
	
	ei= *si= MEM_callocN(AC_TOTIPO*sizeof(EditIpo), "editipo");
	
	for(a=0; a<AC_TOTIPO; a++) {
		getname_ac_ei(ac_ar[a], ei->name);
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

void make_sound_editipo(SpaceIpo *si)
{
	EditIpo *ei;
	int a;
	
	ei= si->editipo= MEM_callocN(SND_TOTIPO*sizeof(EditIpo), "editipo");
	
	si->totipo= SND_TOTIPO;
	
	
	for(a=0; a<SND_TOTIPO; a++) {
		getname_snd_ei(snd_ar[a], ei->name);
		ei->adrcode= snd_ar[a];

		ei->col= ipo_rainbow(a, SND_TOTIPO);
		
		ei->icu= find_ipocurve(si->ipo, ei->adrcode);
		if(ei->icu) {
			ei->flag= ei->icu->flag;
		}
		
		ei++;
	}
}

void make_editipo()
{
	EditIpo *ei;
	Object *ob;
	ID *from;
	rctf *rf;
	int a;

	if(G.sipo->editipo)
		MEM_freeN(G.sipo->editipo);
	G.sipo->editipo= 0;
	G.sipo->totipo= 0;
	ob= OBACT;

	G.sipo->ipo= get_ipo_to_edit(&from);
	G.sipo->from= from;

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
	else if(G.sipo->blocktype==IPO_CO){
		G.sipo->totipo = make_constraint_editipo(G.sipo->ipo, (EditIpo**)&G.sipo->editipo);
		if (ob) {
			ob->ipowin= IPO_CO;
		}
	}
	else if(G.sipo->blocktype==ID_AC) {

		G.sipo->totipo = make_action_editipo(G.sipo->ipo, (EditIpo**)&G.sipo->editipo);
		if (ob) {
			ob->ipowin= ID_AC;
		}
	}

	if(G.sipo->editipo==0) return;
	
	/* rowbut for VISIBLE select */
	G.sipo->rowbut= 0;
	ei= G.sipo->editipo;
	for(a=0; a<G.sipo->totipo; a++, ei++) {
		
		if(ei->flag & IPO_VISIBLE) G.sipo->rowbut |= (1<<a);
		
		if(ei->icu) ei->icu->flag= ei->flag;
	}
	editipo_changed(G.sipo, 0);
	
	if(G.sipo->ipo) {

		if (G.sipo->pin)
			rf= &(G.sipo->v2d.cur);
		else
			rf= &(G.sipo->ipo->cur);
		
		if(rf->xmin<rf->xmax && rf->ymin<rf->ymax) G.v2d->cur= *rf;
		
	}
	else {
		if(G.sipo->blocktype==ID_OB) {
			G.v2d->cur.xmin= 0.0;
			G.v2d->cur.xmax= EFRA;
			G.v2d->cur.ymin= -5.0;
			G.v2d->cur.ymax= +5.0;
		}
		else if(G.sipo->blocktype==ID_CA) {
			G.v2d->cur.xmin= 0.0;
			G.v2d->cur.xmax= EFRA;
			G.v2d->cur.ymin= 0.0;
			G.v2d->cur.ymax= 100.0;
		}
		else if ELEM5(G.sipo->blocktype, ID_MA, ID_CU, ID_WO, ID_LA, IPO_CO) {
			G.v2d->cur.xmin= (float)-0.1;
			G.v2d->cur.xmax= EFRA;
			G.v2d->cur.ymin= (float)-0.1;
			G.v2d->cur.ymax= (float)+1.1;
		}
		else if(G.sipo->blocktype==ID_SEQ) {
			G.v2d->cur.xmin= -5.0;
			G.v2d->cur.xmax= 105.0;
			G.v2d->cur.ymin= (float)-0.1;
			G.v2d->cur.ymax= (float)+1.1;
		}
		else if(G.sipo->blocktype==ID_KE) {
			G.v2d->cur.xmin= (float)-0.1;
			G.v2d->cur.xmax= EFRA;
			G.v2d->cur.ymin= (float)-0.1;
			G.v2d->cur.ymax= (float)+2.1;
		}

	}
}


void test_editipo()
{
	Ipo *ipo;
	ID *from;
	
	if(G.sipo->editipo==0){
		make_editipo();
	}
	else {
		ipo= get_ipo_to_edit(&from);

		if(G.sipo->ipo != ipo || G.sipo->from!=from)
			make_editipo();
		
	}

	if (G.sipo->pin)
		return;


	if(G.sipo->ipo)
		G.sipo->ipo->cur = G.v2d->cur;

}

/* ****************************************** */

int totipo_edit, totipo_sel, totipo_vis, totipo_vert, totipo_vertsel, totipo_key, totipo_keysel;

void get_status_editipo()
{
	EditIpo *ei;
	IpoKey *ik;
	BezTriple *bezt;
	int a, b;
	
	totipo_vis= 0;
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
			if(G.sipo->showkey || (ei->flag & IPO_EDIT)) {
				
				/* if showkey: do count the vertices (for grab) */
				if(G.sipo->showkey==0) totipo_edit++;
				
				if(ei->icu) {
					if(ei->icu->bezt) {
						bezt= ei->icu->bezt;
						b= ei->icu->totvert;
						while(b--) {
							if(ei->icu->ipo==IPO_BEZ) {
								if(bezt->f1 & 1) totipo_vertsel++;
								if(bezt->f3 & 1) totipo_vertsel++;
								totipo_vert+= 2;
							}
							if(bezt->f2 & 1) totipo_vertsel++;
							
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



void update_editipo_flags()
{
	EditIpo *ei;
	IpoKey *ik;
	unsigned int flag;
	int a;
	
	ei= G.sipo->editipo;
	if(ei) {
		for(a=0; a<G.sipo->totipo; a++, ei++) {
			ei->flag &= ~IPO_VISIBLE;
			flag= (1<<a);
			if( G.sipo->rowbut & flag ) ei->flag |= IPO_VISIBLE;
			
			if(ei->icu) ei->icu->flag= ei->flag;
			
		}
	}
	if(G.sipo->showkey) {
		ik= G.sipo->ipokey.first;
		while(ik) {
			for(a=0; a<G.sipo->totipo; a++) {
				if(ik->data[a]) {
					if(ik->flag & 1) {
						ik->data[a]->f1 |= 1;
						ik->data[a]->f2 |= 1;
						ik->data[a]->f3 |= 1;
					}
					else {
						ik->data[a]->f1 &= ~1;
						ik->data[a]->f2 &= ~1;
						ik->data[a]->f3 &= ~1;
					}
				}
			}
			ik= ik->next;
		}
	}
}

void set_editflag_editipo()
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
	
	scrarea_queue_winredraw(curarea);
}

void ipo_toggle_showkey(void) {
	if(G.sipo->showkey) {
		G.sipo->showkey= 0;
		swap_selectall_editipo();	/* sel all */
	}
	else G.sipo->showkey= 1;
	free_ipokey(&G.sipo->ipokey);
	if(G.sipo->ipo) G.sipo->ipo->showkey= G.sipo->showkey;
}

void swap_selectall_editipo()
{
	Object *ob;
	EditIpo *ei;
	IpoKey *ik;
	BezTriple *bezt;
	int a, b; /*  , sel=0; */
	
	
	deselectall_key();

	get_status_editipo();
	


	if(G.sipo->showkey) {
		ik= G.sipo->ipokey.first;
		while(ik) {
			if(totipo_vertsel) ik->flag &= ~1;
			else ik->flag |= 1;
			ik= ik->next;
		}
		update_editipo_flags();

		if(G.sipo->showkey && G.sipo->blocktype==ID_OB ) {
			ob= OBACT;
			if(ob && (ob->ipoflag & OB_DRAWKEY)) draw_object_ext(BASACT);
		}
	}
	else if(totipo_edit==0) {
		ei= G.sipo->editipo;
		if (ei){
			for(a=0; a<G.sipo->totipo; a++) {
				if( ei->flag & IPO_VISIBLE ) {
					if(totipo_sel) ei->flag &= ~IPO_SELECT;
					else ei->flag |= IPO_SELECT;
				}
				ei++;
			}
			update_editipo_flags();
		}
	}
	else {
		ei= G.sipo->editipo;
		for(a=0; a<G.sipo->totipo; a++) {
			if ISPOIN3(ei, flag & IPO_VISIBLE, flag & IPO_EDIT, icu ) {
				bezt= ei->icu->bezt;
				if(bezt) {
					b= ei->icu->totvert;
					while(b--) {
						if(totipo_vertsel) {
							bezt->f1= bezt->f2= bezt->f3= 0;
						}
						else {
							bezt->f1= bezt->f2= bezt->f3= 1;
						}
						bezt++;
					}
				}
			}
			ei++;
		}
		
	}
	
	scrarea_queue_winredraw(curarea);
	
}

void swap_visible_editipo()
{
	EditIpo *ei;
	Object *ob;
	int a; /*  , sel=0; */
	
	get_status_editipo();
	
	G.sipo->rowbut= 0;
	
	ei= G.sipo->editipo;
	for(a=0; a<G.sipo->totipo; a++) {
		if(totipo_vis==0) {
			if(ei->icu) {
				ei->flag |= IPO_VISIBLE;
				G.sipo->rowbut |= (1<<a);
			}
		}
		else ei->flag &= ~IPO_VISIBLE;
		ei++;
	}
	
	update_editipo_flags();
	
	if(G.sipo->showkey) {
		
		make_ipokey();
		
		ob= OBACT;
		if(ob && (ob->ipoflag & OB_DRAWKEY)) allqueue(REDRAWVIEW3D, 0);
	}

	scrarea_queue_winredraw(curarea);
	
}

void deselectall_editipo()
{
	EditIpo *ei;
	IpoKey *ik;
	BezTriple *bezt;
	int a, b; /*  , sel=0; */
	
	deselectall_key();

	get_status_editipo();
	
	if(G.sipo->showkey) {
		ik= G.sipo->ipokey.first;
		while(ik) {
			ik->flag &= ~1;
			ik= ik->next;
		}
		update_editipo_flags();

	}
	else if(totipo_edit==0) {
		
		ei= G.sipo->editipo;
		for(a=0; a<G.sipo->totipo; a++) {
			if( ei->flag & IPO_VISIBLE ) {
				ei->flag &= ~IPO_SELECT;
			}
			ei++;
		}
		update_editipo_flags();
	}
	else {
		ei= G.sipo->editipo;
		for(a=0; a<G.sipo->totipo; a++) {
			if ISPOIN3(ei, flag & IPO_VISIBLE, flag & IPO_EDIT, icu ) {
				if(ei->icu->bezt) {
					bezt= ei->icu->bezt;
					b= ei->icu->totvert;
					while(b--) {
						bezt->f1= bezt->f2= bezt->f3= 0;
						bezt++;
					}
				}
			}
			ei++;
		}
	}
	
	scrarea_queue_winredraw(curarea);
}

short findnearest_ipovert(IpoCurve **icu, BezTriple **bezt)
{
	/* selected verts get a disadvantage */
	/* in icu and (bezt or bp) the nearest is written */
	/* return 0 1 2: handlepunt */
	EditIpo *ei;
	BezTriple *bezt1;
	int a, b;
	short dist= 100, temp, mval[2], hpoint=0;

	*icu= 0;
	*bezt= 0;

	getmouseco_areawin(mval);

	ei= G.sipo->editipo;
	for(a=0; a<G.sipo->totipo; a++, ei++) {
		if ISPOIN3(ei, flag & IPO_VISIBLE, flag & IPO_EDIT, icu) {
			
			if(ei->icu->bezt) {
				bezt1= ei->icu->bezt;
				b= ei->icu->totvert;
				while(b--) {

					ipoco_to_areaco_noclip(G.v2d, bezt1->vec[0], bezt1->s[0]);
					ipoco_to_areaco_noclip(G.v2d, bezt1->vec[1], bezt1->s[1]);
					ipoco_to_areaco_noclip(G.v2d, bezt1->vec[2], bezt1->s[2]);
										
					if(ei->disptype==IPO_DISPBITS) {
						temp= abs(mval[0]- bezt1->s[1][0]);
					}
					else temp= abs(mval[0]- bezt1->s[1][0])+ abs(mval[1]- bezt1->s[1][1]);

					if( bezt1->f2 & 1) temp+=5;
					if(temp<dist) { 
						hpoint= 1; 
						*bezt= bezt1; 
						dist= temp; 
						*icu= ei->icu; 
					}
					
					if(ei->disptype!=IPO_DISPBITS && ei->icu->ipo==IPO_BEZ) {
						/* middle points get an advantage */
						temp= -3+abs(mval[0]- bezt1->s[0][0])+ abs(mval[1]- bezt1->s[0][1]);
						if( bezt1->f1 & 1) temp+=5;
						if(temp<dist) { 
							hpoint= 0; 
							*bezt= bezt1; 
							dist= temp; 
							*icu= ei->icu; 
						}
		
						temp= abs(mval[0]- bezt1->s[2][0])+ abs(mval[1]- bezt1->s[2][1]);
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


void move_to_frame()
{
	EditIpo *ei;
	BezTriple *bezt;
	ID *id;
	float cfra;
	int a, b;
	
	if(G.sipo->editipo==0) return;

	ei= G.sipo->editipo;

	for(a=0; a<G.sipo->totipo; a++, ei++) {
		if ISPOIN(ei, flag & IPO_VISIBLE, icu) {
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
								if(ob->sf!=0.0 && (ob->ipoflag & OB_OFFS_OB) ) {
									cfra+= ob->sf/G.scene->r.framelen;
								}
							}
							CFRA= (short)floor(cfra+0.5);
						
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
}

/* *********************************** */

void do_ipowin_buts(short event)
{
	if((G.qual & LR_SHIFTKEY)==0) {
		G.sipo->rowbut= (1<<event);
	}
	scrarea_queue_winredraw(curarea);
	
	update_editipo_flags();

	if(G.sipo->showkey) {
		make_ipokey();
		if(G.sipo->blocktype==ID_OB) allqueue(REDRAWVIEW3D, 0);
	}

}

void do_ipo_selectbuttons()
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
	if(nr>=0 && nr<G.sipo->totipo) {
		ei= G.sipo->editipo;
		ei+= nr;
		
		if(ei->icu) {
			if((ei->flag & IPO_VISIBLE)==0) {
				ei->flag |= IPO_VISIBLE;
				G.sipo->rowbut |= (1<<nr);
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
}

/* ******************************************* */

EditIpo *get_editipo()
{
	EditIpo *ei;
	int a; /*  , sel=0; */
	
	get_status_editipo();
	
	if(totipo_edit>1) {
		error("Too many editipo's");
		return 0;
	}
	if(G.sipo->editipo==0) return 0;
	
	ei= G.sipo->editipo;
	for(a=0; a<G.sipo->totipo; a++) {
		if(ei->flag & IPO_VISIBLE) {
			if( ei->flag & IPO_EDIT ) return ei;
			else if(totipo_vis==1) return ei;
			
			if(ei->flag & IPO_SELECT) {
				if(totipo_sel==1) return ei;
			}
		}
		ei++;
	}
	return 0;
}


static Ipo *get_ipo(ID *from, short type, int make)
{
	Object *ob;
	Material *ma;
	Curve *cu;
	Sequence *seq;
	Key *key;
	World *wo;
	Lamp *la;
	Camera *ca;
	Ipo *ipo= 0;
	bAction *act;

	if( type==ID_OB) {
		ob= (Object *)from;
		if(ob->id.lib) return 0;
		
		ipo= ob->ipo;
		if(make && ipo==0) ipo= ob->ipo= add_ipo("ObIpo", ID_OB);	
	}
	else if( type==IPO_CO){
		ob= (Object *)from;
		if(ob->id.lib) return 0;

		if (ob->activecon){
			ipo= ob->activecon->ipo;
			if(make && ipo==0) ipo= ob->activecon->ipo= add_ipo("CoIpo", IPO_CO);	
		}
	}
	else if( type==ID_AC) {
		act= (bAction *)from;
		if (!act->achan) return 0;
		if (act->id.lib) return 0;
		ipo= act->achan->ipo;

		/* This should never happen */
		if(make && ipo==0) ipo= act->achan->ipo= add_ipo("AcIpo", ID_AC);
	}
	else if( type==ID_MA) {
		ma= (Material *)from;
		if(ma->id.lib) return 0;
		ipo= ma->ipo;
		
		if(make && ipo==0) ipo= ma->ipo= add_ipo("MatIpo", ID_MA);
	}

	else if( type==ID_SEQ) {
		seq= (Sequence *)from;

		if(seq->type & SEQ_EFFECT) {
			ipo= seq->ipo;
			if(make && ipo==0) ipo= seq->ipo= add_ipo("SeqIpo", ID_SEQ);
		}
		else return 0;
	}	
	else if( type==ID_CU) {
		cu= (Curve *)from;
		if(cu->id.lib) return 0;
		ipo= cu->ipo;
		
		if(make && ipo==0) ipo= cu->ipo= add_ipo("CuIpo", ID_CU);
	}
	else if( type==ID_KE) {
		key= (Key *)from;
		if(key->id.lib) return 0;
		ipo= key->ipo;
		
		if(make && ipo==0) ipo= key->ipo= add_ipo("KeyIpo", ID_KE);
	}
	else if( type==ID_WO) {
		wo= (World *)from;
		if(wo->id.lib) return 0;
		ipo= wo->ipo;
		
		if(make && ipo==0) ipo= wo->ipo= add_ipo("WoIpo", ID_WO);
	}
	else if( type==ID_LA) {
		la= (Lamp *)from;
		if(la->id.lib) return 0;
		ipo= la->ipo;
		
		if(make && ipo==0) ipo= la->ipo= add_ipo("LaIpo", ID_LA);
	}
	else if( type==ID_CA) {
		ca= (Camera *)from;
		if(ca->id.lib) return 0;
		ipo= ca->ipo;
		
		if(make && ipo==0) ipo= ca->ipo= add_ipo("CaIpo", ID_CA);
	}
	else if( type==ID_SO) {
		bSound *snd= (bSound *)from;
		if(snd->id.lib) return 0;
		ipo= snd->ipo;
		
		if(make && ipo==0) ipo= snd->ipo= add_ipo("SndIpo", ID_SO);
	}
	else return 0;
	
	return ipo;	
}


// this function should not have the G.sipo in it...

IpoCurve *get_ipocurve(ID *from, short type, int adrcode, Ipo *useipo)
{
	Ipo *ipo= 0;
	IpoCurve *icu=0;
	
	/* return 0 if lib */
	/* also test if ipo and ipocurve exist */

	if (useipo==NULL) {

		if (G.sipo==NULL || G.sipo->pin==0){
			ipo= get_ipo(from, type, 1);	/* 1= make */
		}
		else
			ipo = G.sipo->ipo;

		
		if(G.sipo) {
			if (G.sipo->pin==0) G.sipo->ipo= ipo;
		}
	}
	else
		ipo= useipo;

		
	if(ipo && ipo->id.lib==0) {
	
		icu= ipo->curve.first;
		while(icu) {
			if(icu->adrcode==adrcode) break;
			icu= icu->next;
		}
		if(icu==0) {
			icu= MEM_callocN(sizeof(IpoCurve), "ipocurve");
			
			icu->flag |= IPO_VISIBLE;

			if (!useipo && G.sipo && G.sipo->pin)
				icu->blocktype = G.sipo->blocktype;
			else
				icu->blocktype= type;
			icu->adrcode= adrcode;
			
			set_icu_vars(icu);
			
			BLI_addtail( &(ipo->curve), icu);
		}
	}
	return icu;
}

void insert_vert_ipo(IpoCurve *icu, float x, float y)
{
	BezTriple *bezt, beztr, *newbezt;
	int a = 0, h1, h2;
	
	memset(&beztr, 0, sizeof(BezTriple));
	beztr.vec[1][0]= x;
	beztr.vec[1][1]= y;
	beztr.hide= IPO_BEZ;
	beztr.f1= beztr.f2= beztr.f3= SELECT;
	beztr.h1= beztr.h2= HD_AUTO;
		
	bezt= icu->bezt;
		
	if(bezt==0) {
		icu->bezt= MEM_callocN( sizeof(BezTriple), "beztriple");
		*(icu->bezt)= beztr;
		icu->totvert= 1;
	}
	else {
		/* all vertices deselect */
		for(a=0; a<icu->totvert; a++, bezt++) {
			bezt->f1= bezt->f2= bezt->f3= 0;
		}
	
		bezt= icu->bezt;
		for(a=0; a<=icu->totvert; a++, bezt++) {
			
			/* no double points */
			if(a<icu->totvert && (bezt->vec[1][0]>x-IPOTHRESH && bezt->vec[1][0]<x+IPOTHRESH)) {
				*(bezt)= beztr;
				break;
			}
			if(a==icu->totvert || bezt->vec[1][0] > x) {
				newbezt= MEM_callocN( (icu->totvert+1)*sizeof(BezTriple), "beztriple");
				
				if(a>0) memcpy(newbezt, icu->bezt, a*sizeof(BezTriple));
				
				bezt= newbezt+a;
				*(bezt)= beztr;
				
				if(a<icu->totvert) memcpy(newbezt+a+1, icu->bezt+a, (icu->totvert-a)*sizeof(BezTriple));
				
				MEM_freeN(icu->bezt);
				icu->bezt= newbezt;
				
				icu->totvert++;
				break;
			}
		}
	}
	
	
	calchandles_ipocurve(icu);
	
	/* set handletype */
	if(icu->totvert>2) {
		h1= h2= HD_AUTO;
		if(a>0) h1= (bezt-1)->h2;
		if(a<icu->totvert-1) h2= (bezt+1)->h1;
		bezt->h1= h1;
		bezt->h2= h2;

		calchandles_ipocurve(icu);
	}
}

void add_vert_ipo()
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
	
	ei= get_editipo();
	if(ei==0) return;

	areamouseco_to_ipoco(G.v2d, mval, &x, &y);
	
	if(ei->icu==0) {
		if(G.sipo->from)
			ei->icu= get_ipocurve(G.sipo->from, G.sipo->blocktype, ei->adrcode, 0);
	}
	if(ei->icu==0) return;

	if(ei->disptype==IPO_DISPBITS) {
		ei->icu->vartype= IPO_BITS;
		val= (int)floor(y-0.5);
		if(val<0) val= 0;
		y= (float)(1 << val);
	}
	
	insert_vert_ipo(ei->icu, x, y);

	/* to be sure: if icu was 0, or only 1 curve visible */
	ei->flag |= IPO_SELECT;
	ei->icu->flag= ei->flag;

	editipo_changed(G.sipo, 1);
}

void add_duplicate_editipo()
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
		if ISPOIN3(ei, flag & IPO_VISIBLE, icu, icu->bezt) {
			if(G.sipo->showkey || (ei->flag & IPO_EDIT)) {
				icu= ei->icu;
				
				/* how many points */
				tot= 0;
				b= icu->totvert;
				bezt= icu->bezt;
				while(b--) {
					if(bezt->f2 & 1) tot++;
					bezt++;
				}
				
				if(tot) {
					icu->totvert+= tot;
					newb= beztn= MEM_mallocN(icu->totvert*sizeof(BezTriple), "bezt");
					bezt= icu->bezt;
					b= icu->totvert-tot;
					while(b--) {
						*beztn= *bezt;
						if(bezt->f2 & 1) {
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
	transform_ipo('g');
}

void remove_doubles_ipo()
{
	EditIpo *ei;
	IpoKey *ik, *ikn;
	BezTriple *bezt, *newb, *new1;
	float val;
	int mode, a, b;
	
	ei= G.sipo->editipo;
	for(a=0; a<G.sipo->totipo; a++, ei++) {
		if ISPOIN3(ei, flag & IPO_VISIBLE, icu, icu->bezt) {
			
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
					if(mode==2 || (bezt->f2 & 1)) {
					
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

void join_ipo()
{
	EditIpo *ei;
	IpoKey *ik;
	IpoCurve *icu;
	BezTriple *bezt, *beztn, *newb;
	float val;
	int mode, tot, a, b;
	
	get_status_editipo();
	
	mode= pupmenu("Join %t|All Selected %x1|Selected doubles %x2");
	if( mode==2 ) {
		remove_doubles_ipo();
		return;
	}
	else if(mode!=1) return;
	
	/* first: multiple selected verts in 1 curve */
	ei= G.sipo->editipo;
	for(a=0; a<G.sipo->totipo; a++, ei++) {
		if ISPOIN3(ei, flag & IPO_VISIBLE, icu, icu->bezt) {
			if(G.sipo->showkey || (ei->flag & IPO_EDIT)) {
				icu= ei->icu;
				
				/* how many points */
				tot= 0;
				b= icu->totvert;
				bezt= icu->bezt;
				while(b--) {
					if(bezt->f2 & 1) tot++;
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
						
						if(bezt->f2 & 1) {
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
}

void ipo_snapmenu()
{
	EditIpo *ei;
	BezTriple *bezt;
	float dx = 0.0;
	int a, b;
	short event, ok, ok2;
	
	event= pupmenu("Snap %t|Horizontal %x1|To next %x2|To frame %x3|To current frame%x4");
	if(event < 1) return;
	
	get_status_editipo();

	ei= G.sipo->editipo;
	for(b=0; b<G.sipo->totipo; b++, ei++) {
		if ISPOIN3(ei, flag & IPO_VISIBLE, icu, icu->bezt) {
		
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
						 if(bezt->f2 & 1) ok= 1;
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
									if(seq) {
										dx= (float)(CFRA-seq->startdisp);
										dx= (float)(100.0*dx/((float)(seq->enddisp-seq->startdisp)));
										
										dx-= bezt->vec[1][0];
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
	editipo_changed(G.sipo, 1);
}



void mouse_select_ipo()
{
	Object *ob;
	EditIpo *ei, *actei= 0;
	IpoCurve *icu;
	IpoKey *ik, *actik;
	BezTriple *bezt;
	Key *key;
	KeyBlock *kb, *actkb=0;
	float x, y, dist, mindist;
	int a, oldflag = 0, hand, ok;
	short mval[2], xo, yo;
	
	if(G.sipo->editipo==0) return;
	
	get_status_editipo();
	
	if(G.sipo->showkey) {
		getmouseco_areawin(mval);
	
		areamouseco_to_ipoco(G.v2d, mval, &x, &y);
		actik= 0;
		mindist= 1000.0;
		ik= G.sipo->ipokey.first;
		while(ik) {
			dist= (float)(fabs(ik->val-x));
			if(ik->flag & 1) dist+= 1.0;
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
						bezt->f1= bezt->f2= bezt->f3= 1;
					}
				}
				else if(hand==0) {
					if(bezt->f1 & 1) bezt->f1= 0;
					else bezt->f1= 1;
				}
				else {
					if(bezt->f3 & 1) bezt->f3= 0;
					else bezt->f3= 1;
				}
			}				
		}
		else {
			deselectall_editipo();
		
			if(bezt) {
				if(hand==1) {
					bezt->f1|= 1; bezt->f2|= 1; bezt->f3|= 1;
				}
				else if(hand==0) bezt->f1|= 1;
				else bezt->f3|= 1;
			}
		}
	}
	else {
		
		/* vertex keys ? */
		
		if(G.sipo->blocktype==ID_KE && G.sipo->from) {
			key= (Key *)G.sipo->from;
			
			ei= G.sipo->editipo;
			if(key->type==KEY_NORMAL || (ei->flag & IPO_VISIBLE)) {
				getmouseco_areawin(mval);
				
				areamouseco_to_ipoco(G.v2d, mval, &x, &y);
				/* how much is 20 pixels? */
				mindist= (float)(20.0*(G.v2d->cur.ymax-G.v2d->cur.ymin)/(float)curarea->winy);
				
				kb= key->block.first;
				while(kb) {
					dist= (float)(fabs(kb->pos-y));
					if(kb->flag & SELECT) dist+= (float)0.01;
					if(dist < mindist) {
						actkb= kb;
						mindist= dist;
					}
					kb= kb->next;
				}
				if(actkb) {
					ok= TRUE;
					if(G.obedit && (actkb->flag & 1)==0) {
						ok= okee("Copy Key after leaving EditMode");
					}
					if(ok) {
						/* also does all keypos */
						deselectall_editipo();
						
						actkb->flag |= 1;
						
						/* calc keypos */
						showkeypos((Key *)G.sipo->from, actkb);
					}
				}
			}
		}
			
		/* select curve */
		if(actkb==0) {
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
			}
		}
	}
	
	update_editipo_flags();
	
	force_draw();
	
	if(G.sipo->showkey && G.sipo->blocktype==ID_OB) {
		ob= OBACT;
		if(ob && (ob->ipoflag & OB_DRAWKEY)) draw_object_ext(BASACT);
	}
	
	getmouseco_areawin(mval);
	xo= mval[0]; 
	yo= mval[1];

	while(get_mbut()&R_MOUSE) {		
		getmouseco_areawin(mval);
		if(abs(mval[0]-xo)+abs(mval[1]-yo) > 4) {
			
			if(actkb) move_keys();
			else transform_ipo('g');
			
			return;
		}
		BIF_wait_for_statechange();
	}
}

int icu_keys_bezier_loop(IpoCurve *icu,
                         int (*bezier_function)(BezTriple *),
                         void (ipocurve_function)(struct IpoCurve *icu)) 
{
    /*  This loops through the beziers in the Ipocurve, and executes 
     *  the generic user provided 'bezier_function' on each one. 
     *  Optionally executes the generic function ipocurve_function on the 
     *  IPO curve after looping (eg. calchandles_ipocurve)
     */

    int b;
    BezTriple *bezt;

    b    = icu->totvert;
    bezt = icu->bezt;

    /* if bezier_function has been specified
     * then loop through each bezier executing
     * it.
     */

    if (bezier_function != NULL) {
        while(b--) {
            /* exit with return code 1 if the bezier function 
             * returns 1 (good for when you are only interested
             * in finding the first bezier that
             * satisfies a condition).
             */
            if (bezier_function(bezt)) return 1;
            bezt++;
        }
    }

    /* if ipocurve_function has been specified 
     * then execute it
     */
    if (ipocurve_function != NULL)
        ipocurve_function(icu);

    return 0;

}

int ipo_keys_bezier_loop(Ipo *ipo,
                         int (*bezier_function)(BezTriple *),
                         void (ipocurve_function)(struct IpoCurve *icu))
{
    /*  This loops through the beziers that are attached to
     *  the selected keys on the Ipocurves of the Ipo, and executes 
     *  the generic user provided 'bezier_function' on each one. 
     *  Optionally executes the generic function ipocurve_function on a 
     *  IPO curve after looping (eg. calchandles_ipocurve)
     */

    IpoCurve *icu;

    /* Loop through each curve in the Ipo
     */
    for (icu=ipo->curve.first; icu; icu=icu->next){
        if (icu_keys_bezier_loop(icu,bezier_function, ipocurve_function))
            return 1;
    }

    return 0;
}

int selected_bezier_loop(int (*looptest)(EditIpo *),
                         int (*bezier_function)(BezTriple *),
                         void (ipocurve_function)(struct IpoCurve *icu))
{
	/*  This loops through the beziers that are attached to
	 *  selected keys in editmode in the IPO window, and executes 
	 *  the generic user-provided 'bezier_function' on each one 
	 *  that satisfies the 'looptest' function. Optionally executes
	 *  the generic function ipocurve_function on a IPO curve
	 *  after looping (eg. calchandles_ipocurve)
	 */

	EditIpo *ei;
	BezTriple *bezt;
	int a, b;

	/* Get the first Edit Ipo from the selected Ipos
	 */
	ei= G.sipo->editipo;

	/* Loop throught all of the selected Ipo's
	 */
	for(a=0; a<G.sipo->totipo; a++, ei++) {
		/* Do a user provided test on the Edit Ipo
		 * to determine whether we want to process it
		 */
		if (looptest(ei)) {
			/* Loop through the selected
			 * beziers on the Edit Ipo
			 */
			bezt = ei->icu->bezt;
			b    = ei->icu->totvert;
			
			/* if bezier_function has been specified
			 * then loop through each bezier executing
			 * it.
			 */
			if (bezier_function != NULL) {
				while(b--) {
					/* exit with return code 1 if the bezier function 
					 * returns 1 (good for when you are only interested
					 * in finding the first bezier that
					 * satisfies a condition).
					 */
					if (bezier_function(bezt)) return 1;
					bezt++;
				}
			}

			/* if ipocurve_function has been specified 
			 * then execute it
			 */
			if (ipocurve_function != NULL)
				ipocurve_function(ei->icu);
		}
		/* nufte flourdje zim ploopydu <-- random dutch looking comment ;) */
		/* looks more like russian to me! (ton) */
	}

	return 0;
}

int select_bezier_add(BezTriple *bezt) {
  /* Select the bezier triple */
  bezt->f1 |= 1;
  bezt->f2 |= 1;
  bezt->f3 |= 1;
  return 0;
}

int select_bezier_subtract(BezTriple *bezt) {
  /* Deselect the bezier triple */
  bezt->f1 &= ~1;
  bezt->f2 &= ~1;
  bezt->f3 &= ~1;
  return 0;
}

int select_bezier_invert(BezTriple *bezt) {
  /* Invert the selection for the bezier triple */
  bezt->f2 ^= 1;
  if ( bezt->f2 & 1 ) {
    bezt->f1 |= 1;
    bezt->f3 |= 1;
  }
  else {
    bezt->f1 &= ~1;
    bezt->f3 &= ~1;
  }
  return 0;
}

int set_bezier_auto(BezTriple *bezt) 
{
	/* Sets the selected bezier handles to type 'auto' 
	 */

	/* is a handle selected? If so
	 * set it to type auto
	 */
	if(bezt->f1 || bezt->f3) {
		if(bezt->f1) bezt->h1= 1; /* the secret code for auto */
		if(bezt->f3) bezt->h2= 1;

		/* if the handles are not of the same type, set them
		 * to type free
		 */
		if(bezt->h1!=bezt->h2) {
			if ELEM(bezt->h1, HD_ALIGN, HD_AUTO) bezt->h1= HD_FREE;
			if ELEM(bezt->h2, HD_ALIGN, HD_AUTO) bezt->h2= HD_FREE;
		}
	}
	return 0;
}

int set_bezier_vector(BezTriple *bezt) 
{
	/* Sets the selected bezier handles to type 'vector' 
	 */

	/* is a handle selected? If so
	 * set it to type vector
	 */
	if(bezt->f1 || bezt->f3) {
		if(bezt->f1) bezt->h1= 2; /* the code for vector */
		if(bezt->f3) bezt->h2= 2;
    
		/* if the handles are not of the same type, set them
		 * to type free
		 */
		if(bezt->h1!=bezt->h2) {
			if ELEM(bezt->h1, HD_ALIGN, HD_AUTO) bezt->h1= HD_FREE;
			if ELEM(bezt->h2, HD_ALIGN, HD_AUTO) bezt->h2= HD_FREE;
		}
	}
	return 0;
}

int bezier_isfree(BezTriple *bezt) 
{
	/* queries whether the handle should be set
	 * to type 'free' (I think)
	 */
	if(bezt->f1 && bezt->h1) return 1;
	if(bezt->f3 && bezt->h2) return 1;
	return 0;
}

int set_bezier_free(BezTriple *bezt) 
{
	/* Sets selected bezier handles to type 'free' 
	 */
	if(bezt->f1) bezt->h1= HD_FREE;
	if(bezt->f3) bezt->h2= HD_FREE;
	return 0;
}

int set_bezier_align(BezTriple *bezt) 
{
	/* Sets selected bezier handles to type 'align' 
	 */
	if(bezt->f1) bezt->h1= HD_ALIGN;
	if(bezt->f3) bezt->h2= HD_ALIGN;
	return 0;
}

int vis_edit_icu_bez(EditIpo *ei) {
	/* A 4 part test for an EditIpo :
	 *   is it a) visible
	 *         b) in edit mode
	 *         c) does it contain an Ipo Curve
	 *         d) does that ipo curve have a bezier
	 *
	 * (The reason why I don't just use the macro
	 * is I need a pointer to a function.)
	 */
	return ISPOIN4(ei, flag & IPO_VISIBLE, flag & IPO_EDIT, icu, icu->bezt);
}

void select_ipo_bezier_keys(Ipo *ipo, int selectmode)
{
  /* Select all of the beziers in all
   * of the Ipo curves belonging to the
   * Ipo, using the selection mode.
   */
  switch (selectmode) {
  case SELECT_ADD:
    ipo_keys_bezier_loop(ipo, select_bezier_add, NULL);
    break;
  case SELECT_SUBTRACT:
    ipo_keys_bezier_loop(ipo, select_bezier_subtract, NULL);
    break;
  case SELECT_INVERT:
    ipo_keys_bezier_loop(ipo, select_bezier_invert, NULL);
    break;
  }
}

void sethandles_ipo_keys(Ipo *ipo, int code)
{
	/* this function lets you set bezier handles all to
	 * one type for some Ipo's (e.g. with hotkeys through
	 * the action window).
	 */ 

	/* code==1: set autohandle */
	/* code==2: set vectorhandle */
	/* als code==3 (HD_ALIGN) toggelt het, vectorhandles worden HD_FREE */
	
	switch(code) {
	case 1:
		/*** Set to auto ***/
		ipo_keys_bezier_loop(ipo, set_bezier_auto,
							 calchandles_ipocurve);
		break;
	case 2:
		/*** Set to vector ***/
		ipo_keys_bezier_loop(ipo, set_bezier_vector,
                         calchandles_ipocurve);
		break;
	default:
		if ( ipo_keys_bezier_loop(ipo, bezier_isfree, NULL) ) {
			/*** Set to free ***/
			ipo_keys_bezier_loop(ipo, set_bezier_free,
                           calchandles_ipocurve);
		}
		else {
			/*** Set to align ***/
			ipo_keys_bezier_loop(ipo, set_bezier_align,
                           calchandles_ipocurve);
		}
		break;
	}


}

void sethandles_ipo(int code)
{
	/* this function lets you set bezier handles all to
	 * one type for some selected keys in edit mode in the
	 * IPO window (e.g. with hotkeys)
	 */ 

	/* code==1: set autohandle */
	/* code==2: set vectorhandle */
	/* als code==3 (HD_ALIGN) toggelt het, vectorhandles worden HD_FREE */

	if(G.sipo->ipo && G.sipo->ipo->id.lib) return;

	switch(code) {
	case 1:
		/*** Set to auto ***/
		selected_bezier_loop(vis_edit_icu_bez, set_bezier_auto,
                         calchandles_ipocurve);
		break;
	case 2:
		/*** Set to vector ***/
		selected_bezier_loop(vis_edit_icu_bez, set_bezier_vector,
                         calchandles_ipocurve);
		break;
	default:
		if (selected_bezier_loop(vis_edit_icu_bez, bezier_isfree, NULL) ) {
			/*** Set to free ***/
			selected_bezier_loop(vis_edit_icu_bez, set_bezier_free,
								 calchandles_ipocurve);
		}
		else {
			/*** Set to align ***/
			selected_bezier_loop(vis_edit_icu_bez, set_bezier_align,
								 calchandles_ipocurve);
		}
		break;
	}

	editipo_changed(G.sipo, 1);
}


void set_ipocurve_constant(struct IpoCurve *icu) {
	/* Sets the type of the IPO curve to constant
	 */
	icu->ipo= IPO_CONST;
}

void set_ipocurve_linear(struct IpoCurve *icu) {
	/* Sets the type of the IPO curve to linear
	 */
	icu->ipo= IPO_LIN;
}

void set_ipocurve_bezier(struct IpoCurve *icu) {
	/* Sets the type of the IPO curve to bezier
	 */
	icu->ipo= IPO_BEZ;
}


void setipotype_ipo(Ipo *ipo, int code)
{
	/* Sets the type of the each ipo curve in the
	 * Ipo to a value based on the code
	 */
	switch (code) {
	case 1:
		ipo_keys_bezier_loop(ipo, NULL, set_ipocurve_constant);
		break;
	case 2:
		ipo_keys_bezier_loop(ipo, NULL, set_ipocurve_linear);
		break;
	case 3:
		ipo_keys_bezier_loop(ipo, NULL, set_ipocurve_bezier);
		break;
	}
}

void set_ipotype()
{
	EditIpo *ei;
	Key *key;
	KeyBlock *kb;
	int a;
	short event;

	if(G.sipo->ipo && G.sipo->ipo->id.lib) return;
	if(G.sipo->showkey) return;
	get_status_editipo();
	
	if(G.sipo->blocktype==ID_KE && totipo_edit==0 && totipo_sel==0) {
		key= (Key *)G.sipo->from;
		if(key==0) return;
		
		event= pupmenu("Key Type %t|Linear %x1|Cardinal %x2|B spline %x3");
		if(event < 1) return;
		
		kb= key->block.first;
		while(kb) {
			if(kb->flag & SELECT) {
				kb->type= 0;
				if(event==1) kb->type= KEY_LINEAR;
				if(event==2) kb->type= KEY_CARDINAL;
				if(event==3) kb->type= KEY_BSPLINE;
			}
			kb= kb->next;
		}
	}
	else {
		event= pupmenu("Ipo Type %t|Constant %x1|Linear %x2|Bezier %x3");
		if(event < 1) return;
		
		ei= G.sipo->editipo;
		for(a=0; a<G.sipo->totipo; a++, ei++) {
			if ISPOIN3(ei, flag & IPO_VISIBLE, flag & IPO_SELECT, icu) {
				if(event==1) ei->icu->ipo= IPO_CONST;
				else if(event==2) ei->icu->ipo= IPO_LIN;
				else ei->icu->ipo= IPO_BEZ;
			}
		}
	}
	scrarea_queue_winredraw(curarea);
}

void borderselect_ipo()
{
	EditIpo *ei;
	IpoKey *ik;
	BezTriple *bezt;
	rcti rect;
	rctf rectf;
	int a, b, val;
	short mval[2];

	get_status_editipo();
	
	val= get_border(&rect, 3);

	if(val) {
		mval[0]= rect.xmin;
		mval[1]= rect.ymin;
		areamouseco_to_ipoco(G.v2d, mval, &rectf.xmin, &rectf.ymin);
		mval[0]= rect.xmax;
		mval[1]= rect.ymax;
		areamouseco_to_ipoco(G.v2d, mval, &rectf.xmax, &rectf.ymax);

		if(G.sipo->showkey) {
			ik= G.sipo->ipokey.first;
			while(ik) {
				if(rectf.xmin<ik->val && rectf.xmax>ik->val) {
					if(val==LEFTMOUSE) ik->flag |= 1;
					else ik->flag &= ~1;
				}
				ik= ik->next;
			}
			update_editipo_flags();
		}
		else if(totipo_edit==0) {
			if(rect.xmin<rect.xmax && rect.ymin<rect.ymax)
				select_proj_ipo(&rectf, val);
		}
		else {
			
			ei= G.sipo->editipo;
			for(a=0; a<G.sipo->totipo; a++, ei++) {
				if ISPOIN3(ei, flag & IPO_VISIBLE, flag & IPO_EDIT, icu) {
					if(ei->icu->bezt) {
						b= ei->icu->totvert;
						bezt= ei->icu->bezt;
						while(b--) {
							int bit= (val==LEFTMOUSE);
							
							if(BLI_in_rctf(&rectf, bezt->vec[0][0], bezt->vec[0][1]))
								bezt->f1 = (bezt->f1&~1) | bit;
							if(BLI_in_rctf(&rectf, bezt->vec[1][0], bezt->vec[1][1]))
								bezt->f2 = (bezt->f2&~1) | bit;
							if(BLI_in_rctf(&rectf, bezt->vec[2][0], bezt->vec[2][1]))
								bezt->f3 = (bezt->f3&~1) | bit;

							bezt++;
						}
					}
				}
			}
		}
		scrarea_queue_winredraw(curarea);
	}
}




void del_ipo()
{
	EditIpo *ei;
	BezTriple *bezt, *bezt1;
	int a, b;
	int del, event;

	get_status_editipo();
	if(G.sipo->ipo && G.sipo->ipo->id.lib) return;
	
	if(totipo_edit==0 && totipo_sel==0 && totipo_vertsel==0) {
		delete_key();
		return;
	}
	
	if( okee("Erase selected")==0 ) return;

	// eerste doorloop, kunnen hele stukken weg? 
	ei= G.sipo->editipo;
	for(a=0; a<G.sipo->totipo; a++, ei++) {
	
		del= 0;
		
		if(G.sipo->showkey==0 && totipo_edit==0) {
			if ISPOIN3(ei, flag & IPO_VISIBLE, flag & IPO_SELECT, icu) {
				del= 1;
			}
		}
		else {
			if ISPOIN(ei, flag & IPO_VISIBLE, icu) {
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
			BLI_remlink( &(G.sipo->ipo->curve), ei->icu);
			if(ei->icu->bezt) MEM_freeN(ei->icu->bezt);
			MEM_freeN(ei->icu);
			ei->flag &= ~IPO_SELECT;
			ei->flag &= ~IPO_EDIT;
			ei->icu= 0;	
		}
	}
	
	// tweede doorloop, kleine stukken weg: alleen curves 
	ei= G.sipo->editipo;
	for(b=0; b<G.sipo->totipo; b++, ei++) {
		if ISPOIN(ei, flag & IPO_VISIBLE, icu) {
			if(G.sipo->showkey || (ei->flag & IPO_EDIT)) {
			
				event= 0;
				if(ei->icu->bezt) {
				
					bezt= ei->icu->bezt;
					for(a=0; a<ei->icu->totvert; a++) {
						if( BEZSELECTED(bezt) ) {
							memcpy(bezt, bezt+1, (ei->icu->totvert-a-1)*sizeof(BezTriple));
							ei->icu->totvert--;
							a--;
							event= 1;
						}
						else bezt++;
					}
					if(event) {
						bezt1 = (BezTriple*) MEM_mallocN(ei->icu->totvert * sizeof(BezTriple), "delNurb");
						memcpy(bezt1, ei->icu->bezt, (ei->icu->totvert)*sizeof(BezTriple) );
						MEM_freeN(ei->icu->bezt);
						ei->icu->bezt= bezt1;
					}
				}
			}
		}
	}
	
	allqueue(REDRAWNLA, 0);
	allqueue (REDRAWACTION, 0);
	allqueue(REDRAWIPO, 0);
	allspace(REMAKEIPO, 0);
}

ListBase ipocopybuf={0, 0};
int totipocopybuf=0;

void free_ipocopybuf()
{
	IpoCurve *icu;
	
	while( (icu= ipocopybuf.first) ) {
		if(icu->bezt) MEM_freeN(icu->bezt);
		BLI_remlink(&ipocopybuf, icu);
		MEM_freeN(icu);
	}
	totipocopybuf= 0;
}

void copy_editipo()
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
		if ISPOIN(ei, flag & IPO_VISIBLE, icu) {
			if( (ei->flag & IPO_EDIT) || (ei->flag & IPO_SELECT) ) {
				icu= MEM_callocN(sizeof(IpoCurve), "ipocopybuf");
				*icu= *(ei->icu);
				BLI_addtail(&ipocopybuf, icu);
				if(icu->bezt) {
					icu->bezt= MEM_mallocN(icu->totvert*sizeof(BezTriple), "ipocopybuf");
					memcpy(icu->bezt, ei->icu->bezt, icu->totvert*sizeof(BezTriple));
				}
				totipocopybuf++;
			}
		}
	}
	
	if(totipocopybuf==0) error("Copybuf is empty");
}

void paste_editipo()
{
	EditIpo *ei;
	IpoCurve *icu;
	int a, ok;
	
	if(G.sipo->showkey) return;
	
	if(totipocopybuf==0) return;
	if(G.sipo->ipo==0) return;
	if(G.sipo->ipo && G.sipo->ipo->id.lib) return;

	get_status_editipo();
	
	if(totipo_vis==0) {
		error("No visible splines");
	}
	else if(totipo_vis!=totipocopybuf && totipo_sel!=totipocopybuf) {
		error("Incompatible paste");
	}
	else {
		/* prevent problems: splines visible that are not selected */
		if(totipo_vis==totipo_sel) totipo_vis= 0;
		
		icu= ipocopybuf.first;
		if(icu==0) return;

		ei= G.sipo->editipo;
		for(a=0; a<G.sipo->totipo; a++, ei++) {
			if(ei->flag & IPO_VISIBLE) {
				ok= 0;
				if(totipo_vis==totipocopybuf) ok= 1;
				if(totipo_sel==totipocopybuf && (ei->flag & IPO_SELECT)) ok= 1;
	
				if(ok) {
			
					ei->icu= get_ipocurve(G.sipo->from, G.sipo->blocktype, ei->adrcode, 0);
					if(ei->icu==0) return;
					
					if(ei->icu->bezt) MEM_freeN(ei->icu->bezt);
					ei->icu->bezt= 0;
					
					ei->icu->totvert= icu->totvert;
					ei->icu->flag= ei->flag= icu->flag;
					ei->icu->extrap= icu->extrap;
					ei->icu->ipo= icu->ipo;
					
					if(icu->bezt) {
						ei->icu->bezt= MEM_mallocN(icu->totvert*sizeof(BezTriple), "ipocopybuf");
						memcpy(ei->icu->bezt, icu->bezt, icu->totvert*sizeof(BezTriple));
					}
					
					icu= icu->next;
					
				}
			}
		}
		editipo_changed(G.sipo, 1);
	}
}

void set_exprap_ipo(int mode)
{
	EditIpo *ei;
	int a;
		
	if(G.sipo->ipo && G.sipo->ipo->id.lib) return;
	/* in case of keys: always ok */

	ei= G.sipo->editipo;
	for(a=0; a<G.sipo->totipo; a++, ei++) {
		if ISPOIN(ei, flag & IPO_VISIBLE, icu) {
			if( (ei->flag & IPO_EDIT) || (ei->flag & IPO_SELECT) || (G.sipo->showkey) ) {
				ei->icu->extrap= mode;
			}
		}
	}
	editipo_changed(G.sipo, 1);
}

int find_other_handles(EditIpo *eicur, float ctime, BezTriple **beztar)
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
	int a, b, totvert, didit=0;
		
	if(G.sipo->ipo && G.sipo->ipo->id.lib) return;

	/* starting with 1 visible curve, selected point, associated points: do lencorr! */

	ei= G.sipo->editipo;
	for(a=0; a<G.sipo->totipo; a++, ei++) {
		if ISPOIN(ei, flag & IPO_VISIBLE, icu) {
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
						
						Normalise(vec1);
						Normalise(vec2);
						
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
						error("Cannot set speed");
					}
				}
			}
			break;	
		}
	}
	
	if(didit==0) error("Did not set speed");
	
	editipo_changed(G.sipo, 1);
	allqueue(REDRAWNLA, 0);
	allqueue (REDRAWACTION, 0);
	allqueue(REDRAWIPO, 0);

}


void insertkey(ID *id, int adrcode)
{
	IpoCurve *icu;
	Ipo *ipo;
	Object *ob;
	void *poin;
	float curval, cfra;
	int type;
	
	if(id) {

		// this call here, otherwise get_ipo_curve gives it from the pinned ipo
		ipo= get_ipo(id, GS(id->name), 1);	// 1=make

		icu= get_ipocurve(id, GS(id->name), adrcode, ipo);
		
		if(icu) {
			poin= get_ipo_poin(id, icu, &type);
			if(poin) {
				curval= read_ipo_poin(poin, type);
				
				cfra= frame_to_float(CFRA);
				
				if( GS(id->name)==ID_OB ) {
					ob= (Object *)id;
					if(ob->sf!=0.0 && (ob->ipoflag & OB_OFFS_OB) ) {
						/* actually frametofloat calc again! */
						cfra-= ob->sf*G.scene->r.framelen;
					}
				}
				
				insert_vert_ipo(icu, cfra, curval);
			}
		}
	}
}

void insertkey_editipo()
{
	EditIpo *ei;
	IpoKey *ik;
	ID *id;
	float *fp, cfra, *insertvals;
	int a, nr, ok, tot;
	short event;
	
	if(G.sipo->showkey)
		event= pupmenu("Insert KeyVertices %t|Current frame %x1|Selected Keys %x2");
	else 
		event= pupmenu("Insert KeyVertices %t|Current frame %x1");
	
	if(event<1) return;
	
	ei= G.sipo->editipo;
	for(nr=0; nr<G.sipo->totipo; nr++, ei++) {
		if ISPOIN(ei, flag & IPO_VISIBLE, icu) {
		
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
						if(ob->sf!=0.0 && (ob->ipoflag & OB_OFFS_OB) ) {
							cfra-= ob->sf*G.scene->r.framelen;
						}
					}
					else if(id && GS(id->name)==ID_SEQ) {
						extern Sequence *last_seq;	/* editsequence.c */
						
						if(last_seq) {
							cfra= (float)(100.0*(cfra-last_seq->startdisp)/((float)(last_seq->enddisp-last_seq->startdisp)));
						}
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
						insert_vert_ipo(ei->icu, fp[0], fp[1]);
					}
					
					MEM_freeN(insertvals);
					calc_ipo(G.sipo->ipo, (float)CFRA);
				}
			}
		}
	}
	allqueue (REDRAWACTION, 0);
	allqueue(REDRAWNLA, 0);
	allqueue(REDRAWIPO, 0);
	allspace(REMAKEIPO, 0);
}


void common_insertkey()
{
	Base *base;
	Object *ob;
	Material *ma;
	ID *id;
	IpoCurve *icu;
	World *wo;
	Lamp *la;
	int tlay, map, event;
	char menustr[256];
	
	if(curarea->spacetype==SPACE_IPO) {
		insertkey_editipo();
	}
	else if(curarea->spacetype==SPACE_BUTS) {
		
		if(G.buts->mainb==BUTS_MAT) {
			id= G.buts->lockpoin;
			ma= G.buts->lockpoin;
			if(id) {
				event= pupmenu("Insert Key %t|RGB%x0|Alpha%x1|HaSize%x2|Mode %x3|All Color%x10|Ofs%x12|Size%x13|All Mapping%x11");
				if(event== -1) return;
				
				map= texchannel_to_adrcode(ma->texact);
				
				if(event==0 || event==10) {
					insertkey(id, MA_COL_R);
					insertkey(id, MA_COL_G);
					insertkey(id, MA_COL_B);
				}
				if(event==1 || event==10) {
					insertkey(id, MA_ALPHA);
				}
				if(event==2 || event==10) {
					insertkey(id, MA_HASIZE);
				}
				if(event==3 || event==10) {
					insertkey(id, MA_MODE);
				}
				if(event==10) {
					insertkey(id, MA_SPEC_R);
					insertkey(id, MA_SPEC_G);
					insertkey(id, MA_SPEC_B);
					insertkey(id, MA_REF);
					insertkey(id, MA_EMIT);
					insertkey(id, MA_AMB);
					insertkey(id, MA_SPEC);
					insertkey(id, MA_HARD);
					insertkey(id, MA_MODE);
				}
				if(event==12 || event==11) {
					insertkey(id, map+MAP_OFS_X);
					insertkey(id, map+MAP_OFS_Y);
					insertkey(id, map+MAP_OFS_Z);
				}
				if(event==13 || event==11) {
					insertkey(id, map+MAP_SIZE_X);
					insertkey(id, map+MAP_SIZE_Y);
					insertkey(id, map+MAP_SIZE_Z);
				}
				if(event==11) {
					insertkey(id, map+MAP_R);
					insertkey(id, map+MAP_G);
					insertkey(id, map+MAP_B);
					insertkey(id, map+MAP_DVAR);
					insertkey(id, map+MAP_COLF);
					insertkey(id, map+MAP_NORF);
					insertkey(id, map+MAP_VARF);
				}
			}
		}
		else if(G.buts->mainb==BUTS_WORLD) {
			id= G.buts->lockpoin;
			wo= G.buts->lockpoin;
			if(id) {
				event= pupmenu("Insert Key %t|ZenRGB%x0|HorRGB%x1|Mist%x2|stars %x3|Ofs%x12|Size%x13");
				if(event== -1) return;
				
				map= texchannel_to_adrcode(wo->texact);
				
				if(event==0) {
					insertkey(id, WO_ZEN_R);
					insertkey(id, WO_ZEN_G);
					insertkey(id, WO_ZEN_B);
				}
				if(event==1) {
					insertkey(id, WO_HOR_R);
					insertkey(id, WO_HOR_G);
					insertkey(id, WO_HOR_B);
				}
				if(event==2) {
					insertkey(id, WO_MISI);
					insertkey(id, WO_MISTDI);
					insertkey(id, WO_MISTSTA);
					insertkey(id, WO_MISTHI);
				}
				if(event==3) {
					insertkey(id, WO_STAR_R);
					insertkey(id, WO_STAR_G);
					insertkey(id, WO_STAR_B);
					insertkey(id, WO_STARDIST);
					insertkey(id, WO_STARSIZE);
				}
				if(event==12) {
					insertkey(id, map+MAP_OFS_X);
					insertkey(id, map+MAP_OFS_Y);
					insertkey(id, map+MAP_OFS_Z);
				}
				if(event==13) {
					insertkey(id, map+MAP_SIZE_X);
					insertkey(id, map+MAP_SIZE_Y);
					insertkey(id, map+MAP_SIZE_Z);
				}
			}
		}
		else if(G.buts->mainb==BUTS_LAMP) {
			id= G.buts->lockpoin;
			la= G.buts->lockpoin;
			if(id) {
				event= pupmenu("Insert Key %t|RGB%x0|Energy%x1|Spotsi%x2|Ofs%x12|Size%x13");
				if(event== -1) return;
				
				map= texchannel_to_adrcode(la->texact);
				
				if(event==0) {
					insertkey(id, LA_COL_R);
					insertkey(id, LA_COL_G);
					insertkey(id, LA_COL_B);
				}
				if(event==1) {
					insertkey(id, LA_ENERGY);
				}
				if(event==2) {
					insertkey(id, LA_SPOTSI);
				}
				if(event==12) {
					insertkey(id, map+MAP_OFS_X);
					insertkey(id, map+MAP_OFS_Y);
					insertkey(id, map+MAP_OFS_Z);
				}
				if(event==13) {
					insertkey(id, map+MAP_SIZE_X);
					insertkey(id, map+MAP_SIZE_Y);
					insertkey(id, map+MAP_SIZE_Z);
				}
				
			}
		}
		else if(G.buts->mainb==BUTS_EDIT) {
			ob= OBACT;
			if(ob && ob->type==OB_CAMERA) {
				id= G.buts->lockpoin;
				if(id) {
					event= pupmenu("Insert Key %t|Lens%x0|Clipping%x1");
					if(event== -1) return;

					if(event==0) {
						insertkey(id, CAM_LENS);
					}
					if(event==1) {
						insertkey(id, CAM_STA);
						insertkey(id, CAM_END);
					}
				}
			}
		}
		else if(G.buts->mainb==BUTS_SOUND) {
			if(G.ssound) {
				id= G.buts->lockpoin;
				if(id) {
					event= pupmenu("Insert Key %t|Volume%x0|Pitch%x1|Panning%x2|Attennuation%x3");
					if(event== -1) return;

					if(event==0) {
						insertkey(id, SND_VOLUME);
					}
					if(event==1) {
						insertkey(id, SND_PITCH);
					}
					if(event==2) {
						insertkey(id, SND_PANNING);
					}
					if(event==3) {
						insertkey(id, SND_ATTEN);
					}
				}
			}
		}
		
		allqueue(REDRAWACTION, 0);
		allqueue(REDRAWNLA, 0);
		allqueue(REDRAWIPO, 0);
		allspace(REMAKEIPO, 0);

	}
	else if(curarea->spacetype==SPACE_VIEW3D) {
		

		base= FIRSTBASE;
		while(base) {
			if TESTBASELIB(base) break;
			base= base->next;
		}
		if(base==0) return;
		
		if (G.obpose)
			strcpy(menustr, "Insert Key%t|Loc%x0|Rot%x1|Size%x2|LocRot%x3|LocRotSize%x4|Avail%x9");
		else
			strcpy(menustr, "Insert Key%t|Loc%x0|Rot%x1|Size%x2|LocRot%x3|LocRotSize%x4|Layer%x5|Avail%x9");

		
		if( (ob = OBACT)) {
			if(ob->type==OB_MESH) strcat(menustr, "| %x6|Mesh%x7");
			else if(ob->type==OB_LATTICE) strcat(menustr, "| %x6|Lattice%x7");
			else if(ob->type==OB_CURVE) strcat(menustr, "| %x6|Curve%x7");
			else if(ob->type==OB_SURF) strcat(menustr, "| %x6|Surface%x7");
			else if(ob->type==OB_IKA) strcat(menustr, "| %x6|Effector%x8");
			if(ob->flag & OB_FROMGROUP)	strcat(menustr, "| %x6|Entire Group%x10");
		}
		
		event= pupmenu(menustr);
		if(event== -1) return;
		
		if(event==7) {
			if(ob->type==OB_MESH) insert_meshkey(ob->data);
			else if ELEM(ob->type, OB_CURVE, OB_SURF) insert_curvekey(ob->data);
			else if(ob->type==OB_LATTICE) insert_lattkey(ob->data);
			
			allqueue(REDRAWIPO, 0);
			allqueue(REDRAWACTION, 0);
			allqueue(REDRAWNLA, 0);
			allqueue(REDRAWBUTSANIM, 0);
			return;
		}
		
		if(event==10) {
			Group *group= find_group(ob);
			if(group) {
				add_group_key(group);
				allqueue(REDRAWBUTSANIM, 0);
			}
		}
		
		base= FIRSTBASE;
		if (G.obpose){
			bAction	*act;
			bPose	*pose;
			bPoseChannel *chan;
			bActionChannel *achan;

			ob = G.obpose;

			/* Get action & pose from object */
			act=ob->action;
			pose=ob->pose;

			collect_pose_garbage(ob);

			if (!act){
				act= G.obpose->action=add_empty_action();
				/* this sets the non-pinned open ipowindow(s) to show the action curve */
				ob->ipowin= ID_AC;
				allqueue(REDRAWIPO, ob->ipowin);
				
				allqueue(REDRAWACTION, 0);
				allqueue(REDRAWNLA, 0);
			}
			if (!pose){
				error ("No pose!"); /* Should never happen */
			}

			if (act->id.lib)
			{
				error ("Can't key libactions");
				return;
			}
			filter_pose_keys ();
			for (chan=pose->chanbase.first; chan; chan=chan->next)
			{
				if (chan->flag & POSE_KEY){
					//			set_action_key(act, chan);
					if(event==0 || event==3 ||event==4) {
						set_action_key(act, chan, AC_LOC_X, 1);
						set_action_key(act, chan, AC_LOC_Y, 1);
						set_action_key(act, chan, AC_LOC_Z, 1);
					}
					if(event==1 || event==3 ||event==4) {
						set_action_key(act, chan, AC_QUAT_X, 1);
						set_action_key(act, chan, AC_QUAT_Y, 1);
						set_action_key(act, chan, AC_QUAT_Z, 1);
						set_action_key(act, chan, AC_QUAT_W, 1);
					}
					if(event==2 || event==4) {
						set_action_key(act, chan, AC_SIZE_X, 1);
						set_action_key(act, chan, AC_SIZE_Y, 1);
						set_action_key(act, chan, AC_SIZE_Z, 1);
					}
					if (event==9){
						for (achan = act->chanbase.first; achan; achan=achan->next){
							if (achan->ipo && !strcmp (achan->name, chan->name)){
								for (icu = achan->ipo->curve.first; icu; icu=icu->next){
									set_action_key(act, chan, icu->adrcode, 0);
								}
								break;
							}
						}
					}
				}

				remake_action_ipos(act);
			}
			allqueue(REDRAWIPO, 0);
			allqueue(REDRAWACTION, 0);
			allqueue(REDRAWNLA, 0);
			
		}
		else
		{
			while(base) {
				if TESTBASELIB(base) {
					id= (ID *)(base->object);
					
					/* all curves in ipo deselect */
					if(base->object->ipo) {
						icu= base->object->ipo->curve.first;
						while(icu) {
							icu->flag &= ~IPO_SELECT;
							if(event==9) insertkey(id, icu->adrcode);
							icu= icu->next;
						}
					}
					
					if(event==0 || event==3 ||event==4) {
						insertkey(id, OB_LOC_X);
						insertkey(id, OB_LOC_Y);
						insertkey(id, OB_LOC_Z);
					}
					if(event==1 || event==3 ||event==4) {
						insertkey(id, OB_ROT_X);
						insertkey(id, OB_ROT_Y);
						insertkey(id, OB_ROT_Z);
					}
					if(event==2 || event==4) {
						insertkey(id, OB_SIZE_X);
						insertkey(id, OB_SIZE_Y);
						insertkey(id, OB_SIZE_Z);
					}
					if(event==5) {
						/* remove localview  */
						tlay= base->object->lay;
						base->object->lay &= 0xFFFFFF;
						insertkey(id, OB_LAY);
						base->object->lay= tlay;
					}
					if(event==8) {
						/* a patch, can be removed (old ika) */
						Ika *ika= ob->data;
						VecMat4MulVecfl(ika->effg, ob->obmat, ika->effn);
						
						insertkey(id, OB_EFF_X);
						insertkey(id, OB_EFF_Y);
						insertkey(id, OB_EFF_Z);
					}
				}
				base= base->next;
			}
		}
		allspace(REMAKEIPO, 0);
		allqueue(REDRAWIPO, 0);
		allqueue(REDRAWVIEW3D, 0);
		allqueue(REDRAWACTION, 0);
		allqueue(REDRAWNLA, 0);
	}
	
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
				if(bezt->f2 & 1) ik->flag= 1;
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

	if(bezt->f2 & 1) ikn->flag= 1;
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
		if ISPOIN(ei, flag & IPO_VISIBLE, icu) {
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
	
	/* test selectflags */
	ik= lb->first;
	while(ik) {
		sel= desel= 0;
		for(a=0; a<G.sipo->totipo; a++) {
			if(ik->data[a]) {
				bezt= ik->data[a];
				if(bezt->f2 & 1) sel++;
				else desel++;
			}
		}
		if(sel && desel) sel= 0;
		for(a=0; a<G.sipo->totipo; a++) {
			if(ik->data[a]) {
				bezt= ik->data[a];
				if(sel) {
					bezt->f1 |= 1;
					bezt->f2 |= 1;
					bezt->f3 |= 1;
				}
				else {
					bezt->f1 &= ~1;
					bezt->f2 &= ~1;
					bezt->f3 &= ~1;
				}
			}
		}
		if(sel) ik->flag = 1;
		else ik->flag= 0;
		
		ik= ik->next;
	}
	get_status_editipo();
}

void make_ipokey_transform(Object *ob, ListBase *lb, int sel)
{
	IpoCurve *icu;
	BezTriple *bezt;
	int a, adrcode = 0, ok, dloc=0, drot=0, dsize=0;
	
	if(ob->ipo==0) return;
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
					if(sel==0 || (bezt->f2 & 1)) {
						add_to_ipokey(lb, bezt, adrcode, OB_TOTIPO);
					}
					bezt++;
				}
			}
		}
		icu= icu->next;
	}
}

void update_ipokey_val()	/* after moving vertices */
{
	IpoKey *ik;
	int a;
	
	ik= G.sipo->ipokey.first;
	while(ik) {
		for(a=0; a<G.sipo->totipo; a++) {
			if(ik->data[a]) {
				ik->val= ik->data[a]->vec[1][0];
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



void nextkey(ListBase *elems, int dir)
{
	IpoKey *ik, *previk;
	int totsel;
	
	if(dir==1) ik= elems->last;
	else ik= elems->first;
	previk= 0;
	totsel= 0;
	
	while(ik) {
		
		if(ik->flag) totsel++;
		
		if(previk) {
			if(G.qual & LR_SHIFTKEY) {
				if(ik->flag) previk->flag= 1;
			}
			else previk->flag= ik->flag;
		}
		
		previk= ik;
		if(dir==1) ik= ik->prev;
		else ik= ik->next;
		
		if(G.qual & LR_SHIFTKEY);
		else if(ik==0) previk->flag= 0;
	}
	
	/* when no key select: */
	if(totsel==0) {
		if(dir==1) ik= elems->first;
		else ik= elems->last;
		
		if(ik) ik->flag= 1;
	}
}

static int float_to_frame (float frame) 
{
	int to= (int) frame;
	
	if (frame-to>0.5) to++;
	
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
	
	base= FIRSTBASE;
	while(base) {
		if TESTBASE(base) {
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
	
	allqueue(REDRAWNLA, 0);
	allqueue(REDRAWACTION, 0);
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWIPO, 0);
	allspace(REMAKEIPO, 0);

}

void nextkey_ipo(int dir)			/* call from ipo queue */
{
	IpoKey *ik;
	int a;
	
	if(G.sipo->showkey==0) return;
	
	nextkey(&G.sipo->ipokey, dir);
	
	/* copy to beziers */
	ik= G.sipo->ipokey.first;
	while(ik) {
		for(a=0; a<G.sipo->totipo; a++) {
			if(ik->data[a]) ik->data[a]->f1= ik->data[a]->f2= ik->data[a]->f3= ik->flag;
		}
		ik= ik->next;
	}		

	allqueue(REDRAWNLA, 0);
	allqueue(REDRAWACTION, 0);
	allqueue(REDRAWIPO, 0);
	if(G.sipo->blocktype == ID_OB) allqueue(REDRAWVIEW3D, 0);
}

void nextkey_obipo(int dir)		/* only call external from view3d queue */
{
	Base *base;
	Object *ob;
	ListBase elems;
	IpoKey *ik;
	int a;
	
	/* problem: this doesnt work when you mix dLoc keys with Loc keys */
	
	base= FIRSTBASE;
	while(base) {
		if TESTBASE(base) {
			ob= base->object;
			if( (ob->ipoflag & OB_DRAWKEY) && ob->ipo && ob->ipo->showkey) {
				elems.first= elems.last= 0;
				make_ipokey_transform(ob, &elems, 0);

				if(elems.first) {
					
					nextkey(&elems, dir);
					
					/* copy to beziers */
					ik= elems.first;
					while(ik) {
						for(a=0; a<OB_TOTIPO; a++) {
							if(ik->data[a]) ik->data[a]->f1= ik->data[a]->f2= ik->data[a]->f3= ik->flag;
						}
						ik= ik->next;
					}
					
					free_ipokey(&elems);
				}
			}
		}
		
		base= base->next;
	}
	allqueue(REDRAWNLA, 0);
	allqueue(REDRAWACTION, 0);
	allqueue(REDRAWVIEW3D, 0);
	allspace(REMAKEIPO, 0);
	allqueue(REDRAWIPO, 0);
}


/* **************************************************** */


void remake_ipo_transverts(TransVert *transmain, float *dvec, int tot)
{
	EditIpo *ei;
	TransVert *tv;
	BezTriple *bezt;
	int a, b;
	
	ei= G.sipo->editipo;
	for(a=0; a<G.sipo->totipo; a++, ei++) {
		
		if ISPOIN(ei, flag & IPO_VISIBLE, icu) {
				
			if(ei->icu->bezt) {
				sort_time_ipocurve(ei->icu);
			}
		}
	}

	ei= G.sipo->editipo;
	tv= transmain;
	for(a=0; a<G.sipo->totipo; a++, ei++) {
		
		if ISPOIN(ei, flag & IPO_VISIBLE, icu) {
			if( (ei->flag & IPO_EDIT) || G.sipo->showkey) {
				if(ei->icu->bezt) {
					bezt= ei->icu->bezt;
					b= ei->icu->totvert;
					while(b--) {
						if(ei->icu->ipo==IPO_BEZ) {
							if(bezt->f1 & 1) {
								tv->loc= bezt->vec[0];
								tv++;
							}
							if(bezt->f3 & 1) {
								tv->loc= bezt->vec[2];
								tv++;
							}
						}
						if(bezt->f2 & 1) {
							tv->loc= bezt->vec[1];
							tv++;
						}
						
						bezt++;
					}
					testhandles_ipocurve(ei->icu);
				}
			}
		}
	}
	
	if(G.sipo->showkey) make_ipokey();
	
	if(dvec==0) return;
	
	tv= transmain;
	for(a=0; a<tot; a++, tv++) {
		tv->oldloc[0]= tv->loc[0]-dvec[0];
		tv->oldloc[1]= tv->loc[1]-dvec[1];
	}
}

void transform_ipo(int mode)
{
	EditIpo *ei;
	BezTriple *bezt;
	TransVert *transmain = NULL, *tv;
	float xref=1.0, yref=1.0, dx, dy, dvec[2], min[3], max[3], vec[2], div, cent[2], size[2], sizefac;
	int tot=0, a, b, firsttime=1, afbreek=0, midtog= 0, dosort, proj = 0;
	unsigned short event = 0;
	short mval[2], val, xo, yo, xn, yn, xc, yc;
	char str[32];
	
	if(G.sipo->ipo && G.sipo->ipo->id.lib) return;
	if(G.sipo->editipo==0) return;
	if(mode=='r') return;	/* from gesture */
	
	INIT_MINMAX(min, max);
	
	/* which vertices are involved */
	get_status_editipo();
	if(totipo_vertsel) {
		tot= totipo_vertsel;
		tv=transmain= MEM_callocN(tot*sizeof(TransVert), "transmain");
		
		ei= G.sipo->editipo;
		for(a=0; a<G.sipo->totipo; a++, ei++) {
			
			if ISPOIN(ei, flag & IPO_VISIBLE, icu) {
				if( (ei->flag & IPO_EDIT) || G.sipo->showkey) {

				
					if(ei->icu->bezt) {
						bezt= ei->icu->bezt;
						b= ei->icu->totvert;
						while(b--) {
							if(ei->icu->ipo==IPO_BEZ) {
								if(bezt->f1 & 1) {
									tv->loc= bezt->vec[0];
									VECCOPY(tv->oldloc, tv->loc);
									if(ei->disptype==IPO_DISPBITS) tv->flag= 1;
									
									/* we take the middle vertex */
									DO_MINMAX2(bezt->vec[1], min, max);

									tv++;
								}
								if(bezt->f3 & 1) {
									tv->loc= bezt->vec[2];
									VECCOPY(tv->oldloc, tv->loc);
									if(ei->disptype==IPO_DISPBITS) tv->flag= 1;
									
									/* we take the middle vertex */
									DO_MINMAX2(bezt->vec[1], min, max);

									tv++;
								}
							}
							if(bezt->f2 & 1) {
								tv->loc= bezt->vec[1];
								VECCOPY(tv->oldloc, tv->loc);
								if(ei->disptype==IPO_DISPBITS) tv->flag= 1;
								DO_MINMAX2(bezt->vec[1], min, max);
								tv++;
							}
							bezt++;
						}
					}
				}
			}
		}
		
	}
	else if(totipo_edit==0 && totipo_sel!=0) {
		
		ei= G.sipo->editipo;
		for(a=0; a<G.sipo->totipo; a++, ei++) {
			if ISPOIN3(ei, flag & IPO_VISIBLE, flag & IPO_SELECT, icu) {
				if(ei->icu->bezt && ei->icu->ipo==IPO_BEZ) tot+= 3*ei->icu->totvert;
				else tot+= ei->icu->totvert;
			}
		}
		if(tot==0) return;
		
		tv=transmain= MEM_callocN(tot*sizeof(TransVert), "transmain");

		ei= G.sipo->editipo;
		for(a=0; a<G.sipo->totipo; a++, ei++) {
			if ISPOIN3(ei, flag & IPO_VISIBLE, flag & IPO_SELECT, icu) {
				if(ei->icu->bezt) {
					
					bezt= ei->icu->bezt;
					b= ei->icu->totvert;
					while(b--) {
						if(ei->icu->ipo==IPO_BEZ) {
							tv->loc= bezt->vec[0];
							VECCOPY(tv->oldloc, tv->loc);
							if(ei->disptype==IPO_DISPBITS) tv->flag= 1;
							tv++;
						
							tv->loc= bezt->vec[2];
							VECCOPY(tv->oldloc, tv->loc);
							if(ei->disptype==IPO_DISPBITS) tv->flag= 1;
							tv++;
						}
						tv->loc= bezt->vec[1];
						VECCOPY(tv->oldloc, tv->loc);
						if(ei->disptype==IPO_DISPBITS) tv->flag= 1;
						
						DO_MINMAX2(bezt->vec[1], min, max);
						
						tv++;
						
						bezt++;
					}
				}
			}
		}

	}

	if(tot==0) {
		if(totipo_edit==0) move_keys();
		return;
	}

	cent[0]= (float)((min[0]+max[0])/2.0);
	cent[1]= (float)((min[1]+max[1])/2.0);

	if(G.sipo->showkey) {
		midtog= 1;
		proj= 1;
	}
	
	ipoco_to_areaco(G.v2d, cent, mval);
	xc= mval[0];
	yc= mval[1];
	
	getmouseco_areawin(mval);
	xo= xn= mval[0];
	yo= yn= mval[1];
	dvec[0]= dvec[1]= 0.0;
	
	sizefac= (float)(sqrt( (float)((yc-yn)*(yc-yn)+(xn-xc)*(xn-xc)) ));
	if(sizefac<2.0) sizefac= 2.0;

	while(afbreek==0) {
		getmouseco_areawin(mval);
		if(mval[0]!=xo || mval[1]!=yo || firsttime) {
			
			if(mode=='g') {
			
				dx= (float)(mval[0]- xo);
				dy= (float)(mval[1]- yo);
	
				div= (float)(G.v2d->mask.xmax-G.v2d->mask.xmin);
				dvec[0]+= (G.v2d->cur.xmax-G.v2d->cur.xmin)*(dx)/div;
	
				div= (float)(G.v2d->mask.ymax-G.v2d->mask.ymin);
				dvec[1]+= (G.v2d->cur.ymax-G.v2d->cur.ymin)*(dy)/div;
				
				if(midtog) dvec[proj]= 0.0;
				
				/* vec is reused below: remake_ipo_transverts */
				vec[0]= dvec[0];
				vec[1]= dvec[1];
				
				apply_keyb_grid(vec, 0.0, (float)1.0, (float)0.1, U.flag & AUTOGRABGRID);
				apply_keyb_grid(vec+1, 0.0, (float)1.0, (float)0.1, 0);
				
				tv= transmain;
				for(a=0; a<tot; a++, tv++) {
					tv->loc[0]= tv->oldloc[0]+vec[0];

					if(tv->flag==0) tv->loc[1]= tv->oldloc[1]+vec[1];
				}
				
				sprintf(str, "X: %.3f   Y: %.3f  ", vec[0], vec[1]);
				headerprint(str);
			}
			else if(mode=='s') {
				
				size[0]=size[1]=(float)( (sqrt( (float)((yc-mval[1])*(yc-mval[1])+(mval[0]-xc)*(mval[0]-xc)) ))/sizefac);
				
				if(midtog) size[proj]= 1.0;
				size[0]*= xref;
				size[1]*= yref;
				
				apply_keyb_grid(size, 0.0, (float)0.2, (float)0.1, U.flag & AUTOSIZEGRID);
				apply_keyb_grid(size+1, 0.0, (float)0.2, (float)0.1, U.flag & AUTOSIZEGRID);

				tv= transmain;

				for(a=0; a<tot; a++, tv++) {
					tv->loc[0]= size[0]*(tv->oldloc[0]-cent[0])+ cent[0];
					if(tv->flag==0) tv->loc[1]= size[1]*(tv->oldloc[1]-cent[1])+ cent[1];
				}
				
				sprintf(str, "sizeX: %.3f   sizeY: %.3f  ", size[0], size[1]);
				headerprint(str);
				
			}
			
			xo= mval[0];
			yo= mval[1];
				
			dosort= 0;
			ei= G.sipo->editipo;
			for(a=0; a<G.sipo->totipo; a++, ei++) {
				if ISPOIN(ei, flag & IPO_VISIBLE, icu) {
					
					/* watch it: if the time is wrong: do not correct handles */
					if (test_time_ipocurve(ei->icu) ) dosort++;
					else testhandles_ipocurve(ei->icu);
				}
			}
			
			if(dosort) {
				if(mode=='g') remake_ipo_transverts(transmain, vec, tot);
				else remake_ipo_transverts(transmain, 0, tot);
			}
			if(G.sipo->showkey) update_ipokey_val();
			
			calc_ipo(G.sipo->ipo, (float)CFRA);

			/* update realtime */
			if(G.sipo->lock) {
				if(G.sipo->blocktype==ID_MA) {
					force_draw_plus(SPACE_BUTS);
				}
				else if(G.sipo->blocktype==ID_KE) {
					do_ob_key(OBACT);
					makeDispList(OBACT);
					force_draw_plus(SPACE_VIEW3D);
				}
				else if(G.sipo->blocktype==ID_AC) {
					do_all_actions();
					force_draw_all();
				}
				else if(G.sipo->blocktype==ID_OB) {
					Base *base= FIRSTBASE;
					
					while(base) {
						if(base->object->ipo==G.sipo->ipo) do_ob_ipo(base->object);
						base= base->next;
					}
					force_draw_plus(SPACE_VIEW3D);
				}
				else force_draw();
			}
			else {
				force_draw();
			}
			firsttime= 0;
		}
		else BIF_wait_for_statechange();
		
		while(qtest()) {
			event= extern_qread(&val);
			if(val) {
				switch(event) {
				case ESCKEY:
				case LEFTMOUSE:
				case RIGHTMOUSE:
				case SPACEKEY:
				case RETKEY:
					afbreek= 1;
					break;
				case MIDDLEMOUSE:
					if(G.sipo->showkey==0) {
						midtog= ~midtog;
						if(midtog) {
							if( abs(mval[0]-xn) > abs(mval[1]-yn)) proj= 1;
							else proj= 0;
							firsttime= 1;
						}
					}
					break;
				case XKEY:
				case YKEY:
					if(event==XKEY) xref= -xref;
					else if(G.sipo->showkey==0) yref= -yref;
					firsttime= 1;
					break;
				case LEFTCTRLKEY:
				case RIGHTCTRLKEY:
					firsttime= 1;
					break;
				default:
					if(mode=='g') {
						if(G.qual & LR_CTRLKEY) {
							if(event==LEFTARROWKEY) {dvec[0]-= 1.0; firsttime= 1;}
							else if(event==RIGHTARROWKEY) {dvec[0]+= 1.0; firsttime= 1;}
							else if(event==UPARROWKEY) {dvec[1]+= 1.0; firsttime= 1;}
							else if(event==DOWNARROWKEY) {dvec[1]-= 1.0; firsttime= 1;}
						}
						else arrows_move_cursor(event);
					}
					else arrows_move_cursor(event);
				}
			}
			if(afbreek) break;
		}
	}
	
	if(event==ESCKEY || event==RIGHTMOUSE) {
		tv= transmain;
		for(a=0; a<tot; a++, tv++) {
			tv->loc[0]= tv->oldloc[0];
			tv->loc[1]= tv->oldloc[1];
		}
		
		dosort= 0;
		ei= G.sipo->editipo;
		for(a=0; a<G.sipo->totipo; a++, ei++) {
			if ISPOIN(ei, flag & IPO_VISIBLE, icu) {
				if( (ei->flag & IPO_EDIT) || G.sipo->showkey) {
					if( test_time_ipocurve(ei->icu)) {
						dosort= 1;
						break;
					}
				}
			}
		}
		
		if(dosort) remake_ipo_transverts(transmain, 0, tot);
		
		ei= G.sipo->editipo;
		for(a=0; a<G.sipo->totipo; a++, ei++) {
			if ISPOIN(ei, flag & IPO_VISIBLE, icu) {
				if( (ei->flag & IPO_EDIT) || G.sipo->showkey) {
					testhandles_ipocurve(ei->icu);
				}
			}
		}
		calc_ipo(G.sipo->ipo, (float)CFRA);
	}
	
	editipo_changed(G.sipo, 1);

	MEM_freeN(transmain);
}

void clever_numbuts_ipo()
{
	BezTriple *bezt=0, *bezt1;
	Key *key;
	KeyBlock *kb;
	EditIpo *ei;
	float far, delta[3], old[3];
	int a, b, scale10=0, totbut=2;

	if(G.sipo->ipo && G.sipo->ipo->id.lib) return;
	if(G.sipo->editipo==0) return;
	
	/* which vertices are involved */
	get_status_editipo();

	if(G.qual & LR_SHIFTKEY) totbut= 1;

	if(G.vd==0) far= 10000.0;
	else far= (float)(MAX2(G.vd->far, 10000.0));

	if(totipo_vertsel) {

		ei= G.sipo->editipo;
		for(a=0; a<G.sipo->totipo; a++, ei++) {
			
			if ISPOIN(ei, flag & IPO_VISIBLE, icu) {
				if( (ei->flag & IPO_EDIT) || G.sipo->showkey) {

					if(ei->icu->bezt) {
						bezt1= ei->icu->bezt;
						b= ei->icu->totvert;
						while(b--) {
							if(BEZSELECTED(bezt1)) {
								bezt= bezt1;
								break;
							}
							bezt1++;
						}
						
					}
				}
			}
			if(bezt) break;
		}
		
		if(bezt==0) return;

		if(bezt->f2 & 1) {
			
			VECCOPY(old, bezt->vec[1]);
			
			if(totipo_vis==1 && G.sipo->blocktype==ID_OB) {
				if ELEM4(ei->icu->adrcode, OB_TIME, OB_ROT_X, OB_ROT_Y, OB_ROT_Z) scale10= 1;
				if ELEM3(ei->icu->adrcode, OB_DROT_X, OB_DROT_Y, OB_DROT_Z) scale10= 1;
			}
			if(scale10) bezt->vec[1][1]*= 10.0;

			add_numbut(0, NUM|FLO, "LocX:", -1000, 10000, bezt->vec[1], 0);
			if(totbut==2) add_numbut(1, NUM|FLO, "LocY:", -far, far, bezt->vec[1]+1, 0);
			do_clever_numbuts("Active BezierPoint", totbut, REDRAW);

			if(scale10) bezt->vec[1][1]/= 10.0;

			VecSubf(delta, bezt->vec[1], old);
			VECCOPY(bezt->vec[1], old);
			
			/* apply */
			ei= G.sipo->editipo;
			for(a=0; a<G.sipo->totipo; a++, ei++) {
				if ISPOIN(ei, flag & IPO_VISIBLE, icu) {
					if( (ei->flag & IPO_EDIT) || G.sipo->showkey) {
						if(ei->icu->bezt) {
							bezt= ei->icu->bezt;
							b= ei->icu->totvert;
							while(b--) {
								if(bezt->f2 & 1) {
									bezt->vec[0][0]+= delta[0];
									bezt->vec[1][0]+= delta[0];
									bezt->vec[2][0]+= delta[0];

									bezt->vec[0][1]+= delta[1];
									bezt->vec[1][1]+= delta[1];
									bezt->vec[2][1]+= delta[1];
								}
								bezt++;
							}
						}
					}
				}
			}

			ei= G.sipo->editipo;
			for(a=0; a<G.sipo->totipo; a++, ei++) {
				if ISPOIN(ei, flag & IPO_VISIBLE, icu) {
					sort_time_ipocurve(ei->icu);
					testhandles_ipocurve(ei->icu);
				}
			}

		}
		else if(bezt->f1 & 1) {
			add_numbut(0, NUM|FLO, "LocX:", -1000, 10000, bezt->vec[0], 0);
			if(totbut==2) add_numbut(1, NUM|FLO, "LocY:", -far, far, bezt->vec[0]+1, 0);
		
			do_clever_numbuts("Active HandlePoint", totbut, REDRAW);
		}
		else if(bezt->f3 & 1) {
			add_numbut(0, NUM|FLO, "LocX:", -1000, 10000, bezt->vec[0], 0);
			if(totbut==2) add_numbut(1, NUM|FLO, "LocY:", -far, far, bezt->vec[2]+1, 0);
		
			do_clever_numbuts("Active HandlePoint", totbut, REDRAW);
		}

		editipo_changed(G.sipo, 1);
	}
	else {
		
		if(G.sipo->blocktype==ID_KE) {
			key= (Key *)G.sipo->from;
			
			if(key==0) return;
			
			kb= key->block.first;
			while(kb) {
				if(kb->flag & SELECT) break;
				kb= kb->next;
			}			
			if(kb && G.sipo->rowbut&1) {
				add_numbut(0, NUM|FLO, "Pos:", -100, 100, &kb->pos, 0);
				do_clever_numbuts("Active Key", 1, REDRAW);		
				sort_keys(key);
			}
		}
	}
}

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
	icu->bezt= 0;
	
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

void ipo_record()
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
	Ipo *ipo;
	void *poin;
	double swaptime;
	float or1, or2 = 0.0, fac, *data1, *data2;
	int type, a, afbreek=0, firsttime=1, cfrao, cfra, sfra, efra;
	unsigned short event = 0;
	short anim, val, xn, yn, mvalo[2], mval[2];
	char str[128];
	
	if(G.sipo->from==0) return;
	if(SFRA>=EFRA) return;
	
	anim= pupmenu("Record Mouse %t|Still %x1|Play anim %x2");
	if(anim < 1) return;
	if(anim!=2) anim= 0;

	ipo= get_ipo(G.sipo->from, G.sipo->blocktype, 1);	/* 1= make */
	if(G.sipo) G.sipo->ipo= ipo;

	/* find the curves... */
	
	ei= G.sipo->editipo;
	for(a=0; a<G.sipo->totipo; a++) {
		if(ei->flag & IPO_VISIBLE) {
			
			if(ei1==0) ei1= ei;
			else if(ei2==0) ei2= ei;
			else {
				error("Max 2 visible curves");
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
	if(ei1->icu==0) ei1->icu= get_ipocurve(G.sipo->from, G.sipo->blocktype, ei1->adrcode, 0);
	if(ei1->icu==0) return;
	poin= get_ipo_poin(G.sipo->from, ei1->icu, &type);
	if(poin) ei1->icu->curval= read_ipo_poin(poin, type);
	or1= ei1->icu->curval;
	ei1->icu->flag |= IPO_LOCK;
	
	if(ei2) {
		if(ei2->icu==0)  ei2->icu= get_ipocurve(G.sipo->from, G.sipo->blocktype, ei2->adrcode, 0);
		if(ei2->icu==0) return;
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
	swaptime= speed_to_swaptime(G.animspeed);
	cfrao= CFRA;
	cfra=efra= SFRA;
	sfra= EFRA;
	
	while(afbreek==0) {
		
		getmouseco_areawin(mval);
		
		if(mval[0]!= mvalo[0] || mval[1]!=mvalo[1] || firsttime || (G.qual & LR_CTRLKEY)) {
			if(anim) CFRA= cfra;
			else firsttime= 0;

			set_timecursor(cfra);
			
			/* do ipo: first all, then the specific ones */
			if(anim==2) {
				do_all_ipos();
				do_all_keys();
			}

			ei1->icu->curval= or1 + fac*(mval[0]-xn);
			if(ei2) ei2->icu->curval= or2 + fac*(mval[1]-yn);

			do_ipo_nocalc(G.sipo->ipo);
			do_all_visible_ikas();
			
			if(G.qual & LR_CTRLKEY) {
				sprintf(str, "Recording... %d\n", cfra);
				data1[ cfra-SFRA ]= ei1->icu->curval;
				if(ei2) data2[ cfra-SFRA ]= ei2->icu->curval;
				
				sfra= MIN2(sfra, cfra);
				efra= MAX2(efra, cfra);
			}
			else sprintf(str, "Mouse Recording. Use CTRL to start. LeftMouse or Space to end");
			
			do_ob_key(OBACT);

			headerprint(str);

			if(sa) scrarea_do_windraw(sa);

			/* minimal wait swaptime */
			tottime -= swaptime;
			while (update_time()) PIL_sleep_ms(1);

			screen_swapbuffers();
			
			tottime= 0.0;
			
			mvalo[0]= mval[0];
			mvalo[1]= mval[1];
			
			if(anim || (G.qual & LR_CTRLKEY)) {
				cfra++;
				if(cfra>EFRA) cfra= SFRA;
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
		if(ei1->icu->bezt==0) {
			BLI_remlink( &(G.sipo->ipo->curve), ei1->icu);
			MEM_freeN(ei1->icu);
			ei1->icu= 0;
		}
		if(ei2) {
			poin= get_ipo_poin(G.sipo->from, ei2->icu, &type);
			if(poin) write_ipo_poin(poin, type, or2);
			if(ei2->icu->bezt==0) {
				BLI_remlink( &(G.sipo->ipo->curve), ei2->icu);
				MEM_freeN(ei2->icu);
				ei2->icu= 0;
			}
		}
	}
	
	if(ei1->icu) ei1->icu->flag &= ~IPO_LOCK;	
	if(ei2 && ei2->icu) ei2->icu->flag &= ~IPO_LOCK;	
	
	editipo_changed(G.sipo, 0);
	do_ipo(G.sipo->ipo);
	waitcursor(0);
	allqueue(REDRAWVIEW3D, 0);
	if(sa) scrarea_queue_headredraw(sa);	/* headerprint */
	scrarea_queue_redraw(oldarea);
	CFRA= cfrao;
	
	/* for the time being? */
	update_for_newframe();
	
	MEM_freeN(data1);
	MEM_freeN(data2);
}



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


int is_ipo_key_selected(Ipo *ipo)
{
	int i;
	IpoCurve *icu;

	if (!ipo)
		return 0;

	for (icu=ipo->curve.first; icu; icu=icu->next){
		for (i=0; i<icu->totvert; i++)
			if (BEZSELECTED(&icu->bezt[i]))
				return 1;
	}

	return 0;
}


void set_ipo_key_selection(Ipo *ipo, int sel)
{
	int i;
	IpoCurve *icu;

	if (!ipo)
		return;

	for (icu=ipo->curve.first; icu; icu=icu->next){
		for (i=0; i<icu->totvert; i++){
			if (sel){
				icu->bezt[i].f1|=1;
				icu->bezt[i].f2|=1;
				icu->bezt[i].f3|=1;
			}
			else{
				icu->bezt[i].f1&=~1;
				icu->bezt[i].f2&=~1;
				icu->bezt[i].f3&=~1;
			}
		}
	}
}

void delete_ipo_keys(Ipo *ipo)
{
	IpoCurve *icu, *next;
	int i;
	
	if (!ipo)
		return;
	
	for (icu=ipo->curve.first; icu; icu=next){
		next = icu->next;
		for (i=0; i<icu->totvert; i++){
			if (icu->bezt[i].f2 & 1){
				//	Delete the item
				memcpy (&icu->bezt[i], &icu->bezt[i+1], sizeof (BezTriple)*(icu->totvert-i-1));
				icu->totvert--;
				i--;
			}
		}
		if (!icu->totvert){
			/* Delete the curve */
			BLI_remlink( &(ipo->curve), icu);
			if(icu->bezt) MEM_freeN(icu->bezt);
			MEM_freeN(icu);
		}
	}
}

int fullselect_ipo_keys(Ipo *ipo)
{
	int i;
	IpoCurve *icu;
	int tvtot = 0;

	if (!ipo)
		return tvtot;
	
	for (icu=ipo->curve.first; icu; icu=icu->next){
		for (i=0; i<icu->totvert; i++){
			if (icu->bezt[i].f2 & 1){
				tvtot+=3;
				icu->bezt[i].f1 |= 1;
				icu->bezt[i].f3 |= 1;
			}
		}
	}

	return tvtot;
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

void borderselect_icu_key(IpoCurve *icu, float xmin, float xmax, 
						  int (*select_function)(BezTriple *))
{
	/* Selects all bezier triples in the Ipocurve 
	 * between times xmin and xmax, using the selection
	 * function.
	 */

	int i;

	/* loop through all of the bezier triples in
	 * the Ipocurve -- if the triple occurs between
	 * times xmin and xmax then select it using the selection
	 * function
	 */
	for (i=0; i<icu->totvert; i++){
		if (icu->bezt[i].vec[1][0] > xmin && icu->bezt[i].vec[1][0] < xmax ){
			select_function(&(icu->bezt[i]));
		}
	}
}

void borderselect_ipo_key(Ipo *ipo, float xmin, float xmax, int selectmode)
{
	/* Selects all bezier triples in each Ipocurve of the
	 * Ipo between times xmin and xmax, using the selection mode.
	 */

	IpoCurve *icu;
	int (*select_function)(BezTriple *);

	/* If the ipo is no good then return */
	if (!ipo)
		return;

	/* Set the selection function based on the
	 * selection mode.
	 */
	switch(selectmode) {
	case SELECT_ADD:
		select_function = select_bezier_add;
		break;
	case SELECT_SUBTRACT:
		select_function = select_bezier_subtract;
		break;
	case SELECT_INVERT:
		select_function = select_bezier_invert;
		break;
	default:
		return;
	}

	/* loop through all of the bezier triples in all
	 * of the Ipocurves -- if the triple occurs between
	 * times xmin and xmax then select it using the selection
	 * function
	 */
	for (icu=ipo->curve.first; icu; icu=icu->next){
		borderselect_icu_key(icu, xmin, xmax, select_function);
	}
}

void select_ipo_key(Ipo *ipo, float selx, int selectmode)
{
	/* Selects all bezier triples in each Ipocurve of the
	 * Ipo at time selx, using the selection mode.
	 */
	int i;
	IpoCurve *icu;
	int (*select_function)(BezTriple *);

	/* If the ipo is no good then return */
	if (!ipo)
		return;

	/* Set the selection function based on the
	 * selection mode.
	 */
	switch(selectmode) {
	case SELECT_ADD:
		select_function = select_bezier_add;
		break;
	case SELECT_SUBTRACT:
		select_function = select_bezier_subtract;
		break;
	case SELECT_INVERT:
		select_function = select_bezier_invert;
		break;
	default:
		return;
	}

	/* loop through all of the bezier triples in all
	 * of the Ipocurves -- if the triple occurs at
	 * time selx then select it using the selection
	 * function
	 */
	for (icu=ipo->curve.first; icu; icu=icu->next){
		for (i=0; i<icu->totvert; i++){
			if (icu->bezt[i].vec[1][0]==selx){
				select_function(&(icu->bezt[i]));
			}
		}
	}
}

void select_icu_key(IpoCurve *icu, float selx, int selectmode)
{
    /* Selects all bezier triples in the Ipocurve
     * at time selx, using the selection mode.
     * This is kind of sloppy the obvious similarities
     * with the above function, forgive me ...
     */
    int i;
    int (*select_function)(BezTriple *);

    /* If the icu is no good then return */
    if (!icu)
        return;

    /* Set the selection function based on the
     * selection mode.
     */
    switch(selectmode) {
    case SELECT_ADD:
        select_function = select_bezier_add;
        break;
    case SELECT_SUBTRACT:
        select_function = select_bezier_subtract;
        break;
    case SELECT_INVERT:
        select_function = select_bezier_invert;
        break;
    default:
        return;
    }

    /* loop through all of the bezier triples in
     * the Ipocurve -- if the triple occurs at
     * time selx then select it using the selection
     * function
     */
    for (i=0; i<icu->totvert; i++){
        if (icu->bezt[i].vec[1][0]==selx){
            select_function(&(icu->bezt[i]));
        }
    }

}
