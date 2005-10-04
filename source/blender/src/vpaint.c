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
#include <io.h>
#else
#include <unistd.h>
#endif   

#include "MEM_guardedalloc.h"

#include "IMB_imbuf.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "MTC_matrixops.h"

#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_object_force.h"
#include "DNA_screen_types.h"
#include "DNA_scene_types.h"
#include "DNA_view3d_types.h"
#include "DNA_userdef_types.h"

#include "BKE_DerivedMesh.h"
#include "BKE_depsgraph.h"
#include "BKE_deform.h"
#include "BKE_displist.h"
#include "BKE_global.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_utildefines.h"

#include "BIF_graphics.h"
#include "BIF_interface.h"
#include "BIF_mywindow.h"
#include "BIF_editview.h"
#include "BIF_space.h"
#include "BIF_screen.h"
#include "BIF_toolbox.h"
#include "BIF_glutil.h"
#include "BIF_gl.h"

#include "BDR_vpaint.h"

#include "BSE_drawview.h"
#include "BSE_trans_types.h"
#include "BSE_view.h"

#include "mydevice.h"
#include "blendef.h"

#include "BIF_editdeform.h"

	/* Gvp.mode */
#define VP_MIX	0
#define VP_ADD	1
#define VP_SUB	2
#define VP_MUL	3
#define VP_FILT	4

#define MAXINDEX	512000

VPaint Gvp= {1.0, 1.0, 1.0, 0.2, 25.0, 1.0, 1.0, 0, VP_AREA+VP_SOFT+VP_SPRAY};
VPaint Gwp= {1.0, 1.0, 1.0, 0.2, 25.0, 1.0, 1.0, 0, VP_AREA+VP_SOFT};
float vpimat[3][3];
unsigned int *vpaintundobuf= NULL;
int totvpaintundo;
int *indexar= NULL;

int totwpaintundo;
MDeformVert *wpaintundobuf=NULL;

/* in contradiction to cpack drawing colors, the MCOL colors (vpaint colors) are per byte! 
   so not endian sensitive. Mcol = ABGR!!! so be cautious with cpack calls */

unsigned int rgba_to_mcol(float r, float g, float b, float a)
{
	int ir, ig, ib, ia;
	unsigned int col;
	char *cp;
	
	ir= floor(255.0*r);
	if(ir<0) ir= 0; else if(ir>255) ir= 255;
	ig= floor(255.0*g);
	if(ig<0) ig= 0; else if(ig>255) ig= 255;
	ib= floor(255.0*b);
	if(ib<0) ib= 0; else if(ib>255) ib= 255;
	ia= floor(255.0*a);
	if(ia<0) ia= 0; else if(ia>255) ia= 255;
	
	cp= (char *)&col;
	cp[0]= ia;
	cp[1]= ib;
	cp[2]= ig;
	cp[3]= ir;
	
	return col;
	
}

unsigned int vpaint_get_current_col(void)
{
	return rgba_to_mcol(Gvp.r, Gvp.g, Gvp.b, 1.0);
}

void do_shared_vertexcol(Mesh *me)
{
	/* if no mcol: do not do */
	/* if tface: only the involved faces, otherwise all */
	MFace *mface;
	TFace *tface;
	int a;
	short *scolmain, *scol;
	char *mcol;
	
	if(me->mcol==0 || me->totvert==0 || me->totface==0) return;
	
	scolmain= MEM_callocN(4*sizeof(short)*me->totvert, "colmain");
	
	tface= me->tface;
	mface= me->mface;
	mcol= (char *)me->mcol;
	for(a=me->totface; a>0; a--, mface++, mcol+=16) {
		if(tface==0 || (tface->mode & TF_SHAREDCOL) || (G.f & G_FACESELECT)==0) {
			scol= scolmain+4*mface->v1;
			scol[0]++; scol[1]+= mcol[1]; scol[2]+= mcol[2]; scol[3]+= mcol[3];
			scol= scolmain+4*mface->v2;
			scol[0]++; scol[1]+= mcol[5]; scol[2]+= mcol[6]; scol[3]+= mcol[7];
			scol= scolmain+4*mface->v3;
			scol[0]++; scol[1]+= mcol[9]; scol[2]+= mcol[10]; scol[3]+= mcol[11];
			if(mface->v4) {
				scol= scolmain+4*mface->v4;
				scol[0]++; scol[1]+= mcol[13]; scol[2]+= mcol[14]; scol[3]+= mcol[15];
			}
		}
		if(tface) tface++;
	}
	
	a= me->totvert;
	scol= scolmain;
	while(a--) {
		if(scol[0]>1) {
			scol[1]/= scol[0];
			scol[2]/= scol[0];
			scol[3]/= scol[0];
		}
		scol+= 4;
	}
	
	tface= me->tface;
	mface= me->mface;
	mcol= (char *)me->mcol;
	for(a=me->totface; a>0; a--, mface++, mcol+=16) {
		if(tface==0 || (tface->mode & TF_SHAREDCOL) || (G.f & G_FACESELECT)==0) {
			scol= scolmain+4*mface->v1;
			mcol[1]= scol[1]; mcol[2]= scol[2]; mcol[3]= scol[3];
			scol= scolmain+4*mface->v2;
			mcol[5]= scol[1]; mcol[6]= scol[2]; mcol[7]= scol[3];
			scol= scolmain+4*mface->v3;
			mcol[9]= scol[1]; mcol[10]= scol[2]; mcol[11]= scol[3];
			if(mface->v4) {
				scol= scolmain+4*mface->v4;
				mcol[13]= scol[1]; mcol[14]= scol[2]; mcol[15]= scol[3];
			}
		}
		if(tface) tface++;
	}

	MEM_freeN(scolmain);
}

void make_vertexcol()	/* single ob */
{
	Object *ob;
	Mesh *me;
	int i;

	/*
	 * Always copies from shadedisplist to mcol.
	 * When there are tfaces, it copies the colors and frees mcol
	 */
	
	if(G.obedit) {
		error("Unable to perform function in Edit Mode");
		return;
	}
	
	ob= OBACT;
	me= get_mesh(ob);
	if(me==0) return;

	if(me->mcol) MEM_freeN(me->mcol);
	mesh_create_shadedColors(ob, 1, (unsigned int**) &me->mcol, NULL);

	for (i=0; i<me->totface*4; i++) {
		me->mcol[i].a = 255;
	}
		
	if(me->tface) mcol_to_tface(me, 1);
	
	DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
	
	allqueue(REDRAWBUTSEDIT, 0);
	allqueue(REDRAWVIEW3D, 0);
}



void copy_vpaint_undo(unsigned int *mcol, int tot)
{
	if(vpaintundobuf) MEM_freeN(vpaintundobuf);
	vpaintundobuf= 0;
	totvpaintundo= tot;	// because of return, it is used by weightpaint
	
	if(mcol==0 || tot==0) return;
	
	vpaintundobuf= MEM_mallocN(4*sizeof(int)*tot, "vpaintundobuf");
	memcpy(vpaintundobuf, mcol, 4*sizeof(int)*tot);
	
}

void vpaint_undo()
{
	Mesh *me;
	Object *ob;
	unsigned int temp, *from, *to;
	int a;
	
	if((G.f & G_VERTEXPAINT)==0) return;
	if(vpaintundobuf==0) return;

	ob= OBACT;
	me= get_mesh(ob);
	if(me==0 || me->totface==0) return;

	if(me->tface) tface_to_mcol(me);
	else if(me->mcol==0) return;
	
	a= MIN2(me->totface, totvpaintundo);
	from= vpaintundobuf;
	to= (unsigned int *)me->mcol;
	a*= 4;
	while(a--) {
		temp= *to;
		*to= *from;
		*from= temp;
		to++; from++;
	}
	DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
	
	allqueue(REDRAWVIEW3D, 0);
	if(me->tface) mcol_to_tface(me, 1);
}

void clear_vpaint()
{
	Mesh *me;
	Object *ob;
	unsigned int *to, paintcol;
	int a;
	
	if((G.f & G_VERTEXPAINT)==0) return;

	ob= OBACT;
	me= get_mesh(ob);
	if(ob->id.lib) return;

	if(me==0 || me->totface==0) return;

	if(me->tface) tface_to_mcol(me);
	if(me->mcol==0) return;

	paintcol= vpaint_get_current_col();

	to= (unsigned int *)me->mcol;
	copy_vpaint_undo(to, me->totface);
	a= 4*me->totface;
	while(a--) {
		*to= paintcol;
		to++; 
	}
	BIF_undo_push("Clear vertex colors");
	DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
	
	allqueue(REDRAWVIEW3D, 0);
	if(me->tface) mcol_to_tface(me, 1);
}

void clear_vpaint_selectedfaces()
{
	Mesh *me;
	TFace *tf;
	Object *ob;
	unsigned int paintcol;
	int i;

	ob= OBACT;

	me= get_mesh(ob);
	tf = me->tface;
	if (!tf) return; /* should not happen, but you never know */

	if(me==0 || me->totface==0) return;

	paintcol= vpaint_get_current_col();

	for (i = 0; i < me->totface; i++) {
		if (tf[i].flag & TF_SELECT) {
			tf[i].col[0] = paintcol;
			tf[i].col[1] = paintcol;
			tf[i].col[2] = paintcol;
			tf[i].col[3] = paintcol;
		}
	}
	BIF_undo_push("Clear vertex colors");
	allqueue(REDRAWVIEW3D, 0);
}

void vpaint_dogamma()
{
	Mesh *me;
	Object *ob;
	float igam, fac;
	int a, temp;
	char *cp, gamtab[256];

	if((G.f & G_VERTEXPAINT)==0) return;

	ob= OBACT;
	me= get_mesh(ob);
	if(me==0 || me->totface==0) return;

	if(me->tface) tface_to_mcol(me);
	else if(me->mcol==0) return;

	copy_vpaint_undo((unsigned int *)me->mcol, me->totface);

	igam= 1.0/Gvp.gamma;
	for(a=0; a<256; a++) {
		
		fac= ((float)a)/255.0;
		fac= Gvp.mul*pow( fac, igam);
		
		temp= 255.9*fac;
		
		if(temp<=0) gamtab[a]= 0;
		else if(temp>=255) gamtab[a]= 255;
		else gamtab[a]= temp;
	}

	a= 4*me->totface;
	cp= (char *)me->mcol;
	while(a--) {
		
		cp[1]= gamtab[ cp[1] ];
		cp[2]= gamtab[ cp[2] ];
		cp[3]= gamtab[ cp[3] ];
		
		cp+= 4;
	}
	allqueue(REDRAWVIEW3D, 0);
	
	if(me->tface) mcol_to_tface(me, 1);
}

/* used for both 3d view and image window */
void sample_vpaint()	/* frontbuf */
{
	unsigned int col;
	int x, y;
	short mval[2];
	char *cp;
	
	getmouseco_areawin(mval);
	x= mval[0]; y= mval[1];
	
	if(x<0 || y<0) return;
	if(x>=curarea->winx || y>=curarea->winy) return;
	
	x+= curarea->winrct.xmin;
	y+= curarea->winrct.ymin;
	
	glReadBuffer(GL_FRONT);
	glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &col);
	glReadBuffer(GL_BACK);

	cp = (char *)&col;
	
	Gvp.r= cp[0];
	Gvp.r /= 255.0;

	Gvp.g= cp[1];
	Gvp.g /= 255.0;

	Gvp.b= cp[2];
	Gvp.b /= 255.0;

	allqueue(REDRAWBUTSEDIT, 0);
	addqueue(curarea->win, REDRAW, 1); // needed for when panel is open...
}

void init_vertexpaint()
{
	
	indexar= MEM_mallocN(sizeof(int)*MAXINDEX + 2, "vertexpaint");
}


void free_vertexpaint()
{
	
	if(indexar) MEM_freeN(indexar);
	indexar= NULL;
	if(vpaintundobuf) MEM_freeN(vpaintundobuf);
	vpaintundobuf= NULL;
	if(wpaintundobuf) 
		free_dverts(wpaintundobuf, totwpaintundo);
	wpaintundobuf= NULL;
}


static unsigned int mcol_blend(unsigned int col1, unsigned int col2, int fac)
{
	char *cp1, *cp2, *cp;
	int mfac;
	unsigned int col=0;
	
	if(fac==0) return col1;
	if(fac>=255) return col2;

	mfac= 255-fac;
	
	cp1= (char *)&col1;
	cp2= (char *)&col2;
	cp=  (char *)&col;
	
	cp[0]= 255;
	cp[1]= (mfac*cp1[1]+fac*cp2[1])>>8;
	cp[2]= (mfac*cp1[2]+fac*cp2[2])>>8;
	cp[3]= (mfac*cp1[3]+fac*cp2[3])>>8;
	
	return col;
}

static unsigned int mcol_add(unsigned int col1, unsigned int col2, int fac)
{
	char *cp1, *cp2, *cp;
	int temp;
	unsigned int col=0;
	
	if(fac==0) return col1;
	
	cp1= (char *)&col1;
	cp2= (char *)&col2;
	cp=  (char *)&col;
	
	cp[0]= 255;
	temp= cp1[1] + ((fac*cp2[1])>>8);
	if(temp>254) cp[1]= 255; else cp[1]= temp;
	temp= cp1[2] + ((fac*cp2[2])>>8);
	if(temp>254) cp[2]= 255; else cp[2]= temp;
	temp= cp1[3] + ((fac*cp2[3])>>8);
	if(temp>254) cp[3]= 255; else cp[3]= temp;
	
	return col;
}

static unsigned int mcol_sub(unsigned int col1, unsigned int col2, int fac)
{
	char *cp1, *cp2, *cp;
	int temp;
	unsigned int col=0;
	
	if(fac==0) return col1;
	
	cp1= (char *)&col1;
	cp2= (char *)&col2;
	cp=  (char *)&col;
	
	cp[0]= 255;
	temp= cp1[1] - ((fac*cp2[1])>>8);
	if(temp<0) cp[1]= 0; else cp[1]= temp;
	temp= cp1[2] - ((fac*cp2[2])>>8);
	if(temp<0) cp[2]= 0; else cp[2]= temp;
	temp= cp1[3] - ((fac*cp2[3])>>8);
	if(temp<0) cp[3]= 0; else cp[3]= temp;
	
	return col;
}

static unsigned int mcol_mul(unsigned int col1, unsigned int col2, int fac)
{
	char *cp1, *cp2, *cp;
	int mfac;
	unsigned int col=0;
	
	if(fac==0) return col1;

	mfac= 255-fac;
	
	cp1= (char *)&col1;
	cp2= (char *)&col2;
	cp=  (char *)&col;
	
	/* first mul, then blend the fac */
	cp[0]= 255;
	cp[1]= (mfac*cp1[1] + fac*((cp2[1]*cp1[1])>>8)  )>>8;
	cp[2]= (mfac*cp1[2] + fac*((cp2[2]*cp1[2])>>8)  )>>8;
	cp[3]= (mfac*cp1[3] + fac*((cp2[3]*cp1[3])>>8)  )>>8;

	
	return col;
}

static void vpaint_blend( unsigned int *col, unsigned int *colorig, unsigned int paintcol, int alpha)
{

	if(Gvp.mode==VP_MIX || Gvp.mode==VP_FILT) *col= mcol_blend( *col, paintcol, alpha);
	else if(Gvp.mode==VP_ADD) *col= mcol_add( *col, paintcol, alpha);
	else if(Gvp.mode==VP_SUB) *col= mcol_sub( *col, paintcol, alpha);
	else if(Gvp.mode==VP_MUL) *col= mcol_mul( *col, paintcol, alpha);
	
	/* if no spray, clip color adding with colorig & orig alpha */
	if((Gvp.flag & VP_SPRAY)==0) {
		unsigned int testcol=0, a;
		char *cp, *ct, *co;
		
		alpha= (int)(255.0*Gvp.a);
		
		if(Gvp.mode==VP_MIX || Gvp.mode==VP_FILT) testcol= mcol_blend( *colorig, paintcol, alpha);
		else if(Gvp.mode==VP_ADD) testcol= mcol_add( *colorig, paintcol, alpha);
		else if(Gvp.mode==VP_SUB) testcol= mcol_sub( *colorig, paintcol, alpha);
		else if(Gvp.mode==VP_MUL) testcol= mcol_mul( *colorig, paintcol, alpha);
		
		cp= (char *)col;
		ct= (char *)&testcol;
		co= (char *)colorig;
		
		for(a=0; a<4; a++) {
			if( ct[a]<co[a] ) {
				if( cp[a]<ct[a] ) cp[a]= ct[a];
				else if( cp[a]>co[a] ) cp[a]= co[a];
			}
			else {
				if( cp[a]<co[a] ) cp[a]= co[a];
				else if( cp[a]>ct[a] ) cp[a]= ct[a];
			}
		}
	}
}


static int sample_backbuf_area(int x, int y, float size)
{
	unsigned int rect[129*129], *rt;
	int x1, y1, x2, y2, a, tot=0, index;
	
	if(totvpaintundo>=MAXINDEX) return 0;
	
	if(size>64.0) size= 64.0;
	
	x1= x-size;
	x2= x+size;
	CLAMP(x1, 0, curarea->winx);
	CLAMP(x2, 0, curarea->winx);
	y1= y-size;
	y2= y+size;
	CLAMP(y1, 0, curarea->winy);
	CLAMP(y2, 0, curarea->winy);
#ifdef __APPLE__
	glReadBuffer(GL_AUX0);
#endif
	glReadPixels(x1+curarea->winrct.xmin, y1+curarea->winrct.ymin, x2-x1+1, y2-y1+1, GL_RGBA, GL_UNSIGNED_BYTE,  rect);
	glReadBuffer(GL_BACK);	

	if(G.order==B_ENDIAN) IMB_convert_rgba_to_abgr( (int)(4*size*size), rect);

	rt= rect;
	size= (y2-y1)*(x2-x1);
	if(size<=0) return 0;

	memset(indexar, 0, sizeof(int)*totvpaintundo+2);	/* plus 2! first element is total */
	
	while(size--) {
			
		if(*rt) {
			index= framebuffer_to_index(*rt);
			if(index>0 && index<=totvpaintundo)
				indexar[index] = 1;
		}
	
		rt++;
	}
	
	for(a=1; a<=totvpaintundo; a++) {
		if(indexar[a]) indexar[tot++]= a;
	}
	
	return tot;
}

static unsigned int sample_backbuf(int x, int y)
{
	unsigned int col;
	
	if(x>=curarea->winx || y>=curarea->winy) return 0;
	
	x+= curarea->winrct.xmin;
	y+= curarea->winrct.ymin;

#ifdef __APPLE__
	glReadBuffer(GL_AUX0);
#endif
	glReadPixels(x,  y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE,  &col);
	glReadBuffer(GL_BACK);	

	if(G.order==B_ENDIAN) SWITCH_INT(col);
		
	return framebuffer_to_index(col);
}

static int calc_vp_alpha_dl(VPaint *vp, DerivedMesh *dm, int vert, short *mval)
{
	float co[3], no[3];
	float fac, dx, dy;
	int alpha;
	short vertco[2];
	
	if(vp->flag & VP_SOFT) {
		dm->getVertCo(dm, vert, co);

	 	project_short_noclip(co, vertco);
		dx= mval[0]-vertco[0];
		dy= mval[1]-vertco[1];
		
		fac= sqrt(dx*dx + dy*dy);
		if(fac > vp->size) return 0;

		alpha= 255.0*vp->a*(1.0-fac/vp->size);
	}
	else {
		alpha= 255.0*vp->a;
	}

	if(vp->flag & VP_NORMALS) {
		dm->getVertNo(dm, vert, no);

			/* transpose ! */
		fac= vpimat[2][0]*no[0]+vpimat[2][1]*no[1]+vpimat[2][2]*no[2];
		if(fac>0.0) {
			dx= vpimat[0][0]*no[0]+vpimat[0][1]*no[1]+vpimat[0][2]*no[2];
			dy= vpimat[1][0]*no[0]+vpimat[1][1]*no[1]+vpimat[1][2]*no[2];
			
			alpha*= fac/sqrt(dx*dx + dy*dy + fac*fac);
		}
		else return 0;
	}
	
	return alpha;
}


void wpaint_undo (void)
{
	Object *ob= OBACT;
	Mesh	*me;
	MDeformVert *swapbuf;

	me = get_mesh(ob);
	if (!me)
		return;

	if (!wpaintundobuf)
		return;

	if (!me->dvert)
		return;

	if (totwpaintundo != me->totvert)
		return;

	swapbuf= me->dvert;

	/* copy undobuf to mesh */
	me->dvert= MEM_mallocN(sizeof(MDeformVert)*me->totvert, "deformVert");
	copy_dverts(me->dvert, wpaintundobuf, totwpaintundo);
	
	/* copy previous mesh to undo */
	free_dverts(wpaintundobuf, me->totvert);
	wpaintundobuf= MEM_mallocN(sizeof(MDeformVert)*me->totvert, "wpaintundo");
	copy_dverts(wpaintundobuf, swapbuf, totwpaintundo);
	
	/* now free previous mesh dverts */
	free_dverts(swapbuf, me->totvert);

	DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
	DAG_object_flush_update(G.scene, modifiers_isDeformedByArmature(ob), OB_RECALC_DATA);
	scrarea_do_windraw(curarea);
	
}

void copy_wpaint_undo (MDeformVert *dverts, int dcount)
{
	if (wpaintundobuf)
		free_dverts(wpaintundobuf, totwpaintundo);

	wpaintundobuf = MEM_mallocN (sizeof(MDeformVert)*dcount, "wpaintundo");
	totwpaintundo = dcount;
	copy_dverts (wpaintundobuf, dverts, dcount);
}

static void wpaint_blend(MDeformWeight *dw, MDeformWeight *uw, float alpha, float paintval)
{
	
	if(dw==NULL || uw==NULL) return;
	
	if(Gwp.mode==VP_MIX || Gwp.mode==VP_FILT)
		dw->weight = paintval*alpha + dw->weight*(1.0-alpha);
	else if(Gwp.mode==VP_ADD)
		dw->weight += paintval*alpha;
	else if(Gwp.mode==VP_SUB) 
		dw->weight -= paintval*alpha;
	else if(Gwp.mode==VP_MUL) 
		/* first mul, then blend the fac */
		dw->weight = ((1.0-alpha) + alpha*paintval)*dw->weight;
	
	CLAMP(dw->weight, 0.0f, 1.0f);
	
	/* if no spray, clip result with orig weight & orig alpha */
	if((Gwp.flag & VP_SPRAY)==0) {
		float testw=0.0f;
		
		alpha= Gwp.a;
		
		if(Gwp.mode==VP_MIX || Gwp.mode==VP_FILT)
			testw = paintval*alpha + uw->weight*(1.0-alpha);
		else if(Gwp.mode==VP_ADD)
			testw = uw->weight + paintval*alpha;
		else if(Gwp.mode==VP_SUB) 
			testw = uw->weight - paintval*alpha;
		else if(Gwp.mode==VP_MUL) 
			/* first mul, then blend the fac */
			testw = ((1.0-alpha) + alpha*paintval)*uw->weight;
		
		CLAMP(testw, 0.0f, 1.0f);
		
		if( testw<uw->weight ) {
			if(dw->weight < testw) dw->weight= testw;
			else if(dw->weight > uw->weight) dw->weight= uw->weight;
		}
		else {
			if(dw->weight > testw) dw->weight= testw;
			else if(dw->weight < uw->weight) dw->weight= uw->weight;
		}
	}
	
}

static MDeformWeight *get_defweight(MDeformVert *dv, int defgroup)
{
	int i;
	for (i=0; i<dv->totweight; i++){
		if (dv->dw[i].def_nr == defgroup)
			return dv->dw+i;
	}
	return NULL;
}

/* used for 3d view */
/* cant sample frontbuf, weight colors are interpolated too unpredictable */
/* so we return the closest value to vertex, wich is actually correct anyway */
void sample_wpaint()
{
	extern float editbutvweight;
	Object *ob= OBACT;
	Mesh *me= get_mesh(ob);
	int index;
	short mval[2], sco[2];
	
	getmouseco_areawin(mval);
	index= sample_backbuf(mval[0], mval[1]);
	
	if(index && index<=me->totface) {
		MFace *mface;
		DerivedMesh *dm;
		MDeformWeight *dw;
		float w1, w2, w3, w4, co[3], fac;
		int needsFree;
		
		dm = mesh_get_derived_deform(ob, &needsFree);
		
		mface= ((MFace *)me->mface) + index-1;
		
		/* calc 3 or 4 corner weights */
		dm->getVertCo(dm, mface->v1, co);
	 	project_short_noclip(co, sco);
		w1= ((mval[0]-sco[0])*(mval[0]-sco[0]) + (mval[1]-sco[1])*(mval[1]-sco[1]));
		
		dm->getVertCo(dm, mface->v2, co);
	 	project_short_noclip(co, sco);
		w2= ((mval[0]-sco[0])*(mval[0]-sco[0]) + (mval[1]-sco[1])*(mval[1]-sco[1]));
		
		dm->getVertCo(dm, mface->v3, co);
	 	project_short_noclip(co, sco);
		w3= ((mval[0]-sco[0])*(mval[0]-sco[0]) + (mval[1]-sco[1])*(mval[1]-sco[1]));
		
		if(mface->v4) {
			dm->getVertCo(dm, mface->v4, co);
			project_short_noclip(co, sco);
			w4= ((mval[0]-sco[0])*(mval[0]-sco[0]) + (mval[1]-sco[1])*(mval[1]-sco[1]));
		}
		else w4= 1.0e10;
		
		fac= MIN4(w1, w2, w3, w4);
		if(w1==fac) {
			dw= get_defweight(me->dvert+mface->v1, ob->actdef-1);
			if(dw) editbutvweight= dw->weight; else editbutvweight= 0.0f;
		}
		else if(w2==fac) {
			dw= get_defweight(me->dvert+mface->v2, ob->actdef-1);
			if(dw) editbutvweight= dw->weight; else editbutvweight= 0.0f;
		}
		else if(w3==fac) {
			dw= get_defweight(me->dvert+mface->v3, ob->actdef-1);
			if(dw) editbutvweight= dw->weight; else editbutvweight= 0.0f;
		}
		else if(w4==fac) {
			if(mface->v4) {
				dw= get_defweight(me->dvert+mface->v4, ob->actdef-1);
				if(dw) editbutvweight= dw->weight; else editbutvweight= 0.0f;
			}
		}
		
		if (needsFree)
			dm->release(dm);
		
	}
	allqueue(REDRAWBUTSEDIT, 0);
	
}


void weight_paint(void)
{
	extern float editbutvweight;
	MDeformWeight	*dw, *uw;
	Object *ob; 
	Mesh *me;
	MFace *mface;
	TFace *tface;
	float mat[4][4], imat[4][4], paintweight;
	int index, totindex, alpha, totw;
	short mval[2], mvalo[2], firsttime=1, mousebut;

	if((G.f & G_WEIGHTPAINT)==0) return;
	if(G.obedit) return;
	
	if(G.qual & LR_CTRLKEY) {
		sample_wpaint();
		return;
	}
	
	if(indexar==NULL) init_vertexpaint();
	
	ob= OBACT;
	if(ob->id.lib) return;

	me= get_mesh(ob);
	if(me==NULL || me->totface==0) return;
	
	/* if nothing was added yet, we make dverts and a vertex deform group */
	if (!me->dvert)
		create_dverts(me);
	
	/* this happens on a Bone select, when no vgroup existed yet */
	if(ob->actdef==0) {
		Object *modob;
		if((modob = modifiers_isDeformedByArmature(ob))) {
			bPoseChannel *pchan;
			for(pchan= modob->pose->chanbase.first; pchan; pchan= pchan->next)
				if(pchan->bone->flag & SELECT)
					break;
			if(pchan) {
				bDeformGroup *dg= get_named_vertexgroup(ob, pchan->name);
				if(dg==NULL)
					dg= add_defgroup_name(ob, pchan->name);	// sets actdef
				else
					ob->actdef= get_defgroup_num(ob, dg);
				allqueue(REDRAWBUTSEDIT, 0);
			}
		}
	}
	if(ob->defbase.first==NULL) {
		add_defgroup(ob);
		allqueue(REDRAWBUTSEDIT, 0);
	}	
	
	if(ob->lay & G.vd->lay); else error("Active object is not in this layer");
	
	persp(PERSP_VIEW);
	/* imat for normals */
	Mat4MulMat4(mat, ob->obmat, G.vd->viewmat);
	Mat4Invert(imat, mat);
	Mat3CpyMat4(vpimat, imat);
	
	/* load projection matrix */
	mymultmatrix(ob->obmat);
	mygetsingmatrix(mat);
	myloadmatrix(G.vd->viewmat);
	
	getmouseco_areawin(mvalo);
	
	if(me->tface) tface_to_mcol(me);
	copy_vpaint_undo( (unsigned int *)me->mcol, me->totface);
	copy_wpaint_undo(me->dvert, me->totvert);
	
	getmouseco_areawin(mval);
	mvalo[0]= mval[0];
	mvalo[1]= mval[1];
	
	if (U.flag & USER_LMOUSESELECT) mousebut = R_MOUSE;
	else mousebut = L_MOUSE;
	
	while (get_mbut() & mousebut) {
		getmouseco_areawin(mval);
		
		if(firsttime || mval[0]!=mvalo[0] || mval[1]!=mvalo[1]) {
			DerivedMesh *dm;
			int needsFree;

			firsttime= 0;
			
			/* which faces are involved */
			if(Gwp.flag & VP_AREA) {
				totindex= sample_backbuf_area(mval[0], mval[1], Gwp.size);
			}
			else {
				indexar[0]= sample_backbuf(mval[0], mval[1]);
				if(indexar[0]) totindex= 1;
				else totindex= 0;
			}
			
			MTC_Mat4SwapMat4(G.vd->persmat, mat);
			
			if(Gwp.flag & VP_COLINDEX) {
				for(index=0; index<totindex; index++) {
					if(indexar[index] && indexar[index]<=me->totface) {
					
						mface= ((MFace *)me->mface) + (indexar[index]-1);
					
						if(mface->mat_nr!=ob->actcol-1) {
							indexar[index]= 0;
						}
					}					
				}
			}

			if((G.f & G_FACESELECT) && me->tface) {
				for(index=0; index<totindex; index++) {
					if(indexar[index] && indexar[index]<=me->totface) {
					
						tface= ((TFace *)me->tface) + (indexar[index]-1);
					
						if((tface->flag & TF_SELECT)==0) {
							indexar[index]= 0;
						}
					}					
				}
			}
			
			/* make sure each vertex gets treated only once */
			/* and calculate filter weight */
			totw= 0;
			if(Gwp.mode==VP_FILT) 
				paintweight= 0.0f;
			else
				paintweight= editbutvweight;
			
			for(index=0; index<totindex; index++) {
				if(indexar[index] && indexar[index]<=me->totface) {
					mface= me->mface + (indexar[index]-1);
					
					(me->dvert+mface->v1)->flag= 1;
					(me->dvert+mface->v2)->flag= 1;
					(me->dvert+mface->v3)->flag= 1;
					if(mface->v4) (me->dvert+mface->v4)->flag= 1;
					
					if(Gwp.mode==VP_FILT) {
						dw= verify_defweight(me->dvert+mface->v1, ob->actdef-1);
						if(dw) {paintweight+= dw->weight; totw++;}
						dw= verify_defweight(me->dvert+mface->v2, ob->actdef-1);
						if(dw) {paintweight+= dw->weight; totw++;}
						dw= verify_defweight(me->dvert+mface->v3, ob->actdef-1);
						if(dw) {paintweight+= dw->weight; totw++;}
						if(mface->v4) {
							dw= verify_defweight(me->dvert+mface->v4, ob->actdef-1);
							if(dw) {paintweight+= dw->weight; totw++;}
						}
					}
				}
			}
			
			if(Gwp.mode==VP_FILT) 
				paintweight/= (float)totw;
			
			dm = mesh_get_derived_deform(ob, &needsFree);

			for(index=0; index<totindex; index++) {
				
				if(indexar[index] && indexar[index]<=me->totface) {
					mface= me->mface + (indexar[index]-1);
					
					if((me->dvert+mface->v1)->flag) {
						alpha= calc_vp_alpha_dl(&Gwp, dm, mface->v1, mval);
						if(alpha) {
							dw= verify_defweight(me->dvert+mface->v1, ob->actdef-1);
							uw= verify_defweight(wpaintundobuf+mface->v1, ob->actdef-1);
							wpaint_blend(dw, uw, (float)alpha/255.0, paintweight);
						}
						(me->dvert+mface->v1)->flag= 0;
					}
					
					if((me->dvert+mface->v2)->flag) {
						alpha= calc_vp_alpha_dl(&Gwp, dm, mface->v2, mval);
						if(alpha) {
							dw= verify_defweight(me->dvert+mface->v2, ob->actdef-1);
							uw= verify_defweight(wpaintundobuf+mface->v2, ob->actdef-1);
							wpaint_blend(dw, uw, (float)alpha/255.0, paintweight);
						}
						(me->dvert+mface->v2)->flag= 0;
					}
					
					if((me->dvert+mface->v3)->flag) {
						alpha= calc_vp_alpha_dl(&Gwp, dm, mface->v3, mval);
						if(alpha) {
							dw= verify_defweight(me->dvert+mface->v3, ob->actdef-1);
							uw= verify_defweight(wpaintundobuf+mface->v3, ob->actdef-1);
							wpaint_blend(dw, uw, (float)alpha/255.0, paintweight);
						}
						(me->dvert+mface->v3)->flag= 0;
					}
					
					if((me->dvert+mface->v4)->flag) {
						if(mface->v4) {
							alpha= calc_vp_alpha_dl(&Gwp, dm, mface->v4, mval);
							if(alpha) {
								dw= verify_defweight(me->dvert+mface->v4, ob->actdef-1);
								uw= verify_defweight(wpaintundobuf+mface->v4, ob->actdef-1);
								wpaint_blend(dw, uw, (float)alpha/255.0, paintweight);
							}
							(me->dvert+mface->v4)->flag= 0;
						}
					}
				}
			}
			if (needsFree)
				dm->release(dm);
			
			MTC_Mat4SwapMat4(G.vd->persmat, mat);
			
		}
		else BIF_wait_for_statechange();
		
		if(mval[0]!=mvalo[0] || mval[1]!=mvalo[1]) {

			DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
			scrarea_do_windraw(curarea);
			
			if(Gwp.flag & (VP_AREA|VP_SOFT)) {
				/* draw circle in backbuf! */
				persp(PERSP_WIN);
				fdrawXORcirc((float)mval[0], (float)mval[1], Gwp.size);
				persp(PERSP_VIEW);
			}

			screen_swapbuffers();
			backdrawview3d(0);
	
			mvalo[0]= mval[0];
			mvalo[1]= mval[1];
		}
	}
	
	if(me->tface) {
		MEM_freeN(me->mcol);
		me->mcol= 0;
	}
	
	DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
	// this flag is event for softbody to refresh weightpaint values
	if(ob->soft) ob->softflag |= OB_SB_REDO;
	
	allqueue(REDRAWVIEW3D, 0);

}

void vertex_paint()
{
	Object *ob;
	Mesh *me;
	MFace *mface;
	TFace *tface;
	float mat[4][4], imat[4][4];
	unsigned int paintcol=0, *mcol, *mcolorig, fcol1, fcol2;
	int index, alpha, totindex;
	short mval[2], mvalo[2], firsttime=1, mousebut;
	
	if((G.f & G_VERTEXPAINT)==0) return;
	if(G.obedit) return;
	
	if(indexar==NULL) init_vertexpaint();
	
	ob= OBACT;
	if(ob->id.lib) return;

	me= get_mesh(ob);
	if(me==NULL || me->totface==0) return;
	if(ob->lay & G.vd->lay); else error("Active object is not in this layer");
	
	if(me->tface==NULL && me->mcol==NULL) make_vertexcol();

	if(me->tface==NULL && me->mcol==NULL) return;
	
	persp(PERSP_VIEW);
	/* imat for normals */
	Mat4MulMat4(mat, ob->obmat, G.vd->viewmat);
	Mat4Invert(imat, mat);
	Mat3CpyMat4(vpimat, imat);
	
	/* load projection matrix */
	mymultmatrix(ob->obmat);
	mygetsingmatrix(mat);
	myloadmatrix(G.vd->viewmat);
	
	paintcol= vpaint_get_current_col();
	
	getmouseco_areawin(mvalo);
	
	if(me->tface) tface_to_mcol(me);
	copy_vpaint_undo( (unsigned int *)me->mcol, me->totface);
	
	getmouseco_areawin(mval);
	mvalo[0]= mval[0];
	mvalo[1]= mval[1];
	
	if (U.flag & USER_LMOUSESELECT) mousebut = R_MOUSE;
	else mousebut = L_MOUSE;
	
	while (get_mbut() & mousebut) {
		getmouseco_areawin(mval);
		
		if(firsttime || mval[0]!=mvalo[0] || mval[1]!=mvalo[1]) {
			DerivedMesh *dm;
			int needsFree;

			firsttime= 0;

			/* which faces are involved */
			if(Gvp.flag & VP_AREA) {
				totindex= sample_backbuf_area(mval[0], mval[1], Gvp.size);
			}
			else {
				indexar[0]= sample_backbuf(mval[0], mval[1]);
				if(indexar[0]) totindex= 1;
				else totindex= 0;
			}
			
			MTC_Mat4SwapMat4(G.vd->persmat, mat);
			
			if(Gvp.flag & VP_COLINDEX) {
				for(index=0; index<totindex; index++) {
					if(indexar[index] && indexar[index]<=me->totface) {
					
						mface= ((MFace *)me->mface) + (indexar[index]-1);
					
						if(mface->mat_nr!=ob->actcol-1) {
							indexar[index]= 0;
						}
					}					
				}
			}
			if((G.f & G_FACESELECT) && me->tface) {
				for(index=0; index<totindex; index++) {
					if(indexar[index] && indexar[index]<=me->totface) {
					
						tface= ((TFace *)me->tface) + (indexar[index]-1);
					
						if((tface->flag & TF_SELECT)==0) {
							indexar[index]= 0;
						}
					}					
				}
			}

			dm= mesh_get_derived_deform(ob, &needsFree);
			for(index=0; index<totindex; index++) {

				if(indexar[index] && indexar[index]<=me->totface) {
				
					mface= ((MFace *)me->mface) + (indexar[index]-1);
					mcol=	  ( (unsigned int *)me->mcol) + 4*(indexar[index]-1);
					mcolorig= ( (unsigned int *)vpaintundobuf) + 4*(indexar[index]-1);

					if(Gvp.mode==VP_FILT) {
						fcol1= mcol_blend( mcol[0], mcol[1], 128);
						if(mface->v4) {
							fcol2= mcol_blend( mcol[2], mcol[3], 128);
							paintcol= mcol_blend( fcol1, fcol2, 128);
						}
						else {
							paintcol= mcol_blend( mcol[2], fcol1, 170);
						}
						
					}
					
					alpha= calc_vp_alpha_dl(&Gvp, dm, mface->v1, mval);
					if(alpha) vpaint_blend( mcol, mcolorig, paintcol, alpha);
					
					alpha= calc_vp_alpha_dl(&Gvp, dm, mface->v2, mval);
					if(alpha) vpaint_blend( mcol+1, mcolorig+1, paintcol, alpha);
	
					alpha= calc_vp_alpha_dl(&Gvp, dm, mface->v3, mval);
					if(alpha) vpaint_blend( mcol+2, mcolorig+2, paintcol, alpha);

					if(mface->v4) {
						alpha= calc_vp_alpha_dl(&Gvp, dm, mface->v4, mval);
						if(alpha) vpaint_blend( mcol+3, mcolorig+3, paintcol, alpha);
					}
				}
			}
			if (needsFree)
				dm->release(dm);
				
			MTC_Mat4SwapMat4(G.vd->persmat, mat);
			
			do_shared_vertexcol(me);
			if(me->tface) {
				mcol_to_tface(me, 0);
			}
	
			DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
			scrarea_do_windraw(curarea);

			if(Gvp.flag & (VP_AREA|VP_SOFT)) {
				/* draw circle in backbuf! */
				persp(PERSP_WIN);
				fdrawXORcirc((float)mval[0], (float)mval[1], Gvp.size);
				persp(PERSP_VIEW);
			}
			screen_swapbuffers();
			backdrawview3d(0);
			
			mvalo[0]= mval[0];
			mvalo[1]= mval[1];
		}
		else BIF_wait_for_statechange();
	}
	
	if(me->tface) {
		MEM_freeN(me->mcol);
		me->mcol= 0;
	}
	
	allqueue(REDRAWVIEW3D, 0);
}

void set_wpaint(void)		/* toggle */
{		
	Object *ob;
	Mesh *me;
	
	scrarea_queue_headredraw(curarea);
	ob= OBACT;
	if(ob->id.lib) return;
	me= get_mesh(ob);
		
	if(me && me->totface>=MAXINDEX) {
		error("Maximum number of faces: %d", MAXINDEX-1);
		G.f &= ~G_WEIGHTPAINT;
		return;
	}
	
	if(G.f & G_WEIGHTPAINT) G.f &= ~G_WEIGHTPAINT;
	else G.f |= G_WEIGHTPAINT;
	
	allqueue(REDRAWVIEW3D, 1);	/* including header */
	allqueue(REDRAWBUTSEDIT, 0);
	
		/* Weightpaint works by overriding colors in mesh,
		 * so need to make sure we recalc on enter and
		 * exit (exit needs doing regardless because we
		 * should redeform).
		 */
	if (me) {
		DAG_object_flush_update(G.scene, OBACT, OB_RECALC_DATA);
	}

	if(G.f & G_WEIGHTPAINT) {
		setcursor_space(SPACE_VIEW3D, CURSOR_VPAINT);
	}
	else {
		freefastshade();	/* to be sure */
		if(!(G.f & G_FACESELECT))
			setcursor_space(SPACE_VIEW3D, CURSOR_STD);
	}
}


void set_vpaint(void)		/* toggle */
{		
	Object *ob;
	Mesh *me;
	
	scrarea_queue_headredraw(curarea);
	ob= OBACT;
	if(ob->id.lib) {
		G.f &= ~G_VERTEXPAINT;
		return;
	}
	
	me= get_mesh(ob);
	
	if(me && me->totface>=MAXINDEX) {
		error("Maximum number of faces: %d", MAXINDEX-1);
		G.f &= ~G_VERTEXPAINT;
		return;
	}
	
	if(me && me->tface==NULL && me->mcol==NULL) make_vertexcol();
	
	if(G.f & G_VERTEXPAINT){
		G.f &= ~G_VERTEXPAINT;
	}
	else {
		G.f |= G_VERTEXPAINT;
		/* Turn off weight painting */
		if (G.f & G_WEIGHTPAINT)
			set_wpaint();
	}
	
	allqueue(REDRAWVIEW3D, 1); 	/* including header */
	allqueue(REDRAWBUTSEDIT, 0);
	
	if(G.f & G_VERTEXPAINT) {
		setcursor_space(SPACE_VIEW3D, CURSOR_VPAINT);
	}
	else {
		freefastshade();	/* to be sure */
		if (me) {
			DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
		}
		if((G.f & G_FACESELECT)==0) setcursor_space(SPACE_VIEW3D, CURSOR_STD);
	}
}

