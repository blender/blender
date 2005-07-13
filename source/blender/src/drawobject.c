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
#include "DNA_object_types.h"
#include "DNA_object_force.h"
#include "DNA_space_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_view3d_types.h"
#include "DNA_world_types.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_editVert.h"

#include "BKE_utildefines.h"
#include "BKE_curve.h"
#include "BKE_constraint.h" // for the get_constraint_target function
#include "BKE_deform.h"		// lattice_modifier()
#include "BKE_DerivedMesh.h"
#include "BKE_displist.h"
#include "BKE_effect.h"
#include "BKE_font.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_ipo.h"
#include "BKE_lattice.h"
#include "BKE_mesh.h"
#include "BKE_material.h"
#include "BKE_mball.h"
#include "BKE_object.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"
#include "BIF_mywindow.h"
#include "BIF_screen.h"
#include "BIF_space.h"
#include "BIF_editarmature.h"
#include "BIF_editmesh.h"
#include "BIF_glutil.h"
#include "BIF_resources.h"

#include "BDR_drawmesh.h"
#include "BDR_drawobject.h"
#include "BDR_editobject.h"

#include "BSE_view.h"
#include "BSE_drawview.h"
#include "BSE_trans_types.h"

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

	/***/

// Materials start counting at # one....
#define MAXMATBUF (MAXMAT + 1)
static float matbuf[MAXMATBUF][2][4];
static int last_gl_matnr=-1;

static void init_gl_materials(Object *ob)
{
	extern Material defmaterial;	// render module abuse...
	Material *ma;
	int a;
	
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
		VECCOPY(matbuf[1][0], matbuf[0][0]);
		VECCOPY(matbuf[1][1], matbuf[0][1]);
	}
	
	for(a=1; a<=ob->totcol; a++) {
		ma= give_current_material(ob, a);
		if(ma==NULL) ma= &defmaterial;
		if(a<MAXMATBUF) {
			matbuf[a][0][0]= (ma->ref+ma->emit)*ma->r;
			matbuf[a][0][1]= (ma->ref+ma->emit)*ma->g;
			matbuf[a][0][2]= (ma->ref+ma->emit)*ma->b;
			matbuf[a][0][3]= 1.0;
			
			matbuf[a][1][0]= ma->spec*ma->specr;
			matbuf[a][1][1]= ma->spec*ma->specg;
			matbuf[a][1][2]= ma->spec*ma->specb;
			matbuf[a][1][3]= 1.0;
		}
	}

	last_gl_matnr = -1;
}

static int set_gl_material(int nr)
{
	if(nr<MAXMATBUF && nr!=last_gl_matnr) {
		glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, matbuf[nr][0]);
		glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, matbuf[nr][1]);
		last_gl_matnr = nr;
	}

	return 1;
}

	/***/
	
unsigned int rect_desel[16]= {0x707070,0x0,0x0,0x707070,0x407070,0x70cccc,0x407070,0x0,0xaaffff,0xffffff,0x70cccc,0x0,0x70cccc,0xaaffff,0x407070,0x707070};
unsigned int rect_sel[16]= {0x707070,0x0,0x0,0x707070,0x702070,0xcc50cc,0x702070,0x0,0xff80ff,0xffffff,0xcc50cc,0x0,0xcc50cc,0xff80ff,0x702070,0x707070};

unsigned int rectu_desel[16]= {0xff4e4e4e,0xff5c2309,0xff000000,0xff4e4f4d,0xff000000,0xffff9d72,0xffff601c,0xff000000,0xff5d2409,0xffffffff,0xffff9d72,0xff5b2209,0xff4e4e4e,0xff5c2309,0xff010100,0xff4f4f4f};
unsigned int rectu_sel[16]= {0xff4e4e4e,0xff403c00,0xff000000,0xff4e4e4d,0xff000000,0xfffff64c,0xffaaa100,0xff000000,0xff403c00,0xffffffff,0xfffff64c,0xff403c00,0xff4f4f4f,0xff403c00,0xff010100,0xff4e4e4e};

unsigned int rectl_desel[81]= {0x777777,0x777777,0xa9fefe,0xaaffff,0xaaffff,0xaaffff,0xaaffff,0x777777,0x777777,0x777777,0xa9fefe,0xaafefe,0x777777,0x777777,0x777777,0xa9fefe,0xa9fefe,0x777777,0xaaffff,0xa9fefe,0x4e4e4e,0x0,0x124040,0x0,0x4e4e4e,0xaafefe,0xaaffff,0xaaffff,0x777777,0x0,0x227777,0x55cccc,0x227777,0x0,0x777777,0xaaffff,0xaaffff,0x777777,0x124040,0x88ffff,0xffffff,0x55cccc,0x124040,0x777777,0xaaffff,0xaaffff,0x777777,0x0,0x55cccc,0x88ffff,0x227777,0x0,0x777777,0xaaffff,0xaafefe,0xaafefe,0x4f4f4f,0x0,0x124040,0x0,0x4e4e4e,0xa9fefe,0xaaffff,0x777777,0xa9fefe,0xa9fefe,0x777777,0x777777,0x777777,0xa9fefe,0xa9fefe,0x777777,0x777777,0x777777,0xa9fefe,0xa9fefe,0xaaffff,0xaaffff,0xaaffff,0x777777,0x777777};
unsigned int rectl_sel[81]= {0x777777,0x777777,0xffaaff,0xffaaff,0xffaaff,0xffaaff,0xffaaff,0x777777,0x777777,0x777777,0xffaaff,0xffaaff,0x777777,0x777777,0x777777,0xffaaff,0xffaaff,0x777777,0xffaaff,0xffaaff,0x4e4e4e,0x10101,0x402440,0x0,0x4e4e4e,0xffaaff,0xffaaff,0xffaaff,0x777777,0x0,0x774477,0xcc77cc,0x774477,0x0,0x777777,0xffaaff,0xffaaff,0x777777,0x402440,0xffaaff,0xffffff,0xcc77cc,0x412541,0x777777,0xffaaff,0xffaaff,0x777777,0x10101,0xcc77cc,0xffaaff,0x774477,0x0,0x777777,0xffaaff,0xffaaff,0xffaaff,0x4e4e4e,0x10101,0x402440,0x0,0x4e4e4e,0xffaaff,0xffaaff,0x777777,0xffaaff,0xffaaff,0x777777,0x777777,0x777777,0xffaaff,0xffaaff,0x777777,0x777777,0x777777,0xffaaff,0xffaaff,0xffaaff,0xffaaff,0xffaaff,0x777777,0x777777};
unsigned int rectlus_desel[81]= {0x777777,0x777777,0xa9fefe,0xaaffff,0xaaffff,0xaaffff,0xaaffff,0x777777,0x777777,0x777777,0xa9fefe,0xaafefe,0x777777,0x777777,0x777777,0xa9fefe,0xa9fefe,0x777777,0xaaffff,0xa9fefe,0x4e4e4e,0x0,0x5c2309,0x0,0x4e4f4d,0xaafefe,0xaaffff,0xaaffff,0x777777,0x0,0xff601c,0xff9d72,0xff601c,0x0,0x777777,0xaaffff,0xaaffff,0x777777,0x5d2409,0xffceb8,0xff9d72,0xff9d72,0x5b2209,0x777777,0xaaffff,0xaaffff,0x777777,0x10100,0xffceb8,0xffceb8,0xff601c,0x0,0x777777,0xaaffff,0xaafefe,0xaafefe,0x4e4e4e,0x0,0x5c2309,0x10100,0x4f4f4f,0xa9fefe,0xaaffff,0x777777,0xa9fefe,0xa9fefe,0x777777,0x777777,0x777777,0xa9fefe,0xa9fefe,0x777777,0x777777,0x777777,0xa9fefe,0xa9fefe,0xaaffff,0xaaffff,0xaaffff,0x777777,0x777777};
unsigned int rectlus_sel[81]= {0x777777,0x777777,0xffaaff,0xffaaff,0xffaaff,0xffaaff,0xffaaff,0x777777,0x777777,0x777777,0xffaaff,0xffaaff,0x777777,0x777777,0x777777,0xffaaff,0xffaaff,0x777777,0xffaaff,0xffaaff,0x4e4e4e,0x10100,0x403c00,0x0,0x4e4e4d,0xffaaff,0xffaaff,0xffaaff,0x777777,0x0,0xaaa100,0xfff64c,0xaaa100,0x0,0x777777,0xffaaff,0xffaaff,0x777777,0x403c00,0xfffde2,0xffffff,0xfff64c,0x403c00,0x777777,0xffaaff,0xffaaff,0x777777,0x10100,0xfff64c,0xfffde2,0xaaa100,0x0,0x777777,0xffaaff,0xffaaff,0xffaaff,0x4f4f4f,0x0,0x403c00,0x10100,0x4e4e4e,0xffaaff,0xffaaff,0x777777,0xffaaff,0xffaaff,0x777777,0x777777,0x777777,0xffaaff,0xffaaff,0x777777,0x777777,0x777777,0xffaaff,0xffaaff,0xffaaff,0xffaaff,0xffaaff,0x777777,0x777777};
unsigned int rectllib_desel[81]= {0xff777777,0xff777777,0xb9b237,0xb9b237,0xb9b237,0xb9b237,0xb9b237,0xff777777,0xff777777,0xff777777,0xb9b237,0xb9b237,0xff777777,0xff777777,0xff777777,0xb9b237,0xb9b237,0xff777777,0xb9b237,0xb9b237,0x4e4e4e,0x0,0x5c2309,0x0,0x4e4f4d,0xb9b237,0xb9b237,0xb9b237,0xff777777,0x0,0xff601c,0xff9d72,0xff601c,0x0,0xff777777,0xb9b237,0xb9b237,0xff777777,0x5d2409,0xffceb8,0xff9d72,0xff9d72,0x5b2209,0xff777777,0xb9b237,0xb9b237,0xff777777,0x10100,0xffceb8,0xffceb8,0xff601c,0x0,0xff777777,0xb9b237,0xb9b237,0xb9b237,0x4e4e4e,0x0,0x5c2309,0x10100,0x4f4f4f,0xb9b237,0xb9b237,0xff777777,0xb9b237,0xb9b237,0xff777777,0xff777777,0xff777777,0xb9b237,0xb9b237,0xff777777,0xff777777,0xff777777,0xb9b237,0xb9b237,0xb9b237,0xb9b237,0xb9b237,0xff777777,0xff777777};
unsigned int rectllib_sel[81]= {0xff777777,0xff777777,0xfff64c,0xfff64c,0xfff64c,0xfff64c,0xfff64c,0xff777777,0xff777777,0xff777777,0xfff64c,0xfff64c,0xff777777,0xff777777,0xff777777,0xfff64c,0xfff64c,0xff777777,0xfff64c,0xfff64c,0x4e4e4e,0x10100,0x403c00,0x0,0x4e4e4d,0xfff64c,0xfff64c,0xfff64c,0xff777777,0x0,0xaaa100,0xfff64c,0xaaa100,0x0,0xff777777,0xfff64c,0xfff64c,0xff777777,0x403c00,0xfffde2,0xffffff,0xfff64c,0x403c00,0xff777777,0xfff64c,0xfff64c,0xff777777,0x10100,0xfff64c,0xfffde2,0xaaa100,0x0,0xff777777,0xfff64c,0xfff64c,0xfff64c,0x4f4f4f,0x0,0x403c00,0x10100,0x4e4e4e,0xfff64c,0xfff64c,0xff777777,0xfff64c,0xfff64c,0xff777777,0xff777777,0xff777777,0xfff64c,0xfff64c,0xff777777,0xff777777,0xff777777,0xfff64c,0xfff64c,0xfff64c,0xfff64c,0xfff64c,0xff777777,0xff777777};

unsigned int rectl_set[81]= {0xff777777,0xff777777,0xaaaaaa,0xaaaaaa,0xaaaaaa,0xaaaaaa,0xaaaaaa,0xff777777,0xff777777,0xff777777,0xaaaaaa,0xaaaaaa,0xff777777,0xff777777,0xff777777,0xaaaaaa,0xaaaaaa,0xff777777,0xaaaaaa,0xaaaaaa,0x4e4e4e,0x10100,0x202020,0x0,0x4e4e4d,0xaaaaaa,0xaaaaaa,0xaaaaaa,0xff777777,0x0,0xaaa100,0xaaaaaa,0xaaa100,0x0,0xff777777,0xaaaaaa,0xaaaaaa,0xff777777,0x202020,0xfffde2,0xffffff,0xaaaaaa,0x202020,0xff777777,0xaaaaaa,0xaaaaaa,0xff777777,0x10100,0xaaaaaa,0xfffde2,0xaaa100,0x0,0xff777777,0xaaaaaa,0xaaaaaa,0xaaaaaa,0x4f4f4f,0x0,0x202020,0x10100,0x4e4e4e,0xaaaaaa,0xaaaaaa,0xff777777,0xaaaaaa,0xaaaaaa,0xff777777,0xff777777,0xff777777,0xaaaaaa,0xaaaaaa,0xff777777,0xff777777,0xff777777,0xaaaaaa,0xaaaaaa,0xaaaaaa,0xaaaaaa,0xaaaaaa,0xff777777,0xff777777};


static unsigned int colortab[24]=
	{0x0,		0xFF88FF, 0xFFBBFF, 
	 0x403000,	0xFFFF88, 0xFFFFBB, 
	 0x104040,	0x66CCCC, 0x77CCCC, 
	 0x101040,	0x5588FF, 0x88BBFF, 
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

void init_draw_rects(void)
{
	if(G.order==B_ENDIAN) {
		IMB_convert_rgba_to_abgr(16, rect_desel);
		IMB_convert_rgba_to_abgr(16, rect_sel);
		
		IMB_convert_rgba_to_abgr(16, rectu_desel);
		IMB_convert_rgba_to_abgr(16, rectu_sel);
		
		IMB_convert_rgba_to_abgr(81, rectl_desel);
		IMB_convert_rgba_to_abgr(81, rectl_sel);
	
		IMB_convert_rgba_to_abgr(81, rectlus_desel);
		IMB_convert_rgba_to_abgr(81, rectlus_sel);
	
		IMB_convert_rgba_to_abgr(81, rectllib_desel);
		IMB_convert_rgba_to_abgr(81, rectllib_sel);
				
		IMB_convert_rgba_to_abgr(81, rectl_set);
	}
}

static void draw_icon_centered(float *pos, unsigned int *rect, int rectsize) 
{
	float hsize= (float) rectsize/2.0;
	GLubyte dummy= 0;
	
	glRasterPos3fv(pos);
	
		/* use bitmap to shift rasterpos in pixels */
	glBitmap(0, 0, 0.0, 0.0, -hsize, -hsize, &dummy);
#if defined (__sun__) || defined ( __sun ) || defined (__sparc) || defined (__sparc__)
	glFlush(); 
#endif	
	glDrawPixels(rectsize, rectsize, GL_RGBA, GL_UNSIGNED_BYTE, rect);
}

/* bad frontbuffer call... because it is used in transform after force_draw() */
void helpline(float *vec)
{
	float vecrot[3], cent[2];
	short mval[2];

	VECCOPY(vecrot, vec);
	if(G.obedit) Mat4MulVecfl(G.obedit->obmat, vecrot);
	else if(G.obpose) Mat4MulVecfl(G.obpose->obmat, vecrot);

	getmouseco_areawin(mval);
	project_float(vecrot, cent);	// no overflow in extreme cases
	if(cent[0]!=3200.0f) {
		persp(PERSP_WIN);
		
		glDrawBuffer(GL_FRONT);
		
		BIF_ThemeColor(TH_WIRE);

		setlinestyle(3);
		glBegin(GL_LINE_STRIP); 
			glVertex2sv(mval); 
			glVertex2fv(cent); 
		glEnd();
		setlinestyle(0);
		
		persp(PERSP_VIEW);
		glFlush(); // flush display for frontbuffer
		glDrawBuffer(GL_BACK);
	}
}

void drawaxes(float size)
{
	int axis;

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
		if((G.f & G_PICKSEL) == 0) {
			if (axis==0)
				BMF_DrawString(G.font, "x");
			else if (axis==1)
				BMF_DrawString(G.font, "y");
			else
				BMF_DrawString(G.font, "z");
		}
	}
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

static void tekenshadbuflimits(Lamp *la, float mat[][4])
{
	float sta[3], end[3], lavec[3];

	lavec[0]= -mat[2][0];
	lavec[1]= -mat[2][1];
	lavec[2]= -mat[2][2];
	Normalise(lavec);

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
	float temp[3],plane[3],mat1[3][3],mat2[3][3],mat3[3][3],mat4[3][3],q[4],co,si,hoek;

	Normalise(lvec);
	Normalise(vvec);				/* is this the correct vector ? */

	Crossf(temp,vvec,lvec);		/* equation for a plane through vvec en lvec */
	Crossf(plane,lvec,temp);		/* a plane perpendicular to this, parrallel with lvec */

	Normalise(plane);

	/* now we've got two equations: one of a cone and one of a plane, but we have
	three unknowns. We remove one unkown by rotating the plane to z=0 (the plane normal) */

	/* rotate around cross product vector of (0,0,1) and plane normal, dot product degrees */
	/* according definition, we derive cross product is (plane[1],-plane[0],0), en cos = plane[2]);*/

	/* translating this comment to english didnt really help me understanding the math! :-) (ton) */
	
	q[1] = plane[1] ; 
	q[2] = -plane[0] ; 
	q[3] = 0 ;
	Normalise(&q[1]);

	hoek = saacos(plane[2])/2.0;
	co = cos(hoek);
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
	float vec[3], lvec[3], vvec[3],x,y,z;
	
	la= ob->data;
	vec[0]=vec[1]=vec[2]= 0.0;
	
	setlinestyle(4);
	
	/* yafray: for photonlight also draw lightcone as for spot */
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

		glBegin(GL_LINE_STRIP);
			glVertex3fv(vvec);
			glVertex3fv(vec);
			glVertex3fv(lvec);
		glEnd();
		
		z = x*sqrt(1.0 - y*y);
		x *= y;

		glTranslatef(0.0 ,  0.0 ,  x);
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
		
	}
	else if ELEM(la->type, LA_HEMI, LA_SUN) {
		glBegin(GL_LINE_STRIP);
			glVertex3fv(vec); 
			vec[2]= -la->dist; 
			glVertex3fv(vec);
		glEnd();
	}
	else {
		if(la->type==LA_AREA) {
			setlinestyle(0);
			if(la->area_shape==LA_AREA_SQUARE) 
				fdrawbox(-la->area_size*0.5, -la->area_size*0.5, la->area_size*0.5, la->area_size*0.5);
			else if(la->area_shape==LA_AREA_RECT) 
				fdrawbox(-la->area_size*0.5, -la->area_sizey*0.5, la->area_size*0.5, la->area_sizey*0.5);
			setlinestyle(3);
			glBegin(GL_LINE_STRIP); 
			glVertex3f(0.0,0.0,0.0);
			glVertex3f(0.0,0.0,-la->dist);
			glEnd();
			setlinestyle(0);
		}
		else if(la->mode & LA_SPHERE) {

			float tmat[4][4], imat[4][4];
			
			vec[0]= vec[1]= vec[2]= 0.0;
			mygetmatrix(tmat);
			Mat4Invert(imat, tmat);
			
			drawcircball(GL_LINE_LOOP, vec, la->dist, imat);

		}
	}

	glPushMatrix();
	glLoadMatrixf(G.vd->viewmat);
	
	VECCOPY(vec, ob->obmat[3]);
	
	setlinestyle(3);
	glBegin(GL_LINE_STRIP);
		glVertex3fv(vec); 
		vec[2]= 0; 
		glVertex3fv(vec);
	glEnd();
	setlinestyle(0);
	
	if(la->type==LA_SPOT && (la->mode & LA_SHAD) ) {
		tekenshadbuflimits(la, ob->obmat);
	}
	glPopMatrix();
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
static void draw_focus_cross(float dist, float size)
{
	glBegin(GL_LINES);
	glVertex3f(-size, 0.f, -dist);
	glVertex3f(size, 0.f, -dist);
	glVertex3f(0.f, -size, -dist);
	glVertex3f(0.f, size, -dist);
	glEnd();
}

void drawcamera(Object *ob)
{
	/* a standing up pyramid with (0,0,0) as top */
	Camera *cam;
	World *wrld;
	float vec[8][4], tmat[4][4], fac, facx, facy, depth;

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

	glBegin(GL_QUADS);

		vec[0][0]= -0.2*cam->drawsize; 
		vec[0][1]= cam->drawsize;
		glVertex3fv(vec[0]);
		
		vec[0][0]= 0.2*cam->drawsize;
		glVertex3fv(vec[0]);
		
		vec[0][1]= 1.6*cam->drawsize;
		glVertex3fv(vec[0]);
		
		vec[0][0]= -0.2*cam->drawsize; 
		glVertex3fv(vec[0]);
	glEnd();

	glBegin(GL_TRIANGLES);
	
		vec[0][0]= -0.4*cam->drawsize;
		vec[0][1]= 1.6*cam->drawsize;
		glVertex3fv(vec[0]);
		
		vec[0][0]= 0.0; 
		vec[0][1]= 2.0*cam->drawsize;
		glVertex3fv(vec[0]);
		
		vec[0][0]= 0.4*cam->drawsize; 
		vec[0][1]= 1.6*cam->drawsize;
		glVertex3fv(vec[0]);
	
	glEnd();
	
	if(cam->flag & (CAM_SHOWLIMITS+CAM_SHOWMIST)) {
		myloadmatrix(G.vd->viewmat);
		Mat4CpyMat4(vec, ob->obmat);
		Mat4Ortho(vec);
		mymultmatrix(vec);

		MTC_Mat4SwapMat4(G.vd->persmat, tmat);
		mygetsingmatrix(G.vd->persmat);

		if(cam->flag & CAM_SHOWLIMITS) {
			draw_limit_line(cam->clipsta, cam->clipend, 0x77FFFF);
			/* yafray: dof focus point */
			if (G.scene->r.renderer==R_YAFRAY) draw_focus_cross(cam->YF_dofdist, cam->drawsize);
		}

		wrld= G.scene->world;
		if(cam->flag & CAM_SHOWMIST) 
			if(wrld) draw_limit_line(wrld->miststa, wrld->miststa+wrld->mistdist, 0xFFFFFF);
			
		MTC_Mat4SwapMat4(G.vd->persmat, tmat);
	}
}

static void tekenvertslatt(short sel)
{
	Lattice *lt;
	BPoint *bp;
	float size;
	int a, uxt, u, vxt, v, wxt, w;

	size= BIF_GetThemeValuef(TH_VERTEX_SIZE);
	glPointSize(size);

	if(sel) BIF_ThemeColor(TH_VERTEX_SELECT);
	else BIF_ThemeColor(TH_VERTEX);

	bglBegin(GL_POINTS);

	bp= editLatt->def;
	lt= editLatt;
	
	if(lt->flag & LT_OUTSIDE) {
		
		for(w=0; w<lt->pntsw; w++) {
			if(w==0 || w==lt->pntsw-1) wxt= 1; else wxt= 0;
			for(v=0; v<lt->pntsv; v++) {
				if(v==0 || v==lt->pntsv-1) vxt= 1; else vxt= 0;
				
				for(u=0; u<lt->pntsu; u++, bp++) {
					if(u==0 || u==lt->pntsu-1) uxt= 1; else uxt= 0;
					if(uxt || vxt || wxt) {
						if(bp->hide==0) {
							if((bp->f1 & 1)==sel) bglVertex3fv(bp->vec);
						}
					}
				}
			}
		}
	}
	else {

		a= editLatt->pntsu*editLatt->pntsv*editLatt->pntsw;
		while(a--) {
			if(bp->hide==0) {
				if((bp->f1 & 1)==sel) bglVertex3fv(bp->vec);
			}
			bp++;
		}
	}
	
	glPointSize(1.0);
	bglEnd();	
}

static void calc_lattverts(void)
{
	BPoint *bp;
	float mat[4][4];
	int a;

	MTC_Mat4SwapMat4(G.vd->persmat, mat);
	mygetsingmatrix(G.vd->persmat);
	
	 bp= editLatt->def;
	
	a= editLatt->pntsu*editLatt->pntsv*editLatt->pntsw;
	while(a--) {
		project_short(bp->vec, bp->s);
		bp++;
	}

	MTC_Mat4SwapMat4(G.vd->persmat, mat);
}


void calc_lattverts_ext(void)
{

	areawinset(curarea->win);
	persp(PERSP_VIEW);
	mymultmatrix(G.obedit->obmat);
	calc_lattverts();
	myloadmatrix(G.vd->viewmat);
	
}


static void drawlattice(Object *ob)
{
	Lattice *lt;
	BPoint *bp, *bpu;
	int u, v, w, dv, dw, uxt, vxt, wxt;

	lt= ob->data;
	if(ob==G.obedit) {
		bp= editLatt->def;
		
		cpack(0x004000);
	}
	else {
		lattice_modifier(ob, 's');
		bp= lt->def;
	}
	
	dv= lt->pntsu;
	dw= dv*lt->pntsv;
	
	if(lt->flag & LT_OUTSIDE) {
		
		for(w=0; w<lt->pntsw; w++) {
			
			if(w==0 || w==lt->pntsw-1) wxt= 1; else wxt= 0;
			
			for(v=0; v<lt->pntsv; v++) {
				
				if(v==0 || v==lt->pntsv-1) vxt= 1; else vxt= 0;
				
				for(u=0, bpu=0; u<lt->pntsu; u++, bp++) {
				
					if(u==0 || u==lt->pntsu-1) uxt= 1; else uxt= 0;
					
					if(uxt || vxt || wxt) {
					
						if(w && (uxt || vxt)) {

							glBegin(GL_LINE_STRIP);
							glVertex3fv( (bp-dw)->vec ); glVertex3fv(bp->vec);
							glEnd();
						}
						if(v && (uxt || wxt)) {

							glBegin(GL_LINES);
							glVertex3fv( (bp-dv)->vec ); glVertex3fv(bp->vec);
							glEnd();
						}
						if(u && (vxt || wxt)) {

							glBegin(GL_LINES);
							glVertex3fv(bpu->vec); glVertex3fv(bp->vec);
							glEnd();
						}
					}
					
					bpu= bp;
				}
			}
		}		
	}
	else {
		for(w=0; w<lt->pntsw; w++) {
			
			for(v=0; v<lt->pntsv; v++) {
				
				for(u=0, bpu=0; u<lt->pntsu; u++, bp++) {
				
					if(w) {

						glBegin(GL_LINES);
						glVertex3fv( (bp-dw)->vec ); glVertex3fv(bp->vec);
						glEnd();
					}
					if(v) {

						glBegin(GL_LINES);
						glVertex3fv( (bp-dv)->vec ); glVertex3fv(bp->vec);
						glEnd();
					}
					if(u) {

						glBegin(GL_LINES);
						glVertex3fv(bpu->vec); glVertex3fv(bp->vec);
						glEnd();
					}
					bpu= bp;
				}
			}
		}
	}
	
	if(ob==G.obedit) {
		
		calc_lattverts();
		
		if(G.vd->zbuf) glDisable(GL_DEPTH_TEST);
		
		tekenvertslatt(0);
		tekenvertslatt(1);
		
		if(G.vd->zbuf) glEnable(GL_DEPTH_TEST); 
	}
	else lattice_modifier(ob, 'e');

}

/* ***************** ******************** */

void calc_mesh_facedots_ext(void)
{
	EditMesh *em = G.editMesh;
	EditFace *efa;
	float mat[4][4];

	if(em->faces.first==NULL) return;
	efa= em->faces.first;

	areawinset(curarea->win);
	persp(PERSP_VIEW);
	
	mymultmatrix(G.obedit->obmat);

	MTC_Mat4SwapMat4(G.vd->persmat, mat);
	mygetsingmatrix(G.vd->persmat);

	efa= em->faces.first;
	while(efa) {
		if( efa->h==0) {
			project_short(efa->cent, &(efa->xs));
		}
		efa= efa->next;
	}
	MTC_Mat4SwapMat4(G.vd->persmat, mat);

	myloadmatrix(G.vd->viewmat);
}

/* window coord, assuming all matrices are set OK */
static void calc_meshverts(DerivedMesh *dm)
{
	float co[3], mat[4][4];
	EditVert *eve;

	MTC_Mat4SwapMat4(G.vd->persmat, mat);
	mygetsingmatrix(G.vd->persmat);

	for(eve= G.editMesh->verts.first; eve; eve= eve->next) {
		if(eve->h==0) {
			dm->getMappedVertCoEM(dm, eve, co);
			project_short(co, &(eve->xs));
		}
	}

	MTC_Mat4SwapMat4(G.vd->persmat, mat);
}

/* window coord for current window, sets matrices temporal */
void calc_meshverts_ext(void)
{
	int dmNeedsFree;
	DerivedMesh *dm = mesh_get_cage_derived(G.obedit, &dmNeedsFree);

	areawinset(curarea->win);
	persp(PERSP_VIEW);
	
	mymultmatrix(G.obedit->obmat);
	calc_meshverts(dm);
	myloadmatrix(G.vd->viewmat);

	if (dmNeedsFree) {
		dm->release(dm);
	}
}

/* window coord for current window, sets matrices temporal, sets (eve->f & 2) when not visible */
void calc_meshverts_ext_f2(void)
{
	int dmNeedsFree;
	DerivedMesh *dm = mesh_get_cage_derived(G.obedit, &dmNeedsFree);
	float co[3], mat[4][4];
	EditVert *eve;

	/* matrices */
	areawinset(curarea->win);
	persp(PERSP_VIEW);
	mymultmatrix(G.obedit->obmat);
	
	MTC_Mat4SwapMat4(G.vd->persmat, mat);
	mygetsingmatrix(G.vd->persmat);

	for(eve= G.editMesh->verts.first; eve; eve= eve->next) {
		eve->f &= ~2;
		if(eve->h==0) {
			dm->getMappedVertCoEM(dm, eve, co);
			project_short_noclip(co, &(eve->xs));
	
			if( eve->xs >= 0 && eve->ys >= 0 && eve->xs<curarea->winx && eve->ys<curarea->winy);
			else eve->f |= 2;
		}
	}
	
	/* restore */
	MTC_Mat4SwapMat4(G.vd->persmat, mat);
	myloadmatrix(G.vd->viewmat);

	if (dmNeedsFree) {
		dm->release(dm);
	}
}


static void calc_Nurbverts(Nurb *nurb)
{
	Nurb *nu;
	BezTriple *bezt;
	BPoint *bp;
	float mat[4][4];
	int a;

	MTC_Mat4SwapMat4(G.vd->persmat, mat);
	mygetsingmatrix(G.vd->persmat);

	nu= nurb;
	while(nu) {
		if((nu->type & 7)==1) {
			bezt= nu->bezt;
			a= nu->pntsu;
			while(a--) {
				project_short(bezt->vec[0], bezt->s[0]);
				project_short(bezt->vec[1], bezt->s[1]);
				project_short(bezt->vec[2], bezt->s[2]);
				bezt++;
			}
		}
		else {
			bp= nu->bp;
			a= nu->pntsu*nu->pntsv;
			while(a--) {
				project_short(bp->vec, bp->s);
				bp++;
			}
		}
		nu= nu->next;
	}

	MTC_Mat4SwapMat4(G.vd->persmat, mat);
}

void calc_nurbverts_ext(void)
{

	areawinset(curarea->win);
	persp(PERSP_VIEW);
	mymultmatrix(G.obedit->obmat);
	calc_Nurbverts(editNurb.first);
	myloadmatrix(G.vd->viewmat);
	
}

////

static void calc_weightpaint_vert_color(Object *ob, int vert, unsigned char *col)
{
	Mesh *me = ob->data;
	float fr, fg, fb, input = 0.0;
	int i;

	if (me->dvert) {
		for (i=0; i<me->dvert[vert].totweight; i++)
			if (me->dvert[vert].dw[i].def_nr==ob->actdef-1)
				input+=me->dvert[vert].dw[i].weight;		
	}

	CLAMP(input, 0.0, 1.0);

	fr = fg = fb = 85;
	if (input<=0.25f){
		fr=0.0f;
		fg=255.0f * (input*4.0f);
		fb=255.0f;
	}
	else if (input<=0.50f){
		fr=0.0f;
		fg=255.0f;
		fb=255.0f * (1.0f-((input-0.25f)*4.0f)); 
	}
	else if (input<=0.75){
		fr=255.0f * ((input-0.50f)*4.0f);
		fg=255.0f;
		fb=0.0f;
	}
	else if (input<=1.0){
		fr=255.0f;
		fg=255.0f * (1.0f-((input-0.75f)*4.0f)); 
		fb=0.0f;
	}

	col[3] = (unsigned char)(fr * ((input/2.0f)+0.5f));
	col[2] = (unsigned char)(fg * ((input/2.0f)+0.5f));
	col[1] = (unsigned char)(fb * ((input/2.0f)+0.5f));
	col[0] = 255;
}
static unsigned char *calc_weightpaint_colors(Object *ob) 
{
	Mesh *me = ob->data;
	MFace *mf = me->mface;
	unsigned char *wtcol;
	int i;
	
	wtcol = MEM_callocN (sizeof (unsigned char) * me->totface*4*4, "weightmap");
	
	memset(wtcol, 0x55, sizeof (unsigned char) * me->totface*4*4);
	for (i=0; i<me->totface; i++, mf++){
		calc_weightpaint_vert_color(ob, mf->v1, &wtcol[(i*4 + 0)*4]); 
		calc_weightpaint_vert_color(ob, mf->v2, &wtcol[(i*4 + 1)*4]); 
		if (mf->v3)
			calc_weightpaint_vert_color(ob, mf->v3, &wtcol[(i*4 + 2)*4]); 
		if (mf->v4)
			calc_weightpaint_vert_color(ob, mf->v4, &wtcol[(i*4 + 3)*4]); 
	}
	
	return wtcol;
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

static void draw_em_face_normals(EditMesh *em, float normalLength)
{
	EditFace *efa;

	glBegin(GL_LINES);
	for(efa= em->faces.first; efa; efa= efa->next) {
		if(efa->h==0 && efa->fgonf!=EM_FGON) {
			glVertex3fv(efa->cent);
			glVertex3f(	efa->cent[0] + normalLength*efa->n[0],
						efa->cent[1] + normalLength*efa->n[1],
						efa->cent[2] + normalLength*efa->n[2]);
			
		}
	}
	glEnd();
}

static void draw_em_face_centers(EditMesh *em, int sel) {
	EditFace *efa;

	bglBegin(GL_POINTS);
	for(efa= em->faces.first; efa; efa= efa->next) {
		if(efa->h==0 && efa->fgonf!=EM_FGON && (efa->f&SELECT)==sel) {
			bglVertex3fv(efa->cent);
		}
	}
	bglEnd();
}


	/* Draw verts with color set based on selection */
static int draw_dm_verts__setDrawOptions(void *userData, EditVert *eve)
{
	int sel = *((int*) userData);

	return (eve->h==0 && (eve->f&SELECT)==sel);
}
static void draw_dm_verts(DerivedMesh *dm, int sel)
{
	dm->drawMappedVertsEM(dm, draw_dm_verts__setDrawOptions, &sel);
}

	/* Draw edges with color set based on selection */
static int draw_dm_edges_sel__setDrawOptions(void *userData, EditEdge *eed)
{
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
	dm->drawMappedEdgesEM(dm, draw_dm_edges_sel__setDrawOptions, cols);
}

	/* Draw edges */
static int draw_dm_edges__setDrawOptions(void *userData, EditEdge *eed)
{
	return eed->h==0;
}
static void draw_dm_edges(DerivedMesh *dm) 
{
	dm->drawMappedEdgesEM(dm, draw_dm_edges__setDrawOptions, NULL);
}

	/* Draw edges with color interpolated based on selection */
static int draw_dm_edges_sel_interp__setDrawOptions(void *userData, EditEdge *eed)
{
	return (eed->h==0);
}
static void draw_dm_edges_sel_interp__setDrawInterpOptions(void *userData, EditEdge *eed, float t)
{
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
	dm->drawMappedEdgesInterpEM(dm, draw_dm_edges_sel_interp__setDrawOptions, draw_dm_edges_sel_interp__setDrawInterpOptions, cols);
}

	/* Draw only seam edges */
static int draw_dm_edges_seams__setDrawOptions(void *userData, EditEdge *eed)
{
	return (eed->h==0 && eed->seam);
}
static void draw_dm_edges_seams(DerivedMesh *dm)
{
	dm->drawMappedEdgesEM(dm, draw_dm_edges_seams__setDrawOptions, NULL);
}

	/* Draw faces with color set based on selection */
static int draw_dm_faces_sel__setDrawOptions(void *userData, EditFace *efa)
{
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
	dm->drawMappedFacesEM(dm, draw_dm_faces_sel__setDrawOptions, cols);
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
						
					glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
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
				glColor4ubv(col);
				draw_dm_verts(cageDM, sel);
			}
			
			if(G.scene->selectmode & SCE_SELECT_FACE) {
				glPointSize(fsize);
				glColor4ubv(fcol);
				draw_em_face_centers(em, sel);
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
	char wire[4], sel[4];

	/* since this function does transparant... */
	BIF_GetThemeColor3ubv(TH_EDGE_SELECT, sel);
	BIF_GetThemeColor3ubv(TH_WIRE, wire);

	for (pass=0; pass<2; pass++) {
			/* show wires in transparant when no zbuf clipping for select */
		if (pass==0) {
			if (G.vd->zbuf && (G.vd->flag & V3D_ZBUF_SELECT)==0) {
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
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
			if(cageDM->drawMappedEdgesInterpEM && (G.scene->selectmode & SCE_SELECT_VERTEX)) {
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

static void draw_em_creases(EditMesh *em)
{
	EditEdge *eed;
	float fac, *v1, *v2, vec[3];
	
	glLineWidth(3.0);
	glBegin(GL_LINES);
	for(eed= em->edges.first; eed; eed= eed->next) {
		if(eed->h==0 && eed->crease!=0.0) {
			BIF_ThemeColor((eed->f&SELECT)?TH_EDGE_SELECT:TH_WIRE);
			
			v1= eed->v1->co; v2= eed->v2->co;
			VECSUB(vec, v2, v1);
			fac= 0.5 + eed->crease/2.0;
			glVertex3f(v1[0] + fac*vec[0], v1[1] + fac*vec[1], v1[2] + fac*vec[2] );
			glVertex3f(v2[0] - fac*vec[0], v2[1] - fac*vec[1], v2[2] - fac*vec[2] );
		}
	}
	glEnd();
	glLineWidth(1.0);
}

static void draw_em_measure_stats(Object *ob, EditMesh *em)
{
	EditEdge *eed;
	EditFace *efa;
	float v1[3], v2[3], v3[3], v4[3];
	float fvec[3];
	char val[32]; /* Stores the measurement display text here */
	float area, col[3]; /* area of the face,  colour of the text to draw */
	
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
			if((eed->f & SELECT) || (G.moving && ((eed->v1->f & SELECT) || (eed->v2->f & SELECT)) )) {
				VECCOPY(v1, eed->v1->co);
				VECCOPY(v2, eed->v2->co);
				
				glRasterPos3f( 0.5*(v1[0]+v2[0]),  0.5*(v1[1]+v2[1]),  0.5*(v1[2]+v2[2]));
				
				if(G.vd->flag & V3D_GLOBAL_STATS) {
					Mat4MulVecfl(ob->obmat, v1);
					Mat4MulVecfl(ob->obmat, v2);
				}
				
				sprintf(val,"%.3f", VecLenf(v1, v2));
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

				sprintf(val,"%.3f", area);
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
				fvec[0]= 0.2*efa->cent[0] + 0.8*efa->v1->co[0];
				fvec[1]= 0.2*efa->cent[1] + 0.8*efa->v1->co[1];
				fvec[2]= 0.2*efa->cent[2] + 0.8*efa->v1->co[2];
				glRasterPos3fv(fvec);
				BMF_DrawString( G.fonts, val);
			}
			if( (e1->f & e2->f & SELECT) || (G.moving && (efa->v2->f & SELECT)) ) {
				/* Vec 2 */
				sprintf(val,"%.3f", VecAngle3(v1, v2, v3));
				fvec[0]= 0.2*efa->cent[0] + 0.8*efa->v2->co[0];
				fvec[1]= 0.2*efa->cent[1] + 0.8*efa->v2->co[1];
				fvec[2]= 0.2*efa->cent[2] + 0.8*efa->v3->co[2];
				glRasterPos3fv(fvec);
				BMF_DrawString( G.fonts, val);
			}
			if( (e2->f & e3->f & SELECT) || (G.moving && (efa->v3->f & SELECT)) ) {
				/* Vec 3 */
				if(efa->v4) 
					sprintf(val,"%.3f", VecAngle3(v2, v3, v4));
				else
					sprintf(val,"%.3f", VecAngle3(v2, v3, v1));
				fvec[0]= 0.2*efa->cent[0] + 0.8*efa->v3->co[0];
				fvec[1]= 0.2*efa->cent[1] + 0.8*efa->v3->co[1];
				fvec[2]= 0.2*efa->cent[2] + 0.8*efa->v3->co[2];
				glRasterPos3fv(fvec);
				BMF_DrawString( G.fonts, val);
			}
				/* Vec 4 */
			if(efa->v4) {
				if( (e3->f & e4->f & SELECT) || (G.moving && (efa->v4->f & SELECT)) ) {
					sprintf(val,"%.3f", VecAngle3(v3, v4, v1));

					fvec[0]= 0.2*efa->cent[0] + 0.8*efa->v4->co[0];
					fvec[1]= 0.2*efa->cent[1] + 0.8*efa->v4->co[1];
					fvec[2]= 0.2*efa->cent[2] + 0.8*efa->v4->co[2];
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

static void draw_em_fancy(Object *ob, EditMesh *em, DerivedMesh *baseDM, DerivedMesh *realDM, int dt)
{
	Mesh *me = ob->data;
	DerivedMesh *cageDM;

	if(realDM && (me->flag&ME_OPT_EDGES)) {
		cageDM = realDM;
	} else {
		cageDM = baseDM;
	}

	if(dt>OB_WIRE) {
		init_gl_materials(ob);
		glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, me->flag & ME_TWOSIDED);

		glEnable(GL_LIGHTING);
		glFrontFace((ob->transflag&OB_NEG_SCALE)?GL_CW:GL_CCW);

		if (realDM) {
			realDM->drawFacesSolid(realDM, set_gl_material);
		} else {
			baseDM->drawFacesSolid(baseDM, set_gl_material);
		}

		glFrontFace(GL_CCW);
		glDisable(GL_LIGHTING);

			// Setup for drawing wire over, disable zbuffer
			// write to show selected edge wires better
		BIF_ThemeColor(TH_WIRE);

		bglPolygonOffset(1.0);
		glDepthMask(0);
	} else {
		if (realDM && !(me->flag&ME_OPT_EDGES)) {
			BIF_ThemeColorBlend(TH_WIRE, TH_BACK, 0.7);
			realDM->drawEdges(realDM);
		}
	}

	if( (G.f & (G_FACESELECT+G_DRAWFACES))) {	/* transp faces */
		char col1[4], col2[4];
			
		BIF_GetThemeColor4ubv(TH_FACE, col1);
		BIF_GetThemeColor4ubv(TH_FACE_SELECT, col2);
		
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
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

	draw_em_fancy_edges(cageDM);

	if(G.f & G_DRAWCREASES) {
		draw_em_creases(em);
	}

	if(ob==G.obedit) {
		calc_meshverts(cageDM);
		draw_em_fancy_verts(em, cageDM);

		if(G.f & G_DRAWNORMALS) {
			BIF_ThemeColor(TH_NORMAL);
			draw_em_face_normals(em, G.scene->editbutsize);
		}

		if(G.f & (G_DRAW_EDGELEN|G_DRAW_FACEAREA|G_DRAW_EDGEANG))
			draw_em_measure_stats(ob, em);
	}

	if(dt>OB_WIRE) {
		glDepthMask(1);
		bglPolygonOffset(0.0);
	}
}

/* Mesh drawing routines */

static void draw_mesh_object_outline(DerivedMesh *dm)
{
	glLineWidth(2.0);
	glDepthMask(0);
				
	dm->drawEdges(dm);
				
	glLineWidth(1.0);
	glDepthMask(1);
}

static void draw_mesh_fancy(Object *ob, DerivedMesh *baseDM, DerivedMesh *realDM, int dt)
{
	Mesh *me = ob->data;
	Material *ma= give_current_material(ob, 1);
	int hasHaloMat = (ma && (ma->mode&MA_HALO));
	int draw_wire = ob->dtx&OB_DRAWWIRE;
	DispList *dl;
	float *obExtVerts;
	DerivedMesh *dm = realDM?realDM:baseDM;

	glFrontFace((ob->transflag&OB_NEG_SCALE)?GL_CW:GL_CCW);

	dl = find_displist(&ob->disp, DL_VERTS);
	obExtVerts = dl?dl->verts:NULL;

		// Unwanted combination.
	if (G.f&G_FACESELECT) draw_wire = 0;

		// This is only for objects from the decimator and
		// is a temporal solution, a reconstruction of the
		// displist system should take care of it (zr/ton)
	if(me->decimated) {
		DispListMesh *dlm = me->decimated;
		MVert *mvert= dlm->mvert;
		MFace *mface= dlm->mface;
		int i;

		for (i=0; i<dlm->totface; i++, mface++) {
			glBegin(GL_LINE_LOOP);
			glVertex3fv(mvert[mface->v1].co);
			glVertex3fv(mvert[mface->v2].co);
			if (mface->v3) {
				glVertex3fv(mvert[mface->v3].co);
				if (mface->v4)
					glVertex3fv(mvert[mface->v4].co);
			}
			glEnd();
		}
	}
	else if(dt==OB_BOUNDBOX) {
		draw_bounding_volume(ob);
	}
	else if(hasHaloMat || (me->totface==0 && (!me->medge || me->totedge==0))) {
		glPointSize(1.5);
		dm->drawVerts(dm);
		glPointSize(1.0);
	}
	else if(dt==OB_WIRE || me->totface==0) {
		draw_wire = 1;
	}
	else if( (ob==OBACT && (G.f & G_FACESELECT)) || (G.vd->drawtype==OB_TEXTURE && dt>OB_SOLID)) {
		if ((G.vd->flag&V3D_SELECT_OUTLINE) && (ob->flag&SELECT) && !(G.f&G_FACESELECT) && !draw_wire) {
			draw_mesh_object_outline(dm);
		}

		draw_tface_mesh(ob, ob->data, dt);
	}
	else if(dt==OB_SOLID ) {
		if ((G.vd->flag&V3D_SELECT_OUTLINE) && (ob->flag&SELECT) && !draw_wire) {
			draw_mesh_object_outline(dm);
		}

		init_gl_materials(ob);
		glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, me->flag & ME_TWOSIDED );

		glEnable(GL_LIGHTING);
		glFrontFace((ob->transflag&OB_NEG_SCALE)?GL_CW:GL_CCW);
		
		dm->drawFacesSolid(dm, set_gl_material);

		glFrontFace(GL_CCW);
		glDisable(GL_LIGHTING);

		BIF_ThemeColor(TH_WIRE);
		dm->drawLooseEdges(dm);
	}
	else if(dt==OB_SHADED) {
		if( (G.f & G_WEIGHTPAINT)) {
			unsigned char *wtcol = calc_weightpaint_colors(ob);
			baseDM->drawFacesColored(baseDM, me->flag&ME_TWOSIDED, wtcol, 0);
			MEM_freeN (wtcol);
		}
		else if((G.f & (G_VERTEXPAINT+G_TEXTUREPAINT)) && me->mcol) {
			baseDM->drawFacesColored(baseDM, me->flag&ME_TWOSIDED, (unsigned char*) me->mcol, 0);
		}
		else if((G.f & (G_VERTEXPAINT+G_TEXTUREPAINT)) && me->tface) {
			tface_to_mcol(me);
			baseDM->drawFacesColored(baseDM, me->flag&ME_TWOSIDED, (unsigned char*) me->mcol, 0);
			MEM_freeN(me->mcol); 
			me->mcol= 0;
		}
		else {
			unsigned int *obCol1, *obCol2;

			dl = ob->disp.first;
			if (!dl || !dl->col1) {
				shadeDispList(ob);
				dl = find_displist(&ob->disp, DL_VERTCOL);
			}
			obCol1 = dl->col1;
			obCol2 = dl->col2;

			if ((G.vd->flag&V3D_SELECT_OUTLINE) && (ob->flag&SELECT) && !draw_wire) {
				draw_mesh_object_outline(dm);
			}

			dm->drawFacesColored(dm, me->flag&ME_TWOSIDED, (unsigned char*) obCol1, (unsigned char*) obCol2);
		}
	}

	if (draw_wire) {
			/* If drawing wire and drawtype is not OB_WIRE then we are
				* overlaying the wires.
				*/
		if (dt!=OB_WIRE) {
			if(ob->flag & SELECT) {
				BIF_ThemeColor((ob==OBACT)?TH_ACTIVE:TH_SELECT);
			} else {
				BIF_ThemeColor(TH_WIRE);
			}

			bglPolygonOffset(1.0);
			glDepthMask(0);	// disable write in zbuffer, selected edge wires show better
		}

		if (me->flag&ME_OPT_EDGES) {
			dm->drawMappedEdges(dm);
		}
		else {
			dm->drawEdges(dm);
		}

		if (dt!=OB_WIRE) {
			glDepthMask(1);
			bglPolygonOffset(0.0);
		}
	}
}

static void draw_mesh_object(Object *ob, int dt)
{
	Mesh *me= ob->data;
	DerivedMesh *baseDM = mesh_get_base_derived(ob);
	DerivedMesh *realDM = mesh_get_derived(ob);

	if(G.obedit && ob->data==G.obedit->data) {
		draw_em_fancy(ob, G.editMesh, baseDM, realDM, dt);
	}
	else {
		if(me->bb==NULL) tex_space_mesh(me);
		if(me->totface<=4 || boundbox_clip(ob->obmat, me->bb)) {
			draw_mesh_fancy(ob, baseDM, realDM, dt);
		}
	}

	baseDM->release(baseDM);
	
}

/* ************** DRAW DISPLIST ****************** */

static int draw_index_wire= 1;
static int index3_nors_incr= 1;

static void drawDispListwire(ListBase *dlbase)
{
	DispList *dl;
	int parts, nr, ofs, *index;
	float *data;

	if(dlbase==0) return;

	dl= dlbase->first;
	while(dl) {
		data= dl->verts;
	
		switch(dl->type) {
		case DL_SEGM:
			parts= dl->parts;
			while(parts--) {
				nr= dl->nr;
				glBegin(GL_LINE_STRIP);
				while(nr--) {
					glVertex3fv(data);
					data+=3;
				}
				glEnd();
			}
			break;
		case DL_POLY:
			parts= dl->parts;
			while(parts--) {
				nr= dl->nr;
				glBegin(GL_LINE_LOOP);
				while(nr--) {
					glVertex3fv(data);
					data+=3;
				}
				glEnd();
			}
			break;
		case DL_SURF:
			parts= dl->parts;
			while(parts--) {
				nr= dl->nr;
				if(dl->flag & DL_CYCL_U) glBegin(GL_LINE_LOOP);
				else glBegin(GL_LINE_STRIP);

				while(nr--) {
					glVertex3fv(data);
					data+=3;
				}
				glEnd();
			}
			ofs= 3*dl->nr;
			nr= dl->nr;
			while(nr--) {
				data= (  dl->verts )+3*nr;
				parts= dl->parts;
				if(dl->flag & DL_CYCL_V) glBegin(GL_LINE_LOOP);
				else glBegin(GL_LINE_STRIP);
				
				while(parts--) {
					glVertex3fv(data);
					data+=ofs;
				}
				glEnd();
			}
			break;
			
		case DL_INDEX3:
			if(draw_index_wire) {
				parts= dl->parts;
				data= dl->verts;
				index= dl->index;
				while(parts--) {

					glBegin(GL_LINE_LOOP);
						glVertex3fv(data+3*index[0]);
						glVertex3fv(data+3*index[1]);
						glVertex3fv(data+3*index[2]);
					glEnd();
					index+= 3;
				}
			}
			break;
			
		case DL_INDEX4:
			if(draw_index_wire) {
				parts= dl->parts;
				data= dl->verts;
				index= dl->index;
				while(parts--) {

					glBegin(GL_LINE_LOOP);
						glVertex3fv(data+3*index[0]);
						glVertex3fv(data+3*index[1]);
						glVertex3fv(data+3*index[2]);
						if(index[3]) glVertex3fv(data+3*index[3]);
					glEnd();
					index+= 4;
				}
			}
			break;
		}
		dl= dl->next;
	}
}

static void drawDispListsolid(ListBase *lb, Object *ob)
{
	DispList *dl;
	int parts, ofs, p1, p2, p3, p4, a, b, *index;
	float *data, *v1, *v2, *v3, *v4;
	float *ndata, *n1, *n2, *n3, *n4;
	
	if(lb==0) return;
	
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
		case DL_SURF:

			set_gl_material(dl->col+1);
			
			if(dl->rt & CU_SMOOTH) glShadeModel(GL_SMOOTH);
			else glShadeModel(GL_FLAT);

			for(a=0; a<dl->parts; a++) {
				
				DL_SURFINDEX(dl->flag & DL_CYCL_U, dl->flag & DL_CYCL_V, dl->nr, dl->parts);
				
				v1= data+ 3*p1; 
				v2= data+ 3*p2;
				v3= data+ 3*p3; 
				v4= data+ 3*p4;
				n1= ndata+ 3*p1; 
				n2= ndata+ 3*p2;
				n3= ndata+ 3*p3; 
				n4= ndata+ 3*p4;
				
				glBegin(GL_QUAD_STRIP);
				
				glNormal3fv(n2); glVertex3fv(v2);
				glNormal3fv(n4); glVertex3fv(v4);

				for(; b<dl->nr; b++) {
					
					glNormal3fv(n1); glVertex3fv(v1);
					glNormal3fv(n3); glVertex3fv(v3);

					v2= v1; v1+= 3;
					v4= v3; v3+= 3;
					n2= n1; n1+= 3;
					n4= n3; n3+= 3;
				}
				
				
				glEnd();
			}
			break;

		case DL_INDEX3:
		
			parts= dl->parts;
			data= dl->verts;
			ndata= dl->nors;
			index= dl->index;

			set_gl_material(dl->col+1);
							
			/* voor polys only one normal needed */
			if(index3_nors_incr==0) {
				while(parts--) {

					glBegin(GL_TRIANGLES);
						glNormal3fv(ndata);
						glVertex3fv(data+3*index[0]);
						glVertex3fv(data+3*index[1]);
						glVertex3fv(data+3*index[2]);
					glEnd();
					index+= 3;
				}
			}
			else {
				while(parts--) {

					glBegin(GL_TRIANGLES);
						ofs= 3*index[0];
						glNormal3fv(ndata+ofs); glVertex3fv(data+ofs);
						ofs= 3*index[1];
						glNormal3fv(ndata+ofs); glVertex3fv(data+ofs);
						ofs= 3*index[2];
						glNormal3fv(ndata+ofs); glVertex3fv(data+ofs);
					glEnd();
					index+= 3;
				}
			}
			break;

		case DL_INDEX4:

			parts= dl->parts;
			data= dl->verts;
			ndata= dl->nors;
			index= dl->index;

			set_gl_material(dl->col+1);
		
			while(parts--) {

				glBegin(index[3]?GL_QUADS:GL_TRIANGLES);
					ofs= 3*index[0];
					glNormal3fv(ndata+ofs); glVertex3fv(data+ofs);
					ofs= 3*index[1];
					glNormal3fv(ndata+ofs); glVertex3fv(data+ofs);
					ofs= 3*index[2];
					glNormal3fv(ndata+ofs); glVertex3fv(data+ofs);
					if(index[3]) {
						ofs= 3*index[3];
						glNormal3fv(ndata+ofs); glVertex3fv(data+ofs);
					}
				glEnd();
				index+= 4;
			}
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
	int parts, p1, p2, p3, p4, a, b, *index;
	float *data, *v1, *v2, *v3, *v4;/*  , *extverts=0 */
	unsigned int *cdata, *c1, *c2, *c3, *c4;
	char *cp;

	if(lb==0) return;

	glShadeModel(GL_SMOOTH);

	dl= lb->first;
	dlob= ob->disp.first;
	while(dl && dlob) {
		
		cdata= dlob->col1;
		data= dl->verts;
		if(cdata==0) break;
		
		switch(dl->type) {
		case DL_SURF:

			for(a=0; a<dl->parts; a++) {

				DL_SURFINDEX(dl->flag & DL_CYCL_U, dl->flag & DL_CYCL_V, dl->nr, dl->parts);

				v1= data+ 3*p1; 
				v2= data+ 3*p2;
				v3= data+ 3*p3; 
				v4= data+ 3*p4;
				c1= cdata+ p1; 
				c2= cdata+ p2;
				c3= cdata+ p3; 
				c4= cdata+ p4;

				for(; b<dl->nr; b++) {

					glBegin(GL_QUADS);
						cp= (char *)c1;
						glColor3ub(cp[3], cp[2], cp[1]);
						glVertex3fv(v1);
						cp= (char *)c2;
						glColor3ub(cp[3], cp[2], cp[1]);
						glVertex3fv(v2);
						cp= (char *)c4;
						glColor3ub(cp[3], cp[2], cp[1]);
						glVertex3fv(v4);
						cp= (char *)c3;
						glColor3ub(cp[3], cp[2], cp[1]);
						glVertex3fv(v3);
					glEnd();

					v2= v1; v1+= 3;
					v4= v3; v3+= 3;
					c2= c1; c1++;
					c4= c3; c3++;
				}
			}
			break;

		case DL_INDEX3:
			
			parts= dl->parts;
			index= dl->index;
			
			while(parts--) {

				glBegin(GL_TRIANGLES);
					cp= (char *)(cdata+index[0]);
					glColor3ub(cp[3], cp[2], cp[1]);					
					glVertex3fv(data+3*index[0]);

					cp= (char *)(cdata+index[1]);
					glColor3ub(cp[3], cp[2], cp[1]);					
					glVertex3fv(data+3*index[1]);

					cp= (char *)(cdata+index[2]);
					glColor3ub(cp[3], cp[2], cp[1]);					
					glVertex3fv(data+3*index[2]);
				glEnd();
				index+= 3;
			}
			break;

		case DL_INDEX4:
		
			parts= dl->parts;
			index= dl->index;
			while(parts--) {

				glBegin(index[3]?GL_QUADS:GL_TRIANGLES);
					cp= (char *)(cdata+index[0]);
					glColor3ub(cp[3], cp[2], cp[1]);					
					glVertex3fv(data+3*index[0]);

					cp= (char *)(cdata+index[1]);
					glColor3ub(cp[3], cp[2], cp[1]);					
					glVertex3fv(data+3*index[1]);

					cp= (char *)(cdata+index[2]);
					glColor3ub(cp[3], cp[2], cp[1]);					
					glVertex3fv(data+3*index[2]);
					
					if(index[3]) {
					
						cp= (char *)(cdata+index[3]);
						glColor3ub(cp[3], cp[2], cp[1]);	
						glVertex3fv(data+3*index[3]);
					}
				glEnd();
				index+= 4;
			}
			break;
			
		}
		dl= dl->next;
		dlob= dlob->next;
	}
	
	glShadeModel(GL_FLAT);
}

static void drawDispList(Object *ob, int dt)
{
	ListBase *lb=0;
	DispList *dl;
	Curve *cu;
	int solid;

	
	solid= (dt > OB_WIRE);

	switch(ob->type) {
	case OB_FONT:
	case OB_CURVE:
		cu= ob->data;
		
		lb= &cu->disp;
		if(lb->first==0) makeDispList(ob);
		
		if(solid) {
			dl= lb->first;
			if(dl==0) return;
			
			/* rule: dl->type INDEX3 is always first in list */
			if(dl->type!=DL_INDEX3) {
				if(ob==G.obedit) curve_to_filledpoly(ob->data, &editNurb, lb);
				else curve_to_filledpoly(ob->data, &cu->nurb, lb);
				
				dl= lb->first;
			}
			if(dl->nors==0) addnormalsDispList(ob, lb);
			
			index3_nors_incr= 0;
			
			if( displist_has_faces(lb)==0) {
				draw_index_wire= 0;
				drawDispListwire(lb);
				draw_index_wire= 1;
			}
			else {
				if(dt==OB_SHADED) {
					if(ob->disp.first==0) shadeDispList(ob);
					drawDispListshaded(lb, ob);
				}
				else {
					init_gl_materials(ob);
					glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, 0);
					drawDispListsolid(lb, ob);
				}
				if(ob==G.obedit) {
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
			drawDispListwire(lb);
			draw_index_wire= 1;
		}
		break;
	case OB_SURF:
	
		lb= &((Curve *)ob->data)->disp;
		if(lb->first==0) makeDispList(ob);
		
		if(solid) {
			dl= lb->first;
			if(dl==0) return;
			
			if(dl->nors==0) addnormalsDispList(ob, lb);
			
			if(dt==OB_SHADED) {
				if(ob->disp.first==0) shadeDispList(ob);
				drawDispListshaded(lb, ob);
			}
			else {
				init_gl_materials(ob);
				glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, 0);
			
				drawDispListsolid(lb, ob);
			}
		}
		else {
			drawDispListwire(lb);
		}
		break;
	case OB_MBALL:
		
		if( is_basis_mball(ob)) {
			lb= &ob->disp;
			if(lb->first==0) makeDispList(ob);
	
			if(solid) {
				
				if(dt==OB_SHADED) {
					dl= lb->first;
					if(dl && dl->col1==0) shadeDispList(ob);
					drawDispListshaded(lb, ob);
				}
				else {
					init_gl_materials(ob);
					glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, 0);
				
					drawDispListsolid(lb, ob);	
				}
			}
			else{
				/* MetaBalls use DL_INDEX4 type of DispList */
				drawDispListwire(lb);
			}
		}
		break;
	}
	
}

/* ******************************** */


static void draw_particle_system(Object *ob, PartEff *paf)
{
	Particle *pa;
	float ptime, ctime, vec[3], vec1[3];
	int a;
	
	pa= paf->keys;
	if(pa==0) {
		build_particle_system(ob);
		pa= paf->keys;
		if(pa==0) return;
	}
	
	myloadmatrix(G.vd->viewmat);
	
	if(ob->ipoflag & OB_OFFS_PARTICLE) ptime= ob->sf;
	else ptime= 0.0;
	ctime= bsystem_time(ob, 0, (float)(G.scene->r.cfra), ptime);

	glPointSize(1.0);
	if(paf->stype!=PAF_VECT) glBegin(GL_POINTS);

	for(a=0; a<paf->totpart; a++, pa+=paf->totkey) {
		
		if(ctime > pa->time) {
			if(ctime < pa->time+pa->lifetime) {
			
				if(paf->stype==PAF_VECT) {
					where_is_particle(paf, pa, ctime, vec);
					where_is_particle(paf, pa, ctime+1.0, vec1);
		

					glBegin(GL_LINE_STRIP);
						glVertex3fv(vec);
						glVertex3fv(vec1);
					glEnd();
					
				}
				else {
					where_is_particle(paf, pa, ctime, vec);
					
					glVertex3fv(vec);
						
				}
			}
		}
	}
	if(paf->stype!=PAF_VECT) glEnd();
	
	mymultmatrix(ob->obmat);	// bring back local matrix for dtx
}

static void draw_static_particle_system(Object *ob, PartEff *paf)
{
	Particle *pa;
	float ctime, mtime, vec[3], vec1[3];
	int a;
	
	pa= paf->keys;
	if(pa==0) {
		build_particle_system(ob);
		pa= paf->keys;
		if(pa==0) return;
	}
	
	glPointSize(1.0);
	if(paf->stype!=PAF_VECT) glBegin(GL_POINTS);

	for(a=0; a<paf->totpart; a++, pa+=paf->totkey) {
		
		where_is_particle(paf, pa, pa->time, vec1);
		
		mtime= pa->time+pa->lifetime+paf->staticstep-1;
		
		for(ctime= pa->time; ctime<mtime; ctime+=paf->staticstep) {
			
			/* make sure hair grows until the end.. */ 
			if(ctime>pa->time+pa->lifetime) ctime= pa->time+pa->lifetime;
			
			if(paf->stype==PAF_VECT) {
				where_is_particle(paf, pa, ctime+1, vec);

				glBegin(GL_LINE_STRIP);
					glVertex3fv(vec);
					glVertex3fv(vec1);
				glEnd();
				
				VECCOPY(vec1, vec);
			}
			else {
				where_is_particle(paf, pa, ctime, vec);
				
				glVertex3fv(vec);
					
			}
		}
	}
	if(paf->stype!=PAF_VECT) glEnd();

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

					glBegin(GL_LINE_STRIP); 
					glVertex3fv(fp);
					glVertex3fv(fp+3); 
					glEnd();
					cpack(col[bezt->h2]);

					glBegin(GL_LINE_STRIP); 
					glVertex3fv(fp+3); 
					glVertex3fv(fp+6); 
					glEnd();
				}
				else if( (bezt->f1 & 1)==sel) {
					fp= bezt->vec[0];
					cpack(col[bezt->h1]);

					glBegin(GL_LINE_STRIP); 
					glVertex3fv(fp); 
					glVertex3fv(fp+3); 
					glEnd();
				}
				else if( (bezt->f3 & 1)==sel) {
					fp= bezt->vec[1];
					cpack(col[bezt->h2]);

					glBegin(GL_LINE_STRIP); 
					glVertex3fv(fp); 
					glVertex3fv(fp+3); 
					glEnd();
				}
			}
			bezt++;
		}
	}
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

					if(nu->flagu & 1) glEnd();
					else glEnd();
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

static void drawnurb(Object *ob, Nurb *nurb, int dt)
{
	Curve *cu;
	Nurb *nu;
	BevPoint *bevp;
	BevList *bl;
	float vec[3];
	int a, nr, skip;

	/* first non-selected handles */
	nu= nurb;
	while(nu) {
		if((nu->type & 7)==CU_BEZIER) {
			tekenhandlesN(nu, 0);
		}
		nu= nu->next;
	}
	
	/* then DispList */
	
	BIF_ThemeColor(TH_WIRE);
	cu= ob->data;
	drawDispList(ob, dt);

	draw_editnurb(ob, nurb, 0);
	draw_editnurb(ob, nurb, 1);

	if(cu->flag & CU_3D) {
	
		if(cu->bev.first==0) makeBevelList(ob);
		
		BIF_ThemeColor(TH_WIRE);
		bl= cu->bev.first;
		nu= nurb;
		while(nu && bl) {
			bevp= (BevPoint *)(bl+1);		
			nr= bl->nr;
			
			skip= nu->resolu/16;
			
			while(nr-- > 0) {
				
				glBegin(GL_LINE_STRIP);
				vec[0]= bevp->x-G.scene->editbutsize*bevp->mat[0][0];
				vec[1]= bevp->y-G.scene->editbutsize*bevp->mat[0][1];
				vec[2]= bevp->z-G.scene->editbutsize*bevp->mat[0][2];
				glVertex3fv(vec);
				vec[0]= bevp->x+G.scene->editbutsize*bevp->mat[0][0];
				vec[1]= bevp->y+G.scene->editbutsize*bevp->mat[0][1];
				vec[2]= bevp->z+G.scene->editbutsize*bevp->mat[0][2];
				glVertex3fv(vec);

				glEnd();
				
				bevp++;
				
				a= skip;
				while(a--) {
					bevp++;
					nr--;
				}
			}

			bl= bl->next;
			nu= nu->next;
		}
	}

	calc_Nurbverts(nurb);

	if(G.vd->zbuf) glDisable(GL_DEPTH_TEST);
	
	nu= nurb;
	while(nu) {
		if((nu->type & 7)==1) tekenhandlesN(nu, 1);
		tekenvertsN(nu, 0);
		nu= nu->next;
	}

	nu= nurb;
	while(nu) {
		tekenvertsN(nu, 1);
		nu= nu->next;
	}
	
	if(G.vd->zbuf) glEnable(GL_DEPTH_TEST); 
}

static void tekentextcurs(void)
{
	cpack(0);

	glBegin(GL_QUADS);
	glVertex2fv(G.textcurs[0]);
	glVertex2fv(G.textcurs[1]);
	glVertex2fv(G.textcurs[2]);
	glVertex2fv(G.textcurs[3]);
	glEnd();
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

static void drawmball(Object *ob, int dt)
{
	MetaBall *mb;
	MetaElem *ml;
	float imat[4][4], tmat[4][4];
	int code= 1;
	
	mb= ob->data;

	if(ob==G.obedit) {
		BIF_ThemeColor(TH_WIRE);
		if((G.f & G_PICKSEL)==0 ) drawDispList(ob, dt);
		ml= editelems.first;
	}
	else {
		drawDispList(ob, dt);
		ml= mb->elems.first;
	}

	/* in case solid draw, reset wire colors */
	if(ob!=G.obedit && (ob->flag & SELECT)) {
		if(ob==OBACT) BIF_ThemeColor(TH_ACTIVE);
		else BIF_ThemeColor(TH_SELECT);
	}
	else BIF_ThemeColor(TH_WIRE);

	mygetmatrix(tmat);
	Mat4Invert(imat, tmat);
	Normalise(imat[0]);
	Normalise(imat[1]);
	
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
}

static void draw_forcefield(Object *ob)
{
	PartDeflect *pd= ob->pd;
	float imat[4][4], tmat[4][4];
	float vec[3]= {0.0, 0.0, 0.0};
	
	/* calculus here, is reused in PFIELD_FORCE */
	mygetmatrix(tmat);
	Mat4Invert(imat, tmat);
//	Normalise(imat[0]);		// we don't do this because field doesnt scale either... apart from wind!
//	Normalise(imat[1]);
	
	if(pd->flag & PFIELD_USEMAX) {
		setlinestyle(3);
		BIF_ThemeColorBlend(TH_WIRE, TH_BACK, 0.5);
		drawcircball(GL_LINE_LOOP, vec, pd->maxdist, imat);
		setlinestyle(0);
	}
	if (pd->forcefield == PFIELD_WIND) {
		float force_val;
		
		Mat4One(tmat);
		BIF_ThemeColorBlend(TH_WIRE, TH_BACK, 0.5);
		
		if (has_ipo_code(ob->ipo, OB_PD_FSTR))
			force_val = IPO_GetFloatValue(ob->ipo, OB_PD_FSTR, G.scene->r.cfra);
		else 
			force_val = pd->f_strength;
		force_val*= 0.1;
		drawcircball(GL_LINE_LOOP, vec, 1.0, tmat);
		vec[2]= 0.5*force_val;
		drawcircball(GL_LINE_LOOP, vec, 1.0, tmat);
		vec[2]= 1.0*force_val;
		drawcircball(GL_LINE_LOOP, vec, 1.0, tmat);
		vec[2]= 1.5*force_val;
		drawcircball(GL_LINE_LOOP, vec, 1.0, tmat);
		
	}
	else if (pd->forcefield == PFIELD_FORCE) {
		float ffall_val;

		if (has_ipo_code(ob->ipo, OB_PD_FFALL)) 
			ffall_val = IPO_GetFloatValue(ob->ipo, OB_PD_FFALL, G.scene->r.cfra);
		else 
			ffall_val = pd->f_power;

		BIF_ThemeColorBlend(TH_WIRE, TH_BACK, 0.5);
		drawcircball(GL_LINE_LOOP, vec, 1.0, imat);
		BIF_ThemeColorBlend(TH_WIRE, TH_BACK, 0.9 - 0.4 / pow(1.5, (double)ffall_val));
		drawcircball(GL_LINE_LOOP, vec, 1.5, imat);
		BIF_ThemeColorBlend(TH_WIRE, TH_BACK, 0.9 - 0.4 / pow(2.0, (double)ffall_val));
		drawcircball(GL_LINE_LOOP, vec, 2.0, imat);
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

		BIF_ThemeColorBlend(TH_WIRE, TH_BACK, 0.7);
		if (force_val < 0) {
			drawspiral(vec, 1.0, imat, 1);
			drawspiral(vec, 1.0, imat, 16);
		}
		else {
			drawspiral(vec, 1.0, imat, -1);
			drawspiral(vec, 1.0, imat, -16);
		}
	}
	
}

static void draw_bb_box(BoundBox *bb)
{
	float *vec;

	vec= bb->vec[0];

	glBegin(GL_LINE_STRIP);
		glVertex3fv(vec); glVertex3fv(vec+3);glVertex3fv(vec+6); glVertex3fv(vec+9);
		glVertex3fv(vec); glVertex3fv(vec+12);glVertex3fv(vec+15); glVertex3fv(vec+18);
		glVertex3fv(vec+21); glVertex3fv(vec+12);
	glEnd();

	glBegin(GL_LINE_STRIP);
		glVertex3fv(vec+3); glVertex3fv(vec+15);
	glEnd();

	glBegin(GL_LINE_STRIP);
		glVertex3fv(vec+6); glVertex3fv(vec+18);
	glEnd();

	glBegin(GL_LINE_STRIP);
		glVertex3fv(vec+9); glVertex3fv(vec+21);
	glEnd();
	
}

void get_local_bounds(Object *ob, float *centre, float *size)
{
	BoundBox *bb= NULL;
	/* uses boundbox, function used by Ketsji */
	
	if(ob->type==OB_MESH) {
		bb= ( (Mesh *)ob->data )->bb;
		if(bb==0) {
			tex_space_mesh(ob->data);
			bb= ( (Mesh *)ob->data )->bb;
		}
	}
	else if ELEM3(ob->type, OB_CURVE, OB_SURF, OB_FONT) {
		bb= ( (Curve *)ob->data )->bb;
	}
	else if(ob->type==OB_MBALL) {
		bb= ob->bb;
	}
	if(bb==NULL) {
		centre[0]= centre[1]= centre[2]= 0.0;
		VECCOPY(size, ob->size);
	}
	else {
		size[0]= 0.5*fabs(bb->vec[0][0] - bb->vec[4][0]);
		size[1]= 0.5*fabs(bb->vec[0][1] - bb->vec[2][1]);
		size[2]= 0.5*fabs(bb->vec[0][2] - bb->vec[1][2]);
		
		centre[0]= (bb->vec[0][0] + bb->vec[4][0])/2.0;
		centre[1]= (bb->vec[0][1] + bb->vec[2][1])/2.0;
		centre[2]= (bb->vec[0][2] + bb->vec[1][2])/2.0;
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
		bb= ( (Mesh *)ob->data )->bb;
		if(bb==0) {
			tex_space_mesh(ob->data);
			bb= ( (Mesh *)ob->data )->bb;
		}
	}
	else if ELEM3(ob->type, OB_CURVE, OB_SURF, OB_FONT) {
		bb= ( (Curve *)ob->data )->bb;
		if(bb==0) {
			makeDispList(ob);
			bb= ( (Curve *)ob->data )->bb;
		}
	}
	else if(ob->type==OB_MBALL) {
		bb= ob->bb;
		if(bb==0) {
			makeDispList(ob);
			bb= ob->bb;
		}
	}
	else {
		drawcube();
		return;
	}
	
	if(bb==0) return;
	
	if(ob->boundtype==OB_BOUND_BOX) draw_bb_box(bb);
	else draw_bb_quadric(bb, ob->boundtype);
	
}

static void drawtexspace(Object *ob)
{
	Mesh *me;
	MetaBall *mb;
	Curve *cu;
	BoundBox bb;
	float *vec, *loc, *size;
	
	if(ob->type==OB_MESH) {
		me= ob->data;
		size= me->size;
		loc= me->loc;
	}
	else if ELEM3(ob->type, OB_CURVE, OB_SURF, OB_FONT) {
		cu= ob->data;
		size= cu->size;
		loc= cu->loc;
	}
	else if(ob->type==OB_MBALL) {
		mb= ob->data;
		size= mb->size;
		loc= mb->loc;
	}
	else return;
	
	bb.vec[0][0]=bb.vec[1][0]=bb.vec[2][0]=bb.vec[3][0]= loc[0]-size[0];
	bb.vec[4][0]=bb.vec[5][0]=bb.vec[6][0]=bb.vec[7][0]= loc[0]+size[0];
	
	bb.vec[0][1]=bb.vec[1][1]=bb.vec[4][1]=bb.vec[5][1]= loc[1]-size[1];
	bb.vec[2][1]=bb.vec[3][1]=bb.vec[6][1]=bb.vec[7][1]= loc[1]+size[1];

	bb.vec[0][2]=bb.vec[3][2]=bb.vec[4][2]=bb.vec[7][2]= loc[2]-size[2];
	bb.vec[1][2]=bb.vec[2][2]=bb.vec[5][2]=bb.vec[6][2]= loc[2]+size[2];
	
	setlinestyle(2);
		
	vec= bb.vec[0];

	glBegin(GL_LINE_STRIP);
		glVertex3fv(vec); glVertex3fv(vec+3);glVertex3fv(vec+6); glVertex3fv(vec+9);
		glVertex3fv(vec); glVertex3fv(vec+12);glVertex3fv(vec+15); glVertex3fv(vec+18);
		glVertex3fv(vec+21); glVertex3fv(vec+12);
	glEnd();

	glBegin(GL_LINE_STRIP);
		glVertex3fv(vec+3); glVertex3fv(vec+15);
	glEnd();

	glBegin(GL_LINE_STRIP);
		glVertex3fv(vec+6); glVertex3fv(vec+18);
	glEnd();

	glBegin(GL_LINE_STRIP);
		glVertex3fv(vec+9); glVertex3fv(vec+21);
	glEnd();
	
	setlinestyle(0);
}

/* draws wire outline */
static void drawSolidSelect(Object *ob) 
{
	glLineWidth(2.0);
	glDepthMask(0);
		
	if(ELEM3(ob->type, OB_FONT,OB_CURVE, OB_SURF)) {
		Curve *cu = ob->data;
		if (displist_has_faces(&cu->disp) && boundbox_clip(ob->obmat, cu->bb)) {
			drawDispListwire(&cu->disp);
		}
	} else if (ob->type==OB_MBALL) {
		drawDispListwire(&ob->disp);
	}
	else if(ob->type==OB_ARMATURE) {
		if(ob!=G.obpose) {
			bArmature *arm= ob->data;
			
			if(G.vd->xray || (arm->flag & ARM_DRAWXRAY)==0)
				draw_armature(ob, OB_WIRE);
		}
	}

	glLineWidth(1.0);
	glDepthMask(1);
}

static void drawWireExtra(Object *ob) 
{
	if(ob!=G.obedit && (ob->flag & SELECT)) {
		if(ob==OBACT) BIF_ThemeColor(TH_ACTIVE);
		else BIF_ThemeColor(TH_SELECT);
	}
	else BIF_ThemeColor(TH_WIRE);

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
	ObHook *hook;
	float vec[3];
	
	for(hook= ob->hooks.first; hook; hook= hook->next) {
		
		VecMat4MulVecfl(vec, ob->obmat, hook->cent);
		if(hook->parent) {
			setlinestyle(3);
			glBegin(GL_LINES);
			glVertex3fv(hook->parent->obmat[3]);
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

void draw_object(Base *base)
{
	Object *ob;
	Curve *cu;
	ListBase elems;
	CfraElem *ce;
	float cfraont, axsize=1.0;
	unsigned int *rect, col=0;
	static int warning_recursive= 0;
	int sel, drawtype, colindex= 0, ipoflag;
	short dt, dtx, zbufoff= 0;
	float vec1[3], vec2[3];
	int i, selstart, selend;
	SelBox *sb;
	float selboxw;

	ob= base->object;

	/* draw keys? */
	if(base==(G.scene->basact) || (base->flag & (SELECT+BA_WAS_SEL))) {
		if(warning_recursive==0 && ob!=G.obedit) {
			if(ob->ipo && ob->ipo->showkey && (ob->ipoflag & OB_DRAWKEY)) {
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
							draw_object(base);
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
						draw_object(base);
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
	if((G.f & G_PICKSEL) == 0) {
		project_short(ob->obmat[3], &base->sx);
		
		if((G.moving & G_TRANSFORM_OBJ) && (base->flag & (SELECT+BA_WAS_SEL))) BIF_ThemeColor(TH_TRANSFORM);
		else {
		
			BIF_ThemeColor(TH_WIRE);
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
				if(base->flag & (SELECT+BA_WAS_SEL)) colindex = 7;
				else colindex = 6;
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
	if(ob==((G.scene->basact) ? (G.scene->basact->object) : 0) && (G.f & (G_FACESELECT+G_VERTEXPAINT+G_TEXTUREPAINT+G_WEIGHTPAINT))) {
		if(ob->type==OB_MESH) {
			
			if(ob==G.obedit);
			else {
				dt= OB_SHADED;
		
				glClearDepth(1.0); glClear(GL_DEPTH_BUFFER_BIT);
				glEnable(GL_DEPTH_TEST);
				zbufoff= 1;
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
	if(dt>=OB_WIRE ) {

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
			if (!(ob->dtx&OB_DRAWWIRE) && (ob->flag&SELECT)) {
				drawSolidSelect(ob);
			}
		}
	}

	switch( ob->type) {
	case OB_MESH:
		if (!(base->flag&OB_RADIO)) {
			draw_mesh_object(ob, dt);
			dtx &= ~OB_DRAWWIRE; // mesh draws wire itself

			if(G.obedit!=ob && warning_recursive==0) {
				PartEff *paf = give_parteff(ob);

				if(paf) {
					if(col) cpack(0xFFFFFF);	/* for visibility */
					if(paf->flag & PAF_STATIC) draw_static_particle_system(ob, paf);
					else if((G.f & G_PICKSEL) == 0) draw_particle_system(ob, paf);	// selection errors happen to easy
					if(col) cpack(col);
				}
			}
		}

		break;
	case OB_FONT:
		cu= ob->data;
		if(ob==G.obedit) {
			tekentextcurs();

			if (cu->flag & CU_FAST) {
				cpack(0xFFFFFF);
				set_inverted_drawing(1);
				drawDispList(ob, OB_WIRE);
				set_inverted_drawing(0);
			}

			if (cu->linewidth != 0.0) {
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
		    		vec1[1] = cu->tb[i].y + cu->linedist*cu->fsize;
		    		vec1[2] = 0.001;
		    		glBegin(GL_LINE_STRIP);
		    		glVertex3fv(vec1);
		    		vec1[0] += cu->tb[i].w;
		    		glVertex3fv(vec1);
		    		vec1[1] -= (cu->tb[i].h + cu->linedist*cu->fsize);
		    		glVertex3fv(vec1);
		    		vec1[0] -= cu->tb[i].w;
		    		glVertex3fv(vec1);
		    		vec1[1] += cu->tb[i].h + cu->linedist*cu->fsize;
		    		glVertex3fv(vec1);
		    		glEnd();
		    	}
	    	}
	    	setlinestyle(0);
	    	

	    	if (getselection(&selstart, &selend) && selboxes) {
		    	cpack(0xffffff);
		    	set_inverted_drawing(1);	    	
	    		for (i=0; i<(selend-selstart+1); i++) {
	    			sb = &(selboxes[i]);
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
		else if(dt==OB_BOUNDBOX) draw_bounding_volume(ob);
		else if(boundbox_clip(ob->obmat, cu->bb)) drawDispList(ob, dt);
			
		break;
	case OB_CURVE:
	case OB_SURF:
		cu= ob->data;
		
		if(ob==G.obedit) {
			drawnurb(ob, editNurb.first, dt);
		}
		else if(dt==OB_BOUNDBOX) draw_bounding_volume(ob);
		else if(boundbox_clip(ob->obmat, cu->bb)) drawDispList(ob, dt);
		
		break;
	case OB_MBALL:
		if(ob==G.obedit) drawmball(ob, dt);
		else if(dt==OB_BOUNDBOX) draw_bounding_volume(ob);
		else drawmball(ob, dt);
		break;
	case OB_EMPTY:
		drawaxes(1.0);
		break;
	case OB_LAMP:
		drawlamp(ob);
		break;
	case OB_CAMERA:
		drawcamera(ob);
		break;
	case OB_LATTICE:
		drawlattice(ob);
		break;
	case OB_ARMATURE:
		{
			bArmature *arm= ob->data;
			if(G.vd->xray==0 && (arm->flag & ARM_DRAWXRAY)) 
				add_view3d_after(G.vd, base, V3D_XRAY);
			else 
				draw_armature(ob, dt);
		}
		break;
	default:
		drawaxes(1.0);
	}
	if(ob->pd && ob->pd->forcefield) draw_forcefield(ob);


	/* draw extra: after normal draw because of makeDispList */
	if(dtx) {
		if(G.f & G_SIMULATION);
		else if(dtx & OB_AXIS) {
			drawaxes(axsize);
		}
		if(dtx & OB_BOUNDBOX) draw_bounding_volume(ob);
		if(dtx & OB_TEXSPACE) drawtexspace(ob);
		if(dtx & OB_DRAWNAME) {
			// patch for several 3d cards (IBM mostly) that crash on glSelect with text drawing
			if((G.f & G_PICKSEL) == 0) {
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
	if(base->flag & OB_FROMDUPLI) return;
	if(base->flag & OB_RADIO) return;
	if(G.f & G_SIMULATION) return;

	if((G.f & (G_PICKSEL))==0) {
		ListBase *list;

		/* draw hook center and offset line */
		if(ob->hooks.first && ob!=G.obedit) draw_hooks(ob);

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
			glColor3ubv(col2);
			
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

		/* object centers */
		if(G.vd->zbuf) glDisable(GL_DEPTH_TEST);
		if(ob->type == OB_LAMP) {
			if(ob->id.lib) {
				if(base->flag & SELECT) rect= rectllib_sel;
				else rect= rectllib_desel;
			}
			else if(ob->id.us>1) {
				if(base->flag & SELECT) rect= rectlus_sel;
				else rect= rectlus_desel;
			}
			else {
				if(base->flag & SELECT) rect= rectl_sel;
				else rect= rectl_desel;
			}
			draw_icon_centered(ob->obmat[3], rect, 9);
		}
		else {
			if(ob->id.lib || ob->id.us>1) {
				if(base->flag & SELECT) rect= rectu_sel;
				else rect= rectu_desel;
			}
			else {
				if(base->flag & SELECT) rect= rect_sel;
				/* The center of the active object (which need not 
				 * be selected) gets drawn as if it were selected
				 */
				else if(base==(G.scene->basact)) rect= rect_sel;
				else rect= rect_desel;
			}
			draw_icon_centered(ob->obmat[3], rect, 4);
		}
		if(G.vd->zbuf) glEnable(GL_DEPTH_TEST);
		
	}
	else if((G.f & (G_VERTEXPAINT|G_FACESELECT|G_TEXTUREPAINT|G_WEIGHTPAINT))==0) {
	
		glBegin(GL_POINTS);
			glVertex3fv(ob->obmat[3]);
		glEnd();
		
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

	draw_object(base);

	G.f &= ~G_DRAW_EXT;

	glFlush();		/* reveil frontbuffer drawing */
	glDrawBuffer(GL_BACK);
	
	if(G.vd->zbuf) {
		G.vd->zbuf= 0;
		glDisable(GL_DEPTH_TEST);
	}
	curarea->win_swap= WIN_FRONT_OK;
}

/* ***************** BACKBUF SEL (BBS) ********* */

static int bbs_mesh_verts__setDrawOptions(void *userData, EditVert *eve)
{
	if (eve->h==0) {
		set_framebuffer_index_color((int) eve->prev);
		return 1;
	} else {
		return 0;
	}
}
static int bbs_mesh_verts(DerivedMesh *dm, int offset)
{
	EditVert *eve, *preveve;

	for (eve=G.editMesh->verts.first; eve; eve= eve->next)
		eve->prev = (EditVert*) offset++;

	glPointSize( BIF_GetThemeValuef(TH_VERTEX_SIZE) );
	dm->drawMappedVertsEM(dm, bbs_mesh_verts__setDrawOptions, NULL);
	glPointSize(1.0);

	for (preveve=NULL, eve=G.editMesh->verts.first; eve; preveve=eve, eve= eve->next)
		eve->prev = preveve;

	return offset;
}		

static int bbs_mesh_wire__setDrawOptions(void *userData, EditEdge *eed)
{
	if (eed->h==0) {
		set_framebuffer_index_color((int) eed->vn);
		return 1;
	} else {
		return 0;
	}
}
static int bbs_mesh_wire(DerivedMesh *dm, int offset)
{
	EditEdge *eed;

	for(eed= G.editMesh->edges.first; eed; eed= eed->next)
		eed->vn= (EditVert*) offset++;

	dm->drawMappedEdgesEM(dm, bbs_mesh_wire__setDrawOptions, NULL);

	return offset;
}		

static int bbs_mesh_solid__setDrawOptions(void *userData, EditFace *efa)
{
	if (efa->h==0) {
		if (userData) {
			set_framebuffer_index_color((int) efa->prev);
		}
		return 1;
	} else {
		return 0;
	}
}
/* two options, facecolors or black */
static int bbs_mesh_solid(Object *ob, DerivedMesh *dm, int facecol)
{
	int glmode, a;
	
	cpack(0);

	if(ob==G.obedit) {
		if (facecol) {
			EditFace *efa, *prevefa;
			int b;

				// tuck original indices in efa->prev
			for(b=1, efa= G.editMesh->faces.first; efa; efa= efa->next, b++) 
				efa->prev= (EditFace *)(b);
			a = b;

			dm->drawMappedFacesEM(dm, bbs_mesh_solid__setDrawOptions, (void*) 1);

			if(G.scene->selectmode & SCE_SELECT_FACE) {
				glPointSize(BIF_GetThemeValuef(TH_FACEDOT_SIZE));
				
				bglBegin(GL_POINTS);
				for(efa= G.editMesh->faces.first; efa; efa= efa->next) {
					if(efa->h==0 && efa->fgonf!=EM_FGON) {
						set_framebuffer_index_color((int)efa->prev);
						bglVertex3fv(efa->cent);
					}
				}
				bglEnd();
			}

			for (prevefa= NULL, efa= G.editMesh->faces.first; efa; prevefa= efa, efa= efa->next)
				efa->prev= prevefa;
			return a;
		} else {
			dm->drawMappedFacesEM(dm, bbs_mesh_solid__setDrawOptions, (void*) 0);
			return 1;
		}
	}
	else {
		Mesh *me= ob->data;
		MVert *mvert;
		MFace *mface;
		TFace *tface;
		DispList *dl;
		float *extverts=NULL;
		int a, totface, hastface;
		
		mvert= me->mvert;
		mface= me->mface;
		tface= me->tface;
		hastface = (me->tface != NULL);
		totface= me->totface;

		dl= find_displist(&ob->disp, DL_VERTS);
		if(dl) extverts= dl->verts;
		
		glBegin(GL_QUADS);
		glmode= GL_QUADS;
		
		for(a=0; a<totface; a++, mface++, tface++) {
			if(mface->v3) {
				if(facecol) {
					if(hastface && tface->flag & TF_HIDE) continue;
					set_framebuffer_index_color(a+1);
				}

				if(mface->v4) {if(glmode==GL_TRIANGLES) {glmode= GL_QUADS; glEnd(); glBegin(GL_QUADS);}}
				else {if(glmode==GL_QUADS) {glmode= GL_TRIANGLES; glEnd(); glBegin(GL_TRIANGLES);}}
				
				if(extverts) {
					glVertex3fv( extverts+3*mface->v1 );
					glVertex3fv( extverts+3*mface->v2 );
					glVertex3fv( extverts+3*mface->v3 );
					if(mface->v4) glVertex3fv( extverts+3*mface->v4 );
				}
				else {
					glVertex3fv( (mvert+mface->v1)->co );
					glVertex3fv( (mvert+mface->v2)->co );
					glVertex3fv( (mvert+mface->v3)->co );
					if(mface->v4) glVertex3fv( (mvert+mface->v4)->co );
				}
			}
		}
		glEnd();
		return 1;
	}
}

void draw_object_backbufsel(Object *ob)
{
	extern int em_solidoffs, em_wireoffs, em_vertoffs;	// let linker solve it... from editmesh_mods.c 
	int dmNeedsFree;
	DerivedMesh *dm;

	mymultmatrix(ob->obmat);

	glClearDepth(1.0); glClear(GL_DEPTH_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);

	switch( ob->type) {
	case OB_MESH:
		dm = mesh_get_cage_derived(ob, &dmNeedsFree);

		if(G.obedit) {
			em_solidoffs= bbs_mesh_solid(ob, dm, G.scene->selectmode & SCE_SELECT_FACE);
			
			bglPolygonOffset(1.0);
			
			// we draw edges always, for loop (select) tools
			em_wireoffs= bbs_mesh_wire(dm, em_solidoffs);
			
			if(G.scene->selectmode & SCE_SELECT_VERTEX) 
				em_vertoffs= bbs_mesh_verts(dm, em_wireoffs);
			else em_vertoffs= em_wireoffs;
			
			bglPolygonOffset(0.0);
		}
		else bbs_mesh_solid(ob, dm, 1);	// 1= facecol, faceselect

		if (dmNeedsFree) {
			dm->release(dm);
		}
		break;
	case OB_CURVE:
	case OB_SURF:
		break;
	}

	myloadmatrix(G.vd->viewmat);

}
