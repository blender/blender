/* ipo.c
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
 * Contributor(s): 2008, Joshua Leung (IPO System cleanup)
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <math.h>
#include <stdio.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "MEM_guardedalloc.h"

#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_curve_types.h"
#include "DNA_camera_types.h"
#include "DNA_lamp_types.h"
#include "DNA_ipo_types.h"
#include "DNA_key_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_object_force.h"
#include "DNA_particle_types.h"
#include "DNA_sequence_types.h"
#include "DNA_scene_types.h"
#include "DNA_sound_types.h"
#include "DNA_texture_types.h"
#include "DNA_view3d_types.h"
#include "DNA_world_types.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"

#include "BKE_bad_level_calls.h"
#include "BKE_utildefines.h"

#include "BKE_action.h"
#include "BKE_blender.h"
#include "BKE_curve.h"
#include "BKE_constraint.h"
#include "BKE_global.h"
#include "BKE_ipo.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_mesh.h"
#include "BKE_object.h"

#ifndef DISABLE_PYTHON
#include "BPY_extern.h" /* for BPY_pydriver_eval() */
#endif

#define SMALL -1.0e-10

/* ***************************** Adrcode Blocktype Defines ********************************* */

/* This array concept was meant to make sure that defines such as OB_LOC_X
   don't have to be enumerated, also for backward compatibility, future changes,
   and to enable it all can be accessed with a for-next loop.
   
   This should whole adrcode system should eventually be replaced by a proper Data API
*/


int co_ar[CO_TOTIPO]= {
	CO_ENFORCE, CO_HEADTAIL
};

int ob_ar[OB_TOTIPO]= {
	OB_LOC_X, OB_LOC_Y, OB_LOC_Z, OB_DLOC_X, OB_DLOC_Y, OB_DLOC_Z, 
	OB_ROT_X, OB_ROT_Y, OB_ROT_Z, OB_DROT_X, OB_DROT_Y, OB_DROT_Z, 
	OB_SIZE_X, OB_SIZE_Y, OB_SIZE_Z, OB_DSIZE_X, OB_DSIZE_Y, OB_DSIZE_Z, 
	OB_LAY, OB_TIME, OB_COL_R, OB_COL_G, OB_COL_B, OB_COL_A,
	OB_PD_FSTR, OB_PD_FFALL, OB_PD_SDAMP, OB_PD_RDAMP, OB_PD_PERM, OB_PD_FMAXD
};

int ac_ar[AC_TOTIPO]= {
	AC_LOC_X, AC_LOC_Y, AC_LOC_Z,  
	 AC_QUAT_W, AC_QUAT_X, AC_QUAT_Y, AC_QUAT_Z,
	AC_SIZE_X, AC_SIZE_Y, AC_SIZE_Z
};

int ma_ar[MA_TOTIPO]= {
	MA_COL_R, MA_COL_G, MA_COL_B, 
	MA_SPEC_R, MA_SPEC_G, MA_SPEC_B, 
	MA_MIR_R, MA_MIR_G, MA_MIR_B,
	MA_REF, MA_ALPHA, MA_EMIT, MA_AMB, 
	MA_SPEC, MA_HARD, MA_SPTR, MA_IOR, 
	MA_MODE, MA_HASIZE, MA_TRANSLU, MA_RAYM,
	MA_FRESMIR, MA_FRESMIRI, MA_FRESTRA, MA_FRESTRAI, MA_ADD,
	
	MA_MAP1+MAP_OFS_X, MA_MAP1+MAP_OFS_Y, MA_MAP1+MAP_OFS_Z, 
	MA_MAP1+MAP_SIZE_X, MA_MAP1+MAP_SIZE_Y, MA_MAP1+MAP_SIZE_Z, 
	MA_MAP1+MAP_R, MA_MAP1+MAP_G, MA_MAP1+MAP_B,
	MA_MAP1+MAP_DVAR, MA_MAP1+MAP_COLF, MA_MAP1+MAP_NORF, MA_MAP1+MAP_VARF, MA_MAP1+MAP_DISP
};

int te_ar[TE_TOTIPO] ={
	
	TE_NSIZE, TE_NDEPTH, TE_NTYPE, TE_TURB,
	
	TE_VNW1, TE_VNW2, TE_VNW3, TE_VNW4,
	TE_VNMEXP, TE_VN_COLT, TE_VN_DISTM,
	
	TE_ISCA, TE_DISTA,
	
	TE_MG_TYP, TE_MGH, TE_MG_LAC, TE_MG_OCT, TE_MG_OFF, TE_MG_GAIN,
	
	TE_N_BAS1, TE_N_BAS2,
	
	TE_COL_R, TE_COL_G, TE_COL_B, TE_BRIGHT, TE_CONTRA
};

int seq_ar[SEQ_TOTIPO]= {
	SEQ_FAC1
};

int cu_ar[CU_TOTIPO]= {
	CU_SPEED
};

int wo_ar[WO_TOTIPO]= {
	WO_HOR_R, WO_HOR_G, WO_HOR_B, WO_ZEN_R, WO_ZEN_G, WO_ZEN_B, 
	WO_EXPOS, WO_MISI, WO_MISTDI, WO_MISTSTA, WO_MISTHI,
	WO_STAR_R, WO_STAR_G, WO_STAR_B, WO_STARDIST, WO_STARSIZE, 

	MA_MAP1+MAP_OFS_X, MA_MAP1+MAP_OFS_Y, MA_MAP1+MAP_OFS_Z, 
	MA_MAP1+MAP_SIZE_X, MA_MAP1+MAP_SIZE_Y, MA_MAP1+MAP_SIZE_Z, 
	MA_MAP1+MAP_R, MA_MAP1+MAP_G, MA_MAP1+MAP_B,
	MA_MAP1+MAP_DVAR, MA_MAP1+MAP_COLF, MA_MAP1+MAP_NORF, MA_MAP1+MAP_VARF
};

int la_ar[LA_TOTIPO]= {
	LA_ENERGY, LA_COL_R, LA_COL_G, LA_COL_B, 
	LA_DIST, LA_SPOTSI, LA_SPOTBL, 
	LA_QUAD1, LA_QUAD2, LA_HALOINT,  

	MA_MAP1+MAP_OFS_X, MA_MAP1+MAP_OFS_Y, MA_MAP1+MAP_OFS_Z, 
	MA_MAP1+MAP_SIZE_X, MA_MAP1+MAP_SIZE_Y, MA_MAP1+MAP_SIZE_Z, 
	MA_MAP1+MAP_R, MA_MAP1+MAP_G, MA_MAP1+MAP_B,
	MA_MAP1+MAP_DVAR, MA_MAP1+MAP_COLF
};

/* yafray: aperture & focal distance curves added */
/* qdn: FDIST now available to Blender as well for defocus node */
int cam_ar[CAM_TOTIPO]= {
	CAM_LENS, CAM_STA, CAM_END, CAM_YF_APERT, CAM_YF_FDIST, CAM_SHIFT_X, CAM_SHIFT_Y
};

int snd_ar[SND_TOTIPO]= {
	SND_VOLUME, SND_PITCH, SND_PANNING, SND_ATTEN
};

int fluidsim_ar[FLUIDSIM_TOTIPO]= {
	FLUIDSIM_VISC, FLUIDSIM_TIME,
	FLUIDSIM_GRAV_X , FLUIDSIM_GRAV_Y , FLUIDSIM_GRAV_Z ,
	FLUIDSIM_VEL_X  , FLUIDSIM_VEL_Y  , FLUIDSIM_VEL_Z  ,
	FLUIDSIM_ACTIVE,
	FLUIDSIM_ATTR_FORCE_STR, FLUIDSIM_ATTR_FORCE_RADIUS,
	FLUIDSIM_VEL_FORCE_STR, FLUIDSIM_VEL_FORCE_RADIUS,
};

int part_ar[PART_TOTIPO]= {
	PART_EMIT_FREQ, PART_EMIT_LIFE, PART_EMIT_VEL, PART_EMIT_AVE, PART_EMIT_SIZE,
	PART_AVE, PART_SIZE, PART_DRAG, PART_BROWN, PART_DAMP, PART_LENGTH, PART_CLUMP,
    PART_GRAV_X, PART_GRAV_Y, PART_GRAV_Z, PART_KINK_AMP, PART_KINK_FREQ, PART_KINK_SHAPE,
	PART_BB_TILT, PART_PD_FSTR, PART_PD_FFALL, PART_PD_FMAXD, PART_PD2_FSTR, PART_PD2_FFALL, PART_PD2_FMAXD
};

/* ************************** Data-Level Functions ************************* */

/* ---------------------- Freeing --------------------------- */

/* frees the ipo curve itself too */
void free_ipo_curve (IpoCurve *icu) 
{
	if (icu == NULL) 
		return;
	
	if (icu->bezt) 
		MEM_freeN(icu->bezt);
	if (icu->driver) 
		MEM_freeN(icu->driver);
	
	MEM_freeN(icu);
}

/* do not free ipo itself */
void free_ipo (Ipo *ipo)
{
	IpoCurve *icu, *icn;
	
	if (ipo == NULL) 
		return;
	
	for (icu= ipo->curve.first; icu; icu= icn) {
		icn= icu->next;
		
		/* must remove the link before freeing, as the curve is freed too */
		BLI_remlink(&ipo->curve, icu);
		free_ipo_curve(icu);
	}
}

/* ---------------------- Init --------------------------- */

/* on adding new ipos, or for empty views */
void ipo_default_v2d_cur (int blocktype, rctf *cur)
{
	switch (blocktype) {
	case ID_CA:
		cur->xmin= (float)G.scene->r.sfra;
		cur->xmax= (float)G.scene->r.efra;
		cur->ymin= 0.0f;
		cur->ymax= 100.0f;
		break;
		
	case ID_MA: case ID_WO: case ID_LA: 
	case ID_CU: case ID_CO:
		cur->xmin= (float)(G.scene->r.sfra - 0.1f);
		cur->xmax= (float)G.scene->r.efra;
		cur->ymin= (float)-0.1f;
		cur->ymax= (float)+1.1f;
		break;
		
	case ID_TE:
		cur->xmin= (float)(G.scene->r.sfra - 0.1f);
		cur->xmax= (float)G.scene->r.efra;
		cur->ymin= (float)-0.1f;
		cur->ymax= (float)+1.1f;
		break;
		
	case ID_SEQ:
		cur->xmin= -5.0f;
		cur->xmax= 105.0f;
		cur->ymin= (float)-0.1f;
		cur->ymax= (float)+1.1f;
		break;
		
	case ID_KE:
		cur->xmin= (float)(G.scene->r.sfra - 0.1f);
		cur->xmax= (float)G.scene->r.efra;
		cur->ymin= (float)-0.1f;
		cur->ymax= (float)+2.1f;
		break;
		
	default:	/* ID_OB and everything else */
		cur->xmin= (float)G.scene->r.sfra;
		cur->xmax= (float)G.scene->r.efra;
		cur->ymin= -5.0f;
		cur->ymax= +5.0f;
		break;
	}
}

/* create a new IPO block (allocates the block) */
Ipo *add_ipo (char name[], int blocktype)
{
	Ipo *ipo;
	
	ipo= alloc_libblock(&G.main->ipo, ID_IP, name);
	ipo->blocktype= blocktype;
	ipo_default_v2d_cur(blocktype, &ipo->cur);

	return ipo;
}

/* ---------------------- Copy --------------------------- */

/* duplicate an IPO block and all its data  */
Ipo *copy_ipo (Ipo *src)
{
	Ipo *dst;
	IpoCurve *icu;
	
	if (src == NULL) 
		return NULL;
	
	dst= copy_libblock(src);
	duplicatelist(&dst->curve, &src->curve);

	for (icu= src->curve.first; icu; icu= icu->next) {
		icu->bezt= MEM_dupallocN(icu->bezt);
		
		if (icu->driver) 
			icu->driver= MEM_dupallocN(icu->driver);
	}
	
	return dst;
}

/* ---------------------- Relink --------------------------- */

/* uses id->newid to match pointers with other copied data 
 * 	- called after single-user or other such
 */
void ipo_idnew (Ipo *ipo)
{
	if (ipo) {
		IpoCurve *icu;
		
		for (icu= ipo->curve.first; icu; icu= icu->next) {
			if (icu->driver)
				ID_NEW(icu->driver->ob);
		}
	}
}

/* --------------------- Find + Check ----------------------- */

/* find the IPO-curve within a given IPO-block with the adrcode of interest */
IpoCurve *find_ipocurve (Ipo *ipo, int adrcode)
{
	if (ipo) {
		IpoCurve *icu;
		
		for (icu= ipo->curve.first; icu; icu= icu->next) {
			if (icu->adrcode == adrcode) 
				return icu;
		}
	}
	return NULL;
}

/* return whether the given IPO block has a IPO-curve with the given adrcode */
short has_ipo_code(Ipo *ipo, int adrcode)
{
	/* return success of faliure from trying to find such an IPO-curve */
	return (find_ipocurve(ipo, adrcode) != NULL);
}

/* ---------------------- Make Local --------------------------- */


/* make the given IPO local (for Objects)
 * - only lib users: do nothing
 * - only local users: set flag
 * - mixed: make copy
 */
void make_local_obipo (Ipo *src)
{
	Object *ob;
	Ipo *dst;
	int local=0, lib=0;
	
	/* check if only local and/or lib */
	for (ob= G.main->object.first; ob; ob= ob->id.next) {
		if (ob->ipo == src) {
			if (ob->id.lib) lib= 1;
			else local= 1;
		}
	}
	
	/* only local - set flag */
	if (local && lib==0) {
		src->id.lib= 0;
		src->id.flag= LIB_LOCAL;
		new_id(0, (ID *)src, 0);
	}
	/* mixed: make copy */
	else if (local && lib) {
		dst= copy_ipo(src);
		dst->id.us= 0;
		
		for (ob= G.main->object.first; ob; ob= ob->id.next) {
			if (ob->ipo == src) {
				if (ob->id.lib == NULL) {
					ob->ipo= dst;
					dst->id.us++;
					src->id.us--;
				}
			}
		}
	}
}

/* make the given IPO local (for Materials)
 * - only lib users: do nothing
 * - only local users: set flag
 * - mixed: make copy
 */
void make_local_matipo (Ipo *src)
{
	Material *ma;
	Ipo *dst;
	int local=0, lib=0;
	
	/* check if only local and/or lib */
	for (ma= G.main->mat.first; ma; ma= ma->id.next) {
		if (ma->ipo == src) {
			if (ma->id.lib) lib= 1;
			else local= 1;
		}
	}
	
	/* only local - set flag */
	if (local && lib==0) {
		src->id.lib= 0;
		src->id.flag= LIB_LOCAL;
		new_id(0, (ID *)src, 0);
	}
	/* mixed: make copy */
	else if (local && lib) {
		dst= copy_ipo(src);
		dst->id.us= 0;
		
		for (ma= G.main->mat.first; ma; ma= ma->id.next) {
			if (ma->ipo == src) {
				if (ma->id.lib == NULL) {
					ma->ipo= dst;
					dst->id.us++;
					src->id.us--;
				}
			}
		}
	}
}

/* make the given IPO local (for ShapeKeys)
 * - only lib users: do nothing
 * - only local users: set flag
 * - mixed: make copy
 */
void make_local_keyipo (Ipo *src)
{
	Key *key;
	Ipo *dst;
	int local=0, lib=0;
	
	/* check if only local and/or lib */
	for (key= G.main->key.first; key; key= key->id.next) {
		if (key->ipo == src) {
			if (key->id.lib) lib= 1;
			else local= 1;
		}
	}
	
	/* only local - set flag */
	if (local && lib==0) {
		src->id.lib= 0;
		src->id.flag= LIB_LOCAL;
		new_id(0, (ID *)src, 0);
	}
	/* mixed: make copy */
	else if (local && lib) {
		dst= copy_ipo(src);
		dst->id.us= 0;
		
		for (key= G.main->key.first; key; key= key->id.next) {
			if (key->ipo == src) {
				if (key->id.lib == NULL) {
					key->ipo= dst;
					dst->id.us++;
					src->id.us--;
				}
			}
		}
	}
}


/* generic call to make IPO's local */
void make_local_ipo (Ipo *ipo)
{
	/* can't touch lib-linked data */
	if (ipo->id.lib == NULL) 
		return;
		
	/* with only one user, just set local flag */
	if (ipo->id.us == 1) {
		ipo->id.lib= 0;
		ipo->id.flag= LIB_LOCAL;
		new_id(0, (ID *)ipo, 0);
		return;
	}
	
	/* when more than 1 user, can only make local for certain blocktypes */
	switch (ipo->blocktype) {
		case ID_OB:
			make_local_obipo(ipo);
			break;
		case ID_MA:
			make_local_matipo(ipo);
			break;
		case ID_KE:
			make_local_keyipo(ipo);
			break;
	}
}

/* ***************************** Keyframe Column Tools ********************************* */

/* add a BezTriple to a column */
void add_to_cfra_elem(ListBase *lb, BezTriple *bezt)
{
	CfraElem *ce, *cen;
	
	for (ce= lb->first; ce; ce= ce->next) {
		/* double key? */
		if (ce->cfra == bezt->vec[1][0]) {
			if (bezt->f2 & SELECT) ce->sel= bezt->f2;
			return;
		}
		/* should key be inserted before this column? */
		else if (ce->cfra > bezt->vec[1][0]) break;
	}
	
	/* create a new column */
	cen= MEM_callocN(sizeof(CfraElem), "add_to_cfra_elem");	
	if (ce) BLI_insertlinkbefore(lb, ce, cen);
	else BLI_addtail(lb, cen);

	cen->cfra= bezt->vec[1][0];
	cen->sel= bezt->f2;
}

/* make a list of keyframe 'columns' in an IPO block */
void make_cfra_list (Ipo *ipo, ListBase *elems)
{
	IpoCurve *icu;
	BezTriple *bezt;
	int a;
	
	for (icu= ipo->curve.first; icu; icu= icu->next) {
		if (icu->flag & IPO_VISIBLE) {
			/* ... removed old checks for adrcode types from here ...
			 * 	- (was this used for IpoKeys in the past?)
			 */
			
			bezt= icu->bezt;
			if (bezt) {
				for (a=0; a < icu->totvert; a++, bezt++) {
					add_to_cfra_elem(elems, bezt);
				}
			}
		}
	}
}

/* ***************************** Timing Stuff ********************************* */

/* This (evil) function is needed to cope with two legacy Blender rendering features
 * mblur (motion blur that renders 'subframes' and blurs them together), and fields 
 * rendering. Thus, the use of ugly globals from object.c
 */
// BAD... EVIL... JUJU...!!!!
float frame_to_float (int cfra)		/* see also bsystem_time in object.c */
{
	extern float bluroffs;	/* bad stuff borrowed from object.c */
	extern float fieldoffs;
	float ctime;
	
	ctime= (float)cfra;
	ctime+= bluroffs+fieldoffs;
	ctime*= G.scene->r.framelen;
	
	return ctime;
}

/* ***************************** IPO Curve Sanity ********************************* */
/* The functions here are used in various parts of Blender, usually after some editing
 * of keyframe data has occurred. They ensure that keyframe data is properly ordered and
 * that the handles are correctly 
 */

/* This function recalculates the handles of an IPO-Curve 
 * If the BezTriples have been rearranged, sort them first before using this.
 */
void calchandles_ipocurve (IpoCurve *icu)
{
	BezTriple *bezt, *prev, *next;
	int a= icu->totvert;

	/* Error checking:
	 *	- need at least two points
	 *	- need bezier keys
	 *	- only bezier-interpolation has handles (for now)
	 */
	if (ELEM(NULL, icu, icu->bezt) || (a < 2) || ELEM(icu->ipo, IPO_CONST, IPO_LIN)) 
		return;
	
	/* get initial pointers */
	bezt= icu->bezt;
	prev= NULL;
	next= (bezt + 1);
	
	/* loop over all beztriples, adjusting handles */
	while (a--) {
		/* clamp timing of handles to be on either side of beztriple */
		if (bezt->vec[0][0] > bezt->vec[1][0]) bezt->vec[0][0]= bezt->vec[1][0];
		if (bezt->vec[2][0] < bezt->vec[1][0]) bezt->vec[2][0]= bezt->vec[1][0];
		
		/* calculate autohandles */
		if (icu->flag & IPO_AUTO_HORIZ) 
			calchandleNurb(bezt, prev, next, 2);	/* 2==special autohandle && keep extrema horizontal */
		else
			calchandleNurb(bezt, prev, next, 1);	/* 1==special autohandle */
		
		/* for automatic ease in and out */
		if ((bezt->h1==HD_AUTO) && (bezt->h2==HD_AUTO)) {
			/* only do this on first or last beztriple */
			if ((a==0) || (a==icu->totvert-1)) {
				/* set both handles to have same horizontal value as keyframe */
				if (icu->extrap==IPO_HORIZ) {
					bezt->vec[0][1]= bezt->vec[2][1]= bezt->vec[1][1];
				}
			}
		}
		
		/* advance pointers for next iteration */
		prev= bezt;
		if (a == 1) next= NULL;
		else next++;
		bezt++;
	}
}

/* Use when IPO-Curve with handles has changed
 * It treats all BezTriples with the following rules:
 *  - PHASE 1: do types have to be altered?
 * 		-> Auto handles: become aligned when selection status is NOT(000 || 111)
 * 		-> Vector handles: become 'nothing' when (one half selected AND other not)
 *  - PHASE 2: recalculate handles
*/
void testhandles_ipocurve (IpoCurve *icu)
{
	BezTriple *bezt;
	int a;

	/* only beztriples have handles (bpoints don't though) */
	if (ELEM(NULL, icu, icu->bezt))
		return;
	
	/* loop over beztriples */
	for (a=0, bezt=icu->bezt; a < icu->totvert; a++, bezt++) {
		short flag= 0;
		
		/* flag is initialised as selection status
		 * of beztriple control-points (labelled 0,1,2)
		 */
		if (bezt->f1 & SELECT) flag |= (1<<0); // == 1
		if (bezt->f2 & SELECT) flag |= (1<<1); // == 2
		if (bezt->f3 & SELECT) flag |= (1<<2); // == 4
		
		/* one or two handles selected only */
		if (ELEM(flag, 0, 7)==0) {
			/* auto handles become aligned */
			if (bezt->h1==HD_AUTO)
				bezt->h1= HD_ALIGN;
			if(bezt->h2==HD_AUTO)
				bezt->h2= HD_ALIGN;
			
			/* vector handles become 'free' when only one half selected */
			if(bezt->h1==HD_VECT) {
				/* only left half (1 or 2 or 1+2) */
				if (flag < 4) 
					bezt->h1= 0;
			}
			if(bezt->h2==HD_VECT) {
				/* only right half (4 or 2+4) */
				if (flag > 3) 
					bezt->h2= 0;
			}
		}
	}

	/* recalculate handles */
	calchandles_ipocurve(icu);
}

/* This function sorts BezTriples so that they are arranged in chronological order,
 * as tools working on IPO-Curves expect that the BezTriples are in order.
 */
void sort_time_ipocurve(IpoCurve *icu)
{
	short ok= 1;
	
	/* keep adjusting order of beztriples until nothing moves (bubble-sort) */
	while (ok) {
		ok= 0;
		
		/* currently, will only be needed when there are beztriples */
		if (icu->bezt) {
			BezTriple *bezt;
			int a;
			
			/* loop over ALL points to adjust position in array and recalculate handles */
			for (a=0, bezt=icu->bezt; a < icu->totvert; a++, bezt++) {
				/* check if thee's a next beztriple which we could try to swap with current */
				if (a < (icu->totvert-1)) {
					/* swap if one is after the other (and indicate that order has changed) */
					if (bezt->vec[1][0] > (bezt+1)->vec[1][0]) {
						SWAP(BezTriple, *bezt, *(bezt+1));
						ok= 1;
					}
					
					/* if either one of both of the points exceeds crosses over the keyframe time... */
					if ( (bezt->vec[0][0] > bezt->vec[1][0]) && (bezt->vec[2][0] < bezt->vec[1][0]) ) {
						/* swap handles if they have switched sides for some reason */
						SWAP(float, bezt->vec[0][0], bezt->vec[2][0]);
						SWAP(float, bezt->vec[0][1], bezt->vec[2][1]);
					}
					else {
						/* clamp handles */
						if (bezt->vec[0][0] > bezt->vec[1][0]) 
							bezt->vec[0][0]= bezt->vec[1][0];
						if (bezt->vec[2][0] < bezt->vec[1][0]) 
							bezt->vec[2][0]= bezt->vec[1][0];
					}
				}
			}
		}
	}
}

/* This function tests if any BezTriples are out of order, thus requiring a sort */
int test_time_ipocurve (IpoCurve *icu)
{
	int a;
	
	/* currently, only need to test beztriples */
	if (icu->bezt) {
		BezTriple *bezt;
		
		/* loop through all beztriples, stopping when one exceeds the one after it */
		for (a=0, bezt= icu->bezt; a < (icu->totvert - 1); a++, bezt++) {
			if (bezt->vec[1][0] > (bezt+1)->vec[1][0])
				return 1;
		}
	}
	
	/* none need any swapping */
	return 0;
}

/* --------- */

/* The total length of the handles is not allowed to be more
 * than the horizontal distance between (v1-v4).
 * This is to prevent curve loops.
*/
void correct_bezpart (float *v1, float *v2, float *v3, float *v4)
{
	float h1[2], h2[2], len1, len2, len, fac;
	
	/* calculate handle deltas */
	h1[0]= v1[0]-v2[0];
	h1[1]= v1[1]-v2[1];
	h2[0]= v4[0]-v3[0];
	h2[1]= v4[1]-v3[1];
	
	/* calculate distances: 
	 * 	- len	= span of time between keyframes 
	 *	- len1	= length of handle of start key
	 *	- len2 	= length of handle of end key
	 */
	len= v4[0]- v1[0];
	len1= (float)fabs(h1[0]);
	len2= (float)fabs(h2[0]);
	
	/* if the handles have no length, no need to do any corrections */
	if ((len1+len2) == 0.0) 
		return;
		
	/* the two handles cross over each other, so force them
	 * apart using the proportion they overlap 
	 */
	if ((len1+len2) > len) {
		fac= len/(len1+len2);
		
		v2[0]= (v1[0]-fac*h1[0]);
		v2[1]= (v1[1]-fac*h1[1]);
		
		v3[0]= (v4[0]-fac*h2[0]);
		v3[1]= (v4[1]-fac*h2[1]);
	}
}

#if 0 // TODO: enable when we have per-segment interpolation
/* This function sets the interpolation mode for an entire Ipo-Curve. 
 * It is primarily used for patching old files, but is also used in the interface
 * to make sure that all segments of the curve use the same interpolation.
 */
void set_interpolation_ipocurve (IpoCurve *icu, short ipo)
{
	BezTriple *bezt;
	int a;
	
	/* validate arguments */
	if (icu == NULL) return;
	if (ELEM3(ipo, IPO_CONST, IPO_LIN, IPO_BEZ)==0) return;

	/* set interpolation mode for whole curve */
	icu->ipo= ipo;
	
	/* set interpolation mode of all beztriples */
	for (a=0, bezt=icu->bezt; a<icu->totvert; a++, bezt++)
		bezt->ipo= ipo;
}
#endif // TODO: enable when we have per-segment interpolation

/* ***************************** Curve Calculations ********************************* */

/* find root/zero */
int findzero (float x, float q0, float q1, float q2, float q3, float *o)
{
	double c0, c1, c2, c3, a, b, c, p, q, d, t, phi;
	int nr= 0;

	c0= q0 - x;
	c1= 3 * (q1 - q0);
	c2= 3 * (q0 - 2*q1 + q2);
	c3= q3 - q0 + 3 * (q1 - q2);
	
	if (c3 != 0.0) {
		a= c2/c3;
		b= c1/c3;
		c= c0/c3;
		a= a/3;
		
		p= b/3 - a*a;
		q= (2*a*a*a - a*b + c) / 2;
		d= q*q + p*p*p;
		
		if (d > 0.0) {
			t= sqrt(d);
			o[0]= (float)(Sqrt3d(-q+t) + Sqrt3d(-q-t) - a);
			
			if ((o[0] >= SMALL) && (o[0] <= 1.000001)) return 1;
			else return 0;
		}
		else if (d == 0.0) {
			t= Sqrt3d(-q);
			o[0]= (float)(2*t - a);
			
			if ((o[0] >= SMALL) && (o[0] <= 1.000001)) nr++;
			o[nr]= (float)(-t-a);
			
			if ((o[nr] >= SMALL) && (o[nr] <= 1.000001)) return nr+1;
			else return nr;
		}
		else {
			phi= acos(-q / sqrt(-(p*p*p)));
			t= sqrt(-p);
			p= cos(phi/3);
			q= sqrt(3 - 3*p*p);
			o[0]= (float)(2*t*p - a);
			
			if ((o[0] >= SMALL) && (o[0] <= 1.000001)) nr++;
			o[nr]= (float)(-t * (p + q) - a);
			
			if ((o[nr] >= SMALL) && (o[nr] <= 1.000001)) nr++;
			o[nr]= (float)(-t * (p - q) - a);
			
			if ((o[nr] >= SMALL) && (o[nr] <= 1.000001)) return nr+1;
			else return nr;
		}
	}
	else {
		a=c2;
		b=c1;
		c=c0;
		
		if (a != 0.0) {
			// discriminant
			p= b*b - 4*a*c;
			
			if (p > 0) {
				p= sqrt(p);
				o[0]= (float)((-b-p) / (2 * a));
				
				if ((o[0] >= SMALL) && (o[0] <= 1.000001)) nr++;
				o[nr]= (float)((-b+p)/(2*a));
				
				if ((o[nr] >= SMALL) && (o[nr] <= 1.000001)) return nr+1;
				else return nr;
			}
			else if (p == 0) {
				o[0]= (float)(-b / (2 * a));
				if ((o[0] >= SMALL) && (o[0] <= 1.000001)) return 1;
				else return 0;
			}
		}
		else if (b != 0.0) {
			o[0]= (float)(-c/b);
			
			if ((o[0] >= SMALL) && (o[0] <= 1.000001)) return 1;
			else return 0;
		}
		else if (c == 0.0) {
			o[0]= 0.0;
			return 1;
		}
		
		return 0;	
	}
}

void berekeny (float f1, float f2, float f3, float f4, float *o, int b)
{
	float t, c0, c1, c2, c3;
	int a;

	c0= f1;
	c1= 3.0f * (f2 - f1);
	c2= 3.0f * (f1 - 2.0f*f2 + f3);
	c3= f4 - f1 + 3.0f * (f2 - f3);
	
	for (a=0; a < b; a++) {
		t= o[a];
		o[a]= c0 + t*c1 + t*t*c2 + t*t*t*c3;
	}
}

void berekenx (float *f, float *o, int b)
{
	float t, c0, c1, c2, c3;
	int a;

	c0= f[0];
	c1= 3 * (f[3] - f[0]);
	c2= 3 * (f[0] - 2*f[3] + f[6]);
	c3= f[9] - f[0] + 3 * (f[3] - f[6]);
	
	for (a=0; a < b; a++) {
		t= o[a];
		o[a]= c0 + t*c1 + t*t*c2 + t*t*t*c3;
	}
}

/* ***************************** IPO - Calculations ********************************* */

/* ---------------------- Curve Evaluation --------------------------- */

/* helper function for evaluating drivers: 
 *	- we need the local transform = current transform - (parent transform + bone transform)
 *	- (local transform is on action channel level)
 */
static void posechannel_get_local_transform (bPoseChannel *pchan, float loc[], float eul[], float size[])
{
	float parmat[4][4], offs_bone[4][4], imat[4][4];
	float diff_mat[4][4];
	
	/* get first the parent + bone transform in parmat */
	if (pchan->parent) {
		/* bone transform itself */
		Mat4CpyMat3(offs_bone, pchan->bone->bone_mat);
		
		/* The bone's root offset (is in the parent's coordinate system) */
		VECCOPY(offs_bone[3], pchan->bone->head);
		
		/* Get the length translation of parent (length along y axis) */
		offs_bone[3][1]+= pchan->parent->bone->length;
		
		Mat4MulSerie(parmat, pchan->parent->pose_mat, offs_bone, NULL, NULL, NULL, NULL, NULL, NULL);
		
		/* invert it */
		Mat4Invert(imat, parmat);
	}
	else {
		Mat4CpyMat3(offs_bone, pchan->bone->bone_mat);
		VECCOPY(offs_bone[3], pchan->bone->head);
		
		/* invert it */
		Mat4Invert(imat, offs_bone);
	}
	
	/* difference: current transform - (parent transform + bone transform)  */
	Mat4MulMat4(diff_mat, pchan->pose_mat, imat);

	/* extract relevant components */
	if (loc)
		VECCOPY(loc, diff_mat[3]);
	if (eul)
		Mat4ToEul(diff_mat, eul);
	if (size)
		Mat4ToSize(diff_mat, size);
}

/* evaluate an IPO-driver to get a 'time' value to use instead of "ipotime"
 *	- "ipotime" is the frame at which IPO-curve is being evaluated
 * 	- has to return a float value 
 */
static float eval_driver (IpoDriver *driver, float ipotime)
{
#ifndef DISABLE_PYTHON
	/* currently, drivers are either PyDrivers (evaluating a PyExpression, or Object/Pose-Channel transforms) */
	if (driver->type == IPO_DRIVER_TYPE_PYTHON) {
		/* check for empty or invalid expression */
		if ( (driver->name[0] == '\0') ||
			 (driver->flag & IPO_DRIVER_FLAG_INVALID) )
		{
			return 0.0f;
		}
		
		/* this evaluates the expression using Python,and returns its result:
		 * 	- on errors it reports, then returns 0.0f
		 */
		return BPY_pydriver_eval(driver);
	}
	else
#endif /* DISABLE_PYTHON */
	{

		Object *ob= driver->ob;
		
		/* must have an object to evaluate */
		if (ob == NULL) 
			return 0.0f;
			
		/* if a proxy, use the proxy source*/
		if (ob->proxy_from)
			ob= ob->proxy_from;
		
		/* use given object as driver */
		if (driver->blocktype == ID_OB) {
			/* depsgraph failure: ob ipos are calculated in where_is_object, this might get called too late */
			if ((ob->ipo) && (ob->ctime != ipotime)) {
				/* calculate the value of relevant channel on the Object, but do not write the value
				 * calculated on to the Object but onto "ipotime" instead
				 */
				calc_ipo_spec(ob->ipo, driver->adrcode, &ipotime);
				return ipotime;
			}
			
			/* return the value of the relevant channel */
			switch (driver->adrcode) {
			case OB_LOC_X:
				return ob->loc[0];
			case OB_LOC_Y:
				return ob->loc[1];
			case OB_LOC_Z:
				return ob->loc[2];
			case OB_ROT_X:	/* hack: euler rotations are divided by 10 deg to fit on same axes as other channels */
				return (float)( ob->rot[0]/(M_PI_2/9.0) );
			case OB_ROT_Y:	/* hack: euler rotations are divided by 10 deg to fit on same axes as other channels */
				return (float)( ob->rot[1]/(M_PI_2/9.0) );
			case OB_ROT_Z:	/* hack: euler rotations are divided by 10 deg to fit on same axes as other channels */
				return (float)( ob->rot[2]/(M_PI_2/9.0) );
			case OB_SIZE_X:
				return ob->size[0];
			case OB_SIZE_Y:
				return ob->size[1];
			case OB_SIZE_Z:
				return ob->size[2];
			}
		}
		
		/* use given pose-channel as driver */
		else {	/* ID_AR */
			bPoseChannel *pchan= get_pose_channel(ob->pose, driver->name);
			
			/* must have at least 1 bone to use */
			if (pchan && pchan->bone) {
				/* rotation difference is not a simple driver (i.e. value drives value), but the angle between 2 bones is driving stuff... 
				 *	- the name of the second pchan is also stored in driver->name, but packed after the other one by DRIVER_NAME_OFFS chars
				 */
				if (driver->adrcode == OB_ROT_DIFF) {
					bPoseChannel *pchan2= get_pose_channel(ob->pose, driver->name+DRIVER_NAME_OFFS);
					
					if (pchan2 && pchan2->bone) {
						float q1[4], q2[4], quat[4], angle;
						
						Mat4ToQuat(pchan->pose_mat, q1);
						Mat4ToQuat(pchan2->pose_mat, q2);
						
						QuatInv(q1);
						QuatMul(quat, q1, q2);
						angle = 2.0f * (saacos(quat[0]));
						angle= ABS(angle);
						
						return (angle > M_PI) ? (float)((2.0f * M_PI) - angle) : (float)(angle);
					}
				}
				
				/* standard driver */
				else {
					float loc[3], eul[3], size[3];
					
					/* retrieve local transforms to return 
					 *	- we use eulers here NOT quats, so that Objects can be driven by bones easily
					 *	  also, this way is more understandable for users
					 */
					posechannel_get_local_transform(pchan, loc, eul, size);
					
					switch (driver->adrcode) {
					case OB_LOC_X:
						return loc[0];
					case OB_LOC_Y:
						return loc[1];
					case OB_LOC_Z:
						return loc[2];
					case OB_ROT_X: /* hack: euler rotations are divided by 10 deg to fit on same axes as other channels */
						return (float)( eul[0]/(M_PI_2/9.0) );
					case OB_ROT_Y: /* hack: euler rotations are divided by 10 deg to fit on same axes as other channels */
						return (float)( eul[1]/(M_PI_2/9.0) );
					case OB_ROT_Z: /* hack: euler rotations are divided by 10 deg to fit on same axes as other channels */
						return (float)( eul[2]/(M_PI_2/9.0) );
					case OB_SIZE_X:
						return size[0];
					case OB_SIZE_Y:
						return size[1];
					case OB_SIZE_Z:
						return size[2];
					}
				}
			}
		}
	}	
	
	/* return 0.0f, as couldn't find relevant data to use */
	return 0.0f;
}

/* evaluate and return the value of the given IPO-curve at the specified frame ("evaltime") */
float eval_icu(IpoCurve *icu, float evaltime) 
{
	float cvalue = 0.0f;
	
	/* if there is a driver, evaluate it to find value to use as "evaltime" 
	 *	- this value will also be returned as the value of the 'curve', if there are no keyframes
	 */
	if (icu->driver) {
		/* ipotime now serves as input for the curve */
		evaltime= cvalue= eval_driver(icu->driver, evaltime);
	}
	
	/* there are keyframes (in the form of BezTriples) which can be interpolated between */
	if (icu->bezt) {
		/* get pointers */
		BezTriple *bezt, *prevbezt, *lastbezt;
		float v1[2], v2[2], v3[2], v4[2], opl[32], dx, fac;
		float cycdx, cycdy, ofs, cycyofs= 0.0;
		int a, b;
		
		/* get pointers */
		a= icu->totvert-1;
		prevbezt= icu->bezt;
		bezt= prevbezt+1;
		lastbezt= prevbezt + a;
		
		/* extrapolation mode is 'cyclic' - find relative place within a cycle */
		if (icu->extrap & IPO_CYCL) {
			/* ofs is start frame of cycle */
			ofs= prevbezt->vec[1][0];
			
			/* calculate period and amplitude (total height) of a cycle */
			cycdx= lastbezt->vec[1][0] - prevbezt->vec[1][0];
			cycdy= lastbezt->vec[1][1] - prevbezt->vec[1][1];
			
			/* cycle occurs over some period of time (cycdx should be positive all the time) */
			if (cycdx) {
				/* check if 'cyclic extrapolation', and thus calculate y-offset for this cycle
				 *	- IPO_CYCLX = (IPO_CYCL + IPO_DIR)
				 */
				if (icu->extrap & IPO_DIR) {
					cycyofs = (float)floor((evaltime - ofs) / cycdx);
					cycyofs *= cycdy;
				}
				
				/* calculate where in the cycle we are (overwrite evaltime to reflect this) */
				evaltime= (float)(fmod(evaltime-ofs, cycdx) + ofs);
				if (evaltime < ofs) evaltime += cycdx;
			}
		}
		
		/* evaluation time at or past endpoints? */
		// TODO: for per-bezt interpolation, replace all icu->ipo with (bezt)->ipo
		if (prevbezt->vec[1][0] >= evaltime) {
			/* before or on first keyframe */
			if ((icu->extrap & IPO_DIR) && (icu->ipo != IPO_CONST)) {
				/* linear or bezier interpolation */
				if (icu->ipo==IPO_LIN) {
					/* Use the next center point instead of our own handle for
					 * linear interpolated extrapolate 
					 */
					if (icu->totvert == 1) 
						cvalue= prevbezt->vec[1][1];
					else {
						bezt = prevbezt+1;
						dx= prevbezt->vec[1][0] - evaltime;
						fac= bezt->vec[1][0] - prevbezt->vec[1][0];
						
						/* prevent division by zero */
						if (fac) {
							fac= (bezt->vec[1][1] - prevbezt->vec[1][1]) / fac;
							cvalue= prevbezt->vec[1][1] - (fac * dx);
						}
						else 
							cvalue= prevbezt->vec[1][1];
					}
				} 
				else {
					/* Use the first handle (earlier) of first BezTriple to calculate the
					 * gradient and thus the value of the curve at evaltime
					 */
					dx= prevbezt->vec[1][0] - evaltime;
					fac= prevbezt->vec[1][0] - prevbezt->vec[0][0];
					
					/* prevent division by zero */
					if (fac) {
						fac= (prevbezt->vec[1][1] - prevbezt->vec[0][1]) / fac;
						cvalue= prevbezt->vec[1][1] - (fac * dx);
					}
					else 
						cvalue= prevbezt->vec[1][1];
				}
			}
			else {
				/* constant (IPO_HORIZ) extrapolation or constant interpolation, 
				 * so just extend first keyframe's value 
				 */
				cvalue= prevbezt->vec[1][1];
			}
		}
		else if (lastbezt->vec[1][0] <= evaltime) {
			/* after or on last keyframe */
			if( (icu->extrap & IPO_DIR) && (icu->ipo != IPO_CONST)) {
				/* linear or bezier interpolation */
				if (icu->ipo==IPO_LIN) {
					/* Use the next center point instead of our own handle for
					 * linear interpolated extrapolate 
					 */
					if (icu->totvert == 1) 
						cvalue= lastbezt->vec[1][1];
					else {
						prevbezt = lastbezt - 1;
						dx= evaltime - lastbezt->vec[1][0];
						fac= lastbezt->vec[1][0] - prevbezt->vec[1][0];
						
						/* prevent division by zero */
						if (fac) {
							fac= (lastbezt->vec[1][1] - prevbezt->vec[1][1]) / fac;
							cvalue= lastbezt->vec[1][1] + (fac * dx);
						}
						else 
							cvalue= lastbezt->vec[1][1];
					}
				} 
				else {
					/* Use the gradient of the second handle (later) of last BezTriple to calculate the
					 * gradient and thus the value of the curve at evaltime
					 */
					dx= evaltime - lastbezt->vec[1][0];
					fac= lastbezt->vec[2][0] - lastbezt->vec[1][0];
					
					/* prevent division by zero */
					if (fac) {
						fac= (lastbezt->vec[2][1] - lastbezt->vec[1][1]) / fac;
						cvalue= lastbezt->vec[1][1] + (fac * dx);
					}
					else 
						cvalue= lastbezt->vec[1][1];
				}
			}
			else {
				/* constant (IPO_HORIZ) extrapolation or constant interpolation, 
				 * so just extend last keyframe's value 
				 */
				cvalue= lastbezt->vec[1][1];
			}
		}
		else {
			/* evaltime occurs somewhere in the middle of the curve */
			// TODO: chould be optimised by using a binary search instead???
			for (a=0; prevbezt && bezt && (a < icu->totvert-1); a++, prevbezt=bezt, bezt++) {  
				/* evaltime occurs within the interval defined by these two keyframes */
				if ((prevbezt->vec[1][0] <= evaltime) && (bezt->vec[1][0] >= evaltime)) {
					/* value depends on interpolation mode */
					if (icu->ipo == IPO_CONST) {
						/* constant (evaltime not relevant, so no interpolation needed) */
						cvalue= prevbezt->vec[1][1];
					}
					else if (icu->ipo == IPO_LIN) {
						/* linear - interpolate between values of the two keyframes */
						fac= bezt->vec[1][0] - prevbezt->vec[1][0];
						
						/* prevent division by zero */
						if (fac) {
							fac= (evaltime - prevbezt->vec[1][0]) / fac;
							cvalue= prevbezt->vec[1][1] + (fac * (bezt->vec[1][1] - prevbezt->vec[1][1]));
						}
						else
							cvalue= prevbezt->vec[1][1];
					}
					else {
						/* bezier interpolation */
							/* v1,v2 are the first keyframe and its 2nd handle */
						v1[0]= prevbezt->vec[1][0];
						v1[1]= prevbezt->vec[1][1];
						v2[0]= prevbezt->vec[2][0];
						v2[1]= prevbezt->vec[2][1];
							/* v3,v4 are the last keyframe's 1st handle + the last keyframe */
						v3[0]= bezt->vec[0][0];
						v3[1]= bezt->vec[0][1];
						v4[0]= bezt->vec[1][0];
						v4[1]= bezt->vec[1][1];
						
						/* adjust handles so that they don't overlap (forming a loop) */
						correct_bezpart(v1, v2, v3, v4);
						
						/* try to get a value for this position - if failure, try another set of points */
						b= findzero(evaltime, v1[0], v2[0], v3[0], v4[0], opl);
						if (b) {
							berekeny(v1[1], v2[1], v3[1], v4[1], opl, 1);
							cvalue= opl[0];
							break;
						}
					}
				}
			}
		}
		
		/* apply y-offset (for 'cyclic extrapolation') to calculated value */
		cvalue+= cycyofs;
	}
	
	/* clamp evaluated value to lie within allowable value range for this channel */
	if (icu->ymin < icu->ymax) {
		CLAMP(cvalue, icu->ymin, icu->ymax);
	}
	
	/* return evaluated value */
	return cvalue;
}

/* ------------------- IPO-Block/Curve Calculation - General API ----------------------- */

/* calculate the value of the given IPO-curve at the current frame, and set its curval */
void calc_icu (IpoCurve *icu, float ctime)
{
	/* calculate and set curval (evaluates driver too) */
	icu->curval= eval_icu(icu, ctime);
}

/* calculate for the current frame, all IPO-curves in IPO-block that can be evaluated 
 *	- icu->curval is set for all IPO-curves which are evaluated!
 */
void calc_ipo (Ipo *ipo, float ctime)
{
	IpoCurve *icu;
	
	/* if there is no IPO block to evaluate, or whole block is "muted" */
	if (ipo == NULL) return;
	if (ipo->muteipo) return;
	
	/* loop over all curves */
	for (icu= ipo->curve.first; icu; icu= icu->next) {
		/* only evaluated curve if allowed to:
		 * 	- Muted channels should not be evaluated as they shouldn't have any effect 
		 *		--> user explictly turned them off!
		 *	- Drivers should be evaluated at all updates
		 *		--> TODO Note: drivers should be separated from standard channels
		 *	- IPO_LOCK is not set, as it is set by some internal mechanisms to prevent
		 *		IPO-curve from overwriting data (currently only used for IPO-Record). 
		 */
		if ((icu->driver) || (icu->flag & IPO_LOCK)==0) { 
			if ((icu->flag & IPO_MUTE)==0)
				calc_icu(icu, ctime);
		}
	}
}

/* ------------------- IPO-Block/Curve Calculation - Special Hacks ----------------------- */

/* Calculate and return the value of the 'Time' Ipo-Curve from an Object,
 * OR return the current time if not found
 * 	- used in object.c -> bsystem_time() 
 */
float calc_ipo_time (Ipo *ipo, float ctime)
{
	/* only Time IPO from Object IPO-blocks are relevant */
	if ((ipo) && (ipo->blocktype == ID_OB)) {
		IpoCurve *icu= find_ipocurve(ipo, OB_TIME);
		
		/* only calculate (and set icu->curval) for time curve */
		if (icu) {
			calc_icu(icu, ctime);
			return (10.0f * icu->curval);
		}
	}
	
	/* no appropriate time-curve found */
	return ctime;
}

/* Evaluate the specified channel in the given IPO block on the specified frame (ctime),
 * writing the value into that channel's icu->curval, but ALSO dumping it in ctime.
 * 	- Returns success and modifies ctime! 
 */
short calc_ipo_spec (Ipo *ipo, int adrcode, float *ctime)
{
	IpoCurve *icu= find_ipocurve(ipo, adrcode);
	
	/* only evaluate if found */
	if (icu) {
		/* only calculate if allowed to (not locked and not muted) 
		 *	- drivers not taken into account, because this may be called when calculating a driver
		 */
		if ((icu->flag & (IPO_LOCK|IPO_MUTE))==0) 
			calc_icu(icu, *ctime);
		
		/* value resulting from calculations is written into ctime! */
		*ctime= icu->curval;
		return 1;
	}
	
	/* couldn't evaluate */
	return 0;
}

/* ***************************** IPO - DataAPI ********************************* */

/* --------------------- Flush/Execute IPO Values ----------------------------- */

/* Flush IpoCurve->curvals to the data they affect (defined by ID)
 *	 - not for Actions or Constraints!  (those have their own special handling)
 */
void execute_ipo (ID *id, Ipo *ipo)
{
	IpoCurve *icu;
	void *poin;
	int type;
	
	/* don't do anything without an IPO block */
	if (ipo == NULL) 
		return;
	
	/* loop over IPO Curves, getting pointer to var to affect, and write into that pointer */
	for (icu= ipo->curve.first; icu; icu= icu->next) {
		poin= get_ipo_poin(id, icu, &type);
		if (poin) write_ipo_poin(poin, type, icu->curval);
	}
}

/* Flush Action-Channel IPO data to Pose Channel */
void execute_action_ipo (bActionChannel *achan, bPoseChannel *pchan)
{
	/* only do this if there's an Action Channel and Pose Channel to use */
	if (achan && achan->ipo && pchan) {
		IpoCurve *icu;
		
		/* loop over IPO-curves, getting a pointer to pchan var to write to
		 *	- assume for now that only 'float' channels will ever get written into
		 */
		for (icu= achan->ipo->curve.first; icu; icu= icu->next) {
			void *poin= get_pchan_ipo_poin(pchan, icu->adrcode);
			if (poin) write_ipo_poin(poin, IPO_FLOAT, icu->curval);
		}
	}
}


/* --------------------- Force Calculation + Flush IPO Values ----------------------------- */

/* Calculate values for given IPO block, then flush to all of block's users
 *	 - for general usage 
 */
void do_ipo (Ipo *ipo)
{
	if (ipo) {
		float ctime= frame_to_float(G.scene->r.cfra);
		
		/* calculate values, then flush to all users of this IPO block */
		calc_ipo(ipo, ctime);
		do_ipo_nocalc(ipo);
	}
}

/* Calculate values for given Material's IPO block, then flush to given Material only */
void do_mat_ipo (Material *ma)
{
	float ctime;
	
	if (ELEM(NULL, ma, ma->ipo)) 
		return;
	
	ctime= frame_to_float(G.scene->r.cfra);
	/* if(ob->ipoflag & OB_OFFS_OB) ctime-= ob->sf; */
	
	/* calculate values for current time, then flush values to given material only */
	calc_ipo(ma->ipo, ctime);
	execute_ipo((ID *)ma, ma->ipo);
}

/* Calculate values for given Object's IPO block, then flush to given Object only
 *	- there's also some funky stuff that looks like it's for scene layers
 */
void do_ob_ipo (Object *ob)
{
	float ctime;
	unsigned int lay;
	
	if (ob->ipo == NULL) 
		return;
	
	/* do not set ob->ctime here: for example when parent in invisible layer */
	ctime= bsystem_time(ob, (float) G.scene->r.cfra, 0.0);
	
	/* calculate values of */
	calc_ipo(ob->ipo, ctime);
	
	/* Patch: remember localview */
	lay= ob->lay & 0xFF000000;
	
	/* flush IPO values to this object only */
	execute_ipo((ID *)ob, ob->ipo);
	
	/* hack: for layer animation??? - is this what this is? (Aligorith, 28Sep2008) */
	ob->lay |= lay;
	if ((ob->id.name[2]=='S') && (ob->id.name[3]=='C') && (ob->id.name[4]=='E')) {
		if (strcmp(G.scene->id.name+2, ob->id.name+6)==0) {
			G.scene->lay= ob->lay;
			copy_view3d_lock(0);
			/* no redraw here! creates too many calls */
		}
	}
}

/* Only execute those IPO-Curves with drivers, on the current frame, for the given Object
 * 	- TODO: Drivers should really be made separate from standard anim channels
 */
void do_ob_ipodrivers (Object *ob, Ipo *ipo, float ctime)
{
	IpoCurve *icu;
	void *poin;
	int type;
	
	for (icu= ipo->curve.first; icu; icu= icu->next) {
		if (icu->driver) {
			icu->curval= eval_icu(icu, ctime);
			
			poin= get_ipo_poin((ID *)ob, icu, &type);
			if (poin) write_ipo_poin(poin, type, icu->curval);
		}
	}
}

/* Special variation to calculate IPO values for Sequence + perform other stuff */
void do_seq_ipo (Sequence *seq, int cfra)
{
	float ctime, div;
	
	/* seq_ipo has an exception: calc both fields immediately */
	if (seq->ipo) {
		if ((seq->flag & SEQ_IPO_FRAME_LOCKED) != 0) {
			ctime = frame_to_float(cfra);
			div = 1.0;
		} 
		else {
			ctime= frame_to_float(cfra - seq->startdisp);
			div= (seq->enddisp - seq->startdisp) / 100.0f;
			if (div == 0.0) return;
		}
		
		/* 2nd field */
		calc_ipo(seq->ipo, (ctime+0.5f)/div);
		execute_ipo((ID *)seq, seq->ipo);
		seq->facf1= seq->facf0;
		
		/* 1st field */
		calc_ipo(seq->ipo, ctime/div);
		execute_ipo((ID *)seq, seq->ipo);
	}
	else 
		seq->facf1= seq->facf0= 1.0f;
}

/* --------- */


/* exception: it does calc for objects...
 * now find out why this routine was used anyway!
 */
void do_ipo_nocalc (Ipo *ipo)
{
	Object *ob;
	Material *ma;
	Tex *tex;
	World *wo;
	Lamp *la;
	Camera *ca;
	bSound *snd;
	
	if (ipo == NULL) 
		return;
	
	/* only flush IPO values (without calculating first/again) on 
	 * to the datablocks that use the given IPO block 
	 */
	switch (ipo->blocktype) {
	case ID_OB:
		for (ob= G.main->object.first; ob; ob= ob->id.next) {
			if (ob->ipo == ipo) do_ob_ipo(ob);
		}
		break;
	case ID_MA:
		for (ma= G.main->mat.first; ma; ma= ma->id.next) {
			if (ma->ipo == ipo) execute_ipo((ID *)ma, ipo);
		}
		break;
	case ID_TE:
		for (tex= G.main->tex.first; tex; tex= tex->id.next) {
			if (tex->ipo == ipo) execute_ipo((ID *)tex, ipo);
		}
		break;
	case ID_WO:
		for (wo= G.main->world.first; wo; wo= wo->id.next) {
			if (wo->ipo == ipo) execute_ipo((ID *)wo, ipo);
		}
		break;
	case ID_LA:
		for (la= G.main->lamp.first; la; la= la->id.next) {
			if (la->ipo == ipo) execute_ipo((ID *)la, ipo);
		}
		break;
	case ID_CA:
		for (ca= G.main->camera.first; ca; ca= ca->id.next) {
			if (ca->ipo == ipo) execute_ipo((ID *)ca, ipo);
		}
		break;
	case ID_SO:
		for (snd= G.main->sound.first; snd; snd= snd->id.next) {
			if (snd->ipo == ipo) execute_ipo((ID *)snd, ipo);
		}
		break;
	}
}

/* Executes IPO's for whole database on frame change, in a specified order,
 * with datablocks being calculated in alphabetical order
 * 	- called on scene_update_for_newframe() only 
 */
void do_all_data_ipos ()
{
	Material *ma;
	Tex *tex;
	World *wo;
	Ipo *ipo;
	Lamp *la;
	Key *key;
	Camera *ca;
	bSound *snd;
	Sequence *seq;
	Editing *ed;
	Base *base;
	float ctime;

	ctime= frame_to_float(G.scene->r.cfra);
	
	/* this exception cannot be depgraphed yet... what todo with objects in other layers?... */
	for (base= G.scene->base.first; base; base= base->next) {
		Object *ob= base->object;
		
		/* only update layer when an ipo */
		if (has_ipo_code(ob->ipo, OB_LAY)) {
			do_ob_ipo(ob);
			base->lay= ob->lay;
		}
	}
	
	/* layers for the set...*/
	if (G.scene->set) {
		for (base= G.scene->set->base.first; base; base= base->next) {
			Object *ob= base->object;
			
			if (has_ipo_code(ob->ipo, OB_LAY)) {
				do_ob_ipo(ob);
				base->lay= ob->lay;
			}
		}
	}
	
	/* Calculate all IPO blocks in use, execept those for Objects */
	for (ipo= G.main->ipo.first; ipo; ipo= ipo->id.next) {
		if ((ipo->id.us) && (ipo->blocktype != ID_OB)) {
			calc_ipo(ipo, ctime);
		}
	}

	/* Texture Blocks */
	for (tex= G.main->tex.first; tex; tex= tex->id.next) {
		if (tex->ipo) execute_ipo((ID *)tex, tex->ipo);
	}
	
	/* Material Blocks */
	for (ma= G.main->mat.first; ma; ma= ma->id.next) {
		if (ma->ipo) execute_ipo((ID *)ma, ma->ipo);
	}
	
	/* World Blocks */
	for (wo= G.main->world.first; wo; wo= wo->id.next) {
		if (wo->ipo) execute_ipo((ID *)wo, wo->ipo);
	}
	
	/* ShapeKey Blocks */
	for (key= G.main->key.first; key; key= key->id.next) {
		if (key->ipo) execute_ipo((ID *)key, key->ipo);
	}
	
	/* Lamp Blocks */
	for (la= G.main->lamp.first; la; la= la->id.next) {
		if (la->ipo) execute_ipo((ID *)la, la->ipo);
	}
	
	/* Camera Blocks */
	for (ca= G.main->camera.first; ca; ca= ca->id.next) {
		if (ca->ipo) execute_ipo((ID *)ca, ca->ipo);
	}
	
	/* Sound Blocks (Old + Unused) */
	for (snd= G.main->sound.first; snd; snd= snd->id.next) {
		if (snd->ipo) execute_ipo((ID *)snd, snd->ipo);
	}

	/* Sequencer: process FAC Ipos used as volume envelopes */
	ed= G.scene->ed;
	if (ed) {
		for (seq= ed->seqbasep->first; seq; seq= seq->next) {
			if ( ((seq->type == SEQ_RAM_SOUND) || (seq->type == SEQ_HD_SOUND)) &&
				 (seq->startdisp <= G.scene->r.cfra+2) && 
			     (seq->enddisp>G.scene->r.cfra) &&
				 (seq->ipo) ) 
			{
					do_seq_ipo(seq, G.scene->r.cfra);
			}
		}
	}
}


/* --------------------- Assorted ----------------------------- */ 

/* clear delta-transforms on all Objects which use the given IPO block */
void clear_delta_obipo(Ipo *ipo)
{
	Object *ob;
	
	/* only search if there's an IPO */
	if (ipo == NULL) 
		return;
	
	/* search through all objects in database */
	for (ob= G.main->object.first; ob; ob= ob->id.next) {
		/* can only update if not a library */
		if (ob->id.lib == NULL) {
			if (ob->ipo == ipo)  {
				memset(&ob->dloc, 0, 12);
				memset(&ob->drot, 0, 12);
				memset(&ob->dsize, 0, 12);
			}
		}
	}
}

/* ***************************** IPO - DataAPI ********************************* */

// !!!!!!!!!!!!!!!!!!!!!!!!!!!! FIXME - BAD CRUFT WARNING !!!!!!!!!!!!!!!!!!!!!!!

/* These functions here should be replaced eventually by the Data API, as this is 
 * inflexible duplication...
 */

/* --------------------- Get Pointer API ----------------------------- */ 

/* get pointer to pose-channel's channel, but set appropriate flags first */
void *get_pchan_ipo_poin (bPoseChannel *pchan, int adrcode)
{
	void *poin= NULL;
	
	switch (adrcode) {
		case AC_QUAT_W:
			poin= &(pchan->quat[0]); 
			pchan->flag |= POSE_ROT;
			break;
		case AC_QUAT_X:
			poin= &(pchan->quat[1]); 
			pchan->flag |= POSE_ROT;
			break;
		case AC_QUAT_Y:
			poin= &(pchan->quat[2]); 
			pchan->flag |= POSE_ROT;
			break;
		case AC_QUAT_Z:
			poin= &(pchan->quat[3]); 
			pchan->flag |= POSE_ROT;
			break;
			
		case AC_LOC_X:
			poin= &(pchan->loc[0]); 
			pchan->flag |= POSE_LOC;
			break;
		case AC_LOC_Y:
			poin= &(pchan->loc[1]); 
			pchan->flag |= POSE_LOC;
			break;
		case AC_LOC_Z:
			poin= &(pchan->loc[2]); 
			pchan->flag |= POSE_LOC;
			break;
		
		case AC_SIZE_X:
			poin= &(pchan->size[0]); 
			pchan->flag |= POSE_SIZE;
			break;
		case AC_SIZE_Y:
			poin= &(pchan->size[1]); 
			pchan->flag |= POSE_SIZE;
			break;
		case AC_SIZE_Z:
			poin= &(pchan->size[2]); 
			pchan->flag |= POSE_SIZE;
			break;
	}
	
	/* return pointer */
	return poin;
}

/* get texture channel */
static void *give_tex_poin (Tex *tex, int adrcode, int *type )
{
	void *poin= NULL;

	switch (adrcode) {
	case TE_NSIZE:
		poin= &(tex->noisesize); break;
	case TE_TURB:
		poin= &(tex->turbul); break;
	case TE_NDEPTH:
		poin= &(tex->noisedepth); *type= IPO_SHORT; break;
	case TE_NTYPE:
		poin= &(tex->noisetype); *type= IPO_SHORT; break;
	case TE_VNW1:
		poin= &(tex->vn_w1); break;
	case TE_VNW2:
		poin= &(tex->vn_w2); break;
	case TE_VNW3:
		poin= &(tex->vn_w3); break;
	case TE_VNW4:
		poin= &(tex->vn_w4); break;
	case TE_VNMEXP:
		poin= &(tex->vn_mexp); break;
	case TE_ISCA:
		poin= &(tex->ns_outscale); break;
	case TE_DISTA:
		poin= &(tex->dist_amount); break;
	case TE_VN_COLT:
		poin= &(tex->vn_coltype); *type= IPO_SHORT; break;
	case TE_VN_DISTM:
		poin= &(tex->vn_distm); *type= IPO_SHORT; break;
	case TE_MG_TYP:
		poin= &(tex->stype); *type= IPO_SHORT; break;
	case TE_MGH:
		poin= &(tex->mg_H); break;
	case TE_MG_LAC:
		poin= &(tex->mg_lacunarity); break;
	case TE_MG_OCT:
		poin= &(tex->mg_octaves); break;
	case TE_MG_OFF:
		poin= &(tex->mg_offset); break;
	case TE_MG_GAIN:
		poin= &(tex->mg_gain); break;
	case TE_N_BAS1:
		poin= &(tex->noisebasis); *type= IPO_SHORT; break;
	case TE_N_BAS2:
		poin= &(tex->noisebasis2); *type= IPO_SHORT; break;
	case TE_COL_R:
		poin= &(tex->rfac); break;
	case TE_COL_G:
		poin= &(tex->gfac); break;
	case TE_COL_B:
		poin= &(tex->bfac); break;
	case TE_BRIGHT:
		poin= &(tex->bright); break;
	case TE_CONTRA:
		poin= &(tex->contrast); break;
	}
	
	/* return pointer */
	return poin;
}

/* get texture-slot/mapping channel */
void *give_mtex_poin (MTex *mtex, int adrcode)
{
	void *poin= NULL;
	
	switch (adrcode) {
	case MAP_OFS_X:
		poin= &(mtex->ofs[0]); break;
	case MAP_OFS_Y:
		poin= &(mtex->ofs[1]); break;
	case MAP_OFS_Z:
		poin= &(mtex->ofs[2]); break;
	case MAP_SIZE_X:
		poin= &(mtex->size[0]); break;
	case MAP_SIZE_Y:
		poin= &(mtex->size[1]); break;
	case MAP_SIZE_Z:
		poin= &(mtex->size[2]); break;
	case MAP_R:
		poin= &(mtex->r); break;
	case MAP_G:
		poin= &(mtex->g); break;
	case MAP_B:
		poin= &(mtex->b); break;
	case MAP_DVAR:
		poin= &(mtex->def_var); break;
	case MAP_COLF:
		poin= &(mtex->colfac); break;
	case MAP_NORF:
		poin= &(mtex->norfac); break;
	case MAP_VARF:
		poin= &(mtex->varfac); break;
	case MAP_DISP:
		poin= &(mtex->dispfac); break;
	}
	
	/* return pointer */
	return poin;
}

/* GS reads the memory pointed at in a specific ordering. There are,
 * however two definitions for it. I have jotted them down here, both,
 * but I think the first one is actually used. The thing is that
 * big-endian systems might read this the wrong way round. OTOH, we
 * constructed the IDs that are read out with this macro explicitly as
 * well. I expect we'll sort it out soon... */

/* from blendef: */
#define GS(a)	(*((short *)(a)))

/* from misc_util: flip the bytes from x  */
/*  #define GS(x) (((unsigned char *)(x))[0] << 8 | ((unsigned char *)(x))[1]) */


/* general function to get pointer to source/destination data  */
void *get_ipo_poin (ID *id, IpoCurve *icu, int *type)
{
	void *poin= NULL;
	MTex *mtex= NULL;

	/* most channels will have float data, but those with other types will override this */
	*type= IPO_FLOAT;

	/* data is divided into 'blocktypes' based on ID-codes */
	switch (GS(id->name)) {
		case ID_OB: /* object channels -----------------------------  */
		{
			Object *ob= (Object *)id;
			
			switch (icu->adrcode) {
			case OB_LOC_X:
				poin= &(ob->loc[0]); break;
			case OB_LOC_Y:
				poin= &(ob->loc[1]); break;
			case OB_LOC_Z:
				poin= &(ob->loc[2]); break;
			case OB_DLOC_X:
				poin= &(ob->dloc[0]); break;
			case OB_DLOC_Y:
				poin= &(ob->dloc[1]); break;
			case OB_DLOC_Z:
				poin= &(ob->dloc[2]); break;
			
			case OB_ROT_X:
				poin= &(ob->rot[0]); *type= IPO_FLOAT_DEGR; break;
			case OB_ROT_Y:
				poin= &(ob->rot[1]); *type= IPO_FLOAT_DEGR; break;
			case OB_ROT_Z:
				poin= &(ob->rot[2]); *type= IPO_FLOAT_DEGR; break;
			case OB_DROT_X:
				poin= &(ob->drot[0]); *type= IPO_FLOAT_DEGR; break;
			case OB_DROT_Y:
				poin= &(ob->drot[1]); *type= IPO_FLOAT_DEGR; break;
			case OB_DROT_Z:
				poin= &(ob->drot[2]); *type= IPO_FLOAT_DEGR; break;
				
			case OB_SIZE_X:
				poin= &(ob->size[0]); break;
			case OB_SIZE_Y:
				poin= &(ob->size[1]); break;
			case OB_SIZE_Z:
				poin= &(ob->size[2]); break;
			case OB_DSIZE_X:
				poin= &(ob->dsize[0]); break;
			case OB_DSIZE_Y:
				poin= &(ob->dsize[1]); break;
			case OB_DSIZE_Z:
				poin= &(ob->dsize[2]); break;
			
			case OB_LAY:
				poin= &(ob->lay); *type= IPO_INT_BIT; break;
				
			case OB_COL_R:	
				poin= &(ob->col[0]); break;
			case OB_COL_G:
				poin= &(ob->col[1]); break;
			case OB_COL_B:
				poin= &(ob->col[2]); break;
			case OB_COL_A:
				poin= &(ob->col[3]); break;
				
			case OB_PD_FSTR:
				if (ob->pd) poin= &(ob->pd->f_strength);
				break;
			case OB_PD_FFALL:
				if (ob->pd) poin= &(ob->pd->f_power);
				break;
			case OB_PD_SDAMP:
				if (ob->pd) poin= &(ob->pd->pdef_damp);
				break;
			case OB_PD_RDAMP:
				if (ob->pd) poin= &(ob->pd->pdef_rdamp);
				break;
			case OB_PD_PERM:
				if (ob->pd) poin= &(ob->pd->pdef_perm);
				break;
			case OB_PD_FMAXD:
				if (ob->pd) poin= &(ob->pd->maxdist);
				break;
			}
		}
			break;
		case ID_MA: /* material channels -----------------------------  */
		{
			Material *ma= (Material *)id;
			
			switch (icu->adrcode) {
			case MA_COL_R:
				poin= &(ma->r); break;
			case MA_COL_G:
				poin= &(ma->g); break;
			case MA_COL_B:
				poin= &(ma->b); break;
			case MA_SPEC_R:
				poin= &(ma->specr); break;
			case MA_SPEC_G:
				poin= &(ma->specg); break;
			case MA_SPEC_B:
				poin= &(ma->specb); break;
			case MA_MIR_R:
				poin= &(ma->mirr); break;
			case MA_MIR_G:
				poin= &(ma->mirg); break;
			case MA_MIR_B:
				poin= &(ma->mirb); break;
			case MA_REF:
				poin= &(ma->ref); break;
			case MA_ALPHA:
				poin= &(ma->alpha); break;
			case MA_EMIT:
				poin= &(ma->emit); break;
			case MA_AMB:
				poin= &(ma->amb); break;
			case MA_SPEC:
				poin= &(ma->spec); break;
			case MA_HARD:
				poin= &(ma->har); *type= IPO_SHORT; break;
			case MA_SPTR:
				poin= &(ma->spectra); break;
			case MA_IOR:
				poin= &(ma->ang); break;
			case MA_MODE:
				poin= &(ma->mode); *type= IPO_INT_BIT; break; // evil... dumping bitflags directly to user!
			case MA_HASIZE:
				poin= &(ma->hasize); break;
			case MA_TRANSLU:
				poin= &(ma->translucency); break;
			case MA_RAYM:
				poin= &(ma->ray_mirror); break;
			case MA_FRESMIR:
				poin= &(ma->fresnel_mir); break;
			case MA_FRESMIRI:
				poin= &(ma->fresnel_mir_i); break;
			case MA_FRESTRA:
				poin= &(ma->fresnel_tra); break;
			case MA_FRESTRAI:
				poin= &(ma->fresnel_tra_i); break;
			case MA_ADD:
				poin= &(ma->add); break;
			}
			
			if (poin == NULL) {
				if (icu->adrcode & MA_MAP1) mtex= ma->mtex[0];
				else if (icu->adrcode & MA_MAP2) mtex= ma->mtex[1];
				else if (icu->adrcode & MA_MAP3) mtex= ma->mtex[2];
				else if (icu->adrcode & MA_MAP4) mtex= ma->mtex[3];
				else if (icu->adrcode & MA_MAP5) mtex= ma->mtex[4];
				else if (icu->adrcode & MA_MAP6) mtex= ma->mtex[5];
				else if (icu->adrcode & MA_MAP7) mtex= ma->mtex[6];
				else if (icu->adrcode & MA_MAP8) mtex= ma->mtex[7];
				else if (icu->adrcode & MA_MAP9) mtex= ma->mtex[8];
				else if (icu->adrcode & MA_MAP10) mtex= ma->mtex[9];
				else if (icu->adrcode & MA_MAP12) mtex= ma->mtex[11];
				else if (icu->adrcode & MA_MAP11) mtex= ma->mtex[10];
				else if (icu->adrcode & MA_MAP13) mtex= ma->mtex[12];
				else if (icu->adrcode & MA_MAP14) mtex= ma->mtex[13];
				else if (icu->adrcode & MA_MAP15) mtex= ma->mtex[14];
				else if (icu->adrcode & MA_MAP16) mtex= ma->mtex[15];
				else if (icu->adrcode & MA_MAP17) mtex= ma->mtex[16];
				else if (icu->adrcode & MA_MAP18) mtex= ma->mtex[17];
				
				if (mtex)
					poin= give_mtex_poin(mtex, (icu->adrcode & (MA_MAP1-1)));
			}
		}
			break;
		case ID_TE: /* texture channels -----------------------------  */
		{
			Tex *tex= (Tex *)id;
			
			if (tex) 
				poin= give_tex_poin(tex, icu->adrcode, type);
		}
			break;
		case ID_SEQ: /* sequence channels -----------------------------  */
		{
			Sequence *seq= (Sequence *)id;
			
			switch (icu->adrcode) {
			case SEQ_FAC1:
				poin= &(seq->facf0); break;
			}
		}
			break;
		case ID_CU: /* curve channels -----------------------------  */
		{
			poin= &(icu->curval);
		}
			break;
		case ID_KE: /* shapekey channels -----------------------------  */
		{
			Key *key= (Key *)id;
			KeyBlock *kb;
			
			for(kb= key->block.first; kb; kb= kb->next) {
				if (kb->adrcode == icu->adrcode)
					break;
			}
			
			if (kb)
				poin= &(kb->curval);
		}
			break;
		case ID_WO: /* world channels -----------------------------  */
		{
			World *wo= (World *)id;
			
			switch (icu->adrcode) {
			case WO_HOR_R:
				poin= &(wo->horr); break;
			case WO_HOR_G:
				poin= &(wo->horg); break;
			case WO_HOR_B:
				poin= &(wo->horb); break;
			case WO_ZEN_R:
				poin= &(wo->zenr); break;
			case WO_ZEN_G:
				poin= &(wo->zeng); break;
			case WO_ZEN_B:
				poin= &(wo->zenb); break;
			
			case WO_EXPOS:
				poin= &(wo->exposure); break;
			
			case WO_MISI:
				poin= &(wo->misi); break;
			case WO_MISTDI:
				poin= &(wo->mistdist); break;
			case WO_MISTSTA:
				poin= &(wo->miststa); break;
			case WO_MISTHI:
				poin= &(wo->misthi); break;
			
			case WO_STAR_R:
				poin= &(wo->starr); break;
			case WO_STAR_G:
				poin= &(wo->starg); break;
			case WO_STAR_B:
				poin= &(wo->starb); break;
			
			case WO_STARDIST:
				poin= &(wo->stardist); break;
			case WO_STARSIZE:
				poin= &(wo->starsize); break;
			}
			
			if (poin == NULL) {
				if (icu->adrcode & MA_MAP1) mtex= wo->mtex[0];
				else if (icu->adrcode & MA_MAP2) mtex= wo->mtex[1];
				else if (icu->adrcode & MA_MAP3) mtex= wo->mtex[2];
				else if (icu->adrcode & MA_MAP4) mtex= wo->mtex[3];
				else if (icu->adrcode & MA_MAP5) mtex= wo->mtex[4];
				else if (icu->adrcode & MA_MAP6) mtex= wo->mtex[5];
				else if (icu->adrcode & MA_MAP7) mtex= wo->mtex[6];
				else if (icu->adrcode & MA_MAP8) mtex= wo->mtex[7];
				else if (icu->adrcode & MA_MAP9) mtex= wo->mtex[8];
				else if (icu->adrcode & MA_MAP10) mtex= wo->mtex[9];
				else if (icu->adrcode & MA_MAP11) mtex= wo->mtex[10];
				else if (icu->adrcode & MA_MAP12) mtex= wo->mtex[11];
				else if (icu->adrcode & MA_MAP13) mtex= wo->mtex[12];
				else if (icu->adrcode & MA_MAP14) mtex= wo->mtex[13];
				else if (icu->adrcode & MA_MAP15) mtex= wo->mtex[14];
				else if (icu->adrcode & MA_MAP16) mtex= wo->mtex[15];
				else if (icu->adrcode & MA_MAP17) mtex= wo->mtex[16];
				else if (icu->adrcode & MA_MAP18) mtex= wo->mtex[17];
				
				if (mtex)
					poin= give_mtex_poin(mtex, (icu->adrcode & (MA_MAP1-1)));
			}
		}
			break;
		case ID_LA: /* lamp channels -----------------------------  */
		{
			Lamp *la= (Lamp *)id;
			
			switch (icu->adrcode) {
			case LA_ENERGY:
				poin= &(la->energy); break;		
			case LA_COL_R:
				poin= &(la->r); break;
			case LA_COL_G:
				poin= &(la->g); break;
			case LA_COL_B:
				poin= &(la->b); break;
			case LA_DIST:
				poin= &(la->dist); break;		
			case LA_SPOTSI:
				poin= &(la->spotsize); break;
			case LA_SPOTBL:
				poin= &(la->spotblend); break;
			case LA_QUAD1:
				poin= &(la->att1); break;
			case LA_QUAD2:
				poin= &(la->att2); break;
			case LA_HALOINT:
				poin= &(la->haint); break;
			}
			
			if (poin == NULL) {
				if (icu->adrcode & MA_MAP1) mtex= la->mtex[0];
				else if (icu->adrcode & MA_MAP2) mtex= la->mtex[1];
				else if (icu->adrcode & MA_MAP3) mtex= la->mtex[2];
				else if (icu->adrcode & MA_MAP4) mtex= la->mtex[3];
				else if (icu->adrcode & MA_MAP5) mtex= la->mtex[4];
				else if (icu->adrcode & MA_MAP6) mtex= la->mtex[5];
				else if (icu->adrcode & MA_MAP7) mtex= la->mtex[6];
				else if (icu->adrcode & MA_MAP8) mtex= la->mtex[7];
				else if (icu->adrcode & MA_MAP9) mtex= la->mtex[8];
				else if (icu->adrcode & MA_MAP10) mtex= la->mtex[9];
				else if (icu->adrcode & MA_MAP11) mtex= la->mtex[10];
				else if (icu->adrcode & MA_MAP12) mtex= la->mtex[11];
				else if (icu->adrcode & MA_MAP13) mtex= la->mtex[12];
				else if (icu->adrcode & MA_MAP14) mtex= la->mtex[13];
				else if (icu->adrcode & MA_MAP15) mtex= la->mtex[14];
				else if (icu->adrcode & MA_MAP16) mtex= la->mtex[15];
				else if (icu->adrcode & MA_MAP17) mtex= la->mtex[16];
				else if (icu->adrcode & MA_MAP18) mtex= la->mtex[17];
				
				if (mtex)
					poin= give_mtex_poin(mtex, (icu->adrcode & (MA_MAP1-1)));
			}
		}
			break;
		case ID_CA: /* camera channels -----------------------------  */
		{
			Camera *ca= (Camera *)id;
			
			switch (icu->adrcode) {
			case CAM_LENS:
				if (ca->type == CAM_ORTHO)
					poin= &(ca->ortho_scale);
				else
					poin= &(ca->lens); 
				break;
			case CAM_STA:
				poin= &(ca->clipsta); break;
			case CAM_END:
				poin= &(ca->clipend); break;
				
			case CAM_YF_APERT:
				poin= &(ca->YF_aperture); break;
			case CAM_YF_FDIST:
				poin= &(ca->YF_dofdist); break;
				
			case CAM_SHIFT_X:
				poin= &(ca->shiftx); break;
			case CAM_SHIFT_Y:
				poin= &(ca->shifty); break;
			}
		}
			break;
		case ID_SO: /* sound channels -----------------------------  */
		{
			bSound *snd= (bSound *)id;
			
			switch (icu->adrcode) {
			case SND_VOLUME:
				poin= &(snd->volume); break;
			case SND_PITCH:
				poin= &(snd->pitch); break;
			case SND_PANNING:
				poin= &(snd->panning); break;
			case SND_ATTEN:
				poin= &(snd->attenuation); break;
			}
		}
			break;
		case ID_PA: /* particle channels -----------------------------  */
		{
			ParticleSettings *part= (ParticleSettings *)id;
			
			switch (icu->adrcode) {
			case PART_EMIT_FREQ:
			case PART_EMIT_LIFE:
			case PART_EMIT_VEL:
			case PART_EMIT_AVE:
			case PART_EMIT_SIZE:
				poin= NULL; 
				break;
			
			case PART_CLUMP:
				poin= &(part->clumpfac); break;
			case PART_AVE:
				poin= &(part->avefac); break;
			case PART_SIZE:
				poin= &(part->size); break;
			case PART_DRAG:
				poin= &(part->dragfac); break;
			case PART_BROWN:
				poin= &(part->brownfac); break;
			case PART_DAMP:
				poin= &(part->dampfac); break;
			case PART_LENGTH:
				poin= &(part->length); break;
			case PART_GRAV_X:
				poin= &(part->acc[0]); break;
			case PART_GRAV_Y:
				poin= &(part->acc[1]); break;
			case PART_GRAV_Z:
				poin= &(part->acc[2]); break;
			case PART_KINK_AMP:
				poin= &(part->kink_amp); break;
			case PART_KINK_FREQ:
				poin= &(part->kink_freq); break;
			case PART_KINK_SHAPE:
				poin= &(part->kink_shape); break;
			case PART_BB_TILT:
				poin= &(part->bb_tilt); break;
				
			case PART_PD_FSTR:
				if (part->pd) poin= &(part->pd->f_strength);
				break;
			case PART_PD_FFALL:
				if (part->pd) poin= &(part->pd->f_power);
				break;
			case PART_PD_FMAXD:
				if (part->pd) poin= &(part->pd->maxdist);
				break;
			case PART_PD2_FSTR:
				if (part->pd2) poin= &(part->pd2->f_strength);
				break;
			case PART_PD2_FFALL:
				if (part->pd2) poin= &(part->pd2->f_power);
				break;
			case PART_PD2_FMAXD:
				if (part->pd2) poin= &(part->pd2->maxdist);
				break;
			}
		}
			break;
	}

	/* return pointer */
	return poin;
}

/* --------------------- IPO-Curve Limits ----------------------------- */

/* set limits for IPO-curve 
 * Note: must be synced with UI and PyAPI
 */
void set_icu_vars (IpoCurve *icu)
{
	/* defaults. 0.0 for y-extents makes these ignored */
	icu->ymin= icu->ymax= 0.0;
	icu->ipo= IPO_BEZ;
	
	switch (icu->blocktype) {
		case ID_OB: /* object channels -----------------------------  */
		{
			if (icu->adrcode == OB_LAY) {
				icu->ipo= IPO_CONST;
				icu->vartype= IPO_BITS;
			}
		}
			break;
		case ID_MA: /* material channels -----------------------------  */
		{
			if (icu->adrcode < MA_MAP1) {
				switch (icu->adrcode) {
				case MA_HASIZE:
					icu->ymax= 10000.0; break;
				case MA_HARD:
					icu->ymax= 511.0; break;
				case MA_SPEC:
					icu->ymax= 2.0; break;
				case MA_MODE:
					icu->ipo= IPO_CONST;
					icu->vartype= IPO_BITS; break;
				case MA_RAYM:
					icu->ymax= 1.0; break;
				case MA_TRANSLU:
					icu->ymax= 1.0; break;
				case MA_IOR:
					icu->ymin= 1.0;
					icu->ymax= 3.0; break;
				case MA_FRESMIR:
					icu->ymax= 5.0; break;
				case MA_FRESMIRI:
					icu->ymin= 1.0;
					icu->ymax= 5.0; break;
				case MA_FRESTRA:
					icu->ymax= 5.0; break;
				case MA_FRESTRAI:
					icu->ymin= 1.0;
					icu->ymax= 5.0; break;
				case MA_ADD:
					icu->ymax= 1.0; break;
				case MA_EMIT:
					icu->ymax= 2.0; break;
				default:
					icu->ymax= 1.0; break;
				}
			}
			else {
				switch (icu->adrcode & (MA_MAP1-1)) {
				case MAP_OFS_X:
				case MAP_OFS_Y:
				case MAP_OFS_Z:
				case MAP_SIZE_X:
				case MAP_SIZE_Y:
				case MAP_SIZE_Z:
					icu->ymax= 1000.0;
					icu->ymin= -1000.0;
					break;
				case MAP_R:
				case MAP_G:
				case MAP_B:
				case MAP_DVAR:
				case MAP_COLF:
				case MAP_VARF:
				case MAP_DISP:
					icu->ymax= 1.0;
					break;
				case MAP_NORF:
					icu->ymax= 25.0;
					break;
				}
			}
		}
			break;
		case ID_TE: /* texture channels -----------------------------  */
		{
			switch (icu->adrcode & (MA_MAP1-1)) {
				case TE_NSIZE:
					icu->ymin= 0.0001f;
					icu->ymax= 2.0f; 
					break;
				case TE_NDEPTH:
					icu->vartype= IPO_SHORT;
					icu->ipo= IPO_CONST;
					icu->ymax= 6.0f; 
					break;
				case TE_NTYPE:
					icu->vartype= IPO_SHORT;
					icu->ipo= IPO_CONST;
					icu->ymax= 1.0f; 
					break;
				case TE_TURB:
					icu->ymax= 200.0f; 
					break;
				case TE_VNW1:
				case TE_VNW2:
				case TE_VNW3:
				case TE_VNW4:
					icu->ymax= 2.0f;
					icu->ymin= -2.0f; 
					break;
				case TE_VNMEXP:
					icu->ymax= 10.0f;
					icu->ymin= 0.01f; 
					break;
				case TE_VN_DISTM:
					icu->vartype= IPO_SHORT;
					icu->ipo= IPO_CONST;
					icu->ymax= 6.0f; 
					break;
				case TE_VN_COLT:
					icu->vartype= IPO_SHORT;
					icu->ipo= IPO_CONST;
					icu->ymax= 3.0f; 
					break;
				case TE_ISCA:
					icu->ymax= 10.0f;
					icu->ymin= 0.01f; 
					break;
				case TE_DISTA:
					icu->ymax= 10.0f; 
					break;
				case TE_MG_TYP:
					icu->vartype= IPO_SHORT;
					icu->ipo= IPO_CONST;
					icu->ymax= 6.0f; 
					break;
				case TE_MGH:
					icu->ymin= 0.0001f;
					icu->ymax= 2.0f; 
					break;
				case TE_MG_LAC:
				case TE_MG_OFF:
				case TE_MG_GAIN:
					icu->ymax= 6.0f; break;
				case TE_MG_OCT:
					icu->ymax= 8.0f; break;
				case TE_N_BAS1:
				case TE_N_BAS2:
					icu->vartype= IPO_SHORT;
					icu->ipo= IPO_CONST;
					icu->ymax= 8.0f; 
					break;
				case TE_COL_R:
					icu->ymax= 0.0f; break;
				case TE_COL_G:
					icu->ymax= 2.0f; break;
				case TE_COL_B:
					icu->ymax= 2.0f; break;
				case TE_BRIGHT:
					icu->ymax= 2.0f; break;
				case TE_CONTRA:
					icu->ymax= 5.0f; break;	
			}
		}
			break;
		case ID_SEQ: /* sequence channels -----------------------------  */
		{
			icu->ymax= 1.0f;
		}
			break;
		case ID_CU: /* curve channels -----------------------------  */
		{
			icu->ymax= 1.0f;
		}
			break;
		case ID_WO: /* world channels -----------------------------  */
		{
			if (icu->adrcode < MA_MAP1) {
				switch (icu->adrcode) {
				case WO_EXPOS:
					icu->ymax= 5.0f; break;
				
				case WO_MISTDI:
				case WO_MISTSTA:
				case WO_MISTHI:
				case WO_STARDIST:
				case WO_STARSIZE:
					break;
					
				default:
					icu->ymax= 1.0f;
					break;
				}
			}
			else {
				switch (icu->adrcode & (MA_MAP1-1)) {
				case MAP_OFS_X:
				case MAP_OFS_Y:
				case MAP_OFS_Z:
				case MAP_SIZE_X:
				case MAP_SIZE_Y:
				case MAP_SIZE_Z:
					icu->ymax= 100.0f;
					icu->ymin= -100.0f;
					break;
				case MAP_R:
				case MAP_G:
				case MAP_B:
				case MAP_DVAR:
				case MAP_COLF:
				case MAP_NORF:
				case MAP_VARF:
				case MAP_DISP:
					icu->ymax= 1.0f;
				}
			}
		}
			break;
		case ID_LA: /* lamp channels -----------------------------  */
		{
			if (icu->adrcode < MA_MAP1) {
				switch (icu->adrcode) {
				case LA_ENERGY:
				case LA_DIST:
					break;		
				
				case LA_COL_R:
				case LA_COL_G:
				case LA_COL_B:
				case LA_SPOTBL:
				case LA_QUAD1:
				case LA_QUAD2:
					icu->ymax= 1.0f; break;
					
				case LA_SPOTSI:
					icu->ymax= 180.0f; break;
				
				case LA_HALOINT:
					icu->ymax= 5.0f; break;
				}
			}
			else {
				switch (icu->adrcode & (MA_MAP1-1)) {
				case MAP_OFS_X:
				case MAP_OFS_Y:
				case MAP_OFS_Z:
				case MAP_SIZE_X:
				case MAP_SIZE_Y:
				case MAP_SIZE_Z:
					icu->ymax= 100.0f;
					icu->ymin= -100.0f;
					break;
				case MAP_R:
				case MAP_G:
				case MAP_B:
				case MAP_DVAR:
				case MAP_COLF:
				case MAP_NORF:
				case MAP_VARF:
				case MAP_DISP:
					icu->ymax= 1.0f;
				}
			}
		}	
			break;
		case ID_CA: /* camera channels -----------------------------  */
		{
			switch (icu->adrcode) {
			case CAM_LENS:
				icu->ymin= 1.0f;
				icu->ymax= 1000.0f;
				break;
			case CAM_STA:
				icu->ymin= 0.001f;
				break;
			case CAM_END:
				icu->ymin= 0.1f;
				break;
				
			case CAM_YF_APERT:
				icu->ymin = 0.0f;
				icu->ymax = 2.0f;
				break;
			case CAM_YF_FDIST:
				icu->ymin = 0.0f;
				icu->ymax = 5000.0f;
				break;
				
			case CAM_SHIFT_X:
			case CAM_SHIFT_Y:
				icu->ymin= -2.0f;
				icu->ymax= 2.0f;
				break;
			}
		}
			break;
		case ID_SO: /* sound channels -----------------------------  */
		{
			switch (icu->adrcode) {
			case SND_VOLUME:
				icu->ymin= 0.0f;
				icu->ymax= 1.0f;
				break;
			case SND_PITCH:
				icu->ymin= -12.0f;
				icu->ymin= 12.0f;
				break;
			case SND_PANNING:
				icu->ymin= 0.0f;
				icu->ymax= 1.0f;
				break;
			case SND_ATTEN:
				icu->ymin= 0.0f;
				icu->ymin= 1.0f;
				break;
			}
		}
			break;
		case ID_PA: /* particle channels -----------------------------  */
		{
			switch (icu->adrcode) {
			case PART_EMIT_LIFE:
			case PART_SIZE:
			case PART_KINK_FREQ:
			case PART_EMIT_VEL:
			case PART_EMIT_AVE:
			case PART_EMIT_SIZE:
				icu->ymin= 0.0f;
				break;
			case PART_CLUMP:
				icu->ymin= -1.0f;
				icu->ymax= 1.0f;
				break;
			case PART_DRAG:
			case PART_DAMP:
			case PART_LENGTH:
				icu->ymin= 0.0f;
				icu->ymax= 1.0f;
				break;
			case PART_KINK_SHAPE:
				icu->ymin= -0.999f;
				icu->ymax= 0.999f;
				break;
			}
		}
			break;
		case ID_CO: /* constraint channels -----------------------------  */
		{
			icu->ymin= 0.0f;
			icu->ymax= 1.0f;
		}
			break;
	}
	
	/* by default, slider limits will be icu->ymin and icu->ymax */
	icu->slide_min= icu->ymin;
	icu->slide_max= icu->ymax;
}

/* --------------------- Pointer I/O API ----------------------------- */
 
/* write the given value directly into the given pointer */
void write_ipo_poin (void *poin, int type, float val)
{
	/* Note: we only support a limited number of types, with the value
	 * to set needing to be cast to the appropriate type first
	 * 	-> (float to integer conversions could be slow)
	 */
	switch(type) {
	case IPO_FLOAT:
		*((float *)poin)= val;
		break;
		
	case IPO_FLOAT_DEGR: /* special hack for rotation so that it fits on same axis as other transforms */
		*((float *)poin)= (float)(val * M_PI_2 / 9.0);
		break;
		
	case IPO_INT:
	case IPO_INT_BIT: // fixme... directly revealing bitflag combinations is evil!
	case IPO_LONG:
		*((int *)poin)= (int)val;
		break;
		
	case IPO_SHORT:
	case IPO_SHORT_BIT: // fixme... directly revealing bitflag combinations is evil!
		*((short *)poin)= (short)val;
		break;
		
	case IPO_CHAR:
	case IPO_CHAR_BIT: // fixme... directly revealing bitflag combinations is evil!
		*((char *)poin)= (char)val;
		break;
	}
}

/* read the value from the pointer that was obtained */
float read_ipo_poin (void *poin, int type)
{
	float val = 0.0;
	
	/* Note: we only support a limited number of types, with the value
	 * to set needing to be cast to the appropriate type first
	 *	-> (int to float conversions may loose accuracy in rare cases)
	 */
	switch (type) {
	case IPO_FLOAT:
		val= *((float *)poin);
		break;
		
	case IPO_FLOAT_DEGR: /* special hack for rotation so that it fits on same axis as other transforms */
		val= *( (float *)poin);
		val = (float)(val / (M_PI_2/9.0));
		break;
	
	case IPO_INT:
	case IPO_INT_BIT: // fixme... directly revealing bitflag combinations is evil!
	case IPO_LONG:
		val= (float)( *((int *)poin) );
		break;
		
	case IPO_SHORT:
	case IPO_SHORT_BIT: // fixme... directly revealing bitflag combinations is evil!
		val= *((short *)poin);
		break;
	
	case IPO_CHAR:
	case IPO_CHAR_BIT: // fixme... directly revealing bitflag combinations is evil
		val= *((char *)poin);
		break;
	}
	
	/* return value */
	return val;
}

// !!!!!!!!!!!!!!!!!!!!!!!!!!!! FIXME - BAD CRUFT WARNING !!!!!!!!!!!!!!!!!!!!!!!


/* ***************************** IPO <--> GameEngine Interface ********************************* */

/* channels is max 32 items, allocated by calling function */
short IPO_GetChannels (Ipo *ipo, IPO_Channel *channels)
{
	IpoCurve *icu;
	int total = 0;
	
	/* don't do anything with no IPO-block */
	if (ipo == NULL) 
		return 0;
	
	/* store the IPO-curve's adrcode in the relevant channel slot */
	for (icu=ipo->curve.first; (icu) && (total < 31); icu=icu->next, total++)
		channels[total]= icu->adrcode;
	
	/* return the number of channels stored */
	return total;
}

/* Get the float value for channel 'channel' at time 'ctime' */
float IPO_GetFloatValue (Ipo *ipo, IPO_Channel channel, float ctime)
{
	/* don't evaluate if no IPO to use */
	if (ipo == NULL) 
		return 0;
	
	/* only calculate the specified channel */
	calc_ipo_spec(ipo, channel, &ctime);
	
	/* unapply rotation hack, as gameengine doesn't use it */
	if ((OB_ROT_X <= channel) && (channel <= OB_DROT_Z))
		ctime *= (float)(M_PI_2/9.0); 

	/* return the value of this channel */
	return ctime;
}
