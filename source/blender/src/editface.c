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


#include <math.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
#include "BLI_winstuff.h"
#endif
#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"

#include "MTC_matrixops.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "DNA_image_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_space_types.h"
#include "DNA_screen_types.h"
#include "DNA_view3d_types.h"

#include "BKE_utildefines.h"
#include "BKE_displist.h"
#include "BKE_global.h"
#include "BKE_mesh.h"
#include "BKE_texture.h"

#include "BSE_view.h"
#include "BSE_edit.h"
#include "BSE_drawview.h"	/* for backdrawview3d */

#include "BIF_interface.h"
#include "BIF_mywindow.h"
#include "BIF_toolbox.h"
#include "BIF_screen.h"
#include "BIF_gl.h"
#include "BIF_graphics.h"
#include "BIF_space.h"	/* for allqueue */

#include "BDR_drawmesh.h"
#include "BDR_editface.h"
#include "BDR_vpaint.h"

#include "BDR_editface.h"
#include "BDR_vpaint.h"

#include "mydevice.h"
#include "blendef.h"
#include "render.h"
#include "butspace.h"

#include "TPT_DependKludge.h"

#ifdef NAN_TPT
#include "../img/IMG_Api.h"
#include "BSE_trans_types.h"
#endif /* NAN_TPT */

TFace *lasttface=0;

static void uv_calc_center_vector(float *result, Object *ob, Mesh *me)
{
	float min[3], max[3], *cursx;
	int a;
	TFace *tface;
	MFace *mface;

	switch (G.vd->around) 
	{
	case V3D_CENTRE: /* bounding box center */
		min[0]= min[1]= min[2]= 1e20f;
		max[0]= max[1]= max[2]= -1e20f; 

		tface= me->tface;
		mface= me->mface;
		for(a=0; a<me->totface; a++, mface++, tface++) {
			if(tface->flag & TF_SELECT && mface->v3) {
				DO_MINMAX((me->mvert+mface->v1)->co, min, max);
				DO_MINMAX((me->mvert+mface->v2)->co, min, max);
				DO_MINMAX((me->mvert+mface->v3)->co, min, max);
				if(mface->v4) DO_MINMAX((me->mvert+mface->v4)->co, min, max);
			}
		}
		VecMidf(result, min, max);
		break;
	case V3D_CURSOR: /*cursor center*/ 
		cursx= give_cursor();
		/* shift to objects world */
		result[0]= cursx[0]-ob->obmat[3][0];
		result[1]= cursx[1]-ob->obmat[3][1];
		result[2]= cursx[2]-ob->obmat[3][2];
		break;
	case V3D_LOCAL: /*object center*/
	case V3D_CENTROID: /* multiple objects centers, only one object here*/
	default:
		result[0]= result[1]= result[2]= 0.0;
		break;
	}
}

static void uv_calc_map_matrix(float result[][4], Object *ob, float upangledeg, float sideangledeg, float radius)
{
	float rotup[4][4], rotside[4][4], viewmatrix[4][4], rotobj[4][4];
	float sideangle= 0.0, upangle= 0.0;
	int k;

	/* get rotation of the current view matrix */
	Mat4CpyMat4(viewmatrix,G.vd->viewmat);
	/* but shifting */
	for( k= 0; k< 4; k++) viewmatrix[3][k] =0.0;

	/* get rotation of the current object matrix */
	Mat4CpyMat4(rotobj,ob->obmat);
	/* but shifting */
	for( k= 0; k< 4; k++) rotobj[3][k] =0.0;

	Mat4Clr(*rotup);
	Mat4Clr(*rotside);

	/* compensate front/side.. against opengl x,y,z world definition */
	/* this is "kanonen gegen spatzen", a few plus minus 1 will do here */
	/* i wanted to keep the reason here, so we're rotating*/
	sideangle= M_PI * (sideangledeg + 180.0) /180.0;
	rotside[0][0]= (float)cos(sideangle);
	rotside[0][1]= -(float)sin(sideangle);
	rotside[1][0]= (float)sin(sideangle);
	rotside[1][1]= (float)cos(sideangle);
	rotside[2][2]= 1.0f;
      
	upangle= M_PI * upangledeg /180.0;
	rotup[1][1]= (float)cos(upangle)/radius;
	rotup[1][2]= -(float)sin(upangle)/radius;
	rotup[2][1]= (float)sin(upangle)/radius;
	rotup[2][2]= (float)cos(upangle)/radius;
	rotup[0][0]= (float)1.0/radius;

	/* calculate transforms*/
	Mat4MulSerie(result,rotup,rotside,viewmatrix,rotobj,NULL,NULL,NULL,NULL);
}

static void uv_calc_shift_project(float *target, float *shift, float rotmat[][4], int projectionmode, float *source, float *min, float *max)
{
	float pv[3];

	VecSubf(pv, source, shift);
	Mat4MulVecfl(rotmat, pv);

	switch(projectionmode) {
	case B_UVAUTO_CYLINDER: 
		tubemap(pv[0], pv[1], pv[2], &target[0],&target[1]);
		/* split line is always zero */
		if (target[0] >= 1.0f) target[0] -= 1.0f;  
		break;

	case B_UVAUTO_SPHERE: 
		spheremap(pv[0], pv[1], pv[2], &target[0],&target[1]);
		/* split line is always zero */
		if (target[0] >= 1.0f) target[0] -= 1.0f;
		break;

	case 3: /* ortho special case for BOUNDS */
		target[0] = -pv[0];
		target[1] = pv[2];
		break;

	case 4: 
		{
		/* very special case for FROM WINDOW */
		float pv4[4], dx, dy, x= 0.0, y= 0.0;

		dx= G.vd->area->winx;
		dy= G.vd->area->winy;

		VecCopyf(pv4, source);
        pv4[3] = 1.0;

		/* rotmat is the object matrix in this case */
        Mat4MulVec4fl(rotmat,pv4); 

		/* almost project_short */
	    Mat4MulVec4fl(G.vd->persmat,pv4);
		if (fabs(pv[3]) > 0.00001) { /* avoid division by zero */
			target[0] = dx/2.0 + (dx/2.0)*pv4[0]/pv4[3];
			target[1] = dy/2.0 + (dy/2.0)*pv4[1]/pv4[3];
		}
		else {
			/* scaling is lost but give a valid result */
			target[0] = dx/2.0 + (dx/2.0)*pv4[0];
			target[1] = dy/2.0 + (dy/2.0)*pv4[1];
		}

        /* G.vd->persmat seems to do this funky scaling */ 
		if(dx > dy) {
			y= (dx-dy)/2.0;
			dy = dx;
		}
		else {
			x= (dy-dx)/2.0;
			dx = dy;
		}
		target[0]= (x + target[0])/dx;
		target[1]= (y + target[1])/dy;

		}
		break;

    default:
		target[0] = 0.0;
		target[1] = 1.0;
	}

	/* we know the values here and may need min_max later */
	/* max requests independand from min; not fastest but safest */ 
	if(min) {
		min[0] = MIN2(target[0], min[0]);
		min[1] = MIN2(target[1], min[1]);
	}
	if(max) {
		max[0] = MAX2(target[0], max[0]);
		max[1] = MAX2(target[1], max[1]);
	}
}

void calculate_uv_map(unsigned short mapmode)
{
	Mesh *me;
	TFace *tface;
	MFace *mface;
	Object *ob;
	float dx, dy, rotatematrix[4][4], radius= 1.0, min[3], cent[3], max[3];
	float fac= 1.0, upangledeg= 0.0, sideangledeg= 90.0;
	int i, b, mi, a, n;
	/* settings from buttonswindow */
	extern float uv_calc_radius, uv_calc_cubesize;
	extern short uv_calc_mapdir, uv_calc_mapalign;

	if(uv_calc_mapdir==1)  {
		upangledeg= 90.0;
		sideangledeg= 0.0;
	}
	else {
		upangledeg= 0.0;
		if(uv_calc_mapalign==1) sideangledeg= 0.0;
		else sideangledeg= 90.0;
	}

	me= get_mesh(ob=OBACT);
	if(me==0 || me->tface==0) return;
	if(me->totface==0) return;
	
	switch(mapmode) {
	case B_UVAUTO_BOUNDS1:
	case B_UVAUTO_BOUNDS2:
	case B_UVAUTO_BOUNDS4:
	case B_UVAUTO_BOUNDS8:
		switch(mapmode) {
		case B_UVAUTO_BOUNDS2: fac = 0.5; break;
		case B_UVAUTO_BOUNDS4: fac = 0.25; break;
		case B_UVAUTO_BOUNDS8: fac = 0.125; break;
		}

		min[0]= min[1]= 1.0;
		max[0]= max[1]= 0.0;

		uv_calc_center_vector(cent, ob, me);
		uv_calc_map_matrix(rotatematrix, ob, upangledeg, sideangledeg, 1.0f);
			
		tface= me->tface;
		mface= me->mface;
		for(a=0; a<me->totface; a++, mface++, tface++) {
			if(tface->flag & TF_SELECT && mface->v3) {
				uv_calc_shift_project(tface->uv[0],cent,rotatematrix,3,(me->mvert+mface->v1)->co,min,max);
				uv_calc_shift_project(tface->uv[1],cent,rotatematrix,3,(me->mvert+mface->v2)->co,min,max);
				uv_calc_shift_project(tface->uv[2],cent,rotatematrix,3,(me->mvert+mface->v3)->co,min,max);
				if(mface->v4)
					uv_calc_shift_project(tface->uv[3],cent,rotatematrix,3,(me->mvert+mface->v4)->co,min,max);
			}
		}
		
		/* rescale UV to be in 0..1,1/2,1/4,1/8 */
		dx= (max[0]-min[0]);
		dy= (max[1]-min[1]);

		tface= me->tface;
		mface= me->mface;
		for(a=0; a<me->totface; a++, mface++, tface++) {
			if(tface->flag & TF_SELECT && mface->v3) {
				if(mface->v4) b= 3; else b= 2;
				for(; b>=0; b--) {
					tface->uv[b][0]= ((tface->uv[b][0]-min[0])*fac)/dx;
					tface->uv[b][1]= 1.0-fac+((tface->uv[b][1]-min[1])*fac)/dy;
				}
			}
		}
		break;

	case B_UVAUTO_WINDOW:		
		cent[0] = cent[1] = cent[2] = 0.0; 
		Mat4CpyMat4(rotatematrix,ob->obmat);

		tface= me->tface;
		mface= me->mface;
		for(a=0; a<me->totface; a++, mface++, tface++) {
			if(tface->flag & TF_SELECT && mface->v3) {
				uv_calc_shift_project(tface->uv[0],cent,rotatematrix,4,(me->mvert+mface->v1)->co,NULL,NULL);
				uv_calc_shift_project(tface->uv[1],cent,rotatematrix,4,(me->mvert+mface->v2)->co,NULL,NULL);
				uv_calc_shift_project(tface->uv[2],cent,rotatematrix,4,(me->mvert+mface->v3)->co,NULL,NULL);
				if(mface->v4)
					uv_calc_shift_project(tface->uv[3],cent,rotatematrix,4,(me->mvert+mface->v4)->co,NULL,NULL);
			}
		}
		break;

	case B_UVAUTO_STD8:
	case B_UVAUTO_STD4:
	case B_UVAUTO_STD2:
	case B_UVAUTO_STD1:
		switch(mapmode) {
		case B_UVAUTO_STD8: fac = 0.125; break;
		case B_UVAUTO_STD4: fac = 0.25; break;
		case B_UVAUTO_STD2: fac = 0.5; break;
		}

		tface= me->tface;
		for(a=0; a<me->totface; a++, tface++)
			if(tface->flag & TF_SELECT) 
				default_uv(tface->uv, fac);
		break;

	case B_UVAUTO_CYLINDER:
	case B_UVAUTO_SPHERE:
		uv_calc_center_vector(cent, ob, me);
			
		if(mapmode==B_UVAUTO_CYLINDER) radius = uv_calc_radius;

		/* be compatible to the "old" sphere/cylinder mode */
		if (uv_calc_mapdir== 2)
			Mat4One(rotatematrix);
		else 
			uv_calc_map_matrix(rotatematrix,ob,upangledeg,sideangledeg,radius);

		tface= me->tface;
		mface= me->mface;
		for(a=0; a<me->totface; a++, mface++, tface++) {
			if(tface->flag & TF_SELECT && mface->v3) {
				uv_calc_shift_project(tface->uv[0],cent,rotatematrix,mapmode,(me->mvert+mface->v1)->co,NULL,NULL);
				uv_calc_shift_project(tface->uv[1],cent,rotatematrix,mapmode,(me->mvert+mface->v2)->co,NULL,NULL);
				uv_calc_shift_project(tface->uv[2],cent,rotatematrix,mapmode,(me->mvert+mface->v3)->co,NULL,NULL);
				n = 3;       
				if(mface->v4) {
					uv_calc_shift_project(tface->uv[3],cent,rotatematrix,mapmode,(me->mvert+mface->v4)->co,NULL,NULL);
					n=4;
				}

				mi = 0;
				for (i = 1; i < n; i++)
					if (tface->uv[i][0] > tface->uv[mi][0]) mi = i;

				for (i = 0; i < n; i++) {
					if (i != mi) {
						dx = tface->uv[mi][0] - tface->uv[i][0];
						if (dx > 0.5) tface->uv[i][0] += 1.0;
					} 
				} 
			}
		}

		break;

	case B_UVAUTO_CUBE:
		{
		/* choose x,y,z axis for projetion depending on the largest normal */
		/* component, but clusters all together around the center of map */
		float no[3];
		short cox, coy;
		float *loc= ob->obmat[3];
		MVert *mv= me->mvert;

		tface= me->tface;
		mface= me->mface;
		for(a=0; a<me->totface; a++, mface++, tface++) {
			if(tface->flag & TF_SELECT && mface->v3) {
				CalcNormFloat((mv+mface->v1)->co, (mv+mface->v2)->co, (mv+mface->v3)->co, no);
					
				no[0]= fabs(no[0]);
				no[1]= fabs(no[1]);
				no[2]= fabs(no[2]);
				
				cox=0; coy= 1;
				if(no[2]>=no[0] && no[2]>=no[1]);
				else if(no[1]>=no[0] && no[1]>=no[2]) coy= 2;
				else { cox= 1; coy= 2; }
				
				tface->uv[0][0]= 0.5+0.5*uv_calc_cubesize*(loc[cox] + (mv+mface->v1)->co[cox]);
				tface->uv[0][1]= 0.5+0.5*uv_calc_cubesize*(loc[coy] + (mv+mface->v1)->co[coy]);
				dx = floor(tface->uv[0][0]);
				dy = floor(tface->uv[0][1]);
				tface->uv[0][0] -= dx;
				tface->uv[0][1] -= dy;
				tface->uv[1][0]= 0.5+0.5*uv_calc_cubesize*(loc[cox] + (mv+mface->v2)->co[cox]);
				tface->uv[1][1]= 0.5+0.5*uv_calc_cubesize*(loc[coy] + (mv+mface->v2)->co[coy]);
				tface->uv[1][0] -= dx;
				tface->uv[1][1] -= dy;
				tface->uv[2][0]= 0.5+0.5*uv_calc_cubesize*(loc[cox] + (mv+mface->v3)->co[cox]);
				tface->uv[2][1]= 0.5+0.5*uv_calc_cubesize*(loc[coy] + (mv+mface->v3)->co[coy]);
				tface->uv[2][0] -= dx;
				tface->uv[2][1] -= dy;
				if(mface->v4) {
					tface->uv[3][0]= 0.5+0.5*uv_calc_cubesize*(loc[cox] + (mv+mface->v4)->co[cox]);
					tface->uv[3][1]= 0.5+0.5*uv_calc_cubesize*(loc[coy] + (mv+mface->v4)->co[coy]);
					tface->uv[3][0] -= dx;
					tface->uv[3][1] -= dy;
				}
			}
		}
		}
		break; 
	default:
		return;
	} /* end switch mapmode */

	/* clipping and wrapping */
	if(G.sima && G.sima->flag & SI_CLIP_UV) {
		tface= me->tface;
		mface= me->mface;
		for(a=0; a<me->totface; a++, mface++, tface++) {
			if(!(tface->flag & TF_SELECT && mface->v3)) return;
				
			dx= dy= 0;
			if(mface->v4) b= 3; else b= 2;
			for(; b>=0; b--) {
				while(tface->uv[b][0] + dx < 0.0) dx+= 0.5;
				while(tface->uv[b][0] + dx > 1.0) dx-= 0.5;
				while(tface->uv[b][1] + dy < 0.0) dy+= 0.5;
				while(tface->uv[b][1] + dy > 1.0) dy-= 0.5;
			}
	
			if(mface->v4) b= 3; else b= 2;
			for(; b>=0; b--) {
				tface->uv[b][0]+= dx;
				CLAMP(tface->uv[b][0], 0.0, 1.0);
				
				tface->uv[b][1]+= dy;
				CLAMP(tface->uv[b][1], 0.0, 1.0);
			}
		}
	}

	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWIMAGE, 0);
}

void set_lasttface()
{
	Mesh *me;
	TFace *tface;
	int a;
	
	lasttface= 0;
	if(OBACT==NULL || OBACT->type!=OB_MESH) return;
	
	me= get_mesh(OBACT);
	if(me==0 || me->tface==0) return;
	
	tface= me->tface;
	a= me->totface;
	while(a--) {
		if(tface->flag & TF_ACTIVE) {
			lasttface= tface;
			return;
		}
		tface++;
	}

	tface= me->tface;
	a= me->totface;
	while(a--) {
		if(tface->flag & TF_SELECT) {
			lasttface= tface;
			return;
		}
		tface++;
	}

	tface= me->tface;
	a= me->totface;
	while(a--) {
		if((tface->flag & TF_HIDE)==0) {
			lasttface= tface;
			return;
		}
		tface++;
	}
}

void default_uv(float uv[][2], float size)
{
	int dy;
	
	if(size>1.0) size= 1.0;

	dy= 1.0-size;
	
	uv[0][0]= 0;
	uv[0][1]= size+dy;
	
	uv[1][0]= 0;
	uv[1][1]= dy;
	
	uv[2][0]= size;
	uv[2][1]= dy;
	
	uv[3][0]= size;
	uv[3][1]= size+dy;
	
	
}

void default_tface(TFace *tface)
{
	default_uv(tface->uv, 1.0);

	tface->col[0]= tface->col[1]= tface->col[2]= tface->col[3]= vpaint_get_current_col();

	tface->mode= TF_TEX;
	tface->mode= 0;
	tface->flag= TF_SELECT;
	tface->tpage= 0;
	tface->mode |= TF_DYNAMIC;
}

void make_tfaces(Mesh *me) 
{
	TFace *tface;
	int a;
	
	a= me->totface;
	if(a==0) return;
	tface= me->tface= MEM_callocN(a*sizeof(TFace), "tface");
	while(a--) {
		default_tface(tface);
		tface++;
	}
	if(me->mcol) {
		mcol_to_tface(me, 1);
	}
}

void reveal_tface()
{
	Mesh *me;
	TFace *tface;
	int a;
	
	me= get_mesh(OBACT);
	if(me==0 || me->tface==0 || me->totface==0) return;
	
	tface= me->tface;
	a= me->totface;
	while(a--) {
		if(tface->flag & TF_HIDE) {
			tface->flag |= TF_SELECT;
			tface->flag -= TF_HIDE;
		}
		tface++;
	}

	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWIMAGE, 0);
}



void hide_tface()
{
	Mesh *me;
	TFace *tface;
	int a;
	
	me= get_mesh(OBACT);
	if(me==0 || me->tface==0 || me->totface==0) return;
	
	if(G.qual & LR_ALTKEY) {
		reveal_tface();
		return;
	}
	
	tface= me->tface;
	a= me->totface;
	while(a--) {
		if(tface->flag & TF_HIDE);
		else {
			if(G.qual & LR_SHIFTKEY) {
				if( (tface->flag & TF_SELECT)==0) tface->flag |= TF_HIDE;
			}
			else {
				if( (tface->flag & TF_SELECT)) tface->flag |= TF_HIDE;
			}
		}
		if(tface->flag & TF_HIDE) tface->flag &= ~TF_SELECT;
		
		tface++;
	}
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWIMAGE, 0);
	
}

void select_linked_tfaces()
{
	Mesh *me;
	TFace *tface;
	MFace *mface;
	int a, doit=1;
	char *cpmain;
	
	me= get_mesh(OBACT);
	if(me==0 || me->tface==0 || me->totface==0) return;
	
	cpmain= MEM_callocN(me->totvert, "cpmain");
	
	while(doit) {
		doit= 0;
		
		/* select connected: fill array */
		tface= me->tface;
		mface= me->mface;
		a= me->totface;
		while(a--) {
			if(tface->flag & TF_HIDE);
			else if(tface->flag & TF_SELECT) {
				if( mface->v3) {
					cpmain[mface->v1]= 1;
					cpmain[mface->v2]= 1;
					cpmain[mface->v3]= 1;
					if(mface->v4) cpmain[mface->v4]= 1;
				}
			}
			tface++; mface++;
		}
		
		/* reverse: using array select the faces */

		tface= me->tface;
		mface= me->mface;
		a= me->totface;
		while(a--) {
			if(tface->flag & TF_HIDE);
			else if((tface->flag & TF_SELECT)==0) {
				if( mface->v3) {
					if(mface->v4) {
						if(cpmain[mface->v4]) {
							tface->flag |= TF_SELECT;
							doit= 1;
						}
					}
					if( cpmain[mface->v1] || cpmain[mface->v2] || cpmain[mface->v3] ) {
						tface->flag |= TF_SELECT;
						doit= 1;
					}
				}
			}
			tface++; mface++;
		}
		
	}
	MEM_freeN(cpmain);
	
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWIMAGE, 0);
	
}

void deselectall_tface()
{
	Mesh *me;
	TFace *tface;
	int a, sel;
		
	me= get_mesh(OBACT);
	if(me==0 || me->tface==0) return;
	
	tface= me->tface;
	a= me->totface;
	sel= 0;
	while(a--) {
		if(tface->flag & TF_HIDE);
		else if(tface->flag & TF_SELECT) sel= 1;
		tface++;
	}
	
	tface= me->tface;
	a= me->totface;
	while(a--) {
		if(tface->flag & TF_HIDE);
		else {
			if(sel) tface->flag &= ~TF_SELECT;
			else tface->flag |= TF_SELECT;
		}
		tface++;
	}
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWIMAGE, 0);
}

void selectswap_tface(void)
{
	Mesh *me;
	TFace *tface;
	int a;
		
	me= get_mesh(OBACT);
	if(me==0 || me->tface==0) return;
	
	tface= me->tface;
	a= me->totface;
	while(a--) {
		if(tface->flag & TF_HIDE);
		else {
			if(tface->flag & TF_SELECT) tface->flag &= ~TF_SELECT;
			else tface->flag |= TF_SELECT;
		}
		tface++;
	}

	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWIMAGE, 0);
}

void rotate_uv_tface()
{
	Mesh *me;
	TFace *tface;
	MFace *mface;
	short mode;
	int a;
	
	me= get_mesh(OBACT);
	if(me==0 || me->tface==0) return;
	
	mode= pupmenu("OK? %t|Rot UV %x1|Rot VertexCol %x2");
	
	if(mode<1) return;
	
	tface= me->tface;
	mface= me->mface;
	a= me->totface;
	while(a--) {
		if(tface->flag & TF_SELECT) {
			if(mode==1) {
				float u1= tface->uv[0][0];
				float v1= tface->uv[0][1];
				
				tface->uv[0][0]= tface->uv[1][0];
				tface->uv[0][1]= tface->uv[1][1];
	
				tface->uv[1][0]= tface->uv[2][0];
				tface->uv[1][1]= tface->uv[2][1];
	
				if(mface->v4) {
					tface->uv[2][0]= tface->uv[3][0];
					tface->uv[2][1]= tface->uv[3][1];
				
					tface->uv[3][0]= u1;
					tface->uv[3][1]= v1;
				}
				else {
					tface->uv[2][0]= u1;
					tface->uv[2][1]= v1;
				}
			}
			else if(mode==2) {
				unsigned int tcol= tface->col[0];
				
				tface->col[0]= tface->col[1];
				tface->col[1]= tface->col[2];
	
				if(mface->v4) {
					tface->col[2]= tface->col[3];
					tface->col[3]= tcol;
				}
				else {
					tface->col[2]= tcol;
				}
			}
		}
		tface++;
		mface++;
	}
	
	makeDispList(OBACT);
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWIMAGE, 0);
}

/**
 * Returns the face under the give position in screen coordinates.
 * Code extracted from face_select routine.
 * Question: why is all of the backbuffer drawn?
 * We're only interested in one pixel!
 * @author	Maarten Gribnau
 * @param	me	the mesh with the faces to be picked
 * @param	x	the x-coordinate to pick at
 * @param	y	the y-coordinate to pick at
 * @return the face under the cursor (-1 if there was no face found)
 */
int face_pick(Mesh *me, short x, short y)
{
	unsigned int col;
	int index;

	if (me==0 || me->tface==0) {
		return -1;
	}

	/* Have OpenGL draw in the back buffer with color coded face indices */
	if (curarea->win_swap==WIN_EQUAL) {
		G.vd->flag |= V3D_NEEDBACKBUFDRAW;
	}
	if (G.vd->flag & V3D_NEEDBACKBUFDRAW) {
		backdrawview3d(0);
		persp(PERSP_VIEW);
	}
	/* Read the pixel under the cursor */
#ifdef __APPLE__
	glReadBuffer(GL_AUX0);
#endif
	glReadPixels(x+curarea->winrct.xmin, y+curarea->winrct.ymin, 1, 1,
		GL_RGBA, GL_UNSIGNED_BYTE, &col);
	glReadBuffer(GL_BACK);

	/* Unbelievable! */
	if (G.order==B_ENDIAN) {
		SWITCH_INT(col);
	}
	/* Convert the color back to a face index */
	index = framebuffer_to_index(col);
	if (col==0 || index<=0 || index>me->totface) {
		return -1;
	}
	return (index-1);
}

void face_select()
{
	Object *ob;
	Mesh *me;
	TFace *tface, *tsel;
	MFace *msel;
	short mval[2];
	int a, index;

	/* Get the face under the cursor */
	ob = OBACT;
	if (!(ob->lay & G.vd->lay)) {
		error("Active object not in this layer!");
	}
	me = get_mesh(ob);
	getmouseco_areawin(mval);
	index = face_pick(me, mval[0], mval[1]);
	if (index==-1) return;
	
	tsel= (((TFace*)me->tface)+index);
	msel= (((MFace*)me->mface)+index);

	if (tsel->flag & TF_HIDE) return;
	
	/* clear flags */
	tface = me->tface;
	a = me->totface;
	while (a--) {
		if (G.qual & LR_SHIFTKEY)
			tface->flag &= ~TF_ACTIVE;
		else
			tface->flag &= ~(TF_ACTIVE+TF_SELECT);
		tface++;
	}
	
	tsel->flag |= TF_ACTIVE;

	if (G.qual & LR_SHIFTKEY) {
		if (tsel->flag & TF_SELECT)
			tsel->flag &= ~TF_SELECT;
		else
			tsel->flag |= TF_SELECT;
	}
	else tsel->flag |= TF_SELECT;
	
	lasttface = tsel;
	
	/* image window redraw */
	allqueue(REDRAWIMAGE, 0);
	allqueue(REDRAWBUTSEDIT, 0);
	allqueue(REDRAWVIEW3D, 0);
}

void face_borderselect()
{
	Mesh *me;
	TFace *tface;
	rcti rect;
	unsigned int *rectm, *rt;
	int a, sx, sy, index, val;
	char *selar;
	
	me= get_mesh(OBACT);
	if(me==0 || me->tface==0) return;
	if(me->totface==0) return;
	
	val= get_border(&rect, 3);
	
	/* why readbuffer here? shouldn't be necessary (maybe a flush or so) */
	glReadBuffer(GL_BACK);
#ifdef __APPLE__
	glReadBuffer(GL_AUX0); /* apple only */
#endif
	
	if(val) {
		selar= MEM_callocN(me->totface+1, "selar");
		
		sx= (rect.xmax-rect.xmin+1);
		sy= (rect.ymax-rect.ymin+1);
		if(sx*sy<=0) return;

		rt=rectm= MEM_mallocN(sizeof(int)*sx*sy, "selrect");
		glReadPixels(rect.xmin+curarea->winrct.xmin,  rect.ymin+curarea->winrct.ymin, sx, sy, GL_RGBA, GL_UNSIGNED_BYTE,  rectm);
		if(G.order==B_ENDIAN) IMB_convert_rgba_to_abgr(sx*sy, rectm);

		a= sx*sy;
		while(a--) {
			if(*rt) {
				index= framebuffer_to_index(*rt);
				if(index<=me->totface) selar[index]= 1;
			}
			rt++;
		}
		
		tface= me->tface;
		for(a=1; a<=me->totface; a++, tface++) {
			if(selar[a]) {
				if(tface->flag & TF_HIDE);
				else {
					if(val==LEFTMOUSE) tface->flag |= TF_SELECT;
					else tface->flag &= ~TF_SELECT;
				}
			}
		}
		
		MEM_freeN(rectm);
		MEM_freeN(selar);
		allqueue(REDRAWVIEW3D, 0);
		allqueue(REDRAWIMAGE, 0);
	}
#ifdef __APPLE__	
	glReadBuffer(GL_BACK);
#endif
}

#define TEST_STRUBI 1				
#ifdef TEST_STRUBI
float CalcNormUV(float *a, float *b, float *c)
{
	float d1[3], d2[3];

	d1[0] = a[0] - b[0];
	d1[1] = a[1] - b[1];
	d2[0] = b[0] - c[0];
	d2[1] = b[1] - c[1];
	return (d1[0] * d2[1] - d1[1] * d2[0]);
}
#endif


/* Pupmenu codes: */
#define UV_CUBE_MAPPING 2
#define UV_CYL_MAPPING 3
#define UV_SPHERE_MAPPING 4
#define UV_BOUNDS8_MAPPING 68
#define UV_BOUNDS4_MAPPING 65
#define UV_BOUNDS2_MAPPING 66
#define UV_BOUNDS1_MAPPING 67
#define UV_STD8_MAPPING 131
#define UV_STD4_MAPPING 130
#define UV_STD2_MAPPING 129
#define UV_STD1_MAPPING 128
#define UV_WINDOW_MAPPING 5
#define UV_CYL_EX 32
#define UV_SPHERE_EX 34

/* Some macro tricks to make pupmenu construction look nicer :-)
   Sorry, just did it for fun. */

#define _STR(x) " " #x
#define STRING(x) _STR(x)

#define MENUSTRING(string, code) string " %x" STRING(code)
#define MENUTITLE(string) string " %t|" 

void uv_autocalc_tface()
{
	short mode;
	mode= pupmenu(MENUTITLE("UV Calculation")
	              MENUSTRING("Cube",          UV_CUBE_MAPPING) "|"
	              MENUSTRING("Cylinder",      UV_CYL_MAPPING) "|"
	              MENUSTRING("Sphere",        UV_SPHERE_MAPPING) "|"
	              MENUSTRING("Bounds to 1/8", UV_BOUNDS8_MAPPING) "|"
	              MENUSTRING("Bounds to 1/4", UV_BOUNDS4_MAPPING) "|"
	              MENUSTRING("Bounds to 1/2", UV_BOUNDS2_MAPPING) "|"
	              MENUSTRING("Bounds to 1/1", UV_BOUNDS1_MAPPING) "|"
	              MENUSTRING("Standard 1/8",  UV_STD8_MAPPING) "|"
	              MENUSTRING("Standard 1/4",  UV_STD4_MAPPING) "|"
	              MENUSTRING("Standard 1/2",  UV_STD2_MAPPING) "|"
	              MENUSTRING("Standard 1/1",  UV_STD1_MAPPING) "|"
	              MENUSTRING("From Window",   UV_WINDOW_MAPPING) );
	
	
	switch(mode) {
	case UV_CUBE_MAPPING:
		calculate_uv_map(B_UVAUTO_CUBE); break;
	case UV_CYL_MAPPING:
		calculate_uv_map(B_UVAUTO_CYLINDER); break;
	case UV_SPHERE_MAPPING:
		calculate_uv_map(B_UVAUTO_SPHERE); break;
	case UV_BOUNDS8_MAPPING:
		calculate_uv_map(B_UVAUTO_BOUNDS8); break;
	case UV_BOUNDS4_MAPPING:
		calculate_uv_map(B_UVAUTO_BOUNDS4); break;
	case UV_BOUNDS2_MAPPING:
		calculate_uv_map(B_UVAUTO_BOUNDS2); break;
	case UV_BOUNDS1_MAPPING:
		calculate_uv_map(B_UVAUTO_BOUNDS1); break;
	case UV_STD8_MAPPING:
		calculate_uv_map(B_UVAUTO_STD8); break;
	case UV_STD4_MAPPING:
		calculate_uv_map(B_UVAUTO_STD4); break;
	case UV_STD2_MAPPING:
		calculate_uv_map(B_UVAUTO_STD2); break;
	case UV_STD1_MAPPING:
		calculate_uv_map(B_UVAUTO_STD1); break;
	case UV_WINDOW_MAPPING:
		calculate_uv_map(B_UVAUTO_WINDOW); break;
	}
}

void set_faceselect()	/* toggle */
{
	Object *ob = OBACT;
	Mesh *me = 0;
	
	scrarea_queue_headredraw(curarea);

	if(G.f & G_FACESELECT) G.f &= ~G_FACESELECT;
	else {
		if (ob && ob->type == OB_MESH) G.f |= G_FACESELECT;
	}

	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWBUTSEDIT, 0);
	allqueue(REDRAWIMAGE, 0);
	
	ob= OBACT;
	me= get_mesh(ob);
	if(me && me->tface==NULL) make_tfaces(me);

	if(G.f & G_FACESELECT) {
		setcursor_space(SPACE_VIEW3D, CURSOR_FACESEL);
		if(me) set_lasttface();
	}
	else if((G.f & (G_WEIGHTPAINT|G_VERTEXPAINT|G_TEXTUREPAINT))==0) {
		if(me) reveal_tface();
		setcursor_space(SPACE_VIEW3D, CURSOR_STD);
		makeDispList(ob);
	}
	countall();
}


#ifdef NAN_TPT
/**
 * Get the view ray through the screen point.
 * Uses the OpenGL settings of the active view port.
 * The coordinates should be given in viewport coordinates.
 * @author	Maarten Gribnau
 * @param	x		the x-coordinate of the screen point.
 * @param	y		the y-coordinate of the screen point.
 * @param	org		origin of the view ray.
 * @param	dir		direction of the view ray.
 */
void get_pick_ray(short x, short y, float org[3], float dir[3])
{
	double mvmatrix[16];
	double projmatrix[16];
	GLint viewport[4];
	double px, py, pz;
	float l;

	/* Get the matrices needed for gluUnProject */
	glGetIntegerv(GL_VIEWPORT, viewport);
	glGetDoublev(GL_MODELVIEW_MATRIX, mvmatrix);
	glGetDoublev(GL_PROJECTION_MATRIX, projmatrix);

	/* Set up viewport so that gluUnProject will give correct values */
	viewport[0] = 0;
	viewport[1] = 0;
	/* printf("viewport = (%4d, %4d, %4d, %4d)\n", viewport[0], viewport[1], viewport[2], viewport[3]); */
	/* printf("cursor = (%4d, %4d)\n", x, y); */

	gluUnProject((GLdouble) x, (GLdouble) y, 0.0, mvmatrix, projmatrix, viewport, &px, &py, &pz);
	org[0] = (float)px; org[1] = (float)py; org[2] = (float)pz;
	/* printf("world point at z=0.0 is (%f, %f, %f)\n", org[0], org[1], org[2]); */
	gluUnProject((GLdouble) x, (GLdouble) y, 1.0, mvmatrix, projmatrix, viewport, &px, &py, &pz); 
	/* printf("world point at z=1.0 is (%f, %f, %f)\n", px, py, pz); */
	dir[0] = ((float)px) - org[0];
	dir[1] = ((float)py) - org[1];
	dir[2] = ((float)pz) - org[2];
	l = (float)sqrt(dir[0]*dir[0] + dir[1]*dir[1] + dir[2]*dir[2]);
	if (!l) return;
	l = 1. / l;
	dir[0] *= l; dir[1] *= l; dir[2] *= l;
	/* printf("ray org. is (%f, %f, %f)\n", org[0], org[1], org[2]); */
	/* printf("ray dir. is (%f, %f, %f)\n", dir[0], dir[1], dir[2]); */
}


int triangle_ray_intersect(float tv0[3], float tv1[3], float tv2[3], float org[3], float dir[3], float uv[2])
{
	float v1v0[3];
	float v2v0[3];
	float n[3], an[3];
	float t, d, l;
	float p[3];
	double u0, v0, u1, v1, u2, v2, uvtemp;
	unsigned int iu, iv;

	/* Calculate normal of the plane (cross, normalize)
	 * Could really use moto here...
	 */
	v1v0[0] = tv1[0] - tv0[0];
	v1v0[1] = tv1[1] - tv0[1];
	v1v0[2] = tv1[2] - tv0[2];
	v2v0[0] = tv2[0] - tv0[0];
	v2v0[1] = tv2[1] - tv0[1];
	v2v0[2] = tv2[2] - tv0[2];
	n[0] = (v1v0[1] * v2v0[2]) - (v1v0[2] * v2v0[1]);
	n[1] = (v1v0[2] * v2v0[0]) - (v1v0[0] * v2v0[2]);
	n[2] = (v1v0[0] * v2v0[1]) - (v1v0[1] * v2v0[0]);
	l = sqrt(n[0]*n[0] + n[1]*n[1] + n[2]*n[2]);
	if (!l) return 0;
	l = 1. / l;
	n[0] *= l; n[1] *= l; n[2] *= l;

	/* Calculate intersection point */
	t = n[0]*dir[0] + n[1]*dir[1] + n[2]*dir[2];
	if (fabs(t) < 1.0e-6) return 0;
	d = -(n[0]*tv0[0] + n[1]*tv0[1] + n[2]*tv0[2]);
	t = -(((n[0]*org[0] + n[1]*org[1] + n[2]*org[2]) + d) / t);
	if (t < 0) return 0;
	p[0] = org[0] + dir[0]*t;
	p[1] = org[1] + dir[1]*t;
	p[2] = org[2] + dir[2]*t;
	/*printf("intersection at (%f, %f, %f)\n", p[0], p[1], p[2]);*/

	/* Calculate the largest component of the normal */
	an[0] = fabs(n[0]); an[1] = fabs(n[1]); an[2] = fabs(n[2]);
	if ((an[0] > an[1]) && (an[0] > an[2])) {
		iu = 1; iv = 2;
	}
	else if ((an[1] > an[0]) && (an[1] > an[2])) {
		iu = 2; iv = 0;
	}
	else {
		iu = 0; iv = 1;
	}
	/* printf("iu, iv = (%d, %d)\n", iu, iv); */

	/* Calculate (u,v) */
	u0 = p[iu] - tv0[iu];
	v0 = p[iv] - tv0[iv];
	u1 = tv1[iu] - tv0[iu];
	v1 = tv1[iv] - tv0[iv];
	u2 = tv2[iu] - tv0[iu];
	v2 = tv2[iv] - tv0[iv];
	/* printf("u0, v0, u1, v1, u2, v2 = (%f, %f, %f, %f, %f, %f)\n", u0, v0, u1, v1, u2, v2); */

	/* These calculations should be in double precision.
	 * On windows we get inpredictable results in single precision
	 */
	if (u1 == 0) {
		uvtemp = u0/u2;
		uv[1] = (float)uvtemp;
		/* if ((uv[1] >= 0.) && (uv[1] <= 1.)) { */
			uv[0] = (float)((v0 - uvtemp*v2) / v1);
		/* } */
	}
	else {
		uvtemp = (v0*u1 - u0*v1)/(v2*u1-u2*v1);
		uv[1] = (float)uvtemp;
		/* if ((uv[1] >= 0) && (uv[1] <= 1)) { */
			uv[0] = (float)((u0 - uvtemp*u2) / u1);
		/* } */
	}
	/* printf("uv[0], uv[1] = (%f, %f)\n", uv[0], uv[1]); */
	return ((uv[0] >= 0) && (uv[1] >= 0) && ((uv[0]+uv[1]) <= 1)) ? 2 : 1;
}

/**
 * Returns the vertex (local) coordinates of a face.
 * No bounds checking!
 * @author	Maarten Gribnau
 * @param	mesh	the mesh with the face.
 * @param	face	the face.
 * @param	v1		vertex 1 coordinates.
 * @param	v2		vertex 2 coordinates.
 * @param	v3		vertex 3 coordinates.
 * @param	v4		vertex 4 coordinates.
 * @return	number of vertices of this face
 */
int face_get_vertex_coordinates(Mesh* mesh, TFace* face, float v1[3], float v2[3], float v3[3], float v4[3])
{
	int num_vertices;
	MVert *mv;
	MFace *mf = (MFace *) (((MFace *)mesh->mface) + (face - (TFace *) mesh->tface));

	num_vertices = mf->v4 == 0 ? 3 : 4;
	mv = mesh->mvert + mf->v1;
	v1[0] = mv->co[0]; v1[1] = mv->co[1]; v1[2] = mv->co[2];
	mv = mesh->mvert + mf->v2;
	v2[0] = mv->co[0]; v2[1] = mv->co[1]; v2[2] = mv->co[2];
	mv = mesh->mvert + mf->v3;
	v3[0] = mv->co[0]; v3[1] = mv->co[1]; v3[2] = mv->co[2];
	if (num_vertices == 4) {
		mv = mesh->mvert + mf->v4;
		v4[0] = mv->co[0]; v4[1] = mv->co[1]; v4[2] = mv->co[2];
	}

	return num_vertices;
}

/**
 * Finds texture coordinates from face edge interpolation values.
 * @author	Maarten Gribnau
 * @param	face	the face.
 * @param	v1		vertex 1 index.
 * @param	v2		vertex 2 index.
 * @param	v3		vertex 3 index.
 * @param	a		interpolation value of edge v2-v1.
 * @param	b		interpolation value of edge v3-v1.
 * @param	u		(u,v) coordinate.
 * @param	v		(u,v) coordinate.
 */
void face_get_uv(TFace* face, int v1, int v2, int v3, float a, float b, float* u, float* v)
{
	float uv01[2], uv21[2];

	/* Pin a,b inside [0,1] range */
#if 0
	a = (float)fmod(a, 1.);
	b = (float)fmod(b, 1.);
#else
	if (a < 0.f) a = 0.f;
	else if (a > 1.f) a = 1.f;
	if (b < 0.f) b = 0.f;
	else if (b > 1.f) b = 1.f;
#endif

	/* Convert to texture coordinates */
	uv01[0] = face->uv[v2][0] - face->uv[v1][0];
	uv01[1] = face->uv[v2][1] - face->uv[v1][1];
	uv21[0] = face->uv[v3][0] - face->uv[v1][0];
	uv21[1] = face->uv[v3][1] - face->uv[v1][1];
	uv01[0] *= a;
	uv01[1] *= a;
	uv21[0] *= b;
	uv21[1] *= b;
	*u = face->uv[v1][0] + (uv01[0] + uv21[0]);
	*v = face->uv[v1][1] + (uv01[1] + uv21[1]);
}

/**
 * Get the (u,v) coordinates on a face from a point in screen coordinates.
 * The coordinates should be given in viewport coordinates.
 * @author	Maarten Gribnau
 * @param	object	the object with the mesh
 * @param	mesh	the mesh with the face to be picked.
 * @param	face	the face to be picked.
 * @param	x		the x-coordinate to pick at.
 * @param	y		the y-coordinate to pick at.
 * @param	u		the u-coordinate calculated.
 * @param	v		the v-coordinate calculated.
 * @return	intersection result:
 *			0 == no intersection, (u,v) invalid
 *			1 == intersection, (u,v) valid
 */
int face_pick_uv(Object* object, Mesh* mesh, TFace* face, short x, short y, float* u, float* v)
{
	float org[3], dir[3];
	float ab[2];
	float v1[3], v2[3], v3[3], v4[3];
	int result;
	int num_verts;

	/* Get a view ray to intersect with the face */
	get_pick_ray(x, y, org, dir);

	/* Convert local vertex coordinates to world */
	num_verts = face_get_vertex_coordinates(mesh, face, v1, v2, v3, v4);
	/* Convert local vertex coordinates to world */
	Mat4MulVecfl(object->obmat, v1);
	Mat4MulVecfl(object->obmat, v2);
	Mat4MulVecfl(object->obmat, v3);
	if (num_verts > 3) {
		Mat4MulVecfl(object->obmat, v4);
	}

	/* Get (u,v) values (local face coordinates) of intersection point
	 * If face is a quad, there are two triangles to check.
	 */
	result = triangle_ray_intersect(v2, v1, v3, org, dir, ab);
	if ( (num_verts == 3) || ((num_verts == 4) && (result > 1)) ) {
		/* Face is a triangle or a quad with a hit on the first triangle */
		face_get_uv(face, 1, 0, 2, ab[0], ab[1], u, v);
		/* printf("triangle 1, texture (u,v)=(%f, %f)\n", *u, *v); */
	}
	else {
		/* Face is a quad and no intersection with first triangle */
		result = triangle_ray_intersect(v4, v3, v1, org, dir, ab);
		face_get_uv(face, 3, 2, 0, ab[0], ab[1], u, v);
		/* printf("triangle 2, texture (u,v)=(%f, %f)\n", *u, *v); */
	}
	return result > 0;
}

/**
 * First attempt at drawing in the texture of a face.
 * @author	Maarten Gribnau
 */
void face_draw()
{
	Object *ob;
	Mesh *me;
	TFace *face, *face_old = 0;
	short xy[2], xy_old[2];
	//int a, index;
	Image *img=NULL, *img_old = NULL;
	IMG_BrushPtr brush;
	IMG_CanvasPtr canvas = 0;
	int rowBytes, face_index;
	char *warn_packed_file = 0;
	float uv[2], uv_old[2];
	extern VPaint Gvp;

	ob = OBACT;
	if (!ob) {
		error("No active object"); return;
	}
	if (!(ob->lay & G.vd->lay)) {
		error("Active object not in this layer"); return;
	}
	me = get_mesh(ob);
	if (!me) {
		error("Active object does not have a mesh"); return;
	}

	brush = IMG_BrushCreate(Gvp.size, Gvp.size, Gvp.r, Gvp.g, Gvp.b, Gvp.a);
	if (!brush) {
		error("Can not create brush"); return;
	}

	persp(PERSP_VIEW);

	getmouseco_areawin(xy_old);
	while (get_mbut() & L_MOUSE) {
		getmouseco_areawin(xy);
		/* Check if cursor has moved */
		if ((xy[0] != xy_old[0]) || (xy[1] != xy_old[1])) {

			/* Get face to draw on */
			face_index = face_pick(me, xy[0], xy[1]);
			if (face_index == -1) face = NULL;
			else face = (((TFace*)me->tface)+face_index);

			/* Check if this is another face. */
			if (face != face_old) {
				/* The active face changed, check the texture */
				if (face) {
					img = face->tpage;
				}
				else {
					img = 0;
				}

				if (img != img_old) {
					/* Faces have different textures. Finish drawing in the old face. */
					if (face_old && canvas) {
						face_pick_uv(ob, me, face_old, xy[0], xy[1], &uv[0], &uv[1]);
						IMG_CanvasDrawLineUV(canvas, brush, uv_old[0], uv_old[1], uv[0], uv[1]);
						img_old->ibuf->userflags |= IB_BITMAPDIRTY;
						/* Delete old canvas */
						IMG_CanvasDispose(canvas);
						canvas = 0;
					}

					/* Create new canvas and start drawing in the new face. */
					if (img) {
						if (img->ibuf && img->packedfile == 0) {
							/* MAART: skipx is not set most of the times. Make a guess. */
							rowBytes = img->ibuf->skipx ? img->ibuf->skipx : img->ibuf->x * 4;
							canvas = IMG_CanvasCreateFromPtr(img->ibuf->rect, img->ibuf->x, img->ibuf->y, rowBytes);
							if (canvas) {
								face_pick_uv(ob, me, face, xy_old[0], xy_old[1], &uv_old[0], &uv_old[1]);
								face_pick_uv(ob, me, face, xy[0], xy[1], &uv[0], &uv[1]);
								IMG_CanvasDrawLineUV(canvas, brush, uv_old[0], uv_old[1], uv[0], uv[1]);
								img->ibuf->userflags |= IB_BITMAPDIRTY;
							}
						}
						else {
							/* TODO: should issue warning that no texture is assigned */
							if (img->packedfile) {
								warn_packed_file = img->id.name + 2;
								img = 0;
							}
						}
					}
				}
				else {
					/* Face changed and faces have the same texture. */
					if (canvas) {
						/* Finish drawing in the old face. */
						if (face_old) {
							face_pick_uv(ob, me, face_old, xy[0], xy[1], &uv[0], &uv[1]);
							IMG_CanvasDrawLineUV(canvas, brush, uv_old[0], uv_old[1], uv[0], uv[1]);
							img_old->ibuf->userflags |= IB_BITMAPDIRTY;
						}

						/* Start drawing in the new face. */
						if (face) {
							face_pick_uv(ob, me, face, xy_old[0], xy_old[1], &uv_old[0], &uv_old[1]);
							face_pick_uv(ob, me, face, xy[0], xy[1], &uv[0], &uv[1]);
							IMG_CanvasDrawLineUV(canvas, brush, uv_old[0], uv_old[1], uv[0], uv[1]);
							img->ibuf->userflags |= IB_BITMAPDIRTY;
						}
					}
				}
			}
			else {
				/* Same face, continue drawing */
				if (face && canvas) {
					/* Get the new (u,v) coordinates */
					face_pick_uv(ob, me, face, xy[0], xy[1], &uv[0], &uv[1]);
					IMG_CanvasDrawLineUV(canvas, brush, uv_old[0], uv_old[1], uv[0], uv[1]);
					img->ibuf->userflags |= IB_BITMAPDIRTY;
				}
			}

			if (face && img) {
				/* Make OpenGL aware of a change in the texture */
				free_realtime_image(img);
				/* Redraw the view */
				scrarea_do_windraw(curarea);
				screen_swapbuffers();
			}

			xy_old[0] = xy[0];
			xy_old[1] = xy[1];
			uv_old[0] = uv[0];
			uv_old[1] = uv[1];
			face_old = face;
			img_old = img;
		}
	}

	IMG_BrushDispose(brush);
	if (canvas) {
		IMG_CanvasDispose(canvas);
		canvas = 0;
	}

	if (warn_packed_file) {
		error("Painting in packed images not supported: %s", warn_packed_file);
	}

	persp(PERSP_WIN);

	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWIMAGE, 0);
	allqueue(REDRAWHEADERS, 0);
}

 /* Selects all faces which have the same uv-texture as the active face 
 * @author	Roel Spruit
 * @return	Void
 * Errors:	- Active object not in this layer
 *		- No active face or active face has no UV-texture			
 */
void get_same_uv(void)
{
	Object *ob;
	Mesh *me;
	TFace *tface;	
	short a, foundtex=0;
	Image *ima;
	char uvname[160];
	
	ob = OBACT;
	if (!(ob->lay & G.vd->lay)) {
		error("Active object not in this layer!");
		return;
	}
	me = get_mesh(ob);
	
		
	/* Search for the active face with a UV-Texture */
	tface = me->tface;
	a = me->totface;
	while (a--) {		
		if(tface->flag & TF_ACTIVE){			
			ima=tface->tpage;
			if(ima && ima->name){
				strcpy(uvname,ima->name);			
				a=0;
				foundtex=1;
			}
		}
		tface++;
	}		
	
	if(!foundtex) {
		error("No active face or active face has no UV-texture");
		return;
	}

	/* select everything with the same texture */
	tface = me->tface;
	a = me->totface;
	while (a--) {		
		ima=tface->tpage;
		if(ima && ima->name){
			if(!strcmp(ima->name, uvname)){
				tface->flag |= TF_SELECT;
			}
			else tface->flag &= ~TF_SELECT;
		}
		else tface->flag &= ~TF_SELECT;
		tface++;
	}
	

	
	/* image window redraw */
	allqueue(REDRAWIMAGE, 0);
	allqueue(REDRAWVIEW3D, 0);
}
#endif /* NAN_TPT */
