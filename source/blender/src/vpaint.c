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
#include "BLI_arithb.h"
#include "MTC_matrixops.h"

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

#include "BKE_armature.h"
#include "BKE_DerivedMesh.h"
#include "BKE_cloth.h"
#include "BKE_customdata.h"
#include "BKE_depsgraph.h"
#include "BKE_deform.h"
#include "BKE_displist.h"
#include "BKE_global.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_multires.h"
#include "BKE_object.h"
#include "BKE_utildefines.h"

#include "BIF_editview.h"
#include "BIF_graphics.h"
#include "BIF_glutil.h"
#include "BIF_gl.h"
#include "BIF_interface.h"
#include "BIF_meshtools.h"
#include "BIF_mywindow.h"
#include "BIF_space.h"
#include "BIF_screen.h"
#include "BIF_toolbox.h"

#include "BDR_vpaint.h"
#include "BDR_editobject.h"

#include "BSE_drawview.h"
#include "BSE_trans_types.h"
#include "BSE_view.h"

#include "multires.h"
#include "mydevice.h"
#include "blendef.h"

#include "BIF_editdeform.h"

	/* Gvp.mode */
#define VP_MIX	0
#define VP_ADD	1
#define VP_SUB	2
#define VP_MUL	3
#define VP_BLUR	4
#define VP_LIGHTEN	5
#define VP_DARKEN	6

#define MAXINDEX	512000

VPaint Gvp= {1.0, 1.0, 1.0, 0.2, 25.0, 1.0, 1.0, 0, VP_AREA+VP_SOFT+VP_SPRAY, 0};
VPaint Gwp= {1.0, 1.0, 1.0, 1.0, 25.0, 1.0, 1.0, 0, VP_AREA+VP_SOFT, 0};

static int *get_indexarray(void)
{
	return MEM_mallocN(sizeof(int)*MAXINDEX + 2, "vertexpaint");
}

void free_vertexpaint()
{
	
	if(Gvp.vpaint_prev) MEM_freeN(Gvp.vpaint_prev);
	Gvp.vpaint_prev= NULL;
	
	mesh_octree_table(NULL, NULL, 'e');
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

static unsigned int vpaint_get_current_col(VPaint *vp)
{
	return rgba_to_mcol(vp->r, vp->g, vp->b, 1.0f);
}

void do_shared_vertexcol(Mesh *me)
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
		if((tface && tface->mode & TF_SHAREDCOL) || (G.f & G_FACESELECT)==0) {
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
		if((tface && tface->mode & TF_SHAREDCOL) || (G.f & G_FACESELECT)==0) {
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

void make_vertexcol(int shade)	/* single ob */
{
	Object *ob;
	Mesh *me;

	if(G.obedit) {
		error("Unable to perform function in Edit Mode");
		return;
	}
	
	ob= OBACT;
	if(!ob || ob->id.lib) return;
	me= get_mesh(ob);
	if(me==0) return;

	/* copies from shadedisplist to mcol */
	if(!me->mcol) {
		CustomData_add_layer(&me->fdata, CD_MCOL, CD_CALLOC, NULL, me->totface);
		mesh_update_customdata_pointers(me);
	}

	if(shade)
		shadeMeshMCol(ob, me);
	else
		memset(me->mcol, 255, 4*sizeof(MCol)*me->totface);
	
	if (me->mr) multires_load_cols(me);
	
	DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
	
	allqueue(REDRAWBUTSEDIT, 0);
	allqueue(REDRAWVIEW3D, 0);
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

static void copy_wpaint_prev (VPaint *vp, MDeformVert *dverts, int dcount)
{
	if (vp->wpaint_prev) {
		free_dverts(vp->wpaint_prev, vp->tot);
		vp->wpaint_prev= NULL;
	}
	
	if(dverts && dcount) {
		
		vp->wpaint_prev = MEM_mallocN (sizeof(MDeformVert)*dcount, "wpaint prev");
		vp->tot = dcount;
		copy_dverts (vp->wpaint_prev, dverts, dcount);
	}
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
	if(!ob || ob->id.lib) return;

	if(me==0 || me->mcol==0 || me->totface==0) return;

	paintcol= vpaint_get_current_col(&Gvp);

	to= (unsigned int *)me->mcol;
	a= 4*me->totface;
	while(a--) {
		*to= paintcol;
		to++; 
	}
	BIF_undo_push("Clear vertex colors");
	DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
	
	allqueue(REDRAWVIEW3D, 0);
}

void clear_vpaint_selectedfaces()
{
	Mesh *me;
	MFace *mf;
	Object *ob;
	unsigned int paintcol, *mcol;
	int i;

	ob= OBACT;
	me= get_mesh(ob);
	if(me==0 || me->totface==0) return;

	if(!me->mcol)
		make_vertexcol(0);

	paintcol= vpaint_get_current_col(&Gvp);

	mf = me->mface;
	mcol = (unsigned int*)me->mcol;
	for (i = 0; i < me->totface; i++, mf++, mcol+=4) {
		if (mf->flag & ME_FACE_SEL) {
			mcol[0] = paintcol;
			mcol[1] = paintcol;
			mcol[2] = paintcol;
			mcol[3] = paintcol;
		}
	}
	
	BIF_undo_push("Clear vertex colors");
	DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
	allqueue(REDRAWVIEW3D, 0);
}


/* fills in the selected faces with the current weight and vertex group */
void clear_wpaint_selectedfaces()
{
	extern float editbutvweight;
	float paintweight= editbutvweight;
	Mesh *me;
	MFace *mface;
	Object *ob;
	MDeformWeight *dw, *uw;
	int *indexar;
	int index, vgroup;
	unsigned int faceverts[5]={0,0,0,0,0};
	unsigned char i;
	int vgroup_mirror= -1;
	
	ob= OBACT;
	me= ob->data;
	if(me==0 || me->totface==0 || me->dvert==0 || !me->mface) return;
	
	indexar= get_indexarray();
	for(index=0, mface=me->mface; index<me->totface; index++, mface++) {
		if((mface->flag & ME_FACE_SEL)==0)
			indexar[index]= 0;
		else
			indexar[index]= index+1;
	}
	
	vgroup= ob->actdef-1;
	
	/* directly copied from weight_paint, should probaby split into a seperate function */
	/* if mirror painting, find the other group */		
	if(Gwp.flag & VP_MIRROR_X) {
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
				int olddef= ob->actdef;	/* tsk, add_defgroup sets the active defgroup */
				curdef= add_defgroup_name (ob, name);
				ob->actdef= olddef;
			}
			
			if(curdef && curdef!=defgroup)
				vgroup_mirror= actdef;
		}
	}
	/* end copy from weight_paint*/
	
	copy_wpaint_prev(&Gwp, me->dvert, me->totvert);
	
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
					dw= verify_defweight(me->dvert+faceverts[i], vgroup);
					if(dw) {
						uw= verify_defweight(Gwp.wpaint_prev+faceverts[i], vgroup);
						uw->weight= dw->weight; /* set the undio weight */
						dw->weight= paintweight;
						
						if(Gwp.flag & VP_MIRROR_X) {	/* x mirror painting */
							int j= mesh_get_x_mirror_vert(ob, faceverts[i]);
							if(j>=0) {
								/* copy, not paint again */
								if(vgroup_mirror != -1) {
									dw= verify_defweight(me->dvert+j, vgroup_mirror);
									uw= verify_defweight(Gwp.wpaint_prev+j, vgroup_mirror);
								} else {
									dw= verify_defweight(me->dvert+j, vgroup);
									uw= verify_defweight(Gwp.wpaint_prev+j, vgroup);
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
	copy_wpaint_prev(&Gwp, NULL, 0);

	DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
	BIF_undo_push("Set vertex weight");
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
	if(me==0 || me->mcol==0 || me->totface==0) return;

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
	
	if(G.f & (G_VERTEXPAINT|G_WEIGHTPAINT)) {
		Gvp.r= cp[0]/255.0f;
		Gvp.g= cp[1]/255.0f;
		Gvp.b= cp[2]/255.0f;
	}
	else {
		Brush *brush= G.scene->toolsettings->imapaint.brush;

		if(brush) {
			brush->rgb[0]= cp[0]/255.0f;
			brush->rgb[1]= cp[1]/255.0f;
			brush->rgb[2]= cp[2]/255.0f;

			allqueue(REDRAWVIEW3D, 0);
			allqueue(REDRAWIMAGE, 0);
		}
	}

	allqueue(REDRAWBUTSEDIT, 0);
	addqueue(curarea->win, REDRAW, 1); /* needed for when panel is open... */
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

static void vpaint_blend( unsigned int *col, unsigned int *colorig, unsigned int paintcol, int alpha)
{

	if(Gvp.mode==VP_MIX || Gvp.mode==VP_BLUR) *col= mcol_blend( *col, paintcol, alpha);
	else if(Gvp.mode==VP_ADD) *col= mcol_add( *col, paintcol, alpha);
	else if(Gvp.mode==VP_SUB) *col= mcol_sub( *col, paintcol, alpha);
	else if(Gvp.mode==VP_MUL) *col= mcol_mul( *col, paintcol, alpha);
	else if(Gvp.mode==VP_LIGHTEN) *col= mcol_lighten( *col, paintcol, alpha);
	else if(Gvp.mode==VP_DARKEN) *col= mcol_darken( *col, paintcol, alpha);
	
	/* if no spray, clip color adding with colorig & orig alpha */
	if((Gvp.flag & VP_SPRAY)==0) {
		unsigned int testcol=0, a;
		char *cp, *ct, *co;
		
		alpha= (int)(255.0*Gvp.a);
		
		if(Gvp.mode==VP_MIX || Gvp.mode==VP_BLUR) testcol= mcol_blend( *colorig, paintcol, alpha);
		else if(Gvp.mode==VP_ADD) testcol= mcol_add( *colorig, paintcol, alpha);
		else if(Gvp.mode==VP_SUB) testcol= mcol_sub( *colorig, paintcol, alpha);
		else if(Gvp.mode==VP_MUL) testcol= mcol_mul( *colorig, paintcol, alpha);
		else if(Gvp.mode==VP_LIGHTEN)  testcol= mcol_lighten( *colorig, paintcol, alpha);
		else if(Gvp.mode==VP_DARKEN)   testcol= mcol_darken( *colorig, paintcol, alpha);
		
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


static int sample_backbuf_area(VPaint *vp, int *indexar, int totface, int x, int y, float size)
{
	unsigned int *rt;
	struct ImBuf *ibuf;
	int x1, y1, x2, y2, a, tot=0, index;
	
	if(totface+4>=MAXINDEX) return 0;
	
	if(size>64.0) size= 64.0;
	
	x1= x-size;
	x2= x+size;
	CLAMP(x1, 0, curarea->winx-1);
	CLAMP(x2, 0, curarea->winx-1);
	y1= y-size;
	y2= y+size;
	CLAMP(y1, 0, curarea->winy-1);
	CLAMP(y2, 0, curarea->winy-1);
#ifdef __APPLE__
	glReadBuffer(GL_AUX0);
#endif
	
	if(x1>=x2 || y1>=y2) return 0;
	
	ibuf = IMB_allocImBuf(2*size + 4, 2*size + 4, 32, IB_rect, 0);
	glReadPixels(x1+curarea->winrct.xmin, y1+curarea->winrct.ymin, x2-x1+1, y2-y1+1, GL_RGBA, GL_UNSIGNED_BYTE,  ibuf->rect);
	glReadBuffer(GL_BACK);	

	if(G.order==B_ENDIAN)  {
		IMB_convert_rgba_to_abgr(ibuf);
	}

	rt= ibuf->rect;
	size= (y2-y1)*(x2-x1);
	if(size<=0) return 0;

	memset(indexar, 0, sizeof(int)*totface+4);	/* plus 2! first element is total, +2 was giving valgrind errors, +4 seems ok */
	
	while(size--) {
			
		if(*rt) {
			index= framebuffer_to_index(*rt);
			if(index>0 && index<=totface)
				indexar[index] = 1;
		}
	
		rt++;
	}
	
	for(a=1; a<=totface; a++) {
		if(indexar[a]) indexar[tot++]= a;
	}

	IMB_freeImBuf(ibuf);
	
	return tot;
}

static int calc_vp_alpha_dl(VPaint *vp, float vpimat[][3], float *vert_nor, short *mval)
{
	float fac, dx, dy;
	int alpha;
	short vertco[2];
	
	if(vp->flag & VP_SOFT) {
	 	project_short_noclip(vert_nor, vertco);
		dx= mval[0]-vertco[0];
		dy= mval[1]-vertco[1];
		
		fac= sqrt(dx*dx + dy*dy);
		if(fac > vp->size) return 0;
		if(vp->flag & VP_HARD)
			alpha= 255;
		else
			alpha= 255.0*vp->a*(1.0-fac/vp->size);
	}
	else {
		alpha= 255.0*vp->a;
	}

	if(vp->flag & VP_NORMALS) {
		float *no= vert_nor+3;
		
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

static void wpaint_blend(MDeformWeight *dw, MDeformWeight *uw, float alpha, float paintval)
{
	
	if(dw==NULL || uw==NULL) return;
	
	if(Gwp.mode==VP_MIX || Gwp.mode==VP_BLUR)
		dw->weight = paintval*alpha + dw->weight*(1.0-alpha);
	else if(Gwp.mode==VP_ADD)
		dw->weight += paintval*alpha;
	else if(Gwp.mode==VP_SUB) 
		dw->weight -= paintval*alpha;
	else if(Gwp.mode==VP_MUL) 
		/* first mul, then blend the fac */
		dw->weight = ((1.0-alpha) + alpha*paintval)*dw->weight;
	else if(Gwp.mode==VP_LIGHTEN) {
		if (dw->weight < paintval)
			dw->weight = paintval*alpha + dw->weight*(1.0-alpha);
	} else if(Gwp.mode==VP_DARKEN) {
		if (dw->weight > paintval)
			dw->weight = paintval*alpha + dw->weight*(1.0-alpha);
	}
	CLAMP(dw->weight, 0.0f, 1.0f);
	
	/* if no spray, clip result with orig weight & orig alpha */
	if((Gwp.flag & VP_SPRAY)==0) {
		float testw=0.0f;
		
		alpha= Gwp.a;
		if(Gwp.mode==VP_MIX || Gwp.mode==VP_BLUR)
			testw = paintval*alpha + uw->weight*(1.0-alpha);
		else if(Gwp.mode==VP_ADD)
			testw = uw->weight + paintval*alpha;
		else if(Gwp.mode==VP_SUB) 
			testw = uw->weight - paintval*alpha;
		else if(Gwp.mode==VP_MUL) 
			/* first mul, then blend the fac */
			testw = ((1.0-alpha) + alpha*paintval)*uw->weight;		
		else if(Gwp.mode==VP_LIGHTEN) {
			if (uw->weight < paintval)
				testw = paintval*alpha + uw->weight*(1.0-alpha);
			else
				testw = uw->weight;
		} else if(Gwp.mode==VP_DARKEN) {
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
/*     sets editbutvweight to the closest weight value to vertex */
/*     note: we cant sample frontbuf, weight colors are interpolated too unpredictable */
static void sample_wpaint(int mode)
{
	Object *ob= OBACT;
	Mesh *me= get_mesh(ob);
	int index;
	short mval[2], sco[2];

	if (!me) return;
	
	getmouseco_areawin(mval);
	index= sample_backbuf(mval[0], mval[1]);
	
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
					notice("No Vertex Group Selected");
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
					
					val= pupmenu(str);
					if(val>=0) {
						ob->actdef= val+1;
						DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
						allqueue(REDRAWVIEW3D, 0);
						allqueue(REDRAWOOPS, 0);
						allqueue(REDRAWBUTSEDIT, 0);
					}
					MEM_freeN(str);
				}
				MEM_freeN(groups);
			}
			else notice("No Vertex Groups in Object");
		}
		else {
			DerivedMesh *dm;
			MDeformWeight *dw;
			extern float editbutvweight;
			float w1, w2, w3, w4, co[3], fac;
			
			dm = mesh_get_derived_final(ob, CD_MASK_BAREMESH);
			if(dm->getVertCo==NULL) {
				notice("Not supported yet");
			}
			else {
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
			}
			dm->release(dm);
		}		
		
	}
	allqueue(REDRAWBUTSEDIT, 0);
	
}

static void do_weight_paint_vertex(Object *ob, int index, int alpha, float paintweight, int vgroup_mirror)
{
	Mesh *me= ob->data;
	MDeformWeight	*dw, *uw;
	int vgroup= ob->actdef-1;
	
	if(Gwp.flag & VP_ONLYVGROUP) {
		dw= get_defweight(me->dvert+index, vgroup);
		uw= get_defweight(Gwp.wpaint_prev+index, vgroup);
	}
	else {
		dw= verify_defweight(me->dvert+index, vgroup);
		uw= verify_defweight(Gwp.wpaint_prev+index, vgroup);
	}
	if(dw==NULL || uw==NULL)
		return;
	
	wpaint_blend(dw, uw, (float)alpha/255.0, paintweight);
	
	if(Gwp.flag & VP_MIRROR_X) {	/* x mirror painting */
		int j= mesh_get_x_mirror_vert(ob, index);
		if(j>=0) {
			/* copy, not paint again */
			if(vgroup_mirror != -1)
				uw= verify_defweight(me->dvert+j, vgroup_mirror);
			else
				uw= verify_defweight(me->dvert+j, vgroup);
				
			uw->weight= dw->weight;
		}
	}
}

void weight_paint(void)
{
	extern float editbutvweight;
	Object *ob; 
	Mesh *me;
	MFace *mface;
	float mat[4][4], imat[4][4], paintweight, *vertexcosnos;
	float vpimat[3][3];
	int *indexar, index, totindex, alpha, totw;
	int vgroup_mirror= -1;
	short mval[2], mvalo[2], firsttime=1;

	if((G.f & G_WEIGHTPAINT)==0) return;
	if(G.obedit) return;
	if(multires_level1_test()) return;
	
	ob= OBACT;
	if(!ob || ob->id.lib) return;

	me= get_mesh(ob);
	if(me==NULL || me->totface==0) return;
	
	/* if nothing was added yet, we make dverts and a vertex deform group */
	if (!me->dvert)
		create_dverts(&me->id);
	
	if(G.qual & LR_CTRLKEY) {
		sample_wpaint(0);
		return;
	}
	if(G.qual & LR_SHIFTKEY) {
		sample_wpaint(1);
		return;
	}
	
	/* ALLOCATIONS! no return after this line */
		/* painting on subsurfs should give correct points too, this returns me->totvert amount */
	vertexcosnos= mesh_get_mapped_verts_nors(ob);
	indexar= get_indexarray();
	copy_wpaint_prev(&Gwp, me->dvert, me->totvert);

	/* this happens on a Bone select, when no vgroup existed yet */
	if(ob->actdef<=0) {
		Object *modob;
		if((modob = modifiers_isDeformedByArmature(ob))) {
			bPoseChannel *pchan;
			for(pchan= modob->pose->chanbase.first; pchan; pchan= pchan->next)
				if(pchan->bone->flag & SELECT)
					break;
			if(pchan) {
				bDeformGroup *dg= get_named_vertexgroup(ob, pchan->name);
				if(dg==NULL)
					dg= add_defgroup_name(ob, pchan->name);	/* sets actdef */
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
	
	getmouseco_areawin(mval);
	mvalo[0]= mval[0];
	mvalo[1]= mval[1];
	
	/* if mirror painting, find the other group */
	if(Gwp.flag & VP_MIRROR_X) {
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
				int olddef= ob->actdef;	/* tsk, add_defgroup sets the active defgroup */
				curdef= add_defgroup_name (ob, name);
				ob->actdef= olddef;
			}
			
			if(curdef && curdef!=defgroup)
				vgroup_mirror= actdef;
		}
	}
	
	while (get_mbut() & L_MOUSE) {
		getmouseco_areawin(mval);
		
		if(firsttime || mval[0]!=mvalo[0] || mval[1]!=mvalo[1]) {
			firsttime= 0;
			
			/* which faces are involved */
			if(Gwp.flag & VP_AREA) {
				totindex= sample_backbuf_area(&Gwp, indexar, me->totface, mval[0], mval[1], Gwp.size);
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

			if((G.f & G_FACESELECT) && me->mface) {
				for(index=0; index<totindex; index++) {
					if(indexar[index] && indexar[index]<=me->totface) {
					
						mface= ((MFace *)me->mface) + (indexar[index]-1);
					
						if((mface->flag & ME_FACE_SEL)==0) {
							indexar[index]= 0;
						}
					}					
				}
			}
			
			/* make sure each vertex gets treated only once */
			/* and calculate filter weight */
			totw= 0;
			if(Gwp.mode==VP_BLUR) 
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
					
					if(Gwp.mode==VP_BLUR) {
						MDeformWeight *dw, *(*dw_func)(MDeformVert *, int) = verify_defweight;
						
						if(Gwp.flag & VP_ONLYVGROUP)
							dw_func= get_defweight;
						
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
			
			if(Gwp.mode==VP_BLUR) 
				paintweight/= (float)totw;
			
			for(index=0; index<totindex; index++) {
				
				if(indexar[index] && indexar[index]<=me->totface) {
					mface= me->mface + (indexar[index]-1);
					
					if((me->dvert+mface->v1)->flag) {
						alpha= calc_vp_alpha_dl(&Gwp, vpimat, vertexcosnos+6*mface->v1, mval);
						if(alpha) {
							do_weight_paint_vertex(ob, mface->v1, alpha, paintweight, vgroup_mirror);
						}
						(me->dvert+mface->v1)->flag= 0;
					}
					
					if((me->dvert+mface->v2)->flag) {
						alpha= calc_vp_alpha_dl(&Gwp, vpimat, vertexcosnos+6*mface->v2, mval);
						if(alpha) {
							do_weight_paint_vertex(ob, mface->v2, alpha, paintweight, vgroup_mirror);
						}
						(me->dvert+mface->v2)->flag= 0;
					}
					
					if((me->dvert+mface->v3)->flag) {
						alpha= calc_vp_alpha_dl(&Gwp, vpimat, vertexcosnos+6*mface->v3, mval);
						if(alpha) {
							do_weight_paint_vertex(ob, mface->v3, alpha, paintweight, vgroup_mirror);
						}
						(me->dvert+mface->v3)->flag= 0;
					}
					
					if((me->dvert+mface->v4)->flag) {
						if(mface->v4) {
							alpha= calc_vp_alpha_dl(&Gwp, vpimat, vertexcosnos+6*mface->v4, mval);
							if(alpha) {
								do_weight_paint_vertex(ob, mface->v4, alpha, paintweight, vgroup_mirror);
							}
							(me->dvert+mface->v4)->flag= 0;
						}
					}
				}
			}
			
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
	
	if(vertexcosnos)
		MEM_freeN(vertexcosnos);
	MEM_freeN(indexar);
	copy_wpaint_prev(&Gwp, NULL, 0);

	DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);

	/* and particles too */
	if(ob->particlesystem.first) {
		ParticleSystem *psys;
		int i;

		psys= ob->particlesystem.first;
		while(psys) {
			for(i=0; i<PSYS_TOT_VG; i++) {
				if(psys->vgroup[i]==ob->actdef) {
					psys->recalc |= PSYS_RECALC_HAIR;
					break;
				}
			}

			psys= psys->next;
		}
	}
	
	BIF_undo_push("Weight Paint");
	allqueue(REDRAWVIEW3D, 0);
}

void vertex_paint()
{
	Object *ob;
	Mesh *me;
	MFace *mface;
	float mat[4][4], imat[4][4], *vertexcosnos;
	float vpimat[3][3];
	unsigned int paintcol=0, *mcol, *mcolorig, fcol1, fcol2;
	int *indexar, index, alpha, totindex;
	short mval[2], mvalo[2], firsttime=1;
	
	if((G.f & G_VERTEXPAINT)==0) return;
	if(G.obedit) return;
	
	ob= OBACT;
	if(!ob || ob->id.lib) return;

	me= get_mesh(ob);
	if(me==NULL || me->totface==0) return;
	if(ob->lay & G.vd->lay); else error("Active object is not in this layer");
	
	if(me->mcol==NULL) make_vertexcol(0);

	if(me->mcol==NULL) return;
	
	/* ALLOCATIONS! No return after his line */
	
				/* painting on subsurfs should give correct points too, this returns me->totvert amount */
	vertexcosnos= mesh_get_mapped_verts_nors(ob);
	indexar= get_indexarray();
	copy_vpaint_prev(&Gvp, (unsigned int *)me->mcol, me->totface);
	
	/* opengl/matrix stuff */
	persp(PERSP_VIEW);
	/* imat for normals */
	Mat4MulMat4(mat, ob->obmat, G.vd->viewmat);
	Mat4Invert(imat, mat);
	Mat3CpyMat4(vpimat, imat);
	
	/* load projection matrix */
	mymultmatrix(ob->obmat);
	mygetsingmatrix(mat);
	myloadmatrix(G.vd->viewmat);
	
	paintcol= vpaint_get_current_col(&Gvp);
	
	getmouseco_areawin(mvalo);
	
	getmouseco_areawin(mval);
	mvalo[0]= mval[0];
	mvalo[1]= mval[1];
	
	while (get_mbut() & L_MOUSE) {
		getmouseco_areawin(mval);
		
		if(firsttime || mval[0]!=mvalo[0] || mval[1]!=mvalo[1]) {

			firsttime= 0;

			/* which faces are involved */
			if(Gvp.flag & VP_AREA) {
				totindex= sample_backbuf_area(&Gvp, indexar, me->totface, mval[0], mval[1], Gvp.size);
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
			if((G.f & G_FACESELECT) && me->mface) {
				for(index=0; index<totindex; index++) {
					if(indexar[index] && indexar[index]<=me->totface) {
						mface= ((MFace *)me->mface) + (indexar[index]-1);
					
						if((mface->flag & ME_FACE_SEL)==0)
							indexar[index]= 0;
					}					
				}
			}

			for(index=0; index<totindex; index++) {

				if(indexar[index] && indexar[index]<=me->totface) {
				
					mface= ((MFace *)me->mface) + (indexar[index]-1);
					mcol=	  ( (unsigned int *)me->mcol) + 4*(indexar[index]-1);
					mcolorig= ( (unsigned int *)Gvp.vpaint_prev) + 4*(indexar[index]-1);

					if(Gvp.mode==VP_BLUR) {
						fcol1= mcol_blend( mcol[0], mcol[1], 128);
						if(mface->v4) {
							fcol2= mcol_blend( mcol[2], mcol[3], 128);
							paintcol= mcol_blend( fcol1, fcol2, 128);
						}
						else {
							paintcol= mcol_blend( mcol[2], fcol1, 170);
						}
						
					}
					
					alpha= calc_vp_alpha_dl(&Gvp, vpimat, vertexcosnos+6*mface->v1, mval);
					if(alpha) vpaint_blend( mcol, mcolorig, paintcol, alpha);
					
					alpha= calc_vp_alpha_dl(&Gvp, vpimat, vertexcosnos+6*mface->v2, mval);
					if(alpha) vpaint_blend( mcol+1, mcolorig+1, paintcol, alpha);
	
					alpha= calc_vp_alpha_dl(&Gvp, vpimat, vertexcosnos+6*mface->v3, mval);
					if(alpha) vpaint_blend( mcol+2, mcolorig+2, paintcol, alpha);

					if(mface->v4) {
						alpha= calc_vp_alpha_dl(&Gvp, vpimat, vertexcosnos+6*mface->v4, mval);
						if(alpha) vpaint_blend( mcol+3, mcolorig+3, paintcol, alpha);
					}
				}
			}
				
			MTC_Mat4SwapMat4(G.vd->persmat, mat);
			
			do_shared_vertexcol(me);
	
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
	
	if(vertexcosnos)
		MEM_freeN(vertexcosnos);
	MEM_freeN(indexar);
	
	/* frees prev buffer */
	copy_vpaint_prev(&Gvp, NULL, 0);

	BIF_undo_push("Vertex Paint");
	
	allqueue(REDRAWVIEW3D, 0);
}

void set_wpaint(void)		/* toggle */
{		
	Object *ob;
	Mesh *me;
	
	scrarea_queue_headredraw(curarea);
	ob= OBACT;
	if(!ob || ob->id.lib) return;
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
		Object *par;
		
		setcursor_space(SPACE_VIEW3D, CURSOR_VPAINT);
		
		mesh_octree_table(ob, NULL, 's');

		/* verify if active weight group is also active bone */
		par= modifiers_isDeformedByArmature(ob);
		if(par && (par->flag & OB_POSEMODE)) {
			bPoseChannel *pchan;
			for(pchan= par->pose->chanbase.first; pchan; pchan= pchan->next)
				if(pchan->bone->flag & BONE_ACTIVE)
					break;
			if(pchan)
				vertexgroup_select_by_name(ob, pchan->name);
		}
	}
	else {
		if(!(G.f & G_FACESELECT))
			setcursor_space(SPACE_VIEW3D, CURSOR_STD);
		
		mesh_octree_table(ob, NULL, 'e');
	}
}


void set_vpaint(void)		/* toggle */
{		
	Object *ob;
	Mesh *me;
	
	scrarea_queue_headredraw(curarea);
	ob= OBACT;
	if(!ob || object_data_is_libdata(ob)) {
		G.f &= ~G_VERTEXPAINT;
		return;
	}
	
	me= get_mesh(ob);
	
	if(me && me->totface>=MAXINDEX) {
		error("Maximum number of faces: %d", MAXINDEX-1);
		G.f &= ~G_VERTEXPAINT;
		return;
	}
	
	if(me && me->mcol==NULL) make_vertexcol(0);
	
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
	
	if (me)
		/* update modifier stack for mapping requirements */
		DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);

	if(G.f & G_VERTEXPAINT) {
		setcursor_space(SPACE_VIEW3D, CURSOR_VPAINT);
	}
	else {
		if((G.f & G_FACESELECT)==0) setcursor_space(SPACE_VIEW3D, CURSOR_STD);
	}
}

