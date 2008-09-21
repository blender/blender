/**
 * $Id: 
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
 * Contributor(s): Blender Foundation, 2005. Full recode
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/* ********** General calls (minimal dependencies) for editing Ipos in Blender ************* */

#include <stdio.h>

#include "BLI_blenlib.h"
#include "BLI_arithb.h"

#include "DNA_curve_types.h"
#include "DNA_ipo_types.h"
#include "DNA_key_types.h"
#include "DNA_object_types.h"
#include "DNA_space_types.h"
#include "DNA_scene_types.h"
#include "DNA_view3d_types.h"

#include "BKE_global.h"
#include "BKE_ipo.h"
#include "BKE_key.h"
#include "BKE_utildefines.h"

#include "BIF_resources.h"

#include "BSE_edit.h"
#include "BSE_editipo_types.h"
#include "BSE_editipo.h"
#include "BSE_drawipo.h"

#include "blendef.h"
#include "mydevice.h"

char *ob_ic_names[OB_TOTNAM] = { "LocX", "LocY", "LocZ", "dLocX", "dLocY", "dLocZ",
	"RotX", "RotY", "RotZ", "dRotX", "dRotY", "dRotZ",
	"ScaleX", "ScaleY", "ScaleZ", "dScaleX", "dScaleY", "dScaleZ",
	"Layer", "Time", "ColR", "ColG", "ColB", "ColA",
	"FStreng", "FFall", "RDamp", "Damping", "Perm", "FMaxD" };

char *co_ic_names[CO_TOTNAM] = { "Inf", "HeadTail" };
char *mtex_ic_names[TEX_TOTNAM] = { "OfsX", "OfsY", "OfsZ", "SizeX", "SizeY", "SizeZ",
	"texR", "texG", "texB", "DefVar", "Col", "Nor", "Var",
	"Disp" };
char *tex_ic_names[TE_TOTNAM] = { "NSize", "NDepth", "NType", "Turb", "Vnw1", "Vnw2",
	"Vnw3", "Vnw4", "MinkMExp", "DistM", "ColT", "iScale",
	"DistA", "MgType", "MgH", "Lacu", "Oct", "MgOff",
	"MgGain", "NBase1", "NBase2", "ColR", "ColG", "ColB", "Bright", "Contras"};
char *ma_ic_names[MA_TOTNAM] = { "R", "G", "B", "SpecR", "SpecG", "SpecB", "MirR",
	"MirG", "MirB", "Ref", "Alpha", "Emit", "Amb", "Spec",
	"Hard", "SpTra", "Ior", "Mode", "HaSize", "Translu",
	"RayMir", "FresMir", "FresMirI", "FresTra", "FresTraI",
	"TraGlow" };
char *seq_ic_names[SEQ_TOTNAM] = { "Fac" };
char *cu_ic_names[CU_TOTNAM] = { "Speed" };
char *key_ic_names[KEY_TOTNAM] = { "Speed", "Key 1", "Key 2", "Key 3", "Key 4", "Key 5",
	"Key 6", "Key 7", "Key 8", "Key 9", "Key 10",
	"Key 11", "Key 12", "Key 13", "Key 14", "Key 15",
	"Key 16", "Key 17", "Key 18", "Key 19", "Key 20",
	"Key 21", "Key 22", "Key 23", "Key 24", "Key 25",
	"Key 26", "Key 27", "Key 28", "Key 29", "Key 30",
	"Key 31", "Key 32", "Key 33", "Key 34", "Key 35",
	"Key 36", "Key 37", "Key 38", "Key 39", "Key 40",
	"Key 41", "Key 42", "Key 43", "Key 44", "Key 45",
	"Key 46", "Key 47", "Key 48", "Key 49", "Key 50",
	"Key 51", "Key 52", "Key 53", "Key 54", "Key 55",
	"Key 56", "Key 57", "Key 58", "Key 59", "Key 60",
	"Key 61", "Key 62", "Key 63"};
char *wo_ic_names[WO_TOTNAM] = { "HorR", "HorG", "HorB", "ZenR", "ZenG", "ZenB", "Expos",
	"Misi", "MisDi", "MisSta", "MisHi", "StarR", "StarB",
	"StarG", "StarDi", "StarSi" };
char *la_ic_names[LA_TOTNAM] = { "Energ", "R", "G", "B", "Dist", "SpoSi", "SpoBl",
	"Quad1", "Quad2", "HaInt" };
/* yafray: two curve names added, 'Apert' for aperture, and 'FDist' for focal distance */
char *cam_ic_names[CAM_TOTNAM] = { "Lens", "ClSta", "ClEnd", "Apert", "FDist", "ShiftX", "ShiftY" };
char *snd_ic_names[SND_TOTNAM] = { "Vol", "Pitch", "Pan", "Atten" };
char *ac_ic_names[AC_TOTNAM] = {"LocX", "LocY", "LocZ", "ScaleX", "ScaleY",
	"ScaleZ", "QuatW", "QuatX", "QuatY", "QuatZ"};
char *ic_name_empty[1] ={ "" };
char *fluidsim_ic_names[FLUIDSIM_TOTNAM] = { "Fac-Visc", "Fac-Time",  "GravX","GravY","GravZ",  "VelX","VelY","VelZ", "Active", "AttrForceStr", "AttrForceRad", "VelForceStr", "VelForceRad" };
char *part_ic_names[PART_TOTNAM] = { "E_Freq", "E_Life", "E_Speed", "E_Angular", "E_Size",
"Angular", "Size", "Drag", "Brown", "Damp", "Length", "Clump",
"GravX", "GravY", "GravZ", "KinkAmp", "KinkFreq", "KinkShape", "BBTilt",
"FStreng", "FFall", "FMaxD", "F2Streng", "F2Fall", "F2MaxD"};

/* gets the appropriate icon for the given blocktype */
int geticon_ipo_blocktype(short blocktype)
{
	switch (blocktype) {
		case ID_OB:
			return ICON_OBJECT;
		case ID_PO:
			return ICON_POSE_HLT;
		case ID_KE:
			return ICON_EDIT;
		case ID_MA:
			return ICON_MATERIAL;
		case ID_WO:
			return ICON_WORLD;
		case ID_CU:
			return ICON_CURVE;
		case ID_CA:
			return ICON_CAMERA;
		case ID_LA:
			return ICON_LAMP;
		case ID_TE:
			return ICON_TEXTURE;
		case ID_CO:
			return ICON_CONSTRAINT;
		case ID_FLUIDSIM:
			return ICON_WORLD; // uggh
		default:
			return 0; // what about blank icon?
	}
}

/* get name of ipo-curve
 * 	- icu should be valid pointer
 *	- ob is only needed for a shapekey-related hack
 */
char *getname_ipocurve(IpoCurve *icu, Object *ob)
{
	switch (icu->blocktype) {
		case ID_OB: 
			return getname_ob_ei(icu->adrcode, 0); /* dummy 2nd arg */ 
		case ID_PO:
			return getname_ac_ei(icu->adrcode);
		case ID_KE:
			{
				static char name[32];
				Key *key= ob_get_key(ob);
				KeyBlock *kb= key_get_keyblock(key, icu->adrcode);
				
				/* only return name if it has been set, otherwise use 
				 * default method using static string (Key #)
				 */
				if ((kb) && (kb->name[0] != '\0'))
					return kb->name; /* return keyblock's name  */
				
				/* in case keyblock is not named or no key/keyblock was found */
				sprintf(name, "Key %d", icu->adrcode);
				return name;
			}
			
		default: /* fixme - add all of the other types! */
			return NULL;
	}
}

char *getname_ac_ei(int nr) 
{
	switch(nr) {
		case AC_LOC_X:
		case AC_LOC_Y:
		case AC_LOC_Z:
			return ac_ic_names[nr-1];
		case AC_SIZE_X:
		case AC_SIZE_Y:
		case AC_SIZE_Z:
			return ac_ic_names[nr-10];
		case AC_QUAT_X:
		case AC_QUAT_Y:
		case AC_QUAT_Z:
		case AC_QUAT_W:
			return ac_ic_names[nr-19];
		default:
			return ic_name_empty[0]; /* empty */
	}
}

char *getname_co_ei(int nr)
{
	switch(nr){
		case CO_ENFORCE:
		case CO_HEADTAIL:
			return co_ic_names[nr-1];
	}
	return ic_name_empty[0];
}

char *getname_ob_ei(int nr, int colipo)
{
	if(nr>=OB_LOC_X && nr <= OB_PD_FMAXD) return ob_ic_names[nr-1];
	
	return ic_name_empty[0];
}

char *getname_tex_ei(int nr)
{
	if(nr>=TE_NSIZE && nr<=TE_CONTRA) return tex_ic_names[nr-1];
	
	return ic_name_empty[0];
}

char *getname_mtex_ei(int nr)
{
	if(nr>=MAP_OFS_X && nr<=MAP_DISP) return mtex_ic_names[nr-1];
	
	return ic_name_empty[0];
}

char *getname_mat_ei(int nr)
{
	if(nr>=MA_MAP1) return getname_mtex_ei((nr & (MA_MAP1-1)));
	else {
		if(nr>=MA_COL_R && nr<=MA_ADD) return ma_ic_names[nr-1];
	}
	return ic_name_empty[0];
}

char *getname_world_ei(int nr)
{
	if(nr>=MA_MAP1) return getname_mtex_ei((nr & (MA_MAP1-1)));
	else {
		if(nr>=WO_HOR_R && nr<=WO_STARSIZE) return wo_ic_names[nr-1];
	}
	return ic_name_empty[0];
}

char *getname_seq_ei(int nr)
{
	if(nr == SEQ_FAC1) return seq_ic_names[nr-1];
	return ic_name_empty[0];
}

char *getname_cu_ei(int nr)
{
	if(nr==CU_SPEED) return cu_ic_names[nr-1];
	return ic_name_empty[0];
}

char *getname_la_ei(int nr)
{
	if(nr>=MA_MAP1) return getname_mtex_ei((nr & (MA_MAP1-1)));
	else {
		if(nr>=LA_ENERGY && nr<=LA_HALOINT) return la_ic_names[nr-1];
	}
	return ic_name_empty[0];
}

char *getname_cam_ei(int nr)
{
	if(nr>=CAM_LENS && nr<=CAM_SHIFT_Y) return cam_ic_names[nr-1];
	return ic_name_empty[0];
}

char *getname_snd_ei(int nr)
{
	if(nr>=SND_VOLUME && nr<=SND_ATTEN) return snd_ic_names[nr-1];
	return ic_name_empty[0];
}

char *getname_fluidsim_ei(int nr)
{
	if(nr <= FLUIDSIM_TOTIPO) return fluidsim_ic_names[nr-1];
	return ic_name_empty[0];
}
char *getname_part_ei(int nr)
{
	if(nr <= PART_TOTIPO) return part_ic_names[nr-1];
	return ic_name_empty[0];
}


void boundbox_ipocurve(IpoCurve *icu, int selectedonly)
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
					if((bezt->f2 & SELECT) || !selectedonly) {
						vec[0]= bezt->vec[1][0];
						vec[1]= 0.0;
						DO_MINMAX(vec, min, max);
						
						vec[1]= 16.0;
						DO_MINMAX(vec, min, max);
					}
				}
				else {
					if((bezt->f1 & SELECT) || !selectedonly) {
						if(icu->ipo==IPO_BEZ && a!=icu->totvert-1)
							DO_MINMAX(bezt->vec[0], min, max);
					}
					if((bezt->f2 & SELECT) || !selectedonly)
						DO_MINMAX(bezt->vec[1], min, max);
					if((bezt->f3 & SELECT) || !selectedonly) {
						if(icu->ipo==IPO_BEZ && a!=0)
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

void boundbox_ipo(Ipo *ipo, rctf *bb, int selectedonly)
{
	IpoCurve *icu;
	int first= 1;
	
	icu= ipo->curve.first;
	while(icu) {
		
		boundbox_ipocurve(icu, selectedonly);
		
		if(first) {
			*bb= icu->totrct;
			first= 0;
		}
		else BLI_union_rctf(bb, &(icu->totrct));
		
		icu= icu->next;
	}
}


unsigned int ipo_rainbow(int cur, int tot)
{
	float dfac, fac, sat;
	
	dfac= (float)(1.0/( (float)tot+1.0));
	
	/* this calculation makes 2 or 4 different cycles of rainbow colors */
	if(cur< tot/2) fac= (float)(cur*2.0f*dfac);
	else fac= (float)((cur-tot/2)*2.0f*dfac +dfac);
	if(tot > 32) fac= fac*1.95f;
	if(fac>1.0f) fac-= 1.0f;
	
	if(fac>0.5f && fac<0.8f) sat= 0.4f;
	else sat= 0.5f;
	
	return hsv_to_cpack(fac, sat, 1.0f);
}		

/* exported to python, hrms... (ton) */
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
		case 8: return MA_MAP9; 
		case 9: return MA_MAP10;
		case 10: return MA_MAP11;
		case 11: return MA_MAP12;
		case 12: return MA_MAP13;
		case 13: return MA_MAP14;
		case 14: return MA_MAP15;
		case 15: return MA_MAP16;
		case 16: return MA_MAP17;
		case 17: return MA_MAP18;
		default: return 0;
	}
}




