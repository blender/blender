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
#endif   
#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"

#include "IMB_imbuf_types.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_userdef_types.h"
#include "DNA_space_types.h"
#include "DNA_image_types.h"
#include "DNA_object_types.h" // only for uvedit_selectionCB() (struct Object)

#include "BKE_depsgraph.h"
#include "BKE_global.h"
#include "BKE_mesh.h"
#include "BKE_displist.h"
#include "BKE_object.h"
#include "BKE_utildefines.h"

#include "BIF_gl.h"
#include "BIF_interface.h"
#include "BIF_screen.h"
#include "BIF_drawimage.h"
#include "BIF_editview.h"
#include "BIF_space.h"
#include "BIF_editsima.h"
#include "BIF_toolbox.h"
#include "BIF_transform.h"
#include "BIF_mywindow.h"

#include "BSE_drawipo.h"
#include "BSE_edit.h"
#include "BSE_trans_types.h"

#include "BDR_editobject.h"
#include "BDR_unwrapper.h"

#include "blendef.h"
#include "mydevice.h"

struct uvvertsort {
	unsigned int v, f;
	unsigned char tf_sel;
	char flag;
};

/* local prototypes */
void clever_numbuts_sima(void);
void sel_uvco_inside_radius(short , TFace *, int , float *, float *, short);
void uvedit_selectionCB(short , Object *, short *, float ); /* used in edit.c*/ 

static int compuvvert(const void *u1, const void *u2)
{
	const struct uvvertsort *v1=u1, *v2=u2;
	if (v1->v > v2->v) return 1;
	else if (v1->v < v2->v) return -1;
	return 0;
}

void object_uvs_changed(Object *ob)
{
	DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);

	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWIMAGE, 0);
}

void object_tface_flags_changed(Object *ob, int updateButtons)
{
	if (updateButtons) allqueue(REDRAWBUTSEDIT, 0);
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWIMAGE, 0);
}

int is_uv_tface_editing_allowed_silent(void)
{
	Mesh *me;

	if(G.obedit) return 0;
	if(G.sima->mode!=SI_TEXTURE) return 0;
	if(!(G.f & G_FACESELECT)) return 0;  
	me= get_mesh(OBACT);
	if(me==0 || me->tface==0) return 0;
	
	return 1;
}

int is_uv_tface_editing_allowed(void)
{
	if(G.obedit) error("Unable to perform action in Edit Mode");

	return is_uv_tface_editing_allowed_silent();
}

void get_connected_limit_tface_uv(float *limit)
{
	if(G.sima->image && G.sima->image->ibuf && G.sima->image->ibuf->x > 0 &&
	   G.sima->image->ibuf->y > 0) {
		limit[0]= 0.05/(float)G.sima->image->ibuf->x;
		limit[1]= 0.05/(float)G.sima->image->ibuf->y;
	}
	else
		limit[0]= limit[1]= 0.05/256.0;
}

void clever_numbuts_sima(void)
{
	float ocent[2], cent[2]= {0.0, 0.0};
	int imx, imy;
	int i, nactive= 0;
	Mesh *me;
	
	if( is_uv_tface_editing_allowed()==0 ) return;
	me= get_mesh(OBACT);
	
	if (G.sima->image && G.sima->image->ibuf) {
		imx= G.sima->image->ibuf->x;
		imy= G.sima->image->ibuf->y;
	} else
		imx= imy= 256;
	
	for (i=0; i<me->totface; i++) {
		MFace *mf= &((MFace*) me->mface)[i];
		TFace *tf= &((TFace*) me->tface)[i];
		
		if (!(tf->flag & TF_SELECT))
			continue;
		
		if (tf->flag & TF_SEL1) {
			cent[0]+= tf->uv[0][0];
			cent[1]+= tf->uv[0][1];
			nactive++;
		}
		if (tf->flag & TF_SEL2) {
			cent[0]+= tf->uv[1][0];
			cent[1]+= tf->uv[1][1];
			nactive++;
		}
		if (tf->flag & TF_SEL3) {
			cent[0]+= tf->uv[2][0];
			cent[1]+= tf->uv[2][1];
			nactive++;
		}
		if (mf->v4 && (tf->flag & TF_SEL4)) {
			cent[0]+= tf->uv[3][0];
			cent[1]+= tf->uv[3][1];
			nactive++;
		}
	}
	
	if (nactive) {
		cent[0]= (cent[0]*imx)/nactive;
		cent[1]= (cent[1]*imy)/nactive;

		add_numbut(0, NUM|FLO, "LocX:", -imx*20, imx*20, &cent[0], NULL);
		add_numbut(1, NUM|FLO, "LocY:", -imy*20, imy*20, &cent[1], NULL);
		
		ocent[0]= cent[0];
		ocent[1]= cent[1];
		if (do_clever_numbuts((nactive==1)?"Active Vertex":"Selected Center", 2, REDRAW)) {
			float delta[2];
			
			delta[0]= (cent[0]-ocent[0])/imx;
			delta[1]= (cent[1]-ocent[1])/imy;

			for (i=0; i<me->totface; i++) {
				MFace *mf= &((MFace*) me->mface)[i];
				TFace *tf= &((TFace*) me->tface)[i];
			
				if (!(tf->flag & TF_SELECT))
					continue;
			
				if (tf->flag & TF_SEL1) {
					tf->uv[0][0]+= delta[0];
					tf->uv[0][1]+= delta[1];
				}
				if (tf->flag & TF_SEL2) {
					tf->uv[1][0]+= delta[0];
					tf->uv[1][1]+= delta[1];
				}
				if (tf->flag & TF_SEL3) {
					tf->uv[2][0]+= delta[0];
					tf->uv[2][1]+= delta[1];
				}
				if (mf->v4 && (tf->flag & TF_SEL4)) {
					tf->uv[3][0]+= delta[0];
					tf->uv[3][1]+= delta[1];
				}
			}
			
			object_uvs_changed(OBACT);
		}
	}
}

void be_square_tface_uv(Mesh *me)
{
	TFace *tface;
	MFace *mface;
	int a;
	
	/* if 1 vertex selected: doit (with the selected vertex) */
	mface= (MFace*)me->mface;
	tface= (TFace*)me->tface;
	for(a=me->totface; a>0; a--, tface++, mface++) {
		if(mface->v4) {
			if(tface->flag & TF_SELECT) {
				if(tface->flag & TF_SEL1) {
					if( tface->uv[1][0] == tface->uv[2][0] ) {
						tface->uv[1][1]= tface->uv[0][1];
						tface->uv[3][0]= tface->uv[0][0];
					}
					else {	
						tface->uv[1][0]= tface->uv[0][0];
						tface->uv[3][1]= tface->uv[0][1];
					}
					
				}
				if(tface->flag & TF_SEL2) {
					if( tface->uv[2][1] == tface->uv[3][1] ) {
						tface->uv[2][0]= tface->uv[1][0];
						tface->uv[0][1]= tface->uv[1][1];
					}
					else {
						tface->uv[2][1]= tface->uv[1][1];
						tface->uv[0][0]= tface->uv[1][0];
					}

				}
				if(tface->flag & TF_SEL3) {
					if( tface->uv[3][0] == tface->uv[0][0] ) {
						tface->uv[3][1]= tface->uv[2][1];
						tface->uv[1][0]= tface->uv[2][0];
					}
					else {
						tface->uv[3][0]= tface->uv[2][0];
						tface->uv[1][1]= tface->uv[2][1];
					}
				}
				if(tface->flag & TF_SEL4) {
					if( tface->uv[0][1] == tface->uv[1][1] ) {
						tface->uv[0][0]= tface->uv[3][0];
						tface->uv[2][1]= tface->uv[3][1];
					}
					else  {
						tface->uv[0][1]= tface->uv[3][1];
						tface->uv[2][0]= tface->uv[3][0];
					}

				}
			}
		}
	}
}

void transform_aspect_ratio_tface_uv(float *aspx, float *aspy)
{
	int w, h;

	transform_width_height_tface_uv(&w, &h);
	*aspx= (float)w/256.0f;
	*aspy= (float)h/256.0f;
}

void transform_width_height_tface_uv(int *width, int *height)
{
	if(G.sima->image && G.sima->image->ibuf) {
		*width= G.sima->image->ibuf->x;
		*height= G.sima->image->ibuf->y;
	}
	else {
		*width= 256;
		*height= 256;
	}
}

void mirror_tface_uv(char mirroraxis)
{
	Mirror((short)mirroraxis);
}

void mirrormenu_tface_uv(void)
{
	short mode= 0;

	if( is_uv_tface_editing_allowed()==0 ) return;

	mode= pupmenu("Mirror%t|X Axis%x1|Y Axis%x2|");

	if(mode==-1) return;

	if(mode==1) mirror_tface_uv('x');
	else if(mode==2) mirror_tface_uv('y');

	BIF_undo_push("Mirror UV");
}

void weld_align_tface_uv(char tool)
{
	MFace *mface;
	TFace *tface;
	Mesh *me;
	float min[2], max[2], cent[2];
	int a;
	
	if( is_uv_tface_editing_allowed()==0 ) return;
	me= get_mesh(OBACT);

	if (!minmax_tface_uv(min, max))
		return;

	cent[0]= (min[0]+max[0])/2.0;
	cent[1]= (min[1]+max[1])/2.0;

	if(tool == 'x' || tool == 'w') {
		tface= me->tface;
		mface= me->mface;
		for(a=me->totface; a>0; a--, tface++, mface++) {
			if(tface->flag & TF_SELECT) {
				if(tface->flag & TF_SEL1)
					tface->uv[0][0]= cent[0];
				if(tface->flag & TF_SEL2)
					tface->uv[1][0]= cent[0];
				if(tface->flag & TF_SEL3)
					tface->uv[2][0]= cent[0];
				if(mface->v4 && (tface->flag & TF_SEL4))
					tface->uv[3][0]= cent[0];
			}
		}
	}

	if(tool == 'y' || tool == 'w') {
		tface= me->tface;
		mface= me->mface;
		for(a=me->totface; a>0; a--, tface++, mface++) {
			if(tface->flag & TF_SELECT) {
				if(tface->flag & TF_SEL1)
					tface->uv[0][1]= cent[1];
				if(tface->flag & TF_SEL2)
					tface->uv[1][1]= cent[1];
				if(tface->flag & TF_SEL3)
					tface->uv[2][1]= cent[1];
				if(mface->v4 && (tface->flag & TF_SEL4))
					tface->uv[3][1]= cent[1];
			}
		}
	}

	object_uvs_changed(OBACT);
}

void weld_align_menu_tface_uv(void)
{
	short mode= 0;

	if( is_uv_tface_editing_allowed()==0 ) return;

	mode= pupmenu("Weld/Align%t|Weld%x1|Align X%x2|Align Y%x3|");

	if(mode==-1) return;

	if(mode==1) weld_align_tface_uv('w');
	else if(mode==2) weld_align_tface_uv('x');
	else if(mode==3) weld_align_tface_uv('y');

	if(mode==1) BIF_undo_push("Weld UV");
	else if(mode==2 || mode==3) BIF_undo_push("Align UV");
}

void select_swap_tface_uv(void)
{
	Mesh *me;
	TFace *tface;
	MFace *mface;
	int a, sel=0;
	
	if( is_uv_tface_editing_allowed()==0 ) return;
	me= get_mesh(OBACT);

	for(a=me->totface, tface= me->tface; a>0; a--, tface++) {
		if(tface->flag & TF_SELECT) {	
			if(tface->flag & (TF_SEL1+TF_SEL2+TF_SEL3+TF_SEL4)) {
				sel= 1;
				break;
			}
		}
	}
	
	mface= me->mface;
	for(a=me->totface, tface= me->tface; a>0; a--, tface++, mface++) {
		if(tface->flag & TF_SELECT) {
			if(mface->v4) {
				if(sel) tface->flag &= ~(TF_SEL1+TF_SEL2+TF_SEL3+TF_SEL4);
				else tface->flag |= (TF_SEL1+TF_SEL2+TF_SEL3+TF_SEL4);
			}
			else {
				if(sel) tface->flag &= ~(TF_SEL1+TF_SEL2+TF_SEL3+TF_SEL4);
				else tface->flag |= (TF_SEL1+TF_SEL2+TF_SEL3);
			}
		}
	}
	
	BIF_undo_push("Select swap UV");

	allqueue(REDRAWIMAGE, 0);
}

static int msel_hit(float *limit, unsigned int *hitarray, unsigned int vertexid, float **uv, float *uv2)
{
	int i;
	for(i=0; i< 4; i++) {
		if(hitarray[i] == vertexid) {
			if(G.sima->flag & SI_LOCALSTICKY) {
				if(fabs(uv[i][0]-uv2[0]) < limit[0] &&
			    fabs(uv[i][1]-uv2[1]) < limit[1])
					return 1;
			}
			else return 1;
		}
	}
	return 0;
}

static void find_nearest_tface(TFace **nearesttf, MFace **nearestmf)
{
	Mesh *me;
	TFace *tf;
	MFace *mf;
	int a, i, nverts, mindist, dist, fcenter[2], uval[2];
	short mval[2];

	getmouseco_areawin(mval);	

	mindist= 0x7FFFFFF;
	*nearesttf= NULL;
	*nearestmf= NULL;

	me= get_mesh(OBACT);
	mf= (MFace*)me->mface;
	tf= (TFace*)me->tface;

	for(a=me->totface; a>0; a--, tf++, mf++) {
		if(tf->flag & TF_SELECT) {

			fcenter[0]= fcenter[1]= 0;
			nverts= mf->v4? 4: 3;
			for(i=0; i<nverts; i++) {
				uvco_to_areaco_noclip(tf->uv[i], uval);
				fcenter[0] += uval[0];
				fcenter[1] += uval[1];
			}

			fcenter[0] /= nverts;
			fcenter[1] /= nverts;

			dist= abs(mval[0]- fcenter[0])+ abs(mval[1]- fcenter[1]);
			if (dist < mindist) {
				*nearesttf= tf;
				*nearestmf= mf;
				mindist= dist;
			}
		}
	}
}

static int nearest_uv_between(TFace *tf, int nverts, int id, short *mval, int *uval)
{
	float m[3], v1[3], v2[3], c1, c2;
	int id1, id2;

	id1= (id+nverts-1)%nverts;
	id2= (id+nverts+1)%nverts;

	m[0] = (float)(mval[0]-uval[0]);
	m[1] = (float)(mval[1]-uval[1]);
	Vec2Subf(v1, tf->uv[id1], tf->uv[id]);
	Vec2Subf(v2, tf->uv[id2], tf->uv[id]);

	/* m and v2 on same side of v-v1? */
	c1= v1[0]*m[1] - v1[1]*m[0];
	c2= v1[0]*v2[1] - v1[1]*v2[0];

	if (c1*c2 < 0.0f)
		return 0;

	/* m and v1 on same side of v-v2? */
	c1= v2[0]*m[1] - v2[1]*m[0];
	c2= v2[0]*v1[1] - v2[1]*v1[0];

	return (c1*c2 >= 0.0f);
}

static void find_nearest_uv(TFace **nearesttf, unsigned int *nearestv, int *nearestuv)
{
	Mesh *me;
	TFace *tf;
	MFace *mf;
	int a, i, nverts, mindist, dist, uval[2];
	short mval[2];

	getmouseco_areawin(mval);	

	mindist= 0x7FFFFFF;
	*nearesttf= NULL;

	me= get_mesh(OBACT);
	mf= (MFace*)me->mface;
	tf= (TFace*)me->tface;

	for(a=me->totface; a>0; a--, tf++, mf++) {
		if(tf->flag & TF_SELECT) {

			nverts= mf->v4? 4: 3;
			for(i=0; i<nverts; i++) {
				uvco_to_areaco_noclip(tf->uv[i], uval);
				dist= abs(mval[0]-uval[0]) + abs(mval[1]-uval[1]);

				if(tf->flag & TF_SEL_MASK(i))
					dist += 5;

				if(dist<=mindist) {
					if(dist==mindist)
						if (!nearest_uv_between(tf, nverts, i, mval, uval))
							continue;

					mindist= dist; 

					*nearesttf= tf;
					*nearestuv= i;

					if (i==0) *nearestv=  mf->v1;
					else if (i==1) *nearestv=  mf->v2;
					else if (i==2) *nearestv=  mf->v3;
					else *nearestv=  mf->v4;
				}
			}
		}
	}
}

void mouse_select_sima(void)
{
	Mesh *me;
	TFace *tf, *nearesttf;
	MFace *mf, *nearestmf;
	int a, selectsticky, sticky, actface, nearestuv, i;
	unsigned int hitv[4], nearestv;
	float *hituv[4], limit[2];
	
	if( is_uv_tface_editing_allowed()==0 ) return;
	me= get_mesh(OBACT);

	get_connected_limit_tface_uv(limit);
	actface= (G.qual & LR_ALTKEY || G.sima->flag & SI_SELACTFACE);
	sticky= (G.qual & LR_CTRLKEY || G.sima->flag & SI_STICKYUVS ||
	         G.sima->flag & SI_LOCALSTICKY);

	if(actface) {
		find_nearest_tface(&nearesttf, &nearestmf);
		if(nearesttf==NULL)
			return;

		nearesttf->flag |= TF_ACTIVE;

		for (i=0; i<4; i++)
			hituv[i]= nearesttf->uv[i];

		hitv[0]= nearestmf->v1;
		hitv[1]= nearestmf->v2;
		hitv[2]= nearestmf->v3;
		hitv[3]= nearestmf->v4? nearestmf->v4: 0xFFFFFFFF;
	}
	else {
		find_nearest_uv(&nearesttf, &nearestv, &nearestuv);
		if(nearesttf==NULL)
			return;

		if(sticky) {
			for(i=0; i<4; i++)
				hitv[i]= 0xFFFFFFFF;
			hitv[nearestuv]= nearestv;
			hituv[nearestuv]= nearesttf->uv[nearestuv];
		}
	}

	if(G.qual & LR_SHIFTKEY) {
		/* (de)select face */
		if(actface) {
			if(!(~nearesttf->flag & (TF_SEL1|TF_SEL2|TF_SEL3))
			   && (!nearestmf->v4 || nearesttf->flag & TF_SEL4)) {
				nearesttf->flag &= ~(TF_SEL1|TF_SEL2|TF_SEL3|TF_SEL4);
				selectsticky= 0;
			}
			else {
				nearesttf->flag |= TF_SEL1|TF_SEL2|TF_SEL3|TF_SEL4;
				selectsticky= 1;
			}
		}
		/* (de)select uv node */
		else {
			if(nearesttf->flag & TF_SEL_MASK(nearestuv)) {
				nearesttf->flag &= ~TF_SEL_MASK(nearestuv);
				selectsticky= 0;
			}
			else {
				nearesttf->flag |= TF_SEL_MASK(nearestuv);
				selectsticky= 1;
			}
		}

		/* (de)select sticky uv nodes */
		if(sticky || actface) {
			mf= (MFace*)me->mface;
			tf= (TFace*)me->tface;
			/* deselect */
			if(selectsticky==0) {
				for(a=me->totface; a>0; a--, tf++, mf++) {
					if(!(tf->flag & TF_SELECT)) continue;
					if(nearesttf && tf!=nearesttf) tf->flag &=~ TF_ACTIVE;
					if (!sticky) continue;

					if(msel_hit(limit, hitv, mf->v1, hituv, tf->uv[0]))
						tf->flag &= ~TF_SEL1;
					if(msel_hit(limit, hitv, mf->v2, hituv, tf->uv[1]))
						tf->flag &= ~TF_SEL2;
					if(msel_hit(limit, hitv, mf->v3, hituv, tf->uv[2]))
						tf->flag &= ~TF_SEL3;
					if (mf->v4)
						if(msel_hit(limit, hitv, mf->v4, hituv, tf->uv[3]))
							tf->flag &= ~TF_SEL4;
				}
			}
			/* select */
			else {
				for(a=me->totface; a>0; a--, tf++, mf++) {
					if(!(tf->flag & TF_SELECT)) continue;
					if(nearesttf && tf!=nearesttf)
						tf->flag &=~ TF_ACTIVE;
					if (!sticky) continue;

					if(msel_hit(limit, hitv, mf->v1, hituv, tf->uv[0]))
						tf->flag |= TF_SEL1;
					if(msel_hit(limit, hitv, mf->v2, hituv, tf->uv[1]))
						tf->flag |= TF_SEL2;
					if(msel_hit(limit, hitv, mf->v3, hituv, tf->uv[2]))
						tf->flag |= TF_SEL3;
					if (mf->v4)
						if(msel_hit(limit, hitv, mf->v4, hituv, tf->uv[3]))
							tf->flag |= TF_SEL4;
				}
			}
		}
	}
	else {
		/* select face and deselect other faces */ 
		if(actface) {
			mf= (MFace*)me->mface;
			tf= (TFace*)me->tface;
			for(a=me->totface; a>0; a--, tf++, mf++) {
				tf->flag &= ~(TF_SEL1|TF_SEL2|TF_SEL3|TF_SEL4);
				if(nearesttf && tf!=nearesttf)
					tf->flag &= ~TF_ACTIVE;
			}
			if(nearesttf)
				nearesttf->flag |= (TF_SEL1|TF_SEL2|TF_SEL3|TF_SEL4);
		}

		/* deselect uvs, and select sticky uvs */
		mf= (MFace*)me->mface;
		tf= (TFace*)me->tface;
		for(a=me->totface; a>0; a--, tf++, mf++) {
			if(tf->flag & TF_SELECT) {
				if(!actface) tf->flag &= ~(TF_SEL1|TF_SEL2|TF_SEL3|TF_SEL4);
				if(!sticky) continue;

				if(msel_hit(limit, hitv, mf->v1, hituv, tf->uv[0]))
					tf->flag |= TF_SEL1;
				if(msel_hit(limit, hitv, mf->v2, hituv, tf->uv[1]))
					tf->flag |= TF_SEL2;
				if(msel_hit(limit, hitv, mf->v3, hituv, tf->uv[2]))
					tf->flag |= TF_SEL3;
				if(mf->v4)
					if(msel_hit(limit, hitv, mf->v4, hituv, tf->uv[3]))
						tf->flag |= TF_SEL4;
			}
		}
		
		if(!actface) 
			nearesttf->flag |= TF_SEL_MASK(nearestuv);
	}
	
	force_draw(1);
	
	BIF_undo_push("Select UV");
	rightmouse_transform();
}

void borderselect_sima(void)
{
	Mesh *me;
	TFace *tface;
	MFace *mface;
	rcti rect;
	rctf rectf;
	int a, val;
	short mval[2];

	if( is_uv_tface_editing_allowed()==0 ) return;
	me= get_mesh(OBACT);

	val= get_border(&rect, 3);

	if(val) {
		mval[0]= rect.xmin;
		mval[1]= rect.ymin;
		areamouseco_to_ipoco(G.v2d, mval, &rectf.xmin, &rectf.ymin);
		mval[0]= rect.xmax;
		mval[1]= rect.ymax;
		areamouseco_to_ipoco(G.v2d, mval, &rectf.xmax, &rectf.ymax);

		mface= me->mface;
		for(a=me->totface, tface= me->tface; a>0; a--, tface++, mface++) {
		
			if(tface->flag & TF_SELECT) {
				
				if(BLI_in_rctf(&rectf, (float)tface->uv[0][0], (float)tface->uv[0][1])) {
					if(val==LEFTMOUSE) tface->flag |= TF_SEL1;
					else tface->flag &= ~TF_SEL1;
				}
				if(BLI_in_rctf(&rectf, (float)tface->uv[1][0], (float)tface->uv[1][1])) {
					if(val==LEFTMOUSE) tface->flag |= TF_SEL2;
					else tface->flag &= ~TF_SEL2;
				}
				if(BLI_in_rctf(&rectf, (float)tface->uv[2][0], (float)tface->uv[2][1])) {
					if(val==LEFTMOUSE) tface->flag |= TF_SEL3;
					else tface->flag &= ~TF_SEL3;
				}
				if(mface->v4 && BLI_in_rctf(&rectf, (float)tface->uv[3][0], (float)tface->uv[3][1])) {
					if(val==LEFTMOUSE) tface->flag |= TF_SEL4;
					else tface->flag &= ~TF_SEL4;
				}
			}
							
		}
		BIF_undo_push("Border select UV");
		scrarea_queue_winredraw(curarea);
	}
}

/** This is an ugly function to set the Tface selection flags depending
  * on whether its UV coordinates are inside the normalized 
  * area with radius rad and offset offset. These coordinates must be
  * normalized to 1.0 
  * Just for readability...
  */

void sel_uvco_inside_radius(short sel, TFace *tface, int index, float *offset, float *ell, short select_mask)
{
	// normalized ellipse: ell[0] = scaleX,
	//                        [1] = scaleY

	float *uv = tface->uv[index];
	float x, y, r2;

	x = (uv[0] - offset[0]) * ell[0];
	y = (uv[1] - offset[1]) * ell[1];

	r2 = x * x + y * y;
	if (r2 < 1.0) {
		if (sel == LEFTMOUSE) tface->flag |= select_mask;
		else tface->flag &= ~select_mask;
	}
}

// see below:
/** gets image dimensions of the 2D view 'v' */
static void getSpaceImageDimension(SpaceImage *sima, float *xy)
{
	Image *img = sima->image;
	float z;

	z = sima->zoom;

	if (img) {
		xy[0] = img->ibuf->x * z;
		xy[1] = img->ibuf->y * z;
	} else {
		xy[0] = 256 * z;
		xy[1] = 256 * z;
	}
}

/** Callback function called by circle_selectCB to enable 
  * brush select in UV editor.
  */

void uvedit_selectionCB(short selecting, Object *editobj, short *mval, float rad) 
{
	float offset[2];
	Mesh *me;
	MFace *mface;
	TFace *tface;
	int i;

	float ellipse[2]; // we need to deal with ellipses, as
	                  // non square textures require for circle
					  // selection. this ellipse is normalized; r = 1.0
	
	me = get_mesh(editobj);

	getSpaceImageDimension(curarea->spacedata.first, ellipse);
	ellipse[0] /= rad;
	ellipse[1] /= rad;

	areamouseco_to_ipoco(G.v2d, mval, &offset[0], &offset[1]);

	mface= me->mface;
	tface= me->tface;

	if (selecting) {
		for(i = 0; i < me->totface; i++) {
			sel_uvco_inside_radius(selecting, tface, 0, offset, ellipse, TF_SEL1);
			sel_uvco_inside_radius(selecting, tface, 1, offset, ellipse, TF_SEL2);
			sel_uvco_inside_radius(selecting, tface, 2, offset, ellipse, TF_SEL3);
			if (mface->v4)
				sel_uvco_inside_radius(selecting, tface, 3, offset, ellipse, TF_SEL4);
			
			tface++; mface++;

		}

		if(G.f & G_DRAWFACES) { /* full redraw only if necessary */
			draw_sel_circle(0, 0, 0, 0, 0); /* signal */
			force_draw(0);
		}
		else { /* force_draw() is no good here... */
			glDrawBuffer(GL_FRONT);
			draw_tfaces();
			glDrawBuffer(GL_BACK);
		}
	}	
}


void mouseco_to_curtile(void)
{
	float fx, fy;
	short mval[2];
	
	if( is_uv_tface_editing_allowed()==0) return;

	if(G.sima->image && G.sima->image->tpageflag & IMA_TILES) {
		
		G.sima->flag |= SI_EDITTILE;
		
		while(get_mbut()&L_MOUSE) {
			
			calc_image_view(G.sima, 'f');
			
			getmouseco_areawin(mval);
			areamouseco_to_ipoco(G.v2d, mval, &fx, &fy);

			if(fx>=0.0 && fy>=0.0 && fx<1.0 && fy<1.0) {
			
				fx= (fx)*G.sima->image->xrep;
				fy= (fy)*G.sima->image->yrep;
				
				mval[0]= fx;
				mval[1]= fy;
				
				G.sima->curtile= mval[1]*G.sima->image->xrep + mval[0];
			}

			scrarea_do_windraw(curarea);
			screen_swapbuffers();
		}
		
		G.sima->flag &= ~SI_EDITTILE;

		image_changed(G.sima, 1);

		allqueue(REDRAWVIEW3D, 0);
		scrarea_queue_winredraw(curarea);
	}
}

void hide_tface_uv(int swap)
{
	Mesh *me;
	TFace *tface;
	MFace *mface;
	int a;

	if( is_uv_tface_editing_allowed()==0 ) return;
	me= get_mesh(OBACT);

	if(swap) {
		mface= me->mface;
		for(a=me->totface, tface= me->tface; a>0; a--, tface++, mface++) {
			if(tface->flag & TF_SELECT) {
				if((tface->flag & (TF_SEL1|TF_SEL2|TF_SEL3))==0) {
					if(!mface->v4)
						tface->flag &= ~TF_SELECT;
					else if(!(tface->flag & TF_SEL4))
						tface->flag &= ~TF_SELECT;
				}
			}
		}
	} else {
		mface= me->mface;
		for(a=me->totface, tface= me->tface; a>0; a--, tface++, mface++) {
			if(tface->flag & TF_SELECT) {
				if(tface->flag & (TF_SEL1|TF_SEL2|TF_SEL3))
						tface->flag &= ~TF_SELECT;
				else if(mface->v4 && tface->flag & TF_SEL4)
						tface->flag &= ~TF_SELECT;
			}
		}
	}

	BIF_undo_push("Hide UV");

	object_tface_flags_changed(OBACT, 0);
}

void reveal_tface_uv(void)
{
	Mesh *me;
	TFace *tface;
	MFace *mface;
	int a;

	if( is_uv_tface_editing_allowed()==0 ) return;
	me= get_mesh(OBACT);

	mface= me->mface;
	for(a=me->totface, tface= me->tface; a>0; a--, tface++, mface++)
		if(!(tface->flag & TF_HIDE))
			if(!(tface->flag & TF_SELECT))
				tface->flag |= (TF_SELECT|TF_SEL1|TF_SEL2|TF_SEL3|TF_SEL4);
	
	BIF_undo_push("Reveal UV");

	object_tface_flags_changed(OBACT, 0);
}

void stitch_uv_tface(int mode)
{
	MFace *mf;
	TFace *tf, *tface;
	Mesh *me;
	int a, b, c, tot, vtot, vtot2;
	float newuv[2], limit[2], *uv, *uv1;
	struct uvvertsort *sortblock, *sb, *sb1, *sb2;
	
	if( is_uv_tface_editing_allowed()==0 ) return;

	limit[0]= limit[1]= 20.0;
	if(mode==1) {
		add_numbut(0, NUM|FLO, "Limit:", 0.1, 1000.0, &limit[0], NULL);
		if (!do_clever_numbuts("Stitch UVs", 1, REDRAW))
			return;
	}

	if(G.sima->image && G.sima->image->ibuf && G.sima->image->ibuf->x > 0 &&
	   G.sima->image->ibuf->y > 0) {
		limit[1]= limit[0]/(float)G.sima->image->ibuf->y;
		limit[0]= limit[0]/(float)G.sima->image->ibuf->x;
	}
	else
		limit[0]= limit[1]= limit[0]/256.0;
	
	me= get_mesh(OBACT);
	tface= (TFace*)me->tface;
	
	tot= 0;
	mf= (MFace*)me->mface;
	tf= (TFace*)me->tface;
	for(a=me->totface; a>0; a--, tf++, mf++) {
		if((tf->flag & TF_SELECT)) {
			if(tf->flag & TF_SEL1) tot++;
			if(tf->flag & TF_SEL2) tot++;
			if(tf->flag & TF_SEL3) tot++;
			if(mf->v4 && tf->flag & TF_SEL4) tot++; 
		}
	}
	if(tot==0) return;

	sb= sortblock= MEM_callocN(sizeof(struct uvvertsort)*tot,"sortstitchuv");

	mf= (MFace*)me->mface;
	tf= (TFace*)me->tface;
	for(a=0; a<me->totface; a++, tf++, mf++) {
		if((tf->flag & TF_SELECT)) {
			if(tf->flag & TF_SEL1) {
				sb->v= mf->v1;
				sb->f= a;
				sb->tf_sel= 0;
				sb++;
			}
			if(tf->flag & TF_SEL2) {
				sb->v= mf->v2;
				sb->f= a;
				sb->tf_sel= 1;
				sb++;
			}
			if(tf->flag & TF_SEL3) {
				sb->v= mf->v3;
				sb->f= a;
				sb->tf_sel= 2;
				sb++;
			}
			if(mf->v4 && tf->flag & TF_SEL4) {
				sb->v= mf->v4;
				sb->f= a;
				sb->tf_sel= 3;
				sb++;
			}
		}
	}
	
	/* sort by vertex */
	qsort(sortblock, tot, sizeof(struct uvvertsort), compuvvert);

	if(mode==0) {
		for (a=0, sb=sortblock; a<tot; a+=vtot, sb+=vtot) {
			newuv[0]= 0; newuv[1]= 0;
			vtot= 0;

			for (b=a, sb1=sb; b<tot && sb1->v==sb->v; b++, sb1++) {
				newuv[0] += tface[sb1->f].uv[sb1->tf_sel][0];
				newuv[1] += tface[sb1->f].uv[sb1->tf_sel][1];
				vtot++;
			}

			newuv[0] /= vtot; newuv[1] /= vtot;

			for (b=a, sb1=sb; b<a+vtot; b++, sb1++) {
				tface[sb1->f].uv[sb1->tf_sel][0]= newuv[0];
				tface[sb1->f].uv[sb1->tf_sel][1]= newuv[1];
			}
		}
	} else if(mode==1) {
		for (a=0, sb=sortblock; a<tot; a+=vtot, sb+=vtot) {
			vtot= 0;
			for (b=a, sb1=sb; b<tot && sb1->v==sb->v; b++, sb1++)
				vtot++;

			for (b=a, sb1=sb; b<a+vtot; b++, sb1++) {
				if(sb1->flag & 2) continue;

				newuv[0]= 0; newuv[1]= 0;
				vtot2 = 0;

				for (c=b, sb2=sb1; c<a+vtot; c++, sb2++) {
					uv = tface[sb2->f].uv[sb2->tf_sel];
					uv1 = tface[sb1->f].uv[sb1->tf_sel];
					if (fabs(uv[0]-uv1[0]) < limit[0] &&
					    fabs(uv[1]-uv1[1]) < limit[1]) {
						newuv[0] += uv[0];
						newuv[1] += uv[1];
						sb2->flag |= 2;
						sb2->flag |= 4;
						vtot2++;
					}
				}

				newuv[0] /= vtot2; newuv[1] /= vtot2;

				for (c=b, sb2=sb1; c<a+vtot; c++, sb2++) {
					if(sb2->flag & 4) {
						tface[sb2->f].uv[sb2->tf_sel][0]= newuv[0];
						tface[sb2->f].uv[sb2->tf_sel][1]= newuv[1];
						sb2->flag &= ~4;
					}
				}
			}
		}
	}
	MEM_freeN(sortblock);

	if(G.sima->flag & SI_BE_SQUARE) be_square_tface_uv(me);

	BIF_undo_push("Stitch UV");

	object_uvs_changed(OBACT);
}

void select_linked_tface_uv(int mode)
{
	MFace *mf;
	TFace *tface, *tf, *nearesttf=NULL;
	Mesh *me;
	char sel, *linkflag;
	int a, b, c, tot, vtot, nearestv, nearestuv, i, nverts;
	float limit[2], *uv, *uv1;
	struct uvvertsort *sortblock, *sb, *sb1, *sb2;
	
	if( is_uv_tface_editing_allowed()==0 ) return;

	me= get_mesh(OBACT);
	get_connected_limit_tface_uv(limit);

	tot= 0;
	mf= (MFace*)me->mface;
	tf= (TFace*)me->tface;
	for(a=me->totface; a>0; a--, tf++, mf++)
		if(tf->flag & TF_SELECT)
			tot += mf->v4? 4: 3;

	if(tot==0) return;

	if (mode!=2) {
		find_nearest_uv(&nearesttf, &nearestv, &nearestuv);

		if(nearesttf==NULL)
			return;
	}
	else {
		nearesttf= NULL;
		nearestuv= 0;
	}

	sb= sortblock= MEM_callocN(sizeof(struct uvvertsort)*tot, "sortsellinkuv");
	linkflag= MEM_callocN(sizeof(char)*me->totface, "linkflaguv");

	mf= (MFace*)me->mface;
	tf= (TFace*)me->tface;
	for(a=0; a<me->totface; a++, tf++, mf++) {
		if(!(tf->flag & TF_HIDE) && (tf->flag & TF_SELECT)) {
			sel= 0;
			sb1= sb;
			nverts= mf->v4? 4: 3;
			for(i=0; i<nverts; i++) {
				if(tf->flag & TF_SEL_MASK(i))
					sel= 1;
				sb->f= a;
				sb->tf_sel= i;
				sb++;
			}

			if(nearesttf==tf || ((sel && mode==2)))
				linkflag[a] = 1;

			(sb1)->v= mf->v1;
			(sb1+1)->v= mf->v2;
			(sb1+2)->v= mf->v3;
			if(mf->v4) (sb1+3)->v= mf->v4;
		}
	}
	
	/* sort by vertex */
	qsort(sortblock, tot, sizeof(struct uvvertsort), compuvvert);

	tface= (TFace*)me->tface;
	sel= 1;
	while(sel) {
		sel= 0;

		/* select all tex vertices that are near a selected tex vertex */
		for (a=0, sb=sortblock; a<tot; a+=vtot, sb+=vtot) {
			vtot= 0;
			for (b=a, sb1=sb; b<tot && sb1->v==sb->v; b++, sb1++)
				vtot++;
			for (b=a, sb1=sb; b<a+vtot; b++, sb1++) {
				if(linkflag[sb1->f]) continue;

				for (c=a, sb2=sb; c<a+vtot; c++, sb2++) {
					if(!(linkflag[sb2->f])) continue;
					
					uv = tface[sb2->f].uv[sb2->tf_sel];
					uv1 = tface[sb1->f].uv[sb1->tf_sel];
					if (fabs(uv[0]-uv1[0]) < limit[0] &&
					    fabs(uv[1]-uv1[1]) < limit[1]) {
						linkflag[sb1->f] = 1;
						sel= 1;
						break;
					}
				}
			}
		}
	}

	if(mode==0 || mode==2) {
		for(a=0, tf=tface; a<me->totface; a++, tf++)
			if(linkflag[a])
				tf->flag |= (TF_SEL1|TF_SEL2|TF_SEL3|TF_SEL4);
			else
				tf->flag &= ~(TF_SEL1|TF_SEL2|TF_SEL3|TF_SEL4);
	}
	else if(mode==1) {
		for(a=0, tf=tface; a<me->totface; a++, tf++) {
			if(linkflag[a]) {
				if (mf->v4) {
					if((tf->flag & (TF_SEL1|TF_SEL2|TF_SEL3|TF_SEL4)))
						break;
				}
				else if(tf->flag & (TF_SEL1|TF_SEL2|TF_SEL3))
					break;
			}
		}

		if (a<me->totface) {
			for(a=0, tf=tface; a<me->totface; a++, tf++)
				if(linkflag[a])
					tf->flag &= ~(TF_SEL1|TF_SEL2|TF_SEL3|TF_SEL4);
		}
		else {
			for(a=0, tf=tface; a<me->totface; a++, tf++)
				if(linkflag[a])
					tf->flag |= (TF_SEL1|TF_SEL2|TF_SEL3|TF_SEL4);
		}
	}
	
	MEM_freeN(sortblock);
	MEM_freeN(linkflag);

	BIF_undo_push("Select linked UV");
	scrarea_queue_winredraw(curarea);
}

void unlink_selection(void)
{
	Mesh *me;
	TFace *tface;
	MFace *mface;
	int a;

	if( is_uv_tface_editing_allowed()==0 ) return;
	me= get_mesh(OBACT);

	mface= me->mface;
	for(a=me->totface, tface= me->tface; a>0; a--, tface++, mface++) {
		if(tface->flag & TF_SELECT) {
			if(mface->v4) {
				if(~tface->flag & (TF_SEL1|TF_SEL2|TF_SEL3|TF_SEL4))
					tface->flag &= ~(TF_SEL1|TF_SEL2|TF_SEL3|TF_SEL4);
			} else {
				if(~tface->flag & (TF_SEL1|TF_SEL2|TF_SEL3))
					tface->flag &= ~(TF_SEL1|TF_SEL2|TF_SEL3);
			}
		}
	}
	
	BIF_undo_push("Unlink UV selection");
	scrarea_queue_winredraw(curarea);
}

void toggle_uv_select(int mode)
{
	switch(mode){
	case 'f':
		G.sima->flag ^= SI_SELACTFACE;
		break;
	case 's':
		G.sima->flag ^= SI_STICKYUVS; 
		if (G.sima->flag & SI_STICKYUVS) G.sima->flag &= ~SI_LOCALSTICKY;
		break;
	case 'l': 
		 G.sima->flag ^= SI_LOCALSTICKY;
		 if (G.sima->flag & SI_LOCALSTICKY) G.sima->flag &= ~SI_STICKYUVS;
		break;
	case 'o':
		G.sima->flag &= ~SI_STICKYUVS; 
		G.sima->flag &= ~SI_LOCALSTICKY;
		break;
	}
	allqueue(REDRAWIMAGE, 0);
}

void pin_tface_uv(int mode)
{
	Mesh *me;
	TFace *tface;
	MFace *mface;
	int a;
	
	if( is_uv_tface_editing_allowed()==0 ) return;
	me= get_mesh(OBACT);
	
	mface= me->mface;
	tface= me->tface;
	for(a=me->totface; a>0; a--, tface++, mface++) {
		if(tface->flag & TF_SELECT) {
			if(mode ==1){
				if(tface->flag & TF_SEL1) tface->unwrap |= TF_PIN1;
				if(tface->flag & TF_SEL2) tface->unwrap |= TF_PIN2;
				if(tface->flag & TF_SEL3) tface->unwrap |= TF_PIN3;
				if(mface->v4)
					if(tface->flag & TF_SEL4) tface->unwrap |= TF_PIN4;
			}
			else if (mode ==0){
				if(tface->flag & TF_SEL1) tface->unwrap &= ~TF_PIN1;
				if(tface->flag & TF_SEL2) tface->unwrap &= ~TF_PIN2;
				if(tface->flag & TF_SEL3) tface->unwrap &= ~TF_PIN3;
				if(mface->v4)
				if(tface->flag & TF_SEL4) tface->unwrap &= ~TF_PIN4;
			}
		}
	}
	
	BIF_undo_push("Pin UV");
	scrarea_queue_winredraw(curarea);
}

int minmax_tface_uv(float *min, float *max)
{
	Mesh *me;
	TFace *tf;
	MFace *mf;
	int a, sel;

	if( is_uv_tface_editing_allowed()==0 ) return 0;
	me= get_mesh(OBACT);

	INIT_MINMAX2(min, max);

	sel= 0;
	mf= (MFace*)me->mface;
	tf= (TFace*)me->tface;
	for(a=me->totface; a>0; a--, tf++, mf++) {
		if(tf->flag & TF_HIDE);
		else if(tf->flag & TF_SELECT) {

			if (tf->flag & TF_SEL1) {
				DO_MINMAX2(tf->uv[0], min, max);
			}
			if (tf->flag & TF_SEL2) {
				DO_MINMAX2(tf->uv[1], min, max);
			}
			if (tf->flag & TF_SEL3) {
				DO_MINMAX2(tf->uv[2], min, max);
			}
			if (mf->v4 && tf->flag & TF_SEL4) {
				DO_MINMAX2(tf->uv[3], min, max);
			}

			sel = 1;
		}
	}

	return sel;
}

