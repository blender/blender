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
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <math.h>
#include <string.h>

#ifdef WIN32
#include <io.h>
#else
#include <unistd.h>
#endif   

#include "MEM_guardedalloc.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_ghash.h"

#include "DNA_anim_types.h"
#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_brush_types.h"
#include "DNA_cloth_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_object_force.h"
#include "DNA_particle_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_scene_types.h"
#include "DNA_view3d_types.h"
#include "DNA_userdef_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "BKE_armature.h"
#include "BKE_action.h"
#include "BKE_brush.h"
#include "BKE_DerivedMesh.h"
#include "BKE_cloth.h"
#include "BKE_context.h"
#include "BKE_customdata.h"
#include "BKE_depsgraph.h"
#include "BKE_deform.h"
#include "BKE_displist.h"
#include "BKE_global.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_utildefines.h"

#include "WM_api.h"
#include "WM_types.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "ED_armature.h"
#include "ED_mesh.h"
#include "ED_object.h"
#include "ED_screen.h"
#include "ED_util.h"
#include "ED_view3d.h"

#include "paint_intern.h"

/* brush->vertexpaint_tool */
#define VP_MIX	0
#define VP_ADD	1
#define VP_SUB	2
#define VP_MUL	3
#define VP_BLUR	4
#define VP_LIGHTEN	5
#define VP_DARKEN	6

#define MAXINDEX	512000

/* XXX */
static void error() {}

/* polling - retrieve whether cursor should be set or operator should be done */


/* Returns true if vertex paint mode is active */
int vertex_paint_mode_poll(bContext *C)
{
	Object *ob = CTX_data_active_object(C);

	return ob && ob->mode == OB_MODE_VERTEX_PAINT;
}

int vertex_paint_poll(bContext *C)
{
	if(vertex_paint_mode_poll(C) && 
	   paint_brush(&CTX_data_tool_settings(C)->vpaint->paint)) {
		ScrArea *sa= CTX_wm_area(C);
		if(sa->spacetype==SPACE_VIEW3D) {
			ARegion *ar= CTX_wm_region(C);
			if(ar->regiontype==RGN_TYPE_WINDOW)
				return 1;
		}
	}
	return 0;
}

int weight_paint_mode_poll(bContext *C)
{
	Object *ob = CTX_data_active_object(C);

	return ob && ob->mode == OB_MODE_WEIGHT_PAINT;
}

int weight_paint_poll(bContext *C)
{
	Object *ob = CTX_data_active_object(C);

	if(ob && ob->mode & OB_MODE_WEIGHT_PAINT &&
	   paint_brush(&CTX_data_tool_settings(C)->wpaint->paint)) {
		ScrArea *sa= CTX_wm_area(C);
		if(sa->spacetype==SPACE_VIEW3D) {
			ARegion *ar= CTX_wm_region(C);
			if(ar->regiontype==RGN_TYPE_WINDOW)
				return 1;
		}
	}
	return 0;
}

static VPaint *new_vpaint(int wpaint)
{
	VPaint *vp= MEM_callocN(sizeof(VPaint), "VPaint");
	
	vp->flag= VP_AREA+VP_SPRAY;
	
	if(wpaint)
		vp->flag= VP_AREA;

	return vp;
}

static int *get_indexarray(void)
{
	return MEM_mallocN(sizeof(int)*MAXINDEX + 2, "vertexpaint");
}


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

unsigned int vpaint_get_current_col(VPaint *vp)
{
	Brush *brush = paint_brush(&vp->paint);
	return rgba_to_mcol(brush->rgb[0], brush->rgb[1], brush->rgb[2], 1.0f);
}

static void do_shared_vertexcol(Mesh *me)
{
	/* if no mcol: do not do */
	/* if tface: only the involved faces, otherwise all */
	MFace *mface;
	MTFace *tface;
	int a;
	short *scolmain, *scol;
	char *mcol;
	
	if(me->mcol==0 || me->totvert==0 || me->totface==0) return;
	
	scolmain= MEM_callocN(4*sizeof(short)*me->totvert, "colmain");
	
	tface= me->mtface;
	mface= me->mface;
	mcol= (char *)me->mcol;
	for(a=me->totface; a>0; a--, mface++, mcol+=16) {
		if((tface && tface->mode & TF_SHAREDCOL) || (me->editflag & ME_EDIT_PAINT_MASK)==0) {
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
	
	tface= me->mtface;
	mface= me->mface;
	mcol= (char *)me->mcol;
	for(a=me->totface; a>0; a--, mface++, mcol+=16) {
		if((tface && tface->mode & TF_SHAREDCOL) || (me->editflag & ME_EDIT_PAINT_MASK)==0) {
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

static void make_vertexcol(Object *ob)	/* single ob */
{
	Mesh *me;
	if(!ob || ob->id.lib) return;
	me= get_mesh(ob);
	if(me==0) return;
	if(me->edit_mesh) return;

	/* copies from shadedisplist to mcol */
	if(!me->mcol) {
		CustomData_add_layer(&me->fdata, CD_MCOL, CD_CALLOC, NULL, me->totface);
		mesh_update_customdata_pointers(me);
	}

	//if(shade)
	//	shadeMeshMCol(scene, ob, me);
	//else

	memset(me->mcol, 255, 4*sizeof(MCol)*me->totface);
	
	DAG_id_flush_update(&me->id, OB_RECALC_DATA);
	
}

static void copy_vpaint_prev(VPaint *vp, unsigned int *mcol, int tot)
{
	if(vp->vpaint_prev) {
		MEM_freeN(vp->vpaint_prev);
		vp->vpaint_prev= NULL;
	}
	vp->tot= tot;	
	
	if(mcol==NULL || tot==0) return;
	
	vp->vpaint_prev= MEM_mallocN(4*sizeof(int)*tot, "vpaint_prev");
	memcpy(vp->vpaint_prev, mcol, 4*sizeof(int)*tot);
	
}

static void copy_wpaint_prev (VPaint *wp, MDeformVert *dverts, int dcount)
{
	if (wp->wpaint_prev) {
		free_dverts(wp->wpaint_prev, wp->tot);
		wp->wpaint_prev= NULL;
	}
	
	if(dverts && dcount) {
		
		wp->wpaint_prev = MEM_mallocN (sizeof(MDeformVert)*dcount, "wpaint prev");
		wp->tot = dcount;
		copy_dverts (wp->wpaint_prev, dverts, dcount);
	}
}


void vpaint_fill(Object *ob, unsigned int paintcol)
{
	Mesh *me;
	MFace *mf;
	unsigned int *mcol;
	int i, selected;

	me= get_mesh(ob);
	if(me==0 || me->totface==0) return;

	if(!me->mcol)
		make_vertexcol(ob);

	selected= (me->editflag & ME_EDIT_PAINT_MASK);

	mf = me->mface;
	mcol = (unsigned int*)me->mcol;
	for (i = 0; i < me->totface; i++, mf++, mcol+=4) {
		if (!selected || mf->flag & ME_FACE_SEL) {
			mcol[0] = paintcol;
			mcol[1] = paintcol;
			mcol[2] = paintcol;
			mcol[3] = paintcol;
		}
	}
	
	DAG_id_flush_update(&me->id, OB_RECALC_DATA);
}


/* fills in the selected faces with the current weight and vertex group */
void wpaint_fill(VPaint *wp, Object *ob, float paintweight)
{
	Mesh *me;
	MFace *mface;
	MDeformWeight *dw, *uw;
	int *indexar;
	int index, vgroup;
	unsigned int faceverts[5]={0,0,0,0,0};
	unsigned char i;
	int vgroup_mirror= -1;
	int selected;
	
	me= ob->data;
	if(me==0 || me->totface==0 || me->dvert==0 || !me->mface) return;
	
	selected= (me->editflag & ME_EDIT_PAINT_MASK);

	indexar= get_indexarray();
	if(selected) {
		for(index=0, mface=me->mface; index<me->totface; index++, mface++) {
			if((mface->flag & ME_FACE_SEL)==0)
				indexar[index]= 0;
			else
				indexar[index]= index+1;
		}
	}
	else {
		for(index=0; index<me->totface; index++)
			indexar[index]= index+1;
	}
	
	vgroup= ob->actdef-1;
	
	/* directly copied from weight_paint, should probaby split into a separate function */
	/* if mirror painting, find the other group */		
	if(me->editflag & ME_EDIT_MIRROR_X) {
		bDeformGroup *defgroup= BLI_findlink(&ob->defbase, ob->actdef-1);
		if(defgroup) {
			bDeformGroup *curdef;
			int actdef= 0;
			char name[32];

			BLI_strncpy(name, defgroup->name, 32);
			bone_flip_name(name, 0);		/* 0 = don't strip off number extensions */
			
			for (curdef = ob->defbase.first; curdef; curdef=curdef->next, actdef++)
				if (!strcmp(curdef->name, name))
					break;
			if(curdef==NULL) {
				int olddef= ob->actdef;	/* tsk, ED_vgroup_add sets the active defgroup */
				curdef= ED_vgroup_add_name (ob, name);
				ob->actdef= olddef;
			}
			
			if(curdef && curdef!=defgroup)
				vgroup_mirror= actdef;
		}
	}
	/* end copy from weight_paint*/
	
	copy_wpaint_prev(wp, me->dvert, me->totvert);
	
	for(index=0; index<me->totface; index++) {
		if(indexar[index] && indexar[index]<=me->totface) {
			mface= me->mface + (indexar[index]-1);
			/* just so we can loop through the verts */
			faceverts[0]= mface->v1;
			faceverts[1]= mface->v2;
			faceverts[2]= mface->v3;
			faceverts[3]= mface->v4;
			for (i=0; i<3 || faceverts[i]; i++) {
				if(!((me->dvert+faceverts[i])->flag)) {
					dw= ED_vgroup_weight_verify(me->dvert+faceverts[i], vgroup);
					if(dw) {
						uw= ED_vgroup_weight_verify(wp->wpaint_prev+faceverts[i], vgroup);
						uw->weight= dw->weight; /* set the undo weight */
						dw->weight= paintweight;
						
						if(me->editflag & ME_EDIT_MIRROR_X) {	/* x mirror painting */
							int j= mesh_get_x_mirror_vert(ob, faceverts[i]);
							if(j>=0) {
								/* copy, not paint again */
								if(vgroup_mirror != -1) {
									dw= ED_vgroup_weight_verify(me->dvert+j, vgroup_mirror);
									uw= ED_vgroup_weight_verify(wp->wpaint_prev+j, vgroup_mirror);
								} else {
									dw= ED_vgroup_weight_verify(me->dvert+j, vgroup);
									uw= ED_vgroup_weight_verify(wp->wpaint_prev+j, vgroup);
								}
								uw->weight= dw->weight; /* set the undo weight */
								dw->weight= paintweight;
							}
						}
					}
					(me->dvert+faceverts[i])->flag= 1;
				}
			}
		}
	}
	
	index=0;
	while (index<me->totvert) {
		(me->dvert+index)->flag= 0;
		index++;
	}
	
	MEM_freeN(indexar);
	copy_wpaint_prev(wp, NULL, 0);

	DAG_id_flush_update(&me->id, OB_RECALC_DATA);
}

/* XXX: should be re-implemented as a vertex/weight paint 'colour correct' operator
 
void vpaint_dogamma(Scene *scene)
{
	VPaint *vp= scene->toolsettings->vpaint;
	Mesh *me;
	Object *ob;
	float igam, fac;
	int a, temp;
	unsigned char *cp, gamtab[256];

	ob= OBACT;
	me= get_mesh(ob);

	if(!(ob->mode & OB_MODE_VERTEX_PAINT)) return;
	if(me==0 || me->mcol==0 || me->totface==0) return;

	igam= 1.0/vp->gamma;
	for(a=0; a<256; a++) {
		
		fac= ((float)a)/255.0;
		fac= vp->mul*pow( fac, igam);
		
		temp= 255.9*fac;
		
		if(temp<=0) gamtab[a]= 0;
		else if(temp>=255) gamtab[a]= 255;
		else gamtab[a]= temp;
	}

	a= 4*me->totface;
	cp= (unsigned char *)me->mcol;
	while(a--) {
		
		cp[1]= gamtab[ cp[1] ];
		cp[2]= gamtab[ cp[2] ];
		cp[3]= gamtab[ cp[3] ];
		
		cp+= 4;
	}
}
 */

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
	cp[1]= (mfac*cp1[1]+fac*cp2[1])/255;
	cp[2]= (mfac*cp1[2]+fac*cp2[2])/255;
	cp[3]= (mfac*cp1[3]+fac*cp2[3])/255;
	
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
	temp= cp1[1] + ((fac*cp2[1])/255);
	if(temp>254) cp[1]= 255; else cp[1]= temp;
	temp= cp1[2] + ((fac*cp2[2])/255);
	if(temp>254) cp[2]= 255; else cp[2]= temp;
	temp= cp1[3] + ((fac*cp2[3])/255);
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
	temp= cp1[1] - ((fac*cp2[1])/255);
	if(temp<0) cp[1]= 0; else cp[1]= temp;
	temp= cp1[2] - ((fac*cp2[2])/255);
	if(temp<0) cp[2]= 0; else cp[2]= temp;
	temp= cp1[3] - ((fac*cp2[3])/255);
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
	cp[1]= (mfac*cp1[1] + fac*((cp2[1]*cp1[1])/255)  )/255;
	cp[2]= (mfac*cp1[2] + fac*((cp2[2]*cp1[2])/255)  )/255;
	cp[3]= (mfac*cp1[3] + fac*((cp2[3]*cp1[3])/255)  )/255;

	
	return col;
}

static unsigned int mcol_lighten(unsigned int col1, unsigned int col2, int fac)
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
	
	/* See if are lighter, if so mix, else dont do anything.
	if the paint col is darker then the original, then ignore */
	if (cp1[1]+cp1[2]+cp1[3] > cp2[1]+cp2[2]+cp2[3])
		return col1;
	
	cp[0]= 255;
	cp[1]= (mfac*cp1[1]+fac*cp2[1])/255;
	cp[2]= (mfac*cp1[2]+fac*cp2[2])/255;
	cp[3]= (mfac*cp1[3]+fac*cp2[3])/255;
	
	return col;
}

static unsigned int mcol_darken(unsigned int col1, unsigned int col2, int fac)
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
	
	/* See if were darker, if so mix, else dont do anything.
	if the paint col is brighter then the original, then ignore */
	if (cp1[1]+cp1[2]+cp1[3] < cp2[1]+cp2[2]+cp2[3])
		return col1;
	
	cp[0]= 255;
	cp[1]= (mfac*cp1[1]+fac*cp2[1])/255;
	cp[2]= (mfac*cp1[2]+fac*cp2[2])/255;
	cp[3]= (mfac*cp1[3]+fac*cp2[3])/255;
	return col;
}

static void vpaint_blend(VPaint *vp, unsigned int *col, unsigned int *colorig, unsigned int paintcol, int alpha)
{
	Brush *brush = paint_brush(&vp->paint);

	if(brush->vertexpaint_tool==VP_MIX || brush->vertexpaint_tool==VP_BLUR) *col= mcol_blend( *col, paintcol, alpha);
	else if(brush->vertexpaint_tool==VP_ADD) *col= mcol_add( *col, paintcol, alpha);
	else if(brush->vertexpaint_tool==VP_SUB) *col= mcol_sub( *col, paintcol, alpha);
	else if(brush->vertexpaint_tool==VP_MUL) *col= mcol_mul( *col, paintcol, alpha);
	else if(brush->vertexpaint_tool==VP_LIGHTEN) *col= mcol_lighten( *col, paintcol, alpha);
	else if(brush->vertexpaint_tool==VP_DARKEN) *col= mcol_darken( *col, paintcol, alpha);
	
	/* if no spray, clip color adding with colorig & orig alpha */
	if((vp->flag & VP_SPRAY)==0) {
		unsigned int testcol=0, a;
		char *cp, *ct, *co;
		
		alpha= (int)(255.0*brush->alpha);
		
		if(brush->vertexpaint_tool==VP_MIX || brush->vertexpaint_tool==VP_BLUR) testcol= mcol_blend( *colorig, paintcol, alpha);
		else if(brush->vertexpaint_tool==VP_ADD) testcol= mcol_add( *colorig, paintcol, alpha);
		else if(brush->vertexpaint_tool==VP_SUB) testcol= mcol_sub( *colorig, paintcol, alpha);
		else if(brush->vertexpaint_tool==VP_MUL) testcol= mcol_mul( *colorig, paintcol, alpha);
		else if(brush->vertexpaint_tool==VP_LIGHTEN)  testcol= mcol_lighten( *colorig, paintcol, alpha);
		else if(brush->vertexpaint_tool==VP_DARKEN)   testcol= mcol_darken( *colorig, paintcol, alpha);
		
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


static int sample_backbuf_area(ViewContext *vc, int *indexar, int totface, int x, int y, float size)
{
	struct ImBuf *ibuf;
	int a, tot=0, index;
	
	if(totface+4>=MAXINDEX) return 0;
	
	/* brecht: disabled this because it obviously failes for
	   brushes with size > 64, why is this here? */
	/*if(size>64.0) size= 64.0;*/
	
	ibuf= view3d_read_backbuf(vc, x-size, y-size, x+size, y+size);
	if(ibuf) {
		unsigned int *rt= ibuf->rect;

		memset(indexar, 0, sizeof(int)*totface+4);	/* plus 2! first element is total, +2 was giving valgrind errors, +4 seems ok */
		
		size= ibuf->x*ibuf->y;
		while(size--) {
				
			if(*rt) {
				index= WM_framebuffer_to_index(*rt);
				if(index>0 && index<=totface)
					indexar[index] = 1;
			}
		
			rt++;
		}
		
		for(a=1; a<=totface; a++) {
			if(indexar[a]) indexar[tot++]= a;
		}

		IMB_freeImBuf(ibuf);
	}
	
	return tot;
}

static float calc_vp_alpha_dl(VPaint *vp, ViewContext *vc, float vpimat[][3], float *vert_nor, float *mval, float pressure)
{
	Brush *brush = paint_brush(&vp->paint);
	float fac, fac_2, size, dx, dy;
	float alpha;
	short vertco[2];
	
	project_short_noclip(vc->ar, vert_nor, vertco);
	dx= mval[0]-vertco[0];
	dy= mval[1]-vertco[1];
	
	if (brush->flag & BRUSH_SIZE_PRESSURE)
		size = pressure * brush->size;
	else
		size = brush->size;
	
	fac_2= dx*dx + dy*dy;
	if(fac_2 > size*size) return 0.f;
	fac = sqrtf(fac_2);
	
	alpha= brush->alpha * brush_curve_strength_clamp(brush, fac, size);
	
	if (brush->flag & BRUSH_ALPHA_PRESSURE)
		alpha *= pressure;
		
	if(vp->flag & VP_NORMALS) {
		float *no= vert_nor+3;
		
		/* transpose ! */
		fac= vpimat[2][0]*no[0]+vpimat[2][1]*no[1]+vpimat[2][2]*no[2];
		if(fac>0.0) {
			dx= vpimat[0][0]*no[0]+vpimat[0][1]*no[1]+vpimat[0][2]*no[2];
			dy= vpimat[1][0]*no[0]+vpimat[1][1]*no[1]+vpimat[1][2]*no[2];
			
			alpha*= fac/sqrtf(dx*dx + dy*dy + fac*fac);
		}
		else return 0.f;
	}
	
	return alpha;
}

static void wpaint_blend(VPaint *wp, MDeformWeight *dw, MDeformWeight *uw, float alpha, float paintval, int flip)
{
	Brush *brush = paint_brush(&wp->paint);
	int tool = brush->vertexpaint_tool;
	
	if(dw==NULL || uw==NULL) return;
	
	if (flip) {
		switch(tool) {
			case VP_MIX:
				paintval = 1.f - paintval; break;
			case VP_ADD:
				tool= VP_SUB; break;
			case VP_SUB:
				tool= VP_ADD; break;
			case VP_LIGHTEN:
				tool= VP_DARKEN; break;
			case VP_DARKEN:
				tool= VP_LIGHTEN; break;
		}
	}
	
	if(tool==VP_MIX || tool==VP_BLUR)
		dw->weight = paintval*alpha + dw->weight*(1.0-alpha);
	else if(tool==VP_ADD)
		dw->weight += paintval*alpha;
	else if(tool==VP_SUB) 
		dw->weight -= paintval*alpha;
	else if(tool==VP_MUL) 
		/* first mul, then blend the fac */
		dw->weight = ((1.0-alpha) + alpha*paintval)*dw->weight;
	else if(tool==VP_LIGHTEN) {
		if (dw->weight < paintval)
			dw->weight = paintval*alpha + dw->weight*(1.0-alpha);
	} else if(tool==VP_DARKEN) {
		if (dw->weight > paintval)
			dw->weight = paintval*alpha + dw->weight*(1.0-alpha);
	}
	CLAMP(dw->weight, 0.0f, 1.0f);
	
	/* if no spray, clip result with orig weight & orig alpha */
	if((wp->flag & VP_SPRAY)==0) {
		float testw=0.0f;
		
		alpha= brush->alpha;
		if(tool==VP_MIX || tool==VP_BLUR)
			testw = paintval*alpha + uw->weight*(1.0-alpha);
		else if(tool==VP_ADD)
			testw = uw->weight + paintval*alpha;
		else if(tool==VP_SUB) 
			testw = uw->weight - paintval*alpha;
		else if(tool==VP_MUL) 
			/* first mul, then blend the fac */
			testw = ((1.0-alpha) + alpha*paintval)*uw->weight;		
		else if(tool==VP_LIGHTEN) {
			if (uw->weight < paintval)
				testw = paintval*alpha + uw->weight*(1.0-alpha);
			else
				testw = uw->weight;
		} else if(tool==VP_DARKEN) {
			if (uw->weight > paintval)
				testw = paintval*alpha + uw->weight*(1.0-alpha);
			else
				testw = uw->weight;
		}
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

/* ----------------------------------------------------- */

/* used for 3d view, on active object, assumes me->dvert exists */
/* if mode==1: */
/*     samples cursor location, and gives menu with vertex groups to activate */
/* else */
/*     sets wp->weight to the closest weight value to vertex */
/*     note: we cant sample frontbuf, weight colors are interpolated too unpredictable */
void sample_wpaint(Scene *scene, ARegion *ar, View3D *v3d, int mode)
{
	ViewContext vc;
	ToolSettings *ts= scene->toolsettings;
	Object *ob= OBACT;
	Mesh *me= get_mesh(ob);
	int index;
	short mval[2] = {0, 0}, sco[2];

	if (!me) return;
	
//	getmouseco_areawin(mval);
	index= view3d_sample_backbuf(&vc, mval[0], mval[1]);
	
	if(index && index<=me->totface) {
		MFace *mface;
		
		mface= ((MFace *)me->mface) + index-1;
		
		if(mode==1) {	/* sampe which groups are in here */
			MDeformVert *dv;
			int a, totgroup;
			
			totgroup= BLI_countlist(&ob->defbase);
			if(totgroup) {
				int totmenu=0;
				int *groups=MEM_callocN(totgroup*sizeof(int), "groups");
				
				dv= me->dvert+mface->v1;
				for(a=0; a<dv->totweight; a++) {
					if (dv->dw[a].def_nr<totgroup)
						groups[dv->dw[a].def_nr]= 1;
				}
				dv= me->dvert+mface->v2;
				for(a=0; a<dv->totweight; a++) {
					if (dv->dw[a].def_nr<totgroup)
						groups[dv->dw[a].def_nr]= 1;
				}
				dv= me->dvert+mface->v3;
				for(a=0; a<dv->totweight; a++) {
					if (dv->dw[a].def_nr<totgroup)
						groups[dv->dw[a].def_nr]= 1;
				}
				if(mface->v4) {
					dv= me->dvert+mface->v4;
					for(a=0; a<dv->totweight; a++) {
						if (dv->dw[a].def_nr<totgroup)
							groups[dv->dw[a].def_nr]= 1;
					}
				}
				for(a=0; a<totgroup; a++)
					if(groups[a]) totmenu++;
				
				if(totmenu==0) {
					//notice("No Vertex Group Selected");
				}
				else {
					bDeformGroup *dg;
					short val;
					char item[40], *str= MEM_mallocN(40*totmenu+40, "menu");
					
					strcpy(str, "Vertex Groups %t");
					for(a=0, dg=ob->defbase.first; dg && a<totgroup; a++, dg= dg->next) {
						if(groups[a]) {
							sprintf(item, "|%s %%x%d", dg->name, a);
							strcat(str, item);
						}
					}
					
					val= 0; // XXX pupmenu(str);
					if(val>=0) {
						ob->actdef= val+1;
						DAG_id_flush_update(&me->id, OB_RECALC_DATA);
					}
					MEM_freeN(str);
				}
				MEM_freeN(groups);
			}
//			else notice("No Vertex Groups in Object");
		}
		else {
			DerivedMesh *dm;
			MDeformWeight *dw;
			float w1, w2, w3, w4, co[3], fac;
			
			dm = mesh_get_derived_final(scene, ob, CD_MASK_BAREMESH);
			if(dm->getVertCo==NULL) {
				//notice("Not supported yet");
			}
			else {
				/* calc 3 or 4 corner weights */
				dm->getVertCo(dm, mface->v1, co);
				project_short_noclip(ar, co, sco);
				w1= ((mval[0]-sco[0])*(mval[0]-sco[0]) + (mval[1]-sco[1])*(mval[1]-sco[1]));
				
				dm->getVertCo(dm, mface->v2, co);
				project_short_noclip(ar, co, sco);
				w2= ((mval[0]-sco[0])*(mval[0]-sco[0]) + (mval[1]-sco[1])*(mval[1]-sco[1]));
				
				dm->getVertCo(dm, mface->v3, co);
				project_short_noclip(ar, co, sco);
				w3= ((mval[0]-sco[0])*(mval[0]-sco[0]) + (mval[1]-sco[1])*(mval[1]-sco[1]));
				
				if(mface->v4) {
					dm->getVertCo(dm, mface->v4, co);
					project_short_noclip(ar, co, sco);
					w4= ((mval[0]-sco[0])*(mval[0]-sco[0]) + (mval[1]-sco[1])*(mval[1]-sco[1]));
				}
				else w4= 1.0e10;
				
				fac= MIN4(w1, w2, w3, w4);
				if(w1==fac) {
					dw= ED_vgroup_weight_get(me->dvert+mface->v1, ob->actdef-1);
					if(dw) ts->vgroup_weight= dw->weight; else ts->vgroup_weight= 0.0f;
				}
				else if(w2==fac) {
					dw= ED_vgroup_weight_get(me->dvert+mface->v2, ob->actdef-1);
					if(dw) ts->vgroup_weight= dw->weight; else ts->vgroup_weight= 0.0f;
				}
				else if(w3==fac) {
					dw= ED_vgroup_weight_get(me->dvert+mface->v3, ob->actdef-1);
					if(dw) ts->vgroup_weight= dw->weight; else ts->vgroup_weight= 0.0f;
				}
				else if(w4==fac) {
					if(mface->v4) {
						dw= ED_vgroup_weight_get(me->dvert+mface->v4, ob->actdef-1);
						if(dw) ts->vgroup_weight= dw->weight; else ts->vgroup_weight= 0.0f;
					}
				}
			}
			dm->release(dm);
		}		
		
	}
	
}

static void do_weight_paint_auto_normalize(MDeformVert *dvert, 
					   int paint_nr, char *map)
{
//	MDeformWeight *dw = dvert->dw;
	float sum=0.0f, fac=0.0f, paintw=0.0f;
	int i, tot=0;

	if (!map)
		return;

	for (i=0; i<dvert->totweight; i++) {
		if (dvert->dw[i].def_nr == paint_nr)
			paintw = dvert->dw[i].weight;

		if (map[dvert->dw[i].def_nr]) {
			tot += 1;
			if (dvert->dw[i].def_nr != paint_nr)
				sum += dvert->dw[i].weight;
		}
	}
	
	if (!tot || sum <= (1.0f - paintw))
		return;

	fac = sum / (1.0f - paintw);
	fac = fac==0.0f ? 1.0f : 1.0f / fac;

	for (i=0; i<dvert->totweight; i++) {
		if (map[dvert->dw[i].def_nr]) {
			if (dvert->dw[i].def_nr != paint_nr)
				dvert->dw[i].weight *= fac;
		}
	}
}

static void do_weight_paint_vertex(VPaint *wp, Object *ob, int index, 
				   float alpha, float paintweight, int flip, 
				   int vgroup_mirror, char *validmap)
{
	Mesh *me= ob->data;
	MDeformWeight *dw, *uw;
	int vgroup= ob->actdef-1;
	
	if(wp->flag & VP_ONLYVGROUP) {
		dw= ED_vgroup_weight_get(me->dvert+index, vgroup);
		uw= ED_vgroup_weight_get(wp->wpaint_prev+index, vgroup);
	}
	else {
		dw= ED_vgroup_weight_verify(me->dvert+index, vgroup);
		uw= ED_vgroup_weight_verify(wp->wpaint_prev+index, vgroup);
	}
	if(dw==NULL || uw==NULL)
		return;
	
	wpaint_blend(wp, dw, uw, alpha, paintweight, flip);
	do_weight_paint_auto_normalize(me->dvert+index, vgroup, validmap);

	if(me->editflag & ME_EDIT_MIRROR_X) {	/* x mirror painting */
		int j= mesh_get_x_mirror_vert(ob, index);
		if(j>=0) {
			/* copy, not paint again */
			if(vgroup_mirror != -1)
				uw= ED_vgroup_weight_verify(me->dvert+j, vgroup_mirror);
			else
				uw= ED_vgroup_weight_verify(me->dvert+j, vgroup);
				
			uw->weight= dw->weight;

			do_weight_paint_auto_normalize(me->dvert+j, vgroup, validmap);
		}
	}
}


/* *************** set wpaint operator ****************** */

static int set_wpaint(bContext *C, wmOperator *op)		/* toggle */
{		
	Object *ob= CTX_data_active_object(C);
	Scene *scene= CTX_data_scene(C);
	VPaint *wp= scene->toolsettings->wpaint;
	Mesh *me;
	
	me= get_mesh(ob);
	if(ob->id.lib || me==NULL) return OPERATOR_PASS_THROUGH;
	
	if(me && me->totface>=MAXINDEX) {
		error("Maximum number of faces: %d", MAXINDEX-1);
		ob->mode &= ~OB_MODE_WEIGHT_PAINT;
		return OPERATOR_CANCELLED;
	}
	
	if(ob->mode & OB_MODE_WEIGHT_PAINT) ob->mode &= ~OB_MODE_WEIGHT_PAINT;
	else ob->mode |= OB_MODE_WEIGHT_PAINT;
	
	
	/* Weightpaint works by overriding colors in mesh,
		* so need to make sure we recalc on enter and
		* exit (exit needs doing regardless because we
				* should redeform).
		*/
	DAG_id_flush_update(&me->id, OB_RECALC_DATA);
	
	if(ob->mode & OB_MODE_WEIGHT_PAINT) {
		Object *par;
		
		if(wp==NULL)
			wp= scene->toolsettings->wpaint= new_vpaint(1);

		paint_init(&wp->paint, PAINT_CURSOR_WEIGHT_PAINT);
		paint_cursor_start(C, weight_paint_poll);
		
		mesh_octree_table(ob, NULL, NULL, 's');
		
		/* verify if active weight group is also active bone */
		par= modifiers_isDeformedByArmature(ob);
		if(par && (par->mode & OB_MODE_POSE)) {
			bArmature *arm= par->data;

			if(arm->act_bone)
				ED_vgroup_select_by_name(ob, arm->act_bone->name);
		}
	}
	else {
		mesh_octree_table(ob, NULL, NULL, 'e');
	}
	
	WM_event_add_notifier(C, NC_SCENE|ND_MODE, scene);
	
	return OPERATOR_FINISHED;
}

/* for switching to/from mode */
static int paint_poll_test(bContext *C)
{
	if(ED_operator_view3d_active(C)==0)
		return 0;
	if(CTX_data_edit_object(C))
		return 0;
	if(CTX_data_active_object(C)==NULL)
		return 0;
	return 1;
}

void PAINT_OT_weight_paint_toggle(wmOperatorType *ot)
{
	
	/* identifiers */
	ot->name= "Weight Paint Mode";
	ot->idname= "PAINT_OT_weight_paint_toggle";
	
	/* api callbacks */
	ot->exec= set_wpaint;
	ot->poll= paint_poll_test;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
}

/* ************ paint radial controls *************/

static int vpaint_radial_control_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	Paint *p = paint_get_active(CTX_data_scene(C));
	Brush *brush = paint_brush(p);
	
	WM_paint_cursor_end(CTX_wm_manager(C), p->paint_cursor);
	p->paint_cursor = NULL;
	brush_radial_control_invoke(op, brush, 1);
	return WM_radial_control_invoke(C, op, event);
}

static int vpaint_radial_control_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	int ret = WM_radial_control_modal(C, op, event);
	if(ret != OPERATOR_RUNNING_MODAL)
		paint_cursor_start(C, vertex_paint_poll);
	return ret;
}

static int vpaint_radial_control_exec(bContext *C, wmOperator *op)
{
	Brush *brush = paint_brush(&CTX_data_scene(C)->toolsettings->vpaint->paint);
	int ret = brush_radial_control_exec(op, brush, 1);
	
	WM_event_add_notifier(C, NC_BRUSH|NA_EDITED, brush);
	
	return ret;
}

static int wpaint_radial_control_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	Paint *p = paint_get_active(CTX_data_scene(C));
	Brush *brush = paint_brush(p);
	
	WM_paint_cursor_end(CTX_wm_manager(C), p->paint_cursor);
	p->paint_cursor = NULL;
	brush_radial_control_invoke(op, brush, 1);
	return WM_radial_control_invoke(C, op, event);
}

static int wpaint_radial_control_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	int ret = WM_radial_control_modal(C, op, event);
	if(ret != OPERATOR_RUNNING_MODAL)
		paint_cursor_start(C, weight_paint_poll);
	return ret;
}

static int wpaint_radial_control_exec(bContext *C, wmOperator *op)
{
	Brush *brush = paint_brush(&CTX_data_scene(C)->toolsettings->wpaint->paint);
	int ret = brush_radial_control_exec(op, brush, 1);
	
	WM_event_add_notifier(C, NC_BRUSH|NA_EDITED, brush);
	
	return ret;
}

void PAINT_OT_weight_paint_radial_control(wmOperatorType *ot)
{
	WM_OT_radial_control_partial(ot);

	ot->name= "Weight Paint Radial Control";
	ot->idname= "PAINT_OT_weight_paint_radial_control";

	ot->invoke= wpaint_radial_control_invoke;
	ot->modal= wpaint_radial_control_modal;
	ot->exec= wpaint_radial_control_exec;
	ot->poll= weight_paint_poll;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO|OPTYPE_BLOCKING;
}

void PAINT_OT_vertex_paint_radial_control(wmOperatorType *ot)
{
	WM_OT_radial_control_partial(ot);

	ot->name= "Vertex Paint Radial Control";
	ot->idname= "PAINT_OT_vertex_paint_radial_control";

	ot->invoke= vpaint_radial_control_invoke;
	ot->modal= vpaint_radial_control_modal;
	ot->exec= vpaint_radial_control_exec;
	ot->poll= vertex_paint_poll;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO|OPTYPE_BLOCKING;
}

/* ************ weight paint operator ********** */

struct WPaintData {
	ViewContext vc;
	int *indexar;
	int vgroup_mirror;
	float *vertexcosnos;
	float wpimat[3][3];
	
	/*variables for auto normalize*/
	int auto_normalize;
	char *vgroup_validmap; /*stores if vgroups tie to deforming bones or not*/
};

static char *wpaint_make_validmap(Mesh *me, Object *ob)
{
	bDeformGroup *dg;
	ModifierData *md;
	char *validmap;
	bPose *pose;
	bPoseChannel *chan;
	ArmatureModifierData *amd;
	GHash *gh = BLI_ghash_new(BLI_ghashutil_strhash, BLI_ghashutil_strcmp);
	int i = 0, step1=1;

	/*add all names to a hash table*/
	for (dg=ob->defbase.first, i=0; dg; dg=dg->next, i++) {
		BLI_ghash_insert(gh, dg->name, NULL);
	}

	if (!i)
		return NULL;

	validmap = MEM_callocN(i, "wpaint valid map");

	/*now loop through the armature modifiers and identify deform bones*/
	for (md = ob->modifiers.first; md; md= !md->next && step1 ? (step1=0), modifiers_getVirtualModifierList(ob) : md->next) {
		if (!(md->mode & (eModifierMode_Realtime|eModifierMode_Virtual)))
			continue;

		if (md->type == eModifierType_Armature) 
		{
			amd = (ArmatureModifierData*) md;

			if(amd->object && amd->object->pose) {
				pose = amd->object->pose;
				
				for (chan=pose->chanbase.first; chan; chan=chan->next) {
					if (chan->bone->flag & BONE_NO_DEFORM)
						continue;

					if (BLI_ghash_haskey(gh, chan->name)) {
						BLI_ghash_remove(gh, chan->name, NULL, NULL);
						BLI_ghash_insert(gh, chan->name, SET_INT_IN_POINTER(1));
					}
				}
			}
		}
	}
	
	/*add all names to a hash table*/
	for (dg=ob->defbase.first, i=0; dg; dg=dg->next, i++) {
		if (BLI_ghash_lookup(gh, dg->name) != NULL) {
			validmap[i] = 1;
		}
	}

	BLI_ghash_free(gh, NULL, NULL);

	return validmap;
}

static int wpaint_stroke_test_start(bContext *C, wmOperator *op, wmEvent *event)
{
	Scene *scene= CTX_data_scene(C);
	struct PaintStroke *stroke = op->customdata;
	ToolSettings *ts= CTX_data_tool_settings(C);
	VPaint *wp= ts->wpaint;
	Object *ob= CTX_data_active_object(C);
	struct WPaintData *wpd;
	Mesh *me;
	float mat[4][4], imat[4][4];
	
	if(scene->obedit) return OPERATOR_CANCELLED;
	
	me= get_mesh(ob);
	if(me==NULL || me->totface==0) return OPERATOR_PASS_THROUGH;
	
	/* if nothing was added yet, we make dverts and a vertex deform group */
	if (!me->dvert)
		ED_vgroup_data_create(&me->id);
	
	/* make mode data storage */
	wpd= MEM_callocN(sizeof(struct WPaintData), "WPaintData");
	paint_stroke_set_mode_data(stroke, wpd);
	view3d_set_viewcontext(C, &wpd->vc);
	wpd->vgroup_mirror= -1;
	
	/*set up auto-normalize, and generate map for detecting which
	  vgroups affect deform bones*/
	wpd->auto_normalize = ts->auto_normalize;
	if (wpd->auto_normalize)
		wpd->vgroup_validmap = wpaint_make_validmap(me, ob);
	
	//	if(qual & LR_CTRLKEY) {
	//		sample_wpaint(scene, ar, v3d, 0);
	//		return;
	//	}
	//	if(qual & LR_SHIFTKEY) {
	//		sample_wpaint(scene, ar, v3d, 1);
	//		return;
	//	}
	
	/* ALLOCATIONS! no return after this line */
	/* painting on subsurfs should give correct points too, this returns me->totvert amount */
	wpd->vertexcosnos= mesh_get_mapped_verts_nors(scene, ob);
	wpd->indexar= get_indexarray();
	copy_wpaint_prev(wp, me->dvert, me->totvert);
	
	/* this happens on a Bone select, when no vgroup existed yet */
	if(ob->actdef<=0) {
		Object *modob;
		if((modob = modifiers_isDeformedByArmature(ob))) {
			Bone *actbone= ((bArmature *)modob->data)->act_bone;
			if(actbone) {
				bPoseChannel *pchan= get_pose_channel(modob->pose, actbone->name);

				if(pchan) {
					bDeformGroup *dg= get_named_vertexgroup(ob, pchan->name);
					if(dg==NULL)
						dg= ED_vgroup_add_name(ob, pchan->name);	/* sets actdef */
					else
						ob->actdef= 1 + get_defgroup_num(ob, dg);
				}
			}
		}
	}
	if(ob->defbase.first==NULL) {
		ED_vgroup_add(ob);
	}
	
	//	if(ob->lay & v3d->lay); else error("Active object is not in this layer");
	
	/* imat for normals */
	mul_m4_m4m4(mat, ob->obmat, wpd->vc.rv3d->viewmat);
	invert_m4_m4(imat, mat);
	copy_m3_m4(wpd->wpimat, imat);
	
	/* if mirror painting, find the other group */
	if(me->editflag & ME_EDIT_MIRROR_X) {
		bDeformGroup *defgroup= BLI_findlink(&ob->defbase, ob->actdef-1);
		if(defgroup) {
			bDeformGroup *curdef;
			int actdef= 0;
			char name[32];
			
			BLI_strncpy(name, defgroup->name, 32);
			bone_flip_name(name, 0);		/* 0 = don't strip off number extensions */
			
			for (curdef = ob->defbase.first; curdef; curdef=curdef->next, actdef++)
				if (!strcmp(curdef->name, name))
					break;
			if(curdef==NULL) {
				int olddef= ob->actdef;	/* tsk, ED_vgroup_add sets the active defgroup */
				curdef= ED_vgroup_add_name (ob, name);
				ob->actdef= olddef;
			}
			
			if(curdef && curdef!=defgroup)
				wpd->vgroup_mirror= actdef;
		}
	}
	
	return 1;
}

static void wpaint_stroke_update_step(bContext *C, struct PaintStroke *stroke, PointerRNA *itemptr)
{
	ToolSettings *ts= CTX_data_tool_settings(C);
	VPaint *wp= ts->wpaint;
	Brush *brush = paint_brush(&wp->paint);
	struct WPaintData *wpd= paint_stroke_mode_data(stroke);
	ViewContext *vc;
	Object *ob;
	Mesh *me;
	float mat[4][4];
	float paintweight= ts->vgroup_weight;
	int *indexar;
	int totindex, index, totw, flip;
	float alpha;
	float mval[2], pressure;
	
	/* cannot paint if there is no stroke data */
	if (wpd == NULL) {
		// XXX: force a redraw here, since even though we can't paint, 
		// at least view won't freeze until stroke ends
		ED_region_tag_redraw(CTX_wm_region(C));
		return;
	}
		
	vc= &wpd->vc;
	ob= vc->obact;
	me= ob->data;
	indexar= wpd->indexar;
	
	view3d_operator_needs_opengl(C);
		
	/* load projection matrix */
	wmMultMatrix(ob->obmat);
	wmGetSingleMatrix(mat);
	wmLoadMatrix(vc->rv3d->viewmat);

	flip = RNA_boolean_get(itemptr, "flip");
	pressure = RNA_float_get(itemptr, "pressure");
	RNA_float_get_array(itemptr, "mouse", mval);
	mval[0]-= vc->ar->winrct.xmin;
	mval[1]-= vc->ar->winrct.ymin;
			
	swap_m4m4(wpd->vc.rv3d->persmat, mat);
			
	/* which faces are involved */
	if(wp->flag & VP_AREA) {
		totindex= sample_backbuf_area(vc, indexar, me->totface, mval[0], mval[1], brush->size);
	}
	else {
		indexar[0]= view3d_sample_backbuf(vc, mval[0], mval[1]);
		if(indexar[0]) totindex= 1;
		else totindex= 0;
	}
			
	if(wp->flag & VP_COLINDEX) {
		for(index=0; index<totindex; index++) {
			if(indexar[index] && indexar[index]<=me->totface) {
				MFace *mface= ((MFace *)me->mface) + (indexar[index]-1);
						
				if(mface->mat_nr!=ob->actcol-1) {
					indexar[index]= 0;
				}
			}
		}
	}
			
	if((me->editflag & ME_EDIT_PAINT_MASK) && me->mface) {
		for(index=0; index<totindex; index++) {
			if(indexar[index] && indexar[index]<=me->totface) {
				MFace *mface= ((MFace *)me->mface) + (indexar[index]-1);
						
				if((mface->flag & ME_FACE_SEL)==0) {
					indexar[index]= 0;
				}
			}					
		}
	}
			
	/* make sure each vertex gets treated only once */
	/* and calculate filter weight */
	totw= 0;
	if(brush->vertexpaint_tool==VP_BLUR) 
		paintweight= 0.0f;
	else
		paintweight= ts->vgroup_weight;
			
	for(index=0; index<totindex; index++) {
		if(indexar[index] && indexar[index]<=me->totface) {
			MFace *mface= me->mface + (indexar[index]-1);
					
			(me->dvert+mface->v1)->flag= 1;
			(me->dvert+mface->v2)->flag= 1;
			(me->dvert+mface->v3)->flag= 1;
			if(mface->v4) (me->dvert+mface->v4)->flag= 1;
					
			if(brush->vertexpaint_tool==VP_BLUR) {
				MDeformWeight *dw, *(*dw_func)(MDeformVert *, int) = ED_vgroup_weight_verify;
						
				if(wp->flag & VP_ONLYVGROUP)
					dw_func= ED_vgroup_weight_get;
						
				dw= dw_func(me->dvert+mface->v1, ob->actdef-1);
				if(dw) {paintweight+= dw->weight; totw++;}
				dw= dw_func(me->dvert+mface->v2, ob->actdef-1);
				if(dw) {paintweight+= dw->weight; totw++;}
				dw= dw_func(me->dvert+mface->v3, ob->actdef-1);
				if(dw) {paintweight+= dw->weight; totw++;}
				if(mface->v4) {
					dw= dw_func(me->dvert+mface->v4, ob->actdef-1);
					if(dw) {paintweight+= dw->weight; totw++;}
				}
			}
		}
	}
			
	if(brush->vertexpaint_tool==VP_BLUR) 
		paintweight/= (float)totw;
			
	for(index=0; index<totindex; index++) {
				
		if(indexar[index] && indexar[index]<=me->totface) {
			MFace *mface= me->mface + (indexar[index]-1);
					
			if((me->dvert+mface->v1)->flag) {
				alpha= calc_vp_alpha_dl(wp, vc, wpd->wpimat, wpd->vertexcosnos+6*mface->v1, mval, pressure);
				if(alpha) {
					do_weight_paint_vertex(wp, ob, mface->v1, 
						alpha, paintweight, flip, wpd->vgroup_mirror, 
						wpd->vgroup_validmap);
				}
				(me->dvert+mface->v1)->flag= 0;
			}
					
			if((me->dvert+mface->v2)->flag) {
				alpha= calc_vp_alpha_dl(wp, vc, wpd->wpimat, wpd->vertexcosnos+6*mface->v2, mval, pressure);
				if(alpha) {
					do_weight_paint_vertex(wp, ob, mface->v2, 
						alpha, paintweight, flip, wpd->vgroup_mirror, 
						wpd->vgroup_validmap);
				}
				(me->dvert+mface->v2)->flag= 0;
			}
					
			if((me->dvert+mface->v3)->flag) {
				alpha= calc_vp_alpha_dl(wp, vc, wpd->wpimat, wpd->vertexcosnos+6*mface->v3, mval, pressure);
				if(alpha) {
					do_weight_paint_vertex(wp, ob, mface->v3, 
						alpha, paintweight, flip, wpd->vgroup_mirror, 
						wpd->vgroup_validmap);
				}
				(me->dvert+mface->v3)->flag= 0;
			}
					
			if((me->dvert+mface->v4)->flag) {
				if(mface->v4) {
					alpha= calc_vp_alpha_dl(wp, vc, wpd->wpimat, wpd->vertexcosnos+6*mface->v4, mval, pressure);
					if(alpha) {
						do_weight_paint_vertex(wp, ob, mface->v4, 
							alpha, paintweight, flip, wpd->vgroup_mirror,
							wpd->vgroup_validmap);
					}
					(me->dvert+mface->v4)->flag= 0;
				}
			}
		}
	}
			
	swap_m4m4(vc->rv3d->persmat, mat);
			
	DAG_id_flush_update(ob->data, OB_RECALC_DATA);
	ED_region_tag_redraw(vc->ar);
}

static void wpaint_stroke_done(bContext *C, struct PaintStroke *stroke)
{
	ToolSettings *ts= CTX_data_tool_settings(C);
	Object *ob= CTX_data_active_object(C);
	struct WPaintData *wpd= paint_stroke_mode_data(stroke);
	
	if(wpd) {
		if(wpd->vertexcosnos)
			MEM_freeN(wpd->vertexcosnos);
		MEM_freeN(wpd->indexar);
		
		if (wpd->vgroup_validmap)
			MEM_freeN(wpd->vgroup_validmap);
		
		MEM_freeN(wpd);
	}
	
	/* frees prev buffer */
	copy_wpaint_prev(ts->wpaint, NULL, 0);
	
	/* and particles too */
	if(ob->particlesystem.first) {
		ParticleSystem *psys;
		int i;
		
		for(psys= ob->particlesystem.first; psys; psys= psys->next) {
			for(i=0; i<PSYS_TOT_VG; i++) {
				if(psys->vgroup[i]==ob->actdef) {
					psys->recalc |= PSYS_RECALC_RESET;
					break;
				}
			}
		}
	}
	
	DAG_id_flush_update(ob->data, OB_RECALC_DATA);	
}


static int wpaint_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	
	op->customdata = paint_stroke_new(C, NULL, wpaint_stroke_test_start,
					  wpaint_stroke_update_step,
					  wpaint_stroke_done);
	
	/* add modal handler */
	WM_event_add_modal_handler(C, op);

	op->type->modal(C, op, event);
	
	return OPERATOR_RUNNING_MODAL;
}

void PAINT_OT_weight_paint(wmOperatorType *ot)
{
	
	/* identifiers */
	ot->name= "Weight Paint";
	ot->idname= "PAINT_OT_weight_paint";
	
	/* api callbacks */
	ot->invoke= wpaint_invoke;
	ot->modal= paint_stroke_modal;
	/* ot->exec= vpaint_exec; <-- needs stroke property */
	ot->poll= weight_paint_poll;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO|OPTYPE_BLOCKING;

	RNA_def_collection_runtime(ot->srna, "stroke", &RNA_OperatorStrokeElement, "Stroke", "");
}

static int weight_paint_set_exec(bContext *C, wmOperator *op)
{
	struct Scene *scene= CTX_data_scene(C);
	Object *obact = CTX_data_active_object(C);

	wpaint_fill(scene->toolsettings->wpaint, obact, scene->toolsettings->vgroup_weight);
	ED_region_tag_redraw(CTX_wm_region(C)); // XXX - should redraw all 3D views
	return OPERATOR_FINISHED;
}

void PAINT_OT_weight_set(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Weight Set";
	ot->idname= "PAINT_OT_weight_set";

	/* api callbacks */
	ot->exec= weight_paint_set_exec;
	ot->poll= facemask_paint_poll;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* ************ set / clear vertex paint mode ********** */


static int set_vpaint(bContext *C, wmOperator *op)		/* toggle */
{	
	Object *ob= CTX_data_active_object(C);
	Scene *scene= CTX_data_scene(C);
	VPaint *vp= scene->toolsettings->vpaint;
	Mesh *me;
	
	me= get_mesh(ob);
	
	if(me==NULL || object_data_is_libdata(ob)) {
		ob->mode &= ~OB_MODE_VERTEX_PAINT;
		return OPERATOR_PASS_THROUGH;
	}
	
	if(me && me->totface>=MAXINDEX) {
		error("Maximum number of faces: %d", MAXINDEX-1);
		ob->mode &= ~OB_MODE_VERTEX_PAINT;
		return OPERATOR_FINISHED;
	}
	
	if(me && me->mcol==NULL) make_vertexcol(ob);
	
	/* toggle: end vpaint */
	if(ob->mode & OB_MODE_VERTEX_PAINT) {
		
		ob->mode &= ~OB_MODE_VERTEX_PAINT;
	}
	else {
		ob->mode |= OB_MODE_VERTEX_PAINT;
		/* Turn off weight painting */
		if (ob->mode & OB_MODE_WEIGHT_PAINT)
			set_wpaint(C, op);
		
		if(vp==NULL)
			vp= scene->toolsettings->vpaint= new_vpaint(0);
		
		paint_cursor_start(C, vertex_paint_poll);

		paint_init(&vp->paint, PAINT_CURSOR_VERTEX_PAINT);
	}
	
	if (me)
		/* update modifier stack for mapping requirements */
		DAG_id_flush_update(&me->id, OB_RECALC_DATA);
	
	WM_event_add_notifier(C, NC_SCENE|ND_MODE, scene);
	
	return OPERATOR_FINISHED;
}

void PAINT_OT_vertex_paint_toggle(wmOperatorType *ot)
{
	
	/* identifiers */
	ot->name= "Vertex Paint Mode";
	ot->idname= "PAINT_OT_vertex_paint_toggle";
	
	/* api callbacks */
	ot->exec= set_vpaint;
	ot->poll= paint_poll_test;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
}



/* ********************** vertex paint operator ******************* */

/* Implementation notes:

Operator->invoke()
  - validate context (add mcol)
  - create customdata storage
  - call paint once (mouse click)
  - add modal handler 

Operator->modal()
  - for every mousemove, apply vertex paint
  - exit on mouse release, free customdata
    (return OPERATOR_FINISHED also removes handler and operator)

For future:
  - implement a stroke event (or mousemove with past positons)
  - revise whether op->customdata should be added in object, in set_vpaint

*/

typedef struct VPaintData {
	ViewContext vc;
	unsigned int paintcol;
	int *indexar;
	float *vertexcosnos;
	float vpimat[3][3];
} VPaintData;

static int vpaint_stroke_test_start(bContext *C, struct wmOperator *op, wmEvent *event)
{
	ToolSettings *ts= CTX_data_tool_settings(C);
	struct PaintStroke *stroke = op->customdata;
	VPaint *vp= ts->vpaint;
	struct VPaintData *vpd;
	Object *ob= CTX_data_active_object(C);
	Mesh *me;
	float mat[4][4], imat[4][4];

	/* context checks could be a poll() */
	me= get_mesh(ob);
	if(me==NULL || me->totface==0) return OPERATOR_PASS_THROUGH;
	
	if(me->mcol==NULL) make_vertexcol(ob);
	if(me->mcol==NULL) return OPERATOR_CANCELLED;
	
	/* make mode data storage */
	vpd= MEM_callocN(sizeof(struct VPaintData), "VPaintData");
	paint_stroke_set_mode_data(stroke, vpd);
	view3d_set_viewcontext(C, &vpd->vc);
	
	vpd->vertexcosnos= mesh_get_mapped_verts_nors(vpd->vc.scene, ob);
	vpd->indexar= get_indexarray();
	vpd->paintcol= vpaint_get_current_col(vp);
	
	/* for filtering */
	copy_vpaint_prev(vp, (unsigned int *)me->mcol, me->totface);
	
	/* some old cruft to sort out later */
	mul_m4_m4m4(mat, ob->obmat, vpd->vc.rv3d->viewmat);
	invert_m4_m4(imat, mat);
	copy_m3_m4(vpd->vpimat, imat);

	return 1;
}

static void vpaint_paint_face(VPaint *vp, VPaintData *vpd, Object *ob, int index, float mval[2], float pressure, int flip)
{
	ViewContext *vc = &vpd->vc;
	Brush *brush = paint_brush(&vp->paint);
	Mesh *me = get_mesh(ob);
	MFace *mface= ((MFace*)me->mface) + index;
	unsigned int *mcol= ((unsigned int*)me->mcol) + 4*index;
	unsigned int *mcolorig= ((unsigned int*)vp->vpaint_prev) + 4*index;
	float alpha;
	int i;
	
	if((vp->flag & VP_COLINDEX && mface->mat_nr!=ob->actcol-1) ||
	   ((me->editflag & ME_EDIT_PAINT_MASK) && !(mface->flag & ME_FACE_SEL)))
		return;

	if(brush->vertexpaint_tool==VP_BLUR) {
		unsigned int fcol1= mcol_blend( mcol[0], mcol[1], 128);
		if(mface->v4) {
			unsigned int fcol2= mcol_blend( mcol[2], mcol[3], 128);
			vpd->paintcol= mcol_blend( fcol1, fcol2, 128);
		}
		else {
			vpd->paintcol= mcol_blend( mcol[2], fcol1, 170);
		}
		
	}

	for(i = 0; i < (mface->v4 ? 4 : 3); ++i) {
		alpha= calc_vp_alpha_dl(vp, vc, vpd->vpimat, vpd->vertexcosnos+6*(&mface->v1)[i], mval, pressure);
		if(alpha)
			vpaint_blend(vp, mcol+i, mcolorig+i, vpd->paintcol, (int)(alpha*255.0));
	}
}

static void vpaint_stroke_update_step(bContext *C, struct PaintStroke *stroke, PointerRNA *itemptr)
{
	ToolSettings *ts= CTX_data_tool_settings(C);
	struct VPaintData *vpd = paint_stroke_mode_data(stroke);
	VPaint *vp= ts->vpaint;
	Brush *brush = paint_brush(&vp->paint);
	ViewContext *vc= &vpd->vc;
	Object *ob= vc->obact;
	Mesh *me= ob->data;
	float mat[4][4];
	int *indexar= vpd->indexar;
	int totindex, index, flip;
	float pressure, mval[2];

	RNA_float_get_array(itemptr, "mouse", mval);
	flip = RNA_boolean_get(itemptr, "flip");
	pressure = RNA_float_get(itemptr, "pressure");
			
	view3d_operator_needs_opengl(C);
			
	/* load projection matrix */
	wmMultMatrix(ob->obmat);
	wmGetSingleMatrix(mat);
	wmLoadMatrix(vc->rv3d->viewmat);

	mval[0]-= vc->ar->winrct.xmin;
	mval[1]-= vc->ar->winrct.ymin;

			
	/* which faces are involved */
	if(vp->flag & VP_AREA) {
		totindex= sample_backbuf_area(vc, indexar, me->totface, mval[0], mval[1], brush->size);
	}
	else {
		indexar[0]= view3d_sample_backbuf(vc, mval[0], mval[1]);
		if(indexar[0]) totindex= 1;
		else totindex= 0;
	}
			
	swap_m4m4(vc->rv3d->persmat, mat);
			
	for(index=0; index<totindex; index++) {				
		if(indexar[index] && indexar[index]<=me->totface)
			vpaint_paint_face(vp, vpd, ob, indexar[index]-1, mval, pressure, flip);
	}
						
	swap_m4m4(vc->rv3d->persmat, mat);

	/* was disabled because it is slow, but necessary for blur */
	if(brush->vertexpaint_tool == VP_BLUR)
		do_shared_vertexcol(me);
			
	ED_region_tag_redraw(vc->ar);
			
	DAG_id_flush_update(ob->data, OB_RECALC_DATA);
}

static void vpaint_stroke_done(bContext *C, struct PaintStroke *stroke)
{
	ToolSettings *ts= CTX_data_tool_settings(C);
	struct VPaintData *vpd= paint_stroke_mode_data(stroke);
	
	if(vpd->vertexcosnos)
		MEM_freeN(vpd->vertexcosnos);
	MEM_freeN(vpd->indexar);
	
	/* frees prev buffer */
	copy_vpaint_prev(ts->vpaint, NULL, 0);
	
	MEM_freeN(vpd);
}

static int vpaint_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	
	op->customdata = paint_stroke_new(C, NULL, vpaint_stroke_test_start,
					  vpaint_stroke_update_step,
					  vpaint_stroke_done);
	
	/* add modal handler */
	WM_event_add_modal_handler(C, op);

	op->type->modal(C, op, event);
	
	return OPERATOR_RUNNING_MODAL;
}

void PAINT_OT_vertex_paint(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Vertex Paint";
	ot->idname= "PAINT_OT_vertex_paint";
	
	/* api callbacks */
	ot->invoke= vpaint_invoke;
	ot->modal= paint_stroke_modal;
	/* ot->exec= vpaint_exec; <-- needs stroke property */
	ot->poll= vertex_paint_poll;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO|OPTYPE_BLOCKING;

	RNA_def_collection_runtime(ot->srna, "stroke", &RNA_OperatorStrokeElement, "Stroke", "");
}

/* ********************** weight from bones operator ******************* */

static int weight_from_bones_poll(bContext *C)
{
	Object *ob= CTX_data_active_object(C);

	return (ob && (ob->mode & OB_MODE_WEIGHT_PAINT) && modifiers_isDeformedByArmature(ob));
}

static int weight_from_bones_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Object *ob= CTX_data_active_object(C);
	Object *armob= modifiers_isDeformedByArmature(ob);
	Mesh *me= ob->data;
	int type= RNA_enum_get(op->ptr, "type");

	create_vgroups_from_armature(scene, ob, armob, type, (me->editflag & ME_EDIT_MIRROR_X));

	DAG_id_flush_update(&me->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, me);

	return OPERATOR_FINISHED;
}

void PAINT_OT_weight_from_bones(wmOperatorType *ot)
{
	static EnumPropertyItem type_items[]= {
		{ARM_GROUPS_AUTO, "AUTOMATIC", 0, "Automatic", "Automatic weights froms bones."},
		{ARM_GROUPS_ENVELOPE, "ENVELOPES", 0, "From Envelopes", "Weights from envelopes with user defined radius."},
		{0, NULL, 0, NULL, NULL}};

	/* identifiers */
	ot->name= "Weight from Bones";
	ot->idname= "PAINT_OT_weight_from_bones";
	
	/* api callbacks */
	ot->exec= weight_from_bones_exec;
	ot->invoke= WM_menu_invoke;
	ot->poll= weight_from_bones_poll;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	/* properties */
	ot->prop= RNA_def_enum(ot->srna, "type", type_items, 0, "Type", "Method to use for assigning weights.");
}

