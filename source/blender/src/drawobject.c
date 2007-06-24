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

#include <string.h>
#include <math.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "MEM_guardedalloc.h"

#include "BMF_Api.h"

#include "IMB_imbuf.h"


#include "MTC_matrixops.h"

#include "DNA_armature_types.h"
#include "DNA_camera_types.h"
#include "DNA_curve_types.h"
#include "DNA_constraint_types.h" // for drawing constraint
#include "DNA_effect_types.h"
#include "DNA_ipo_types.h"
#include "DNA_lamp_types.h"
#include "DNA_lattice_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_meta_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_object_force.h"
#include "DNA_space_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_userdef_types.h"
#include "DNA_view3d_types.h"
#include "DNA_world_types.h"
// FSPARTICLE
#include "DNA_object_fluidsim.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_editVert.h"
#include "BLI_edgehash.h"

#include "BKE_utildefines.h"
#include "BKE_curve.h"
#include "BKE_constraint.h" // for the get_constraint_target function
#include "BKE_DerivedMesh.h"
#include "BKE_displist.h"
#include "BKE_effect.h"
#include "BKE_font.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_ipo.h"
#include "BKE_key.h"
#include "BKE_lattice.h"
#include "BKE_mesh.h"
#include "BKE_material.h"
#include "BKE_mball.h"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_anim.h"			//for the where_on_path function
#ifdef WITH_VERSE
#include "BKE_verse.h"
#endif

#include "BIF_editarmature.h"
#include "BIF_editdeform.h"
#include "BIF_editmesh.h"
#include "BIF_glutil.h"
#include "BIF_gl.h"
#include "BIF_glutil.h"
#include "BIF_mywindow.h"
#include "BIF_resources.h"
#include "BIF_retopo.h"
#include "BIF_screen.h"
#include "BIF_space.h"

#include "BDR_drawmesh.h"
#include "BDR_drawobject.h"
#include "BDR_editobject.h"
#include "BDR_sculptmode.h"
#include "BDR_vpaint.h"

#include "BSE_drawview.h"
#include "BSE_node.h"
#include "BSE_trans_types.h"
#include "BSE_view.h"

#include "blendef.h"
#include "mydevice.h"
#include "nla.h"

#include "BKE_deform.h"

/* pretty stupid */
/*  extern Lattice *editLatt; already in BKE_lattice.h  */
/* editcurve.c */
extern ListBase editNurb;
/* editmball.c */
extern ListBase editelems;

static void draw_bounding_volume(Object *ob);

/* ************* Setting OpenGL Material ************ */

// Materials start counting at # one....
#define MAXMATBUF (MAXMAT + 1)
static float matbuf[MAXMATBUF][2][4];
static int totmat_gl= 0;

int set_gl_material(int nr)
{
	static int last_gl_matnr= -1;
	static int last_ret_val= 1;
	
	/* prevent index to use un-initialized array items */
	if(nr>totmat_gl) nr= totmat_gl;
	
	if(nr<0) {
		last_gl_matnr= -1;
		last_ret_val= 1;
	}
	else if(nr<MAXMATBUF && nr!=last_gl_matnr) {
		glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, matbuf[nr][0]);
		glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, matbuf[nr][1]);
		last_gl_matnr = nr;
		last_ret_val= matbuf[nr][0][3]!=0.0;
		
		/* matbuf alpha: 0.0 = skip draw, 1.0 = no blending, else blend */
		if(matbuf[nr][0][3]!= 0.0 && matbuf[nr][0][3]!= 1.0) {
			glEnable(GL_BLEND);
		}
		else
			glDisable(GL_BLEND);
			
	}
	
	return last_ret_val;
}

/* returns 1: when there's alpha needed to be drawn in a 2nd pass */
int init_gl_materials(Object *ob, int check_alpha)
{
	extern Material defmaterial;	// render module abuse...
	Material *ma;
	int a, has_alpha= 0;
	
	if(ob->totcol==0) {
		matbuf[0][0][0]= defmaterial.r;
		matbuf[0][0][1]= defmaterial.g;
		matbuf[0][0][2]= defmaterial.b;
		matbuf[0][0][3]= 1.0;

		matbuf[0][1][0]= defmaterial.specr;
		matbuf[0][1][1]= defmaterial.specg;
		matbuf[0][1][2]= defmaterial.specb;
		matbuf[0][1][3]= 1.0;
		
		/* do material 1 too, for displists! */
		QUATCOPY(matbuf[1][0], matbuf[0][0]);
		QUATCOPY(matbuf[1][1], matbuf[0][1]);
	}
	
	for(a=1; a<=ob->totcol; a++) {
		ma= give_current_material(ob, a);
		ma= editnode_get_active_material(ma);
		if(ma==NULL) ma= &defmaterial;

		if(a<MAXMATBUF) {
			if (ma->mode & MA_SHLESS) {
				matbuf[a][0][0]= 2*ma->r;
				matbuf[a][0][1]= 2*ma->g;
				matbuf[a][0][2]= 2*ma->b;
			} else {
				matbuf[a][0][0]= (ma->ref+ma->emit)*ma->r;
				matbuf[a][0][1]= (ma->ref+ma->emit)*ma->g;
				matbuf[a][0][2]= (ma->ref+ma->emit)*ma->b;
			}

			/* draw transparent, not in pick-select, nor editmode */
			if(check_alpha && !(G.f & G_PICKSEL) && (ob->dtx & OB_DRAWTRANSP) && !(G.obedit && G.obedit->data==ob->data)) {
				if(G.vd->transp) {	// drawing the transparent pass
					if(ma->alpha==1.0) matbuf[a][0][3]= 0.0;	// means skip solid
					else matbuf[a][0][3]= ma->alpha;
				}
				else {	// normal pass
					if(ma->alpha==1.0) matbuf[a][0][3]= 1.0;
					else {
						matbuf[a][0][3]= 0.0;	// means skip transparent
						has_alpha= 1;			// return value, to indicate adding to after-draw queue
					}
				}
			}
			else
				matbuf[a][0][3]= 1.0;
			
			if (!(ma->mode & MA_SHLESS)) {
				matbuf[a][1][0]= ma->spec*ma->specr;
				matbuf[a][1][1]= ma->spec*ma->specg;
				matbuf[a][1][2]= ma->spec*ma->specb;
				matbuf[a][1][3]= 1.0;
			}
		}
	}

	totmat_gl= ob->totcol;
	set_gl_material(-1);		// signal for static variable
	return has_alpha;
}


	/***/
static unsigned int colortab[24]=
	{0x0,		0xFF88FF, 0xFFBBFF, 
	 0x403000,	0xFFFF88, 0xFFFFBB, 
	 0x104040,	0x66CCCC, 0x77CCCC, 
	 0x104010,	0x55BB55, 0x66FF66, 
	 0xFFFFFF
};


static float cube[8][3] = {
	{-1.0, -1.0, -1.0},
	{-1.0, -1.0,  1.0},
	{-1.0,  1.0,  1.0},
	{-1.0,  1.0, -1.0},
	{ 1.0, -1.0, -1.0},
	{ 1.0, -1.0,  1.0},
	{ 1.0,  1.0,  1.0},
	{ 1.0,  1.0, -1.0},
};

/* flag is same as for draw_object */
void drawaxes(float size, int flag, char drawtype)
{
	int axis;
	float v1[3]= {0.0, 0.0, 0.0};
	float v2[3]= {0.0, 0.0, 0.0};
	float v3[3]= {0.0, 0.0, 0.0};
	
	switch(drawtype) {
	
	case OB_PLAINAXES:
		for (axis=0; axis<3; axis++) {
			float v1[3]= {0.0, 0.0, 0.0};
			float v2[3]= {0.0, 0.0, 0.0};
			
			glBegin(GL_LINES);

			v1[axis]= size;
			v2[axis]= -size;
			glVertex3fv(v1);
			glVertex3fv(v2);
			
			glEnd();
		}
		break;
	case OB_SINGLE_ARROW:
	
		glBegin(GL_LINES);
		/* in positive z direction only */
		v1[2]= size;
		glVertex3fv(v1);
		glVertex3fv(v2);
		glEnd();
		
		/* square pyramid */
		glBegin(GL_TRIANGLES);
		
		v2[0]= size*0.035; v2[1] = size*0.035;
		v3[0]= size*-0.035; v3[1] = size*0.035;
		v2[2]= v3[2]= size*0.75;
		
		for (axis=0; axis<4; axis++) {
			if (axis % 2 == 1) {
				v2[0] *= -1;
				v3[1] *= -1;
			} else {
				v2[1] *= -1;
				v3[0] *= -1;
			}
			
			glVertex3fv(v1);
			glVertex3fv(v2);
			glVertex3fv(v3);
			
		}
		glEnd();
		
		break;
	case OB_ARROWS:
	default:
		for (axis=0; axis<3; axis++) {
			float v1[3]= {0.0, 0.0, 0.0};
			float v2[3]= {0.0, 0.0, 0.0};
			int arrow_axis= (axis==0)?1:0;
			
			glBegin(GL_LINES);

			v2[axis]= size;
			glVertex3fv(v1);
			glVertex3fv(v2);
				
			v1[axis]= size*0.8;
			v1[arrow_axis]= -size*0.125;
			glVertex3fv(v1);
			glVertex3fv(v2);
				
			v1[arrow_axis]= size*0.125;
			glVertex3fv(v1);
			glVertex3fv(v2);

			glEnd();
				
			v2[axis]+= size*0.125;
			glRasterPos3fv(v2);
			
			// patch for 3d cards crashing on glSelect for text drawing (IBM)
			if((flag & DRAW_PICKING) == 0) {
				if (axis==0)
					BMF_DrawString(G.font, "x");
				else if (axis==1)
					BMF_DrawString(G.font, "y");
				else
					BMF_DrawString(G.font, "z");
			}
		}
		break;
	}
}

/* circle for object centers, special_color is for library or ob users */
static void drawcentercircle(float *vec, int selstate, int special_color)
{
	View3D *v3d= G.vd;
	float size;
	
	size= v3d->persmat[0][3]*vec[0]+ v3d->persmat[1][3]*vec[1]+ v3d->persmat[2][3]*vec[2]+ v3d->persmat[3][3];
	size*= v3d->pixsize*((float)U.obcenter_dia*0.5f);

	/* using gldepthfunc guarantees that it does write z values, but not checks for it, so centers remain visible independt order of drawing */
	if(v3d->zbuf)  glDepthFunc(GL_ALWAYS);
	glEnable(GL_BLEND);
	
	if(special_color) {
#ifdef WITH_VERSE
		if (selstate==VERSE) glColor4ub(0x00, 0xFF, 0x00, 155);
		else if (selstate==ACTIVE || selstate==SELECT) glColor4ub(0x88, 0xFF, 0xFF, 155);
#else
		if (selstate==ACTIVE || selstate==SELECT) glColor4ub(0x88, 0xFF, 0xFF, 155);
#endif

		else glColor4ub(0x55, 0xCC, 0xCC, 155);
	}
	else {
		if (selstate == ACTIVE) BIF_ThemeColorShadeAlpha(TH_ACTIVE, 0, -80);
		else if (selstate == SELECT) BIF_ThemeColorShadeAlpha(TH_SELECT, 0, -80);
		else if (selstate == DESELECT) BIF_ThemeColorShadeAlpha(TH_TRANSFORM, 0, -80);
	}
	drawcircball(GL_POLYGON, vec, size, v3d->viewinv);
	
	BIF_ThemeColorShadeAlpha(TH_WIRE, 0, -30);
	drawcircball(GL_LINE_LOOP, vec, size, v3d->viewinv);
	
	glDisable(GL_BLEND);
	if(v3d->zbuf)  glDepthFunc(GL_LEQUAL);
}


void drawsolidcube(float size)
{
	float n[3];

	glPushMatrix();
	glScalef(size, size, size);
	
	n[0]=0; n[1]=0; n[2]=0;
	glBegin(GL_QUADS);
		n[0]= -1.0;
		glNormal3fv(n); 
		glVertex3fv(cube[0]); glVertex3fv(cube[1]); glVertex3fv(cube[2]); glVertex3fv(cube[3]);
		n[0]=0;
	glEnd();

	glBegin(GL_QUADS);
		n[1]= -1.0;
		glNormal3fv(n); 
		glVertex3fv(cube[0]); glVertex3fv(cube[4]); glVertex3fv(cube[5]); glVertex3fv(cube[1]);
		n[1]=0;
	glEnd();

	glBegin(GL_QUADS);
		n[0]= 1.0;
		glNormal3fv(n); 
		glVertex3fv(cube[4]); glVertex3fv(cube[7]); glVertex3fv(cube[6]); glVertex3fv(cube[5]);
		n[0]=0;
	glEnd();

	glBegin(GL_QUADS);
		n[1]= 1.0;
		glNormal3fv(n); 
		glVertex3fv(cube[7]); glVertex3fv(cube[3]); glVertex3fv(cube[2]); glVertex3fv(cube[6]);
		n[1]=0;
	glEnd();

	glBegin(GL_QUADS);
		n[2]= 1.0;
		glNormal3fv(n); 
		glVertex3fv(cube[1]); glVertex3fv(cube[5]); glVertex3fv(cube[6]); glVertex3fv(cube[2]);
		n[2]=0;
	glEnd();

	glBegin(GL_QUADS);
		n[2]= -1.0;
		glNormal3fv(n); 
		glVertex3fv(cube[7]); glVertex3fv(cube[4]); glVertex3fv(cube[0]); glVertex3fv(cube[3]);
	glEnd();
	
	glPopMatrix();
}

static void drawcube(void)
{

	glBegin(GL_LINE_STRIP);
		glVertex3fv(cube[0]); glVertex3fv(cube[1]);glVertex3fv(cube[2]); glVertex3fv(cube[3]);
		glVertex3fv(cube[0]); glVertex3fv(cube[4]);glVertex3fv(cube[5]); glVertex3fv(cube[6]);
		glVertex3fv(cube[7]); glVertex3fv(cube[4]);
	glEnd();

	glBegin(GL_LINE_STRIP);
		glVertex3fv(cube[1]); glVertex3fv(cube[5]);
	glEnd();

	glBegin(GL_LINE_STRIP);
		glVertex3fv(cube[2]); glVertex3fv(cube[6]);
	glEnd();

	glBegin(GL_LINE_STRIP);
		glVertex3fv(cube[3]); glVertex3fv(cube[7]);
	glEnd();
}

#if 0
static void drawcube_size(float *size)
{

	glPushMatrix();
	glScalef(size[0],  size[1],  size[2]);
	

	glBegin(GL_LINE_STRIP);
		glVertex3fv(cube[0]); glVertex3fv(cube[1]);glVertex3fv(cube[2]); glVertex3fv(cube[3]);
		glVertex3fv(cube[0]); glVertex3fv(cube[4]);glVertex3fv(cube[5]); glVertex3fv(cube[6]);
		glVertex3fv(cube[7]); glVertex3fv(cube[4]);
	glEnd();

	glBegin(GL_LINE_STRIP);
		glVertex3fv(cube[1]); glVertex3fv(cube[5]);
	glEnd();

	glBegin(GL_LINE_STRIP);
		glVertex3fv(cube[2]); glVertex3fv(cube[6]);
	glEnd();

	glBegin(GL_LINE_STRIP);
		glVertex3fv(cube[3]); glVertex3fv(cube[7]);
	glEnd();
	
	glPopMatrix();
}
#endif

static void drawshadbuflimits(Lamp *la, float mat[][4])
{
	float sta[3], end[3], lavec[3];

	lavec[0]= -mat[2][0];
	lavec[1]= -mat[2][1];
	lavec[2]= -mat[2][2];
	Normalize(lavec);

	sta[0]= mat[3][0]+ la->clipsta*lavec[0];
	sta[1]= mat[3][1]+ la->clipsta*lavec[1];
	sta[2]= mat[3][2]+ la->clipsta*lavec[2];

	end[0]= mat[3][0]+ la->clipend*lavec[0];
	end[1]= mat[3][1]+ la->clipend*lavec[1];
	end[2]= mat[3][2]+ la->clipend*lavec[2];


	glBegin(GL_LINE_STRIP);
		glVertex3fv(sta);
		glVertex3fv(end);
	glEnd();

	glPointSize(3.0);
	bglBegin(GL_POINTS);
	bglVertex3fv(sta);
	bglVertex3fv(end);
	bglEnd();
	glPointSize(1.0);
}



static void spotvolume(float *lvec, float *vvec, float inp)
{
	/* camera is at 0,0,0 */
	float temp[3],plane[3],mat1[3][3],mat2[3][3],mat3[3][3],mat4[3][3],q[4],co,si,angle;

	Normalize(lvec);
	Normalize(vvec);				/* is this the correct vector ? */

	Crossf(temp,vvec,lvec);		/* equation for a plane through vvec en lvec */
	Crossf(plane,lvec,temp);		/* a plane perpendicular to this, parrallel with lvec */

	Normalize(plane);

	/* now we've got two equations: one of a cone and one of a plane, but we have
	three unknowns. We remove one unkown by rotating the plane to z=0 (the plane normal) */

	/* rotate around cross product vector of (0,0,1) and plane normal, dot product degrees */
	/* according definition, we derive cross product is (plane[1],-plane[0],0), en cos = plane[2]);*/

	/* translating this comment to english didnt really help me understanding the math! :-) (ton) */
	
	q[1] = plane[1] ; 
	q[2] = -plane[0] ; 
	q[3] = 0 ;
	Normalize(&q[1]);

	angle = saacos(plane[2])/2.0;
	co = cos(angle);
	si = sqrt(1-co*co);

	q[0] =  co;
	q[1] *= si;
	q[2] *= si;
	q[3] =  0;

	QuatToMat3(q,mat1);

	/* rotate lamp vector now over acos(inp) degrees */

	vvec[0] = lvec[0] ; 
	vvec[1] = lvec[1] ; 
	vvec[2] = lvec[2] ;

	Mat3One(mat2);
	co = inp;
	si = sqrt(1-inp*inp);

	mat2[0][0] =  co;
	mat2[1][0] = -si;
	mat2[0][1] =  si;
	mat2[1][1] =  co;
	Mat3MulMat3(mat3,mat2,mat1);

	mat2[1][0] =  si;
	mat2[0][1] = -si;
	Mat3MulMat3(mat4,mat2,mat1);
	Mat3Transp(mat1);

	Mat3MulMat3(mat2,mat1,mat3);
	Mat3MulVecfl(mat2,lvec);
	Mat3MulMat3(mat2,mat1,mat4);
	Mat3MulVecfl(mat2,vvec);

	return;
}

static void drawlamp(Object *ob)
{
	Lamp *la;
	View3D *v3d= G.vd;
	float vec[3], lvec[3], vvec[3], circrad, x,y,z;
	float pixsize, lampsize;
	float imat[4][4], curcol[4];
	char col[4];
	
	la= ob->data;
	
	/* we first draw only the screen aligned & fixed scale stuff */
	glPushMatrix();
	myloadmatrix(G.vd->viewmat);

	/* lets calculate the scale: */
	pixsize= v3d->persmat[0][3]*ob->obmat[3][0]+ v3d->persmat[1][3]*ob->obmat[3][1]+ v3d->persmat[2][3]*ob->obmat[3][2]+ v3d->persmat[3][3];
	pixsize*= v3d->pixsize;
	lampsize= pixsize*((float)U.obcenter_dia*0.5f);

	/* and view aligned matrix: */
	Mat4CpyMat4(imat, G.vd->viewinv);
	Normalize(imat[0]);
	Normalize(imat[1]);
	
	/* for AA effects */
	glGetFloatv(GL_CURRENT_COLOR, curcol);
	curcol[3]= 0.6;
	glColor4fv(curcol);
	
	if(ob->id.us>1) {
		if (ob==OBACT || (ob->flag & SELECT)) glColor4ub(0x88, 0xFF, 0xFF, 155);
		else glColor4ub(0x77, 0xCC, 0xCC, 155);
	}
	
	/* Inner Circle */
	VECCOPY(vec, ob->obmat[3]);
	glEnable(GL_BLEND);
	drawcircball(GL_LINE_LOOP, vec, lampsize, imat);
	glDisable(GL_BLEND);
	drawcircball(GL_POLYGON, vec, lampsize, imat);
	
	/* restore */
	if(ob->id.us>1)
		glColor4fv(curcol);
		
	/* Outer circle */
	circrad = 3.0f*lampsize;
	drawcircball(GL_LINE_LOOP, vec, circrad, imat);
	
	setlinestyle(3);

	/* draw dashed outer circle if shadow is on. remember some lamps can't have certain shadows! */
	if (la->type!=LA_HEMI) {
		if ((la->mode & LA_SHAD_RAY) ||
			((la->mode & LA_SHAD_BUF) && (la->type==LA_SPOT)) )
		{
			drawcircball(GL_LINE_LOOP, vec, circrad + 3.0f*pixsize, imat);
		}
	}
	
	/* draw the pretty sun rays */
	if(la->type==LA_SUN) {
		float v1[3], v2[3], mat[3][3];
		short axis;
		
		/* setup a 45 degree rotation matrix */
		VecRotToMat3(imat[2], M_PI/4.0f, mat);
		
		/* vectors */
		VECCOPY(v1, imat[0]);
		VecMulf(v1, circrad*1.2f);
		VECCOPY(v2, imat[0]);
		VecMulf(v2, circrad*2.5f);
		
		/* center */
		glTranslatef(vec[0], vec[1], vec[2]);
		
		setlinestyle(3);
		
		glBegin(GL_LINES);
		for (axis=0; axis<8; axis++) {
			glVertex3fv(v1);
			glVertex3fv(v2);
			Mat3MulVecfl(mat, v1);
			Mat3MulVecfl(mat, v2);
		}
		glEnd();
		
		glTranslatef(-vec[0], -vec[1], -vec[2]);

	}		
	
	if (la->type==LA_LOCAL) {
		if(la->mode & LA_SPHERE) {
			drawcircball(GL_LINE_LOOP, vec, la->dist, imat);
		}
		/* yafray: for photonlight also draw lightcone as for spot */
	}
	
	glPopMatrix();	/* back in object space */
	vec[0]= vec[1]= vec[2]= 0.0f;
	
	if ((la->type==LA_SPOT) || (la->type==LA_YF_PHOTON)) {	
		lvec[0]=lvec[1]= 0.0; 
		lvec[2] = 1.0;
		x = G.vd->persmat[0][2];
		y = G.vd->persmat[1][2];
		z = G.vd->persmat[2][2];
		vvec[0]= x*ob->obmat[0][0] + y*ob->obmat[0][1] + z*ob->obmat[0][2];
		vvec[1]= x*ob->obmat[1][0] + y*ob->obmat[1][1] + z*ob->obmat[1][2];
		vvec[2]= x*ob->obmat[2][0] + y*ob->obmat[2][1] + z*ob->obmat[2][2];

		y = cos( M_PI*la->spotsize/360.0 );
		spotvolume(lvec, vvec, y);
		x = -la->dist;
		lvec[0] *=  x ; 
		lvec[1] *=  x ; 
		lvec[2] *=  x;
		vvec[0] *= x ; 
		vvec[1] *= x ; 
		vvec[2] *= x;

		/* draw the angled sides of the cone */
		glBegin(GL_LINE_STRIP);
			glVertex3fv(vvec);
			glVertex3fv(vec);
			glVertex3fv(lvec);
		glEnd();
		
		z = x*sqrt(1.0 - y*y);
		x *= y;

		/* draw the circle/square at the end of the cone */
		glTranslatef(0.0, 0.0 ,  x);
		if(la->mode & LA_SQUARE) {
			vvec[0]= fabs(z);
			vvec[1]= fabs(z);
			vvec[2]= 0.0;
			glBegin(GL_LINE_LOOP);
				glVertex3fv(vvec);
				vvec[1]= -fabs(z);
				glVertex3fv(vvec);
				vvec[0]= -fabs(z);
				glVertex3fv(vvec);
				vvec[1]= fabs(z);
				glVertex3fv(vvec);
			glEnd();
		}
		else circ(0.0, 0.0, fabs(z));
		
		/* draw the circle/square representing spotbl */
		if(la->type==LA_SPOT) {
			float spotblcirc = fabs(z)*(1 - pow(la->spotblend, 2));
			/* make sure the line is always visible - prevent it from reaching the outer border (or 0) 
			 * values are kinda arbitrary - just what seemed to work well */
			if (spotblcirc == 0) spotblcirc = 0.15;
			else if (spotblcirc == fabs(z)) spotblcirc = fabs(z) - 0.07;
			circ(0.0, 0.0, spotblcirc);
		}
		
	}
	else if ELEM(la->type, LA_HEMI, LA_SUN) {
		
		/* draw the line from the circle along the dist */
		glBegin(GL_LINE_STRIP);
			vec[2] = -circrad;
			glVertex3fv(vec); 
			vec[2]= -la->dist; 
			glVertex3fv(vec);
		glEnd();
		
		if(la->type==LA_HEMI) {
			/* draw the hemisphere curves */
			short axis, steps, dir;
			float outdist, zdist, mul;
			vec[0]=vec[1]=vec[2]= 0.0;
			outdist = 0.14; mul = 1.4; dir = 1;
			
			setlinestyle(4);
			/* loop over the 4 compass points, and draw each arc as a LINE_STRIP */
			for (axis=0; axis<4; axis++) {
				float v[3]= {0.0, 0.0, 0.0};
				zdist = 0.02;
				
				glBegin(GL_LINE_STRIP);
				
				for (steps=0; steps<6; steps++) {
					if (axis == 0 || axis == 1) { 		/* x axis up, x axis down */	
						/* make the arcs start at the edge of the energy circle */
						if (steps == 0) v[0] = dir*circrad;
						else v[0] = v[0] + dir*(steps*outdist);
					} else if (axis == 2 || axis == 3) { 		/* y axis up, y axis down */
						/* make the arcs start at the edge of the energy circle */
						if (steps == 0) v[1] = dir*circrad;
						else v[1] = v[1] + dir*(steps*outdist); 
					}
		
					v[2] = v[2] - steps*zdist;
					
					glVertex3fv(v);
					
					zdist = zdist * mul;
				}
				
				glEnd();
				/* flip the direction */
				dir = -dir;
			}
		}
	} else if(la->type==LA_AREA) {
		setlinestyle(3);
		if(la->area_shape==LA_AREA_SQUARE) 
			fdrawbox(-la->area_size*0.5, -la->area_size*0.5, la->area_size*0.5, la->area_size*0.5);
		else if(la->area_shape==LA_AREA_RECT) 
			fdrawbox(-la->area_size*0.5, -la->area_sizey*0.5, la->area_size*0.5, la->area_sizey*0.5);

		glBegin(GL_LINE_STRIP); 
		glVertex3f(0.0,0.0,-circrad);
		glVertex3f(0.0,0.0,-la->dist);
		glEnd();
	}
	
	/* and back to viewspace */
	myloadmatrix(G.vd->viewmat);
	VECCOPY(vec, ob->obmat[3]);

	setlinestyle(0);
	
	if(la->type==LA_SPOT && (la->mode & LA_SHAD_BUF) ) {
		drawshadbuflimits(la, ob->obmat);
	}
	
	BIF_GetThemeColor4ubv(TH_LAMP, col);
	glColor4ub(col[0], col[1], col[2], col[3]);
	 
	glEnable(GL_BLEND);
	
	if (vec[2]>0) vec[2] -= circrad;
	else vec[2] += circrad;
	
	glBegin(GL_LINE_STRIP);
		glVertex3fv(vec); 
		vec[2]= 0; 
		glVertex3fv(vec);
	glEnd();
	
	glPointSize(2.0);
	glBegin(GL_POINTS);
		glVertex3fv(vec);
	glEnd();
	glPointSize(1.0);
	
	glDisable(GL_BLEND);
	
	/* restore for drawing extra stuff */
	glColor3fv(curcol);

}

static void draw_limit_line(float sta, float end, unsigned int col)
{
	glBegin(GL_LINES);
	glVertex3f(0.0, 0.0, -sta);
	glVertex3f(0.0, 0.0, -end);
	glEnd();

	glPointSize(3.0);
	glBegin(GL_POINTS);
	cpack(col);
	glVertex3f(0.0, 0.0, -sta);
	glVertex3f(0.0, 0.0, -end);
	glEnd();
	glPointSize(1.0);
}		


/* yafray: draw camera focus point (cross, similar to aqsis code in tuhopuu) */
/* qdn: now also enabled for Blender to set focus point for defocus composit node */
static void draw_focus_cross(float dist, float size)
{
	glBegin(GL_LINES);
	glVertex3f(-size, 0.f, -dist);
	glVertex3f(size, 0.f, -dist);
	glVertex3f(0.f, -size, -dist);
	glVertex3f(0.f, size, -dist);
	glEnd();
}

/* flag similar to draw_object() */
static void drawcamera(Object *ob, int flag)
{
	/* a standing up pyramid with (0,0,0) as top */
	Camera *cam;
	World *wrld;
	float vec[8][4], tmat[4][4], fac, facx, facy, depth;
	int i;

	cam= ob->data;
	
	glDisable(GL_LIGHTING);
	glDisable(GL_CULL_FACE);
	
	if(G.vd->persp>=2 && cam->type==CAM_ORTHO && ob==G.vd->camera) {
		facx= 0.5*cam->ortho_scale*1.28;
		facy= 0.5*cam->ortho_scale*1.024;
		depth= -cam->clipsta-0.1;
	}
	else {
		fac= cam->drawsize;
		if(G.vd->persp>=2 && ob==G.vd->camera) fac= cam->clipsta+0.1; /* that way it's always visible */
		
		depth= - fac*cam->lens/16.0;
		facx= fac*1.28;
		facy= fac*1.024;
	}
	
	vec[0][0]= 0.0; vec[0][1]= 0.0; vec[0][2]= 0.001;	/* GLBUG: for picking at iris Entry (well thats old!) */
	vec[1][0]= facx; vec[1][1]= facy; vec[1][2]= depth;
	vec[2][0]= facx; vec[2][1]= -facy; vec[2][2]= depth;
	vec[3][0]= -facx; vec[3][1]= -facy; vec[3][2]= depth;
	vec[4][0]= -facx; vec[4][1]= facy; vec[4][2]= depth;

	glBegin(GL_LINE_LOOP);
		glVertex3fv(vec[1]); 
		glVertex3fv(vec[2]); 
		glVertex3fv(vec[3]); 
		glVertex3fv(vec[4]);
	glEnd();
	

	if(G.vd->persp>=2 && ob==G.vd->camera) return;
	
	glBegin(GL_LINE_STRIP);
		glVertex3fv(vec[2]); 
		glVertex3fv(vec[0]);
		glVertex3fv(vec[1]);
		glVertex3fv(vec[4]);
		glVertex3fv(vec[0]);
		glVertex3fv(vec[3]); 
	glEnd();


	/* arrow on top */
	vec[0][2]= depth;

	
	/* draw an outline arrow for inactive cameras and filled
	 * for active cameras. We actually draw both outline+filled
	 * for active cameras so the wire can be seen side-on */	
	for (i=0;i<2;i++) {
		if (i==0) glBegin(GL_LINE_LOOP);
		else if (i==1 && (ob == G.vd->camera)) glBegin(GL_TRIANGLES);
		else break;
		
		vec[0][0]= -0.7*cam->drawsize;
		vec[0][1]= 1.1*cam->drawsize;
		glVertex3fv(vec[0]);
		
		vec[0][0]= 0.0; 
		vec[0][1]= 1.8*cam->drawsize;
		glVertex3fv(vec[0]);
		
		vec[0][0]= 0.7*cam->drawsize; 
		vec[0][1]= 1.1*cam->drawsize;
		glVertex3fv(vec[0]);
	
		glEnd();
	}

	if(flag==0) {
		if(cam->flag & (CAM_SHOWLIMITS+CAM_SHOWMIST)) {
			myloadmatrix(G.vd->viewmat);
			Mat4CpyMat4(vec, ob->obmat);
			Mat4Ortho(vec);
			mymultmatrix(vec);

			MTC_Mat4SwapMat4(G.vd->persmat, tmat);
			mygetsingmatrix(G.vd->persmat);

			if(cam->flag & CAM_SHOWLIMITS) {
				draw_limit_line(cam->clipsta, cam->clipend, 0x77FFFF);
				/* qdn: was yafray only, now also enabled for Blender to be used with defocus composit node */
				draw_focus_cross(cam->YF_dofdist, cam->drawsize);
			}

			wrld= G.scene->world;
			if(cam->flag & CAM_SHOWMIST) 
				if(wrld) draw_limit_line(wrld->miststa, wrld->miststa+wrld->mistdist, 0xFFFFFF);
				
			MTC_Mat4SwapMat4(G.vd->persmat, tmat);
		}
	}
}

static void lattice_draw_verts(Lattice *lt, DispList *dl, short sel)
{
	BPoint *bp = lt->def;
	float *co = dl?dl->verts:NULL;
	int u, v, w;

	BIF_ThemeColor(sel?TH_VERTEX_SELECT:TH_VERTEX);
	glPointSize(BIF_GetThemeValuef(TH_VERTEX_SIZE));
	bglBegin(GL_POINTS);

	for(w=0; w<lt->pntsw; w++) {
		int wxt = (w==0 || w==lt->pntsw-1);
		for(v=0; v<lt->pntsv; v++) {
			int vxt = (v==0 || v==lt->pntsv-1);
			for(u=0; u<lt->pntsu; u++, bp++, co+=3) {
				int uxt = (u==0 || u==lt->pntsu-1);
				if(!(lt->flag & LT_OUTSIDE) || uxt || vxt || wxt) {
					if(bp->hide==0) {
						if((bp->f1 & 1)==sel) {
							bglVertex3fv(dl?co:bp->vec);
						}
					}
				}
			}
		}
	}
	
	glPointSize(1.0);
	bglEnd();	
}

void lattice_foreachScreenVert(void (*func)(void *userData, BPoint *bp, int x, int y), void *userData)
{
	int i, N = editLatt->pntsu*editLatt->pntsv*editLatt->pntsw;
	DispList *dl = find_displist(&G.obedit->disp, DL_VERTS);
	float *co = dl?dl->verts:NULL;
	BPoint *bp = editLatt->def;
	float pmat[4][4], vmat[4][4];
	short s[2];

	view3d_get_object_project_mat(curarea, G.obedit, pmat, vmat);

	for (i=0; i<N; i++, bp++, co+=3) {
		if (bp->hide==0) {
			view3d_project_short_clip(curarea, dl?co:bp->vec, s, pmat, vmat);
			func(userData, bp, s[0], s[1]);
		}
	}
}

static void drawlattice__point(Lattice *lt, DispList *dl, int u, int v, int w, int use_wcol)
{
	int index = ((w*lt->pntsv + v)*lt->pntsu) + u;

	if(use_wcol) {
		float col[3];
		MDeformWeight *mdw= get_defweight (lt->dvert+index, use_wcol-1);
		
		weight_to_rgb(mdw?mdw->weight:0.0f, col, col+1, col+2);
		glColor3fv(col);
	}
	
	if (dl) {
		glVertex3fv(&dl->verts[index*3]);
	} else {
		glVertex3fv(lt->def[index].vec);
	}
}

/* lattice color is hardcoded, now also shows weightgroup values in edit mode */
static void drawlattice(Object *ob)
{
	Lattice *lt;
	DispList *dl;
	int u, v, w;
	int use_wcol= 0;

	lt= (ob==G.obedit)?editLatt:ob->data;
	dl= find_displist(&ob->disp, DL_VERTS);
	if(ob==G.obedit) {
		cpack(0x004000);
		
		if(ob->defbase.first && lt->dvert) {
			use_wcol= ob->actdef;
			glShadeModel(GL_SMOOTH);
		}
	}
	
	glBegin(GL_LINES);
	for(w=0; w<lt->pntsw; w++) {
		int wxt = (w==0 || w==lt->pntsw-1);
		for(v=0; v<lt->pntsv; v++) {
			int vxt = (v==0 || v==lt->pntsv-1);
			for(u=0; u<lt->pntsu; u++) {
				int uxt = (u==0 || u==lt->pntsu-1);

				if(w && ((uxt || vxt) || !(lt->flag & LT_OUTSIDE))) {
					drawlattice__point(lt, dl, u, v, w-1, use_wcol);
					drawlattice__point(lt, dl, u, v, w, use_wcol);
				}
				if(v && ((uxt || wxt) || !(lt->flag & LT_OUTSIDE))) {
					drawlattice__point(lt, dl, u, v-1, w, use_wcol);
					drawlattice__point(lt, dl, u, v, w, use_wcol);
				}
				if(u && ((vxt || wxt) || !(lt->flag & LT_OUTSIDE))) {
					drawlattice__point(lt, dl, u-1, v, w, use_wcol);
					drawlattice__point(lt, dl, u, v, w, use_wcol);
				}
			}
		}
	}		
	glEnd();
	
	/* restoration for weight colors */
	if(use_wcol)
		glShadeModel(GL_FLAT);

	if(ob==G.obedit) {
		if(G.vd->zbuf) glDisable(GL_DEPTH_TEST);
		
		lattice_draw_verts(lt, dl, 0);
		lattice_draw_verts(lt, dl, 1);
		
		if(G.vd->zbuf) glEnable(GL_DEPTH_TEST); 
	}
}

/* ***************** ******************** */

static void mesh_foreachScreenVert__mapFunc(void *userData, int index, float *co, float *no_f, short *no_s)
{
	struct { void (*func)(void *userData, EditVert *eve, int x, int y, int index); void *userData; int clipVerts; float pmat[4][4], vmat[4][4]; } *data = userData;
	EditVert *eve = EM_get_vert_for_index(index);
	short s[2];

	if (eve->h==0) {
		if (data->clipVerts) {
			view3d_project_short_clip(curarea, co, s, data->pmat, data->vmat);
		} else {
			view3d_project_short_noclip(curarea, co, s, data->pmat);
		}

		data->func(data->userData, eve, s[0], s[1], index);
	}
}
void mesh_foreachScreenVert(void (*func)(void *userData, EditVert *eve, int x, int y, int index), void *userData, int clipVerts)
{
	struct { void (*func)(void *userData, EditVert *eve, int x, int y, int index); void *userData; int clipVerts; float pmat[4][4], vmat[4][4]; } data;
	DerivedMesh *dm = editmesh_get_derived_cage(CD_MASK_BAREMESH);

	data.func = func;
	data.userData = userData;
	data.clipVerts = clipVerts;

	view3d_get_object_project_mat(curarea, G.obedit, data.pmat, data.vmat);

	EM_init_index_arrays(1, 0, 0);
	dm->foreachMappedVert(dm, mesh_foreachScreenVert__mapFunc, &data);
	EM_free_index_arrays();

	dm->release(dm);
}

static void mesh_foreachScreenEdge__mapFunc(void *userData, int index, float *v0co, float *v1co)
{
	struct { void (*func)(void *userData, EditEdge *eed, int x0, int y0, int x1, int y1, int index); void *userData; int clipVerts; float pmat[4][4], vmat[4][4]; } *data = userData;
	EditEdge *eed = EM_get_edge_for_index(index);
	short s[2][2];

	if (eed->h==0) {
		if (data->clipVerts==1) {
			view3d_project_short_clip(curarea, v0co, s[0], data->pmat, data->vmat);
			view3d_project_short_clip(curarea, v1co, s[1], data->pmat, data->vmat);
		} else {
			view3d_project_short_noclip(curarea, v0co, s[0], data->pmat);
			view3d_project_short_noclip(curarea, v1co, s[1], data->pmat);

			if (data->clipVerts==2) {
                if (!(s[0][0]>=0 && s[0][1]>= 0 && s[0][0]<curarea->winx && s[0][1]<curarea->winy)) 
					if (!(s[1][0]>=0 && s[1][1]>= 0 && s[1][0]<curarea->winx && s[1][1]<curarea->winy)) 
						return;
			}
		}

		data->func(data->userData, eed, s[0][0], s[0][1], s[1][0], s[1][1], index);
	}
}
void mesh_foreachScreenEdge(void (*func)(void *userData, EditEdge *eed, int x0, int y0, int x1, int y1, int index), void *userData, int clipVerts)
{
	struct { void (*func)(void *userData, EditEdge *eed, int x0, int y0, int x1, int y1, int index); void *userData; int clipVerts; float pmat[4][4], vmat[4][4]; } data;
	DerivedMesh *dm = editmesh_get_derived_cage(CD_MASK_BAREMESH);

	data.func = func;
	data.userData = userData;
	data.clipVerts = clipVerts;

	view3d_get_object_project_mat(curarea, G.obedit, data.pmat, data.vmat);

	EM_init_index_arrays(0, 1, 0);
	dm->foreachMappedEdge(dm, mesh_foreachScreenEdge__mapFunc, &data);
	EM_free_index_arrays();

	dm->release(dm);
}

static void mesh_foreachScreenFace__mapFunc(void *userData, int index, float *cent, float *no)
{
	struct { void (*func)(void *userData, EditFace *efa, int x, int y, int index); void *userData; float pmat[4][4], vmat[4][4]; } *data = userData;
	EditFace *efa = EM_get_face_for_index(index);
	short s[2];

	if (efa && efa->h==0 && efa->fgonf!=EM_FGON) {
		view3d_project_short_clip(curarea, cent, s, data->pmat, data->vmat);

		data->func(data->userData, efa, s[0], s[1], index);
	}
}
void mesh_foreachScreenFace(void (*func)(void *userData, EditFace *efa, int x, int y, int index), void *userData)
{
	struct { void (*func)(void *userData, EditFace *efa, int x, int y, int index); void *userData; float pmat[4][4], vmat[4][4]; } data;
	DerivedMesh *dm = editmesh_get_derived_cage(CD_MASK_BAREMESH);

	data.func = func;
	data.userData = userData;

	view3d_get_object_project_mat(curarea, G.obedit, data.pmat, data.vmat);

	EM_init_index_arrays(0, 0, 1);
	dm->foreachMappedFaceCenter(dm, mesh_foreachScreenFace__mapFunc, &data);
	EM_free_index_arrays();

	dm->release(dm);
}

void nurbs_foreachScreenVert(void (*func)(void *userData, Nurb *nu, BPoint *bp, BezTriple *bezt, int beztindex, int x, int y), void *userData)
{
	float pmat[4][4], vmat[4][4];
	short s[2];
	Nurb *nu;
	int i;

	view3d_get_object_project_mat(curarea, G.obedit, pmat, vmat);

	for (nu= editNurb.first; nu; nu=nu->next) {
		if((nu->type & 7)==CU_BEZIER) {
			for (i=0; i<nu->pntsu; i++) {
				BezTriple *bezt = &nu->bezt[i];

				if(bezt->hide==0) {
					view3d_project_short_clip(curarea, bezt->vec[0], s, pmat, vmat);
					func(userData, nu, NULL, bezt, 0, s[0], s[1]);
					view3d_project_short_clip(curarea, bezt->vec[1], s, pmat, vmat);
					func(userData, nu, NULL, bezt, 1, s[0], s[1]);
					view3d_project_short_clip(curarea, bezt->vec[2], s, pmat, vmat);
					func(userData, nu, NULL, bezt, 2, s[0], s[1]);
				}
			}
		}
		else {
			for (i=0; i<nu->pntsu*nu->pntsv; i++) {
				BPoint *bp = &nu->bp[i];

				if(bp->hide==0) {
					view3d_project_short_clip(curarea, bp->vec, s, pmat, vmat);
					func(userData, nu, bp, NULL, -1, s[0], s[1]);
				}
			}
		}
	}
}

/* ************** DRAW MESH ****************** */

/* First section is all the "simple" draw routines, 
 * ones that just pass some sort of primitive to GL,
 * with perhaps various options to control lighting,
 * color, etc.
 *
 * These routines should not have user interface related
 * logic!!!
 */

static void draw_dm_face_normals__mapFunc(void *userData, int index, float *cent, float *no)
{
	EditFace *efa = EM_get_face_for_index(index);

	if (efa->h==0 && efa->fgonf!=EM_FGON) {
		glVertex3fv(cent);
		glVertex3f(	cent[0] + no[0]*G.scene->editbutsize,
					cent[1] + no[1]*G.scene->editbutsize,
					cent[2] + no[2]*G.scene->editbutsize);
	}
}
static void draw_dm_face_normals(DerivedMesh *dm) {
	glBegin(GL_LINES);
	dm->foreachMappedFaceCenter(dm, draw_dm_face_normals__mapFunc, 0);
	glEnd();
}

static void draw_dm_face_centers__mapFunc(void *userData, int index, float *cent, float *no)
{
	EditFace *efa = EM_get_face_for_index(index);
	int sel = *((int*) userData);

	if (efa->h==0 && efa->fgonf!=EM_FGON && (efa->f&SELECT)==sel) {
		bglVertex3fv(cent);
	}
}
static void draw_dm_face_centers(DerivedMesh *dm, int sel)
{
	bglBegin(GL_POINTS);
	dm->foreachMappedFaceCenter(dm, draw_dm_face_centers__mapFunc, &sel);
	bglEnd();
}

static void draw_dm_vert_normals__mapFunc(void *userData, int index, float *co, float *no_f, short *no_s)
{
	EditVert *eve = EM_get_vert_for_index(index);

	if (eve->h==0) {
		glVertex3fv(co);

		if (no_f) {
			glVertex3f(	co[0] + no_f[0]*G.scene->editbutsize,
						co[1] + no_f[1]*G.scene->editbutsize,
						co[2] + no_f[2]*G.scene->editbutsize);
		} else {
			glVertex3f(	co[0] + no_s[0]*G.scene->editbutsize/32767.0f,
						co[1] + no_s[1]*G.scene->editbutsize/32767.0f,
						co[2] + no_s[2]*G.scene->editbutsize/32767.0f);
		}
	}
}
static void draw_dm_vert_normals(DerivedMesh *dm) {
	glBegin(GL_LINES);
	dm->foreachMappedVert(dm, draw_dm_vert_normals__mapFunc, NULL);
	glEnd();
}

	/* Draw verts with color set based on selection */
static void draw_dm_verts__mapFunc(void *userData, int index, float *co, float *no_f, short *no_s)
{
	EditVert *eve = EM_get_vert_for_index(index);
	int sel = *((int*) userData);

	if (eve->h==0 && (eve->f&SELECT)==sel) {
		bglVertex3fv(co);
	}
}
static void draw_dm_verts(DerivedMesh *dm, int sel)
{
	bglBegin(GL_POINTS);
	dm->foreachMappedVert(dm, draw_dm_verts__mapFunc, &sel);
	bglEnd();
}

	/* Draw edges with color set based on selection */
static int draw_dm_edges_sel__setDrawOptions(void *userData, int index)
{
	EditEdge *eed = EM_get_edge_for_index(index);
	unsigned char **cols = userData;

	if (eed->h==0) {
		glColor4ubv(cols[(eed->f&SELECT)?1:0]);
		return 1;
	} else {
		return 0;
	}
}
static void draw_dm_edges_sel(DerivedMesh *dm, unsigned char *baseCol, unsigned char *selCol) 
{
	unsigned char *cols[2];
	cols[0] = baseCol;
	cols[1] = selCol;
	dm->drawMappedEdges(dm, draw_dm_edges_sel__setDrawOptions, cols);
}

	/* Draw edges */
static int draw_dm_edges__setDrawOptions(void *userData, int index)
{
	return EM_get_edge_for_index(index)->h==0;
}
static void draw_dm_edges(DerivedMesh *dm) 
{
	dm->drawMappedEdges(dm, draw_dm_edges__setDrawOptions, NULL);
}

	/* Draw edges with color interpolated based on selection */
static int draw_dm_edges_sel_interp__setDrawOptions(void *userData, int index)
{
	return EM_get_edge_for_index(index)->h==0;
}
static void draw_dm_edges_sel_interp__setDrawInterpOptions(void *userData, int index, float t)
{
	EditEdge *eed = EM_get_edge_for_index(index);
	unsigned char **cols = userData;
	unsigned char *col0 = cols[(eed->v1->f&SELECT)?1:0];
	unsigned char *col1 = cols[(eed->v2->f&SELECT)?1:0];

	glColor4ub(	col0[0] + (col1[0]-col0[0])*t,
				col0[1] + (col1[1]-col0[1])*t,
				col0[2] + (col1[2]-col0[2])*t,
				col0[3] + (col1[3]-col0[3])*t);
}
static void draw_dm_edges_sel_interp(DerivedMesh *dm, unsigned char *baseCol, unsigned char *selCol)
{
	unsigned char *cols[2];
	cols[0] = baseCol;
	cols[1] = selCol;
	dm->drawMappedEdgesInterp(dm, draw_dm_edges_sel_interp__setDrawOptions, draw_dm_edges_sel_interp__setDrawInterpOptions, cols);
}

	/* Draw only seam edges */
static int draw_dm_edges_seams__setDrawOptions(void *userData, int index)
{
	EditEdge *eed = EM_get_edge_for_index(index);

	return (eed->h==0 && eed->seam);
}
static void draw_dm_edges_seams(DerivedMesh *dm)
{
	dm->drawMappedEdges(dm, draw_dm_edges_seams__setDrawOptions, NULL);
}

	/* Draw only sharp edges */
static int draw_dm_edges_sharp__setDrawOptions(void *userData, int index)
{
	EditEdge *eed = EM_get_edge_for_index(index);

	return (eed->h==0 && eed->sharp);
}
static void draw_dm_edges_sharp(DerivedMesh *dm)
{
	dm->drawMappedEdges(dm, draw_dm_edges_sharp__setDrawOptions, NULL);
}


	/* Draw faces with color set based on selection */
static int draw_dm_faces_sel__setDrawOptions(void *userData, int index, int *drawSmooth_r)
{
	EditFace *efa = EM_get_face_for_index(index);
	unsigned char **cols = userData;

	if (efa->h==0) {
		glColor4ubv(cols[(efa->f&SELECT)?1:0]);
		return 1;
	} else {
		return 0;
	}
}
static void draw_dm_faces_sel(DerivedMesh *dm, unsigned char *baseCol, unsigned char *selCol) 
{
	unsigned char *cols[2];
	cols[0] = baseCol;
	cols[1] = selCol;
	dm->drawMappedFaces(dm, draw_dm_faces_sel__setDrawOptions, cols, 0);
}

static int draw_dm_creases__setDrawOptions(void *userData, int index)
{
	EditEdge *eed = EM_get_edge_for_index(index);

	if (eed->h==0 && eed->crease!=0.0) {
		BIF_ThemeColorBlend(TH_WIRE, TH_EDGE_SELECT, eed->crease);
		return 1;
	} else {
		return 0;
	}
}
static void draw_dm_creases(DerivedMesh *dm)
{
	glLineWidth(3.0);
	dm->drawMappedEdges(dm, draw_dm_creases__setDrawOptions, NULL);
	glLineWidth(1.0);
}

/* Second section of routines: Combine first sets to form fancy
 * drawing routines (for example rendering twice to get overlays).
 *
 * Also includes routines that are basic drawing but are too
 * specialized to be split out (like drawing creases or measurements).
 */

/* EditMesh drawing routines*/

static void draw_em_fancy_verts(EditMesh *em, DerivedMesh *cageDM)
{
	int sel;

	if(G.vd->zbuf) glDepthMask(0);		// disable write in zbuffer, zbuf select

	for (sel=0; sel<2; sel++) {
		char col[4], fcol[4];
		int pass;

		BIF_GetThemeColor3ubv(sel?TH_VERTEX_SELECT:TH_VERTEX, col);
		BIF_GetThemeColor3ubv(sel?TH_FACE_DOT:TH_WIRE, fcol);

		for (pass=0; pass<2; pass++) {
			float size = BIF_GetThemeValuef(TH_VERTEX_SIZE);
			float fsize = BIF_GetThemeValuef(TH_FACEDOT_SIZE);

			if (pass==0) {
				if(G.vd->zbuf && !(G.vd->flag&V3D_ZBUF_SELECT)) {
					glDisable(GL_DEPTH_TEST);
						
					glEnable(GL_BLEND);
				} else {
					continue;
				}

				size = (size>2.1?size/2.0:size);
				fsize = (fsize>2.1?fsize/2.0:fsize);
				col[3] = fcol[3] = 100;
			} else {
				col[3] = fcol[3] = 255;
			}
				
			if(G.scene->selectmode & SCE_SELECT_VERTEX) {
				glPointSize(size);
				glColor4ubv((GLubyte *)col);
				draw_dm_verts(cageDM, sel);
			}
			
			if(G.scene->selectmode & SCE_SELECT_FACE) {
				glPointSize(fsize);
				glColor4ubv((GLubyte *)fcol);
				draw_dm_face_centers(cageDM, sel);
			}
			
			if (pass==0) {
				glDisable(GL_BLEND);
				glEnable(GL_DEPTH_TEST);
			}
		}
	}

	if(G.vd->zbuf) glDepthMask(1);
	glPointSize(1.0);
}

static void draw_em_fancy_edges(DerivedMesh *cageDM)
{
	int pass;
	unsigned char wire[4], sel[4];

	/* since this function does transparant... */
	BIF_GetThemeColor3ubv(TH_EDGE_SELECT, (char *)sel);
	BIF_GetThemeColor3ubv(TH_WIRE, (char *)wire);

	for (pass=0; pass<2; pass++) {
			/* show wires in transparant when no zbuf clipping for select */
		if (pass==0) {
			if (G.vd->zbuf && (G.vd->flag & V3D_ZBUF_SELECT)==0) {
				glEnable(GL_BLEND);
				glDisable(GL_DEPTH_TEST);

				wire[3] = sel[3] = 85;
			} else {
				continue;
			}
		} else {
			wire[3] = sel[3] = 255;
		}

		if(G.scene->selectmode == SCE_SELECT_FACE) {
			draw_dm_edges_sel(cageDM, wire, sel);
		}	
		else if( (G.f & G_DRAWEDGES) || (G.scene->selectmode & SCE_SELECT_EDGE) ) {	
			if(cageDM->drawMappedEdgesInterp && (G.scene->selectmode & SCE_SELECT_VERTEX)) {
				glShadeModel(GL_SMOOTH);
				draw_dm_edges_sel_interp(cageDM, wire, sel);
				glShadeModel(GL_FLAT);
			} else {
				draw_dm_edges_sel(cageDM, wire, sel);
			}
		}
		else {
			glColor4ubv(wire);
			draw_dm_edges(cageDM);
		}

		if (pass==0) {
			glDisable(GL_BLEND);
			glEnable(GL_DEPTH_TEST);
		}
	}
}	

#ifdef WITH_VERSE
/*
 * draw some debug info about verse mesh (vertex indexes,
 * face indexes, status of )
 */
static void draw_verse_debug(Object *ob, EditMesh *em)
{
	struct EditVert *eve=NULL;
	struct EditFace *efa=NULL;
	float v1[3], v2[3], v3[3], v4[3], fvec[3], col[3];
	char val[32];
	
	if(G.vd->zbuf && (G.vd->flag & V3D_ZBUF_SELECT)==0)
		glDisable(GL_DEPTH_TEST);

	if(G.vd->zbuf) bglPolygonOffset(5.0);

	BIF_GetThemeColor3fv(TH_TEXT, col);
	/* make color a bit more red */
	if(col[0]> 0.5) {col[1]*=0.7; col[2]*= 0.7;}
	else col[0]= col[0]*0.7 + 0.3;
	glColor3fv(col);

	/* draw IDs of verse vertexes */
	for(eve = em->verts.first; eve; eve = eve->next) {
		if(eve->vvert) {
			VecLerpf(fvec, ob->loc, eve->co, 1.1);
			glRasterPos3f(fvec[0], fvec[1], fvec[2]);

			sprintf(val, "%d", ((VerseVert*)eve->vvert)->id);
			BMF_DrawString(G.fonts, val);
		}
	}

	/* draw IDs of verse faces */
	for(efa = em->faces.first; efa; efa = efa->next) {
		if(efa->vface) {
			VECCOPY(v1, efa->v1->co);
			VECCOPY(v2, efa->v2->co);
			VECCOPY(v3, efa->v3->co);
			if(efa->v4) {
				VECCOPY(v4, efa->v4->co);
				glRasterPos3f(0.25*(v1[0]+v2[0]+v3[0]+v4[0]),
						0.25*(v1[1]+v2[1]+v3[1]+v4[1]),
						0.25*(v1[2]+v2[2]+v3[2]+v4[2]));
			}
			else {
				glRasterPos3f((v1[0]+v2[0]+v3[0])/3,
						(v1[1]+v2[1]+v3[1])/3,
						(v1[2]+v2[2]+v3[2])/3);
			}
			
			sprintf(val, "%d", ((VerseFace*)efa->vface)->id);
			BMF_DrawString(G.fonts, val);
			
		}
	}
	
	if(G.vd->zbuf) {
		glEnable(GL_DEPTH_TEST);
		bglPolygonOffset(0.0);
	}
}
#endif

static void draw_em_measure_stats(Object *ob, EditMesh *em)
{
	EditEdge *eed;
	EditFace *efa;
	float v1[3], v2[3], v3[3], v4[3];
	float fvec[3];
	char val[32]; /* Stores the measurement display text here */
	char conv_float[5]; /* Use a float conversion matching the grid size */
	float area, col[3]; /* area of the face,  color of the text to draw */
	
	/* make the precission of the pronted value proportionate to the gridsize */
	if ((G.vd->grid) < 0.01)
		strcpy(conv_float, "%.6f");
	else if ((G.vd->grid) < 0.1)
		strcpy(conv_float, "%.5f");
	else if ((G.vd->grid) < 1.0)
		strcpy(conv_float, "%.4f");
	else if ((G.vd->grid) < 10.0)
		strcpy(conv_float, "%.3f");
	else
		strcpy(conv_float, "%.2f");
	
	
	if(G.vd->zbuf && (G.vd->flag & V3D_ZBUF_SELECT)==0)
		glDisable(GL_DEPTH_TEST);

	if(G.vd->zbuf) bglPolygonOffset(5.0);
	
	if(G.f & G_DRAW_EDGELEN) {
		BIF_GetThemeColor3fv(TH_TEXT, col);
		/* make color a bit more red */
		if(col[0]> 0.5) {col[1]*=0.7; col[2]*= 0.7;}
		else col[0]= col[0]*0.7 + 0.3;
		glColor3fv(col);
		
		for(eed= em->edges.first; eed; eed= eed->next) {
			/* draw non fgon edges, or selected edges, or edges next to selected verts while draging */
			if((eed->h != EM_FGON) && ((eed->f & SELECT) || (G.moving && ((eed->v1->f & SELECT) || (eed->v2->f & SELECT)) ))) {
				VECCOPY(v1, eed->v1->co);
				VECCOPY(v2, eed->v2->co);
				
				glRasterPos3f( 0.5*(v1[0]+v2[0]),  0.5*(v1[1]+v2[1]),  0.5*(v1[2]+v2[2]));
				
				if(G.vd->flag & V3D_GLOBAL_STATS) {
					Mat4MulVecfl(ob->obmat, v1);
					Mat4MulVecfl(ob->obmat, v2);
				}
				
				sprintf(val, conv_float, VecLenf(v1, v2));
				BMF_DrawString( G.fonts, val);
			}
		}
	}

	if(G.f & G_DRAW_FACEAREA) {
		extern int faceselectedOR(EditFace *efa, int flag);		// editmesh.h shouldn't be in this file... ok for now?
		
		BIF_GetThemeColor3fv(TH_TEXT, col);
		/* make color a bit more green */
		if(col[1]> 0.5) {col[0]*=0.7; col[2]*= 0.7;}
		else col[1]= col[1]*0.7 + 0.3;
		glColor3fv(col);
		
		for(efa= em->faces.first; efa; efa= efa->next) {
			if((efa->f & SELECT) || (G.moving && faceselectedOR(efa, SELECT)) ) {
				VECCOPY(v1, efa->v1->co);
				VECCOPY(v2, efa->v2->co);
				VECCOPY(v3, efa->v3->co);
				if (efa->v4) {
					VECCOPY(v4, efa->v4->co);
				}
				if(G.vd->flag & V3D_GLOBAL_STATS) {
					Mat4MulVecfl(ob->obmat, v1);
					Mat4MulVecfl(ob->obmat, v2);
					Mat4MulVecfl(ob->obmat, v3);
					if (efa->v4) Mat4MulVecfl(ob->obmat, v4);
				}
				
				if (efa->v4)
					area=  AreaQ3Dfl(v1, v2, v3, v4);
				else
					area = AreaT3Dfl(v1, v2, v3);

				sprintf(val, conv_float, area);
				glRasterPos3fv(efa->cent);
				BMF_DrawString( G.fonts, val);
			}
		}
	}

	if(G.f & G_DRAW_EDGEANG) {
		EditEdge *e1, *e2, *e3, *e4;
		
		BIF_GetThemeColor3fv(TH_TEXT, col);
		/* make color a bit more blue */
		if(col[2]> 0.5) {col[0]*=0.7; col[1]*= 0.7;}
		else col[2]= col[2]*0.7 + 0.3;
		glColor3fv(col);
		
		for(efa= em->faces.first; efa; efa= efa->next) {
			VECCOPY(v1, efa->v1->co);
			VECCOPY(v2, efa->v2->co);
			VECCOPY(v3, efa->v3->co);
			if(efa->v4) {
				VECCOPY(v4, efa->v4->co); 
			}
			else {
				VECCOPY(v4, v3);
			}
			if(G.vd->flag & V3D_GLOBAL_STATS) {
				Mat4MulVecfl(ob->obmat, v1);
				Mat4MulVecfl(ob->obmat, v2);
				Mat4MulVecfl(ob->obmat, v3);
				if (efa->v4) Mat4MulVecfl(ob->obmat, v4);
			}
			
			e1= efa->e1;
			e2= efa->e2;
			e3= efa->e3;
			if(efa->e4) e4= efa->e4; else e4= e3;
			
			/* Calculate the angles */
				
			if( (e4->f & e1->f & SELECT) || (G.moving && (efa->v1->f & SELECT)) ) {
				/* Vec 1 */
				sprintf(val,"%.3f", VecAngle3(v4, v1, v2));
				VecLerpf(fvec, efa->cent, efa->v1->co, 0.8);
				glRasterPos3fv(fvec);
				BMF_DrawString( G.fonts, val);
			}
			if( (e1->f & e2->f & SELECT) || (G.moving && (efa->v2->f & SELECT)) ) {
				/* Vec 2 */
				sprintf(val,"%.3f", VecAngle3(v1, v2, v3));
				VecLerpf(fvec, efa->cent, efa->v2->co, 0.8);
				glRasterPos3fv(fvec);
				BMF_DrawString( G.fonts, val);
			}
			if( (e2->f & e3->f & SELECT) || (G.moving && (efa->v3->f & SELECT)) ) {
				/* Vec 3 */
				if(efa->v4) 
					sprintf(val,"%.3f", VecAngle3(v2, v3, v4));
				else
					sprintf(val,"%.3f", VecAngle3(v2, v3, v1));
				VecLerpf(fvec, efa->cent, efa->v3->co, 0.8);
				glRasterPos3fv(fvec);
				BMF_DrawString( G.fonts, val);
			}
				/* Vec 4 */
			if(efa->v4) {
				if( (e3->f & e4->f & SELECT) || (G.moving && (efa->v4->f & SELECT)) ) {
					sprintf(val,"%.3f", VecAngle3(v3, v4, v1));
					VecLerpf(fvec, efa->cent, efa->v4->co, 0.8);
					glRasterPos3fv(fvec);
					BMF_DrawString( G.fonts, val);
				}
			}
		}
	}    
	
	if(G.vd->zbuf) {
		glEnable(GL_DEPTH_TEST);
		bglPolygonOffset(0.0);
	}
}

static int draw_em_fancy__setFaceOpts(void *userData, int index, int *drawSmooth_r)
{
	EditFace *efa = EM_get_face_for_index(index);

	if (efa->h==0) {
		set_gl_material(efa->mat_nr+1);
		return 1;
	} else {
		return 0;
	}
}
static void draw_em_fancy(Object *ob, EditMesh *em, DerivedMesh *cageDM, DerivedMesh *finalDM, int dt)
{
	Mesh *me = ob->data;

	EM_init_index_arrays(1, 1, 1);

	if(dt>OB_WIRE) {
		glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, me->flag & ME_TWOSIDED);

		glEnable(GL_LIGHTING);
		glFrontFace((ob->transflag&OB_NEG_SCALE)?GL_CW:GL_CCW);

		finalDM->drawMappedFaces(finalDM, draw_em_fancy__setFaceOpts, NULL, 0);

		glFrontFace(GL_CCW);
		glDisable(GL_LIGHTING);
		
		// Setup for drawing wire over, disable zbuffer
		// write to show selected edge wires better
		BIF_ThemeColor(TH_WIRE);

		bglPolygonOffset(1.0);
		glDepthMask(0);
	} 
	else {
		if (cageDM!=finalDM) {
			BIF_ThemeColorBlend(TH_WIRE, TH_BACK, 0.7);
			finalDM->drawEdges(finalDM, 1);
		}
	}
	
	if((G.f & (G_FACESELECT+G_DRAWFACES))) {	/* transp faces */
		unsigned char col1[4], col2[4];
			
		BIF_GetThemeColor4ubv(TH_FACE, (char *)col1);
		BIF_GetThemeColor4ubv(TH_FACE_SELECT, (char *)col2);
		
		glEnable(GL_BLEND);
		glDepthMask(0);		// disable write in zbuffer, needed for nice transp
		
		draw_dm_faces_sel(cageDM, col1, col2);

		glDisable(GL_BLEND);
		glDepthMask(1);		// restore write in zbuffer
	}

	/* here starts all fancy draw-extra over */

	if(G.f & G_DRAWSEAMS) {
		BIF_ThemeColor(TH_EDGE_SEAM);
		glLineWidth(2);

		draw_dm_edges_seams(cageDM);

		glColor3ub(0,0,0);
		glLineWidth(1);
	}
	
	if(G.f & G_DRAWSHARP) {
		BIF_ThemeColor(TH_EDGE_SHARP);
		glLineWidth(2);

		draw_dm_edges_sharp(cageDM);

		glColor3ub(0,0,0);
		glLineWidth(1);
	}

	if(G.f & G_DRAWCREASES) {
		draw_dm_creases(cageDM);
	}

	draw_em_fancy_edges(cageDM);

	if(ob==G.obedit) {
		retopo_matrix_update(G.vd);

		draw_em_fancy_verts(em, cageDM);

		if(G.f & G_DRAWNORMALS) {
			BIF_ThemeColor(TH_NORMAL);
			draw_dm_face_normals(cageDM);
		}
		if(G.f & G_DRAW_VNORMALS) {
			BIF_ThemeColor(TH_NORMAL);
			draw_dm_vert_normals(cageDM);
		}

		if(G.f & (G_DRAW_EDGELEN|G_DRAW_FACEAREA|G_DRAW_EDGEANG))
			draw_em_measure_stats(ob, em);
#ifdef WITH_VERSE
		if(em->vnode && (G.f & G_DRAW_VERSE_DEBUG))
			draw_verse_debug(ob, em);
#endif
	}

	if(dt>OB_WIRE) {
		glDepthMask(1);
		bglPolygonOffset(0.0);
	}

	EM_free_index_arrays();
}

/* Mesh drawing routines */

static void draw_mesh_object_outline(Object *ob, DerivedMesh *dm)
{
	
	if(G.vd->transp==0) {	// not when we draw the transparent pass
		glLineWidth(2.0);
		glDepthMask(0);
		
		/* if transparent, we cannot draw the edges for solid select... edges have no material info.
		   drawFacesSolid() doesn't draw the transparent faces */
		if(ob->dtx & OB_DRAWTRANSP) {
			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); 
			dm->drawFacesSolid(dm, set_gl_material);
			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		}
		else {
			dm->drawEdges(dm, 0);
		}
					
		glLineWidth(1.0);
		glDepthMask(1);
	}
}

static int wpaint__setSolidDrawOptions(void *userData, int index, int *drawSmooth_r)
{
	*drawSmooth_r = 1;
	return 1;
}

static void draw_mesh_fancy(Base *base, int dt, int flag)
{
	Object *ob= base->object;
	Mesh *me = ob->data;
	Material *ma= give_current_material(ob, 1);
	int hasHaloMat = (ma && (ma->mode&MA_HALO));
	int draw_wire = ob->dtx&OB_DRAWWIRE;
	int totvert, totedge, totface;
	DispList *dl;
	DerivedMesh *dm= mesh_get_derived_final(ob, get_viewedit_datamask());

	if(!dm)
		return;

#ifdef WITH_VERSE
	if(me->vnode) {
		struct VNode *vnode = (VNode*)me->vnode;
		struct VLayer *vert_vlayer = find_verse_layer_type((VGeomData*)vnode->data, VERTEX_LAYER);
		struct VLayer *face_vlayer = find_verse_layer_type((VGeomData*)vnode->data, POLYGON_LAYER);

		if(vert_vlayer) totvert = vert_vlayer->dl.da.count;
		else totvert = 0;
		totedge = 0;	/* total count of edge needn't to be zero, but verse doesn't know edges */
		if(face_vlayer) totface = face_vlayer->dl.da.count;
		else totface = 0;
	}
	else {
		totvert = dm->getNumVerts(dm);
		totedge = dm->getNumEdges(dm);
		totface = dm->getNumFaces(dm);
	}
#else
	totvert = dm->getNumVerts(dm);
	totedge = dm->getNumEdges(dm);
	totface = dm->getNumFaces(dm);
#endif
	
	/* vertexpaint, faceselect wants this, but it doesnt work for shaded? */
	if(dt!=OB_SHADED)
		glFrontFace((ob->transflag&OB_NEG_SCALE)?GL_CW:GL_CCW);

		// Unwanted combination.
	if (ob==OBACT && (G.f&G_FACESELECT)) draw_wire = 0;

	if(dt==OB_BOUNDBOX) {
		draw_bounding_volume(ob);
	}
	else if(hasHaloMat || (totface==0 && totedge==0)) {
		glPointSize(1.5);
		dm->drawVerts(dm);
		glPointSize(1.0);
	}
	else if(dt==OB_WIRE || totface==0) {
		draw_wire = 1;
	}
	else if( (ob==OBACT && (G.f & (G_FACESELECT|G_TEXTUREPAINT))) || (G.vd->drawtype==OB_TEXTURE && dt>OB_SOLID)) {
		
		if ((G.vd->flag&V3D_SELECT_OUTLINE) && (base->flag&SELECT) && !(G.f&(G_FACESELECT|G_PICKSEL)) && !draw_wire) {
			draw_mesh_object_outline(ob, dm);
		}

		draw_tface_mesh(ob, ob->data, dt);
	}
	else if(dt==OB_SOLID ) {
		
		if ((G.vd->flag&V3D_SELECT_OUTLINE) && (base->flag&SELECT) && !draw_wire) {
			draw_mesh_object_outline(ob, dm);
		}

		glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, me->flag & ME_TWOSIDED );

		glEnable(GL_LIGHTING);
		glFrontFace((ob->transflag&OB_NEG_SCALE)?GL_CW:GL_CCW);

		dm->drawFacesSolid(dm, set_gl_material);

		glFrontFace(GL_CCW);
		glDisable(GL_LIGHTING);

		if(base->flag & SELECT) {
			BIF_ThemeColor((ob==OBACT)?TH_ACTIVE:TH_SELECT);
		} else {
			BIF_ThemeColor(TH_WIRE);
		}
		dm->drawLooseEdges(dm);
	}
	else if(dt==OB_SHADED) {
		int do_draw= 1;	/* to resolve all G.f settings below... */
		
		if(ob==OBACT) {
			do_draw= 0;
			if( (G.f & G_WEIGHTPAINT)) {
				set_gl_material(0);		/* enforce defmaterial settings */
				
				/* but set default spec */
				glColorMaterial(GL_FRONT_AND_BACK, GL_SPECULAR);
				glEnable(GL_COLOR_MATERIAL);	/* according manpages needed */
				glColor3ub(120, 120, 120);
				glDisable(GL_COLOR_MATERIAL);
				/* diffuse */
				glColorMaterial(GL_FRONT_AND_BACK, GL_DIFFUSE);
				glEnable(GL_LIGHTING);
				glEnable(GL_COLOR_MATERIAL);

				dm->drawMappedFaces(dm, wpaint__setSolidDrawOptions, me->mface, 1);
				glDisable(GL_COLOR_MATERIAL);
				glDisable(GL_LIGHTING);
			}
			else if((G.f & (G_VERTEXPAINT+G_TEXTUREPAINT)) && me->mcol) {
				dm->drawMappedFaces(dm, wpaint__setSolidDrawOptions, NULL, 1);
			}
			else if((G.f & (G_VERTEXPAINT+G_TEXTUREPAINT)) && me->mtface) {
				glColor3f(1.0f, 1.0f, 1.0f);
				dm->drawMappedFaces(dm, wpaint__setSolidDrawOptions, NULL, 0);
			}
			else do_draw= 1;
		}
		if(do_draw) {
			dl = ob->disp.first;
			if (!dl || !dl->col1) {
				/* release and reload derivedmesh because it might be freed in
				   shadeDispList due to a different datamask */
				dm->release(dm);
				shadeDispList(base);
				dl = find_displist(&ob->disp, DL_VERTCOL);
			    dm= mesh_get_derived_final(ob, get_viewedit_datamask());
			}

			if ((G.vd->flag&V3D_SELECT_OUTLINE) && (base->flag&SELECT) && !draw_wire) {
				draw_mesh_object_outline(ob, dm);
			}

				/* False for dupliframe objects */
			if (dl) {
				unsigned int *obCol1 = dl->col1;
				unsigned int *obCol2 = dl->col2;

				dm->drawFacesColored(dm, me->flag&ME_TWOSIDED, (unsigned char*) obCol1, (unsigned char*) obCol2);
			}

			if(base->flag & SELECT) {
				BIF_ThemeColor((ob==OBACT)?TH_ACTIVE:TH_SELECT);
			} else {
				BIF_ThemeColor(TH_WIRE);
			}
			dm->drawLooseEdges(dm);
		}
	}
	
	/* set default draw color back for wire or for draw-extra later on */
	if (dt!=OB_WIRE) {
		if(base->flag & SELECT) {
			if(ob==OBACT && ob->flag & OB_FROMGROUP) 
				BIF_ThemeColor(TH_GROUP_ACTIVE);
			else if(ob->flag & OB_FROMGROUP) 
				BIF_ThemeColorShade(TH_GROUP_ACTIVE, -16);
			else if(flag!=DRAW_CONSTCOLOR)
				BIF_ThemeColor((ob==OBACT)?TH_ACTIVE:TH_SELECT);
			else
				glColor3ub(80,80,80);
		} else {
			if (ob->flag & OB_FROMGROUP) 
				BIF_ThemeColor(TH_GROUP);
			else {
				if(ob->dtx & OB_DRAWWIRE && flag==DRAW_CONSTCOLOR)
					glColor3ub(80,80,80);
				else
					BIF_ThemeColor(TH_WIRE);
			}
		}
	}
	if (draw_wire) {
			/* If drawing wire and drawtype is not OB_WIRE then we are
				* overlaying the wires.
				*/

		if (dt!=OB_WIRE) {
			bglPolygonOffset(1.0);
			glDepthMask(0);	// disable write in zbuffer, selected edge wires show better
		}

		dm->drawEdges(dm, (dt==OB_WIRE || totface==0));

		if (dt!=OB_WIRE) {
			glDepthMask(1);
			bglPolygonOffset(0.0);
		}
	}

	dm->release(dm);
}

/* returns 1 if nothing was drawn, for detecting to draw an object center */
static int draw_mesh_object(Base *base, int dt, int flag)
{
	Object *ob= base->object;
	Mesh *me= ob->data;
	int has_alpha= 0, drawlinked= 0, retval= 0;
	
	if(G.obedit && ob!=G.obedit && ob->data==G.obedit->data) {
		if(ob_get_key(ob));
		else drawlinked= 1;
	}
	
	if(ob==G.obedit || drawlinked) {
		DerivedMesh *finalDM, *cageDM;
		
		if (G.obedit!=ob)
			finalDM = cageDM = editmesh_get_derived_base();
		else
			cageDM = editmesh_get_derived_cage_and_final(&finalDM,
			                                get_viewedit_datamask());

		if(dt>OB_WIRE) init_gl_materials(ob, 0);	// no transp in editmode, the fancy draw over goes bad then
		draw_em_fancy(ob, G.editMesh, cageDM, finalDM, dt);

		if (cageDM != finalDM)
			cageDM->release(cageDM);
		finalDM->release(finalDM);
	}
	else if(!G.obedit && (G.f & G_SCULPTMODE) &&(G.scene->sculptdata.flags & SCULPT_DRAW_FAST) &&
	        OBACT==ob && !sculpt_modifiers_active(ob)) {
		sculptmode_draw_mesh(0);
	}
	else {
		/* don't create boundbox here with mesh_get_bb(), the derived system will make it, puts deformed bb's OK */
		
		if(me->totface<=4 || boundbox_clip(ob->obmat, me->bb)) {
			if(dt==OB_SOLID) has_alpha= init_gl_materials(ob, (base->flag & OB_FROMDUPLI)==0);
			draw_mesh_fancy(base, dt, flag);
			
			if(me->totvert==0) retval= 1;
		}
	}
	
	/* init_gl_materials did the proper checking if this is needed */
	if(has_alpha) add_view3d_after(G.vd, base, V3D_TRANSP);
	
	return retval;
}

/* ************** DRAW DISPLIST ****************** */

static int draw_index_wire= 1;
static int index3_nors_incr= 1;

/* returns 1 when nothing was drawn */
static int drawDispListwire(ListBase *dlbase)
{
	DispList *dl;
	int parts, nr;
	float *data;

	if(dlbase==NULL) return 1;
	
	glDisableClientState(GL_NORMAL_ARRAY);
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); 

	for(dl= dlbase->first; dl; dl= dl->next) {
		if(dl->parts==0 || dl->nr==0)
			continue;
		
		data= dl->verts;
	
		switch(dl->type) {
		case DL_SEGM:
			
			glVertexPointer(3, GL_FLOAT, 0, data);
			
			for(parts=0; parts<dl->parts; parts++)
				glDrawArrays(GL_LINE_STRIP, parts*dl->nr, dl->nr);
				
			break;
		case DL_POLY:
			
			glVertexPointer(3, GL_FLOAT, 0, data);
			
			for(parts=0; parts<dl->parts; parts++)
				glDrawArrays(GL_LINE_LOOP, parts*dl->nr, dl->nr);
			
			break;
		case DL_SURF:
			
			glVertexPointer(3, GL_FLOAT, 0, data);
			
			for(parts=0; parts<dl->parts; parts++) {
				if(dl->flag & DL_CYCL_U) 
					glDrawArrays(GL_LINE_LOOP, parts*dl->nr, dl->nr);
				else
					glDrawArrays(GL_LINE_STRIP, parts*dl->nr, dl->nr);
			}
			
			for(nr=0; nr<dl->nr; nr++) {
				int ofs= 3*dl->nr;
				
				data= (  dl->verts )+3*nr;
				parts= dl->parts;

				if(dl->flag & DL_CYCL_V) glBegin(GL_LINE_LOOP);
				else glBegin(GL_LINE_STRIP);
				
				while(parts--) {
					glVertex3fv(data);
					data+=ofs;
				}
				glEnd();
				
				/* (ton) this code crashes for me when resolv is 86 or higher... no clue */
//				glVertexPointer(3, GL_FLOAT, sizeof(float)*3*dl->nr, data + 3*nr);
//				if(dl->flag & DL_CYCL_V) 
//					glDrawArrays(GL_LINE_LOOP, 0, dl->parts);
//				else
//					glDrawArrays(GL_LINE_STRIP, 0, dl->parts);
			}
			break;
			
		case DL_INDEX3:
			if(draw_index_wire) {
				glVertexPointer(3, GL_FLOAT, 0, dl->verts);
				glDrawElements(GL_TRIANGLES, 3*dl->parts, GL_UNSIGNED_INT, dl->index);
			}
			break;
			
		case DL_INDEX4:
			if(draw_index_wire) {
				glVertexPointer(3, GL_FLOAT, 0, dl->verts);
				glDrawElements(GL_QUADS, 4*dl->parts, GL_UNSIGNED_INT, dl->index);
			}
			break;
		}
	}
	
	glEnableClientState(GL_NORMAL_ARRAY);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL); 
	
	return 0;
}

static void drawDispListsolid(ListBase *lb, Object *ob)
{
	DispList *dl;
	float *data, curcol[4];
	float *ndata;
	
	if(lb==NULL) return;
	
	/* for drawing wire */
	glGetFloatv(GL_CURRENT_COLOR, curcol);

	glEnable(GL_LIGHTING);
	
	if(ob->transflag & OB_NEG_SCALE) glFrontFace(GL_CW);
	else glFrontFace(GL_CCW);
	
	if(ob->type==OB_MBALL) {	// mball always smooth shaded
		glShadeModel(GL_SMOOTH);
	}
	
	dl= lb->first;
	while(dl) {
		data= dl->verts;
		ndata= dl->nors;

		switch(dl->type) {
		case DL_SEGM:
			if(ob->type==OB_SURF) {
				int nr;

				glDisable(GL_LIGHTING);
				glColor3fv(curcol);
				
				// glVertexPointer(3, GL_FLOAT, 0, dl->verts);
				// glDrawArrays(GL_LINE_STRIP, 0, dl->nr);

				glBegin(GL_LINE_STRIP);
				for(nr= dl->nr; nr; nr--, data+=3)
					glVertex3fv(data);
				glEnd();

				glEnable(GL_LIGHTING);
			}
			break;
		case DL_POLY:
			if(ob->type==OB_SURF) {
				int nr;
				
				BIF_ThemeColor(TH_WIRE);
				glDisable(GL_LIGHTING);
				
				/* for some reason glDrawArrays crashes here in half of the platforms (not osx) */
				//glVertexPointer(3, GL_FLOAT, 0, dl->verts);
				//glDrawArrays(GL_LINE_LOOP, 0, dl->nr);
				
				glBegin(GL_LINE_LOOP);
				for(nr= dl->nr; nr; nr--, data+=3)
					glVertex3fv(data);
				glEnd();
				
				glEnable(GL_LIGHTING);
				break;
			}
		case DL_SURF:
			
			if(dl->index) {
				set_gl_material(dl->col+1);
				
				if(dl->rt & CU_SMOOTH) glShadeModel(GL_SMOOTH);
				else glShadeModel(GL_FLAT);
				
				glVertexPointer(3, GL_FLOAT, 0, dl->verts);
				glNormalPointer(GL_FLOAT, 0, dl->nors);
				glDrawElements(GL_QUADS, 4*dl->totindex, GL_UNSIGNED_INT, dl->index);
			}			
			break;

		case DL_INDEX3:
		
			set_gl_material(dl->col+1);
			
			glVertexPointer(3, GL_FLOAT, 0, dl->verts);
			
			/* voor polys only one normal needed */
			if(index3_nors_incr==0) {
				glDisableClientState(GL_NORMAL_ARRAY);
				glNormal3fv(ndata);
			}
			else
				glNormalPointer(GL_FLOAT, 0, dl->nors);
			
			glDrawElements(GL_TRIANGLES, 3*dl->parts, GL_UNSIGNED_INT, dl->index);
			
			if(index3_nors_incr==0)
				glEnableClientState(GL_NORMAL_ARRAY);

			break;

		case DL_INDEX4:

			set_gl_material(dl->col+1);
			
			glVertexPointer(3, GL_FLOAT, 0, dl->verts);
			glNormalPointer(GL_FLOAT, 0, dl->nors);
			glDrawElements(GL_QUADS, 4*dl->parts, GL_UNSIGNED_INT, dl->index);
			
			break;
		}
		dl= dl->next;
	}

	glShadeModel(GL_FLAT);
	glDisable(GL_LIGHTING);
	glFrontFace(GL_CCW);
}

static void drawDispListshaded(ListBase *lb, Object *ob)
{
	DispList *dl, *dlob;
	unsigned int *cdata;

	if(lb==NULL) return;

	glShadeModel(GL_SMOOTH);
	glDisableClientState(GL_NORMAL_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);
	
	dl= lb->first;
	dlob= ob->disp.first;
	while(dl && dlob) {
		
		cdata= dlob->col1;
		if(cdata==NULL) break;
		
		switch(dl->type) {
		case DL_SURF:
			if(dl->index) {
				glVertexPointer(3, GL_FLOAT, 0, dl->verts);
				glColorPointer(4, GL_UNSIGNED_BYTE, 0, cdata);
				glDrawElements(GL_QUADS, 4*dl->totindex, GL_UNSIGNED_INT, dl->index);
			}			
			break;

		case DL_INDEX3:
			
			glVertexPointer(3, GL_FLOAT, 0, dl->verts);
			glColorPointer(4, GL_UNSIGNED_BYTE, 0, cdata);
			glDrawElements(GL_TRIANGLES, 3*dl->parts, GL_UNSIGNED_INT, dl->index);
			break;

		case DL_INDEX4:
			
			glVertexPointer(3, GL_FLOAT, 0, dl->verts);
			glColorPointer(4, GL_UNSIGNED_BYTE, 0, cdata);
			glDrawElements(GL_QUADS, 4*dl->parts, GL_UNSIGNED_INT, dl->index);
			break;
		}
		
		dl= dl->next;
		dlob= dlob->next;
	}
	
	glShadeModel(GL_FLAT);
	glEnableClientState(GL_NORMAL_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
}

/* returns 1 when nothing was drawn */
static int drawDispList(Base *base, int dt)
{
	Object *ob= base->object;
	ListBase *lb=0;
	DispList *dl;
	Curve *cu;
	int solid, retval= 0;
	
	solid= (dt > OB_WIRE);

	switch(ob->type) {
	case OB_FONT:
	case OB_CURVE:
		cu= ob->data;
		
		lb= &cu->disp;
		
		if(solid) {
			dl= lb->first;
			if(dl==NULL) return 1;
			
			if(dl->nors==0) addnormalsDispList(ob, lb);
			index3_nors_incr= 0;
			
			if( displist_has_faces(lb)==0) {
				draw_index_wire= 0;
				drawDispListwire(lb);
				draw_index_wire= 1;
			}
			else {
				if(dt==OB_SHADED) {
					if(ob->disp.first==0) shadeDispList(base);
					drawDispListshaded(lb, ob);
				}
				else {
					init_gl_materials(ob, 0);
					glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, 0);
					drawDispListsolid(lb, ob);
				}
				if(ob==G.obedit && cu->bevobj==NULL && cu->taperobj==NULL) {
					cpack(0);
					draw_index_wire= 0;
					drawDispListwire(lb);
					draw_index_wire= 1;
				}
			}
			index3_nors_incr= 1;
		}
		else {
			draw_index_wire= 0;
			retval= drawDispListwire(lb);
			draw_index_wire= 1;
		}
		break;
	case OB_SURF:
	
		lb= &((Curve *)ob->data)->disp;
		
		if(solid) {
			dl= lb->first;
			if(dl==NULL) return 1;
			
			if(dl->nors==NULL) addnormalsDispList(ob, lb);
			
			if(dt==OB_SHADED) {
				if(ob->disp.first==NULL) shadeDispList(base);
				drawDispListshaded(lb, ob);
			}
			else {
				init_gl_materials(ob, 0);
				glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, 0);
			
				drawDispListsolid(lb, ob);
			}
		}
		else {
			retval= drawDispListwire(lb);
		}
		break;
	case OB_MBALL:
		
		if( is_basis_mball(ob)) {
			lb= &ob->disp;
			if(lb->first==NULL) makeDispListMBall(ob);
			if(lb->first==NULL) return 1;
			
			if(solid) {
				
				if(dt==OB_SHADED) {
					dl= lb->first;
					if(dl && dl->col1==0) shadeDispList(base);
					drawDispListshaded(lb, ob);
				}
				else {
					init_gl_materials(ob, 0);
					glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, 0);
				
					drawDispListsolid(lb, ob);	
				}
			}
			else{
				/* MetaBalls use DL_INDEX4 type of DispList */
				retval= drawDispListwire(lb);
			}
		}
		break;
	}
	
	return retval;
}

/* ******************************** */


static void draw_particle_system(Base *base, PartEff *paf)
{
	Object *ob= base->object;
	Particle *pa;
	float ptime, ctime, vec[3], vec1[3], mat[4][4];
	int a, totpart;
	
	pa= paf->keys;
	// FSPARTICLE always rebuild fluid particle system upon change...
	if( (pa==NULL) 
	    || ( (ob->fluidsimFlag & OB_FLUIDSIM_ENABLE) && (ob->fluidsimSettings) && (ob->fluidsimSettings->type == OB_FLUIDSIM_PARTICLE)) 
			) {
		build_particle_system(ob);
		pa= paf->keys;
		if(pa==NULL) return;
	}
	
	myloadmatrix(G.vd->viewmat);
	/* flag abuse... but I need working code too now. This feature doesnt work for per frame animated objects */
	if( (base->flag & OB_FROMDUPLI) && (ob->flag & OB_FROMGROUP) ) {
		Mat4MulMat4(mat, paf->imat, ob->obmat);
		mymultmatrix(mat);
	}
	
	if(ob->ipoflag & OB_OFFS_PARTICLE) ptime= ob->sf;
	else ptime= 0.0;
	ctime= bsystem_time(ob, 0, (float)(G.scene->r.cfra), ptime);

	glPointSize(1.0);
	
	if(paf->stype==PAF_VECT) glBegin(GL_LINES);
	else glBegin(GL_POINTS);

	totpart= (paf->disp*paf->totpart)/100;
	for(a=0; a<totpart; a++, pa+=paf->totkey) {
		
		if(ctime > pa->time) {
			if(ctime < pa->time+pa->lifetime) {
			
				if(paf->stype==PAF_VECT) {
					where_is_particle(paf, pa, ctime, vec);
					where_is_particle(paf, pa, ctime+1.0, vec1);
		
					glVertex3fv(vec);
					glVertex3fv(vec1);
				}
				else {
					where_is_particle(paf, pa, ctime, vec);
					
					glVertex3fv(vec);
				}
			}
		}
	}
	glEnd();
	
	myloadmatrix(G.vd->viewmat);
	mymultmatrix(ob->obmat);	// bring back local matrix for dtx
}

static void draw_static_particle_system(Object *ob, PartEff *paf, int dt)
{
	Particle *pa;
	float ctime, mtime, vec[3], veco[3];
	int a, use_norm=0, totpart;
	
	pa= paf->keys;
	// FSPARTICLE always rebuild upon change...
	if( (pa==NULL) 
	    || ( (ob->fluidsimFlag & OB_FLUIDSIM_ENABLE) && (ob->fluidsimSettings) && (ob->fluidsimSettings->type == OB_FLUIDSIM_PARTICLE)) 
			) {
		build_particle_system(ob);
		pa= paf->keys;
		if(pa==NULL) return;
	}
	
	if(paf->stype==PAF_VECT) {
		if(dt>OB_WIRE) {
			
			/* shaded/texture mode: we still draw solid, so have to set materials */
			if(dt>OB_SOLID) init_gl_materials(ob, 0);

			glEnable(GL_LIGHTING);
			set_gl_material(paf->omat);
			use_norm= 1;
		}
	}
	else {
		glPointSize(1.0);
		glBegin(GL_POINTS);
	}
	
	totpart= (paf->disp*paf->totpart)/100;
	for(a=0; a<totpart; a++, pa+=paf->totkey) {
		
		if(paf->stype==PAF_VECT) {
			
			glBegin(GL_LINE_STRIP);
			where_is_particle(paf, pa, pa->time, veco);

			mtime= pa->time+pa->lifetime+paf->staticstep;
			for(ctime= pa->time+paf->staticstep; ctime<mtime; ctime+=paf->staticstep) {
				
				where_is_particle(paf, pa, ctime, vec);
				
				if(use_norm) {
					float no[3];
					VECSUB(no, vec, veco);
					glNormal3fv(no);
				}
				glVertex3fv(veco);
				VECCOPY(veco, vec);
			}
			
			glVertex3fv(veco);
			glEnd();
		}
		else {
			mtime= pa->time+pa->lifetime+paf->staticstep-1;
			for(ctime= pa->time; ctime<mtime; ctime+=paf->staticstep) {
				where_is_particle(paf, pa, ctime, vec);
				glVertex3fv(vec);
			}
		}
	}
	if(paf->stype==PAF_VECT) {
		glDisable(GL_LIGHTING);
	}
	else {
		glEnd();
	}
	
}

unsigned int nurbcol[8]= {
	0, 0x9090, 0x409030, 0x603080, 0, 0x40fff0, 0x40c033, 0xA090F0 };

static void tekenhandlesN(Nurb *nu, short sel)
{
	BezTriple *bezt;
	float *fp;
	unsigned int *col;
	int a;

	if(nu->hide) return;
	
	glBegin(GL_LINES); 
	
	if( (nu->type & 7)==1) {
		if(sel) col= nurbcol+4;
		else col= nurbcol;

		bezt= nu->bezt;
		a= nu->pntsu;
		while(a--) {
			if(bezt->hide==0) {
				if( (bezt->f2 & 1)==sel) {
					fp= bezt->vec[0];
					
					cpack(col[bezt->h1]);
					glVertex3fv(fp);
					glVertex3fv(fp+3); 

					cpack(col[bezt->h2]);
					glVertex3fv(fp+3); 
					glVertex3fv(fp+6); 
				}
				else if( (bezt->f1 & 1)==sel) {
					fp= bezt->vec[0];
					
					cpack(col[bezt->h1]);
					glVertex3fv(fp); 
					glVertex3fv(fp+3); 
				}
				else if( (bezt->f3 & 1)==sel) {
					fp= bezt->vec[1];
					
					cpack(col[bezt->h2]);
					glVertex3fv(fp); 
					glVertex3fv(fp+3); 
				}
			}
			bezt++;
		}
	}
	glEnd();
}

static void tekenvertsN(Nurb *nu, short sel)
{
	BezTriple *bezt;
	BPoint *bp;
	float size;
	int a;

	if(nu->hide) return;

	if(sel) BIF_ThemeColor(TH_VERTEX_SELECT);
	else BIF_ThemeColor(TH_VERTEX);

	size= BIF_GetThemeValuef(TH_VERTEX_SIZE);
	glPointSize(size);
	
	bglBegin(GL_POINTS);
	
	if((nu->type & 7)==1) {

		bezt= nu->bezt;
		a= nu->pntsu;
		while(a--) {
			if(bezt->hide==0) {
				if((bezt->f1 & 1)==sel) bglVertex3fv(bezt->vec[0]);
				if((bezt->f2 & 1)==sel) bglVertex3fv(bezt->vec[1]);
				if((bezt->f3 & 1)==sel) bglVertex3fv(bezt->vec[2]);
			}
			bezt++;
		}
	}
	else {
		bp= nu->bp;
		a= nu->pntsu*nu->pntsv;
		while(a--) {
			if(bp->hide==0) {
				if((bp->f1 & 1)==sel) bglVertex3fv(bp->vec);
			}
			bp++;
		}
	}
	
	bglEnd();
	glPointSize(1.0);
}

static void draw_editnurb(Object *ob, Nurb *nurb, int sel)
{
	Nurb *nu;
	BPoint *bp, *bp1;
	int a, b, ofs;
	
	nu= nurb;
	while(nu) {
		if(nu->hide==0) {
			switch(nu->type & 7) {
			case CU_POLY:
				cpack(nurbcol[3]);
				bp= nu->bp;
				for(b=0; b<nu->pntsv; b++) {
					if(nu->flagu & 1) glBegin(GL_LINE_LOOP);
					else glBegin(GL_LINE_STRIP);

					for(a=0; a<nu->pntsu; a++, bp++) {
						glVertex3fv(bp->vec);
					}

					glEnd();
				}
				break;
			case CU_NURBS:

				bp= nu->bp;
				for(b=0; b<nu->pntsv; b++) {
					bp1= bp;
					bp++;
					for(a=nu->pntsu-1; a>0; a--, bp++) {
						if(bp->hide==0 && bp1->hide==0) {
							if(sel) {
								if( (bp->f1 & 1) && ( bp1->f1 & 1) ) {
									cpack(nurbcol[5]);
		
									glBegin(GL_LINE_STRIP);
									glVertex3fv(bp->vec); 
									glVertex3fv(bp1->vec);
									glEnd();
								}
							}
							else {
								if( (bp->f1 & 1) && ( bp1->f1 & 1) );
								else {
									cpack(nurbcol[1]);
		
									glBegin(GL_LINE_STRIP);
									glVertex3fv(bp->vec); 
									glVertex3fv(bp1->vec);
									glEnd();
								}
							}
						}
						bp1= bp;
					}
				}
				if(nu->pntsv > 1) {	/* surface */

					ofs= nu->pntsu;
					for(b=0; b<nu->pntsu; b++) {
						bp1= nu->bp+b;
						bp= bp1+ofs;
						for(a=nu->pntsv-1; a>0; a--, bp+=ofs) {
							if(bp->hide==0 && bp1->hide==0) {
								if(sel) {
									if( (bp->f1 & 1) && ( bp1->f1 & 1) ) {
										cpack(nurbcol[7]);
			
										glBegin(GL_LINE_STRIP);
										glVertex3fv(bp->vec); 
										glVertex3fv(bp1->vec);
										glEnd();
									}
								}
								else {
									if( (bp->f1 & 1) && ( bp1->f1 & 1) );
									else {
										cpack(nurbcol[3]);
			
										glBegin(GL_LINE_STRIP);
										glVertex3fv(bp->vec); 
										glVertex3fv(bp1->vec);
										glEnd();
									}
								}
							}
							bp1= bp;
						}
					}

				}
				break;
			}
		}
		nu= nu->next;
	}
}

static void drawnurb(Base *base, Nurb *nurb, int dt)
{
	Object *ob= base->object;
	Curve *cu = ob->data;
	Nurb *nu;
	BevList *bl;

	retopo_matrix_update(G.vd);

	/* DispList */
	BIF_ThemeColor(TH_WIRE);
	drawDispList(base, dt);

	if(G.vd->zbuf) glDisable(GL_DEPTH_TEST);
	
	/* first non-selected handles */
	for(nu=nurb; nu; nu=nu->next) {
		if((nu->type & 7)==CU_BEZIER) {
			tekenhandlesN(nu, 0);
		}
	}
	draw_editnurb(ob, nurb, 0);
	draw_editnurb(ob, nurb, 1);
	/* selected handles */
	for(nu=nurb; nu; nu=nu->next) {
		if((nu->type & 7)==1) tekenhandlesN(nu, 1);
		tekenvertsN(nu, 0);
	}
	
	if(G.vd->zbuf) glEnable(GL_DEPTH_TEST);

	/* direction vectors for 3d curve paths */
	if(cu->flag & CU_3D) {
		BIF_ThemeColor(TH_WIRE);
		for (bl=cu->bev.first,nu=nurb; nu && bl; bl=bl->next,nu=nu->next) {
			BevPoint *bevp= (BevPoint *)(bl+1);		
			int nr= bl->nr;
			int skip= nu->resolu/16;
			float fac;
			
			while (nr-->0) {
				float ox,oy,oz; // Offset perpendicular to the curve
				float dx,dy,dz; // Delta along the curve

				fac = calc_curve_subdiv_radius(cu, nu, (bl->nr - nr)) * G.scene->editbutsize;
				
				ox = fac*bevp->mat[0][0];
				oy = fac*bevp->mat[0][1];
				oz = fac*bevp->mat[0][2];
			
				dx = fac*bevp->mat[2][0];
				dy = fac*bevp->mat[2][1];
				dz = fac*bevp->mat[2][2];

				glBegin(GL_LINE_STRIP);
				glVertex3f(bevp->x - ox - dx, bevp->y - oy - dy, bevp->z - oz - dz);
				glVertex3f(bevp->x, bevp->y, bevp->z);
				glVertex3f(bevp->x + ox - dx, bevp->y + oy - dy, bevp->z + oz - dz);
				glEnd();
				
				bevp += skip+1;
				nr -= skip;
			}
		}
	}

	if(G.vd->zbuf) glDisable(GL_DEPTH_TEST);
	
	for(nu=nurb; nu; nu=nu->next) {
		tekenvertsN(nu, 1);
	}
	
	if(G.vd->zbuf) glEnable(GL_DEPTH_TEST); 
}

/* draw points on curve speed handles */
static void curve_draw_speed(Object *ob)
{
	Curve *cu= ob->data;
	IpoCurve *icu;
	BezTriple *bezt;
	float loc[4], dir[3];
	int a;
	
	if(cu->ipo==NULL)
		return;
	
	icu= cu->ipo->curve.first; 
	if(icu==NULL || icu->totvert<2)
		return;
	
	glPointSize( BIF_GetThemeValuef(TH_VERTEX_SIZE) );
	bglBegin(GL_POINTS);

	for(a=0, bezt= icu->bezt; a<icu->totvert; a++, bezt++) {
		if( where_on_path(ob, bezt->vec[1][1], loc, dir)) {
			BIF_ThemeColor((bezt->f2 & SELECT) && ob==OBACT?TH_VERTEX_SELECT:TH_VERTEX);
			bglVertex3fv(loc);
		}
	}

	glPointSize(1.0);
	bglEnd();
}


static void tekentextcurs(void)
{
	cpack(0);
	
	set_inverted_drawing(1);
	glBegin(GL_QUADS);
	glVertex2fv(G.textcurs[0]);
	glVertex2fv(G.textcurs[1]);
	glVertex2fv(G.textcurs[2]);
	glVertex2fv(G.textcurs[3]);
	glEnd();
	set_inverted_drawing(0);
}

static void drawspiral(float *cent, float rad, float tmat[][4], int start)
{
	float vec[3], vx[3], vy[3];
	int a, tot=32;
	char inverse=0;
	/* 32 values of sin function (still same result!) */
	static float si[32] = {0.00000000,
		0.20129852,
		0.39435585,
		0.57126821,
		0.72479278,
		0.84864425,
		0.93775213,
		0.98846832,
		0.99871650,
		0.96807711,
		0.89780453,
		0.79077573,
		0.65137248,
		0.48530196,
		0.29936312,
		0.10116832,
		-0.10116832,
                -0.29936312,
                -0.48530196,
                -0.65137248,
                -0.79077573,
                -0.89780453,
                -0.96807711,
                -0.99871650,
                -0.98846832,
                -0.93775213,
                -0.84864425,
                -0.72479278,
                -0.57126821,
                -0.39435585,
                -0.20129852,
		0.00000000};
	/* 32 values of cos function (still same result!) */
	static float co[32] ={1.00000000,
                0.97952994,
                0.91895781,
                0.82076344,
                0.68896691,
                0.52896401,
                0.34730525,
                0.15142777,
                -0.05064916,
                -0.25065253,
                -0.44039415,
                -0.61210598,
                -0.75875812,
                -0.87434661,
                -0.95413925,
                -0.99486932,
                -0.99486932,
                -0.95413925,
                -0.87434661,
                -0.75875812,
                -0.61210598,
                -0.44039415,
                -0.25065253,
                -0.05064916,
                0.15142777,
                0.34730525,
                0.52896401,
                0.68896691,
                0.82076344,
                0.91895781,
                0.97952994,
                1.00000000};
		
	if (start < 0) {
		inverse = 1;
		start *= -1;
	}

	VECCOPY(vx, tmat[0]);
	VECCOPY(vy, tmat[1]);
	VecMulf(vx, rad);
	VecMulf(vy, rad);

	VECCOPY(vec, cent);

	if (inverse==0) {
		for(a=0; a<tot; a++) {
			if (a+start>31)
				start=-a + 1;
			glBegin(GL_LINES);							
			glVertex3fv(vec);
			vec[0]= cent[0] + *(si+a+start) * (vx[0] * (float)a/(float)tot) + *(co+a+start) * (vy[0] * (float)a/(float)tot);
			vec[1]= cent[1] + *(si+a+start) * (vx[1] * (float)a/(float)tot) + *(co+a+start) * (vy[1] * (float)a/(float)tot);
			vec[2]= cent[2] + *(si+a+start) * (vx[2] * (float)a/(float)tot) + *(co+a+start) * (vy[2] * (float)a/(float)tot);
			glVertex3fv(vec);
			glEnd();
		}
	}
	else {
		a=0;
		vec[0]= cent[0] + *(si+a+start) * (vx[0] * (float)(-a+31)/(float)tot) + *(co+a+start) * (vy[0] * (float)(-a+31)/(float)tot);
		vec[1]= cent[1] + *(si+a+start) * (vx[1] * (float)(-a+31)/(float)tot) + *(co+a+start) * (vy[1] * (float)(-a+31)/(float)tot);
		vec[2]= cent[2] + *(si+a+start) * (vx[2] * (float)(-a+31)/(float)tot) + *(co+a+start) * (vy[2] * (float)(-a+31)/(float)tot);
		for(a=0; a<tot; a++) {
			if (a+start>31)
				start=-a + 1;
			glBegin(GL_LINES);							
			glVertex3fv(vec);
			vec[0]= cent[0] + *(si+a+start) * (vx[0] * (float)(-a+31)/(float)tot) + *(co+a+start) * (vy[0] * (float)(-a+31)/(float)tot);
			vec[1]= cent[1] + *(si+a+start) * (vx[1] * (float)(-a+31)/(float)tot) + *(co+a+start) * (vy[1] * (float)(-a+31)/(float)tot);
			vec[2]= cent[2] + *(si+a+start) * (vx[2] * (float)(-a+31)/(float)tot) + *(co+a+start) * (vy[2] * (float)(-a+31)/(float)tot);
			glVertex3fv(vec);
			glEnd();
		}
	}
}

void drawcircball(int mode, float *cent, float rad, float tmat[][4])
{
	float vec[3], vx[3], vy[3];
	int a, tot=32;

	/* 32 values of sin function (still same result!) */
	static float si[32] = {0.00000000,
		0.20129852,
		0.39435585,
		0.57126821,
		0.72479278,
		0.84864425,
		0.93775213,
		0.98846832,
		0.99871650,
		0.96807711,
		0.89780453,
		0.79077573,
		0.65137248,
		0.48530196,
		0.29936312,
		0.10116832,
		-0.10116832,
                -0.29936312,
                -0.48530196,
                -0.65137248,
                -0.79077573,
                -0.89780453,
                -0.96807711,
                -0.99871650,
                -0.98846832,
                -0.93775213,
                -0.84864425,
                -0.72479278,
                -0.57126821,
                -0.39435585,
                -0.20129852,
		0.00000000};
	/* 32 values of cos function (still same result!) */
	static float co[32] ={1.00000000,
                0.97952994,
                0.91895781,
                0.82076344,
                0.68896691,
                0.52896401,
                0.34730525,
                0.15142777,
                -0.05064916,
                -0.25065253,
                -0.44039415,
                -0.61210598,
                -0.75875812,
                -0.87434661,
                -0.95413925,
                -0.99486932,
                -0.99486932,
                -0.95413925,
                -0.87434661,
                -0.75875812,
                -0.61210598,
                -0.44039415,
                -0.25065253,
                -0.05064916,
                0.15142777,
                0.34730525,
                0.52896401,
                0.68896691,
                0.82076344,
                0.91895781,
                0.97952994,
                1.00000000};
		
	VECCOPY(vx, tmat[0]);
	VECCOPY(vy, tmat[1]);
	VecMulf(vx, rad);
	VecMulf(vy, rad);
	
	glBegin(mode);
	for(a=0; a<tot; a++) {
		vec[0]= cent[0] + *(si+a) * vx[0] + *(co+a) * vy[0];
		vec[1]= cent[1] + *(si+a) * vx[1] + *(co+a) * vy[1];
		vec[2]= cent[2] + *(si+a) * vx[2] + *(co+a) * vy[2];
		glVertex3fv(vec);
	}
	glEnd();
}

/* return 1 if nothing was drawn */
static int drawmball(Base *base, int dt)
{
	Object *ob= base->object;
	MetaBall *mb;
	MetaElem *ml;
	float imat[4][4], tmat[4][4];
	int code= 1;
	
	mb= ob->data;

	if(ob==G.obedit) {
		BIF_ThemeColor(TH_WIRE);
		if((G.f & G_PICKSEL)==0 ) drawDispList(base, dt);
		ml= editelems.first;
	}
	else {
		if((base->flag & OB_FROMDUPLI)==0) 
			drawDispList(base, dt);
		ml= mb->elems.first;
	}

	if(ml==NULL) return 1;
	
	/* in case solid draw, reset wire colors */
	if(ob!=G.obedit && (ob->flag & SELECT)) {
		if(ob==OBACT) BIF_ThemeColor(TH_ACTIVE);
		else BIF_ThemeColor(TH_SELECT);
	}
	else BIF_ThemeColor(TH_WIRE);

	mygetmatrix(tmat);
	Mat4Invert(imat, tmat);
	Normalize(imat[0]);
	Normalize(imat[1]);
	
	while(ml) {
	
		/* draw radius */
		if(ob==G.obedit) {
			if((ml->flag & SELECT) && (ml->flag & MB_SCALE_RAD)) cpack(0xA0A0F0);
			else cpack(0x3030A0);
			
			if(G.f & G_PICKSEL) {
				ml->selcol1= code;
				glLoadName(code++);
			}
		}
		drawcircball(GL_LINE_LOOP, &(ml->x), ml->rad, imat);

		/* draw stiffness */
		if(ob==G.obedit) {
			if((ml->flag & SELECT) && !(ml->flag & MB_SCALE_RAD)) cpack(0xA0F0A0);
			else cpack(0x30A030);
			
			if(G.f & G_PICKSEL) {
				ml->selcol2= code;
				glLoadName(code++);
			}
			drawcircball(GL_LINE_LOOP, &(ml->x), ml->rad*atan(ml->s)/M_PI_2, imat);
		}
		
		ml= ml->next;
	}
	return 0;
}

static void draw_forcefield(Object *ob)
{
	PartDeflect *pd= ob->pd;
	float imat[4][4], tmat[4][4];
	float vec[3]= {0.0, 0.0, 0.0};
	int curcol;
	float size;
	
	if(ob!=G.obedit && (ob->flag & SELECT)) {
		if(ob==OBACT) curcol= TH_ACTIVE;
		else curcol= TH_SELECT;
	}
	else curcol= TH_WIRE;
	
	/* scale size of circle etc with the empty drawsize */
	if (ob->type == OB_EMPTY) size = ob->empty_drawsize;
	else size = 1.0;
	
	/* calculus here, is reused in PFIELD_FORCE */
	mygetmatrix(tmat);
	Mat4Invert(imat, tmat);
//	Normalize(imat[0]);		// we don't do this because field doesnt scale either... apart from wind!
//	Normalize(imat[1]);
	
	if (pd->forcefield == PFIELD_WIND) {
		float force_val;
		
		Mat4One(tmat);
		BIF_ThemeColorBlend(curcol, TH_BACK, 0.5);
		
		if (has_ipo_code(ob->ipo, OB_PD_FSTR))
			force_val = IPO_GetFloatValue(ob->ipo, OB_PD_FSTR, G.scene->r.cfra);
		else 
			force_val = pd->f_strength;
		force_val*= 0.1;
		drawcircball(GL_LINE_LOOP, vec, size, tmat);
		vec[2]= 0.5*force_val;
		drawcircball(GL_LINE_LOOP, vec, size, tmat);
		vec[2]= 1.0*force_val;
		drawcircball(GL_LINE_LOOP, vec, size, tmat);
		vec[2]= 1.5*force_val;
		drawcircball(GL_LINE_LOOP, vec, size, tmat);
		
	}
	else if (pd->forcefield == PFIELD_FORCE) {
		float ffall_val;

		if (has_ipo_code(ob->ipo, OB_PD_FFALL)) 
			ffall_val = IPO_GetFloatValue(ob->ipo, OB_PD_FFALL, G.scene->r.cfra);
		else 
			ffall_val = pd->f_power;

		BIF_ThemeColorBlend(curcol, TH_BACK, 0.5);
		drawcircball(GL_LINE_LOOP, vec, size, imat);
		BIF_ThemeColorBlend(curcol, TH_BACK, 0.9 - 0.4 / pow(1.5, (double)ffall_val));
		drawcircball(GL_LINE_LOOP, vec, size*1.5, imat);
		BIF_ThemeColorBlend(curcol, TH_BACK, 0.9 - 0.4 / pow(2.0, (double)ffall_val));
		drawcircball(GL_LINE_LOOP, vec, size*2.0, imat);
	}
	else if (pd->forcefield == PFIELD_VORTEX) {
		float ffall_val, force_val;

		Mat4One(imat);
		if (has_ipo_code(ob->ipo, OB_PD_FFALL)) 
			ffall_val = IPO_GetFloatValue(ob->ipo, OB_PD_FFALL, G.scene->r.cfra);
		else 
			ffall_val = pd->f_power;

		if (has_ipo_code(ob->ipo, OB_PD_FSTR))
			force_val = IPO_GetFloatValue(ob->ipo, OB_PD_FSTR, G.scene->r.cfra);
		else 
			force_val = pd->f_strength;

		BIF_ThemeColorBlend(curcol, TH_BACK, 0.7);
		if (force_val < 0) {
			drawspiral(vec, size*1.0, imat, 1);
			drawspiral(vec, size*1.0, imat, 16);
		}
		else {
			drawspiral(vec, size*1.0, imat, -1);
			drawspiral(vec, size*1.0, imat, -16);
		}
	}
	else if (pd->forcefield == PFIELD_GUIDE && ob->type==OB_CURVE) {
		Curve *cu= ob->data;
		if((cu->flag & CU_PATH) && cu->path && cu->path->data) {
			float mindist, guidevec1[4], guidevec2[3];

			if (has_ipo_code(ob->ipo, OB_PD_FSTR))
				mindist = IPO_GetFloatValue(ob->ipo, OB_PD_FSTR, G.scene->r.cfra);
			else 
				mindist = pd->f_strength;

			/*path end*/
			setlinestyle(3);
			where_on_path(ob, 1.0f, guidevec1, guidevec2);
			BIF_ThemeColorBlend(curcol, TH_BACK, 0.5);
			drawcircball(GL_LINE_LOOP, guidevec1, mindist, imat);

			/*path beginning*/
			setlinestyle(0);
			where_on_path(ob, 0.0f, guidevec1, guidevec2);
			BIF_ThemeColorBlend(curcol, TH_BACK, 0.5);
			drawcircball(GL_LINE_LOOP, guidevec1, mindist, imat);
			
			VECCOPY(vec, guidevec1);	/* max center */
		}
	}
	
	/* as last, guide curve alters it */
	if(pd->flag & PFIELD_USEMAX) {
		setlinestyle(3);
		BIF_ThemeColorBlend(curcol, TH_BACK, 0.5);
		drawcircball(GL_LINE_LOOP, vec, pd->maxdist, imat);
		setlinestyle(0);
	}
}

static void draw_box(float vec[8][3])
{
	glBegin(GL_LINE_STRIP);
	glVertex3fv(vec[0]); glVertex3fv(vec[1]);glVertex3fv(vec[2]); glVertex3fv(vec[3]);
	glVertex3fv(vec[0]); glVertex3fv(vec[4]);glVertex3fv(vec[5]); glVertex3fv(vec[6]);
	glVertex3fv(vec[7]); glVertex3fv(vec[4]);
	glEnd();

	glBegin(GL_LINES);
	glVertex3fv(vec[1]); glVertex3fv(vec[5]);
	glVertex3fv(vec[2]); glVertex3fv(vec[6]);
	glVertex3fv(vec[3]); glVertex3fv(vec[7]);
	glEnd();
}

/* uses boundbox, function used by Ketsji */
void get_local_bounds(Object *ob, float *center, float *size)
{
	BoundBox *bb= object_get_boundbox(ob);
	
	if(bb==NULL) {
		center[0]= center[1]= center[2]= 0.0;
		VECCOPY(size, ob->size);
	}
	else {
		size[0]= 0.5*fabs(bb->vec[0][0] - bb->vec[4][0]);
		size[1]= 0.5*fabs(bb->vec[0][1] - bb->vec[2][1]);
		size[2]= 0.5*fabs(bb->vec[0][2] - bb->vec[1][2]);
		
		center[0]= (bb->vec[0][0] + bb->vec[4][0])/2.0;
		center[1]= (bb->vec[0][1] + bb->vec[2][1])/2.0;
		center[2]= (bb->vec[0][2] + bb->vec[1][2])/2.0;
	}
}



static void draw_bb_quadric(BoundBox *bb, short type)
{
	float size[3], cent[3];
	GLUquadricObj *qobj = gluNewQuadric(); 
	
	gluQuadricDrawStyle(qobj, GLU_SILHOUETTE); 
	
	size[0]= 0.5*fabs(bb->vec[0][0] - bb->vec[4][0]);
	size[1]= 0.5*fabs(bb->vec[0][1] - bb->vec[2][1]);
	size[2]= 0.5*fabs(bb->vec[0][2] - bb->vec[1][2]);
	
	cent[0]= (bb->vec[0][0] + bb->vec[4][0])/2.0;
	cent[1]= (bb->vec[0][1] + bb->vec[2][1])/2.0;
	cent[2]= (bb->vec[0][2] + bb->vec[1][2])/2.0;
	
	glPushMatrix();
	if(type==OB_BOUND_SPHERE) {
		glTranslatef(cent[0], cent[1], cent[2]);
		glScalef(size[0], size[1], size[2]);
		gluSphere(qobj, 1.0, 8, 5);
	}	
	else if(type==OB_BOUND_CYLINDER) {
		float radius = size[0] > size[1] ? size[0] : size[1];
		glTranslatef(cent[0], cent[1], cent[2]-size[2]);
		glScalef(radius, radius, 2.0*size[2]);
		gluCylinder(qobj, 1.0, 1.0, 1.0, 8, 1);
	}
	else if(type==OB_BOUND_CONE) {
		float radius = size[0] > size[1] ? size[0] : size[1];
		glTranslatef(cent[0], cent[2]-size[2], cent[1]);
		glScalef(radius, 2.0*size[2], radius);
		glRotatef(-90., 1.0, 0.0, 0.0);
		gluCylinder(qobj, 1.0, 0.0, 1.0, 8, 1);
	}
	glPopMatrix();
	
	gluDeleteQuadric(qobj); 
}

static void draw_bounding_volume(Object *ob)
{
	BoundBox *bb=0;
	
	if(ob->type==OB_MESH) {
		bb= mesh_get_bb(ob->data);
	}
	else if ELEM3(ob->type, OB_CURVE, OB_SURF, OB_FONT) {
		bb= ( (Curve *)ob->data )->bb;
	}
	else if(ob->type==OB_MBALL) {
		bb= ob->bb;
		if(bb==0) {
			makeDispListMBall(ob);
			bb= ob->bb;
		}
	}
	else {
		drawcube();
		return;
	}
	
	if(bb==0) return;
	
	if(ob->boundtype==OB_BOUND_BOX) draw_box(bb->vec);
	else draw_bb_quadric(bb, ob->boundtype);
	
}

static void drawtexspace(Object *ob)
{
	float vec[8][3], loc[3], size[3];
	
	if(ob->type==OB_MESH) {
		mesh_get_texspace(ob->data, loc, NULL, size);
	}
	else if ELEM3(ob->type, OB_CURVE, OB_SURF, OB_FONT) {
		Curve *cu= ob->data;
		VECCOPY(size, cu->size);
		VECCOPY(loc, cu->loc);
	}
	else if(ob->type==OB_MBALL) {
		MetaBall *mb= ob->data;
		VECCOPY(size, mb->size);
		VECCOPY(loc, mb->loc);
	}
	else return;
	
	vec[0][0]=vec[1][0]=vec[2][0]=vec[3][0]= loc[0]-size[0];
	vec[4][0]=vec[5][0]=vec[6][0]=vec[7][0]= loc[0]+size[0];
	
	vec[0][1]=vec[1][1]=vec[4][1]=vec[5][1]= loc[1]-size[1];
	vec[2][1]=vec[3][1]=vec[6][1]=vec[7][1]= loc[1]+size[1];

	vec[0][2]=vec[3][2]=vec[4][2]=vec[7][2]= loc[2]-size[2];
	vec[1][2]=vec[2][2]=vec[5][2]=vec[6][2]= loc[2]+size[2];
	
	setlinestyle(2);

	draw_box(vec);

	setlinestyle(0);
}

/* draws wire outline */
static void drawSolidSelect(Base *base) 
{
	Object *ob= base->object;
	
	glLineWidth(2.0);
	glDepthMask(0);
	
	if(ELEM3(ob->type, OB_FONT,OB_CURVE, OB_SURF)) {
		Curve *cu = ob->data;
		if (displist_has_faces(&cu->disp) && boundbox_clip(ob->obmat, cu->bb)) {
			draw_index_wire= 0;
			drawDispListwire(&cu->disp);
			draw_index_wire= 1;
		}
	} else if (ob->type==OB_MBALL) {
		if((base->flag & OB_FROMDUPLI)==0) 
			drawDispListwire(&ob->disp);
	}
	else if(ob->type==OB_ARMATURE) {
		if(!(ob->flag & OB_POSEMODE)) {
			draw_armature(base, OB_WIRE);
		}
	}

	glLineWidth(1.0);
	glDepthMask(1);
}

static void drawWireExtra(Object *ob) 
{
	if(ob!=G.obedit && (ob->flag & SELECT)) {
		if(ob==OBACT) {
			if(ob->flag & OB_FROMGROUP) BIF_ThemeColor(TH_GROUP_ACTIVE);
			else BIF_ThemeColor(TH_ACTIVE);
		}
		else if(ob->flag & OB_FROMGROUP)
			BIF_ThemeColorShade(TH_GROUP_ACTIVE, -16);
		else
			BIF_ThemeColor(TH_SELECT);
	}
	else {
		if(ob->flag & OB_FROMGROUP)
			BIF_ThemeColor(TH_GROUP);
		else {
			if(ob->dtx & OB_DRAWWIRE) {
				glColor3ub(80,80,80);
			} else {
				BIF_ThemeColor(TH_WIRE);
			}
		}
	}
	
	bglPolygonOffset(1.0);
	glDepthMask(0);	// disable write in zbuffer, selected edge wires show better
	
	if (ELEM3(ob->type, OB_FONT, OB_CURVE, OB_SURF)) {
		Curve *cu = ob->data;
		if (boundbox_clip(ob->obmat, cu->bb)) {
			if (ob->type==OB_CURVE)
				draw_index_wire= 0;
			drawDispListwire(&cu->disp);
			if (ob->type==OB_CURVE)
				draw_index_wire= 1;
		}
	} else if (ob->type==OB_MBALL) {
		drawDispListwire(&ob->disp);
	}

	glDepthMask(1);
	bglPolygonOffset(0.0);
}

/* should be called in view space */
static void draw_hooks(Object *ob)
{
	ModifierData *md;
	float vec[3];
	
	for (md=ob->modifiers.first; md; md=md->next) {
		if (md->type==eModifierType_Hook) {
			HookModifierData *hmd = (HookModifierData*) md;

			VecMat4MulVecfl(vec, ob->obmat, hmd->cent);

			if(hmd->object) {
				setlinestyle(3);
				glBegin(GL_LINES);
				glVertex3fv(hmd->object->obmat[3]);
				glVertex3fv(vec);
				glEnd();
				setlinestyle(0);
			}

			glPointSize(3.0);
			bglBegin(GL_POINTS);
			bglVertex3fv(vec);
			bglEnd();
			glPointSize(1.0);
		}
	}
}


//<rcruiz>
void drawRBpivot(bRigidBodyJointConstraint *data){
	float radsPerDeg = 6.283185307179586232f / 360.f;
	int axis;
	float v1[3]= {data->pivX, data->pivY, data->pivZ};
	float eu[3]= {radsPerDeg*data->axX, radsPerDeg*data->axY, radsPerDeg*data->axZ};
	


	float mat[4][4];
	EulToMat4(eu,mat);
	glLineWidth (4.0f);
	setlinestyle(2);
	for (axis=0; axis<3; axis++) {
			float dir[3] = {0,0,0};
			float v[3]= {data->pivX, data->pivY, data->pivZ};

			dir[axis] = 1.f;
			glBegin(GL_LINES);
			Mat4MulVecfl(mat,dir);
			v[0] += dir[0];
			v[1] += dir[1];
			v[2] += dir[2];
			glVertex3fv(v1);
			glVertex3fv(v);			
			glEnd();
			glRasterPos3fv(v);
			if (axis==0)
				BMF_DrawString(G.font, "px");
			else if (axis==1)
				BMF_DrawString(G.font, "py");
			else
				BMF_DrawString(G.font, "pz");			
	}
	glLineWidth (1.0f);
	setlinestyle(0);
}

/* flag can be DRAW_PICKING	and/or DRAW_CONSTCOLOR */
void draw_object(Base *base, int flag)
{
	static int warning_recursive= 0;
	Object *ob;
	Curve *cu;
	float cfraont;
	float vec1[3], vec2[3];
	unsigned int col=0;
	int sel, drawtype, colindex= 0, ipoflag;
	int i, selstart, selend, empty_object=0;
	short dt, dtx, zbufoff= 0;

	/* only once set now, will be removed too, should become a global standard */
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	ob= base->object;

	if (ob!=G.obedit) {
		if (ob->restrictflag & OB_RESTRICT_VIEW) 
			return;
	}

	/* xray delay? */
	if((flag & DRAW_PICKING)==0 && (base->flag & OB_FROMDUPLI)==0) {
		/* xray and transp are set when it is drawing the 2nd/3rd pass */
		if(!G.vd->xray && !G.vd->transp && (ob->dtx & OB_DRAWXRAY)) {
			add_view3d_after(G.vd, base, V3D_XRAY);
			return;
		}
	}

	/* draw keys? */
	if(base==(G.scene->basact) || (base->flag & (SELECT+BA_WAS_SEL))) {
		if(flag==0 && warning_recursive==0 && ob!=G.obedit) {
			if(ob->ipo && ob->ipo->showkey && (ob->ipoflag & OB_DRAWKEY)) {
				ListBase elems;
				CfraElem *ce;
				float temp[7][3];

				warning_recursive= 1;

				elems.first= elems.last= 0;
				make_cfra_list(ob->ipo, &elems);

				cfraont= (G.scene->r.cfra);
				drawtype= G.vd->drawtype;
				if(drawtype>OB_WIRE) G.vd->drawtype= OB_WIRE;
				sel= base->flag;
				memcpy(temp, &ob->loc, 7*3*sizeof(float));

				ipoflag= ob->ipoflag;
				ob->ipoflag &= ~OB_OFFS_OB;

				set_no_parent_ipo(1);
				disable_speed_curve(1);

				if ((ob->ipoflag & OB_DRAWKEYSEL)==0) {
					ce= elems.first;
					while(ce) {
						if(!ce->sel) {
							(G.scene->r.cfra)= ce->cfra/G.scene->r.framelen;

							base->flag= 0;

							where_is_object_time(ob, (G.scene->r.cfra));
							draw_object(base, 0);
						}
						ce= ce->next;
					}
				}

				ce= elems.first;
				while(ce) {
					if(ce->sel) {
						(G.scene->r.cfra)= ce->cfra/G.scene->r.framelen;

						base->flag= SELECT;

						where_is_object_time(ob, (G.scene->r.cfra));
						draw_object(base, 0);
					}
					ce= ce->next;
				}

				set_no_parent_ipo(0);
				disable_speed_curve(0);

				base->flag= sel;
				ob->ipoflag= ipoflag;

				/* restore icu->curval */
				(G.scene->r.cfra)= cfraont;

				memcpy(&ob->loc, temp, 7*3*sizeof(float));
				where_is_object(ob);
				G.vd->drawtype= drawtype;

				BLI_freelistN(&elems);

				warning_recursive= 0;
			}
		}
	}

	/* patch? children objects with a timeoffs change the parents. How to solve! */
	/* if( ((int)ob->ctime) != F_(G.scene->r.cfra)) where_is_object(ob); */

	mymultmatrix(ob->obmat);

	/* which wire color */
	if((flag & DRAW_CONSTCOLOR) == 0) {
		project_short(ob->obmat[3], &base->sx);

		if((G.moving & G_TRANSFORM_OBJ) && (base->flag & (SELECT+BA_WAS_SEL))) BIF_ThemeColor(TH_TRANSFORM);
		else {

			if(ob->type==OB_LAMP) BIF_ThemeColor(TH_LAMP);
			else BIF_ThemeColor(TH_WIRE);

			if((G.scene->basact)==base) {
				if(base->flag & (SELECT+BA_WAS_SEL)) BIF_ThemeColor(TH_ACTIVE);
			}
			else {
				if(base->flag & (SELECT+BA_WAS_SEL)) BIF_ThemeColor(TH_SELECT);
			}

			// no theme yet
			if(ob->id.lib) {
				if(base->flag & (SELECT+BA_WAS_SEL)) colindex = 4;
				else colindex = 3;
			}
			else if(warning_recursive==1) {
				if(base->flag & (SELECT+BA_WAS_SEL)) {
					if(G.scene->basact==base) colindex = 8;
					else colindex= 7;
				}
				else colindex = 6;
			}
			else if(ob->flag & OB_FROMGROUP) {
				if(base->flag & (SELECT+BA_WAS_SEL)) {
					if(G.scene->basact==base) BIF_ThemeColor(TH_GROUP_ACTIVE);
					else BIF_ThemeColorShade(TH_GROUP_ACTIVE, -16); 
				}
				else BIF_ThemeColor(TH_GROUP);
				colindex= 0;
			}

		}	

		if(colindex) {
			col= colortab[colindex];
			cpack(col);
		}
	}

	/* maximum drawtype */
	dt= MIN2(G.vd->drawtype, ob->dt);
	if(G.vd->zbuf==0 && dt>OB_WIRE) dt= OB_WIRE;
	dtx= 0;

	/* faceselect exception: also draw solid when dt==wire, except in editmode */
	if(ob==OBACT && (G.f & (G_FACESELECT+G_VERTEXPAINT+G_TEXTUREPAINT+G_WEIGHTPAINT))) {
		if(ob->type==OB_MESH) {

			if(ob==G.obedit);
			else {
				dt= OB_SHADED;

				glClearDepth(1.0); glClear(GL_DEPTH_BUFFER_BIT);
				glEnable(GL_DEPTH_TEST);
				if(dt<OB_SOLID) zbufoff= 1;
			}
		}
		else {
			if(dt<OB_SOLID) {
				dt= OB_SOLID;
				glClearDepth(1.); glClear(GL_DEPTH_BUFFER_BIT);
				glEnable(GL_DEPTH_TEST);
				zbufoff= 1;
			}
		}
	}
	
	/* draw-extra supported for boundbox drawmode too */
	if(dt>=OB_BOUNDBOX ) {

		dtx= ob->dtx;
		if(G.obedit==ob) {
			// the only 2 extra drawtypes alowed in editmode
			dtx= dtx & (OB_DRAWWIRE|OB_TEXSPACE);
		}

		if(G.f & G_DRAW_EXT) {
			if(ob->type==OB_EMPTY || ob->type==OB_CAMERA || ob->type==OB_LAMP) dt= OB_WIRE;
		}
	}

	/* draw outline for selected solid objects, mesh does itself */
	if((G.vd->flag & V3D_SELECT_OUTLINE) && ob->type!=OB_MESH) {
		if(dt>OB_WIRE && dt<OB_TEXTURE && ob!=G.obedit) {
			if (!(ob->dtx&OB_DRAWWIRE) && (ob->flag&SELECT) && !(flag&DRAW_PICKING)) {
				drawSolidSelect(base);
			}
		}
	}

	switch( ob->type) {
		case OB_MESH:
			if (!(base->flag&OB_RADIO)) {
				empty_object= draw_mesh_object(base, dt, flag);
				if(flag!=DRAW_CONSTCOLOR) dtx &= ~OB_DRAWWIRE; // mesh draws wire itself

				if(G.obedit!=ob && warning_recursive==0) {
					PartEff *paf = give_parteff(ob);

					if(paf) {
						if(col || (ob->flag & SELECT)) cpack(0xFFFFFF);	/* for visibility, also while wpaint */
						if(paf->flag & PAF_STATIC) draw_static_particle_system(ob, paf, dt);
						else if((flag & DRAW_PICKING) == 0) draw_particle_system(base, paf);	// selection errors happen to easy
						if(col) cpack(col);
					}
				}
			}

			break;
		case OB_FONT:
			cu= ob->data;
			if (cu->disp.first==NULL) makeDispListCurveTypes(ob, 0);
			if(ob==G.obedit) {
				tekentextcurs();

				if (cu->flag & CU_FAST) {
					cpack(0xFFFFFF);
					set_inverted_drawing(1);
					drawDispList(base, OB_WIRE);
					set_inverted_drawing(0);
				} else {
					drawDispList(base, dt);
				}

				if (cu->linewidth != 0.0) {
					cpack(0xff44ff);
					BIF_ThemeColor(TH_WIRE);
					VECCOPY(vec1, ob->orig);
					VECCOPY(vec2, ob->orig);
					vec1[0] += cu->linewidth;
					vec2[0] += cu->linewidth;
					vec1[1] += cu->linedist * cu->fsize;
					vec2[1] -= cu->lines * cu->linedist * cu->fsize;
					setlinestyle(3);
					glBegin(GL_LINE_STRIP); 
					glVertex2fv(vec1); 
					glVertex2fv(vec2); 
					glEnd();
					setlinestyle(0);
				}

				setlinestyle(3);
				for (i=0; i<cu->totbox; i++) {
					if (cu->tb[i].w != 0.0) {
						if (i == (cu->actbox-1))
							BIF_ThemeColor(TH_ACTIVE);
						else
							BIF_ThemeColor(TH_WIRE);
						vec1[0] = cu->tb[i].x;
						vec1[1] = cu->tb[i].y + cu->fsize;
						vec1[2] = 0.001;
						glBegin(GL_LINE_STRIP);
						glVertex3fv(vec1);
						vec1[0] += cu->tb[i].w;
						glVertex3fv(vec1);
						vec1[1] -= cu->tb[i].h;
						glVertex3fv(vec1);
						vec1[0] -= cu->tb[i].w;
						glVertex3fv(vec1);
						vec1[1] += cu->tb[i].h;
						glVertex3fv(vec1);
						glEnd();
					}
				}
				setlinestyle(0);


				if (getselection(&selstart, &selend) && selboxes) {
					float selboxw;

					cpack(0xffffff);
					set_inverted_drawing(1);	    	
					for (i=0; i<(selend-selstart+1); i++) {
						SelBox *sb = &(selboxes[i]);

						if (i<(selend-selstart)) {
							if (selboxes[i+1].y == sb->y)
								selboxw= selboxes[i+1].x - sb->x;
							else
								selboxw= sb->w;
						}
						else {
							selboxw= sb->w;
						}
						glBegin(GL_QUADS);
						glVertex3f(sb->x, sb->y, 0.001);
						glVertex3f(sb->x+selboxw, sb->y, 0.001);	    			
						glVertex3f(sb->x+selboxw, sb->y+sb->h, 0.001);
						glVertex3f(sb->x, sb->y+sb->h, 0.001);
						glEnd();
					}
					set_inverted_drawing(0);	    		
				}
			}
			else if(dt==OB_BOUNDBOX) 
				draw_bounding_volume(ob);
			else if(boundbox_clip(ob->obmat, cu->bb)) 
				empty_object= drawDispList(base, dt);

			break;
		case OB_CURVE:
		case OB_SURF:
			cu= ob->data;
			/* still needed for curves hidden in other layers. depgraph doesnt handle that yet */
			if (cu->disp.first==NULL) makeDispListCurveTypes(ob, 0);

			if(ob==G.obedit) {
				drawnurb(base, editNurb.first, dt);
			}
			else if(dt==OB_BOUNDBOX) 
				draw_bounding_volume(ob);
			else if(boundbox_clip(ob->obmat, cu->bb)) {
				empty_object= drawDispList(base, dt);
				
				if(cu->path)
					curve_draw_speed(ob);
			}			
			break;
		case OB_MBALL:
			if(ob==G.obedit) 
				drawmball(base, dt);
			else if(dt==OB_BOUNDBOX) 
				draw_bounding_volume(ob);
			else 
				empty_object= drawmball(base, dt);
			break;
		case OB_EMPTY:
			drawaxes(ob->empty_drawsize, flag, ob->empty_drawtype);
			break;
		case OB_LAMP:
			drawlamp(ob);
			if(dtx || (base->flag & SELECT)) mymultmatrix(ob->obmat);
			break;
		case OB_CAMERA:
			drawcamera(ob, flag);
			break;
		case OB_LATTICE:
			drawlattice(ob);
			break;
		case OB_ARMATURE:
			if(dt>OB_WIRE) set_gl_material(0);	// we use defmaterial
			empty_object= draw_armature(base, dt);
			break;
		default:
			drawaxes(1.0, flag, OB_ARROWS);
	}
	if(ob->pd && ob->pd->forcefield) draw_forcefield(ob);

	{
		bConstraint *con;
		for(con=ob->constraints.first; con; con= con->next) 
		{
			if(con->type==CONSTRAINT_TYPE_RIGIDBODYJOINT) 
			{
				bRigidBodyJointConstraint *data = (bRigidBodyJointConstraint*)con->data;
				if(data->flag&CONSTRAINT_DRAW_PIVOT)
					drawRBpivot(data);
			}
		}
	}

	/* draw extra: after normal draw because of makeDispList */
	if(dtx) {
		if(G.f & G_SIMULATION);
		else if(dtx & OB_AXIS) {
			drawaxes(1.0f, flag, OB_ARROWS);
		}
		if(dtx & OB_BOUNDBOX) draw_bounding_volume(ob);
		if(dtx & OB_TEXSPACE) drawtexspace(ob);
		if(dtx & OB_DRAWNAME) {
			/* patch for several 3d cards (IBM mostly) that crash on glSelect with text drawing */
			/* but, we also dont draw names for sets or duplicators */
			if(flag == 0) {
				glRasterPos3f(0.0,  0.0,  0.0);

				BMF_DrawString(G.font, " ");
				BMF_DrawString(G.font, ob->id.name+2);
			}
		}
		if(dtx & OB_DRAWIMAGE) drawDispListwire(&ob->disp);
		if((dtx & OB_DRAWWIRE) && dt>=OB_SOLID) drawWireExtra(ob);
	}

	if(dt<OB_SHADED) {
		if((ob->gameflag & OB_ACTOR) && (ob->gameflag & OB_DYNAMIC)) {
			float tmat[4][4], imat[4][4], vec[3];

			vec[0]= vec[1]= vec[2]= 0.0;
			mygetmatrix(tmat);
			Mat4Invert(imat, tmat);

			setlinestyle(2);
			drawcircball(GL_LINE_LOOP, vec, ob->inertia, imat);
			setlinestyle(0);
		}
	}

	myloadmatrix(G.vd->viewmat);

	if(zbufoff) glDisable(GL_DEPTH_TEST);

	if(warning_recursive) return;
	if(base->flag & (OB_FROMDUPLI|OB_RADIO)) return;
	if(G.f & G_SIMULATION) return;

	/* object centers, need to be drawn in viewmat space for speed, but OK for picking select */
	if(ob!=OBACT || (G.f & (G_VERTEXPAINT|G_FACESELECT|G_TEXTUREPAINT|G_WEIGHTPAINT))==0) {
		int do_draw_center= -1;	/* defines below are zero or positive... */

		if((G.scene->basact)==base) 
			do_draw_center= ACTIVE;
		else if(base->flag & SELECT) 
			do_draw_center= SELECT;
		else if(empty_object || (G.vd->flag & V3D_DRAW_CENTERS)) 
			do_draw_center= DESELECT;

		if(do_draw_center != -1) {
			if(flag & DRAW_PICKING) {
				/* draw a single point for opengl selection */
				glBegin(GL_POINTS);
				glVertex3fv(ob->obmat[3]);
				glEnd();
			} 
			else if((flag & DRAW_CONSTCOLOR)==0) {
				/* we don't draw centers for duplicators and sets */
#ifdef WITH_VERSE
				if(ob->vnode)
					drawcentercircle(ob->obmat[3], VERSE, 1);
				else
#endif
					drawcentercircle(ob->obmat[3], do_draw_center, ob->id.lib || ob->id.us>1);
			}
		}
	}

	/* not for sets, duplicators or picking */
	if(flag==0 && (!(G.vd->flag & V3D_HIDE_HELPLINES))) {
		ListBase *list;

		/* draw hook center and offset line */
		if(ob!=G.obedit) draw_hooks(ob);

		/* help lines and so */
		if(ob!=G.obedit && ob->parent && (ob->parent->lay & G.vd->lay)) {
			setlinestyle(3);
			glBegin(GL_LINES);
			glVertex3fv(ob->obmat[3]);
			glVertex3fv(ob->orig);
			glEnd();
			setlinestyle(0);
		}

		/* Drawing the constraint lines */
		list = &ob->constraints;
		if (list){
			/*
		 extern void make_axis_color(char *col, char *col2, char axis);	// drawview.c
		 */
			bConstraint *curcon;
			float size[3], tmat[4][4];
			char col[4], col2[4];

			BIF_GetThemeColor3ubv(TH_GRID, col);
			make_axis_color(col, col2, 'z');
			glColor3ubv((GLubyte *)col2);

			for (curcon = list->first; curcon; curcon=curcon->next){
				if ((curcon->flag & CONSTRAINT_EXPAND)&&(curcon->type!=CONSTRAINT_TYPE_NULL)&&(constraint_has_target(curcon))){
					get_constraint_target_matrix(curcon, TARGET_OBJECT, NULL, tmat, size, bsystem_time(ob, 0, (float)(G.scene->r.cfra), ob->sf));
					setlinestyle(3);
					glBegin(GL_LINES);
					glVertex3fv(tmat[3]);
					glVertex3fv(ob->obmat[3]);
					glEnd();
					setlinestyle(0);
				}
			}
		}
	}

	free_old_images();
}

void draw_object_ext(Base *base)
{
	
	if(G.vd==NULL || base==NULL) return;
	
	if(G.vd->drawtype > OB_WIRE) {
		G.vd->zbuf= 1;
		glEnable(GL_DEPTH_TEST);
	}
	
	G.f |= G_DRAW_EXT;

	glDrawBuffer(GL_FRONT);
	persp(PERSP_VIEW);

	if(G.vd->flag & V3D_CLIPPING)
		view3d_set_clipping(G.vd);
	
	draw_object(base, 0);

	if(G.vd->flag & V3D_CLIPPING)
		view3d_clr_clipping();
	
	G.f &= ~G_DRAW_EXT;

	bglFlush();		/* reveil frontbuffer drawing */
	glDrawBuffer(GL_BACK);
	
	if(G.vd->zbuf) {
		G.vd->zbuf= 0;
		glDisable(GL_DEPTH_TEST);
	}
	curarea->win_swap= WIN_FRONT_OK;
}

/* ***************** BACKBUF SEL (BBS) ********* */

static void bbs_mesh_verts__mapFunc(void *userData, int index, float *co, float *no_f, short *no_s)
{
	int offset = (long) userData;
	EditVert *eve = EM_get_vert_for_index(index);

	if (eve->h==0) {
		set_framebuffer_index_color(offset+index);
		bglVertex3fv(co);
	}
}
static int bbs_mesh_verts(DerivedMesh *dm, int offset)
{
	glPointSize( BIF_GetThemeValuef(TH_VERTEX_SIZE) );
	bglBegin(GL_POINTS);
	dm->foreachMappedVert(dm, bbs_mesh_verts__mapFunc, (void*)(long) offset);
	bglEnd();
	glPointSize(1.0);

	return offset + G.totvert;
}		

static int bbs_mesh_wire__setDrawOptions(void *userData, int index)
{
	int offset = (long) userData;
	EditEdge *eed = EM_get_edge_for_index(index);

	if (eed->h==0) {
		set_framebuffer_index_color(offset+index);
		return 1;
	} else {
		return 0;
	}
}
static int bbs_mesh_wire(DerivedMesh *dm, int offset)
{
	dm->drawMappedEdges(dm, bbs_mesh_wire__setDrawOptions, (void*)(long) offset);

	return offset + G.totedge;
}		

static int bbs_mesh_solid__setSolidDrawOptions(void *userData, int index, int *drawSmooth_r)
{
	if (EM_get_face_for_index(index)->h==0) {
		if (userData) {
			set_framebuffer_index_color(index+1);
		}
		return 1;
	} else {
		return 0;
	}
}

static void bbs_mesh_solid__drawCenter(void *userData, int index, float *cent, float *no)
{
	EditFace *efa = EM_get_face_for_index(index);

	if (efa->h==0 && efa->fgonf!=EM_FGON) {
		set_framebuffer_index_color(index+1);

		bglVertex3fv(cent);
	}
}

/* two options, facecolors or black */
static int bbs_mesh_solid_EM(DerivedMesh *dm, int facecol)
{
	cpack(0);

	if (facecol) {
		dm->drawMappedFaces(dm, bbs_mesh_solid__setSolidDrawOptions, (void*)(long) 1, 0);

		if(G.scene->selectmode & SCE_SELECT_FACE) {
			glPointSize(BIF_GetThemeValuef(TH_FACEDOT_SIZE));
		
			bglBegin(GL_POINTS);
			dm->foreachMappedFaceCenter(dm, bbs_mesh_solid__drawCenter, NULL);
			bglEnd();
		}

		return 1+G.totface;
	} else {
		dm->drawMappedFaces(dm, bbs_mesh_solid__setSolidDrawOptions, (void*) 0, 0);
		return 1;
	}
}

static int bbs_mesh_solid__setDrawOpts(void *userData, int index, int *drawSmooth_r)
{
	Mesh *me = userData;

	if (!me->mtface || !(me->mface[index].flag&ME_HIDE)) {
		set_framebuffer_index_color(index+1);
		return 1;
	} else {
		return 0;
	}
}

static int bbs_mesh_wire__setDrawOpts(void *userData, int index)
{
	struct { Mesh *me; EdgeHash *eh; int offset; } *data = userData;
	MEdge *med = data->me->medge + index;
	unsigned long flags = (long)BLI_edgehash_lookup(data->eh, med->v1, med->v2);

	if (flags & 1) {
		set_framebuffer_index_color(data->offset+index);
		return 1;
	} else
		return 0;
}

static void bbs_mesh_solid(Object *ob)
{
	DerivedMesh *dm = mesh_get_derived_final(ob, get_viewedit_datamask());
	Mesh *me = (Mesh*)ob->data;
	
	glColor3ub(0, 0, 0);
	dm->drawMappedFaces(dm, bbs_mesh_solid__setDrawOpts, me, 0);

	/* draw edges for seam marking in faceselect mode, but not when painting,
	   so that painting doesn't get interrupted on an edge */
	if ((G.f & G_FACESELECT) && !(G.f & (G_VERTEXPAINT|G_TEXTUREPAINT|G_WEIGHTPAINT)) && me->mtface) {
		struct { Mesh *me; EdgeHash *eh; int offset; } userData;

		userData.me = me;
		userData.eh = get_tface_mesh_marked_edge_info(me);
		userData.offset = userData.me->totface+1;

		bglPolygonOffset(1.0);
		dm->drawMappedEdges(dm, bbs_mesh_wire__setDrawOpts, (void*)&userData);
		bglPolygonOffset(0.0);

		BLI_edgehash_free(userData.eh, NULL);
	}

	dm->release(dm);
}

void draw_object_backbufsel(Object *ob)
{

	mymultmatrix(ob->obmat);

	glClearDepth(1.0); glClear(GL_DEPTH_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);

	switch( ob->type) {
	case OB_MESH:
		if(ob==G.obedit) {
			DerivedMesh *dm = editmesh_get_derived_cage(CD_MASK_BAREMESH);

			EM_init_index_arrays(1, 1, 1);

			em_solidoffs= bbs_mesh_solid_EM(dm, G.scene->selectmode & SCE_SELECT_FACE);
			
			bglPolygonOffset(1.0);
			
			// we draw edges always, for loop (select) tools
			em_wireoffs= bbs_mesh_wire(dm, em_solidoffs);

			// we draw verts if vert select mode or if in transform (for snap).
			if(G.scene->selectmode & SCE_SELECT_VERTEX || G.moving & G_TRANSFORM_EDIT) 
				em_vertoffs= bbs_mesh_verts(dm, em_wireoffs);
			else em_vertoffs= em_wireoffs;
			
			bglPolygonOffset(0.0);

			dm->release(dm);

			EM_free_index_arrays();
		}
		else bbs_mesh_solid(ob);

		break;
	case OB_CURVE:
	case OB_SURF:
		break;
	}

	myloadmatrix(G.vd->viewmat);
}


/* ************* draw object instances for bones, for example ****************** */
/*               assumes all matrices/etc set OK */

void draw_object_instance(Object *ob, int dt, int outline)
{
	DerivedMesh *dm=NULL, *edm=NULL;
	
	if(ob==NULL || ob->type!=OB_MESH) return;
	
	if(G.obedit && ob->data==G.obedit->data)
		edm= editmesh_get_derived_base();
	else 
		dm = mesh_get_derived_final(ob, CD_MASK_BAREMESH);

	if(dt<=OB_WIRE) {
		if(dm)
			dm->drawEdges(dm, 1);
		else if(edm)
			edm->drawEdges(edm, 1);	
	}
	else {
		if(outline)
			draw_mesh_object_outline(ob, dm?dm:edm);

		if(dm)
			init_gl_materials(ob, 0);
		else {
			glEnable(GL_COLOR_MATERIAL);
			BIF_ThemeColor(TH_BONE_SOLID);
			glDisable(GL_COLOR_MATERIAL);
		}
		
		glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, 0);
		glFrontFace((ob->transflag&OB_NEG_SCALE)?GL_CW:GL_CCW);
		glEnable(GL_LIGHTING);
		
		if(dm)
			dm->drawFacesSolid(dm, set_gl_material);
		else if(edm)
			edm->drawMappedFaces(edm, NULL, NULL, 0);
		
		glDisable(GL_LIGHTING);
	}

	if(edm) edm->release(edm);
	if(dm) dm->release(dm);
}

