/*  exotic.c   
 * 
 *  $Id$
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
 *  
 *  eigen videoscape formaat:
 *
 * 
 * lamp:
 *		3DG2
		aantal_lampen
		
		type
		spsi spbl
		r, g, b, energy
		locx, locy, locz
		vecx, vecy, vecz

		
	curve / nurbs:
		3DG3
		5 of 11 (curve of surf)
		aantal_nurbs
		extr1 extr2
		
		mat[0][0] mat[0][1] mat[0][2] mat[0][3]
		mat[1][0] mat[1][1] mat[1][2] mat[1][3]
		...		
		
		type
		pntsu, pntsv
		resolu, resolv
		orderu, orderv
		flagu, flagv
		
		(als type==nurb) x y z w
						 x y z w
						 ...
		(als type==bez)  xyz xyz xyz h1 h2 h3
						 xyz xyz xyz h1 h2 h3
						 ...
 *  
 * 
 */


#include <ctype.h> /* isdigit, isspace */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
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

#include "DNA_object_types.h"
#include "DNA_mesh_types.h"
#include "DNA_material_types.h"
#include "DNA_lamp_types.h"
#include "DNA_curve_types.h"
#include "DNA_image_types.h"
#include "DNA_camera_types.h"
#include "DNA_scene_types.h"
#include "DNA_view3d_types.h"
#include "DNA_userdef_types.h"

#include "BKE_bad_level_calls.h"
#include "BKE_utildefines.h"
#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_editVert.h"

#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_mesh.h"
#include "BKE_library.h"
#include "BKE_global.h"
#include "BKE_object.h"
#include "BKE_material.h"
#include "BKE_exotic.h"
/*  #include "BKE_error.h" */
#include "BKE_screen.h"
#include "BKE_displist.h"
#include "BKE_curve.h"

#include "BPY_extern.h"

/***/

static int is_dxf(char *str);
static void dxf_read(char *filename);

/***/

static void read_videoscape_mesh(char *str)
{
	Object *ob;
	Mesh *me;
	MVert *mvert;
	MFace *mface;
	Material *ma;
	FILE *fp;
	float *vertdata, *vd, min[3], max[3], cent[3], ftemp;
	unsigned int color[32], col;
	int totcol, a, b, verts, tottria=0, totquad=0, totedge=0, poly, nr0, nr, first;
	short end;
	char s[50];
	
	fp= fopen(str, "rb");
	if(fp==NULL) {
		error("Can't read file");
		return;
	}
	
	fscanf(fp, "%40s", s);
	
	fscanf(fp, "%d\n", &verts);
	if(verts<=0) {
		fclose(fp);
		error("Read error");
		return;
	}
	
	if(verts>65000) {
		error("too many vertices");
		fclose(fp);
		return;
	}
	
	INIT_MINMAX(min, max);
	vd= vertdata= MEM_mallocN(sizeof(float)*3*verts, "videoscapelezer");
	
	for(a=0; a<verts; a++) {
		fscanf(fp, "%f %f %f", vd, vd+1, vd+2);
		DO_MINMAX(vd, min, max);
		vd+=3;
	}
	
	/* count faces and colors */
	for(a=0; a<32; a++) color[a]= 0;
	totcol= 0;
	end= 1;
	while(end>0) {
		end= fscanf(fp,"%d", &poly);
		if(end<=0) break;
	
		if(poly==3) tottria++;
		else if(poly==4) totquad++;
		else totedge+= poly;
	
		for(a=0;a<poly;a++) {
			end= fscanf(fp,"%d", &nr);
			if(end<=0) break;
		}
		if(end<=0) break;
		
		end= fscanf(fp,"%i\n", &col);
		col &= 0xF0F0F0;
		for(a=0; a<totcol; a++) {
			if(color[a]==col) break;
		}
		if(a>=totcol && totcol<32) {
			color[totcol]= col;
			totcol++;
		}
	}
	
	if(totedge+tottria+totquad>65000) {
		printf(" var1: %d, var2: %d, var3: %d \n", totedge, tottria, totquad);
		error("too many faces");
		MEM_freeN(vertdata);
		fclose(fp);
		return;
	}
	
	/* new object */
	ob= add_object(OB_MESH);
	me= ob->data;
	me->totvert= verts;
	me->totface= totedge+tottria+totquad;
	
	me->mvert= MEM_callocN(me->totvert*sizeof(MVert), "mverts");
	if(me->totface) me->mface= MEM_callocN(me->totface*sizeof(MFace), "mface");
	
	/* colors */
	if(totcol) {
		ob->mat= MEM_callocN(sizeof(void *)*totcol, "ob->mat");
		me->mat= MEM_callocN(sizeof(void *)*totcol, "me->mat");
		me->totcol= totcol;
		ob->totcol= (unsigned char) me->totcol;
		ob->actcol= 1;
	}
	
	/* materials */
	for(a=0; a<totcol; a++) {
		ma= G.main->mat.first;
		while(ma) {
			if(ma->mtex[0]==0) {
				col= rgb_to_cpack(ma->r, ma->g, ma->b);
				if(color[a]==col) {
					me->mat[a]= ma;
					ma->id.us++;
					break;
				}
			}
			ma= ma->id.next;
		}
		if(ma==0) {
			ma= add_material("ext");
			me->mat[a]= ma;
			cpack_to_rgb(color[a], cent, cent+1, cent+2);
			ma->r= cent[0];
			ma->g= cent[1];
			ma->b= cent[2];
			automatname(ma);
		}
	}
	
	/* verts */
	
	cent[0]= (min[0]+max[0])/2.0f;
	cent[1]= (min[1]+max[1])/2.0f;
	cent[2]= (min[2]+max[2])/2.0f;
	VECCOPY(ob->loc, cent);
	
	a= me->totvert;
	vd= vertdata;
	mvert= me->mvert;
	while(a--) {
		VecSubf(mvert->co, vd, cent);
		mvert++;
		vd+= 3;
	}
	
	/* faces */
	if(me->totface) {
		rewind(fp);
	
		fscanf(fp, "%40s", s);
		fscanf(fp, "%d\n", &verts);
		/* fake read */
		for(a=0;a<verts;a++) {
			fscanf(fp, "%f %f %f", &ftemp, &ftemp, &ftemp);
		}
		
		a= me->totface;
		mface= me->mface;
		while(a--) {
			end= fscanf(fp,"%d", &poly);
			if(end<=0) break;
	
			if(poly==3 || poly==4) {
				fscanf(fp,"%d", &nr);
				mface->v1= MIN2(nr, me->totvert-1);
				fscanf(fp,"%d", &nr);
				mface->v2= MIN2(nr, me->totvert-1);
				fscanf(fp,"%d", &nr);
				mface->v3= MIN2(nr, me->totvert-1);
				if(poly==4) {
					if( fscanf(fp,"%d", &nr) <=0 ) break;
					mface->v4= MIN2(nr, me->totvert-1);
				}
				mface->edcode= 3;
				
				test_index_mface(mface, poly);
				
				mface++;
			}
			else {
				if( fscanf(fp,"%d", &nr0) <=0) break;
				first= nr0;
				for(b=1; b<poly; b++) {
					end= fscanf(fp,"%d", &nr);
					if(end<=0) break;
					nr= MIN2(nr, me->totvert-1);
					mface->v1= nr;
					mface->v2= nr0;
					nr0= nr;
					mface++;
					a--;
				}
				mface->v1= first;
				mface->v2= nr;
				mface->edcode= 1;
				mface++;
				if(end<=0) break;
			}
			end= fscanf(fp,"%i", &col);
			col &= 0xF0F0F0;
			if(end<=0) break;
			
			for(b=0; b<totcol; b++) {
				if(color[b]==col) {
					(mface-1)->mat_nr= b;
					break;
				}
			}
		}
	}
	
	fclose(fp);
	MEM_freeN(vertdata);
	
	G.obedit= ob;
	make_editMesh();
	load_editMesh();
	free_editMesh();
	G.obedit= 0;
	tex_space_mesh(me);
	waitcursor(1);
}

static void read_radiogour(char *str)
{
	Object *ob;
	Mesh *me;
	MVert *mvert;
	MFace *mface;
	FILE *fp;
	float *vertdata, *vd, min[3], max[3], cent[3], ftemp;
	unsigned int *colv, *colf, *colvertdata;
	int  itemp, a, b, verts, tottria=0, totquad=0, totedge=0, poly, nr0, nr, first;
	short end;
	char s[50];
	
	fp= fopen(str, "rb");
	if(fp==NULL) {
		error("Can't read file");
		return;
	}
	
	fscanf(fp, "%40s", s);
	
	fscanf(fp, "%d\n", &verts);
	if(verts<=0) {
		fclose(fp);
		error("Read error");
		return;
	}
	
	if(verts>65000) {
		error("too many vertices");
		fclose(fp);
		return;
	}
	
	INIT_MINMAX(min, max);
	vd= vertdata= MEM_mallocN(sizeof(float)*3*verts, "videoscapelezer");
	colv= colvertdata= MEM_mallocN(verts*sizeof(float), "coldata");
	
	for(a=0; a<verts; a++) {
		fscanf(fp, "%f %f %f %i", vd, vd+1, vd+2, colv);
		DO_MINMAX(vd, min, max);
		vd+=3;
		colv++;
	}
	
	/* count faces */
	end= 1;
	while(end>0) {
		end= fscanf(fp,"%d", &poly);
		if(end<=0) break;
	
		if(poly==3) tottria++;
		else if(poly==4) totquad++;
		else totedge+= poly;
	
		for(a=0;a<poly;a++) {
			end= fscanf(fp,"%d", &nr);
			if(end<=0) break;
		}
		if(end<=0) break;
		
	}
	
	if(totedge+tottria+totquad>65000) {
		printf(" var1: %d, var2: %d, var3: %d \n", totedge, tottria, totquad);
		error("too many faces");
		MEM_freeN(vertdata);
		MEM_freeN(colvertdata);
		fclose(fp);
		return;
	}
	
	/* new object */
	ob= add_object(OB_MESH);
	me= ob->data;
	me->totvert= verts;
	me->totface= totedge+tottria+totquad;
	me->flag= 0;
	me->mvert= MEM_callocN(me->totvert*sizeof(MVert), "mverts");
	if(me->totface) me->mface= MEM_callocN(me->totface*sizeof(MFace), "mface");
	
	/* verts */
	
	cent[0]= (min[0]+max[0])/2.0f;
	cent[1]= (min[1]+max[1])/2.0f;
	cent[2]= (min[2]+max[2])/2.0f;
	VECCOPY(ob->loc, cent);
	
	a= me->totvert;
	vd= vertdata;
	mvert= me->mvert;
	while(a--) {
		VecSubf(mvert->co, vd, cent);
		mvert++;
		vd+= 3;
	}
	
	/* faces */
	if(me->totface) {
		rewind(fp);
	
		fscanf(fp, "%40s", s);
		fscanf(fp, "%d\n", &verts);
		for(a=0;a<verts;a++) {
			fscanf(fp, "%f %f %f %i", &ftemp, &ftemp, &ftemp, &itemp);
		}
		
		a= me->totface;
		mface= me->mface;
		while(a--) {
			end= fscanf(fp,"%d", &poly);
			if(end<=0) break;
	
			if(poly==3 || poly==4) {
				fscanf(fp,"%d", &nr);
				mface->v1= MIN2(nr, me->totvert-1);
				fscanf(fp,"%d", &nr);
				mface->v2= MIN2(nr, me->totvert-1);
				fscanf(fp,"%d", &nr);
				mface->v3= MIN2(nr, me->totvert-1);
				if(poly==4) {
					if( fscanf(fp,"%d", &nr) <=0 ) break;
					mface->v4= MIN2(nr, me->totvert-1);
				}
				mface->edcode= 3;
				
				test_index_mface(mface, poly);
				
				mface++;
			}
			else {
				if( fscanf(fp,"%d", &nr0) <=0) break;
				first= nr0;
				for(b=1; b<poly; b++) {
					end= fscanf(fp,"%d", &nr);
					if(end<=0) break;
					nr= MIN2(nr, me->totvert-1);
					mface->v1= nr;
					mface->v2= nr0;
					nr0= nr;
					mface++;
					a--;
				}
				mface->v1= first;
				mface->v2= nr;
				mface->edcode= 1;
				mface->flag= ME_SMOOTH;
				
				mface++;
				if(end<=0) break;
			}
		}
		
		/* mcol is 4 colors per face */
		me->mcol= MEM_mallocN(4*sizeof(int)*me->totface, "mcol");
		colf= (unsigned int *)me->mcol;

		a= me->totface;
		mface= me->mface;
		while(a--) {
			
			colf[0]= colvertdata[mface->v1];
			colf[1]= colvertdata[mface->v2];
			colf[2]= colvertdata[mface->v3];
			colf[3]= colvertdata[mface->v4];
			
			colf+= 4;
			mface++;
		}
		
		MEM_freeN(colvertdata);
	}
	
	fclose(fp);
	MEM_freeN(vertdata);
	
	G.obedit= ob;
	make_editMesh();
	load_editMesh();
	free_editMesh();

	G.obedit= 0;
	tex_space_mesh(me);

	waitcursor(1);
}


static void read_videoscape_lamp(char *str)
{
	Object *ob;
	Lamp *la;
	FILE *fp;
	float vec[3], *q1;
	int tot, val;
	char s[50];
	
	fp= fopen(str, "rb");
	if(fp==NULL) {
		error("Can't read file");
		return;
	}

	fscanf(fp, "%40s", s);
	fscanf(fp, "%d\n", &tot);
	
	while(tot--) {
		ob= add_object(OB_LAMP);
		la= ob->data;
		
		fscanf(fp, "%d\n", &val);
		la->type= val;
		if(la->type==1) la->type= LA_SPOT;
		else if(la->type==2) la->type= LA_SUN;
		
		fscanf(fp, "%f %f\n", &la->spotsize, &la->spotblend);
		
		fscanf(fp, "%f %f %f %f\n", &la->r, &la->g, &la->b, &la->energy);		
		
		fscanf(fp, "%f %f %f\n", ob->loc, ob->loc+1, ob->loc+2);
		val= fscanf(fp, "%f %f %f\n", vec, vec+1, vec+2);
		q1= vectoquat(vec, 5, 2);
		QuatToEul(q1, ob->rot);
		
		if(val<=0) break;
		
	}
	fclose(fp);
}

static void read_videoscape_nurbs(char *str)
{
	Object *ob;
	Curve *cu;
	Nurb *nu;
	BezTriple *bezt;
	BPoint *bp;
	FILE *fp;
	float tmat[4][4], omat[3][3], imat[3][3], mat[3][3];
	int a, tot, type, val;
	char s[50];

	fp= fopen(str, "rb");
	if(fp==NULL) {
		error("Can't read file");
		return;
	}

	fscanf(fp, "%40s", s);
	fscanf(fp, "%d\n", &type);
	
	if(type==5) ob= add_object(OB_SURF);
	else ob= add_object(OB_CURVE);
	cu= ob->data;
	
	fscanf(fp, "%d\n", &tot);
	fscanf(fp, "%d %d\n", &type, &val);
	
	cu->ext1= 0.002f*type;
	cu->ext2= 0.002f*val;

	for(a=0; a<4; a++) fscanf(fp, "%e %e %e %e\n", tmat[a], tmat[a]+1, tmat[a]+2, tmat[a]+3);

	VECCOPY(ob->loc, tmat[3]);

	Mat3CpyMat4(omat, tmat);
	Mat3ToEul(omat, ob->rot);
	EulToMat3(ob->rot, mat);
	Mat3Inv(imat, mat);
	Mat3MulMat3(tmat, imat, omat);
	
	while(tot--) {
		nu= (Nurb*)MEM_callocN(sizeof(Nurb),"nu from exotic");
		BLI_addtail(&cu->nurb, nu);
		
		fscanf(fp, "%d\n", &type);
		nu->type= type;

		fscanf(fp, "%d %d\n", &type, &val);
		nu->pntsu= type; nu->pntsv= val;
		fscanf(fp, "%d %d\n", &type, &val);
		nu->resolu= type; nu->resolv= val;
		fscanf(fp, "%d %d\n", &type, &val);
		nu->orderu= type; nu->orderv= val;
		fscanf(fp, "%d %d\n", &type, &val);
		nu->flagu= type; nu->flagv= val;
		
		if( (nu->type & 7)==CU_BEZIER) {
			a= nu->pntsu;
			nu->bezt= bezt= MEM_callocN(a*sizeof(BezTriple), "bezt from exotic");
			while(a--) {
				fscanf(fp, "%f %f %f ", bezt->vec[0], bezt->vec[0]+1, bezt->vec[0]+2);
				Mat4MulVecfl(tmat, bezt->vec[0]);
				fscanf(fp, "%f %f %f ", bezt->vec[1], bezt->vec[1]+1, bezt->vec[1]+2);
				Mat4MulVecfl(tmat, bezt->vec[1]);
				fscanf(fp, "%f %f %f ", bezt->vec[2], bezt->vec[2]+1, bezt->vec[2]+2);
				Mat4MulVecfl(tmat, bezt->vec[2]);
				fscanf(fp, "%d %d\n", &type, &val);
				bezt->h1= type;
				bezt->h2= val;
				bezt++;
			}
		}
		else {
			a= nu->pntsu*nu->pntsv;
			if(a) {
				nu->bp= bp= MEM_callocN(a*sizeof(BPoint), "bp from exotic");
				while(a--) {
					fscanf(fp, "%f %f %f %f\n", bp->vec, bp->vec+1, bp->vec+2, bp->vec+3);
					Mat4MulVecfl(tmat, bp->vec);
					bp++;
				}
				
				val= KNOTSU(nu);
				nu->knotsu= MEM_mallocN(sizeof(float)*val, "knots");
				for(a=0; a<val; a++) fscanf(fp, "%f\n", nu->knotsu+a);
				
				if(nu->pntsv>1) {
					val= KNOTSV(nu);
					nu->knotsv= MEM_mallocN(sizeof(float)*val, "knots");
					for(a=0; a<val; a++) fscanf(fp, "%f\n", nu->knotsv+a);
				}
			}
			else {
				BLI_remlink(&cu->nurb, nu);
				MEM_freeN(nu);
			}
		}
	}
	fclose(fp);
	makeDispList(ob);
}

static void read_videoscape(char *str)
{
	int file, type;
	unsigned short val, numlen;
	char name[FILE_MAXDIR+FILE_MAXFILE], head[FILE_MAXFILE], tail[FILE_MAXFILE];
	
	strcpy(name, str);

	while( TRUE ) {
		file= open(name, O_BINARY|O_RDONLY);
		if(file<=0) break;
		else {
			read(file, &type, 4);
			close(file);
			
			if(type==DDG1) read_videoscape_mesh(name);
			else if(type==DDG2) read_videoscape_lamp(name);
			else if(type==DDG3) read_videoscape_nurbs(name);
		}

		val = BLI_stringdec(name, head, tail, &numlen);
		BLI_stringenc(name, head, tail, numlen, val + 1);

	}
}


/* ***************** INVENTOR ******************* */


#define IV_MAXSTACK 500000
#define IV_MAXFIELD 10
#define IV_MAXCOL 16

static float *iv_data_stack;
static float ivcolors[IV_MAXCOL][3];
static Object *ivsurf;
static ListBase ivbase;

struct IvNode {
	struct IvNode *next, *prev;
	char *nodename;
	char *fieldname[IV_MAXFIELD];
	int datalen[IV_MAXFIELD];
	float *data[IV_MAXFIELD];
};

static int iv_curcol=0;

static int iv_colornumber(struct IvNode *iv)
{
	float *fp, fr = 0.0, fg = 0.0, fb = 0.0;
	int a;
	char *cp;
	
	/* search back to last material */
	while(iv) {
		if( strcmp(iv->nodename, "Material")==0) {
			fp= iv->data[0];
			if(fp==0) fp= iv->data[1];
			if(fp) {
				fr= fp[0];
				fg= fp[1];
				fb= fp[2];
			}
			break;
		}
		else if( strcmp(iv->nodename, "BaseColor")==0) {
			fp= iv->data[0];
			fr= fp[0];
			fg= fp[1];
			fb= fp[2];
			break;
		}
		else if( strcmp(iv->nodename, "PackedColor")==0) {
			cp= (char *)iv->data[0];
			fr= cp[3]/255.0f;
			fg= cp[2]/255.0f;
			fb= cp[1]/255.0f;
			break;
		}
		iv= iv->prev;
		
	}
	if(iv==0) return 0;
	if(iv->datalen[0]<3) return 0;
	
	for(a=0; a<iv_curcol; a++) {
	
		if(ivcolors[a][0]== fr)
			if(ivcolors[a][1]== fg)
				if(ivcolors[a][2]== fb) return a+1
				;
	}
	
	if(a>=IV_MAXCOL) a= IV_MAXCOL-1;
	iv_curcol= a+1;
	ivcolors[a][0]= fr;
	ivcolors[a][1]= fg;
	ivcolors[a][2]= fb;
	
	return iv_curcol;
}

static int iv_finddata(struct IvNode *iv, char *field, int fieldnr)
{
	/* search for "field", count data size and make datablock. return skipdata */
	float *fp;
	int len, stackcount, skipdata=0;
	char *cpa, terminator, str[64];
	long i;
	
	len= strlen(field);

	cpa= iv->nodename+1;
	while( *cpa != '}' ) {
		
		if( *cpa == *field ) {
			if( strncmp(cpa, field, len)==0 ) {
				iv->fieldname[fieldnr]= cpa;
				
				/* read until first character */
				cpa+= len;
				skipdata+= len;
				*cpa= 0;
				cpa++;
				skipdata++;
				
				while( *cpa==32 || *cpa==13 || *cpa==10 || *cpa==9) cpa++;
				if( *cpa=='[' ) {
					terminator= ']';
					cpa++;
					skipdata++;
				}
				else terminator= 13;
				
				stackcount= 0;
				fp= iv_data_stack;
				
				while( *cpa!=terminator && *cpa != '}' ) {
					
					/* in fact, isdigit should include the dot and minus */
					if( (isdigit(*cpa) || *cpa=='.' || *cpa=='-') && (isspace(cpa[-1]) || cpa[-1]==0 || cpa[-1]==',') ) {
						if(cpa[1]=='x') {
							memcpy(str, cpa, 16);
							str[16]= 0;
							
							sscanf(str, "%x", (int *)fp);
						}
						else {
							/* atof doesn't stop after the first float
							 * in a long string at Windows... so we copy 
							 * the float to a new string then atof... */
							 
							i=strpbrk(cpa, ", \n")-cpa;
							
							if (i>63) *fp= 0.0;
							else {
								memcpy(str, cpa, i);
								str[i]=0;
							
								*fp= (float) atof(str);
							}
						}
												
						stackcount++;
						if(stackcount>=IV_MAXSTACK) {
							printf("stackoverflow in IV read\n");
							break;
						}
						fp++;
					}
					cpa++;
					skipdata++;
				}
				
				iv->datalen[fieldnr]= stackcount;
				if(stackcount) {
					iv->data[fieldnr]= MEM_mallocN(sizeof(float)*stackcount, "iv_finddata");
					memcpy(iv->data[fieldnr], iv_data_stack, sizeof(float)*stackcount);
				}
				else iv->data[fieldnr]= 0;
				
				return skipdata;
			}
		}
		cpa++;
		skipdata++;
	}
	
	return skipdata;
}

static void read_iv_index(float *data, float *baseadr, float *index, int nr, int coordtype)
{
	/* write in data: baseadr with offset index (and number nr) */
	float *fp;
	int ofs;
	
	while(nr--) {
		ofs= (int) *index;
		fp= baseadr+coordtype*ofs;
		VECCOPY(data, fp);
		data+= 3;
		index++;
	}
}



static void read_inventor(char *str, struct ListBase *listb)
{
	struct IvNode *iv, *ivp, *ivn;
	char *maindata, *md, *cpa;
	float *index, *data, *fp;
	int file, filelen, count, lll, face, nr = 0;
	int skipdata, ok, a, b, tot, first, colnr, coordtype, polytype, *idata;
	struct DispList *dl;
	
	ivbase.first= ivbase.last= 0;
	iv_curcol= 0;
	ivsurf= 0;
	
	file= open(str, O_BINARY|O_RDONLY);
	if(file== -1) {
		error("Can't read file\n");
		return;
	}
	filelen= BLI_filesize(file);
	
	maindata= MEM_mallocN(filelen, "leesInventor");
	read(file, maindata, filelen);
	close(file);

	iv_data_stack= MEM_mallocN(sizeof(float)*IV_MAXSTACK, "ivstack");

	/* preprocess: remove comments */
	md= maindata+20;
	count= 20;
	while(count<filelen) {
		if( *md=='#' ) {	/* comment */
			while( *md!=13 && *md!=10) {	/* enters */
				*md= 32;
				md++;
				count++;
				if(count>=filelen) break;
			}
		}
		md++;
		count++;	
	}
	

	/* now time to collect: which are the nodes and fields? */
	md= maindata;
	count= 0;
	while(count<filelen) {
		if( *md=='{' ) {	/* read back */
		
			cpa= md-1;
			while( *cpa==32 || *cpa==13 || *cpa==10 || *cpa==9) {	/* remove spaces/enters/tab  */
				*cpa= 0;
				cpa--;
			}		
				
			while( *cpa>32 && *cpa<128) cpa--;
			cpa++;
			*md= 0;
			
			ok= 0;
			skipdata= 0;
			iv= MEM_callocN(sizeof(struct IvNode), "leesInventor");
			iv->nodename= cpa;

			if(strcmp(cpa, "Coordinate3")==0 || strcmp(cpa, "Coordinate4")==0) {
				skipdata= iv_finddata(iv, "point", 0);
				ok= 1;
			}
			else if(strcmp(cpa, "VertexProperty")==0) {
				skipdata= iv_finddata(iv, "vertex", 0);
				ok= 1;
			}
			else if(strcmp(cpa, "IndexedLineSet")==0) {
				skipdata= iv_finddata(iv, "coordIndex", 0);
				ok= 1;
			}
			else if(strcmp(cpa, "IndexedTriangleMesh")==0) {
				skipdata= iv_finddata(iv, "coordIndex", 0);
				ok= 1;
			}
			else if(strcmp(cpa, "IndexedFaceSet")==0) {
				skipdata= iv_finddata(iv, "coordIndex", 0);
				ok= 1;
			}
			else if(strcmp(cpa, "FaceSet")==0) {
				skipdata= iv_finddata(iv, "numVertices", 0);
				ok= 1;
			}
			else if(strcmp(cpa, "Material")==0) {
				iv_finddata(iv, "diffuseColor", 0);
				iv_finddata(iv, "ambientColor", 1);
				ok= 1;
			}
			else if(strcmp(cpa, "BaseColor")==0) {
				iv_finddata(iv, "rgb", 0);
				ok= 1;
			}
			else if(strcmp(cpa, "PackedColor")==0) {
				iv_finddata(iv, "rgba", 0);
				ok= 1;
			}
			else if(strcmp(cpa, "QuadMesh")==0) {
				iv_finddata(iv, "verticesPerColumn", 0);
				iv_finddata(iv, "verticesPerRow", 1);
				
				ok= 1;
			}
			else if(strcmp(cpa, "IndexedTriangleStripSet")==0) {
				skipdata= iv_finddata(iv, "coordIndex", 0);
				ok= 1;
			}
			else if(strcmp(cpa, "TriangleStripSet")==0) {
				skipdata= iv_finddata(iv, "numVertices", 0);
				ok= 1;
			}
			else if(strcmp(cpa, "IndexedNurbsSurface")==0 || strcmp(cpa, "NurbsSurface")==0) {
				iv_finddata(iv, "numUControlPoints", 0);
				iv_finddata(iv, "numVControlPoints", 1);
				iv_finddata(iv, "uKnotVector", 2);
				iv_finddata(iv, "vKnotVector", 3);
				ok= 1;
			}
			else {
				/* to the end */
				while( *md != '}') {
					md++;
					count++;
					if(count<filelen) break;
				}
			}
			
			
			if(ok) {
				BLI_addtail(&ivbase, iv);
				md+= skipdata;
				count+= skipdata;
			}
			else MEM_freeN(iv);
			
		}
		md++;
		count++;
	}
	
	/* join nodes */
	iv= ivbase.first;
	
	while(iv) {
		ivn= iv->next;
		
		if( strncmp(iv->nodename, "Indexed", 7)==0) {
			/* seek back: same name? */
			
			ivp= iv->prev;
			while(ivp) {
				if(strcmp(iv->nodename, ivp->nodename)==0) break;

				if(strcmp(ivp->nodename, "Coordinate3")==0 || 
				   strcmp(ivp->nodename, "Coordinate4")==0 ||
				   strcmp(ivp->nodename, "VertexProperty")==0) {
					ivp= 0;
					break;
				}
				ivp= ivp->prev;
			}
			
			if(ivp) {
				/* add iv to ivp */
				
				tot= iv->datalen[0] + ivp->datalen[0];
				if(tot) {
					data= MEM_mallocN(tot*sizeof(float), "samenvoeg iv");
					memcpy(data, ivp->data[0], sizeof(float)*ivp->datalen[0]);
					memcpy(data+ivp->datalen[0], iv->data[0], sizeof(float)*iv->datalen[0]);
					
					ivp->datalen[0]+= iv->datalen[0];
					MEM_freeN(ivp->data[0]);
					ivp->data[0]= data;
					
					BLI_remlink(&ivbase, iv);
					MEM_freeN(iv->data[0]);
					MEM_freeN(iv);
				}
			}
		}
		
		iv= ivn;
	}

	
	/* convert Nodes to DispLists */
	iv= ivbase.first;
	while(iv) {
		
		/* printf(" Node: %s\n", iv->nodename); */
		/* if(iv->fieldname[0]) printf(" Field: %s len %d\n", iv->fieldname[0], iv->datalen[0]); */
		coordtype= 3;
		
		if( strcmp(iv->nodename, "IndexedLineSet")==0 ) {
			
			colnr= iv_colornumber(iv);

			/* seek back to data */
			ivp= iv;
			while(ivp->prev) {
				ivp= ivp->prev;
				if( strcmp(ivp->nodename, "Coordinate3")==0 ) {
					coordtype= 3;
					break;
				}
				if( strcmp(ivp->nodename, "Coordinate4")==0 ) {
					coordtype= 4;
					break;
				}
			}
			if(ivp) {
			
				/* count the nr of lines */
				tot= 0;
				index= iv->data[0];
                                lll = iv->datalen[0]-1;
				for(a=0; a<lll; a++) {
					if(index[0]!= -1 && index[1]!= -1) tot++;
					index++;
				}
				
				tot*= 2;	/* nr of vertices */
				dl= MEM_callocN(sizeof(struct DispList)+tot*3*sizeof(float), "leesInventor1");
				BLI_addtail(listb, dl);
				dl->type= DL_SEGM;
				dl->nr= 2;
				dl->parts= tot/2;
				dl->col= colnr;
				data= (float *)(dl+1);
				
				index= iv->data[0];
				for(a=0; a<lll; a++) {
					if(index[0]!= -1 && index[1]!= -1) {
						read_iv_index(data, ivp->data[0], index, 2, coordtype);
						data+= 6;
					}
					index++;
				}
			}
		}
		else if( strcmp(iv->nodename, "FaceSet")==0 ) {
			
			colnr= iv_colornumber(iv);
		
			/* seek back to data */
			ivp= iv;
			while(ivp->prev) {
				ivp= ivp->prev;
				if( strcmp(ivp->nodename, "Coordinate3")==0 ) {
					coordtype= 3;
					break;
				}
				if( strcmp(ivp->nodename, "Coordinate4")==0 ) {
					coordtype= 4;
					break;
				}
			}
			
			if(ivp) {
				/* count triangles */
				tot= 0;
				
				index= iv->data[0];
				polytype= (int) index[0];
				
				for(a=0; a<iv->datalen[0]; a++) {
					if(index[0]== polytype) tot++;	/* one kind? */
					index++;
				}
				
				
				tot*= polytype;		/* nr of vertices */
				dl= MEM_callocN(sizeof(struct DispList)+tot*3*sizeof(float), "leesInventor4");
				BLI_addtail(listb, dl);
				dl->type= DL_POLY;
				dl->nr= polytype;
				dl->parts= tot/polytype;
				dl->col= colnr;
				data= (float *)(dl+1);

				index= ivp->data[0];
				first= 1;
				for(a=0; a<iv->datalen[0]; a++) {
					
					VECCOPY(data, index);
					data+= 3;
					index+= 3;

					VECCOPY(data, index);
					data+= 3;
					index+= 3;

					VECCOPY(data, index);
					data+= 3;
					index+= 3;

					if(polytype==4) {
						VECCOPY(data, index);
						data+= 3;
						index+= 3;
					}
				}
			}
		}
		else if( strcmp(iv->nodename, "TriangleStripSet")==0 ) {
			
			colnr= iv_colornumber(iv);
		
			/* seek back to data */
			ivp= iv;
			while(ivp->prev) {
				ivp= ivp->prev;
				if( strcmp(ivp->nodename, "Coordinate3")==0 ) {
					coordtype= 3;
					break;
				}
				if( strcmp(ivp->nodename, "Coordinate4")==0 ) {
					coordtype= 4;
					break;
				}
			}
			
			if(ivp) {
				/* count triangles */
				tot= 0;
				face= 0;
				
				index= iv->data[0];		/* strip size */ 
				
				for(a=0; a<iv->datalen[0]; a++) {
					tot+= (int) index[0];
					face+= ((int) index[0]) - 2;
					index++;
				}
				
				dl= MEM_callocN(sizeof(struct DispList), "leesInventor4");
				dl->verts= MEM_callocN( tot*3*sizeof(float), "dl verts");
				dl->index= MEM_callocN( face*3*sizeof(int), "dl index");
				
				dl->type= DL_INDEX3;
				dl->nr= tot;
				dl->parts= face;

				BLI_addtail(listb, dl);
				dl->col= colnr;

				index= iv->data[0];		/* strip size */ 
				fp= ivp->data[0];		/* vertices */
				data= dl->verts;
				idata= dl->index;
				first= 0;
				
				for(a=0; a<iv->datalen[0]; a++) {
					
					/* vertices */
					for(b=0; b<index[0]; b++) {
						VECCOPY(data, fp);
						data+= 3; 
						fp+= coordtype;
					}
						
					/* indices */
                                        lll = index[0] - 2;
					for(b=0; b<lll; b++) {
						idata[0]= first;
						idata[1]= first+1;
						idata[2]= first+2;
						first++;
						idata+= 3;
					}
					first+= 2;
					
					index++;
				}
			}
		}
		else if( strcmp(iv->nodename, "IndexedFaceSet")==0 ) {
			
			colnr= iv_colornumber(iv);
		
			/* seek back to data */
			ivp= iv;
			while(ivp->prev) {
				ivp= ivp->prev;
				if( strcmp(ivp->nodename, "Coordinate3")==0 ) {
					coordtype= 3;
					break;
				}
				if( strcmp(ivp->nodename, "Coordinate4")==0 ) {
					coordtype= 4;
					break;
				}
			}
			if(ivp) {
			
				/* count triangles */
				face= 0;
				index= iv->data[0];
                lll = iv->datalen[0]-2;
				for(a=0; a<lll; a++) {
					if(index[0]!= -1 && index[1]!= -1 && index[2]!= -1) face++;
					index++;
				}

				/*number of vertices */
				tot= ivp->datalen[0]/coordtype;

				if(tot) {
					dl= MEM_callocN(sizeof(struct DispList), "leesInventor5");
					BLI_addtail(listb, dl);
					dl->type= DL_INDEX3;
					dl->nr= tot;
					dl->parts= face;
					dl->col= colnr;
	
					dl->verts= MEM_callocN( tot*3*sizeof(float), "dl verts");
					dl->index= MEM_callocN(sizeof(int)*3*face, "dl index");
	
					/* vertices */
					fp= ivp->data[0];
					data= dl->verts;
					for(b=tot; b>0; b--) {
						VECCOPY(data, fp);
						data+= 3; 
						fp+= coordtype;
					}
					
					/* indices */
					index= iv->data[0];
					idata= dl->index;
					first= 1;
					lll=iv->datalen[0]-2;
					for(a=0; a<lll; a++) {
						
						if(index[0]!= -1 && index[1]!= -1 && index[2]!= -1) {
	
							/* this trick is to fill poly's with more than 3 vertices correctly */
							if(first) {
								nr= (int) index[0];
								first= 0;
							}
							idata[0]= nr;
							idata[1]= (int) index[1];
							idata[2]= (int) index[2];
							idata+= 3;
						}
						else first= 1;
						
						index++;
					}
				}
			}
		}
		else if( strcmp(iv->nodename, "IndexedTriangleMesh")==0 || 
				 strcmp(iv->nodename, "IndexedTriangleStripSet")==0 ) {
			
			colnr= iv_colornumber(iv);
		
			/* seek back to data */
			ivp= iv;
			while(ivp->prev) {
				ivp= ivp->prev;
				if( strcmp(ivp->nodename, "Coordinate3")==0 ) {
					coordtype= 3;
					break;
				}
				if( strcmp(ivp->nodename, "Coordinate4")==0 ) {
					coordtype= 4;
					break;
				}
			}
			if(ivp) {
			
				/* count triangles */
				face= 0;
				index= iv->data[0];
                                lll=iv->datalen[0]-2;
				for(a=0; a<lll; a++) {
					if(index[0]!= -1 && index[1]!= -1 && index[2]!= -1) face++;
					index++;
				}
				
				/* nr of vertices */
				tot= ivp->datalen[0]/coordtype;
				
				dl= MEM_callocN(sizeof(struct DispList), "leesInventor6");
				BLI_addtail(listb, dl);
				dl->type= DL_INDEX3;
				dl->nr= tot;
				dl->parts= face;
				dl->col= colnr;
				
				dl->verts= MEM_callocN( tot*3*sizeof(float), "dl verts");
				dl->index= MEM_callocN(sizeof(int)*3*face, "dl index");

				/* vertices */
				fp= ivp->data[0];
				data= dl->verts;
				for(b=tot; b>0; b--) {
					VECCOPY(data, fp);
					data+= 3; 
					fp+= coordtype;
				}
				
				/* indices */
				index= iv->data[0];
				idata= dl->index;
				
                                lll=iv->datalen[0]-2;
				for(a=lll; a>0; a--) {
				
					if(index[0]!= -1 && index[1]!= -1 && index[2]!= -1) {
						idata[0]= (int) index[0];
						idata[1]= (int) index[1];
						idata[2]= (int) index[2];
						idata+= 3;
					}
					index++;
				}
			}
		}
		else if( strcmp(iv->nodename, "QuadMesh")==0 ) {
			
			colnr= iv_colornumber(iv);
		
			/* seek back to data */
			ivp= iv;
			while(ivp->prev) {
				ivp= ivp->prev;
				if( strcmp(ivp->nodename, "Coordinate3")==0 ) {
					coordtype= 3;
					break;
				}
				if( strcmp(ivp->nodename, "VertexProperty")==0 ) {
					coordtype= 3;
					break;
				}
				if( strcmp(ivp->nodename, "Coordinate4")==0 ) {
					coordtype= 4;
					break;
				}
			}
			
			if(ivp) {
				tot= (int) (floor(*(iv->data[0])+0.5) * floor(*(iv->data[1])+0.5));

				if(tot>0) {
					dl= MEM_callocN(sizeof(struct DispList)+tot*3*sizeof(float), "leesInventor8");
					BLI_addtail(listb, dl);
					dl->type= DL_SURF;
					dl->parts= (int) floor(*(iv->data[0])+0.5);
					dl->nr= (int) floor(*(iv->data[1])+0.5);
					dl->col= colnr;
					data= (float *)(dl+1);
					memcpy(data, ivp->data[0], tot*3*sizeof(float));
				}
			}
		}
		else if(strcmp(iv->nodename, "IndexedNurbsSurface")==0 || strcmp(iv->nodename, "NurbsSurface")==0) {
			
			colnr= iv_colornumber(iv);
		
			/* sek back to data */
			ivp= iv;
			while(ivp->prev) {
				ivp= ivp->prev;
				if( strcmp(ivp->nodename, "Coordinate3")==0 ) {
					coordtype= 3;
					break;
				}
				if( strcmp(ivp->nodename, "Coordinate4")==0 ) {
					coordtype= 4;
					break;
				}
			}
			if(ivp) {
				a= (int) *(iv->data[0]);
				b= (int) *(iv->data[1]);
				
				tot= a*b;

				if( (a>=4 || b>=4) && tot>6) {
					Object *ob;
					Curve *cu;
					Nurb *nu;
					BPoint *bp;
					
					if(ivsurf==0) {
						ob= add_object(OB_SURF);
						ivsurf= ob;
					}
					else ob= ivsurf;
					cu= ob->data;
					nu = (Nurb*) MEM_callocN(sizeof(Nurb),"addNurbprim") ;
					BLI_addtail(&cu->nurb, nu);
					nu->type= CU_NURBS;

					nu->pntsu= a;
					nu->pntsv= b;
					nu->resolu= 2*a;
					nu->resolv= 2*b;

					nu->flagu= 0;
					nu->flagv= 0;
					
					nu->bp = bp =
						(BPoint*)MEM_callocN(tot * sizeof(BPoint), "addNurbprim3");
					a= tot;
					data= ivp->data[0];
					while(a--) {
						VECCOPY(bp->vec, data);
						if(coordtype==4) {
							bp->vec[3]= data[3];
							VecMulf(bp->vec, 1.0f/data[3]);
						}
						else bp->vec[3]= 1.0;
						data+= coordtype;
						bp++;
					}
					
					/* iv->datalen[2] / [3] is number of knots */
					nu->orderu= iv->datalen[2] - nu->pntsu;
					nu->orderv= iv->datalen[3] - nu->pntsv;
					
					nu->knotsu= MEM_mallocN( sizeof(float)*(iv->datalen[2]), "knots");
					memcpy(nu->knotsu, iv->data[2], sizeof(float)*(iv->datalen[2]));
					nu->knotsv= MEM_mallocN( sizeof(float)*(iv->datalen[3]), "knots");
					memcpy(nu->knotsv, iv->data[3], sizeof(float)*(iv->datalen[3]));					

					switchdirectionNurb(nu);

				}
				else {
					dl= MEM_callocN(sizeof(struct DispList)+tot*3*sizeof(float), "leesInventor3");
					BLI_addtail(listb, dl);
					dl->type= DL_SURF;
					dl->nr= (int) *(iv->data[0]);
					dl->parts= (int) *(iv->data[1]);
					dl->col= colnr;
					data= (float *)(dl+1);
					
					a= tot;
					fp= ivp->data[0];
					while(a--) {
						VECCOPY(data, fp);
						fp+= coordtype;
						data+= 3;
					}
				}
			}
		}
		iv= iv->next;
	}

	/* free */
	iv= ivbase.first;
	while(iv) {
		for(a=0; a<IV_MAXFIELD; a++) {
			if(iv->data[a]) MEM_freeN(iv->data[a]);
		}
		iv= iv->next;
	}

	BLI_freelistN(&ivbase);
	MEM_freeN(maindata);
	MEM_freeN(iv_data_stack);
	
}

/* ************************************************************ */

static void displist_to_mesh(DispList *dlfirst)
{
	Object *ob;
	Mesh *me;
	Material *ma;
	DispList *dl;
	MVert *mvert;
	MFace *mface;
	float *data, vec[3], min[3], max[3];
	int a, b, startve, *idata, totedge=0, tottria=0, totquad=0, totvert=0, totvlak, totcol=0, colnr;
	int p1, p2, p3, p4;
		
	/* count first */
	INIT_MINMAX(min, max);

	dl= dlfirst;
	while(dl) {
	
		/* PATCH 1 (polyfill) can't be done, there's no listbase here. do that first! */
		/* PATCH 2 */
		if(dl->type==DL_SEGM && dl->nr>2) {
			data= (float *)(dl+1);
			if(data[0]==data[3*(dl->nr-1)]) {
				if(data[1]==data[3*(dl->nr-1)+1]) {
					if(data[2]==data[3*(dl->nr-1)+2]) {
						dl->type= DL_POLY;
						dl->nr--;
					}
				}
			}
		}
		
		/* colors */
		if(dl->col > totcol) totcol= dl->col;
		
		/* size and count */
		if(dl->type==DL_SURF) {
			a= dl->nr;
			b= dl->parts;
			if(dl->flag & 1) a++;
			if(dl->flag & 2) b++;
			
			totquad+= a*b;

			totvert+= dl->nr*dl->parts;

			data= (float *)(dl+1);
			for(a= dl->nr*dl->parts; a>0; a--) {
				DO_MINMAX(data, min, max);
				data+= 3;
			}
		}
		else if(dl->type==DL_POLY) {
			if(dl->nr==3 || dl->nr==4) {
				if(dl->nr==3) tottria+= dl->parts;
				else totquad+= dl->parts;
				
				totvert+= dl->nr*dl->parts;

				data= (float *)(dl+1);
				for(a= dl->nr*dl->parts; a>0; a--) {
					DO_MINMAX(data, min, max);
					data+= 3;
				}
			}
			else if(dl->nr>4) {
				
				tottria+= dl->nr*dl->parts;
				totvert+= dl->nr*dl->parts;
				
				data= (float *)(dl+1);
				for(a= dl->nr*dl->parts; a>0; a--) {
					DO_MINMAX(data, min, max);
					data+= 3;
				}
				
			}
		}
		else if(dl->type==DL_INDEX3) {
			tottria+= dl->parts;
			totvert+= dl->nr;
			
			data= dl->verts;
			for(a= dl->nr; a>0; a--) {
				DO_MINMAX(data, min, max);
				data+= 3;
			}
		}
		else if(dl->type==DL_SEGM) {
			
			tottria+= (dl->nr-1)*dl->parts;
			totvert+= dl->nr*dl->parts;
			
			data= (float *)(dl+1);
			for(a= dl->nr*dl->parts; a>0; a--) {
				DO_MINMAX(data, min, max);
				data+= 3;
			}
		}

		dl= dl->next;
	}

	if(totvert==0) {
		return;
	}
	if(totvert>=65000 || tottria>=65000) {
		if (totvert>=65000) {
			error("Too many vertices (%d)", totvert);
		} else {
			error("Too many faces (%d)", tottria);
		}

		return;
	}
	
	if(totcol>16) {
		error("Found more than 16 different colors");
		totcol= 16;
	}

	vec[0]= (min[0]+max[0])/2;
	vec[1]= (min[1]+max[1])/2;
	vec[2]= (min[2]+max[2])/2;

	ob= add_object(OB_MESH);
	VECCOPY(ob->loc, vec);
	where_is_object(ob);

	me= ob->data;
	
	/* colors */
	if(totcol) {
		ob->mat= MEM_callocN(sizeof(void *)*totcol, "ob->mat");
		me->mat= MEM_callocN(sizeof(void *)*totcol, "me->mat");
		me->totcol= totcol;
		ob->totcol= (unsigned char) me->totcol;
		ob->actcol= 1;
	}
	
	/* materials */
	for(a=0; a<totcol; a++) {
		ma= G.main->mat.first;
		while(ma) {
			if(ma->mtex[0]==0) {
				if(ivcolors[a][0]==ma->r && ivcolors[a][1]==ma->g && ivcolors[a][2]==ma->b) {
					me->mat[a]= ma;
					ma->id.us++;
					break;
				}
			}
			ma= ma->id.next;
		}
		if(ma==0) {
			ma= add_material("ext");
			me->mat[a]= ma;
			ma->r= ivcolors[a][0];
			ma->g= ivcolors[a][1];
			ma->b= ivcolors[a][2];
			automatname(ma);
		}
	}
	
	totvlak= totquad+tottria+totedge;

	printf("Import: %d vertices %d faces\n", totvert, totvlak);
	
	if(totvert) me->mvert= MEM_callocN(totvert*sizeof(MVert), "mvert");
	if(totvlak) me->mface= MEM_callocN(totvlak*sizeof(MFace), "mface");
	me->totvert= totvert;
	me->totface= totvlak;
	
	mvert= me->mvert;
	mface= me->mface;

	startve= 0;

	dl= dlfirst;
	while(dl) {
		
		colnr= (dl->col>15 ? 15: dl->col);
		if(colnr) colnr--;
		
		if(dl->type==DL_SURF) {
			data= (float *)(dl+1);

			for(a=dl->parts*dl->nr; a>0; a--) {
				mvert->co[0]= data[0] -vec[0];
				mvert->co[1]= data[1] -vec[1];
				mvert->co[2]= data[2] -vec[2];
				
				data+=3;
				mvert++;
			}

			for(a=0; a<dl->parts; a++) {

				DL_SURFINDEX(dl->flag & 1, dl->flag & 2, dl->nr, dl->parts);
				p1+= startve; 
				p2+= startve; 
				p3+= startve; 
				p4+= startve;

				for(; b<dl->nr; b++) {
				
					mface->v1= p1;
					mface->v2= p2;
					mface->v3= p4;
					mface->v4= p3;
					
					mface->mat_nr= colnr;
					test_index_mface(mface, 4);
					
					mface++;
					
					p4= p3; 
					p3++;
					p2= p1; 
					p1++;
				}
			}
			
			startve += dl->parts*dl->nr;

		}
		else if(dl->type==DL_POLY) {
		
			if(dl->nr==3 || dl->nr==4) {
				data= (float *)(dl+1);

				for(a=dl->parts*dl->nr; a>0; a--) {
					mvert->co[0]= data[0] -vec[0];
					mvert->co[1]= data[1] -vec[1];
					mvert->co[2]= data[2] -vec[2];
					data+=3;
					mvert++;
				}

				for(a=0; a<dl->parts; a++) {
					if(dl->nr==3) {
						mface->v1= startve+a*dl->nr;
						mface->v2= startve+a*dl->nr+1;
						mface->v3= startve+a*dl->nr+2;
						mface->mat_nr= colnr;
						test_index_mface(mface, 3);
						mface++;
					}
					else {
						mface->v1= startve+a*dl->nr;
						mface->v2= startve+a*dl->nr+1;
						mface->v3= startve+a*dl->nr+2;
						mface->v4= startve+a*dl->nr+3;
						mface->mat_nr= colnr;
						test_index_mface(mface, 4);
						mface++;
					}
				}
				startve += dl->parts*dl->nr;
			}
			else if(dl->nr>4) {
				data= (float *)(dl+1);

				for(a=dl->parts*dl->nr; a>0; a--) {
					mvert->co[0]= data[0] -vec[0];
					mvert->co[1]= data[1] -vec[1];
					mvert->co[2]= data[2] -vec[2];
					
					data+=3;
					mvert++;
				}

				for(b=0; b<dl->parts; b++) {
					for(a=0; a<dl->nr; a++) {
						mface->v1= startve+a;
						
						if(a==dl->nr-1) mface->v2= startve;
						else mface->v2= startve+a+1;
						
						mface->mat_nr= colnr;
						test_index_mface(mface, 2);

						mface++;
					}
					startve += dl->nr;
				}
			}
		}
		else if(dl->type==DL_INDEX3) {
			data= dl->verts;
			
			for(a=dl->nr; a>0; a--) {
				mvert->co[0]= data[0] -vec[0];
				mvert->co[1]= data[1] -vec[1];
				mvert->co[2]= data[2] -vec[2];
				data+=3;
				mvert++;
			}

			idata= dl->index;
			for(b=dl->parts; b>0; b--) {
				mface->v1= startve+idata[0];
				mface->v2= startve+idata[1];
				mface->v3= startve+idata[2];
				mface->mat_nr= colnr;
				
				if (mface->v1>me->totvert-1) mface->v1= me->totvert-1;
				if (mface->v2>me->totvert-1) mface->v2= me->totvert-1;
				if (mface->v3>me->totvert-1) mface->v3= me->totvert-1;

				test_index_mface(mface, 3);
				mface++;
				idata+= 3;
			}
			startve += dl->nr;
		}
		else if(dl->type==DL_SEGM) {
			data= (float *)(dl+1);

			for(a=dl->parts*dl->nr; a>0; a--) {
				mvert->co[0]= data[0] -vec[0];
				mvert->co[1]= data[1] -vec[1];
				mvert->co[2]= data[2] -vec[2];
				data+=3;
				mvert++;
			}

			for(b=0; b<dl->parts; b++) {
				for(a=0; a<dl->nr-1; a++) {
					mface->v1= startve+a;
					mface->v2= startve+a+1;
					mface->mat_nr= colnr;
					test_index_mface(mface, 2);
					mface++;
				}
				startve += dl->nr;
			}
		}
		dl= dl->next;
	}

	G.obedit= ob;
	make_editMesh();
	load_editMesh();
	free_editMesh();
	
	G.obedit= 0;
	tex_space_mesh(me);

}

static void displist_to_objects(ListBase *lbase)
{
	DispList *dl, *first, *prev, *next;
	ListBase tempbase;
	int maxaantal, curcol, totvert=0, vert;
	
	/* irst this: is still active */
	if(ivsurf) {
		where_is_object(ivsurf);
		docentre_new();
	}

	dl= lbase->first;
	while(dl) {
		next= dl->next;
		
		/* PATCH 1: polyfill */
		if(dl->type==DL_POLY && dl->nr>4) {
			/* solution: put them together in separate listbase */
			;
		}
		/* PATCH 2: poly's of 2 points */
		if(dl->type==DL_POLY && dl->nr==2) dl->type= DL_SEGM;
		
		dl= next;
	}

	/* count vertices */

	dl= lbase->first;
	while(dl) {

		if(dl->type==DL_SURF) totvert+= dl->nr*dl->parts;
		else if(dl->type==DL_POLY) {
			if(dl->nr==3 || dl->nr==4) totvert+= dl->nr*dl->parts;
			else if(dl->nr>4) totvert+= dl->nr*dl->parts;
		}
		else if(dl->type==DL_INDEX3) totvert+= dl->nr;
		else if(dl->type==DL_SEGM) totvert+= dl->nr*dl->parts;

		dl= dl->next;
	}

	if(totvert==0) {
		
		if(ivsurf==0) error("Found no data");
		if(lbase->first) BLI_freelistN(lbase);
		
		return;
	}

	maxaantal= 32000;
	
	if(totvert>maxaantal) {
	
		/* try to put colors together */
		curcol= 0;
		tempbase.first= tempbase.last= 0;

		while(lbase->first) {
			dl= lbase->first;
			while(dl) {
				next= dl->next;
				if(dl->col==curcol) {
					BLI_remlink(lbase, dl);
					BLI_addtail(&tempbase, dl);
					dl->col= 0;
				}
				
				dl= next;
			}
			
			/* in tempbase are all 'curcol' */
			totvert= 0;
			dl= first= tempbase.first;
			while(dl) {
				vert= 0;
				
				if(dl->type==DL_SURF) vert= dl->nr*dl->parts;
				else if(dl->type==DL_POLY) {
					if(dl->nr==3 || dl->nr==4) vert= dl->nr*dl->parts;
					else if(dl->nr>4) vert= dl->nr*dl->parts;
				}
				else if(dl->type==DL_INDEX3) totvert+= dl->nr;
				else if(dl->type==DL_SEGM) vert= dl->nr*dl->parts;
				
				totvert+= vert;
				if(totvert > maxaantal || dl->next==0) {
					if(dl->next==0) {
						displist_to_mesh(first);
					}
					else if(dl->prev) {
						prev= dl->prev;
						prev->next= 0;
						displist_to_mesh(first);
						prev->next= dl;
						first= dl;
						totvert= 0;
					}
				}
				
				dl= dl->next;
			}
			
			freedisplist(&tempbase);
			
			curcol++;
		}
	}
	else displist_to_mesh(lbase->first);

	freedisplist(lbase);

}

int BKE_read_exotic(char *name)
{
	ListBase lbase={0, 0};
	int file, len;
	char str[32];
	int *s0 = (int*) str;
	int retval = 0;

	// make sure we're not trying to read a directory....

	len= strlen(name);
	if (name[len-1] !='/' && name[len-1] != '\\') {
		file = open(name, O_BINARY|O_RDONLY);

		if (file <= 0) {
			error("Can't open file: %s", name);
		} else {
			read(file, str, 31);
			close(file);

			if ((*s0 != FORM) && (strncmp(str, "BLEN", 4) != 0)) {

				waitcursor(1);
				
				if(*s0==GOUR) {
					if(G.obedit) {
						error("Unable to perform function in EditMode");
					} else {
						read_radiogour(name);
						retval = 1;
					}
				}
				else if ELEM4(*s0, DDG1, DDG2, DDG3, DDG4) {
					if(G.obedit) {
						error("Unable to perform function in EditMode");
					} else {
						read_videoscape(name);
						retval = 1;
					}
				}
				else if(strncmp(str, "#Inventor V1.0", 14)==0) {
					if( strncmp(str+15, "ascii", 5)==0) {
						read_inventor(name, &lbase);
						displist_to_objects(&lbase);				
						retval = 1;
					} else {
						error("Can only read Inventor 1.0 ascii");
					}
				}
				else if((strncmp(str, "#VRML V1.0 asc", 14)==0)) {
					read_inventor(name, &lbase);
					displist_to_objects(&lbase);				
					retval = 1;
				}
				else if(is_dxf(name)) {
					dxf_read(name);
					retval = 1;
				}
				// TODO: this should not be in the kernel...
				else { // unknown format, call Python importloader 
					if (BPY_call_importloader(name)) {
						retval = 1;
					} else {	
						error("Unknown file type or error, check console");
					}	
				
				}	
				waitcursor(0);
			}
		}
	}

	if (retval == 1) {
		strcpy(G.sce, name);
	}

	return (retval);
}


/* ************************ WRITE ************************** */


char videosc_dir[160]= {0, 0};


static void write_videoscape_mesh(Object *ob, char *str)
{
	Mesh *me;
	Material *ma;
	MVert *mvert;
	MFace *mface;
	FILE *fp;
	EditVert *eve;
	EditVlak *evl;
	unsigned int kleur[32];
	float co[3];
	int a;
	long tot;
	char *cp;
	
	if(ob && ob->type==OB_MESH);
	else {
		return;
	}

	kleur[0]= 0x00C0C0C0;
	if(G.order==L_ENDIAN) SWITCH_INT(kleur[0]);

	cp= (char *)kleur;
	for(a=0; a<ob->totcol; a++, cp+=4) {
		
		ma= give_current_material(ob, a+1);
		if(ma) {
			cp[0]= (unsigned char) (255.0*ma->emit);
			cp[1]= (unsigned char) (255.0*ma->b);
			cp[2]= (unsigned char) (255.0*ma->g);
			cp[3]= (unsigned char) (255.0*ma->r);
		}
		else kleur[a]= 0x00C0C0C0;
	
		if(G.order==L_ENDIAN) SWITCH_INT(kleur[a]);
		
		if(a>30) break;
	}
	
	fp= fopen(str, "wb");
	if(fp==NULL) return;

	fprintf(fp,"3DG1\n");

	if(G.obedit) {

		fprintf(fp, "%d\n", G.totvert);
	
		tot= 0;
		eve= G.edve.first;
		while(eve) {
			VECCOPY(co, eve->co);
			Mat4MulVecfl(ob->obmat, co);
			fprintf(fp, "%f %f %f\n", co[0], co[1], co[2] );
			eve->vn= (struct EditVert *)tot;
			tot++;
			eve= eve->next;
		}
		evl= G.edvl.first;
		while(evl) {

			if(evl->v4==0) {
				fprintf(fp, "3 %p %p %p 0x%x\n", evl->v1->vn, evl->v2->vn, evl->v3->vn, kleur[evl->mat_nr]);
			}
			else {
				fprintf(fp, "4 %p %p %p %p 0x%x\n", evl->v1->vn, evl->v2->vn, evl->v3->vn, evl->v4->vn, kleur[evl->mat_nr]);
			}
			evl= evl->next;
		}
	}
	else {
		DispList *dl;
		float *extverts=0;
		
		dl= find_displist(&ob->disp, DL_VERTS);
		if(dl) extverts= dl->verts;
		
		me= ob->data;
		
		fprintf(fp, "%d\n", me->totvert);
		
		mvert= me->mvert;
		mface= me->mface;
		for(a=0; a<me->totvert; a++, mvert++) {
			if(extverts) {
				VECCOPY(co, extverts);
				extverts+= 3;
			}
			else {
				VECCOPY(co, mvert->co);
			}
			Mat4MulVecfl(ob->obmat, co);
			fprintf(fp, "%f %f %f\n", co[0], co[1], co[2] );
		}
		for(a=0; a<me->totface; a++, mface++) {
			if(mface->v3==0) {
				fprintf(fp, "2 %d %d 0x%x\n", mface->v1, mface->v2, kleur[mface->mat_nr]);
			}
			else if(mface->v4==0) {
				fprintf(fp, "3 %d %d %d 0x%x\n", mface->v1, mface->v2, mface->v3, kleur[mface->mat_nr]);
			}
			else {
				fprintf(fp, "4 %d %d %d %d 0x%x\n", mface->v1, mface->v2, mface->v3, mface->v4, kleur[mface->mat_nr]);
			}
		}
	}
	
	fclose(fp);
	
}


void write_videoscape(char *str)
{
	Base *base;
	int file, val, lampdone=0;
	unsigned short numlen;
	char head[FILE_MAXFILE], tail[FILE_MAXFILE];
	
	if(BLI_testextensie(str,".trace")) str[ strlen(str)-6]= 0;
	if(BLI_testextensie(str,".blend")) str[ strlen(str)-6]= 0;
	if(BLI_testextensie(str,".ble")) str[ strlen(str)-4]= 0;
	if(BLI_testextensie(str,".obj")==0) strcat(str, ".obj");

	file= open(str,O_BINARY|O_RDONLY);
	close(file);
	if(file>-1) if(saveover(str)==0) return;

	strcpy(videosc_dir, str);

	base= G.scene->base.first;
	while(base) {
		if((base->flag & SELECT) && (base->lay & G.scene->lay))  {
			if(base->object->type==OB_MESH) {
				write_videoscape_mesh(base->object, str);
				val = BLI_stringdec(str, head, tail, &numlen);
				BLI_stringenc(str, head, tail, numlen, val + 1);
			}
			else if(base->object->type==OB_CURVE || base->object->type==OB_SURF) {
				/* write_videoscape_nurbs(base->object, str); */
				/* val = stringdec(str, head, tail, &numlen); */
				/* stringenc(str, head, tail, numlen, val + 1); */
			}
			else if(lampdone==0 && base->object->type==OB_LAMP) {
				/* lampdone= 1; */
				/* write_videoscape_lamps(str); */
				/* val = stringdec(str, head, tail, &numlen); */
				/* stringenc(str, head, tail, numlen, val + 1); */
			}
		}
		base= base->next;
	}
	
	
	/* remove when higher numbers exist */
	while(remove(str)==0) {
		
		val = BLI_stringdec(str, head, tail, &numlen);
		BLI_stringenc(str, head, tail, numlen, val + 1);
	}
}

/* ******************************* WRITE VRML ***************************** */

static void replace_chars(char *str1, char *str2)
{
	int a= strlen(str2);
	
	str1[a]= 0;
	while(a--) {
		if(str2[a]=='.' || str2[a]==' ') str1[a]= '_';
		else str1[a]= str2[a];
	}
}


static void write_material_vrml(FILE *fp, Material *ma)
{
	char str[32];
	
	replace_chars(str, ma->id.name+2);
	
	fprintf(fp, "\tDEF %s\n", str);
	fprintf(fp, "\tMaterial {\n");
	
	fprintf(fp, "\t\tdiffuseColor %f %f %f\n", ma->r, ma->g, ma->b);
	fprintf(fp, "\t\tspecularColor %f %f %f\n", ma->specr, ma->specg, ma->specb);
	fprintf(fp, "\t\tshininess %f \n", ((float)ma->har)/100.0);
	fprintf(fp, "\t\ttransparency %f \n", 1.0-ma->alpha);
	
	fprintf(fp, "\t}\n");
	
}

unsigned int *mcol_to_vcol(Mesh *me)
{
	MFace *mface;
	unsigned int *mcol, *mcoln, *mcolmain;
	int a;

	if(me->totface==0 || me->mcol==0) return 0;
	
	mcoln= mcolmain= MEM_mallocN(sizeof(int)*me->totvert, "mcoln");
	mcol = (unsigned int *)me->mcol;
	mface= me->mface;
	
	for(a=me->totface; a>0; a--, mface++) {
		mcoln[mface->v1]= mcol[0];
		mcoln[mface->v2]= mcol[1];
		if(mface->v3) mcoln[mface->v3]= mcol[2];
		if(mface->v4) mcoln[mface->v4]= mcol[3];

		mcol+= 4;
	}
	
	return mcolmain;
}

void mcol_to_rgba(unsigned int col, float *r, float *g, float *b, float *a)
{
	char *cp;
	
	cp = (char *)&col;
	
	*r= cp[3];
	*r /= 255.0;

	*g= cp[2];
	*g /= 255.0;

	*b= cp[1];
	*b /= 255.0;

	*a= cp[0];
	*a /= 255.0;
}

static void write_mesh_vrml(FILE *fp, Mesh *me)
{
	Material *ma;
	MVert *mvert;
	MFace *mface;
	TFace *tface;
	Image *ima;
	int a, b, totcol, texind;
	char str[32];
	
	replace_chars(str, me->id.name+2);

	fprintf(fp, "\tDEF %s\n", str);
	fprintf(fp, "\tSeparator {\n");
	
	if(me->tface) {
		ima= ((TFace *)me->tface)->tpage;
		if(ima) {
			fprintf(fp, "\t\tTexture2 {\n");
			fprintf(fp, "\t\t\tfilename %s\n", ima->name);
			fprintf(fp, "\t\t\twrapS REPEAT \n");
			fprintf(fp, "\t\t\twrapT REPEAT \n");
			fprintf(fp, "\t\t}\n");
		}
		tface_to_mcol(me);
	}
	
	if(me->mcol) {
		unsigned int *mcol, *mcolmain;
		float r, g, b, cola;
		
		fprintf(fp, "\t\tMaterial {\n");
		fprintf(fp, "\t\t\tdiffuseColor [\n");
		
		a= me->totvert;
		mcol= mcolmain= mcol_to_vcol(me);
		if(mcol) {
			while(a--) {
				mcol_to_rgba(*mcol, &r, &g, &b, &cola);
				fprintf(fp, "\t\t\t\t %f %f %f,\n", r, g, b);
				mcol++;
			}
			MEM_freeN(mcolmain);
		}
		fprintf(fp, "\t\t\t]\n");
		fprintf(fp, "\t\t}\n");

		fprintf(fp, "\t\tMaterialBinding { value PER_VERTEX_INDEXED }\n");
	}


	fprintf(fp, "\t\tCoordinate3 {\n");
	fprintf(fp, "\t\t\tpoint [\n");
	
	a= me->totvert;
	mvert= me->mvert;
	while(a--) {
		fprintf(fp, "\t\t\t\t %f %f %f,\n", mvert->co[0], mvert->co[1], mvert->co[2]);
		mvert++;
	}
	fprintf(fp, "\t\t\t]\n");
	fprintf(fp, "\t\t}\n");
	
	
	totcol= me->totcol;
	if(totcol==0) totcol= 1;
	texind= 0; // index for uv coords
	
	for(b=0; b<totcol; b++) {
		
		if(me->mcol==0) {
			if(me->mat) {
				ma= me->mat[b];
				if(ma) {
					replace_chars(str, ma->id.name+2);

					fprintf(fp, "\t\tUSE %s\n\n", str);
				}
			}
		}
		
		if(me->tface) {
			fprintf(fp, "\t\tTextureCoordinate2 {\n");
			fprintf(fp, "\t\t\tpoint [\n");
	
			a= me->totface;
			mface= me->mface;
			tface= me->tface;
			while(a--) {
				if(mface->mat_nr==b) {
					fprintf(fp, "\t\t\t\t %f %f,\n", tface->uv[0][0], tface->uv[0][1]); 
					fprintf(fp, "\t\t\t\t %f %f,\n", tface->uv[3][0], tface->uv[3][1]); 
					if(mface->v3) fprintf(fp, "\t\t\t\t %f %f,\n", tface->uv[2][0], tface->uv[2][1]); 
					if(mface->v4) fprintf(fp, "\t\t\t\t %f %f,\n", tface->uv[1][0], tface->uv[1][1]); 
				}
				mface++;
				tface++;
			}
			fprintf(fp, "\t\t\t]\n");
			fprintf(fp, "\t\t}\n");
		}

		fprintf(fp, "\t\tIndexedFaceSet {\n");
		fprintf(fp, "\t\t\tcoordIndex [\n");

		a= me->totface;
		mface= me->mface;
		while(a--) {
			if(mface->mat_nr==b) {
				if(mface->v4) fprintf(fp, "\t\t\t\t %d, %d, %d, %d, -1,\n", mface->v1, mface->v2, mface->v3, mface->v4); 
				else if(mface->v3) fprintf(fp, "\t\t\t\t %d, %d, %d, -1,\n", mface->v1, mface->v2, mface->v3); 
			}
			mface++;
		}
		fprintf(fp, "\t\t\t]\n");

		if(me->tface) {
			fprintf(fp, "\t\t\ttextureCoordIndex [\n");
	
			a= me->totface;
			mface= me->mface;
			while(a--) {
				if(mface->mat_nr==b) {
					if(mface->v4) {
						fprintf(fp, "\t\t\t\t %d, %d, %d, %d, -1,\n", texind, texind+1, texind+2, texind+3); 
						texind+= 4;
					}
					else if(mface->v3) {
						fprintf(fp, "\t\t\t\t %d, %d, %d, -1,\n", texind, texind+1, texind+2); 
						texind+= 3;
					}
				}
				mface++;
			}
			fprintf(fp, "\t\t\t]\n");
		}
		fprintf(fp, "\t\t}\n");
	}
	
	fprintf(fp, "\t}\n");
	
	if(me->tface) {
		MEM_freeN(me->mcol);
		me->mcol= 0;
	}

}

static void write_camera_vrml(FILE *fp, Object *ob)
{
	Camera *cam;
	
	if(ob==0) return;
	Mat4Invert(ob->imat, ob->obmat);

	fprintf(fp, "\tMatrixTransform {\n");

	fprintf(fp, "\tmatrix \n");

	fprintf(fp, "\t\t%f %f %f %f\n", ob->imat[0][0], ob->imat[0][1], ob->imat[0][2], ob->imat[0][3]);
	fprintf(fp, "\t\t%f %f %f %f\n", ob->imat[1][0], ob->imat[1][1], ob->imat[1][2], ob->imat[1][3]);
	fprintf(fp, "\t\t%f %f %f %f\n", ob->imat[2][0], ob->imat[2][1], ob->imat[2][2], ob->imat[2][3]);
	fprintf(fp, "\t\t%f %f %f %f\n", ob->imat[3][0], ob->imat[3][1], ob->imat[3][2], ob->imat[3][3]);

	fprintf(fp, "\t}\n");

	cam= ob->data;

	fprintf(fp, "\tPerspectiveCamera {\n");
	fprintf(fp, "\t\tfocalDistance %f\n", cam->lens/10.0);
	
	fprintf(fp, "\t}\n");

}

static void write_object_vrml(FILE *fp, Object *ob)
{
	ID *id;
	char str[32];
	
	fprintf(fp, "\tSeparator {\n");
	fprintf(fp, "\t\tMatrixTransform {\n");

	fprintf(fp, "\t\tmatrix \n");

	fprintf(fp, "\t\t\t%f %f %f %f\n", ob->obmat[0][0], ob->obmat[0][1], ob->obmat[0][2], ob->obmat[0][3]);
	fprintf(fp, "\t\t\t%f %f %f %f\n", ob->obmat[1][0], ob->obmat[1][1], ob->obmat[1][2], ob->obmat[1][3]);
	fprintf(fp, "\t\t\t%f %f %f %f\n", ob->obmat[2][0], ob->obmat[2][1], ob->obmat[2][2], ob->obmat[2][3]);
	fprintf(fp, "\t\t\t%f %f %f %f\n", ob->obmat[3][0], ob->obmat[3][1], ob->obmat[3][2], ob->obmat[3][3]);

	fprintf(fp, "\t\t}\n");

	id= ob->data;

	replace_chars(str, id->name+2);

	fprintf(fp, "\t\tUSE %s\n", str);
	fprintf(fp, "\t}\n");
}


void write_vrml(char *str)
{
	Mesh *me;
	Material *ma;
	Base *base;
	FILE *fp;
	
	if(BLI_testextensie(str,".blend")) str[ strlen(str)-6]= 0;
	if(BLI_testextensie(str,".ble")) str[ strlen(str)-4]= 0;
	if(BLI_testextensie(str,".wrl")==0) strcat(str, ".wrl");

	if(saveover(str)==0) return;
	
	fp= fopen(str, "w");
	
	if(fp==NULL) {
		error("Can't write file");
		return;
	}
	strcpy(videosc_dir, str);

	waitcursor(1);
	
	/* FIRST: write all the datablocks */
	
	fprintf(fp, "#VRML V1.0 ascii\n\n# Blender V2.0\n\n# 'Switch' is used as a hack, to ensure it is not part of the drawing\n\n");
	fprintf(fp, "Separator {\n");
	fprintf(fp, "Switch {\n");

	ma= G.main->mat.first;
	while(ma) {
		if(ma->id.us) {
			write_material_vrml(fp, ma);
		}
		ma= ma->id.next;
	}

	me= G.main->mesh.first;
	while(me) {
		if(me->id.us) {
			write_mesh_vrml(fp, me);
		}
		me= me->id.next;
	}
	
	/* THEN:Hidden Objects */
	fprintf(fp, "\n\t# Hidden Objects, in invisible layers\n\n");
	base= G.scene->base.first;
	while(base) {
		if(base->object->type== OB_MESH) {
			if( (base->lay & G.scene->lay)==0 ) {
				write_object_vrml(fp, base->object);
			}
		}
		base= base->next;
	}

	fprintf(fp, "}\n");
	fprintf(fp, "\n# Visible Objects\n\n");
	fprintf(fp, "Separator {\n");
	
	/* The camera */

	write_camera_vrml(fp, G.scene->camera);
	
	/* THEN:The Objects */
	
	base= G.scene->base.first;
	while(base) {
		if(base->object->type== OB_MESH) {
			if(base->lay & G.scene->lay) {
				write_object_vrml(fp, base->object);
			}
		}
		base= base->next;
	}
	
	fprintf(fp, "}\n");
	fprintf(fp, "}\n");
	
	fclose(fp);
	
	waitcursor(0);
}


/* ******************************* WRITE DXF ***************************** */

#define write_group(id,data) fprintf(fp, "%d\n%s\n", id, data)

/* A completely wacky function to try and make good
indexed (AutoCAD index) values out of straight rgb 
ones... crazy */

static int rgb_to_dxf_col (float rf, float gf, float bf) 
{
	int r= (int) (rf*255.0f);
	int g= (int) (gf*255.0f);
	int b= (int) (bf*255.0f);
	float h,s,v;
	int ret;
	
	/* Grayscale value */
	if (((int)r/10)==((int)g/10) && ((int)g/10)==((int)b/10)) ret= 250+((int)r/51);
	/* A nice chroma value */
	else {
		rgb_to_hsv (rf,gf,bf,&h,&s,&v);
		
		ret= (int) (10.0f + (h*239.0f));
		CLAMP(ret,10,249);
		
		/* If its whitish make the index odd */
		if (s<.5 || v>.5) if(ret%2) ret++;
	}
	
	return ret;
}

/* And its completely wacky complement */

static void dxf_col_to_rgb (int cid, float *rf, float *gf, float *bf)
{
	float h, s, v;
	
	/* Grayscale values */
	if (cid>=250 && cid <= 255) {
		*rf= *gf= *bf= (float) ((cid-250)*51)/255;
		CLAMP(*rf, 0.0, 1.0);
		CLAMP(*gf, 0.0, 1.0);
		CLAMP(*bf, 0.0, 1.0);
		
	/* Pure values */
	} else if (cid<10) {
		switch (cid) {
		case 1:
			*rf=1.0;
			*gf=0.0;
			*bf=0.0;
			break;
		case 2:
			*rf=1.0;
			*gf=1.0;
			*bf=0.0;
			break;
		case 3:
			*gf=1.0;
			*rf=0.0;
			*bf=0.0;
			break;
		case 4:
			*rf=0.0;
			*gf=1.0;
			*bf=1.0;
			break;
		case 5:
			*rf=0.0;
			*gf=0.0;
			*bf=1.0;
			break;
		case 6:
			*rf=1.0;
			*gf=0.0;
			*bf=1.0;
			break;
		case 7:
		default:
			*rf= *gf= *bf= 1.0;
			break;
		}
	} else {
		/* Get chroma values */
			
		h= (float) (cid-10)/239;
		CLAMP(h, 0.0, 1.0);
		
		/* If its odd make it a bit whitish */
		if (cid%2) { s=.75; v= 0.25; 
		} else {  s= 0.25; v= 0.75;}
		
		hsv_to_rgb (h, s, v, rf, gf, bf);
	}
}

static void write_mesh_dxf(FILE *fp, Mesh *me)
{
	Material *ma;
	MVert *mvert;
	MFace *mface;
	int a;
	char str[32];
	
	replace_chars(str, me->id.name+2);

	write_group(0, "BLOCK");
	
	write_group(2, str); /* The name */
		
	write_group(8, "Meshes"); /* DXF Layer */
	write_group(70, "64"); /* DXF block flags */
	
	write_group(10, "0.0"); /* X of base */
	write_group(20, "0.0"); /* Y of base */
	write_group(30, "0.0"); /* Z of base */

	write_group(3, str); /* The name (again) */
	
	write_group(0, "POLYLINE"); /* Start the mesh */
	write_group(66, "1"); /* Vertices follow flag */
	write_group(8,"Meshes"); /* DXF Layer */

	if (me->totcol) {
		ma= me->mat[0];
		if(ma) {
			sprintf(str,"%d",rgb_to_dxf_col(ma->r,ma->g,ma->b));
			write_group(62, str); /* Color index */
		}
	}

	write_group(70, "64"); /* Polymesh mesh flag */
	
	fprintf(fp, "71\n%d\n", me->totvert); /* Total vertices */
	fprintf(fp, "72\n%d\n", me->totface); /* Total faces */
	
	/* Write the vertices */
	a= me->totvert;
	mvert= me->mvert;
	while(a--) {
		write_group(0, "VERTEX"); /* Start a new vertex */
		write_group(8, "Meshes"); /* DXF Layer */
		fprintf (fp, "10\n%f\n", mvert->co[0]); /* X cord */
		fprintf (fp, "20\n%f\n", mvert->co[1]); /* Y cord */
		fprintf (fp, "30\n%f\n", mvert->co[2]); /* Z cord */
		write_group(70, "192"); /* Polymesh vertex flag */
				
		mvert++;
	}

	/* Write the face entries */
	a= me->totface;
	mface= me->mface;
	while(a--) {
		if (mface->v4 || mface->v3) {
			write_group(0, "VERTEX"); /* Start a new face */
			write_group(8, "Meshes");
		
			/* Write a face color */
			if (me->totcol) {
				ma= me->mat[mface->mat_nr];
				if(ma) {
					sprintf(str,"%d",rgb_to_dxf_col(ma->r,ma->g,ma->b));
					write_group(62, str); /* Color index */
				}
			}
			else write_group(62, "254"); /* Color Index */

			/* Not sure what this really corresponds too */
			write_group(10, "0.0"); /* X of base */
			write_group(20, "0.0"); /* Y of base */
			write_group(30, "0.0"); /* Z of base */
		
			write_group(70, "128"); /* Polymesh face flag */
		
			if(mface->v4) {
				fprintf (fp, "71\n%d\n", mface->v1+1);
				fprintf (fp, "72\n%d\n", mface->v2+1);
				fprintf (fp, "73\n%d\n", mface->v3+1);
				fprintf (fp, "74\n%d\n", mface->v4+1);
			} else if(mface->v3) {
				fprintf (fp, "71\n%d\n", mface->v1+1);
				fprintf (fp, "72\n%d\n", mface->v2+1);
				fprintf (fp, "73\n%d\n", mface->v3+1);
			}
		}
		mface++;
	}

	write_group(0, "SEQEND");	
	
	write_group(0, "ENDBLK");
}

static void write_object_dxf(FILE *fp, Object *ob, int layer)
{
	ID *id;
	char str[32];

	id= ob->data;

	write_group(0, "INSERT"); /* Start an insert group */
	
	sprintf(str, "%d", layer);
	write_group(8, str);

	replace_chars(str, id->name+2);
	write_group(2, str);

	fprintf (fp, "10\n%f\n", ob->loc[0]); /* X of base */
	fprintf (fp, "20\n%f\n", ob->loc[1]); /* Y of base */
	fprintf (fp, "30\n%f\n", ob->loc[2]); /* Z of base */
	
	fprintf (fp, "41\n%f\n", ob->size[0]); /* X scale */
	fprintf (fp, "42\n%f\n", ob->size[1]); /* Y scale */
	fprintf (fp, "43\n%f\n", ob->size[2]); /* Z scale */
	
	fprintf (fp, "50\n%f\n", (float) ob->rot[2]*180/M_PI); /* Can only write the Z rot */
}

void write_dxf(char *str)
{
	Mesh *me;
	Base *base;
	FILE *fp;
	
	if(BLI_testextensie(str,".blend")) str[ strlen(str)-6]= 0;
	if(BLI_testextensie(str,".ble")) str[ strlen(str)-4]= 0;
	if(BLI_testextensie(str,".dxf")==0) strcat(str, ".dxf");

	if (BLI_exists(str))
		if(saveover(str)==0)
			return;
	
	fp= fopen(str, "w");
	
	if(fp==NULL) {
		error("Can't write file");
		return;
	}
	strcpy(videosc_dir, str);
	
	waitcursor(1);
	
	/* The header part of the DXF */
	
	write_group(0, "SECTION");
    write_group(2, "HEADER");
	write_group(0, "ENDSEC");

	/* The blocks part of the DXF */
	
	write_group(0, "SECTION");
    write_group(2, "BLOCKS");

	/* Write all the meshes */
	me= G.main->mesh.first;
	while(me) {
		if(me->id.us) {
			write_mesh_dxf(fp, me);
		}
		me= me->id.next;
	}

	write_group(0, "ENDSEC");

	/* The entities part of the DXF */
	
	write_group(0, "SECTION");
    write_group(2, "ENTITIES");

	/* Write all the mesh objects */
	base= G.scene->base.first;
	while(base) {
		if(base->object->type== OB_MESH) {
			write_object_dxf(fp, base->object, base->lay);
		}
		base= base->next;
	}

	write_group(0, "ENDSEC");
	
	/* Thats all */
	
	write_group(0, "EOF");
	fclose(fp);
	
	waitcursor(0);
}


static int dxf_line;
static FILE *dxf_fp;

/* exotic.c(2863) : note C6311: c:/Program Files/Microsoft Visual
 * Studio/VC98/include\ctype.h(268) : see previous definition of
 * 'iswspace' */
#define ton_iswspace(c) (c==' '||c=='\n'||c=='\t')

static void clean_wspace (char *str) 
{
	char *from, *to;
	char t;
	
	from= str;
	to=str;
	
	while (*from!=0) {
		t= *from;
		*to= t;
		
		if(!ton_iswspace(*from)) to++;
		from++;
	}
	*to=0;
}

static int all_wspace(char *str)
{
	while(*str != 0) {
		if (!ton_iswspace(*str)) return 0;
		str++;
	}

	return 1;
}

static int all_digits(char *str)
{
	while(*str != 0) {
		if (!isdigit(*str)) return 0;
		str++;
	}

	return 1;
}

static int dxf_get_layer_col(char *layer) 
{
	return 1;
}

static int dxf_get_layer_num(char *layer)
{
	int ret = 1;
	
	if (all_digits(layer) && atoi(layer)<(1<<20)) ret= atoi(layer);
	if (ret == 0) ret = 1;

	return ret;
}

static void dos_clean(char *str)
{
	while (*str) {
		if (*str == 0x0d) {
			*str='\n';
			*(++str)= 0;
			break;
		}
		str++;
	}	
}

static int read_groupf(char *str) 
{
	short c;
	int ret=-1;
	char tmp[256];
	
	strcpy(str, " ");

	while ((c=getc(dxf_fp)) && ton_iswspace(c));
	ungetc(c, dxf_fp);
	if (c==EOF) return -1;
	
	fgets(tmp, 255, dxf_fp);

	dos_clean(tmp);

	if(sscanf(tmp, "%d\n", &ret)!=1) return -2;
		
	fgets(tmp, 255, dxf_fp);

	dos_clean(tmp);

	if (!all_wspace(tmp)) {
		if (sscanf(tmp, "%s\n", str)!=1) return -2;
	}
	
	clean_wspace(str);
	dxf_line+=2;
	
	return ret;
}

#define id_test(id) if(id<0) {char errmsg[128];fclose(dxf_fp); if(id==-1) sprintf(errmsg, "Error inputting dxf, near line %d", dxf_line); else if(id==-2) sprintf(errmsg, "Error reading dxf, near line %d", dxf_line);error(errmsg); return;}

#define read_group(id,str) {id= read_groupf(str); id_test(id);}

#define group_is(idtst,str) (id==idtst&&strcmp(val,str)==0)
#define group_isnt(idtst,str) (id!=idtst||strcmp(val,str)!=0)
#define id_check(idtst,str) if(group_isnt(idtst,str)) { fclose(dxf_fp); error("Error parsing dxf, near line %d", dxf_line); return;}

static int id;
static char val[256];

static short error_exit=0;
static short hasbumped=0;

static int is_dxf(char *str)
{	
	dxf_line=0;
	
	dxf_fp= fopen(str, "r");
	if (dxf_fp==NULL) return 0;

	id= read_groupf(val);
	if ((id==0 && strcmp(val, "SECTION")==0)||id==999) return 1;
	
	fclose(dxf_fp);
	
	return 0;
}

/* NOTES ON THE READER */ 
/*
	--
	It turns out that most DXF writers like (LOVE) to
	write meshes as a long string of 3DFACE entities.
	This means the natural way to read a DXF file
	(every entity corresponds to an object) is completely
	unusable, reading in 10,000 faces each as an
	object just doesn't cut it. Thus the 3DFACE
	entry reader holds state, and only finalizes to
	an object when a) the layer name changes, b) the
	entry type changes, c) we are done reading.

	PS... I decided to do the same thing with LINES, 
	apparently the same thing happens sometimes as
	well.

	PPS... I decided to do the same thing with everything.
	Now it is all really nasty and should be rewritten. 
*/

static void dxf_add_mat (Object *ob, Mesh *me, float color[3], char *layer) 
{
	Material *ma;
	
	if (!me) return;
	
	if(ob) ob->mat= MEM_callocN(sizeof(void *)*1, "ob->mat");
	if(ob) ob->totcol= 1;
	if(ob) ob->actcol= 1;

	me->totcol= 1;
	me->mat= MEM_callocN(sizeof(void *)*1, "me->mat");
	
	if (color[0]<0) {
		if (strlen(layer)) dxf_col_to_rgb(dxf_get_layer_col(layer), &color[0], &color[1], &color[2]);
		color[0]= color[1]= color[2]= 0.8f;
	}												
						
	ma= G.main->mat.first;
	while(ma) {
		if(ma->mtex[0]==0) {
			if(color[0]==ma->r && color[1]==ma->g && color[2]==ma->b) {
				me->mat[0]= ma;
				ma->id.us++;
				break;
			}
		}
		ma= ma->id.next;
	}
	if(ma==0) {
		ma= add_material("ext");
		me->mat[0]= ma;
		ma->r= color[0];
		ma->g= color[1];
		ma->b= color[2];
		automatname(ma);
	}
}

	/* General DXF vars */
static float cent[3]={0.0, 0.0, 0.0};
static char layname[32]="";
static char entname[32]="";
static float color[3]={-1.0, -1.0, -1.0};
static float *vcenter;
static float zerovec[3]= {0.0, 0.0, 0.0};

#define reset_vars cent[0]= cent[1]= cent[2]=0.0; strcpy(layname, ""); color[0]= color[1]= color[2]= -1.0

static void dxf_read_point(int noob) {	
	/* Blender vars */
	Object *ob;
	Mesh *me;
	MVert *mvert;
	
	reset_vars;

	read_group(id, val);								
	while(id!=0) {
		if (id==8) {
			BLI_strncpy(layname, val, sizeof(layname));
		} else if (id==10) {
			cent[0]= (float) atof(val);
		} else if (id==20) {
			cent[1]= (float) atof(val);
		} else if (id==30) {
			cent[2]= (float) atof(val);
		} else if (id==60) {
			/* short invisible= atoi(val); */
		} else if (id==62) {
			int colorid= atoi(val);
							
			CLAMP(colorid, 1, 255);
			dxf_col_to_rgb(colorid, &color[0], &color[1], &color[2]);
		}
		read_group(id, val);								
	}

	if (noob) {
		ob= NULL;
		me= add_mesh(); G.totmesh++;
		((ID *)me)->us=0;

		if (strlen(entname)) new_id(&G.main->mesh, (ID *)me, entname);
		else if (strlen(layname)) new_id(&G.main->mesh, (ID *)me, layname);

		vcenter= zerovec;
	} else {
		ob= add_object(OB_MESH);
		if (strlen(entname)) new_id(&G.main->object, (ID *)ob, entname);
		else if (strlen(layname)) new_id(&G.main->object, (ID *)ob, layname);
	
		if (strlen(layname)) ob->lay= dxf_get_layer_num(layname);
		else ob->lay= G.scene->lay;

		me= ob->data;
		
		vcenter= ob->loc;
	}
	me->totvert= 1; /* Its a line dude */
	me->totface= 0;
											
	me->mvert= MEM_callocN(me->totvert*sizeof(MVert), "mverts");
	me->mface= NULL;
					
	dxf_add_mat (ob, me, color, layname);					

	mvert= me->mvert;
	mvert->co[0]= mvert->co[1]= mvert->co[2]= 0;
		
	if (ob) VECCOPY(ob->loc, cent);
	
	if (!noob) {
		G.obedit= ob;
		make_editMesh();
		load_editMesh();
		free_editMesh();
		waitcursor(1);		/* patch yah... */

		G.obedit= 0;
	}
	tex_space_mesh(me);
	
	hasbumped=1;
}

	/* Line state vars */
static Object *linehold=NULL;
static Mesh *linemhold=NULL;

static char oldllay[32];
static short lwasline=0; /* last was face 3d? */

static void dxf_close_line(void)
{
	linemhold=NULL;
	if (linehold==NULL) return;
	
	G.obedit= linehold;
	make_editMesh();
	load_editMesh();
	free_editMesh();
	waitcursor(1);		/* patch yah... */

	G.obedit= 0;
	tex_space_mesh(linehold->data);

	linehold=NULL;
}

static void dxf_read_line(int noob) {	
	/* Entity specific vars */
	float epoint[3]={0.0, 0.0, 0.0};
	short vspace=0; /* Whether or not coords are relative */
	
	/* Blender vars */
	Object *ob;
	Mesh *me;
	MVert *mvert, *vtmp;
	MFace *mface, *ftmp;
	
	reset_vars;

	read_group(id, val);								
	while(id!=0) {
		if (id==8) {
			BLI_strncpy(layname, val, sizeof(layname));
		} else if (id==10) {
			cent[0]= (float) atof(val);
		} else if (id==20) {
			cent[1]= (float) atof(val);
		} else if (id==30) {
			cent[2]= (float) atof(val);
		} else if (id==11) {
			epoint[0]= (float) atof(val);
		} else if (id==21) {
			epoint[1]= (float) atof(val);
		} else if (id==31) {
			epoint[2]= (float) atof(val);
		} else if (id==60) {
			/* short invisible= atoi(val); */
		} else if (id==62) {
			int colorid= atoi(val);
							
			CLAMP(colorid, 1, 255);
			dxf_col_to_rgb(colorid, &color[0], &color[1], &color[2]);
		} else if (id==67) {
			vspace= atoi(val);
		}
		read_group(id, val);								
	}

	/* Check to see if we need to make a new object */

	if(!lwasline || strcmp(layname, oldllay)!=0) dxf_close_line();
	if(linemhold != NULL && linemhold->totvert>65000) dxf_close_line();
					
	if (linemhold==NULL) {
		if (noob) {
			ob= NULL;
			me= add_mesh(); G.totmesh++;
			((ID *)me)->us=0;
		
			if (strlen(entname)) new_id(&G.main->mesh, (ID *)me, entname);
			else if (strlen(layname)) new_id(&G.main->mesh, (ID *)me, layname);
		
			vcenter= zerovec;
		} else {
			ob= add_object(OB_MESH);
			if (strlen(entname)) new_id(&G.main->object, (ID *)ob, entname);
			else if (strlen(layname)) new_id(&G.main->object, (ID *)ob, layname);
		
			if (strlen(layname)) ob->lay= dxf_get_layer_num(layname);
			else ob->lay= G.scene->lay;
		
			me= ob->data;
			
			vcenter= ob->loc;
		}
		
		me->totvert=0;
		me->totface=0;
		me->mvert=NULL;
		me->mface=NULL;

		strcpy(oldllay, layname);		

		if(ob) VECCOPY(ob->loc, cent);

		dxf_add_mat (ob, me, color, layname);

		linehold= ob;
		linemhold= me;
	} else {
		ob= linehold;
		me= linemhold;
	}

	me->totvert+= 2;
	me->totface++;
	
	vtmp= MEM_callocN(me->totvert*sizeof(MVert), "mverts");
	ftmp= MEM_callocN(me->totface*sizeof(MFace), "mface");

	if(me->mvert) {
		memcpy(vtmp, me->mvert, (me->totvert-2)*sizeof(MVert));
		MEM_freeN(me->mvert);
	}
	me->mvert= vtmp;
	vtmp=NULL;

	if(me->mface) {
		memcpy(ftmp, me->mface, (me->totface-1)*sizeof(MFace));
		MEM_freeN(me->mface);
	}
	me->mface= ftmp;
	ftmp=NULL;
	
	mvert= &me->mvert[(me->totvert-2)];
	VecSubf(mvert->co, cent, vcenter);
	
	mvert++;
	if (vspace) { VECCOPY(mvert->co, epoint);
	} else VecSubf(mvert->co, epoint, vcenter);
		
	mface= &(((MFace*)me->mface)[me->totface-1]);
	mface->v1= me->totvert-2;
	mface->v2= me->totvert-1;
		
	mface->edcode= 1;
	mface->mat_nr= 0;
	
	hasbumped=1;
}

        /* 2D Polyline state vars */
static Object *p2dhold=NULL;
static Mesh *p2dmhold=NULL;
static char oldplay[32];
static short lwasp2d=0;

static void dxf_close_2dpoly(void)
{
        p2dmhold= NULL;
        if (p2dhold==NULL) return;
        
        G.obedit= p2dhold;
        make_editMesh();
        load_editMesh();
		free_editMesh();
		waitcursor(1);		/* patch yah... */
		
        G.obedit= 0;
        tex_space_mesh(p2dhold->data);

        p2dhold=NULL;
}

static void dxf_read_polyline(int noob) {	
	/* Entity specific vars */
	short vspace=0; /* Whether or not coords are relative */
	int flag=0;
	int vflags=0;
	int vids[4];
	int nverts;
	
	/* Blender vars */
	Object *ob;
	Mesh *me;
	float vert[3];
	
	MVert *mvert, *vtmp;
	MFace *mface, *ftmp;
	
	reset_vars;

	read_group(id, val);								
	while(id!=0) {
		if (id==8) {
			BLI_strncpy(layname, val, sizeof(layname));
		} else if (id==10) {
			cent[0]= (float) atof(val);
		} else if (id==20) {
			cent[1]= (float) atof(val);
		} else if (id==30) {
			cent[2]= (float) atof(val);
		} else if (id==60) {
			/* short invisible= atoi(val); */
		} else if (id==62) {
			int colorid= atoi(val);
							
			CLAMP(colorid, 1, 255);
			dxf_col_to_rgb(colorid, &color[0], &color[1], &color[2]);
		} else if (id==67) {
			vspace= atoi(val);
		} else if (id==70) {
			flag= atoi(val);			
		}
		read_group(id, val);								
	}

	if (flag&1) {
		if(!lwasp2d || strcmp(layname, oldplay)!=0) dxf_close_2dpoly();
		if(p2dmhold != NULL && p2dmhold->totvert>65000) dxf_close_2dpoly();

		if (p2dmhold==NULL) {
			if (noob) {
				ob= NULL;
				me= add_mesh(); G.totmesh++;
				((ID *)me)->us=0;
		
				if (strlen(entname)) new_id(&G.main->mesh, (ID *)me, entname);
				else if (strlen(layname)) new_id(&G.main->mesh, (ID *)me, layname);
		
				vcenter= zerovec;
			} else {
				ob= add_object(OB_MESH);
				if (strlen(entname)) new_id(&G.main->object, (ID *)ob, entname);
				else if (strlen(layname)) new_id(&G.main->object, (ID *)ob, layname);
			
				if (strlen(layname)) ob->lay= dxf_get_layer_num(layname);
				else ob->lay= G.scene->lay;
		
				me= ob->data;
				
				vcenter= ob->loc;
			}
			me->totvert=0;
			me->totface=0;
			me->mvert=NULL;
			me->mface=NULL;

			strcpy(oldplay, layname);
				
			if(ob) VECCOPY(ob->loc, cent);
		
			dxf_add_mat (ob, me, color, layname);
		
			p2dhold= ob;
			p2dmhold= me;
		} else {
			ob= p2dhold;
			me= p2dmhold;
		}
		
		nverts=0;
		while (group_is(0, "VERTEX")) {
			read_group(id, val);
			while(id!=0) {
				if (id==10) {
					vert[0]= (float) atof(val);
				} else if (id==20) {
					vert[1]= (float) atof(val);
				} else if (id==30) {
					vert[2]= (float) atof(val);
				}
				read_group(id, val);
			}
			nverts++;
			me->totvert++;
			
			vtmp= MEM_callocN(me->totvert*sizeof(MVert), "mverts");
			
			if (me->mvert) {
				memcpy (vtmp, me->mvert, (me->totvert-1)*sizeof(MVert));
				MEM_freeN(me->mvert);
			}
			me->mvert= vtmp;
			vtmp= NULL;
			
			mvert= &me->mvert[me->totvert-1];
			
			if (vspace) { VECCOPY(mvert->co, vert);
			} else VecSubf(mvert->co, vert, vcenter);
		}

		me->totface++;
		ftmp= MEM_callocN(me->totface*sizeof(MFace), "mface");

		if(me->mface) {
			memcpy(ftmp, me->mface, (me->totface-1)*sizeof(MFace));
			MEM_freeN(me->mface);
        }
        me->mface= ftmp;
        ftmp=NULL;

		mface= &(((MFace*)me->mface)[me->totface-1]);
		
		mface->v1= (me->totvert-nverts)+0;
		mface->v2= (me->totvert-nverts)+1;
		mface->v3= (me->totvert-nverts)+2;
		if (nverts==4) mface->v4= (me->totvert-nverts)+3;
		
		mface->edcode= 3;
		mface->mat_nr= 0;

		test_index_mface(mface, nverts);
		
		lwasp2d=1;
	} else if (flag&64) {
		if (noob) {
			ob= NULL;
			me= add_mesh(); G.totmesh++;
			((ID *)me)->us=0;
	
			if (strlen(entname)) new_id(&G.main->mesh, (ID *)me, entname);
			else if (strlen(layname)) new_id(&G.main->mesh, (ID *)me, layname);
	
			vcenter= zerovec;
		} else {
			ob= add_object(OB_MESH);
			if (strlen(entname)) new_id(&G.main->object, (ID *)ob, entname);
			else if (strlen(layname)) new_id(&G.main->object, (ID *)ob, layname);
		
			if (strlen(layname)) ob->lay= dxf_get_layer_num(layname);
			else ob->lay= G.scene->lay;
	
			me= ob->data;
			
			vcenter= ob->loc;
		}
		me->totvert=0;
		me->totface=0;
		me->mvert=NULL;
		me->mface=NULL;
			
		if(ob) VECCOPY(ob->loc, cent);
	
		dxf_add_mat (ob, me, color, layname);

		while (group_is(0, "VERTEX")) {
			vflags= 0;
			vids[0]= vids[1]= vids[2]= vids[3]= 0;
		
			vflags=0;
			read_group(id, val);
			while(id!=0) {
				if(id==8) {
					; /* Layer def, skip */
				} else if (id==10) {
					vert[0]= (float) atof(val);
				} else if (id==20) {
					vert[1]= (float) atof(val);
				} else if (id==30) {
					vert[2]= (float) atof(val);
				} else if (id==70) {
					vflags= atoi(val);
				} else if (id==71) {
					vids[0]= abs(atoi(val));
				} else if (id==72) {
					vids[1]= abs(atoi(val));
				} else if (id==73) {
					vids[2]= abs(atoi(val));
				} else if (id==74) {
					vids[3]= abs(atoi(val));
				}
				read_group(id, val);
			}
			
			if (vflags & 128 && vflags & 64) {
				me->totvert++;
				
				/* If we are nearing the limit scan to the next entry */
				if(me->totvert > 65000) while(group_isnt(0, "SEQEND")) read_group(id, val);
		
				vtmp= MEM_callocN(me->totvert*sizeof(MVert), "mverts");
	
				if(me->mvert) {
					memcpy(vtmp, me->mvert, (me->totvert-1)*sizeof(MVert));
					MEM_freeN(me->mvert);
				}
				me->mvert= vtmp;
				vtmp=NULL;
				
				mvert= &me->mvert[(me->totvert-1)];
	
				if (vspace) { VECCOPY(mvert->co, vert);
				} else VecSubf(mvert->co, vert, vcenter);
	
			} else if (vflags & 128) {
				if(vids[2]==0) {
					error("(PL) Error parsing dxf, not enough vertices near line %d", dxf_line);
			
					error_exit=1;
					fclose(dxf_fp);
					return;
				}
	
				me->totface++;
		
				ftmp= MEM_callocN(me->totface*sizeof(MFace), "mfaces");
	
				if(me->mface) {
					memcpy(ftmp, me->mface, (me->totface-1)*sizeof(MFace));
					MEM_freeN(me->mface);
				}
				me->mface= ftmp;
				ftmp=NULL;			
				
				mface= &(((MFace*)me->mface)[me->totface-1]);
				mface->v1= vids[0]-1;
				mface->v2= vids[1]-1;
				mface->v3= vids[2]-1;
	
				if(vids[3])
					mface->v4= vids[3]-1;
	
				mface->edcode= 3;
				mface->mat_nr= 0;
	
				if(vids[3])
					test_index_mface(mface, 4);
				else 
					test_index_mface(mface, 3);
			} else {
				error("Error parsing dxf, unknown polyline information near %d", dxf_line);
			
				error_exit=1;
				fclose(dxf_fp);
				return;
			}
	
		}	
		
		if (!noob) {
			G.obedit= ob;
			make_editMesh();
			load_editMesh();
			free_editMesh();
			waitcursor(1);		/* patch yah... */
			G.obedit= 0;
		}
		tex_space_mesh(me);
	}
}

	/* 3D Face state vars */
static Object *f3dhold=NULL;
static Mesh *f3dmhold=NULL;
static char oldflay[32];
static short lwasf3d=0; /* last was face 3d? */

static void dxf_close_3dface(void)
{
	f3dmhold= NULL;
	if (f3dhold==NULL) return;
	
	G.obedit= f3dhold;
	make_editMesh();
	load_editMesh();
	free_editMesh();
	waitcursor(1);		/* patch yah... */
	G.obedit= 0;
	tex_space_mesh(f3dhold->data);

	f3dhold=NULL;
}

static void dxf_read_3dface(int noob) 
{	
	/* Entity specific vars */
	float vert2[3]={0.0, 0.0, 0.0};
	float vert3[3]={0.0, 0.0, 0.0};
	float vert4[3]={0.0, 0.0, 0.0};
	short vspace=0;

	int nverts=0;
	
	/* Blender vars */
	Object *ob;
	Mesh *me;
	MVert *mvert, *vtmp;
	MFace *mface, *ftmp;
	
	reset_vars;

	read_group(id, val);								
	while(id!=0) {
		if (id==8) {
			BLI_strncpy(layname, val, sizeof(layname));
		
		/* First vert/origin */
		} else if (id==10) {
			cent[0]= (float) atof(val);
			if (nverts<1)nverts++;
		} else if (id==20) {
			cent[1]= (float) atof(val);
			if (nverts<1)nverts++;
		} else if (id==30) {
			cent[2]= (float) atof(val);
			if (nverts<1)nverts++;
			
		/* Second vert */
		} else if (id==11) {
			vert2[0]= (float) atof(val);
			if (nverts<2)nverts++;
		} else if (id==21) {
			vert2[1]= (float) atof(val);
			if (nverts<2)nverts++;
		} else if (id==31) {
			vert2[2]= (float) atof(val);
			if (nverts<2)nverts++;
		
		/* Third vert */
		} else if (id==12) {
			vert3[0]= (float) atof(val);
			if (nverts<3)nverts++;
		} else if (id==22) {
			vert3[1]= (float) atof(val);
			if (nverts<3)nverts++;
		} else if (id==32) {
			vert3[2]= (float) atof(val);
			if (nverts<3)nverts++;
			
		/* Fourth vert */
		} else if (id==13) {
			vert4[0]= (float) atof(val);
			if (nverts<4)nverts++;
		} else if (id==23) {
			vert4[1]= (float) atof(val);
			if (nverts<4)nverts++;
		} else if (id==33) {
			vert4[2]= (float) atof(val);
			if (nverts<4)nverts++;
			
		/* Other */
		} else if (id==60) {
			/* short invisible= atoi(val); */
		} else if (id==62) {
			int colorid= atoi(val);
							
			CLAMP(colorid, 1, 255);
			dxf_col_to_rgb(colorid, &color[0], &color[1], &color[2]);
		} else if (id==67) {
			vspace= atoi(val);
		}
		read_group(id, val);								
	}

	/* Check to see if we need to make a new object */

	if(!lwasf3d || strcmp(layname, oldflay)!=0) dxf_close_3dface();
	if(f3dmhold != NULL && f3dmhold->totvert>65000) dxf_close_3dface();
	
	if(nverts<3) {
		error("(3DF) Error parsing dxf, not enough vertices near line %d", dxf_line);
		
		error_exit=1;
		fclose(dxf_fp);
		return;
	}

	if (f3dmhold==NULL) {
		if (noob) {
			ob= NULL;
			me= add_mesh(); G.totmesh++;
			((ID *)me)->us=0;

			if (strlen(entname)) new_id(&G.main->mesh, (ID *)me, entname);
			else if (strlen(layname)) new_id(&G.main->mesh, (ID *)me, layname);
	
			vcenter= zerovec;
		} else {
			ob= add_object(OB_MESH);
			if (strlen(entname)) new_id(&G.main->object, (ID *)ob, entname);
			else if (strlen(layname)) new_id(&G.main->object, (ID *)ob, layname);
	
			if (strlen(layname)) ob->lay= dxf_get_layer_num(layname);
			else ob->lay= G.scene->lay;

			me= ob->data;
		
			vcenter= ob->loc;
		}
		me->totvert=0;
		me->totface=0;
		me->mvert=NULL;
		me->mface=NULL;
		
		strcpy(oldflay, layname);
		
		if(ob) VECCOPY(ob->loc, cent);
	
		dxf_add_mat (ob, me, color, layname);
		
		f3dhold= ob;
		f3dmhold= me;
	} else {
		ob= f3dhold;
		me= f3dmhold;
	}
	
	me->totvert+= nverts;
	me->totface++;
	
	vtmp= MEM_callocN(me->totvert*sizeof(MVert), "mverts");
	ftmp= MEM_callocN(me->totface*sizeof(MFace), "mface");

	if(me->mvert) {
		memcpy(vtmp, me->mvert, (me->totvert-nverts)*sizeof(MVert));
		MEM_freeN(me->mvert);
	}
	me->mvert= vtmp;
	vtmp=NULL;

	if(me->mface) {
		memcpy(ftmp, me->mface, (me->totface-1)*sizeof(MFace));
		MEM_freeN(me->mface);
	}
	me->mface= ftmp;
	ftmp=NULL;
	
	mvert= &me->mvert[(me->totvert-nverts)];
	VecSubf(mvert->co, cent, vcenter);
						
	mvert++;
	if (vspace) { VECCOPY(mvert->co, vert2);
	} else VecSubf(mvert->co, vert2, vcenter);

	mvert++;
	if (vspace) { VECCOPY(mvert->co, vert3);
	} else VecSubf(mvert->co, vert3, vcenter);

	if (nverts==4) {
		mvert++;
		if (vspace) { VECCOPY(mvert->co, vert4);
		} else VecSubf(mvert->co, vert4, vcenter);		
	}

	mface= &(((MFace*)me->mface)[me->totface-1]);
	mface->v1= (me->totvert-nverts)+0;
	mface->v2= (me->totvert-nverts)+1;
	mface->v3= (me->totvert-nverts)+2;

	if (nverts==4)
		mface->v4= (me->totvert-nverts)+3;

	mface->edcode= 3;
	mface->mat_nr= 0;

	test_index_mface(mface, nverts);
	
	hasbumped=1;
}

static void dxf_read(char *filename)
{
	dxf_line=0;
	
	dxf_fp= fopen(filename, "r");
	if (dxf_fp==NULL) return;

	while (1) {	
		read_group(id, val);
		if (group_is(0, "EOF")) break;
		
		if (id==999) continue;
		id_check(0, "SECTION");
	
		read_group(id, val);
		if (group_is(2, "HEADER")) {		
		} else if (group_is(2, "TABLES")) {
		} else if (group_is(2, "OBJECTS")) {
		} else if (group_is(2, "CLASSES")) {
		} else if (group_is(2, "BLOCKS")) {	
			while(1) {
				read_group(id, val);
				if (group_is(0, "BLOCK")) {
					while(group_isnt(0, "ENDBLK")) {
						read_group(id, val);
						if(id==2) {
							BLI_strncpy(entname, val, sizeof(entname));
						} else if (id==3) {
							/* Now the object def should follow */
							if(strlen(entname)==0) {
								error("Error parsing dxf, no mesh name near %d", dxf_line);
								fclose(dxf_fp);
								return;
							}
						
							/* Now the object def should follow */
							while(group_isnt(0, "ENDBLK")) {
								read_group(id, val);
								if(group_is(0, "POLYLINE")) {
									dxf_read_polyline(1);
									if(error_exit) return;
									lwasf3d=0;
									lwasline=0;

									while(group_isnt(0, "SEQEND")) read_group(id, val);						
								} else if(group_is(0, "ATTRIB")) {
									while(group_isnt(0, "SEQEND")) read_group(id, val);						
									lwasf3d=0;
									lwasp2d=0;
									lwasline=0;
								} else if(group_is(0, "POINT")) {
									dxf_read_point(1);
									if(error_exit) return;
									lwasf3d=0;
									lwasp2d=0;
									lwasline=0;
								} else if(group_is(0, "LINE")) {
									dxf_read_line(1);
									if(error_exit) return;
									lwasline=1;
									lwasp2d=0;
									lwasf3d=0;
								} else if(group_is(0, "3DFACE")) {
									dxf_read_3dface(1);
									if(error_exit) return;
									lwasf3d=1;
									lwasp2d=0;
									lwasline=0;
								} else if (group_is(0, "ENDBLK")) { 
									break;
								}
							}
						} else if (group_is(0, "ENDBLK")) {
							break;
						}
					}
					while(id!=0) read_group(id, val);
				} else if(group_is(0, "ENDSEC")) {
					break;
				}
			}
		} else if (group_is(2, "ENTITIES")) {			
			while(group_isnt(0, "ENDSEC")) {
				char obname[32]="";
				char layname[32]="";
				float cent[3]={0.0, 0.0, 0.0};
				float obsize[3]={1.0, 1.0, 1.0};
				float obrot[3]={0.0, 0.0, 0.0};
				int i;
				
				if(!hasbumped) read_group(id, val);
				hasbumped=0;
				if (group_is(0, "INSERT")) {
					Base *base;
					Object *ob;
					void *obdata;
					
					read_group(id, val);
					while(id!=0) {
						if(id==2) {
							BLI_strncpy(obname, val, sizeof(obname));
						} else if (id==8) {
							BLI_strncpy(layname, val, sizeof(layname));
						} else if (id==10) {
							cent[0]= (float) atof(val);
						} else if (id==20) {
							cent[1]= (float) atof(val);
						} else if (id==30) {
							cent[2]= (float) atof(val);
						} else if (id==41) {
							obsize[0]= (float) atof(val);
						} else if (id==42) {
							obsize[1]= (float) atof(val);
						} else if (id==43) {
							obsize[2]= (float) atof(val);
						} else if (id==50) {
							obrot[2]= (float) (atof(val)*M_PI/180.0);
						} else if (id==60) {
							/* short invisible= atoi(val); */
						}
						
						read_group(id, val);
					}
			
					if(strlen(obname)==0) {
						error("Error parsing dxf, no object name near %d", dxf_line);
						fclose(dxf_fp);
						return;
					}
					
					obdata= find_id("ME", obname);
	
					if (obdata) {
						ob= alloc_libblock(&G.main->object, ID_OB, obname);
	
						ob->type= OB_MESH;
	
						ob->dt= OB_SHADED;
						if(U.flag & MAT_ON_OB) ob->colbits= -1;

						ob->trackflag= OB_POSY;
						ob->upflag= OB_POSZ;

						ob->ipoflag = OB_OFFS_OB+OB_OFFS_PARENT;
	
						ob->dupon= 1; ob->dupoff= 0;
						ob->dupsta= 1; ob->dupend= 100;
		
						G.totobj++;

						ob->data= obdata;
						((ID*)ob->data)->us++;
						
						VECCOPY(ob->loc, cent);
						VECCOPY(ob->size, obsize);
						VECCOPY(ob->rot, obrot);
						
						ob->mat= MEM_callocN(sizeof(void *)*1, "ob->mat");
						ob->totcol= (unsigned char) ((Mesh*)ob->data)->totcol;
						ob->actcol= 1;
	
						for (i=0; i<ob->totcol; i++) ob->mat[i]= ((Mesh*)ob->data)->mat[i];
						
						if (strlen(layname)) ob->lay= dxf_get_layer_num(layname);
						else ob->lay= G.scene->lay;
	
						/* aan de scene hangen */
						base= MEM_callocN( sizeof(Base), "add_base");
						BLI_addhead(&G.scene->base, base);
		
						base->lay= ob->lay;
		
						base->object= ob;
	
						G.obedit= ob;
						make_editMesh();
						load_editMesh();
						free_editMesh();
						waitcursor(1);		/* patch yah... */
						G.obedit= 0;
						tex_space_mesh(ob->data);
					}

					hasbumped=1;

					lwasf3d=0;
					lwasp2d=0;
					lwasline=0;
				} else if(group_is(0, "POLYLINE")) {
					dxf_read_polyline(0);
					if(error_exit) return;
					lwasf3d=0;
					lwasline=0;

					while(group_isnt(0, "SEQEND")) read_group(id, val);						
				} else if(group_is(0, "ATTRIB")) {
					while(group_isnt(0, "SEQEND")) read_group(id, val);						
					lwasf3d=0;
					lwasp2d=0;
					lwasline=0;
				} else if(group_is(0, "POINT")) {
					dxf_read_point(0);
					if(error_exit) return;
					lwasf3d=0;
					lwasp2d=0;
					lwasline=0;
				} else if(group_is(0, "LINE")) {
					dxf_read_line(0);
					if(error_exit) return;
					lwasline=1;
					lwasp2d=0;
					lwasf3d=0;
				} else if(group_is(0, "3DFACE")) {
					dxf_read_3dface(0);
					if(error_exit) return;
					lwasline=0;
					lwasp2d=0;
					lwasf3d=1;
				} else if(group_is(0, "ENDSEC")) {
					break;
				}
			}
		}
	
		while(group_isnt(0, "ENDSEC")) read_group(id, val);
	}		
	id_check(0, "EOF");
	
	fclose (dxf_fp);
	
	/* Close any remaining state held stuff */
	dxf_close_3dface();
	dxf_close_2dpoly();
	dxf_close_line();
}
